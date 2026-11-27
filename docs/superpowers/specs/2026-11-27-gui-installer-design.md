# Graphical installer and uninstaller: design

Issue #139, branch `feat/gui-installer`, version 1.7.2.

## Why

Nobody should need the command line to install X52 Pro. Today the plugin has an **Install plugin** button, but the
ready-made profiles and the guides are not in the download, and there is no uninstall.

## Goals

- Install by dragging the app to Applications and answering a setup window.
- The X-Plane folder is shown and confirmed, or asked for when not found.
- Plugin, ready-made profiles and guides are installed from inside the app, after confirmation.
- A button opens the guides folder.
- Uninstall removes everything that was added, and asks whether to keep or delete the user's custom setup.
- Manual removal is fully described.

Not goals: Apple notarization (we stay free), automatic download of updates, changes to X-Plane's joystick settings.

Anti-goals: overwriting or deleting a profile the user made; deleting a file we do not know without saying so.

## Approaches

| | Approach | Trade-off | Result |
|---|---|---|---|
| A | DMG, then a setup window inside the app | One more step after dragging; reuses X-Plane detection; no password | **Chosen** |
| B | Mac installer package (.pkg) | Poor at choosing the X-Plane folder, needs a password, still blocked by macOS | Dropped |
| C | Separate installer app | A second program to maintain, same block | Dropped |

A DMG with a folder to drag was also considered: the app would sit one level down in Applications and files setup
needs could be moved. The app is dragged alone and carries everything inside it.

## Download

`make release` makes `X52-Pro-<version>.dmg` (replaces the zip) containing:

- `X52 Pro.app` and a shortcut to Applications.
- `Read me first.pdf`: drag, open, and if macOS blocks the app go to **System Settings > Privacy & Security >
  Open Anyway**; then setup appears. It ends with how to uninstall.
- A clickable shortcut to the Privacy & Security page, only if a test on a downloaded DMG shows it works.

The app cannot show this notice itself: macOS does not run it before approval. The same steps go in the README and
the release notes (the old "right-click, then Open" text is removed).

Inside the app (`Contents/Resources`): the plugin (as now), the ready-made profiles (only files with
`standard = 1`), the guides (manual, PDF button maps, uninstall guide).

## Setup window

Shown at start when setup has not been finished, or when a newer app finds an older plugin, profiles or guides.
Also opened from the menu bar menu (**Setup...**). Opened from the DMG instead of Applications, it only asks the
user to drag the app first.

1. **X-Plane folder.** Found: shown, with **Use this folder** and **Choose another...** (a list when several are
   found). Not found: **Choose folder...**, accepted only if it has `Resources/plugins`, then confirmed.
2. **What to install.** Three ticked boxes: plugin, ready-made profiles (with their number), guides. Nothing is
   written before **Install**.
3. **Result.** What was installed, what was left alone, **Open guides**, and "Restart X-Plane" (also said
   beforehand if X-Plane is running).

Setup can be skipped ("Not now"); the app then works as today.

Rules:

- A profile is written when it is missing, or when the file there is ready-made (`standard = 1`). Any other file with
  the same name is left alone and listed.
- Guides go to `Documents/X52 Pro/`. Only files we ship are overwritten. macOS may ask to allow access to Documents;
  if refused, setup says so and installs the rest.
- **Open guides** is in the main window and the menu bar menu. If the folder is missing, it offers to install it.
- Every X-Plane folder and file written is noted in `Application Support/X52Pro/install-record.txt`.
- After installing, the window reminds the user of *Do nothing* for programmed buttons and, if wanted, of the
  Accessibility permission for X-Plane (keystrokes), with a button that opens that settings page.

## Uninstall

**Uninstall X52 Pro...** in the app menu and the menu bar menu. It asks to quit X-Plane first, then offers:

- **Keep my custom setup** (default), or **Delete it too**.

It lists what will be removed and asks for confirmation. It covers every X-Plane folder found or recorded, so older
hand installs are removed too.

Always removed:

- `<X-Plane>/Resources/plugins/X52Pro/`
- ready-made profiles in `<X-Plane>/Output/preferences/X52Pro/` and in `Application Support/X52Pro/`
- files the app and plugin generate there: `.plugin-active`, `plugin-off`, `xplane-aircraft.txt`, `xplane-map.txt`,
  `*.tmp`, and the install record
- the guides we shipped
- start at login, caches and saved window state, the app's macOS privacy entries
- the app (moved to the Trash, as the last step)

Custom setup (kept or deleted as chosen): the user's own profiles (including ones set aside as `.deleted`),
`profile-choices.txt`, `stick-calibration.txt`, and the app's preferences.

Never touched: X-Plane's own settings, the X-Plane Accessibility permission, exported manuals and custom templates,
and any file we do not recognise (listed at the end). Folders are removed only when empty.

Builder's calls: "delete" moves the custom setup to the Trash, so a mistake can be undone; if a step fails, the rest
continues and the failures are listed, and the app stays in place so uninstall can be run again.

## Removing by hand

The same list with full paths, step by step. Shown from the uninstall window (**How to remove it by hand**), shipped
with the guides, summarised in `Read me first.pdf`, and in the README.

## Code and tests

- File work in plain C++ beside `src/mac/xplane.*` (what to install, what to remove, keep or delete), tested against a
  made-up home and X-Plane folder: fresh install, update, user file with a ready-made name, keep, delete, unknown
  files, several X-Plane folders, no X-Plane, a failed write.
- The two windows in a new Swift file; `main.swift` only gains the menu items and button.
- Uninstall is never tried on a real X-Plane folder without a backup.
- Docs: README, DETAILS, profiles/README, manual, in-app help, changelog. The release rule in `CLAUDE.md` changes
  from zip to DMG. `Install.command` stays for people who build from source.

## Decided by the owner

1. **Artwork:** the map PDFs stay as they are, in the repository and in the DMG.
2. **Version:** 1.7.2.
