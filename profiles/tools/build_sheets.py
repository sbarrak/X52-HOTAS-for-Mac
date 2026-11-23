"""Builds one printable button map (PDF, A4 landscape) per aircraft into profiles/mapping/.

Pages: Mode 1, Mode 2, Mode 3 (pinkie not pressed: the pinkie is not a shift in these profiles),
then the screen (MFD) pages and the lights / alarms.
Needs reportlab (pip install reportlab). Aircraft pictures are read from the X-Plane folder.
Usage: python3 build_sheets.py ["/path/to/X-Plane 12"]
"""
import os
import re
import sys

from reportlab.lib.colors import HexColor, white
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aircraft import *  # noqa: F401,F403
from build_profiles import key_for

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "mapping"))
XPLANE = sys.argv[1] if len(sys.argv) > 1 else "/Applications/Steam Games/steamapps/common/X-Plane 12"

W, H = landscape(A4)
M = 28
INK = HexColor("#1d1d1f")
GREY = HexColor("#6e6e73")
LIGHT = HexColor("#c7c7cc")
FILL = HexColor("#ececef")
BODY = HexColor("#d9d9de")
LCD = HexColor("#c8d8a8")
LCD_INK = HexColor("#26331a")
MODE_HEX = {1: HexColor("#2e9d4f"), 2: HexColor("#d98c00"), 3: HexColor("#c8312b")}
LED_HEX = {"green": HexColor("#2e9d4f"), "amber": HexColor("#e09a00"), "red": HexColor("#c8312b"), "off": HexColor("#bdbdc2")}


def arrow(c, x, y, d, s=3.2):
    """Small filled triangle pointing up/right/down/left, centred on x, y."""
    pts = {"u": [(0, s), (-s, -s * 0.8), (s, -s * 0.8)], "d": [(0, -s), (-s, s * 0.8), (s, s * 0.8)],
           "r": [(s, 0), (-s * 0.8, s), (-s * 0.8, -s)], "l": [(-s, 0), (s * 0.8, s), (s * 0.8, -s)]}[d]
    p = c.beginPath()
    p.moveTo(x + pts[0][0], y + pts[0][1])
    for px, py in pts[1:]:
        p.lineTo(x + px, y + py)
    p.close()
    c.drawPath(p, stroke=0, fill=1)


class Sheet:
    def __init__(self, c, ac):
        self.c = c
        self.ac = ac
        self.f = features(ac)
        self.L = layout(self.f)
        self.titles = mode_titles(self.f)
        path = os.path.join(XPLANE, "Aircraft", "Laminar Research", ac["folder"], ac["icon"])
        self.img = ImageReader(path) if os.path.exists(path) else None

    # ---------------------------------------------------------------- frame
    def header(self, badge_title, badge_sub, badge_colour, pinkie_line):
        c = self.c
        top = H - M
        if self.img:
            c.drawImage(self.img, M, top - 58, width=103, height=58, mask="auto")
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 19)
        c.drawString(M + 116, top - 24, self.ac["name"])
        c.setFont("Helvetica", 9)
        c.setFillColor(GREY)
        c.drawString(M + 116, top - 39, "X52 Pro button map  -  X-Plane 12")
        files = ", ".join(key_for(a) + ".x52" for a in self.ac["acfs"])
        c.setFont("Helvetica", 7)
        c.drawString(M + 116, top - 52, "Profile file: " + files)
        bw, bh = 200, 52
        bx = W - M - bw
        c.setFillColor(badge_colour)
        c.roundRect(bx, top - bh - 3, bw, bh, 6, stroke=0, fill=1)
        c.setFillColor(white)
        c.setFont("Helvetica-Bold", 17)
        c.drawString(bx + 12, top - 22, badge_title)
        c.setFont("Helvetica", 9.5)
        c.drawString(bx + 12, top - 36, badge_sub)
        c.setFont("Helvetica-Bold", 8.5)
        c.drawString(bx + 12, top - 49, pinkie_line)
        c.setStrokeColor(LIGHT)
        c.setLineWidth(0.6)
        c.line(M, top - 66, W - M, top - 66)

    def footer(self, text):
        c = self.c
        c.setStrokeColor(LIGHT)
        c.line(M, M + 22, W - M, M + 22)
        c.setFillColor(GREY)
        c.setFont("Helvetica", 7)
        y = M + 12
        for line in text:
            c.drawString(M, y, line)
            y -= 9

    # ---------------------------------------------------------------- callouts
    def entry(self, mode, button):
        if button in self.L[mode]:
            return self.L[mode][button][0], True
        if button in self.L[ANY]:
            return self.L[ANY][button][0], False
        return None, False

    def box(self, x, ytop, w, title, rows, mode, anchor=None, side="l"):
        """rows: [(tag, button)] tag is u/r/d/l, 'p' for press, '1'/'2' text tags, or ('x', text) fixed."""
        c = self.c
        lh = 11.5
        h = 12 + lh * len(rows)
        if anchor:
            ax, ay = anchor[0], anchor[1]
            r = anchor[2] if len(anchor) > 2 else 0
            sx = x + w if side == "l" else x
            dx, dy = sx - ax, (ytop - 6) - ay
            n = (dx * dx + dy * dy) ** 0.5 or 1
            ax, ay = ax + dx / n * r, ay + dy / n * r
            c.setStrokeColor(GREY)
            c.setLineWidth(0.5)
            c.line(sx, ytop - 6, ax, ay)
            c.setFillColor(INK)
            c.circle(ax, ay, 1.4, stroke=0, fill=1)
        c.setStrokeColor(INK)
        c.setLineWidth(0.7)
        c.setFillColor(white)
        c.rect(x, ytop - h, w, h, stroke=1, fill=1)
        c.setFillColor(FILL)
        c.rect(x, ytop - 12, w, 12, stroke=0, fill=1)
        c.setStrokeColor(INK)
        c.rect(x, ytop - h, w, h, stroke=1, fill=0)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 7)
        c.drawString(x + 4, ytop - 9, title)
        y = ytop - 12
        for tag, what in rows:
            c.setStrokeColor(LIGHT)
            c.setLineWidth(0.4)
            c.line(x + 16, y, x + w, y)
            cy = y - lh / 2
            c.setFillColor(INK)
            if tag in "urdl" and len(tag) == 1:
                arrow(c, x + 8, cy, tag)
            else:
                c.setFont("Helvetica-Bold", 6.5)
                c.drawCentredString(x + 8, cy - 2.3, tag)
            if isinstance(what, str):
                label, own = what, False
                c.setFillColor(GREY)
                c.setFont("Helvetica-Oblique", 7.5)
            else:
                label, own = self.entry(mode, what)
                if label is None:
                    label = "-"
                    c.setFillColor(LIGHT)
                    c.setFont("Helvetica", 8)
                elif own:
                    c.setFillColor(INK)
                    c.setFont("Helvetica-Bold", 8)
                else:
                    c.setFillColor(HexColor("#3a3a3c"))
                    c.setFont("Helvetica", 8)
            c.drawString(x + 20, cy - 2.8, label)
            y -= lh
        c.setStrokeColor(INK)
        c.setLineWidth(0.4)
        c.line(x + 16, ytop - 12, x + 16, ytop - h)
        return ytop - h

    # ---------------------------------------------------------------- devices
    def throttle(self, x0, mode):
        c = self.c
        cx = x0 + 193
        c.setFillColor(GREY)
        c.setFont("Helvetica-Bold", 10)
        c.drawCentredString(cx, 455, "THROTTLE")
        # base with the screen
        c.setFillColor(BODY)
        c.setStrokeColor(GREY)
        c.setLineWidth(0.8)
        c.roundRect(cx - 62, 70, 124, 92, 10, stroke=1, fill=1)
        c.setFillColor(LCD)
        c.rect(cx - 28, 96, 56, 40, stroke=1, fill=1)
        c.setFillColor(LCD_INK)
        c.setFont("Helvetica", 5)
        c.drawCentredString(cx, 124, "MFD")
        c.setFillColor(HexColor("#8e8e93"))
        c.circle(cx - 40, 116, 6, stroke=1, fill=1)   # page wheel
        c.circle(cx + 40, 116, 6, stroke=1, fill=1)   # right wheel
        # lever
        c.setFillColor(BODY)
        c.roundRect(cx - 34, 168, 68, 248, 16, stroke=1, fill=1)
        c.setFillColor(HexColor("#8e8e93"))
        c.circle(cx - 12, 390, 9, stroke=1, fill=1)   # throttle hat
        c.setFillColor(white)
        c.setFont("Helvetica-Bold", 5)
        c.drawCentredString(cx - 12, 388, "HAT")
        c.setFillColor(MODE_HEX[mode] if mode else GREY)
        c.circle(cx + 18, 396, 6, stroke=1, fill=1)   # E
        c.circle(cx + 20, 364, 6, stroke=1, fill=1)   # D
        c.setFillColor(HexColor("#5a5a5f"))
        c.circle(cx + 20, 332, 6, stroke=1, fill=1)   # i
        c.circle(cx - 14, 350, 5, stroke=1, fill=1)   # mouse
        c.roundRect(cx - 19, 290, 10, 26, 3, stroke=1, fill=1)   # scroll
        c.setFillColor(white)
        c.setFont("Helvetica-Bold", 6)
        for (px, py, t) in ((cx + 18, 394, "E"), (cx + 20, 362, "D"), (cx + 20, 330, "i")):
            c.drawCentredString(px, py, t)
        return cx

    def stick(self, x0, mode):
        c = self.c
        cx = x0 + 193
        c.setFillColor(GREY)
        c.setFont("Helvetica-Bold", 10)
        c.drawCentredString(cx, 455, "STICK")
        c.setFillColor(BODY)
        c.setStrokeColor(GREY)
        c.setLineWidth(0.8)
        c.roundRect(cx - 62, 70, 124, 84, 10, stroke=1, fill=1)
        c.setFillColor(HexColor("#8e8e93"))
        for i, tx in enumerate((cx - 36, cx, cx + 36)):
            c.roundRect(tx - 9, 96, 18, 28, 3, stroke=1, fill=1)
            c.setFillColor(white)
            c.setFont("Helvetica-Bold", 6)
            c.drawCentredString(tx, 106, "T%d" % (i + 1))
            c.setFillColor(HexColor("#8e8e93"))
        c.setFillColor(BODY)
        c.roundRect(cx - 28, 160, 56, 246, 14, stroke=1, fill=1)
        # trigger (front = left) and pinkie (back = right)
        c.roundRect(cx - 44, 318, 18, 34, 6, stroke=1, fill=1)
        c.roundRect(cx + 26, 210, 16, 40, 6, stroke=1, fill=1)
        c.setFillColor(HexColor("#b0b0b5"))
        c.circle(cx - 12, 390, 8, stroke=1, fill=1)   # main hat
        c.setFillColor(MODE_HEX[mode] if mode else GREY)
        c.circle(cx - 12, 360, 8, stroke=1, fill=1)   # hat 2 (lit)
        c.setFillColor(HexColor("#c8312b"))
        c.circle(cx + 12, 392, 7, stroke=1, fill=1)   # fire
        c.setFillColor(MODE_HEX[mode] if mode else GREY)
        c.circle(cx + 16, 368, 5, stroke=1, fill=1)   # A
        c.circle(cx + 16, 348, 5, stroke=1, fill=1)   # B
        c.setFillColor(HexColor("#5a5a5f"))
        c.circle(cx - 14, 300, 5, stroke=1, fill=1)   # C
        c.setFillColor(white)
        c.setFont("Helvetica-Bold", 5.5)
        for (px, py, t) in ((cx + 16, 366, "A"), (cx + 16, 346, "B"), (cx - 14, 298, "C"), (cx + 12, 390, "F"),
                            (cx - 12, 358, "H2"), (cx - 12, 388, "H1")):
            c.drawCentredString(px, py, t)
        return cx

    def mode_page(self, mode):
        c = self.c
        self.header("MODE %d" % mode, self.titles[mode], MODE_HEX[mode], "Pinkie: not pressed")
        bw = 128
        # ---- throttle (left half)
        x0 = M
        cx = self.throttle(x0, mode)
        lx, rx = x0, x0 + 386 - bw
        self.box(lx, 440, bw, "THROTTLE HAT", [("u", THAT_U), ("r", THAT_R), ("d", THAT_D), ("l", THAT_L)], mode, (cx - 12, 390, 10))
        self.box(lx, 360, bw, "MOUSE BUTTON", [("p", MOUSE)], mode, (cx - 14, 350, 6))
        self.box(lx, 318, bw, "SCROLL WHEEL", [("u", SCROLL_F), ("d", SCROLL_B), ("p", SCROLL_C)], mode, (cx - 14, 303, 6))
        self.box(lx, 160, bw, "SCREEN WHEELS (all modes)",
                 [("L", "Left wheel: change page"), ("R", "Right wheel: cursor / select")], mode, (cx - 40, 116, 7))
        self.box(rx, 440, bw, "BUTTON E", [("p", E)], mode, (cx + 18, 396, 7), side="r")
        self.box(rx, 398, bw, "BUTTON D", [("p", D)], mode, (cx + 20, 364, 7), side="r")
        self.box(rx, 356, bw, "CLUTCH (i)", [("p", "Hold: safe mode / pick profile")], mode, (cx + 20, 332, 7), side="r")
        lever = "Collective: X-Plane" if self.f["kind"] == "heli" else ("Throttle: X-Plane; slider: flaps" if self.f["flaps"] else "Throttle: X-Plane")
        self.box(rx, 250, bw, "THROTTLE LEVER", [("", lever)], mode, (cx + 34, 260), side="r")
        # ---- stick (right half)
        x0 = W / 2 + 7
        cx = self.stick(x0, mode)
        lx, rx = x0, x0 + 386 - bw
        self.box(lx, 440, bw, "HAT 2 (lit)", [("u", SHAT_U), ("r", SHAT_R), ("d", SHAT_D), ("l", SHAT_L)], mode, (cx - 12, 360, 9))
        self.box(lx, 372, bw, "TRIGGER", [("1", TRIGGER), ("2", TRIGGER2)], mode, (cx - 44, 335))
        self.box(lx, 330, bw, "BUTTON C", [("p", C)], mode, (cx - 14, 300, 6))
        self.box(lx, 290, bw, "MAIN HAT (H1)", [("", "Look around")], mode, (cx - 12, 390, 9))
        y = 440
        y = self.box(rx, y, bw, "FIRE", [("p", FIRE)], mode, (cx + 12, 392, 8), side="r") - 6
        y = self.box(rx, y, bw, "BUTTON A", [("p", A)], mode, (cx + 16, 368, 6), side="r") - 6
        y = self.box(rx, y, bw, "BUTTON B", [("p", B)], mode, (cx + 16, 348, 6), side="r") - 6
        y = self.box(rx, y, bw, "PINKIE", [("p", PINKIE)], mode, (cx + 42, 230), side="r") - 6
        y = self.box(rx, y, bw, "T1", [("u", T1U), ("d", T1D)], mode, (cx - 36, 124), side="r") - 6
        y = self.box(rx, y, bw, "T2", [("u", T2U), ("d", T2D)], mode, (cx, 124), side="r") - 6
        self.box(rx, y, bw, "T3", [("u", T3U), ("d", T3D)], mode, (cx + 36, 124), side="r")
        self.footer([
            "Bold = this mode only.  Regular = the same in every mode.  -  = not used.  Mode dial: 1 / 2 / 3.  Pinkie is a normal button (not a shift).",
            "Set every button used here to 'Do nothing' in X-Plane > Settings > Joystick.  The plugin drives the stick, throttle, slider and rotaries (E: zoom; i: speed brake where fitted). The main hat looks around (POV 1: Q, E, R, F); the throttle's mouse nub pans (arrows; all the way: Shift + arrows).",
        ])
        c.showPage()

    # ---------------------------------------------------------------- screen & lights
    def screen_page(self):
        c = self.c
        self.header("SCREEN & LIGHTS", "MFD pages, lights and alarms", HexColor("#3a3a3c"), "All modes  -  Pinkie: not pressed")
        own = pages(self.f)
        # left: page order and LCD previews
        x = M
        y = 470
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 11)
        c.drawString(x, y, "Screen pages (left wheel turns the page)")
        y -= 16
        order = BUILT_IN_PAGES + [t for _, t, _ in own] + ["Buttons", "Brightness"]
        c.setFont("Helvetica", 8)
        line = ""
        for i, name in enumerate(order):
            part = ("%d %s" % (i + 1, name)) + ("   " if i < len(order) - 1 else "")
            if c.stringWidth(line + part, "Helvetica", 8) > 380:
                c.drawString(x, y, line)
                y -= 11
                line = ""
            line += part
        c.drawString(x, y, line)
        y -= 12
        c.setFillColor(GREY)
        c.setFont("Helvetica-Oblique", 7.5)
        c.drawString(x, y, "Pages 1-7 are built in (radios, transponder, autopilot): turn the right wheel to pick, press to change.")
        y -= 10
        c.drawString(x, y, "Pages added by this profile, shown below (right wheel scrolls the rows):")
        y -= 14
        lw, col = 120, 0
        top = y
        for pid, title, rows in own:
            lh = 10
            h = 16 + lh * len(rows)
            lx = x + col * (lw + 10)
            c.setFillColor(LCD)
            c.setStrokeColor(GREY)
            c.roundRect(lx, top - h, lw, h, 4, stroke=1, fill=1)
            c.setFillColor(LCD_INK)
            c.setFont("Courier-Bold", 8)
            c.drawString(lx + 6, top - 11, title)
            yy = top - 11
            c.setFont("Courier", 7.5)
            for label, fmt, ref in rows:
                yy -= lh
                sample = re.sub(r"%[-0-9.]*[dfeg]", "###", fmt)
                c.drawString(lx + 6, yy, label)
                c.drawRightString(lx + lw - 6, yy, sample)
            col += 1
        # controls of the screen
        y = 140
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 9)
        c.drawString(x, y, "Screen controls")
        c.setFont("Helvetica", 8)
        for t in ["Left wheel: turn = next / previous page.",
                  "Right wheel: turn = move the cursor;  press = act ([118.000] swaps, > picks a part to change).",
                  "Function / Start-Stop / Reset under the screen: the stick's own clocks and stopwatch.",
                  "Clutch (i) held + main hat up/down: scroll profiles; right: use it; left: back to automatic."]:
            y -= 12
            c.drawString(x, y, t)
        if self.f["master"]:
            y -= 16
            c.setFillColor(GREY)
            c.setFont("Helvetica-Oblique", 7.5)
            c.drawString(x, y, "Screen and lights are on only while the avionics are powered (master switch in the profile).")

        # right: lights
        x = W / 2 + 14
        y = 470
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 11)
        c.drawString(x, y, "Button lights by mode")
        y -= 18
        names = [l for l in LED_ORDER]
        cw = 31
        c.setFont("Helvetica-Bold", 6.5)
        for i, l in enumerate(names):
            c.drawCentredString(x + 52 + i * cw, y, LED_TITLES[l])
        for m in (1, 2, 3):
            y -= 14
            c.setFillColor(INK)
            c.setFont("Helvetica-Bold", 8)
            c.drawString(x, y - 3, "Mode %d" % m)
            leds = mode_leds(m)
            for i, l in enumerate(names):
                c.setFillColor(LED_HEX[leds[l]])
                c.circle(x + 52 + i * cw, y, 4.2, stroke=0, fill=1)
        y -= 14
        c.setFillColor(GREY)
        c.setFont("Helvetica-Oblique", 7)
        c.drawString(x, y, "Fire and Throttle lights have one colour: lit or off.")

        y -= 22
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 11)
        c.drawString(x, y, "Live lights (follow the aircraft)")
        c.setFont("Helvetica", 8)
        for _, _, words in rules(self.f):
            y -= 12
            c.drawString(x + 6, y, "- " + words)

        y -= 22
        c.setFont("Helvetica-Bold", 11)
        c.drawString(x, y, "Alarms (top one wins)")
        y -= 14
        c.setFont("Helvetica-Bold", 7)
        c.setFillColor(GREY)
        c.drawString(x + 6, y, "WHEN")
        c.drawString(x + 170, y, "LIGHT")
        c.drawString(x + 225, y, "SHOWS")
        c.setFont("Helvetica", 8)
        c.setFillColor(INK)
        for i, (_, name, light, effect) in enumerate(alarms(self.f)):
            y -= 12
            c.drawString(x + 6, y, "%d. %s" % (i + 1, name))
            c.drawString(x + 170, y, light)
            c.drawString(x + 225, y, effect)
        y -= 14
        c.setFillColor(GREY)
        c.setFont("Helvetica-Oblique", 7)
        c.drawString(x, y, "Order of importance: alarms, then live lights, then the mode colours.")
        self.footer([
            "Profiles are plain text: edit them in the X52 Pro menu bar app (Buttons, Live lights, Alarms tabs) or in X-Plane (Plugins > X52 Pro).",
            "Built from profiles/tools; the picture is the aircraft's own icon from the X-Plane folder.",
        ])
        c.showPage()


def build(ac):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "%s - X52 Pro map.pdf" % ac["name"])
    c = canvas.Canvas(path, pagesize=(W, H))
    c.setTitle("%s - X52 Pro button map" % ac["name"])
    c.setAuthor("X52 Pro for X-Plane 12")
    s = Sheet(c, ac)
    for m in (1, 2, 3):
        s.mode_page(m)
    s.screen_page()
    c.save()
    return path


if __name__ == "__main__":
    for ac in AIRCRAFT:
        print(build(ac))
