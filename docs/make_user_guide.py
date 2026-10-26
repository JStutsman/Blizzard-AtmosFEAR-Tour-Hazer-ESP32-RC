"""Builds the Hazer Remote user guide PDF.

Usage (needs reportlab):  python docs/make_user_guide.py docs/Hazer-Remote-User-Guide.pdf
"""
import sys
from reportlab.lib.pagesizes import letter
from reportlab.lib import colors
from reportlab.lib.units import inch
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table,
                                TableStyle, KeepTogether)
from reportlab.graphics.shapes import Drawing, Rect, String, Circle, Line, Group
from reportlab.platypus.flowables import Flowable

OUT = sys.argv[1]

# ---------------------------------------------------------------- styles
ss = getSampleStyleSheet()
ACCENT = colors.HexColor("#1f5fbf")
INK = colors.HexColor("#1b1f24")
MUTED = colors.HexColor("#5b6470")
RULE = colors.HexColor("#d5dae1")
TINT = colors.HexColor("#eef3fb")
WARN = colors.HexColor("#fff4e0")

body = ParagraphStyle("body", parent=ss["BodyText"], fontName="Helvetica", fontSize=10.5,
                      leading=15, textColor=INK, spaceAfter=6)
small = ParagraphStyle("small", parent=body, fontSize=9, leading=12.5, textColor=MUTED)
h1 = ParagraphStyle("h1", parent=ss["Heading1"], fontName="Helvetica-Bold", fontSize=20,
                    leading=24, textColor=ACCENT, spaceBefore=4, spaceAfter=10)
h2 = ParagraphStyle("h2", parent=ss["Heading2"], fontName="Helvetica-Bold", fontSize=13.5,
                    leading=18, textColor=INK, spaceBefore=12, spaceAfter=5)
bullet = ParagraphStyle("bullet", parent=body, leftIndent=16, bulletIndent=4, spaceAfter=3)
step = ParagraphStyle("step", parent=body, leftIndent=20, bulletIndent=2, spaceAfter=4)
cell = ParagraphStyle("cell", parent=body, fontSize=9.5, leading=12.5, spaceAfter=0)
cellb = ParagraphStyle("cellb", parent=cell, fontName="Helvetica-Bold")
note = ParagraphStyle("note", parent=body, fontSize=10, leading=14, spaceAfter=0)
caption = ParagraphStyle("caption", parent=small, alignment=TA_CENTER, spaceBefore=4, spaceAfter=10)

story = []
P = lambda t, s=body: story.append(Paragraph(t, s))

def bullets(items):
    for it in items:
        story.append(Paragraph(it, bullet, bulletText="•"))

def steps(items):
    for i, it in enumerate(items, 1):
        story.append(Paragraph(it, step, bulletText=f"{i}."))

def table(rows, widths, header=True):
    data = [[Paragraph(str(c), cellb if (header and r == 0) else cell) for c in row]
            for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    st = [("GRID", (0, 0), (-1, -1), 0.5, RULE),
          ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
          ("LEFTPADDING", (0, 0), (-1, -1), 6), ("RIGHTPADDING", (0, 0), (-1, -1), 6)]
    if header:
        st.append(("BACKGROUND", (0, 0), (-1, 0), TINT))
    t.setStyle(TableStyle(st))
    story.append(t)
    story.append(Spacer(1, 8))

def callout(text, bg=TINT):
    t = Table([[Paragraph(text, note)]], colWidths=[6.5 * inch])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), bg),
                           ("BOX", (0, 0), (-1, -1), 0.5, RULE),
                           ("LEFTPADDING", (0, 0), (-1, -1), 10), ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                           ("TOPPADDING", (0, 0), (-1, -1), 8), ("BOTTOMPADDING", (0, 0), (-1, -1), 8)]))
    story.append(t)
    story.append(Spacer(1, 8))

# ---------------------------------------------------------------- screen mockups
# Drawn in the remote's own 320 x 240 pixel coordinates (y down), scaled to fit.
class Screen(Flowable):
    def __init__(self, draw_fn, scale=1.0):
        super().__init__()
        self.draw_fn, self.s = draw_fn, scale
        self.hAlign = "CENTER"
        self.width, self.height = 320 * scale, 240 * scale

    def wrap(self, aw, ah):
        return self.width, self.height

    def draw(self):
        c = self.canv
        s = self.s
        c.saveState()
        c.translate((self.width - 320 * s) / 2, 0)
        c.scale(s, s)
        c.translate(0, 240)
        c.scale(1, -1)                       # screen coordinates: y grows downward
        c.setFillColor(colors.black)
        c.roundRect(0, 0, 320, 240, 6, stroke=0, fill=1)
        self.draw_fn(c)
        c.restoreState()

def text(c, s, x, y, size=10, col=colors.white, align="center", bold=False):
    c.saveState()
    c.translate(x, y)
    c.scale(1, -1)
    c.setFillColor(col)
    c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
    if align == "center":
        c.drawCentredString(0, -size * 0.35, s)
    elif align == "left":
        c.drawString(0, -size * 0.35, s)
    else:
        c.drawRightString(0, -size * 0.35, s)
    c.restoreState()

def button(c, x, y, w, h, label, bg=colors.HexColor("#3c3c46"), size=9):
    c.setFillColor(bg)
    c.setStrokeColor(colors.white)
    c.setLineWidth(0.8)
    c.roundRect(x, y, w, h, 6, stroke=1, fill=1)
    text(c, label, x + w / 2, y + h / 2, size)

def gear(c, x, y, col):
    import math
    c.setFillColor(col)
    for i in range(8):
        a = i * math.pi / 4
        c.circle(x + 9 * math.cos(a), y + 9 * math.sin(a), 3, stroke=0, fill=1)
    c.circle(x, y, 8, stroke=0, fill=1)
    c.setFillColor(colors.HexColor("#145ac8"))
    c.circle(x, y, 3, stroke=0, fill=1)

def bars(c, x, y, level):
    for i in range(4):
        h = 3 + i * 2
        c.setFillColor(colors.HexColor("#78ff8c") if i < level else colors.HexColor("#0f3c82"))
        c.rect(x + i * 4, y - h, 3, h, stroke=0, fill=1)

def main_screen(c):
    blue = colors.HexColor("#145ac8")
    c.setFillColor(blue)
    c.setStrokeColor(colors.white)
    c.roundRect(8, 8, 304, 88, 8, stroke=1, fill=1)
    gear(c, 28, 28, colors.white)
    text(c, "Haze On", 150, 26, 17)
    text(c, "Vol 50%  Fan 90%", 160, 56, 15)
    c.setFillColor(colors.red)
    c.circle(296, 22, 7, stroke=0, fill=1)
    info = colors.HexColor("#bed7ff")
    text(c, "RX1 Ch89 -57dBm", 52, 83, 8.5, info, "left")
    bars(c, 128, 88, 3)
    text(c, "|", 152, 83, 8.5, info)
    text(c, "RX2 Ch109 -67dBm", 160, 83, 8.5, info, "left")
    bars(c, 242, 88, 2)
    labels = ["FUNCTION", "UP", "DOWN", "TIMER", "START/STOP", "MANUAL"]
    for i, l in enumerate(labels):
        bg = colors.HexColor("#e68c14") if l == "START/STOP" else colors.HexColor("#3c3c46")
        button(c, [8, 112, 216][i % 3], [104, 170][i // 3], 96, 58, l, bg)

def settings_screen(c):
    text(c, "Settings", 160, 16, 15)
    rows = [("Receivers", "RX 1 + 2", "<", ">"), ("RX1 DMX", "89", "-", "+"),
            ("RX2 DMX", "109 *", "-", "+"), ("Display", "Dark", "<", ">"),
            ("Backlight", "30%", "-", "+")]
    for r, (lab, val, a, b) in enumerate(rows):
        y = 34 + r * 34
        text(c, lab, 10, y + 15, 9, colors.HexColor("#9696a0"), "left")
        text(c, val, 92, y + 15, 14, colors.white, "left")
        button(c, 214, y, 44, 30, a, size=13)
        button(c, 266, y, 44, 30, b, size=13)
    button(c, 10, 206, 300, 30, "DONE", colors.HexColor("#1e8c3c"), 12)

def numpad_screen(c):
    text(c, "RX1 DMX address", 160, 14, 14)
    text(c, "89", 160, 46, 17)
    text(c, "1-510", 312, 46, 9, colors.HexColor("#9696a0"), "right")
    keys = [("1", 0, 0, 1), ("2", 1, 0, 1), ("3", 2, 0, 1), ("DEL", 3, 0, 1),
            ("4", 0, 1, 1), ("5", 1, 1, 1), ("6", 2, 1, 1), ("CLR", 3, 1, 1),
            ("7", 0, 2, 1), ("8", 1, 2, 1), ("9", 2, 2, 1), ("CANCEL", 3, 2, 1),
            ("0", 0, 3, 3), ("OK", 3, 3, 1)]
    KX, KY = [6, 84, 162, 240], [68, 112, 156, 200]
    for lab, col, row, span in keys:
        w = KX[col + span - 1] + 74 - KX[col]
        bg = colors.HexColor("#1e8c3c") if lab == "OK" else colors.HexColor("#3c3c46")
        button(c, KX[col], KY[row], w, 40, lab, bg, 13 if len(lab) == 1 else 9)

def take_screen(c):
    text(c, "Take control", 160, 16, 15)
    text(c, "RX1: in use by another remote", 160, 44, 9.5, colors.HexColor("#ffbe50"))
    text(c, "RX2: controlled by this remote", 160, 64, 9.5)
    text(c, "This remote's haze settings apply at once.", 160, 94, 9, colors.HexColor("#9696a0"))
    for i, l in enumerate(["RX 1", "RX 2", "BOTH"]):
        button(c, [10, 112, 214][i], 110, 96, 50, l, colors.HexColor("#c86e14"), 13)
    button(c, 10, 176, 300, 50, "CANCEL", size=12)

# ================================================================ content
# ---------------- cover
story.append(Spacer(1, 1.6 * inch))
story.append(Paragraph("Hazer Remote", ParagraphStyle("t", parent=h1, fontSize=34, leading=40,
                                                       alignment=TA_CENTER)))
story.append(Paragraph("User Guide", ParagraphStyle("t2", parent=h1, fontSize=20, leading=26,
                                                     textColor=INK, alignment=TA_CENTER)))
story.append(Spacer(1, 10))
story.append(Paragraph("Wireless touchscreen remote for the Blizzard AtmosFEAR Tour HZ haze machine",
                       ParagraphStyle("t3", parent=body, alignment=TA_CENTER, textColor=MUTED, fontSize=12)))
story.append(Spacer(1, 0.4 * inch))
story.append(Screen(main_screen, 1.15))
story.append(Spacer(1, 0.5 * inch))
story.append(Paragraph("Firmware v1.1 (remote and receiver) &nbsp;&middot;&nbsp; October 2026",
                       ParagraphStyle("t4", parent=small, alignment=TA_CENTER)))
story.append(PageBreak())

# ---------------- contents
story.append(Paragraph("Contents", h1))
toc = ["1. How the system works", "2. Quick start", "3. The remote's screen",
       "4. Running the hazer", "5. Status messages", "6. Settings (gear icon)",
       "7. Typing values on the number pad", "8. Two receivers and DMX addresses",
       "9. More than one remote", "10. The receiver", "11. Receiver setup mode",
       "12. Debug screen", "13. Troubleshooting", "14. Serial commands", "15. Reference"]
for t in toc:
    P(t)
story.append(PageBreak())

# ---------------- 1
story.append(Paragraph("1. How the system works", h1))
P("The system replaces the hazer's front panel with a touchscreen you can carry around the venue. "
  "It has two kinds of devices:")
bullets([
    "<b>Remote (transmitter)</b> - a 2.8\" touchscreen (ESP32-2432S028R, \"CYD\"). Its screen copies the "
    "hazer's front panel: a blue display and six buttons.",
    "<b>Receiver</b> - a small ESP32-C3 board with an RS485 module, plugged into the hazer's DMX input. "
    "It turns the remote's commands into the DMX signal the hazer understands.",
])
P("The remote and receivers talk directly to each other over ESP-NOW, a WiFi-based radio link. There is no "
  "router, no app and no pairing step. The remote sends the haze, fan and volume values 20 times a second; "
  "each receiver answers 4 times a second with its status, so the remote always knows whether the link "
  "is working.")
P("One remote can control one or two receivers (two hazers), and two or more remotes can share the same "
  "receivers. Only one remote controls a given receiver at a time (see section 9).")
callout("<b>Safety built in.</b> If a receiver hears nothing from its remote for 3 seconds - out of range, "
        "remote switched off, battery flat - it switches its outputs to zero and the hazer stops. It also "
        "keeps the outputs at zero for the first 2 seconds after power-up and while it is in setup mode.")

# ---------------- 2
story.append(Paragraph("2. Quick start", h1))
steps([
    "On the hazer, set its DMX address to <b>510</b> (\"DMX512 Add\" in the hazer's own menu). This is the "
    "receiver's factory address. You can choose other addresses later (section 8).",
    "Connect the receiver's DMX cable to the hazer's DMX input and power the receiver. Its LED gives a "
    "double blink while it waits for a remote.",
    "Power the remote. The blue display shows <b>No Link</b> briefly, then <b>Ready Haze</b> once the "
    "receiver answers. The bottom line shows the receiver's signal strength.",
    "Press <b>START/STOP</b>. The button turns orange, the display shows <b>Haze On</b> and a red dot "
    "appears in its top-right corner. The hazer starts.",
    "Press <b>START/STOP</b> again to stop.",
])
P("If you have two hazers, tap the gear icon, set <b>Receivers</b> to <b>RX 1 + 2</b>, and give each "
  "receiver its own DMX address (section 8).")
story.append(PageBreak())

# ---------------- 3
story.append(Paragraph("3. The remote's screen", h1))
story.append(Screen(main_screen, 1.05))
story.append(Paragraph("Main screen with two receivers selected and haze on", caption))
table([
    ["Area", "What it shows or does"],
    ["Top line", "Current state: Ready Haze, Haze On, Timer Wait, No Link and so on (section 5). On a "
                 "FUNCTION page it shows the page name instead."],
    ["Second line", "Volume and fan on the main page, or the value being set on a FUNCTION page. "
                    "Tap it on a FUNCTION page to type the value (section 7)."],
    ["Bottom line", "One entry per selected receiver: name, DMX address, signal strength and a signal "
                    "icon. Orange text means a problem (fail-safe or controlled by another remote). "
                    "A red X means no link. Tap it to open <b>Take control</b> (section 9)."],
    ["Gear (top-left)", "Tap to open Settings (section 6)."],
    ["Red dot (top-right)", "The remote is telling a receiver to haze right now."],
    ["Six buttons", "FUNCTION, UP, DOWN, TIMER, START/STOP, MANUAL - laid out like the hazer's panel. "
                    "An active mode lights orange; a pressed button lights green."],
], [1.4 * inch, 5.1 * inch])
P("<b>Hidden gestures on the blue display:</b> a short tap on the gear opens Settings; a short tap on the "
  "bottom line opens Take control; holding anywhere on the display for 2 seconds opens the PIN screen "
  "(sections 11 and 12).")
story.append(PageBreak())

# ---------------- 4
story.append(Paragraph("4. Running the hazer", h1))
table([
    ["Button", "What it does"],
    ["START/STOP", "Turns haze on and leaves it on. Press again to stop. Lights orange while on."],
    ["MANUAL", "Haze only while you hold the button. Let go to stop."],
    ["TIMER", "Runs automatic cycles: wait for the <b>Interval</b>, haze for the <b>Duration</b>, "
              "repeat. The display shows Timer Wait or Timer Haze and a countdown. Press TIMER again to stop."],
    ["FUNCTION", "Steps through the hazer's settings pages (below). Each press saves the page you leave."],
    ["UP / DOWN", "Change the value on the current page. Hold to repeat."],
], [1.3 * inch, 5.2 * inch])
story.append(Paragraph("FUNCTION pages", h2))
P("These are the same pages as on the hazer itself, in the same order:")
table([
    ["Page", "Range", "Meaning"],
    ["(main)", "-", "Status, volume and fan."],
    ["Interval Set", "0-200 s", "Timer: pause between bursts."],
    ["Duration Set", "0-200 s", "Timer: length of each burst."],
    ["Volume Set", "1-100 %", "Haze output."],
    ["FanSpeed Set", "1-100 %", "Fan speed."],
], [1.4 * inch, 1.1 * inch, 4.0 * inch])
P("The remote runs the timer itself because the hazer locks its own buttons while it receives DMX. "
  "All values are saved in the remote and kept when it is switched off.")
callout("Changing Volume or Fan takes effect immediately, even while hazing.")

# ---------------- 5
story.append(Paragraph("5. Status messages", h1))
table([
    ["Top line", "Meaning", "What to do"],
    ["Ready Haze", "Linked and idle.", "-"],
    ["Haze On / Manual Haze", "Hazing (latched / while held).", "-"],
    ["Timer Wait / Timer Haze", "Timer running; countdown below.", "Press TIMER to stop."],
    ["No Link", "No selected receiver is answering.", "Check the receiver has power and is in range."],
    ["In Use", "Every selected receiver is controlled by another remote.",
     "Tap the bottom line to take control (section 9)."],
    ["Rx Fail-safe", "A receiver is linked but has not had data yet, or has just been released.",
     "Usually clears within a second."],
    ["Rx Config Mode", "A receiver is in setup mode. Haze is disabled.",
     "Finish setup or wait 10 minutes (section 11)."],
    ["Radio Error", "The remote's radio failed to start, or no receiver addresses are set.",
     "Restart the remote; see section 13."],
], [1.6 * inch, 2.6 * inch, 2.3 * inch])
P("If only one of two receivers drops out, the top lines keep showing what the working receiver is doing; "
  "the problem shows on the bottom line, e.g. <i>RX2 Ch109 No link</i> with a red X.")
story.append(PageBreak())

# ---------------- 6
story.append(KeepTogether([Paragraph("6. Settings (gear icon)", h1),
                           Screen(settings_screen, 0.95), Paragraph("Settings screen", caption)]))
P("Tap the gear in the top-left of the blue display. Each row has its value and two buttons. "
  "Tap a number to type it instead (section 7).")
table([
    ["Row", "Values", "Notes"],
    ["Receivers", "RX 1, RX 2, RX 1 + 2", "Which receivers this remote controls. A receiver you deselect "
                                          "stops at once."],
    ["RX1 DMX / RX2 DMX", "1-510", "DMX address of each receiver. Hold +/- to speed up to steps of 10. "
                                   "A * means not sent yet; it is sent when you press DONE."],
    ["Display", "Normal, Dark", "Dark uses dim colors for dark venues."],
    ["Backlight", "5-100 %", "Screen brightness. Normal and Dark each remember their own level "
                             "(defaults 100 % and 30 %)."],
], [1.4 * inch, 1.5 * inch, 3.6 * inch])
P("<b>DONE</b> saves everything and returns to the panel. The screen also closes and saves by itself after "
  "30 seconds without a touch. Receivers, Display and Backlight change as soon as you tap them.")

# ---------------- 7
story.append(KeepTogether([Paragraph("7. Typing values on the number pad", h1),
                           Screen(numpad_screen, 0.9), Paragraph("Number pad", caption)]))
P("Tap the value on a FUNCTION page (Interval, Duration, Volume, FanSpeed) or the number on the "
  "RX1 DMX, RX2 DMX or Backlight row in Settings.")
bullets([
    "The pad opens with the current value. <b>DEL</b> removes the last digit, <b>CLR</b> clears.",
    "The allowed range is shown on the right. <b>OK</b> only accepts a value in range; otherwise the "
    "range turns orange and the pad stays open.",
    "<b>CANCEL</b>, or 30 seconds without a touch, closes the pad without changing anything.",
])
story.append(PageBreak())

# ---------------- 8
story.append(Paragraph("8. Two receivers and DMX addresses", h1))
P("Each hazer uses three DMX channels, starting at its address: haze on/off, fan, volume. "
  "A receiver set to address 89 drives channels 89, 90 and 91.")
story.append(Paragraph("Example: two hazers on 89 and 109", h2))
steps([
    "On hazer 1 set \"DMX512 Add\" to <b>89</b>; on hazer 2 set it to <b>109</b>.",
    "On the remote, tap the gear and set <b>Receivers</b> to <b>RX 1 + 2</b>.",
    "Set <b>RX1 DMX</b> to 89 and <b>RX2 DMX</b> to 109 (with +/- or the number pad).",
    "Press <b>DONE</b>. Within a second both receivers switch; the bottom line shows "
    "<i>RX1 Ch89 | RX2 Ch109</i>.",
])
callout("<b>The address must match on both ends.</b> If the receiver sends on channels 89-91 but the "
        "hazer listens on 510, the hazer ignores it.", WARN)
story.append(Paragraph("Receivers that were off", h2))
P("The remote remembers the address you set for each receiver. If a receiver was off or out of range when "
  "you changed it, the remote sends the new address as soon as that receiver answers again. The receiver "
  "stores the address, so it survives power cycles.")
P("Because of this, an address you set on the remote wins over one typed on the receiver's setup page. "
  "With several remotes, give every remote the same addresses (section 9).")

# ---------------- 9
story.append(Paragraph("9. More than one remote", h1))
P("Any number of remotes can be used with the same receivers, but each receiver follows only <b>one</b> "
  "remote at a time - the one in control. This stops two people fighting over the same hazer.")
bullets([
    "<b>The first remote to send to a receiver controls it.</b> Other remotes are ignored and show "
    "<i>In use</i> in orange for that receiver.",
    "<b>Automatic handover:</b> when the controlling remote is switched off, goes out of range or "
    "deselects the receiver, another remote can control it after 3 seconds.",
    "<b>Take control on purpose:</b> tap the bottom line of the blue display. The Take control screen "
    "shows each receiver's state; tap <b>RX 1</b>, <b>RX 2</b> or <b>BOTH</b>. The other remote "
    "switches to <i>In use</i> straight away.",
])
story.append(KeepTogether([Screen(take_screen, 0.75), Paragraph("Take control screen", caption)]))
callout("Taking control applies <b>this</b> remote's haze settings at once. If it is set to Haze On, "
        "the hazer starts. Taking a receiver also adds it to this remote's Receivers setting.", WARN)
P("Only the remote in control can change a receiver's DMX address. Keep the same DMX addresses in every "
  "remote's Settings, or a receiver's address changes when control passes between remotes.")
story.append(PageBreak())

# ---------------- 10
story.append(Paragraph("10. The receiver", h1))
story.append(Paragraph("Wiring", h2))
table([
    ["ESP32-C3 pin", "Connects to", "Purpose"],
    ["GPIO10", "RS485 module DI", "DMX data out"],
    ["GPIO7", "RS485 module DE and RE (tied together)", "Held high = transmit"],
    ["GPIO8", "On-board LED (C3 SuperMini, active low)", "Status LED"],
    ["RS485 A / B", "DMX pin 3 (+) / pin 2 (-), ground to pin 1", "DMX cable to the hazer"],
], [1.3 * inch, 2.9 * inch, 2.3 * inch])
story.append(Paragraph("Status LED", h2))
table([
    ["Pattern", "Meaning"],
    ["One short blink", "Linked to a remote, receiving data."],
    ["Double blink", "Fail-safe: no data (no remote, or remote released it). Outputs are zero."],
    ["Fast flicker", "Setup mode (section 11)."],
    ["Slow blink", "Identify."],
    ["N quick blinks at power-up", "Counting quick power cycles toward setup mode."],
], [2.0 * inch, 4.5 * inch])
story.append(Paragraph("Fail-safe", h2))
P("Outputs go to zero (hazer stops) when no data arrives for 3 seconds (adjustable on the setup page), "
  "for the first 2 seconds after power-up, and while in setup mode. A remote deselecting the receiver "
  "also zeroes it immediately.")

# ---------------- 11
story.append(Paragraph("11. Receiver setup mode", h1))
P("Setup mode turns the receiver into a WiFi access point with a web page for its settings. "
  "Haze is disabled while it is active. There are two ways in:")
bullets([
    "<b>From the remote:</b> hold the blue display for 2 seconds, enter PIN <b>4817</b>, confirm. The "
    "selected receivers enter setup mode.",
    "<b>At the receiver:</b> power-cycle it 5 times quickly (each time on for less than 2 seconds). "
    "The LED starts flickering fast.",
])
P("Then join the WiFi network <b>Hazer-RX-Setup-&lt;MAC&gt;</b> with password <b>48174817</b> and open "
  "<b>http://192.168.4.1</b> in a browser.")
table([
    ["Setting", "Notes"],
    ["DMX base address", "1-510. Emit = base, fan = base+1, volume = base+2. Normally set from the remote."],
    ["Fail-safe timeout", "0.5-60 seconds (default 3)."],
    ["Radio channel", "1-13, default 6. Must match the remotes (set in their firmware)."],
    ["Allowed remote MAC", "Leave blank to accept any remote. Fill in to lock the receiver to one "
                           "remote; other remotes then show No link."],
    ["Bench test", "Haze switch and fan/volume sliders to test the hazer without a remote."],
    ["Restore defaults / Exit", "Exit restarts the receiver into normal operation."],
], [1.7 * inch, 4.8 * inch])
P("Setup mode ends by itself after 10 minutes without activity.")
callout("The PIN, setup password and radio channel are set in the firmware's Config.h files. Change them "
        "there and re-flash if you need different values.")
story.append(PageBreak())

# ---------------- 12
story.append(Paragraph("12. Debug screen", h1))
P("Hold the blue display for 2 seconds and enter PIN <b>1234</b>. The debug screen updates twice a second; "
  "tap anywhere to close. Haze control keeps running while it is open.")
table([
    ["Line", "Shows"],
    ["TX firmware", "Remote firmware version and build date."],
    ["MAC / ch / Up", "This remote's radio address, radio channel and time since power-up."],
    ["Heap / Send", "Free memory and which receivers this remote is sending to."],
    ["Out", "What the remote is sending: emit on/off, fan, volume."],
    ["RX1 / RX2", "Each receiver's address and whether it is selected; then LINK UP / NO LINK, receiver "
                  "firmware version, DMX address, time since its last reply; then signal strength at "
                  "both ends, the values it is actually outputting, and flags: FS fail-safe, CFG setup "
                  "mode, ID identify, BUSY controlled by another remote."],
], [1.4 * inch, 5.1 * inch])

# ---------------- 13
story.append(Paragraph("13. Troubleshooting", h1))
table([
    ["Problem", "Check"],
    ["No Link on the remote", "Receiver powered? LED double-blinking? Within range? Same radio channel "
                              "(default 6) on both? Receiver not locked to another remote's MAC?"],
    ["Linked but the hazer does nothing", "Hazer DMX address matches the receiver's (bottom line, Ch...)? "
                                          "DMX cable and RS485 wiring (section 10)? Try the Bench test "
                                          "in setup mode."],
    ["Shows In use", "Another remote controls that receiver. Take control (section 9) or switch the "
                     "other remote off and wait 3 seconds."],
    ["DMX address keeps changing back", "Another remote has a different address saved for that "
                                        "receiver. Make all remotes' Settings match."],
    ["Hazer stops by itself", "Signal dropping out (check the dBm and bars); fail-safe stops the hazer "
                              "after 3 s without data. Move the remote closer or raise the receiver."],
    ["Screen too dim / too bright", "Settings, Backlight. Dark mode has its own level."],
    ["Touches land in the wrong place", "Touch calibration: set TOUCH_CAL_ON_BOOT in the remote's "
                                        "Config.h, flash, and read the values from the serial monitor."],
    ["Display blank", "In the remote firmware, change Panel_ILI9341 to Panel_ST7789 in Display.h."],
], [2.0 * inch, 4.5 * inch])
story.append(PageBreak())

# ---------------- 14
story.append(Paragraph("14. Serial commands", h1))
P("Everything the touchscreen does can also be done from a computer. Connect the remote by USB and open a "
  "serial terminal at <b>115200 baud</b> (for example <font face='Courier'>pio device monitor -b 115200</font>, "
  "or the Arduino serial monitor). Type a command and press Enter; type <b>help</b> for the list. "
  "Commands are not case-sensitive. The screen updates to match.")
table([
    ["Command", "What it does"],
    ["start / stop", "Haze on (latched) / stop everything, including the timer and manual."],
    ["manual on | off | <i>seconds</i>", "Hold MANUAL. With a number (1-60) it releases by itself."],
    ["timer on | off", "Interval/duration cycles."],
    ["volume <i>1-100</i>, fan <i>1-100</i>", "Set volume or fan %."],
    ["interval <i>0-200</i>, duration <i>0-200</i>", "Set the timer values in seconds."],
    ["function, up, down", "Press those panel buttons."],
    ["page main|interval|duration|volume|fan", "Go to a FUNCTION page."],
    ["rx 1 | 2 | both", "Receivers to control (Settings)."],
    ["dmx <i>1|2</i> <i>1-510</i>", "DMX address of a receiver (Settings). Sent while it is linked and "
                                    "controlled by this remote."],
    ["display normal | dark, backlight <i>5-100</i>", "Display settings."],
    ["take 1 | 2 | both", "Take control from another remote."],
    ["config <i>PIN</i>", "Send the selected receivers into setup mode (stops the haze)."],
    ["identify 1 | 2", "Blink a receiver's LED for 3 seconds."],
    ["status", "State, values, settings and one line per receiver."],
    ["screen", "The text currently on the blue display."],
    ["debug", "Everything on the debug screen."],
    ["watch <i>seconds</i> | off", "Print the status repeatedly."],
    ["reboot", "Restart the remote."],
], [2.6 * inch, 3.9 * inch])
P("Example: <font face='Courier'>rx both</font>, <font face='Courier'>dmx 1 89</font>, "
  "<font face='Courier'>dmx 2 109</font>, <font face='Courier'>status</font>.")
story.append(PageBreak())

# ---------------- 15
story.append(Paragraph("15. Reference", h1))
story.append(Paragraph("Specifications", h2))
table([
    ["Item", "Value"],
    ["Radio", "ESP-NOW on WiFi channel 6 (configurable)"],
    ["Data rate", "Remote sends 20 times/s; receivers reply 4 times/s"],
    ["Link timeout on the remote", "1.5 s without a reply = No link"],
    ["Receiver fail-safe", "3 s without data (0.5-60 s on the setup page)"],
    ["Control handover", "3 s after the controlling remote goes silent"],
    ["DMX output", "Continuous DMX512, 3 channels per hazer: emit (on at 128 or more), fan, volume"],
    ["Remote hardware", "ESP32-2432S028R (\"CYD\"), 2.8\" 320x240 touchscreen"],
    ["Receiver hardware", "ESP32-C3 (SuperMini) + RS485 module"],
], [2.2 * inch, 4.3 * inch])
story.append(Paragraph("Your devices", h2))
table([
    ["Device", "Radio MAC address"],
    ["Receiver 1 (RX1)", "E8:3D:C1:81:F2:F8"],
    ["Receiver 2 (RX2)", "E8:3D:C1:83:83:F0"],
    ["Remote 1", "70:4B:CA:7B:96:E4"],
    ["Remote 2", "B4:BF:E9:10:B6:54"],
], [2.2 * inch, 4.3 * inch])
P("Receiver addresses are compiled into the remote firmware (RX_MAC_RECEIVER_1/2 in Hazer-TX/Config.h). "
  "A new receiver needs its MAC added there; it prints the MAC on its serial monitor at power-up.", small)
story.append(Paragraph("Updating the firmware", h2))
P("The firmware is built with VS Code + PlatformIO. Open <i>Hazer-Remote.code-workspace</i>, plug the device "
  "into USB, and upload:")
bullets([
    "Remote: <font face='Courier'>pio run -d remote-controller -t upload --upload-port COMxx</font>",
    "Receiver: <font face='Courier'>pio run -d receiver -t upload --upload-port COMxx</font>",
])
P("Settings stored on a device (values, DMX addresses, display) are kept across updates. Update remotes and "
  "receivers together: v1.1 remotes need v1.1 receivers for the In use / Take control features.")
P("<b>Source code:</b> remote - github.com/JStutsman/Blizzard-AtmosFEAR-Tour-Hazer-ESP32-RC; "
  "receiver - github.com/JStutsman/Blizzard-AtmosFEAR-Tour-Hazer-ESP32-RC-Receiver", small)

# ---------------------------------------------------------------- build
def footer(c, doc):
    c.saveState()
    c.setFont("Helvetica", 8.5)
    c.setFillColor(MUTED)
    if doc.page > 1:
        c.drawString(0.75 * inch, 0.5 * inch, "Hazer Remote - User Guide (v1.1)")
        c.drawRightString(letter[0] - 0.75 * inch, 0.5 * inch, f"Page {doc.page}")
    c.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=letter, leftMargin=inch, rightMargin=inch,
                        topMargin=0.8 * inch, bottomMargin=0.8 * inch,
                        title="Hazer Remote - User Guide", author="JStutsman",
                        subject="Wireless remote for the Blizzard AtmosFEAR Tour HZ hazer")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print("wrote", OUT)
