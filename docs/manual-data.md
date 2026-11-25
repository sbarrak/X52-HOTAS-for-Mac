# Manual data: what a manual template can show

The app's "Export manual" fills a Word template with the data of one profile. This file is the contract between
the app and the templates: every field name a template may use, and what it contains. The code is
`src/core/manual.cpp`.

**Schema 1.** The schema number goes up only when a field is renamed or removed, or its meaning changes.
Adding a field does not change it, so an older template keeps working with a newer app. A template says which
schema it needs; an app that knows an older schema refuses it with a message.

All values are plain text. A field with nothing to say is an empty string, never missing.

## Single values

| Tag | Contains |
|---|---|
| `manual.schema` | The schema number (`1`). |
| `app.version` | Version of the app that made the export, such as `1.5.0`. |
| `profile.name` | The profile's name. |
| `profile.file` | The profile's file name, such as `aircraft-F-14D.x52`. |
| `profile.hold_ms` | How long a button is held before it counts as a hold, such as `400 ms`. |
| `profile.release_actions` | The buttons that do something when let go, or `none`. |
| `profile.power_conditions` | When the screen and the lights are on (`Screen: always on; lights: always on`). |
| `shift.m1`, `shift.m2`, `shift.m3` | `yes` when the Pinkie is a shift in that mode. |
| `profile.based_on` | The profile it was copied from, or empty. |
| `profile.standard` | `yes` for a ready-made profile, otherwise `no`. |
| `profile.pinkie_shift` | `yes` when the pinkie switch is a shift, `no` when it is a normal button. |
| `profile.shift_button` | The name of the shift button, such as `Pinkie`. |
| `mode.1.name`, `mode.2.name`, `mode.3.name` | The names of the three modes (Takeoff, Cruise, Landing by default). |

## The fixed assignment tables

The manual lists the same controls in the same order for every profile, so a table has a row for each control and a cell
for each mode. A cell's tag is `<table>.<control>.m<mode>`, for example `stick.a_tap.m2`. A control with nothing
assigned shows an em dash.

| Table | Tag prefix | Controls (the `<control>` part of the tag) |
|---|---|---|
| Joystick assignments | `stick` | `tr1`, `tr2`, `f_covered_fire`, `a_tap`, `a_hold`, `b`, `c`, `p_pinkie`, `h1_main_hat`, `h2_up`, `h2_right`, `h2_down`, `h2_left`, `t1_up`, `t1_down`, `t2_up`, `t2_down`, `t3_up`, `t3_down`, `roll_pitch_twist` |
| Throttle assignments | `throttle` | `d`, `e_press`, `i_press`, `mb_mouse_button`, `mw_forward`, `mw_aft`, `mw_press`, `th_up`, `th_right`, `th_down`, `th_left`, `ms_mouse_nub`, `l_turn`, `r_turn_press`, `e_upper_ring`, `i_lower_ring`, `ax_lever`, `sl_slider` |
| Joystick, Pinkie held | `pinkie_stick` | the same as `stick` |
| Throttle, Pinkie held | `pinkie_throttle` | the same as `throttle` |

- `a_tap` is what a tap on A does and `a_hold` what holding it does. The other buttons show their press, and
  ` (hold: ...)` when a hold does something else.
- `h1_main_hat` and `ms_mouse_nub` sum up four (or eight) buttons: `Look in direction` and `Pan / far: fast pan` when they
  have the usual look and pan commands, otherwise the labels one after another.
- `p_pinkie` shows `Shift` in the modes where the Pinkie is a shift.
- `i_press` is `Safe mode / picker`; `l_turn` and `r_turn_press` are the screen's wheels (`Change page`, `Cursor / select`);
  the two rings show what the dial is set to; `ax_lever`, `sl_slider` and `roll_pitch_twist` say whether the plugin drives them.
  These rows show an em dash in the Pinkie tables.

## The display and lights

| Tag | Contains |
|---|---|
| `pages.<1..12>.m<1..3>` | The page the left wheel reaches in that position and mode (hidden pages left out); empty after the last. |
| `light.<fire, a, b, d, e, t1, t2, t3, pov, clutch, throttle>.m<1..3>` | The light's colour in that mode as an icon (a hollow disc when off), with ` Pinkie ` and a second icon after it when the colour differs while the Pinkie is held. `...m<n>.name` is the same in words (`green`, `red (Pinkie: amber)`). |
| `alarm.<1..8>.name`, `.light`, `.m1`, `.m2`, `.m3`, `.rate` | An alarm: its name, its light, and in each mode an icon for its colour: a full disc when steady, black rays when flashing, a split disc (primary colour left, secondary right) when alternating, three colours when it uses the mode light. The rate is `Steady`, `Slow` or `Fast`. Rows without an alarm are empty. |

**The light icons** are 21 PNG files in `templates/light-icons/` (drawn by `tools/make_light_icons.py`), bundled with the app
and added to the Word file when they are used, so a template only has to give the cell its alignment. A value shows
an icon with `\x04<name>\x05`: `off`; `g`, `a`, `r` (steady); `g_flash`, `a_flash`, `r_flash`; `ra`, `ar`, `rg`, `gr`,
`ag`, `ga` (two colours, primary first) and the same with `_flash`; `gar` and `gar_flash` (three colours: the mode's own).
A value can also colour part of itself with `\x01RRGGBB,size\x02 ... \x03`; `stripStyle` gives the plain text.

Two lists have no fixed size, but never fewer than 3 rows (padded with empty rows):

| List | Row fields |
|---|---|
| `user_pages` | `page.title`, `page.fields` (the labels of the page's rows) |
| `rules` | `rule.light`, `rule.condition` (the ready-made rule's name, such as `Parking brake`, else the test on the dataref), `rule.active` and `rule.otherwise` (icons; the three-colour icon when it leaves the mode's colour; `rule.active_name` and `rule.otherwise_name` in words) (the live lights) |

## Plain lists of buttons (optional)

For a simpler layout than the manual's, a template can repeat one table row for every button.

A template repeats a table row once per entry of a list.

| List | Rows |
|---|---|
| `stick_buttons` | Every button of the stick, in button order: trigger, fire, A, B, C, pinkie, the three toggles, trigger 2nd stage, hat 2, the main hat (POV 1). |
| `throttle_buttons` | Every other button: D, E, mouse button, scroll wheel, throttle hat, mode dial, clutch, the screen's buttons and wheel, the mouse nub. |

Each row of those lists has:

| Tag | Contains |
|---|---|
| `button.number` | The button's number as the plugin counts it (1 to 51). |
| `button.name` | The button's name, such as `Trigger` or `Throttle hat up`. |
| `button.m1`, `button.m2`, `button.m3` | What it does in mode 1, 2, 3: the label of the press, and ` (hold: label)` when a hold does something else. A binding for any mode shows in all three. Empty when nothing is bound. |
| `button.p1`, `button.p2`, `button.p3` | The same with the pinkie held. |
| `button.used` | `yes` when any of the six cells above is not empty, otherwise `no`. |
| `button.used_plain` | `yes` when any of the three plain mode cells is not empty. |
| `button.used_pinkie` | `yes` when any of the three Pinkie cells is not empty. |

A binding with no label shows its command text instead (`cmd:sim/...`).

**Only the used rows:** tag the row control `repeat:stick_buttons:used_plain` (or `:used_pinkie`, `:used`) and the
buttons with nothing bound are left out. Without the part after the second colon, every button is listed.

## Making and changing a template

Step by step, for people who just use Word: [custom-manual-template.md](custom-manual-template.md). The short version:

The shipped template is `templates/X52-Manual-Template.docx`: the layout of the standard aircraft maps (pictures, fixed tables, display and lights dashboard). Open it in Word and restyle it freely: fonts, colours,
page layout, extra text and pictures are all yours.

- **Show a value:** Developer tab > Plain Text Content Control, then Properties > Tag: the tag from the tables above.
  Format the control's text the way you want the value to look.
- **Repeat a table row:** put a content control around the whole row, tag it `repeat:stick_buttons` (or
  `repeat:throttle_buttons`), and use the row tags (`button.name`, `button.m1`, ...) in its cells.
- **Needs a newer app?** File > Info > Properties > Advanced > Custom: add a number property `X52TemplateSchema` with
  the schema the template needs. An app that knows an older schema will refuse it.
- **Start over:** `python3 tools/make_manual_template.py` rebuilds the template from the empty aircraft profile template in `docs/mapping/` (standard library only). The pictures, the header and the layout come from that file; the aircraft picture in the header is a placeholder you replace in Word.
- **Try a template without the app:** `bin/x52manual profile.x52 template.docx out.docx` (built by `make`); it finds the light icons in the `light-icons` folder next to the template.
- **Redraw the light icons:** `python3 tools/make_light_icons.py` (standard library only).

## Planned (not in schema 1 yet)

The aircraft picture and the per-aircraft systems pages of the standard maps. They will be added without changing the schema number.
