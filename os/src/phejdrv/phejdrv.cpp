/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phejdrv -- the boot-test driver for J5/J6 (staged as the SSM test EXE). It runs the script c:\phejdrv.txt, one command per
 * line ('#' starts a comment), and prints every step as "[J5] ..." on the debug port:
 *
 *   install <path.sis>       install with the real installer (sisinst.exe, in the ROM); prints the exit code
 *   install-fail <path.sis>  the same, but the install is expected to fail (e.g. a missing dependency)
 *   list <hexuid> ...        wait (up to 90 s) until AppArc lists every application, print its caption
 *   start <hexuid>           start the application through AppArc, exactly as the menu does
 *   sleep <seconds>
 *   uninstall <hexuid>       sisinst -u <package uid> (silent, kills a running application)
 *   gone <hexuid> ...        every application is no longer listed by AppArc and its launcher file is deleted
 *
 * No script: the J5 test (install c:\sisinst.sis, list and start e5a0a001, or the UID in a one-line c:\phejdrv.txt).
 */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <apgcli.h>
#include <apacmdln.h>

static TInt RunSisinst(const TDesC& aArgs)
	{
	RProcess inst;
	TInt r = inst.Create(_L("sisinst.exe"), aArgs);
	if (r != KErrNone)
		return r;
	TRequestStatus st;
	inst.Logon(st);
	inst.Resume();
	User::WaitForRequest(st);
	r = inst.ExitReason();
	inst.Close();
	return r;
	}

static TBool Listed(RApaLsSession& aLs, TUint32 aUid, TApaAppInfo& aInfo)
	{
	return aLs.GetAppInfo(aInfo, TUid::Uid(aUid)) == KErrNone;
	}

static void ListL(RApaLsSession& aLs, TLex& aLex)
	{
	TPtrC t;
	for (t.Set(aLex.NextToken()); t.Length(); t.Set(aLex.NextToken()))
		{
		TLex u(t);
		TUint32 uid = 0;
		u.Val(uid, EHex);
		TApaAppInfo info;
		TInt i = 0;
		while (!Listed(aLs, uid, info) && i++ < 90)
			User::After(1000000);
		if (i > 90)
			RDebug::Printf("[J5] app %08x NOT in the AppArc list", uid);
		else
			RDebug::Printf("[J5] AppArc lists %08x: caption '%S'", uid, &info.iCaption);
		}
	}

static void StartL(RApaLsSession& aLs, TUint32 aUid)
	{
	TApaAppInfo info;
	if (!Listed(aLs, aUid, info))
		{
		RDebug::Printf("[J5] start %08x: not listed", aUid);
		return;
		}
	CApaCommandLine* cmd = CApaCommandLine::NewLC();
	cmd->SetExecutableNameL(info.iFullName);
	cmd->SetCommandL(EApaCommandRun);
	TThreadId tid;
	TInt r = aLs.StartApp(*cmd, tid);
	RDebug::Printf("[J5] StartApp %08x -> %d", aUid, r);
	CleanupStack::PopAndDestroy(cmd);
	}

static void GoneL(RApaLsSession& aLs, RFs& aFs, TLex& aLex)
	{
	TPtrC t;
	for (t.Set(aLex.NextToken()); t.Length(); t.Set(aLex.NextToken()))
		{
		TLex u(t);
		TUint32 uid = 0;
		u.Val(uid, EHex);
		TApaAppInfo info;
		TBool listed = Listed(aLs, uid, info);
		TInt i = 0;
		while (listed && i++ < 30)
			{
			User::After(1000000);
			listed = Listed(aLs, uid, info);
			}
		TFileName f;
		f.Format(_L("c:\\sys\\bin\\phejl_%08x.exe"), uid);
		TEntry ent;
		TBool file = aFs.Entry(f, ent) == KErrNone;
		RDebug::Printf("[J5] %08x after uninstall: listed %d, launcher file %d -> %s", uid, listed, file, (listed || file) ? "STILL THERE" : "GONE");
		}
	}

static void DoL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RApaLsSession ls;
	User::LeaveIfError(ls.Connect());
	CleanupClosePushL(ls);
	RFile f;
	TBuf8<1024> script;
	TInt r = f.Open(fs, _L("c:\\phejdrv.txt"), EFileRead);
	if (r == KErrNone)
		{
		f.Read(script);
		f.Close();
		}
	TBuf<1024> text;
	text.Copy(script);
	// the J5 form: a lone hex UID (or nothing) = install c:\sisinst.sis, list + start that application
	if (r != KErrNone || (text.Locate('\n') == KErrNotFound && text.Locate(' ') == KErrNotFound))
		{
		TLex lex(text);
		TUint32 uid = 0xE5A0A001;
		if (text.Length())
			lex.Val(uid, EHex);
		text.Format(_L("install c:\\sisinst.sis\nlist %x\nstart %x\n"), uid, uid);
		}
	TInt pos = 0;
	while (pos < text.Length())
		{
		TInt nl = text.Mid(pos).Locate('\n');
		TPtrC line;
		line.Set(nl == KErrNotFound ? text.Mid(pos) : text.Mid(pos, nl));
		pos += nl == KErrNotFound ? text.Length() : nl + 1;
		TLex lex(line);
		lex.SkipSpace();
		TPtrC cmd = lex.NextToken();
		if (cmd.Length() == 0 || cmd[0] == '#')
			continue;
		if (cmd == _L("install") || cmd == _L("install-fail"))
			{
			TPtrC path = lex.NextToken();
			TInt e = RunSisinst(path);
			RDebug::Printf("[J5] %S %S -> %d%s", &cmd, &path, e, cmd == _L("install-fail") ? (e != KErrNone ? " (refused, as expected)" : " (UNEXPECTEDLY installed)") : "");
			}
		else if (cmd == _L("list"))
			ListL(ls, lex);
		else if (cmd == _L("start"))
			{
			TUint32 uid = 0;
			lex.SkipSpace();
			lex.Val(uid, EHex);
			StartL(ls, uid);
			}
		else if (cmd == _L("sleep"))
			{
			TInt s = 0;
			lex.SkipSpace();
			lex.Val(s);
			User::After(s * 1000000);
			}
		else if (cmd == _L("uninstall"))
			{
			TBuf<64> a;
			TPtrC pk = lex.NextToken();
			a.Format(_L("-u %S"), &pk);
			TInt e = RunSisinst(a);
			RDebug::Printf("[J5] uninstall %S -> %d", &a, e);
			}
		else if (cmd == _L("gone"))
			GoneL(ls, fs, lex);
		}
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
