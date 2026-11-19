/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phejdrv -- the boot-test driver for J5 (staged as the SSM test EXE): install c:\sisinst.sis with the real installer
 * (sisinst.exe, in the ROM), wait until AppArc lists the application whose UID is in c:\phejdrv.txt (hex; default E5A0A001),
 * print its caption, then start it through AppArc exactly as the menu does (RApaLsSession::StartApp). Every line is
 * "[J5] ..." on the debug port.
 */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <apgcli.h>
#include <apacmdln.h>

static TUint32 WantedUid()
	{
	TUint32 uid = 0xE5A0A001;
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		RFile f;
		if (f.Open(fs, _L("c:\\phejdrv.txt"), EFileRead) == KErrNone)
			{
			TBuf8<32> b;
			f.Read(b);
			f.Close();
			TBuf<32> w;
			w.Copy(b);
			TLex lex(w);
			TUint32 v;
			if (lex.Val(v, EHex) == KErrNone)
				uid = v;
			}
		fs.Close();
		}
	return uid;
	}

static void DoL()
	{
	// 1. the install
	RProcess inst;
	TInt r = inst.Create(_L("sisinst.exe"), _L("c:\\sisinst.sis"));
	RDebug::Printf("[J5] sisinst create %d", r);
	if (r == KErrNone)
		{
		TRequestStatus st;
		inst.Logon(st);
		inst.Resume();
		User::WaitForRequest(st);
		RDebug::Printf("[J5] sisinst exit %d (%d)", inst.ExitReason(), st.Int());
		inst.Close();
		}
	// 2. AppArc lists the registration
	const TUint32 uid = WantedUid();
	RApaLsSession ls;
	User::LeaveIfError(ls.Connect());
	CleanupClosePushL(ls);
	TApaAppInfo info;
	TInt found = KErrNotFound;
	for (TInt i = 0; i < 90 && found != KErrNone; i++)
		{
		found = ls.GetAppInfo(info, TUid::Uid(uid));
		if (found != KErrNone)
			User::After(1000000);
		}
	if (found != KErrNone)
		{
		RDebug::Printf("[J5] app %08x NOT in the AppArc list (%d)", uid, found);
		User::Leave(found);
		}
	RDebug::Printf("[J5] AppArc lists %08x: caption '%S' file '%S'", uid, &info.iCaption, &info.iFullName);
	// 3. start it as the menu does
	CApaCommandLine* cmd = CApaCommandLine::NewLC();
	cmd->SetExecutableNameL(info.iFullName);
	cmd->SetCommandL(EApaCommandRun);
	TThreadId tid;
	r = ls.StartApp(*cmd, tid);
	RDebug::Printf("[J5] StartApp %08x -> %d", uid, r);
	CleanupStack::PopAndDestroy(2);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, DoL());
	delete cleanup;
	RDebug::Printf("[J5] driver done %d", err);
	return err;
	}
