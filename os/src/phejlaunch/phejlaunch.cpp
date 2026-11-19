/*
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * phejlaunch -- the application AppArc starts for one installed MIDlet suite. Its registration (<name>_reg.rsc) names this
 * EXE; the package packer (recipes/j5-pack.py) gives each suite a copy with its own UID3, and installs
 * !:\private\<UID3>\launch.txt beside it: the command line of the runtime (phemidp.exe), e.g.
     -1 HelloJ1 -classpathext c:/resource/phejava/suites/e5a0a001/app.jar
 * The launcher starts the runtime with it and ends; the runtime opens its own window.
 */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>

static TInt Run()
	{
	const TUint32 uid = RProcess().Type()[2].iUid;
	TFileName self = RProcess().FileName();
	TBuf<64> path;
	path.Format(_L("%c:\\private\\%08x\\launch.txt"), (TUint)self[0], uid);
	RFs fs;
	TInt r = fs.Connect();
	if (r != KErrNone)
		return r;
	RFile f;
	r = f.Open(fs, path, EFileRead | EFileShareReadersOnly);
	TBuf8<400> cmd8;
	if (r == KErrNone)
		{
		r = f.Read(cmd8);
		f.Close();
		}
	fs.Close();
	RDebug::Printf("[phejlaunch] %S -> %d", &path, r);
	if (r != KErrNone)
		return r;
	TBuf<400> cmd;
	cmd.Copy(cmd8);
	while (cmd.Length() && (cmd[cmd.Length() - 1] == '\n' || cmd[cmd.Length() - 1] == '\r'))
		cmd.SetLength(cmd.Length() - 1);
	RProcess p;
	r = p.Create(_L("phemidp.exe"), cmd);
	RDebug::Printf("[phejlaunch] start phemidp.exe '%S': %d", &cmd, r);
	if (r == KErrNone)
		{
		p.Resume();
		p.Close();
		}
	return r;
	}

TInt E32Main()
	{
	return Run();
	}
