# Three-state live lights and alarms (1.6.0)

## Intent
Today a live light has one condition and two colours (then / else), and an alarm has one trigger and one effect.
People want a light that shows three states in one place (for example green gear down, amber gear moving, off gear up),
and an alarm that escalates (amber, then red, then red flashing) without writing three separate alarms.

## Two separate things (as the user described them)
- **Live lights** show a state. They never grab attention.
- **Alarms** grab attention and take over the light. When the alarm clears, the light returns to what it showed
  before (live light, else the mode colour). This layering already exists (`shownColor`: alarms over live lights
  over the mode colour) and does not change. A test is added to lock it in.

## Goals
- Live light: an ordered list of up to 3 levels (condition, colour or off) plus an "otherwise" colour (can be off).
  First true level wins.
- Alarm: up to 3 levels, each with a condition, a colour and an effect (steady, blink, alternate). First true level
  wins; no true level means the alarm is inactive and the light falls through.
- Each step or level can have up to 3 conditions joined by AND or by OR (one join type per step, no mixing, to
  keep it simple). Today's limit is 2.
- Old profiles keep working: a rule becomes one step plus "otherwise"; an alarm becomes one level.

## Non-goals
- No merged editor for alarms and live lights.
- No mixed AND/OR groups or parentheses.
- No acknowledge: an alarm stops when the situation that triggered it ends.
- No change to the layering order.

## Constraints
- Profile format change: bump `Profile::version`, add a migration, keep generated profiles at `x52pro-profile 3`.
- Fire and Throttle lights are single colour (off or lit); steps for them only offer off or lit.
- Follow the feature checklist in CLAUDE.md (generator, profiles on disk, button maps, documents, in-app help).
- Version 1.6.0: the second number is raised on the user's decision.

## Approaches
1. **Shared step list for live lights and alarms** (chosen). One structure, alarms add an effect.
2. Live lights get the list, alarms stay single. Rejected: does not give escalating alarms.
3. One merged editor. Rejected: mixes two concepts the user wants kept apart.

## Left to the builder
- Exact file format for steps and levels, and how the migration reads old lines.
- Editor layout in the app (`main.swift`) and the plugin window (`ui.cpp`).
- Tests in `tests/test_core.cpp` and `tests/test_plugin.cpp`, including the fall-through test.
