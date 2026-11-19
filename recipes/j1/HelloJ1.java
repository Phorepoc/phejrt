// Copyright The Phorepoc Authors
// SPDX-License-Identifier: GPL-2.0-only
import java.util.Timer;
import java.util.TimerTask;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Item;
import javax.microedition.lcdui.ItemStateListener;
import javax.microedition.lcdui.StringItem;
import javax.microedition.lcdui.TextField;
import javax.microedition.midlet.MIDlet;

/** Test MIDlet: a Form with a text, a numeric field and an Exit command; every event it gets is printed as "[J1] ...". */
public class HelloJ1 extends MIDlet implements CommandListener, ItemStateListener {
    private final Command exit = new Command("Exit", Command.EXIT, 1);
    private TextField number;

    public void startApp() {
        System.out.println("[J1] HelloJ1.startApp: MIDP " + System.getProperty("microedition.profiles")
            + ", CLDC " + System.getProperty("microedition.configuration"));
        Form f = new Form("HelloJ1");
        f.append(new StringItem("phoneME MIDP", "on Phorepoc OS"));
        number = new TextField("number", "", 8, TextField.NUMERIC);
        f.append(number);
        f.addCommand(exit);
        f.setCommandListener(this);
        f.setItemStateListener(this);
        Display.getDisplay(this).setCurrent(f);
        System.out.println("[J1] Form shown");
        new Timer().schedule(new TimerTask() {
            public void run() {
                System.out.println("[J1] timeout exit");
                notifyDestroyed();
            }
        }, 150000);
    }

    public void commandAction(Command c, Displayable d) {
        System.out.println("[J1] command " + c.getLabel());
        notifyDestroyed();
    }

    public void itemStateChanged(Item item) {
        System.out.println("[J1] number=" + number.getString());
    }

    public void pauseApp() {}

    public void destroyApp(boolean u) { System.out.println("[J1] destroyApp"); }
}
