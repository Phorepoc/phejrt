// Copyright The Phorepoc Authors
// SPDX-License-Identifier: GPL-2.0-only
import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Form;

public class HelloJ1 extends MIDlet {
    public void startApp() {
        System.out.println("[J1] HelloJ1.startApp: MIDP " + System.getProperty("microedition.profiles")
            + ", CLDC " + System.getProperty("microedition.configuration"));
        Form f = new Form("J1");
        f.append("phoneME on the host");
        Display.getDisplay(this).setCurrent(f);
        System.out.println("[J1] Form shown");
        notifyDestroyed();
    }
    public void pauseApp() {}
    public void destroyApp(boolean u) { System.out.println("[J1] destroyApp"); }
}
