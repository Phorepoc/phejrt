// Copyright The Phorepoc Authors
// SPDX-License-Identifier: GPL-2.0-only
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

/** Second test MIDlet of the suite: a Canvas that draws and prints the key and pointer events it gets ("[J2] ..."). */
public class HelloJ2 extends MIDlet implements CommandListener {
    private final Command exit = new Command("Exit", Command.EXIT, 1);
    private String last = "touch or press a key";

    private class Board extends Canvas {
        protected void paint(Graphics g) {
            g.setColor(0xfff4d0);
            g.fillRect(0, 0, getWidth(), getHeight());
            g.setColor(0x884400);
            g.drawRect(2, 2, getWidth() - 5, getHeight() - 5);
            g.setFont(Font.getFont(Font.FACE_MONOSPACE, Font.STYLE_BOLD, Font.SIZE_MEDIUM));
            g.drawString("HelloJ2 (canvas)", 8, 10, Graphics.TOP | Graphics.LEFT);
            g.drawString(last, 8, 40, Graphics.TOP | Graphics.LEFT);
        }
        protected void keyPressed(int code) {
            last = "key " + code;
            System.out.println("[J2] key " + code);
            repaint();
        }
        protected void pointerPressed(int x, int y) {
            last = "pen " + x + "," + y;
            System.out.println("[J2] pen " + x + "," + y);
            repaint();
        }
    }

    public void startApp() {
        System.out.println("[J2] HelloJ2.startApp");
        Board b = new Board();
        b.addCommand(exit);
        b.setCommandListener(this);
        Display.getDisplay(this).setCurrent(b);
    }

    public void commandAction(Command c, Displayable d) {
        System.out.println("[J2] command " + c.getLabel());
        notifyDestroyed();
    }

    public void pauseApp() {}

    public void destroyApp(boolean u) {}
}
