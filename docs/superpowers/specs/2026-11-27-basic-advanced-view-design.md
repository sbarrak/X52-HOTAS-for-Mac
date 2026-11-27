# Basic and Advanced views: design

Status: approved. Issue #141, branch `feat/basic-advanced-view`, version 1.8.0.

## Why

The amount of text and the number of settings in the app can be overwhelming. The user picks how much to see.

## Goals

- A **Basic / Advanced** switch in the window header, remembered between launches.
- In both views, long explanations move behind an ⓘ button.

Not goals: changing what any setting does, changing profiles or the plugin, redesigning the plugin's window in
X-Plane, the Pages editor, the calibration window or setup and uninstall.

Anti-goals: a hidden setting that acts without the user being able to tell; a view switch that changes a profile.

## Approaches

| | Approach | Trade-off | Result |
|---|---|---|---|
| A | View switch plus ⓘ help | Most work; covers both text and settings | **Chosen** |
| B | ⓘ help and a "More options" section per tab, no modes | Simpler; no two views | Dropped |
| C | View switch only | Quicker; Advanced stays as wordy as today | Dropped |

## What each view shows

| Tab | Basic | Advanced adds |
|---|---|---|
| Buttons | Button list, mode, Command, name on screen, Import, Clear | Add Keystroke, Dataref, Wait; empty held and released sections; numbering help |
| Lights | Everything | Nothing |
| Alarms | List, add from template, remove, order, name, light, effect, colour, Try; the condition as one read-only line | Add blank alarm, condition editor |
| Screen & clock | Brightness, clocks, date order, mode names | Name display time, latched clutch, fast wheel jumps |
| Axes | Drive stick and throttle, dead zone, expo, invert, calibrate and reset, dial jobs and invert, slider flaps | Smoothing, damping, Steady, From/To, zoom steps, calibration values, list of every control |
| Test | Everything | Nothing |
| Live lights, Pages, Master & brightness | Tab hidden | Whole tab |
| Footer | Plugin state, install or update, messages, Open guides | The two folder paths and X-Plane folder... (also shown in Basic while X-Plane is not found) |

Both views keep: errors, the ready-made profile notice, the header, the profile menu, and the instruction to set
programmed buttons to **Do nothing** in X-Plane.

## Behaviour

- The view is an app preference, never part of a profile.
- First launch with no saved view: **Advanced** if the app already has saved preferences from earlier use (an
  upgrade), otherwise **Basic**.
- Switching to Basic while on a hidden tab opens Buttons. Command-1 to Command-n follow the visible tabs.
- Buttons, Basic: an action the profile already has (held, released, keystroke, dataref, wait) stays visible and
  editable.
- Hidden content is announced, only when the profile has it:
  - Hidden tabs with content: one line by the tabs, for example "In Advanced: Live lights, Pages · Show".
  - Hidden settings on a visible tab that differ from their defaults: one line on that tab,
    "Advanced settings in use · Show". This includes brightness that follows a dataref, on Lights and Screen.
  - **Show** switches to Advanced.

## Help text

- A paragraph of explanation becomes at most one short line plus an ⓘ button. The ⓘ opens a popover with the full
  text; no information is dropped.
- Warnings, errors and what the user must do to be safe stay visible, shortened (the Do nothing instruction,
  calibrating on the ground or paused).
- ⓘ buttons work with the keyboard and have a spoken label.

## Testing and documents

- Tests for the first-launch default and for "this profile has hidden content".
- Every tab checked in both views at the smallest window size; screenshots in `docs/images/` redone.
- README, DETAILS, manual, in-app help and CHANGELOG updated.

## Left to the builder

The ⓘ component, the wording of the short lines, exact placement of the switch and notices, and how the code
decides that a profile has hidden content.
