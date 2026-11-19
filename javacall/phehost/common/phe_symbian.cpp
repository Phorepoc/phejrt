/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phehost on Symbian: the one place that calls the native OS.
 *
 * 1. phe_rdebug sends text to the debug port (serial in an emulator, RDebug on a device) a line at a time, as Phorepoc's
 *    boot tests do. The VM prints a character or a fragment per call, so a partial line is kept until its newline.
 * 2. The display: a plain RWsSession + RWindowGroup + RWindow client (no CONE, no Avkon), after tools/phorepoc-x86/netsurf's
 *    window server back end. The javacall LCD buffer (RGB565, javacall/phehost/midp/lcd.c) is copied into an EColor64K
 *    CFbsBitmap (the same layout) and drawn into the window inside an Invalidate/BeginRedraw pair. One thread (the "ws
 *    thread") owns every window server object -- the session is not shared -- and wakes on the WSERV event/redraw
 *    requests and on a 25 ms timer; javacall_lcd_flush only sets a flag the timer tick picks up. Keys and pen events
 *    become javanotify_key_event / javanotify_pen_event (they queue on the javacall event queue, thread safe).
 */
#ifdef __SYMBIAN32__
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32keys.h>
#include <w32std.h>
#include <fbs.h>
#include <pthread.h>

extern "C" {
#include "javacall_keypress.h"
#include "javacall_penevent.h"
}

static TBuf8<200> gLine;

static void flushLine() {
    if (gLine.Length()) {
        gLine.Append(_L8("\r\n"));          // RawPrint adds no newline
        RDebug::RawPrint(gLine);
        gLine.Zero();
    }
}

extern "C" void phe_rdebug(const char* s, int length) {
    for (int i = 0; i < length; i++) {
        char c = s[i];
        if (c == '\n') {
            flushLine();
        } else if (c != '\r') {
            if (gLine.Length() >= 190) flushLine();
            gLine.Append((TUint8)c);
        }
    }
}

namespace {

const TUint32 KGroupId = 0x4A315F31;   // 'J1_1'
volatile TInt gFlush = 0;
const unsigned short* gFb = NULL;
TInt gW = 0, gH = 0;

struct TWs {
    RWsSession iWs;
    CWsScreenDevice* iScreen;
    CWindowGc* iGc;
    RWindowGroup iGroup;
    RWindow iWin;
    CFbsBitmap* iBmp;
    RTimer iTimer;
    TRequestStatus iEvSt, iRdSt, iTmSt;
    TInt iTicks;
};

void CopyFrame(TWs& w) {
    const TInt stride = CFbsBitmap::ScanLineLength(gW, EColor64K);
    w.iBmp->BeginDataAccess();
    TUint8* dst = (TUint8*)w.iBmp->DataAddress();
    for (TInt y = 0; y < gH; y++)
        Mem::Copy(dst + y * stride, gFb + y * gW, gW * 2);
    w.iBmp->EndDataAccess(EFalse);
}

void Draw(TWs& w) {
    const TRect all(TPoint(0, 0), TSize(gW, gH));
    CopyFrame(w);
    w.iGc->Activate(w.iWin);
    w.iWin.Invalidate(all);
    w.iWin.BeginRedraw(all);
    w.iGc->BitBlt(TPoint(0, 0), w.iBmp, all);
    w.iWin.EndRedraw();
    w.iGc->Deactivate();
    w.iWs.Flush();
}

// Symbian key codes -> javacall keys (0 = not a MIDP key)
TInt MapKey(TUint aCode) {
    switch (aCode) {
    case EKeyUpArrow: return JAVACALL_KEY_UP;
    case EKeyDownArrow: return JAVACALL_KEY_DOWN;
    case EKeyLeftArrow: return JAVACALL_KEY_LEFT;
    case EKeyRightArrow: return JAVACALL_KEY_RIGHT;
    case EKeyEnter: case EKeyDevice3: return JAVACALL_KEY_SELECT;
    case EKeyDevice0: return JAVACALL_KEY_SOFT1;      // left softkey (F5 in the emulator)
    case EKeyDevice1: return JAVACALL_KEY_SOFT2;      // right softkey (F7)
    case EKeyBackspace: return JAVACALL_KEY_CLEAR;
    case EKeyYes: return JAVACALL_KEY_SEND;
    default: break;
    }
    if ((aCode >= '0' && aCode <= '9') || aCode == '*' || aCode == '#')
        return (TInt)aCode;
    return 0;
}

TInt gEvLog = 0;

void HandleEvent(TWs& w) {
    TWsEvent ev;
    w.iWs.GetEvent(ev);
    if (ev.Type() == EEventPointer) {
        const TPointerEvent& p = *ev.Pointer();
        if (gEvLog++ < 40) RDebug::Printf("[phehost] pen %d at %d,%d", (TInt)p.iType, p.iPosition.iX, p.iPosition.iY);
        switch (p.iType) {
        case TPointerEvent::EButton1Down: javanotify_pen_event(p.iPosition.iX, p.iPosition.iY, JAVACALL_PENPRESSED); break;
        case TPointerEvent::EButton1Up: javanotify_pen_event(p.iPosition.iX, p.iPosition.iY, JAVACALL_PENRELEASED); break;
        case TPointerEvent::EDrag: javanotify_pen_event(p.iPosition.iX, p.iPosition.iY, JAVACALL_PENDRAGGED); break;
        default: break;
        }
    } else if (ev.Type() == EEventKey) {
        const TInt k = MapKey(ev.Key()->iCode);
        if (gEvLog++ < 40) RDebug::Printf("[phehost] key code %d -> javacall %d", (TInt)ev.Key()->iCode, k);
        if (k) {
            javanotify_key_event((javacall_key)k, JAVACALL_KEYPRESSED);
            javanotify_key_event((javacall_key)k, JAVACALL_KEYRELEASED);
        }
    }
}

void* WsThread(void*) {
    TWs w;
    Mem::FillZ(&w, sizeof(w));
    TInt r = w.iWs.Connect();
    if (r == KErrNone) {
        w.iScreen = new CWsScreenDevice(w.iWs);
        r = w.iScreen ? w.iScreen->Construct() : KErrNoMemory;
    }
    if (r == KErrNone) r = w.iScreen->CreateContext(w.iGc);
    if (r == KErrNone) {
        w.iGroup = RWindowGroup(w.iWs);
        r = w.iGroup.Construct(KGroupId, ETrue);
    }
    if (r == KErrNone) {
        w.iGroup.SetOrdinalPosition(0, 1000);
        w.iWin = RWindow(w.iWs);
        r = w.iWin.Construct(w.iGroup, KGroupId + 1);
    }
    if (r == KErrNone) {
        const TSize sc = w.iScreen->SizeInPixels();
        // the window covers the whole screen when the frame is as big (the normal case: lcd.c asks for the screen's size);
        // a smaller frame (PHE_LCD_SIZE) is centred
        w.iWin.SetExtent(TPoint((sc.iWidth - gW) / 2, (sc.iHeight - gH) / 2), TSize(gW, gH));
        w.iWin.SetBackgroundColor(KRgbWhite);
        w.iWin.PointerFilter(EPointerFilterEnterExit | EPointerFilterMove | EPointerFilterDrag, 0);
        w.iWin.Activate();
        if (!RFbsSession::GetSession()) RFbsSession::Connect();
        w.iBmp = new CFbsBitmap;
        r = w.iBmp ? w.iBmp->Create(TSize(gW, gH), EColor64K) : KErrNoMemory;
    }
    if (r == KErrNone) r = w.iTimer.CreateLocal();
    RDebug::Printf("[phehost] ws window %dx%d r=%d", gW, gH, r);
    if (r != KErrNone) return NULL;
    w.iWs.EventReady(&w.iEvSt);
    w.iWs.RedrawReady(&w.iRdSt);
    w.iTimer.After(w.iTmSt, 25000);
    for (;;) {
        User::WaitForAnyRequest();
        if (w.iEvSt != KRequestPending) {
            HandleEvent(w);
            w.iWs.EventReady(&w.iEvSt);
        }
        if (w.iRdSt != KRequestPending) {
            TWsRedrawEvent red;
            w.iWs.GetRedraw(red);
            w.iWs.RedrawReady(&w.iRdSt);
            gFlush = 1;
        }
        if (w.iTmSt != KRequestPending) {
            if (gFlush) {
                gFlush = 0;
                Draw(w);
            }
            if ((++w.iTicks % 40) == 0) w.iGroup.SetOrdinalPosition(0, 1000);   // stay in front of the UI that starts later
            w.iTimer.After(w.iTmSt, 25000);
        }
    }
}

}   // namespace

extern "C" int phe_ws_screen_size(int* aW, int* aH) {
    RWsSession ws;
    TInt r = ws.Connect();
    if (r != KErrNone) return r;
    CWsScreenDevice* sd = new CWsScreenDevice(ws);
    r = sd ? sd->Construct() : KErrNoMemory;
    if (r == KErrNone) {
        const TSize sc = sd->SizeInPixels();
        *aW = sc.iWidth;
        *aH = sc.iHeight;
    }
    delete sd;
    ws.Close();
    return r;
}

extern "C" int phe_ws_start(const unsigned short* aFb, int aW, int aH) {
    gFb = aFb;
    gW = aW;
    gH = aH;
    pthread_t t;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 64 * 1024);
    return pthread_create(&t, &a, WsThread, NULL);
}

extern "C" void phe_ws_flush(void) {
    gFlush = 1;
}
#endif
