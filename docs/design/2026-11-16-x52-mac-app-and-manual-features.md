# X52 Mac App and Manual Features Implementation Plan

**Goal:** Add the manual's missing features to the X-Plane plugin and build a menu bar app that sets, saves and keeps applying lights, screen and clock settings.

**Architecture:** All behaviour lives in the existing pure C++ core (`src/core`) so it is unit tested; the plugin and a new Swift menu bar app are thin shells over it. The app reaches the C++ through a small C bridge. A lock file decides who writes to the stick.

**Tech Stack:** C++17, IOKit, X-Plane SDK 4.2, Swift/SwiftUI/AppKit built with `swiftc` from the Makefile.

**Spec:** `docs/superpowers/specs/2026-11-16-x52-mac-app-and-manual-features-design.md`

**Execution:** native, in this session (the user said "build it"). Not a git repository, so no commits.

## Global Constraints

- arm64, plugin min macOS 12, app min macOS 13.
- One profile format; the app changes only lights, brightness, clock, date and latched clutch.
- While the plugin holds `<profile dir>/.plugin-active` (flock), the app sends nothing to the stick.
- USB values (libx52 `commands.h`, `control.c`): blink index 0xb4 value 0x51/0x50; clock 2/3 offset index 0xc1/0xc2, value `h24<<15 | negative<<10 | minutes(0..1023)`; date 0xc4 + year 0xc8 packed per format.

## Review Focus

- Profile written by the older plugin (no new keys, header `1`): loads with defaults and gains wheel bindings once.
- App saves while the plugin has unsaved-to-disk button edits: bindings must survive (read-modify-write).
- Unreadable profile file: neither program overwrites it.
- Stick unplugged and replugged: lights come back without a restart.
- Wheel turned on a page with nothing to edit (Buttons, DME): nothing happens.

## Tasks

### 1. Core: profile fields and USB packers
Files: `src/core/profile.{h,cpp}`, `src/core/mfd.{h,cpp}`, `src/mac/x52device.{h,cpp}`, `tests/test_core.cpp`.
- `Profile`: `int version`, `bool clutchLatched`, `int clockOffset[2]` (minutes from GMT, -720..840), `bool clock12h[3]`, `int dateFormat` (0 DD-MM-YY, 1 MM-DD-YY, 2 YY-MM-DD). Keys: `clutch_latched`, `clock2_offset`, `clock3_offset`, `clock1_12h`..`clock3_12h`, `date_format`. Header written as `x52pro-profile 2`.
- `packBlink(bool)`, `packClockOffset(int clock, int minutes, bool h24)`, `packDate(day, month, year2, format)` returning two requests.
- `DeviceState`: `blink`, `clockOffset[2]`, `h12[3]`, `dateFormat`; `X52Device::stop(bool blank = true)`.
- Default profile binds 37/38/39 to `x52pro/mfd_up`, `mfd_down`, `mfd_select`; `addWheelBindings(Profile&)` for version-1 files.

### 2. Core: engine
Files: `src/core/engine.{h,cpp}`, tests.
- Unprogrammed button shows `buttonName`.
- Latched clutch: press toggles when `clutchLatched`.

### 3. Core: editable pages
Files: `src/core/pages.{h,cpp}` (new), `src/core/mfd.cpp` (old formatter removed), tests.
- `struct PageCursor { int item; bool editing; }`, `int pageItemCount(int page)`, `Lines formatSimPage(int page, const SimData&, const PageCursor&)`, `enum class Wheel { Up, Down, Select }`, `std::vector<std::string> pageInput(int page, PageCursor&, Wheel)` returning X-Plane command names to run once.

### 4. Plugin
Files: `src/plugin/{app.h,plugin.cpp,store.*,ui.cpp}`, `src/mac/x52input.cpp`, `tests/test_plugin.cpp`.
- Commands `x52pro/mfd_up|mfd_down|mfd_select`; blink while clutch engaged; clock fields to the device.
- Profile picking with clutch + POV hat; lock file; reload when the profile file changes; latched clutch checkbox.

### 5. App
Files: `src/app/agent.{h,cpp}`, `src/app/bridge.{h,cpp}`, `src/app/main.swift`, `src/app/Info.plist`, `Makefile`, `tests/test_agent.cpp`.
- `Agent` (C++): owns input + device, follows mode, yields to the plugin lock, read-modify-write saves.
- Swift: status item menu, settings window (Lights, Screen and clock, Test), start at login.

### 6. Verify
`make` (all tests), run the app against the real stick, install plugin with an atomic replace.
