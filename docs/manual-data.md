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
| `profile.based_on` | The profile it was copied from, or empty. |
| `profile.standard` | `yes` for a ready-made profile, otherwise `no`. |
| `profile.pinkie_shift` | `yes` when the pinkie switch is a shift, `no` when it is a normal button. |
| `profile.shift_button` | The name of the shift button, such as `Pinkie`. |
| `mode.1.name`, `mode.2.name`, `mode.3.name` | The names of the three modes (Takeoff, Cruise, Landing by default). |

## Lists (repeating rows)

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

A binding with no label shows its command text instead (`cmd:sim/...`).

## Planned (not in schema 1 yet)

Lights, alarms, screen pages and the axes. They will be added without changing the schema number.
