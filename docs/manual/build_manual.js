// Builds X52-Pro-Manual.docx next to this file.
//   node docs/manual/build_manual.js        (needs the "docx" npm package)
// The pictures come from docs/images (made by `X52Pro --snapshot`), the version from the VERSION file.
const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel, AlignmentType, ImageRun, Table, TableRow, TableCell,
  WidthType, ShadingType, BorderStyle, LevelFormat, Footer, Header, PageNumber, PageBreak, Bookmark,
  InternalHyperlink, TabStopType,
} = require("docx");

const root = path.join(__dirname, "..", "..");
const version = fs.readFileSync(path.join(root, "VERSION"), "utf8").trim();
const FONT = "Arial";
const MONO = "Courier New";
const ACCENT = "1F5FBF";
const GREY = "5A5A5A";
const PAGE_W = 11906, PAGE_H = 16838, MARGIN = 1304;           // A4, 2.3 cm margins
const CONTENT_W = PAGE_W - 2 * MARGIN;                           // 9298 DXA

// ---------------------------------------------------------------- helpers
// Inline text: **bold**, `code`, plain.
function runs(text, base = {}) {
  const out = [];
  const re = /(\*\*[^*]+\*\*|`[^`]+`)/g;
  let last = 0, m;
  while ((m = re.exec(text))) {
    if (m.index > last) out.push(new TextRun({ text: text.slice(last, m.index), font: FONT, ...base }));
    const tok = m[0];
    if (tok.startsWith("**")) out.push(new TextRun({ text: tok.slice(2, -2), bold: true, font: FONT, ...base }));
    else out.push(new TextRun({ text: tok.slice(1, -1), font: MONO, size: (base.size || 21) - 1, ...base, }));
    last = m.index + tok.length;
  }
  if (last < text.length) out.push(new TextRun({ text: text.slice(last), font: FONT, ...base }));
  return out;
}
const p = (text, opts = {}) => new Paragraph({ children: runs(text), spacing: { after: 120, line: 288 }, ...opts });
const bullet = (text, level = 0) => new Paragraph({
  children: runs(text), numbering: { reference: "bullets", level }, spacing: { after: 60, line: 276 },
});
let numberedRef = 0;
function numbered(items) {                       // each call restarts at 1
  const ref = "num" + numberedRef++;
  numberings.push({ reference: ref, levels: [{ level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
    style: { paragraph: { indent: { left: 540, hanging: 360 } } } }] });
  return items.map((t) => new Paragraph({ children: runs(t), numbering: { reference: ref, level: 0 }, spacing: { after: 60, line: 276 } }));
}
const numberings = [{
  reference: "bullets",
  levels: [
    { level: 0, format: LevelFormat.BULLET, text: "•", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 540, hanging: 270 } } } },
    { level: 1, format: LevelFormat.BULLET, text: "–", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 1080, hanging: 270 } } } },
  ],
}];
let bmCount = 0;
const toc = [];
function h1(text) {
  const id = "ch" + ++bmCount;
  toc.push({ id, text, level: 1 });
  return new Paragraph({ heading: HeadingLevel.HEADING_1, pageBreakBefore: true, children: [new Bookmark({ id, children: [new TextRun({ text })] })] });
}
function h2(text) {
  const id = "sec" + ++bmCount;
  toc.push({ id, text, level: 2 });
  return new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new Bookmark({ id, children: [new TextRun({ text })] })] });
}
const h3 = (text) => new Paragraph({ heading: HeadingLevel.HEADING_3, children: [new TextRun({ text })] });

function note(label, text, color = ACCENT, fill = "EAF1FB") {
  const side = { style: BorderStyle.SINGLE, size: 24, color, space: 8 };
  return new Paragraph({
    children: [new TextRun({ text: label + "  ", bold: true, font: FONT, color }), ...runs(text)],
    shading: { type: ShadingType.CLEAR, fill, color: "auto" },
    border: { left: side },
    indent: { left: 200, right: 120 },
    spacing: { before: 120, after: 160, line: 276 },
  });
}
const tip = (t) => note("Tip", t);
const important = (t) => note("Important", t, "B25A00", "FFF3E0");

function image(file, caption, widthIn = 6.0) {
  const buf = fs.readFileSync(path.join(root, "docs", "images", file));
  const w = buf.readUInt32BE(16), h = buf.readUInt32BE(20);          // PNG header
  const wpx = Math.round(widthIn * 96), hpx = Math.round((wpx * h) / w);
  return [
    new Paragraph({
      alignment: AlignmentType.CENTER, keepNext: true, spacing: { before: 120, after: 60 },
      children: [new ImageRun({
        type: "png", data: buf, transformation: { width: wpx, height: hpx },
        altText: { title: caption, description: caption, name: file },
      })],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER, spacing: { after: 200 },
      children: [new TextRun({ text: caption, italics: true, size: 18, color: GREY, font: FONT })],
    }),
  ];
}

const border = { style: BorderStyle.SINGLE, size: 4, color: "C8CDD4" };
const borders = { top: border, bottom: border, left: border, right: border };
function table(widths, header, rows) {
  const total = widths.reduce((a, b) => a + b, 0);
  const cell = (t, i, head) => new TableCell({
    width: { size: widths[i], type: WidthType.DXA }, borders,
    shading: head ? { type: ShadingType.CLEAR, fill: "E3EAF5", color: "auto" } : undefined,
    margins: { top: 70, bottom: 70, left: 110, right: 110 },
    children: [new Paragraph({ children: runs(t, { size: 19, bold: head ? true : undefined }), spacing: { after: 0 } })],
  });
  return new Table({
    width: { size: total, type: WidthType.DXA }, columnWidths: widths,
    rows: [
      new TableRow({ tableHeader: true, children: header.map((t, i) => cell(t, i, true)) }),
      ...rows.map((r) => new TableRow({ cantSplit: true, children: r.map((t, i) => cell(t, i, false)) })),
    ],
  });
}
const gap = () => new Paragraph({ children: [], spacing: { after: 120 } });

// ---------------------------------------------------------------- content
const cover = [
  new Paragraph({ spacing: { before: 2600, after: 0 }, children: [] }),
  new Paragraph({ children: [new TextRun({ text: "X52 Pro", font: FONT, size: 96, bold: true, color: ACCENT })] }),
  new Paragraph({ spacing: { after: 240 }, children: [new TextRun({ text: "for X-Plane 12 on Apple Silicon", font: FONT, size: 40, color: "2B2B2B" })] }),
  new Paragraph({
    spacing: { after: 600 }, border: { bottom: { style: BorderStyle.SINGLE, size: 12, color: ACCENT, space: 8 } },
    children: [new TextRun({ text: "Your X52 Pro, fully programmable, on a Mac.", font: FONT, size: 28, italics: true, color: GREY })],
  }),
  new Paragraph({ children: [new TextRun({ text: "User manual", font: FONT, size: 44, bold: true })] }),
  new Paragraph({ spacing: { after: 120 }, children: [new TextRun({ text: `Version ${version}`, font: FONT, size: 28, color: GREY })] }),
  new Paragraph({ spacing: { after: 1800 }, children: [new TextRun({ text: "18 November 2026", font: FONT, size: 24, color: GREY })] }),
  new Paragraph({
    spacing: { after: 60 },
    children: [new TextRun({ text: "What the app does, its windows, and the plugin inside X-Plane.", font: FONT, size: 22, color: GREY })],
  }),
  new Paragraph({
    children: [new TextRun({ text: "MIT licence. Not affiliated with Laminar Research, Logitech or Saitek.", font: FONT, size: 18, color: GREY })],
  }),
];

const body = [];
const add = (...xs) => xs.flat().forEach((x) => body.push(x));

// ---- 1
add(h1("1  What X52 Pro does"));
add(p("The Saitek / Logitech X52 and X52 Pro are some of the most popular flight sticks ever made. What made them special was the software that came with them: it gave every button more than one job, ran macros, wrote on the little screen and lit the buttons. That software only ever ran on Windows."));
add(p("**X52 Pro** is a new program for Apple Silicon Macs (M1 and later) that does the same job, and some new things besides. It is not a conversion of Logitech's software; it is written from scratch for the Mac and X-Plane 12. It comes in two parts that share the same profile files:"));
add(bullet("**The app**, a small program in the menu bar. It sets the lights, the screen and the clocks, keeps them applied in the background, and lets you edit everything else without starting the simulator."));
add(bullet("**The plugin**, which runs inside X-Plane 12. It reads the stick while you fly, runs your buttons, drives the screen and the lights from what the aircraft is doing, and has its own window for programming the buttons."));
add(h2("1.1  What you can do with it"));
add(table([2300, 6998], ["Area", "What it gives you"], [
  ["Buttons", "Three modes (the mode dial) and a pinkie shift give every button up to six jobs. A button can run an X-Plane command, set, toggle or add to a dataref, or type a key. Several steps with waits make a macro. A short press and a long press can do different things."],
  ["Lights", "A colour for every light in each mode (and each mode with the pinkie held). Live lights follow the simulator, for example Button E red while the parking brake is on. Alarms blink or alternate colours for stall, gear unsafe, fire and more. A master switch can turn all lights, or the screen, off while the avionics are off."],
  ["Screen", "Radio and autopilot pages driven by the two wheels (COM, NAV, ADF, DME, transponder, autopilot, OBS / heading), in a classic or a side-by-side layout. Your own pages with text, live dataref values and links. Three clocks, a stopwatch, and brightness."],
  ["Axes", "Dead zone, expo curve and inversion for the stick, twist and throttle; the slider as a flap lever; the dials as speed brake or zoom; a live view of every control the stick reports."],
  ["Manual", "A printable map of any profile as a Word file: the joystick and throttle pictures, every button in every mode, the lights, alarms and screen pages. Use the built-in layout or your own Word template."],
  ["Profiles", "Ready-made profiles for the aircraft that come with X-Plane 12, which are never changed. Your own copies, one per aircraft or one for all. Plain text files you can read and share."],
]));
add(gap());
add(h2("1.2  What you need"));
add(bullet("An Apple Silicon Mac (M1 or later) with macOS 13 or newer."));
add(bullet("X-Plane 12."));
add(bullet("A Saitek / Logitech X52 or X52 Pro. The standard X52 works, but its buttons have no colours, so the colour features need the X52 Pro."));
add(p("The program is not signed with a paid Apple developer account, so macOS asks you to confirm the first time you open it (see chapter 2)."));
add(h2("1.3  How this manual is organised"));
add(bullet("Chapter 2 installs the program and starts it for the first time."));
add(bullet("Chapter 3 explains the ideas behind everything else: modes, layers, priorities, and profiles."));
add(bullet("Chapter 4 walks through the app and each of its windows."));
add(bullet("Chapter 5 describes the plugin and its window inside X-Plane."));
add(bullet("Chapter 6 is about the stick's screen: the pages, the wheels and your own pages."));
add(bullet("Chapter 7 covers testing, troubleshooting and the profile file, and an appendix lists the button numbers."));

// ---- 2
add(h1("2  Install and first start"));
add(h2("2.1  From the download"));
add(...numbered([
  `Unzip \`X52-Pro-${version}.zip\` and drag **X52 Pro** into **Applications**.`,
  "Open it. The first time, right-click it and choose **Open**, because the app is not signed with a paid Apple developer account.",
  "In the window that opens, press **Install plugin**. The app finds X-Plane 12 by itself, Steam or not. If it cannot, press **X-Plane folder...** at the bottom right and pick the X-Plane 12 folder.",
  "Restart X-Plane. The plugin is loaded when X-Plane starts.",
]));
add(h2("2.2  From the source"));
add(p("Double-click `Install.command`. It builds the app (it asks for Apple's free command line tools if you do not have them), copies it to Applications and opens it. Then follow steps 3 and 4 above. From a terminal, `make install-app` does the same. To put the plugin into X-Plane without the app, use `make install`, or `make install XPLANE=\"/path/to/X-Plane 12\"` if the folder is not found."));
add(h2("2.3  How the app finds X-Plane"));
add(p("The app reads the install list X-Plane keeps in `~/Library/Preferences/x-plane_install_12.txt`, then Steam's library list, then the usual places (Applications, your home folder, Desktop, other disks). A folder you choose by hand is remembered. At the bottom of the app's window you can see which X-Plane is in use and whether the plugin is installed, out of date or missing, with a button to install or update it."));
add(h2("2.4  Keystrokes need one permission"));
add(p("If a button should type a keyboard key, macOS needs to be told that X-Plane may do that. Open **System Settings > Privacy & Security > Accessibility** and allow **X-Plane**. The plugin's Settings tab shows whether this is granted. Buttons that run X-Plane commands or write datarefs do not need it."));
add(h2("2.5  The menu bar icon and the window"));
add(p("Once running, the app lives in the menu bar as a game controller icon. Its menu shows the version and the state of the stick, and has **About X52 Pro**, **Check for updates...**, **Check for updates automatically**, **Open settings...**, **Open pages editor...**, **Calibrate stick...**, **Start at login** and **Quit X52 Pro**. While a window is open the app also appears in the Dock; pressing Command-Q then only puts it back in the menu bar so that the stick stays driven. Use **Quit Completely** (in the app menu) or **Quit X52 Pro** in the menu bar icon's menu to stop it."));
add(p("The stick forgets its lights when it loses power, so the app sends them again whenever the stick is plugged in. Lights, screen and clock settings hold in any program; the button programming stays inside X-Plane."));
add(h2("2.6  The app and the plugin working together"));
add(p("They share the same profile files. While X-Plane is running, the plugin drives the stick and the app stays quiet: it still saves what you change, and the plugin applies it within a second. When X-Plane quits, the app takes over again. Without X-Plane installed, profiles live in `~/Library/Application Support/X52Pro/`; with it, in `X-Plane 12/Output/preferences/X52Pro/`."));

// ---- 3
add(h1("3  How it works: the ideas"));
add(h2("3.1  Modes and the pinkie"));
add(p("The X52's mode dial has three positions, and the pinkie switch can be held to shift. Every button can have a different job in each of these layers:"));
add(table([2600, 6698], ["Layer", "When it applies"], [
  ["Any mode", "The job applies in every mode, unless the current mode has its own job for that button."],
  ["Mode 1, 2, 3", "The mode dial's position."],
  ["Mode 1, 2, 3 + Pinkie", "The same, while the pinkie switch is held. Up to six jobs per button in total."],
]));
add(gap());
add(p("The ready-made profiles use the modes like this: mode 1 for takeoff (battery, avionics, lights, gear), mode 2 for cruise (the autopilot; all the weapons in the fighters), mode 3 for landing (flaps and approach jobs, speedbrake, autobrake, reverse thrust on D while held, gear). The autopilot is one on/off button (B) in every mode, holding A sets the parking brake (a tap is the gear, or the landing light in cruise), and in the F-4 the trigger is the guns and the Fire button drops bombs; in the F-14 the trigger fires the selected weapon, the throttle hat in mode 2 picks it (up Sidewinder, right Sparrow, down Gun, left Phoenix) and Fire is free. In the F-14 hat 2 is the weapon select (up, right, down, left = Sidewinder, Sparrow, Gun, Phoenix), so the trims are on it only with the pinkie held in combat (pitch up and down, rudder left and right); the pinkie is the wheel brakes in the other modes. In them the pinkie is an ordinary button (wheel brakes while held); the **Pinkie switch works as Shift** setting in the plugin turns it into a real shift."));
add(h2("3.2  What a button can do"));
add(p("Each button, in each layer, has two lists of steps: **When pressed**, and optionally **When held longer than 0.4 s**. A step is one of:"));
add(bullet("**Command**: any X-Plane command, for example `sim/autopilot/servos_on`. Commands from add-on aircraft can be typed in full."));
add(bullet("**Keystroke**: a key or shortcut such as `shift+f1`."));
add(bullet("**Dataref**: write a value. **Set** puts a number there, **Toggle** flips between two values (0 and 1), **Add** adds an amount, and a negative number subtracts."));
add(bullet("**Wait**: a pause between steps."));
add(p("One command or key on its own follows the button: it is held for as long as you hold the button. Two or more steps run once, as a macro. Every button also shows a name on the stick's screen when you press it (up to 16 characters), which you can set per button."));
add(h2("3.3  What wins when several things want the same light"));
add(p("Each light shows the first of these that applies:"));
add(...numbered([
  "**Master switch**: when it says the lights are off, they are dark.",
  "**Alarms**: the first alarm in the list that is triggered.",
  "**Live lights**: the rule for that light, if it has one.",
  "**Mode colours**: the plain colour chosen for the current mode (and pinkie).",
]));
add(h2("3.4  The stick and X-Plane both see the buttons"));
add(important("A button you program in X52 Pro must be set to **Do nothing** in X-Plane's own **Settings > Joystick**, otherwise X-Plane and the plugin both act on it. Buttons you leave unprogrammed keep working through X-Plane as before."));
add(p("The app helps: on the Buttons tab, **Set to Do nothing in X-Plane** lists the buttons you program and shows what X-Plane has on each one (green when it is already Do nothing, orange when it is still set). This is read-only: X52 Pro never changes X-Plane's joystick settings."));
add(p("The flight axes (roll, pitch, twist, throttle) stay with X-Plane unless you turn on the axis processing in the plugin (section 4.9). The main POV hat and the throttle's mouse nub are buttons of X52 Pro (section 3.6): set both to **None** in X-Plane's Joystick settings."));
add(h2("3.5  Profiles"));
add(p("A profile is one text file with everything: buttons, lights, live lights, alarms, screen pages, clocks and axis settings. There is a **Default** profile for all aircraft and there can be one profile per aircraft; X-Plane uses the aircraft's own profile when it has one, and Default otherwise."));
add(h3("Ready-made profiles are never changed"));
add(p("The profiles that come with X52 Pro (Default and one for each X-Plane aircraft) are marked ready-made. Neither the app nor the plugin ever writes to them, so you can always go back to the original."));
add(bullet("Change anything on a ready-made profile in the app and it asks for a name first. Your change, and any after it, go into a new profile of yours, which the app switches to. Cancel drops the change."));
add(bullet("Your copy remembers where it came from, and X-Plane uses it for that aircraft from then on. Picking the ready-made profile again goes back to it."));
add(bullet("In X-Plane, a change made on a ready-made profile works for that flight but is not saved. **Save as my copy** on the window's bottom line keeps it. **Delete my copy** sets your copy aside and the ready-made profile is used again."));
add(bullet("Profiles you make yourself (an import, a copy, a blank one) save as you change them."));
add(h3("New, copy and delete"));
add(p("The menu next to the app's profile picker (the round button with three dots) has **New blank profile...** (no buttons, live lights or alarms, the built-in pages; the axis settings are kept so the aircraft still flies), **Save as new profile...**, and **Delete this profile...** for a profile of yours. A deleted profile is set aside as `<name>.x52.deleted`; **Undo delete** next to the picker brings it back."));
add(h3("Import what X-Plane has"));
add(p("On the Buttons tab, **Import what X-Plane has...** asks which of X-Plane's joystick profiles to import (the one for the aircraft last loaded is first) and suggests a name. It creates a new profile whose buttons carry X-Plane's commands as Any mode assignments, and switches to it. An existing profile is never overwritten."));

// ---- 4
add(h2("3.6  POV 1 (the main hat) and the mouse nub"));
add(p("The stick reports its main hat (POV 1) and the small stick on the throttle (the mouse nub) as moving controls, not as buttons. X52 Pro turns each of them into buttons, so that every profile can bind them like any other button (Appendix A):"));
add(bullet("**POV 1** gives buttons 40 to 43: up, right, down, left. A diagonal holds two of them at once."));
add(bullet("**The mouse nub** gives buttons 44 to 47 when it is pushed part way (up, right, down, left) and buttons 48 to 51 when it is pushed all the way. Only one of the two sets is held in a direction at a time, so the far button takes over from the near one."));
add(p("Every profile has them set, the ready-made ones and your own: POV 1 **looks around** (`sim/general/rot_up`, `rot_right`, `rot_down`, `rot_left`: what the Q, E, R and F keys do in X-Plane), and the nub **pans the view** (`sim/general/up`, `right`, `down`, `left`: what the arrow keys do), and pushed all the way it pans faster (`up_fast` and so on: what Shift and an arrow key do). They are ordinary Command bindings, so you can change or clear them on the Buttons tab like any other. A profile made before these existed gets them when it is read, except for any of these buttons you had already programmed."));
add(important("Set POV 1 and the mouse nub to **None** in X-Plane's **Settings > Joystick**, otherwise X-Plane and the plugin both act on them. Holding the clutch (i) hands POV 1 to the profile picker (section 5.3): the look-around bindings pause until you let go. The nub is not affected by the clutch."));
add(p("X52 Pro 1.2.6 sent the keys Q, E, R and F for POV 1. Keystrokes need macOS permission and queue up behind a busy simulator, so from 1.2.7 both use X-Plane's own commands. A binding still exactly as 1.2.6 made it is switched over when the profile is read."));
add(h1("4  The app and its windows"));
add(p("Open the window from the menu bar icon (**Open settings...**, Command-comma). It has a bar at the top, a row of tabs, and a status area at the bottom. The pages editor opens in a window of its own."));
add(h2("4.1  The top bar"));
add(bullet("**Plugin on/off**: the button at the left of the status line switches the X-Plane plugin on (green light, **Plugin on**) or off (red light, **Plugin off**). Off, the plugin does nothing in X-Plane: no buttons, axes, dials, screen or lights, and X-Plane's own Joystick settings are in charge; the status line then says \"Stick connected, plugin off\". It takes effect within a second and stays off until you switch it back on."));
add(bullet("**Status line**: whether the stick is connected, the mode it is in (\"Mode 2\", or \"Pinkie + 2\" while the pinkie is held), and whether the plugin is driving it. The dot is green for the app, blue when the plugin is active, grey when no stick is found."));
add(bullet("**Profile**: the profile being edited. Click it to open a list with two groups, **Your profiles** and **Ready-made profiles**; the **+** / **-** in front of a group unfolds or folds it, and the list grows to fit but never past the window (it scrolls). Below it, one line is kept free for messages (an error, or the notice that a ready-made profile is selected), so the window does not jump."));
add(bullet("**The menu (three dots)**: New blank profile, Save as new profile, **Export Current Template**, Save a Built-In Template Blank Copy, Choose Custom Template, Use the Built-In Template, Delete this profile (section 4.13)."));
add(bullet("**Undo delete**: appears after you delete a profile."));
add(bullet("**The blue bar**: shown while a ready-made profile is selected, with **Save as new profile...**."));
add(p("Tabs can be opened with Command-1 to Command-9, and the left and right arrow keys move along them. Tab reaches every control in the window, and the arrow keys choose in lists."));
add(h2("4.2  Buttons"));
add(p("Does what the X-Plane window's button editor does, without starting the simulator. Pick the layer at the top (**Programming for**: Any mode, a mode, or a mode and pinkie), click a button in the list, and add steps under **When pressed** and **When held**. While you type a command or a dataref the app suggests matches from X-Plane's own lists (`Resources/plugins/Commands.txt` and `DataRefs.txt`) and from the add-on aircraft's own scripts, so no simulator is needed. Changes are saved at once and a running X-Plane applies them within a second."));
add(...image("app-buttons.png", "Figure 1. The Buttons tab: button B in mode 1 runs the command that turns the autopilot servos on."));
add(bullet("**+ Command**, **+ Keystroke**, **+ Dataref**, **+ Wait** add a step. The arrows move a step up or down; the cross removes it."));
add(bullet("**Name on screen** is what the stick's screen shows when the button is pressed."));
add(bullet("**Import what X-Plane has...** starts a new profile from X-Plane's own joystick settings (section 3.5)."));
add(bullet("**Set to Do nothing in X-Plane** lists the buttons you program, so you can set them in X-Plane (section 3.4)."));
add(h2("4.3  Lights"));
add(p("A colour for every light in each of the three modes: Fire button, A, B, D, E, the three toggle pairs, POV 2, Clutch (i) and Throttle. The Fire button and the Throttle light are a single colour (off or lit). Choose **Pinkie released** or **Pinkie held** to edit the colours used while the pinkie switch is held."));
add(...image("app-lights.png", "Figure 2. The Lights tab: one colour per light and mode, with the mode now on the stick marked \"(now)\"."));
add(bullet("The column for the mode on the stick now is marked **(now)**, and so is the status line."));
add(bullet("**Copy to all modes from** copies one mode's colours to the other two."));
add(bullet("**Light brightness** sets how bright the lights are."));
add(p("Changing the lights of another mode does not change what the stick shows now; it shows them when you turn the dial to that mode. A profile from before pinkie colours existed keeps its colours for the pinkie too."));
add(h2("4.4  Live lights"));
add(p("A live light follows the simulator instead of the mode dial. Each light can have one rule. A rule overrides the plain colours of the Lights tab, and alarms override rules."));
add(...image("app-live-lights.png", "Figure 3. The Live lights tab: three rules, one per light (parking brake, gear and flaps)."));
add(bullet("**Add rule** adds a line. Click a line (its arrow) to open or close it. Tick boxes and press **Remove selected** to delete several at once; **Select all** ticks every line."));
add(bullet("A rule is: the light, a dataref, a comparison, a number, the colour if the test is true and the colour if not."));
add(bullet("A second test can be joined with **AND** or **OR**, and either test can be turned around with **NOT**."));
add(bullet("**Start from** fills in a ready-made rule (parking brake, gear, flaps, speedbrake, autopilot, landing lights, stall, master warning) or type any dataref. `name[3]` reads one item of a list."));
add(bullet("A colour can also be **mode colour**, which leaves that state to the mode dial's colour."));
add(p("Inside X-Plane the plugin reads the datarefs directly, ten times a second. With X-Plane running but the plugin not, the app asks X-Plane over its network interface (UDP port 49000, on this Mac only); that needs **Settings > Network > Accept incoming connections** in X-Plane. If a dataref cannot be read, that rule says nothing and the light keeps its mode colour. Fire and Throttle can only be off or lit."));
add(h2("4.5  Alarms"));
add(p("Alarms have the highest priority among the lights. The list is in priority order: the first alarm that is triggered decides its light, and an alarm that is not triggered leaves the light to the next one. With Stall above Gear unsafe, a gear problem still shows while the plane is not stalling."));
add(...image("app-alarms.png", "Figure 4. The Alarms tab: eight alarms in priority order, each with its light, effect and trigger."));
add(bullet("An alarm has a name, a light, an effect (**steady**, **blink**, or **alternate** between two colours), a speed for the last two (slow: one cycle a second, fast: two a second) and a trigger: one test on a dataref, or two joined with AND / OR, each with an optional NOT."));
add(bullet("**Add from template** adds stall, gear unsafe, low fuel, overspeed (Vne) and autopilot disconnect; change the light, colours or speed afterwards."));
add(bullet("**Move up** and **Move down** (in an opened alarm) change the priority."));
add(bullet("**Try on the stick** shows an alarm for a few seconds whatever the simulator says, so you can see the blinking. The Test tab has the same buttons."));
add(p("Alarms are edited in the app, not in the X-Plane window; the plugin applies them inside X-Plane. Brightness is not affected by alarms."));
add(h2("4.6  Screen & clock"));
add(...image("app-screen.png", "Figure 5. The Screen & clock tab."));
add(bullet("**Screen brightness**: how bright the stick's screen is (never below 10 %)."));
add(bullet("**Clock 1** shows GMT, the simulator's local time, the Mac's local time or a fixed time zone. **Clock 2** and **Clock 3** are set from GMT with a time zone offset. Each can be 12-hour or 24-hour. **Date order** is day-month-year, month-day-year or year-month-day."));
add(bullet("On the stick, the **Start/Stop** and **Reset** buttons under the screen switch between the three clocks, and **Function** switches to the stopwatch. While X-Plane is not running, Clock 1 shows the Mac's own time instead of the simulator's."));
add(bullet("**Mode names**: when the mode dial moves, the stick's screen says \"MODE 3\" and the mode's name (up to 16 characters; the ready-made profiles say Takeoff, Cruise, Landing) for the time under **Show the name for** (1.5 s by default, 0 turns it off), then goes back to the page you were on. Names and time belong to the profile."));
add(bullet("**Latched clutch**: one press of the clutch (i) turns safe mode on, the next press turns it off. Safe mode applies in X-Plane: buttons only show their name on the screen, and the clutch and POV lights flash while it is on."));
add(bullet("**Fast turns of the right wheel jump**: while changing heading, altitude, vertical speed, speed, OBS or a fine COM/NAV value, spinning the wheel quickly repeats each click (by default up to x10: heading moves 10 degrees, altitude 1000 ft). A slow turn still steps by one. **Fast jump** sets the factor."));
add(h2("4.7  Pages"));
add(p("Lists the left wheel's pages in order: the built-in pages (with a lock) and your own. **Open pages editor...** opens the editor window (section 4.12). **Restore default pages...** asks first, then removes all your pages and brings back the usual order."));
add(...image("app-pages.png", "Figure 6. The Pages tab."));
add(p("**Radio screens** chooses the layout for the radio pages in this profile:"));
add(bullet("**Classic**: one radio per screen (COM1 / NAV1, COM2 / NAV2, ADF)."));
add(bullet("**Side by side**: COM 1+2, NAV 1+2 and ADF 1+2, each pair on one screen."));
add(p("**Order for** chooses whether the order below applies to every mode or to one mode only, so that, for example, mode 2 can start with the autopilot page. Turning the mode dial to a mode whose order differs opens the first page of that mode's order. The eye shows or skips a page; a skipped page stays in the list (crossed out) and the left wheel passes it by."));
add(h2("4.8  Master & brightness"));
add(p("The **master switch** keeps the screen, and separately every light, on only while a dataref has the value you choose, for example while the avionics are powered (**Use avionics power** fills that in; **Use battery** and **Always on** are there too). When the test is false the screen goes blank or the lights go dark. With no dataref chosen, or one X-Plane does not know, it stays on."));
add(...image("app-master.png", "Figure 7. The Master & brightness tab: the screen and the lights follow avionics power."));
add(bullet("**Try it off on the stick** switches it off for five seconds without X-Plane, so you can see it working."));
add(bullet("It is a normal condition, so **AND**, **OR** and **NOT** work as for alarms."));
add(bullet("**Brightness** can follow a dataref instead of the fixed sliders, for example the panel light dial: the value runs between two points and the brightness between dimmest and brightest."));
add(p("Test mode shows the screen and the lights whatever the master switch says."));
add(h2("4.9  Axes"));
add(p("Shows every control the stick reports with its value, and names the one that is moving, the rotaries and slider included. X-Plane stays in charge of the flight axes by default."));
add(...image("app-axes.png", "Figure 8. The Axes tab."));
add(bullet("**The plugin drives the stick and twist** and **The plugin drives the throttle**: the plugin reads roll, pitch, twist and throttle itself, applies a **dead zone**, an **expo** curve and optional **invert**, and gives the result to X-Plane. It does this by setting X-Plane's `override_joystick` and `override_throttles`, and releases them when you untick the box. The throttle is only taken over once it has moved. While X-Plane's own autothrottle is on, the plugin leaves the throttle to it; once the autothrottle is off, the lever takes over again when it reaches the power the autothrottle left (the X-Plane window's Axes tab says where), so the power never jumps. This works inside X-Plane only; the app itself never moves the controls."));
add(bullet("**Stick handling**: with the plugin driving the stick, X-Plane's own response curve and stability augmentation are skipped, so the plugin takes their place. The stick starts with soft curves (expo 50 for pitch, 40 for roll, 30 for the twist). **Smoothing** (60 ms) calms small jitter and hand tremor; big or quick moves, such as letting go of the stick, reach X-Plane at once. **Damping** (30 %) gently resists the aircraft pitching and rolling while the stick is near the centre; it is capped at a few percent of the travel, so you can always overpower it, and it is off on the ground and while the autopilot flies. Set either to 0 to switch it off."));
add(bullet("**Calibration**: **Calibrate stick...** opens a window that guides you step by step: push the stick to its four edges until each dot turns green, turn the twist both ways (or skip it), let go so the resting point becomes the centre, then check the measured values and press **Save**. The plugin then uses the stick's own centre and ends; the saved values show on the Axes tab (min .. centre .. max). In the X-Plane window it is simpler: **Calibrate stick**, move everything to its limits, let go, **Done**. **Reset** goes back to the stick's full range. The calibration belongs to the stick, not to a profile, and is the same in the app and in the X-Plane window: a calibration made in one is picked up by the other within a second. Do it on the ground or paused, since the stick still flies the aircraft meanwhile."));
add(bullet("**Dials and slider** (lower and upper rotary, slider): a change smaller than **Steady** is ignored, so the value does not flicker."));
add(bullet("**What the dials do**: for the lower dial (i, by the clutch) and the E dial pick **Speed brake**, **Zoom** or **Nothing**, and **Invert** to turn it the other way. Speed brake goes in four steps over the dial's turn: Armed, Retracted, Half, Full. Zoom uses X-Plane's own zoom; **Zoom strength** sets how many zoom steps a full turn gives (96 by default, 12 to 480), and you can change it while flying. An aircraft that does not take the armed position (the Citation X) stays retracted in the first step. The plugin acts only while you turn the dial, so speed-brake buttons, the mouse wheel and the auto-spoilers still work. Set the dials to None in X-Plane's Joystick settings; if X-Plane still uses one for speed brakes or zoom, the plugin leaves that job to X-Plane and its window says so."));
add(bullet("**Smooth levers**: a dial can also be **Prop**, **Mixture**, **Rudder trim**, **Aileron trim**, **Cowl flaps** or **Carb heat**. A full turn covers the game's range for that job, within the aircraft's own trim limits; **From** and **To** (percent) narrow it for finer control. Prop, mixture, cowl flaps and carb heat move every engine by the same amount, so levers set apart stay apart. After loading an aircraft or profile, or when a button, the mouse or the autopilot moved the value (even by a single small trim step), the dial does nothing until you turn it to X-Plane's own value, so nothing ever jumps; the plugin window says where to turn it."));
add(bullet("**Slider sets the flaps**: in X-Plane the slider moves the flap lever to the aircraft's own stops. Pushed away from you deploys the flaps and towards you retracts them; **Invert flaps** reverses that. This works whether the axes above are on or off, and the flap buttons still work in between. Set the slider to None in X-Plane's Joystick settings if X-Plane uses it for something else."));
add(h2("4.10  Test"));
add(...image("app-test.png", "Figure 9. The Test tab."));
add(bullet("**On the stick's screen** mirrors what the three lines show. **Held now** and **Last moved** name the buttons and controls you are using."));
add(bullet("**Write on the stick's screen**: three lines of your own (16 characters each). **Show on stick** displays them; **Back to normal** returns to the usual screen."));
add(bullet("**Alarms**: a **Try** button for each alarm of the profile shows it on the stick."));
add(h2("4.11  The menu bar icon"));
add(p("The menu shows the version, the state of the stick, **About X52 Pro** (version and licence), **Check for updates...**, **Check for updates automatically**, **Open settings...**, **Open pages editor...**, **Calibrate stick...**, **Start at login** (tick it to have the app start when you log in, so the lights are set before you start the simulator) and **Quit X52 Pro**."));
add(p("**Updates.** The app looks at the project's latest release on GitHub when it starts (at most once a day, and it says nothing unless there is something new) and whenever you choose **Check for updates...**. When a newer version exists the menu item reads **Update available** and opens the release page, where you download it; nothing is downloaded or installed by itself, and nothing about you or your Mac is sent. Only a change of the first two numbers of the version counts (1.2.3 to 1.3.0 does; 1.2.3 to 1.2.40 does not), so small fixes never ask you to download again. Untick **Check for updates automatically** to check only by hand."));
add(h2("4.12  The pages editor"));
add(p("A window of its own for the selected profile (chosen at its top). Everything is saved at once into the profile file, with the button assignments and other settings in it kept, and a running X-Plane picks it up within a second."));
add(...image("app-pages-editor.png", "Figure 10. The pages editor: the Flight page with seven value rows, and a preview of the stick's screen.", 6.2));
add(bullet("**Left: the left wheel's pages in order.** The built-in pages carry a lock (COM1 / NAV1, COM2 / NAV2, ADF, DME, Transponder, Autopilot, OBS / Heading, Buttons, Brightness); yours have a page icon and the number of their rows, with the pages inside each of yours indented under it. The eye shows or skips a page, and the arrows move the selected page in the wheel's order. **Add page** adds a page the left wheel reaches. **Add inside** adds a page that only a link on the selected page opens. **Remove** removes a page, the pages inside it, and the links that opened them. A built-in page cannot be removed or changed, only skipped or moved."));
add(bullet("**Middle: the selected page.** Its **Title**, **Opens from** (the left wheel, or inside another page) and its rows. Each row is **Text**, **Value** or **Link**. A value row takes a dataref (suggestions come from X-Plane's `DataRefs.txt` as you type), a label and a format, with an example of what the format makes of 1234.5. A link row picks the page it opens, or makes a new page inside this one."));
add(bullet("**Right: the preview.** The page on a black three-line screen, with X-Plane's live numbers when it answers over UDP and made-up ones otherwise, and the page lines as they are saved in the profile file (**Show file**)."));
add(bullet("**Restore default pages...** asks first, then removes all your pages and brings back the usual order with every built-in page shown."));

add(h2("4.13  Export the manual of a profile"));
add(p("The menu next to the profile picker (the round button with three dots) makes a printable map of the profile in use, as a Word file, with the same layout as the maps of the ready-made profiles."));
add(...numbered([
  "Open the menu and choose **Export Current Template...**.",
  "Pick where to save the file. It is named after the profile.",
  "Open the `.docx` in Word. For a PDF, use **File > Save as PDF** in Word; the app makes only the Word file.",
]));
add(p("The first page shows the joystick and throttle pictures with the assignment tables: every control in each of the three modes, then the same controls with the pinkie held. The next page is the display and lights dashboard: the page order of the stick's screen for each mode, the fields of your own pages, and the colour of every light in each mode, shown as small icons. A page of its own lists the live lights and the alarms, which can grow; the alarms are a list that gets longer when the profile has more of them. A control with nothing assigned shows a dash."));
add(p("The icons: a full disc is a steady light, a disc with rays flashes, a disc in two halves alternates the two colours (the main colour on the left), a disc in three parts is the colour the mode gives that light, and a hollow disc is off."));
add(h3("Your own template"));
add(p("The layout comes from a Word file, a template, with a spot for every value. You can use your own:"));
add(bullet("**Save a Built-In Template Blank Copy...** saves the built-in template to a place of your choice, as a starting point."));
add(bullet("Open the copy in Word and change what you like: fonts, colours, wording, pictures, the order of the pages. The spots the app fills are Word content controls with a tag (Developer tab, Properties); format their text the way you want the values to look."));
add(bullet("**Choose Custom Template...** makes the app use your file (it remembers it); **Use the Built-In Template** goes back."));
add(bullet("A spot with a tag the app does not know comes out empty and the app lists it after the export, so a typo is easy to find."));
add(p("The guide “Your own manual template” (`docs/custom-manual-template.md` in the project) explains it step by step, and `docs/manual-data.md` lists every tag. The built-in template also sits in the project as `templates/X52-Manual-Template.docx`."));
add(note("Tip", "The aircraft picture in the page header is a grey placeholder. In Word, double-click the header, right-click the picture and choose Change Picture; the app keeps your picture.", "1E6091", "E8F1F8"));

// ---- 5
add(h1("5  The plugin inside X-Plane"));
add(p("The plugin is the part that runs while you fly. It appears in X-Plane's **Plugins** menu as **X52 Pro** (with its version), and in the Plugin Admin."));
add(h2("5.1  The Plugins > X52 Pro menu"));
add(table([3400, 5898], ["Item", "What it does"], [
  ["Program buttons...", "Opens the programming window."],
  ["Reload profile from disk", "Reads the profile file again. Use it after editing the file by hand. A profile saved while X-Plane runs is also picked up within a second."],
  ["Test mode on/off", "While on, the stick's screen names whatever you move or press, and the plugin's button assignments are paused."],
]));
add(gap());
add(p("These actions also exist as X-Plane commands (under `x52pro/`), so you can give them a key or a button. There are commands for the screen's page wheel and cursor too (`mfd_page_next`, `mfd_page_prev`, up, down and select), which the ready-made profiles bind to the stick's wheels."));
add(h2("5.2  The programming window"));
add(p("It is a floating window inside X-Plane, drawn by the plugin itself. At the top are the tabs **Buttons**, **Lights**, **Live lights**, **Settings** and **Axes**, and **Change profile...**. The bottom line shows the profile in use and its file, with **Save as my copy** / **Delete my copy** for ready-made profiles. Changes are saved as you make them. The window uses the same profile files as the app, so what you set in one is seen in the other."));
add(p("Pressing a button on the stick selects it in the list (the mode dial and the pinkie cannot be programmed, so they select nothing). Buttons that have a light show its colour on the left, for the layer you are programming; it is only a picture, the colours are set on the Lights tab."));
add(p("Tab moves between text fields, and Escape cancels the command search."));
add(h3("Buttons"));
add(p("Press a button on the stick (or click it in the list), choose **Any mode**, a mode, or a mode and pinkie under **Programming for**, then add steps under **When pressed** and **When held**:"));
add(bullet("**+ Command**: search X-Plane's commands by word (\"gear\", \"flaps\", ...). Commands of add-on aircraft are found too (with the aircraft's name shown) when the aircraft uses xlua scripts or you have run X-Plane with the DataRefTool plugin; any other can be typed in full (`author/aircraft/command`). Several words narrow the list; the Down and Up arrows move through the suggestions and Return picks one."));
add(bullet("**+ Keystroke**: click **Record** and press the key, or type for example `shift+f1`."));
add(bullet("**+ Dataref**: write a value to a dataref. **Set** puts a number there, **Toggle** flips between two values, **Add** adds an amount (negative to subtract); the last two can keep the result between a minimum and a maximum. The dataref name turns red if X-Plane does not know it. Some datarefs are read-only and X-Plane refuses to change them; the window then says so."));
add(bullet("**+ Wait**: a pause between steps."));
add(p("The window also tells you what the stick is doing: which mode is on now, and the name of any button held. If the stick is a standard X52, it notes that button colours need the X52 Pro."));
add(h3("Lights and Live lights"));
add(p("**Lights** has the colour tables of section 4.3, with a *Pinkie held* choice. **Live lights** has the rules of section 4.4 under **Program buttons... > Live lights**. Alarms, the master switch and the pages are edited in the app, not here."));
add(h3("Settings"));
add(bullet("**Pinkie switch works as Shift** (adds Mode 1-3 + Pinkie)."));
add(bullet("**Clutch (i) is a safe mode**, and **Latched clutch**."));
add(bullet("**COM fine tuning steps 8.33 kHz channels** (off: 25 kHz)."));
add(bullet("**Side-by-side radio screens** instead of one radio per screen."));
add(bullet("**Fast turns of the right wheel jump**, with the jump size."));
add(bullet("**Test mode**: the stick's screen names whatever you move or press."));
add(bullet("**Typing keyboard keys**: shows \"allowed by macOS\" or \"not allowed by macOS yet\" (see section 2.4)."));
add(bullet("The screen as the stick shows it right now, what is held, and the profile file's name."));
add(h3("Axes"));
add(p("The same as the app's Axes tab (section 4.9): every control the stick reports, the two switches that let the plugin process the flight axes, the dead zone, expo, smoothing, damping and steady values, and the stick calibration."));
add(h2("5.3  Picking a profile"));
add(bullet("**In the window:** **Change profile...** lists every profile in two groups, **Your profiles** and **Ready-made profiles** (click **[+]** / **[-]** to unfold or fold a group; a long list scrolls), including ones you imported, and **Automatic** to go back to the aircraft's own profile or Default. A profile picked here stays until the aircraft changes; the window says \"(picked)\"."));
add(bullet("**On the stick:** hold the clutch, push the main POV hat (POV 1) up or down to scroll through the profiles, right to use the one shown, left to go back to automatic. While the clutch is held the hat's look-around bindings pause, so the view does not move. (If X-Plane itself also has an assignment on the hat, it may still move: set the hat to **None** in X-Plane.)"));
add(bullet("**In the app:** the **Profile** picker. The app's choice is used when X-Plane is not running."));
add(h2("5.4  The stick while you fly"));
add(bullet("**Mode dial.** A worn dial can flicker between positions; the mode only changes once the dial has rested for 0.1 s (`mode_settle_ms` in the profile file)."));
add(bullet("**Clutch (i)** is a safe mode: while held, buttons only show their name on the screen and the clutch and POV lights flash."));
add(bullet("**Any button** shows its name on the screen when pressed, programmed or not."));
add(bullet("**The screen** shows the profile and mode, the name of the button you pressed, or the page you are on (chapter 6)."));
add(bullet("**Lights** follow the colours, live lights and alarms of the profile, ten times a second."));
add(h2("5.5  What the plugin installs"));
add(p("The plugin is one file, `X52Pro.xpl`, in `X-Plane 12/Resources/plugins/X52Pro/mac_x64/`. The app installs it, or updates it when it is out of date. Profiles are plain text in `X-Plane 12/Output/preferences/X52Pro/`."));

// ---- 6
add(h1("6  The stick's screen"));
add(p("The three-line, 16-character screen on the throttle is the cockpit panel of the X52. Two wheels beside it work as follows."));
add(h2("6.1  The wheels"));
add(bullet("**The left wheel (page wheel)** flips between pages: COM1/NAV1, COM2/NAV2, ADF, DME, Transponder, Autopilot, OBS/Heading, then your own pages, then Buttons, then Brightness (the last page). In the side-by-side layout the radio pages are COM 1+2, NAV 1+2 and ADF 1+2."));
add(bullet("**The right wheel and its button** change what the page shows. Turn to move the cursor (the `>` mark) up and down, press to act. `[118.000]` is an active frequency: press to swap it with the standby. `>` or `<` marks a part of a value: press (it becomes `>>` or `<<`), turn to change it, press again to finish. Turning alone never changes anything."));
add(bullet("**Fast turns jump.** While changing heading, altitude, vertical speed, speed, OBS or a fine COM/NAV value, spinning the wheel quickly repeats each click (up to x10 by default); slow turns still step by one."));
add(bullet("COM fine steps are 25 kHz, or 8.33 kHz channels if you choose that."));
add(bullet("The **Function**, **Start/Stop** and **Reset** buttons under the screen are the stick's own clock and stopwatch buttons."));
add(h2("6.2  The Brightness page"));
add(p("The last page sets the screen's and the lights' brightness from the stick: turn the right wheel to pick Screen or Lights, press to change (`>>`), turn in 5 % steps, press again to finish. It is saved at once. The screen never goes below 10 %. A brightness that follows a dataref shows `dataref` and is not changed here."));
add(h2("6.3  The Buttons page"));
add(p("Shows the name of the profile on the first line, the name of the button you last pressed on the second, and the mode (with \"+ Pinkie\" while the pinkie is held) on the third. It is the default screen, and it is useful for learning a profile."));
add(h2("6.4  Your own pages"));
add(p("You can add pages to the left wheel's list. A page has rows, and a row is plain text, a dataref value, or a link that opens another page. Build them in the pages editor (section 4.12), or write them by hand in the profile file:"));
[
  'page fly "Flight"',
  'row fly value "Alt" "%.0f ft" sim/cockpit2/gauges/indicators/altitude_ft_pilot',
  'row fly value "IAS" "%.0f kt" sim/cockpit2/gauges/indicators/airspeed_kts_pilot',
  'row fly link "Fuel" fuel',
  'page fuel "Fuel" in fly',
  'row fuel value "Total" "%.0f kg" sim/flightmodel/weight/m_fuel_total',
  'row fuel value "Tank 1" "%.0f kg" sim/cockpit2/fuel/fuel_quantity[0]',
].forEach((line, i, a) => body.push(new Paragraph({
  children: [new TextRun({ text: line, font: MONO, size: 17 })],
  shading: { type: ShadingType.CLEAR, fill: "F2F4F7", color: "auto" },
  indent: { left: 200, right: 120 },
  spacing: { before: i === 0 ? 80 : 0, after: i === a.length - 1 ? 160 : 0 },
})));
add(bullet("A page without `in` is reached with the left wheel. A page with `in <page id>` is only opened by a link row. Page ids are letters, digits, `-` and `_`."));
add(bullet("Rows: `text \"words\"`, `value \"label\" \"format\" dataref` and `link \"label\" page-id`. The format has one `%d`, `%f`, `%e` or `%g`, with an optional width and decimals, for example `%.0f kg`. `name[3]` reads one item of a list."));
add(bullet("The left wheel's order and the pages it skips are two lines: `wheel_order = @fly com1 com2 xpdr ap vorhdg buttons brightness` and `wheel_hidden = adf dme`. Built-in pages are `com1 com2 adf dme xpdr ap vorhdg buttons brightness`; your own top-level pages are `@<id>`. A mode's own order is `wheel_order.1`, `.2` or `.3`."));
add(bullet("On the screen the cursor `>` is on one row (the right wheel moves it and the screen scrolls to keep it in view). Pressing the wheel on a link opens its page; inside a sub-page the first row is `< Back`. Leaving a page with the left wheel forgets where you were. A value that cannot be read shows `--`."));
add(bullet("To get the default screen back, use **Restore default pages...** or delete the `page`, `row`, `wheel_order` and `wheel_hidden` lines."));
add(p("The Function, Start/Stop and Reset buttons are not used for going back, because the stick itself uses them for its clocks and stopwatch."));

// ---- 7
add(h1("7  Testing, troubleshooting and files"));
add(h2("7.1  Test the stick"));
add(p("**Without X-Plane:** double-click `Test Stick.command`. It opens a small menu: name controls (move or press anything and the stick's screen says what it is), write a message on the screen, set the lights, clear the screen, list the controls. The same tool from a terminal is `./bin/x52probe` with `--identify`, `--text \"HELLO\" \"X52 PRO\" \"ON A MAC\"` or `--leds red`."));
add(p("**Inside the app:** the Test tab (section 4.10). **Inside X-Plane:** Plugins > X52 Pro > Test mode on/off, or the checkbox on the window's Settings tab."));
add(h2("7.2  If something does not work"));
add(table([3000, 6298], ["Problem", "What to try"], [
  ["The app says the plugin is not installed, or out of date.", "Press **Install plugin** or **Update plugin** at the bottom of the window, then restart X-Plane."],
  ["The app cannot find X-Plane.", "Press **X-Plane folder...** and choose the X-Plane 12 folder."],
  ["A button does two things.", "That button is not set to **Do nothing** in X-Plane's Settings > Joystick. Set it, or remove it from the profile."],
  ["A keystroke button does nothing.", "Allow X-Plane in System Settings > Privacy & Security > Accessibility. The plugin's Settings tab shows the state."],
  ["The lights are not what I set.", "Remember the order: master switch, alarms, live lights, mode colours. Check the Alarms and Live lights tabs. A ready-made profile is never changed: your change is in your copy, so check the profile picker."],
  ["A live light stays on its mode colour.", "The dataref could not be read (wrong name, or X-Plane not answering). Without the plugin, turn on Settings > Network > Accept incoming connections in X-Plane."],
  ["The stick's screen shows my text, not the page.", "The Test tab's lines are still shown. Press **Back to normal**, or turn Test mode off."],
  ["Axes behave oddly.", "If you ticked the plugin to drive the axes, also check X-Plane's own axis assignments, and the dead zone and expo on the Axes tab. Untick the boxes to give the axes back to X-Plane."],
  ["I changed a profile file and nothing happened.", "Use Plugins > X52 Pro > Reload profile from disk (a saved change is also picked up within a second)."],
]));
add(gap());
add(h2("7.3  The profile file"));
add(p("Profiles are plain text, so you can edit and share them. The app and the plugin write the same format. A few lines to recognise:"));
[
  "standard = 1                                  (a ready-made profile: never written)",
  "based_on = aircraft-Cessna_CitationX          (what a copy of it came from)",
  "rule e > 0.5 red green sim/cockpit2/controls/parking_brake_ratio",
  'alarm pov blink fast red amber "Stall" > 0 sim/cockpit2/annunciators/stall_warning',
  "master screen > 0.5 sim/cockpit2/switches/avionics_power_on",
  "bind ... set:sim/x=1   toggle:sim/x=0,1   add:sim/x=0.1,0,1",
].forEach((line, i, a) => add(new Paragraph({
  children: [new TextRun({ text: line, font: MONO, size: 17 })],
  shading: { type: ShadingType.CLEAR, fill: "F2F4F7", color: "auto" },
  indent: { left: 200, right: 120 },
  spacing: { before: i === 0 ? 80 : 0, after: i === a.length - 1 ? 160 : 0 },
})));
add(p("The profile in use for each aircraft is kept in `profile-choices.txt` next to the profiles. The plugin also learns X-Plane's own number for each button you press (saved in `xplane-map.txt`) and records the aircraft's description for the app (`xplane-aircraft.txt`)."));
add(h2("7.4  Build and test from the source"));
add(p("`make` builds everything into `bin/` and runs the tests. `make install-app` puts the app in Applications. `make release` makes `bin/X52-Pro-<version>.zip`, ready to share. The version number is in the file `VERSION`; change it there and everything (the app's About box and window titles, the plugin's name in X-Plane, and the zip) follows."));

// ---- appendix
add(h1("Appendix A  Button numbers"));
add(p("The numbers are the usual X52 button numbers (JOY1 to JOY39 in Windows lists). Numbers 40 to 51 are not buttons on the stick: X52 Pro makes them from POV 1 and the mouse nub (section 3.6)."));
add(table([2200, 7098], ["Numbers", "Buttons"], [
  ["1 to 8", "Trigger, Fire, A, B, C, Pinkie, D, E"],
  ["9 to 14", "Toggle 1 up / down (9, 10), toggle 2 (11, 12), toggle 3 (13, 14)"],
  ["15", "The trigger's second stage"],
  ["16", "The mouse button"],
  ["17 to 19", "The scroll wheel"],
  ["20 to 23", "The stick's hat"],
  ["24 to 27", "The throttle's hat"],
  ["28 to 30", "The mode dial"],
  ["31", "The clutch (i)"],
  ["40 to 43", "POV 1, the stick's main hat: up, right, down, left (a diagonal holds two). Look around."],
  ["44 to 47", "The throttle's mouse nub pushed part way: up, right, down, left. Pan."],
  ["48 to 51", "The mouse nub pushed all the way: up, right, down, left. Pan faster."],
]));
add(gap());
add(h1("Appendix B  Words used in this manual"));
add(table([2400, 6898], ["Word", "Meaning"], [
  ["Command", "Something X-Plane can do, with a name such as `sim/autopilot/servos_on`."],
  ["Dataref", "A value X-Plane keeps, such as the parking brake or the altitude. A button can write one; a light, alarm or page row can read one."],
  ["Macro", "Two or more steps on one button, run once when it is pressed."],
  ["Layer", "A mode, or a mode with the pinkie held, in which a button has its own job."],
  ["Live light", "A light that follows a dataref instead of the mode dial."],
  ["Alarm", "A light that blinks or alternates when something is wrong. Overrides live lights."],
  ["Master switch", "A test that keeps the screen, or the lights, off while a dataref has the wrong value."],
  ["Ready-made profile", "A profile that comes with X52 Pro and is never changed. Your edits go into a copy."],
  ["Plugin", "The part of X52 Pro that runs inside X-Plane (`X52Pro.xpl`)."],
  ["MFD", "The stick's multifunction display, the three-line screen on the throttle."],
]));
add(gap());
add(h2("Licence"));
add(p("X52 Pro is released under the MIT licence. The bundled X-Plane SDK keeps its own licence (see `THIRD_PARTY_NOTICES.md`). The USB commands and button numbering follow the open-source libx52 project's documentation of the device. Not affiliated with Laminar Research, Logitech or Saitek."));

// ---- contents page (internal links, no page numbers: robust in every viewer)
const contents = [
  new Paragraph({ pageBreakBefore: true, children: [new TextRun({ text: "Contents", font: FONT, size: 36, bold: true, color: ACCENT })], spacing: { after: 240 } }),
  ...toc.filter((t) => t.level === 1 || t.level === 2).map((t) => new Paragraph({
    indent: { left: t.level === 2 ? 440 : 0 },
    spacing: { before: t.level === 1 ? 140 : 0, after: 40 },
    children: [new InternalHyperlink({ anchor: t.id, children: [new TextRun({
      text: t.text, font: FONT, size: t.level === 1 ? 22 : 20, bold: t.level === 1, color: t.level === 1 ? "000000" : "333333" })] })],
  })),
];

// ---------------------------------------------------------------- document
const doc = new Document({
  creator: "X52 Pro",
  title: `X52 Pro ${version} manual`,
  description: "User manual for X52 Pro for X-Plane 12 on Apple Silicon",
  styles: {
    default: { document: { run: { font: FONT, size: 21 } } },
    paragraphStyles: [
      { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { font: FONT, size: 40, bold: true, color: ACCENT },
        paragraph: { spacing: { before: 0, after: 240 }, keepNext: true, outlineLevel: 0,
          border: { bottom: { style: BorderStyle.SINGLE, size: 8, color: ACCENT, space: 6 } } } },
      { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { font: FONT, size: 28, bold: true, color: "1B2A41" },
        paragraph: { spacing: { before: 320, after: 120 }, keepNext: true, outlineLevel: 1 } },
      { id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { font: FONT, size: 23, bold: true, color: "333333" },
        paragraph: { spacing: { before: 220, after: 80 }, keepNext: true, outlineLevel: 2 } },
    ],
  },
  numbering: { config: numberings },
  sections: [
    {
      properties: { page: { size: { width: PAGE_W, height: PAGE_H }, margin: { top: MARGIN, right: MARGIN, bottom: MARGIN, left: MARGIN } }, titlePage: true },
      headers: { default: new Header({ children: [new Paragraph({
        alignment: AlignmentType.RIGHT, children: [new TextRun({ text: `X52 Pro ${version}  –  User manual`, font: FONT, size: 17, color: GREY })] })] }),
        first: new Header({ children: [new Paragraph({ children: [] })] }) },
      footers: { default: new Footer({ children: [new Paragraph({
        alignment: AlignmentType.CENTER, children: [new TextRun({ children: [PageNumber.CURRENT], font: FONT, size: 18, color: GREY })] })] }),
        first: new Footer({ children: [new Paragraph({ children: [] })] }) },
      children: [...cover, ...contents, ...body],
    },
  ],
});

Packer.toBuffer(doc).then((buf) => {
  const out = path.join(__dirname, "X52-Pro-Manual.docx");
  fs.writeFileSync(out, buf);
  console.log("wrote", out, buf.length, "bytes");
});
