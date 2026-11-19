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
import javax.microedition.rms.RecordStore;

/** Test MIDlet: a Form with a text, a numeric field and an Exit command; every event it gets is printed as "[J1] ...". */
public class HelloJ1 extends MIDlet implements CommandListener, ItemStateListener {
    private final Command exit = new Command("Exit", Command.EXIT, 1);
    private TextField number;

    public void startApp() {
        System.out.println("[J1] HelloJ1.startApp: MIDP " + System.getProperty("microedition.profiles")
            + ", CLDC " + System.getProperty("microedition.configuration"));
        int shown = 0;
        try {   // RMS: a record store is a file under the Java home (C: on Symbian); the count survives a restart
            RecordStore rs = RecordStore.openRecordStore("j5", true);
            int before = rs.getNumRecords();
            byte[] d = ("run " + (before + 1) + " éαЖ").getBytes();
            rs.addRecord(d, 0, d.length);
            int after = rs.getNumRecords();
            rs.closeRecordStore();
            rs = RecordStore.openRecordStore("j5", false);
            String last = new String(rs.getRecord(rs.getNextRecordID() - 1));
            rs.closeRecordStore();
            System.out.println("[J1] rms records " + before + " -> " + after + ", last='" + last.substring(0, 5) + "'");
            shown = after;
        } catch (Exception e) {
            System.out.println("[J1] rms failed: " + e);
        }
        Form f = new Form("HelloJ1");
        f.append(new StringItem("phoneME MIDP", "on Phorepoc OS"));
        f.append(new StringItem("runs: " + shown, "éèü αβγ Жж"));
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
