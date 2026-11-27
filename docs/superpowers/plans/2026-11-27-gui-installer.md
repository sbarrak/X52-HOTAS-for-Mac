# Graphical Installer and Uninstaller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Install X52 Pro from a DMG with a setup window, and remove it again from inside the app.

**Architecture:** All file work is plain C++ in `src/mac/setup.*`, tested against a made-up home and X-Plane folder. The bridge exposes it to Swift, where a new `src/app/setup.swift` holds the setup and uninstall windows. The Makefile bundles profiles and guides into the app and builds the DMG.

**Tech Stack:** C++17, Swift/SwiftUI, make, `hdiutil`.

**Spec:** `docs/superpowers/specs/2026-11-27-gui-installer-design.md` (issue #139)

## Global Constraints

- macOS 13 or newer, arm64. No new third-party dependencies. No notarization.
- Version `1.7.2` (`VERSION` and `CHANGELOG.md`).
- Never overwrite or delete a profile without `standard = 1`, except by the user's "delete custom setup" choice, which moves it to the Trash.
- Never delete a file we do not recognise; never follow a symbolic link when removing. Folders go only when empty.
- Guides folder: `Documents/X52 Pro/`. Install record: `Library/Application Support/X52Pro/install-record.txt`.
- Bundle identifier: `serbar.x52pro.app`.
- Tests and trial runs use made-up folders. No uninstall on the real X-Plane folder without a backup.
- `make` passes before every commit.
- The map PDFs ship as they are.

## Review Focus

- A user's own profile has a ready-made file name: it survives install, update and uninstall (Tasks 1, 2).
- X-Plane folder not writable: that item is listed as failed, the others are installed (Task 1).
- X-Plane on a disk that is no longer there at uninstall: listed, nothing else stops (Task 2).
- A symbolic link or subfolder inside a profile folder: left in place and listed, target untouched (Task 2).
- Folder paths with spaces, non-ASCII letters or a trailing slash, also read back from the record (Task 1).

---

### Task 1: Install logic

**Files:**
- Create: `src/mac/setup.h`, `src/mac/setup.cpp`, `tests/test_setup.cpp`
- Modify: `Makefile` (`MAC` gains `setup.cpp`; `build/test_setup` target linking `setup.cpp`, `xplane.cpp`, `plugin/store.cpp`, `$(CORE)`; run it in `test`; check that every target using `$(MAC)` still links `plugin/store.cpp`)

**Interfaces:**
- Consumes: `isXPlaneDir`, `installPlugin`, `pluginState` (`xplane.h`); `Store::isStandard` (`plugin/store.h`).
- Produces:

```cpp
struct SetupPaths {
    std::string home;        // the user's home folder
    std::string resources;   // the app's Contents/Resources (X52Pro.xpl, profiles/, guides/)
};
std::string supportDir(const SetupPaths&);   // home/Library/Application Support/X52Pro
std::string guidesDir(const SetupPaths&);    // home/Documents/X52 Pro
std::string profilesDir(const SetupPaths&, const std::string& xplaneDir);  // <xp>/Output/preferences/X52Pro, "" xp: supportDir
struct InstallChoice { bool plugin = true, profiles = true, guides = true; };
struct Report { std::vector<std::string> done, kept, failed; };   // one path or sentence each
Report install(const SetupPaths&, const std::string& xplaneDir, const InstallChoice&);
int bundledProfileCount(const SetupPaths&);
struct InstallRecord { InstallChoice chosen{false, false, false}; std::vector<std::string> xplanes, files; };
bool readRecord(const SetupPaths&, InstallRecord&);          // false: setup never finished
bool setupNeeded(const SetupPaths&, const std::string& xplaneDir);
```

`xplaneDir` empty installs guides only; plugin and profiles are listed as failed.
Record file: one `key<TAB>value` per line, keys `item`, `xplane`, `file`; a later install adds to it.
`setupNeeded`: no record, or a chosen item is missing or differs from the bundle (a user's file in a profile's place does not count).

- [ ] **Step 1: Write failing tests** (helpers as in `tests/test_xplane.cpp`; profile fixtures are `"x52pro-profile 3\nstandard = 1\n"` plus a marker line)
  - `fresh_install_writes_plugin_profiles_and_guides`: files exist with bundled bytes; `failed` empty; record lists the X-Plane folder and files; `setupNeeded` false.
  - `a_users_profile_with_a_ready_made_name_is_kept`: existing file without `standard = 1` unchanged, listed in `kept`, `setupNeeded` false.
  - `an_older_ready_made_profile_is_updated`: `setupNeeded` true before, new bytes after.
  - `only_ready_made_files_count_as_bundled_profiles`: a bundled file without `standard = 1` is neither counted nor copied.
  - `unticked_items_are_not_written_or_nagged_about`.
  - `a_folder_that_cannot_be_written_fails_alone`: `chmod 0555` on `Output/preferences`: profiles in `failed`, plugin and guides installed.
  - `guides_install_without_x_plane`.
  - `odd_paths_survive_the_record`: X-Plane at `"…/Vôos de teste/X-Plane 12/"`, read back without the slash.
- [ ] **Step 2:** `make test` fails (no `setup.h`).
- [ ] **Step 3: Implement.** Each file is written to `<name>.new` and renamed over the target.
- [ ] **Step 4:** `make test` passes.
- [ ] **Step 5:** Commit `Setup: install logic (#139)`.

### Task 2: Uninstall logic

**Files:** Modify `src/mac/setup.h`, `src/mac/setup.cpp`, `tests/test_setup.cpp`

**Interfaces:**
- Consumes: Task 1.
- Produces:

```cpp
struct UninstallPlan {
    std::vector<std::string> remove;    // always removed (files)
    std::vector<std::string> custom;    // the user's setup: kept, or moved to the Trash by the app
    std::vector<std::string> unknown;   // left alone
    std::vector<std::string> folders;   // removed afterwards when empty, deepest first
    std::vector<std::string> missing;   // recorded X-Plane folders that are not there
};
UninstallPlan planUninstall(const SetupPaths&, const std::vector<std::string>& foundXPlanes);
Report removeFiles(const UninstallPlan&);
void pruneFolders(const UninstallPlan&);
```

X-Plane folders: found plus recorded, no repeats. Classification:
- `remove`: `plugins/X52Pro/mac_x64/X52Pro.xpl[.new]`; ready-made `*.x52` and `*.x52.deleted`; `.plugin-active`, `plugin-off`, `xplane-aircraft.txt`, `xplane-map.txt`, `*.tmp`, `*.new`, `.DS_Store`; the install record; recorded and bundled-name guides; everything under `Library/Caches/serbar.x52pro.app`, `Library/HTTPStorages/serbar.x52pro.app*`, `Library/Saved Application State/serbar.x52pro.app.savedState`.
- `custom`: other `*.x52` and `*.x52.deleted`, `profile-choices.txt`, `stick-calibration.txt`.
- `unknown`: anything else in those folders, including links and subfolders.

The app preferences are handled in Swift (Task 4).

- [ ] **Step 1: Write failing tests**
  - `uninstall_removes_what_was_installed` (fresh install, plan, remove, prune: no `X52Pro` folder, no guides folder).
  - `custom_setup_is_listed_apart_and_not_removed` (own profile, `.deleted` own profile, calibration, choices: all in `custom`, still on disk; folder survives prune).
  - `a_deleted_ready_made_profile_is_removed`.
  - `unknown_files_links_and_subfolders_are_left` (link target outside unchanged).
  - `hand_installs_without_a_record_are_found`.
  - `a_missing_x_plane_folder_is_reported_not_fatal`.
  - `a_file_that_cannot_be_removed_is_listed_and_the_rest_go`.
- [ ] **Step 2:** `make test` fails.
- [ ] **Step 3: Implement** (`lstat`, never `stat`, while classifying).
- [ ] **Step 4:** `make test` passes.
- [ ] **Step 5:** Commit `Setup: uninstall logic (#139)`.

### Task 3: Bundle contents and setup window

**Files:**
- Create: `src/app/setup.swift`
- Modify: `src/app/bridge.h`, `src/app/bridge.cpp`, `src/app/main.swift` (menu item, footer button, start-up), `Makefile`, `tests/test_agent.cpp`, `VERSION`, `CHANGELOG.md`

**Interfaces:**
- Consumes: Tasks 1 and 2.
- Produces (C bridge; list results are newline-joined):

```c
void x52_setup_init(const char* home, const char* resources);
int  x52_setup_needed(const char* xplane_dir);
int  x52_setup_profile_count(void);
void x52_setup_install(const char* xplane_dir, int plugin, int profiles, int guides);
const char* x52_setup_report(int which);        // 0 done, 1 kept, 2 failed
const char* x52_setup_guides_dir(void);
void x52_uninstall_plan(void);                  // uses findXPlane and the record
const char* x52_uninstall_list(int which);      // 0 remove, 1 custom, 2 unknown, 3 missing
const char* x52_uninstall_remove(void);         // the failures
void x52_uninstall_prune(void);
```

Swift: `struct SetupView: View`, `AppDelegate.openSetup()`, `Model.openGuides()`.

- [ ] **Step 1: Failing test** in `test_agent.cpp`, `setup_bridge_installs_and_reports` (temporary home and resources: done list non-empty, failed empty, `x52_setup_needed` 0).
- [ ] **Step 2:** Implement the bridge; `make test` passes.
- [ ] **Step 3: Makefile `app`:** copy `profiles/*.x52` with `^standard = 1` to `Resources/profiles`; copy `docs/manual/X52-Pro-Manual.docx` and `docs/mapping/*.pdf` to `Resources/guides`; compile `setup.swift` together with `main.swift` (top-level code stays in `main.swift` only) and add it to the target's prerequisites. Sign after copying.
- [ ] **Step 4: Setup window** as the spec's "Setup window": three steps, **Use this folder**, **Choose another...**, **Choose folder...**, three ticks, **Not now**, **Install**, result with **Open guides** and "Restart X-Plane"; *Do nothing* reminder and an Accessibility settings button (`x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility`). Confirming a folder stores `xplaneFolder` and restarts the model.
  - Opens at start (not in `--snapshot` or `--background` runs) when `x52_setup_needed`.
  - From a translocated path or a read-only volume: only the "drag to Applications" message.
  - Menu items **Setup...**; **Open guides** in the menu bar menu and the footer, offering setup when the folder is missing.
  - The existing **Install plugin** button stays.
- [ ] **Step 5:** `VERSION` 1.7.2 and the changelog "Added" entry; `make` passes.
- [ ] **Step 6:** Try it with a temporary `HOME` and a made-up X-Plane folder; screenshot each step.
- [ ] **Step 7:** Commit `Setup window, bundled profiles and guides (1.7.2, #139)`.

### Task 4: Uninstall window

**Files:** Modify `src/app/setup.swift`, `src/app/main.swift`

**Interfaces:** Consumes the Task 3 bridge. Produces `struct UninstallView: View`, `AppDelegate.openUninstall()`.

- [ ] **Step 1: Window:** **Uninstall X52 Pro...** in both menus; blocked while X-Plane runs; **Keep my custom setup** (default) / **Delete it too**; the lists; **How to remove it by hand**; a destructive confirmation.
- [ ] **Step 2: Order of work:** `model.stop()`; remove files; if deleting, `FileManager.trashItem` each custom file; prune; unregister the login item; `tccutil reset All serbar.x52pro.app`; if deleting, `removePersistentDomain`; show failures and unknown files. Only with no failures: trash the app (only when it is in an Applications folder) and quit. With failures the app stays and restarts the model.
- [ ] **Step 3:** `make` passes. Trial run with a temporary home, never the real folder.
- [ ] **Step 4:** Commit `Uninstall from the app (#139)`.

### Task 5: DMG and documents

**Files:**
- Create: `docs/install/read-me-first.txt`, `docs/install/uninstall-by-hand.txt`, `tools/make_text_pdf.swift` (arguments: input text, output PDF; A4)
- Modify: `Makefile`, `README.md`, `DETAILS.md`, `profiles/README.md`, `SECURITY.md` (only if wrong), `Install.command`, `CLAUDE.md` (release rule: `X52-Pro-X.Y.Z.dmg`), `docs/manual/build_manual.js` and the rebuilt manual, in-app help text, `CHANGELOG.md`

- [ ] **Step 1:** Write both texts from the spec's "Download", "Uninstall" and "Removing by hand", with full paths. `uninstall-by-hand` PDF also goes into `Resources/guides`.
- [ ] **Step 2:** `make release` builds `bin/X52-Pro-$(VERSION).dmg` with `hdiutil create -format UDZO` from a staging folder: `X52 Pro.app`, `Applications` link, `Read me first.pdf`.
- [ ] **Step 3:** Verify: `hdiutil verify`, mount, list the three entries, `codesign --verify --deep` the app, detach.
- [ ] **Step 4:** Test a Privacy & Security shortcut file on a quarantined copy of the DMG. Include it only if it opens the page; record the result on issue #139.
- [ ] **Step 5:** Update the documents; remove "right-click, then Open"; rebuild the manual with `node`; render a page of each new PDF and look at it.
- [ ] **Step 6:** `make` passes; `git diff --stat`; commit `DMG release and install documents (#139)`.
- [ ] **Step 7:** Owner's check: install from the DMG on the real Mac. Then PR with `Closes #139` (push and PR when the owner says so).
