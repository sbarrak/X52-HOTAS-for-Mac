# Pages editor in the menu bar app: handoff

Goal: a **Pages** tab in the menu bar app (`X52 Pro.app`) to add, edit, reorder and remove the user's own
screen pages and rows, without editing the profile file by hand. The engine is done (see below); this is
the editor on top of it. Start from a fresh branch off `main`.

## What already exists

- **Data and file format**: `src/core/pagetree.h/.cpp`. `PageTree` holds `UserPage { id, title, parent, rows }`;
  `PageRow { kind (Text|Value|Link), text, format, dataref, target }`. `serializePages` / `parsePageLine` /
  `parseRowLine` write and read the lines in the profile file:
  ```
  page <id> "<title>" [in <parent id>]
  row <page id> text "<words>"
  row <page id> value "<label>" "<format>" <dataref>
  row <page id> link "<label>" <page id>
  ```
  A page without `in` is reached with the left wheel; a page with `in` is only opened by a link row.
  Ids are letters, digits, `-`, `_` and are unique. A row's page must be defined above it in the file.
- **Profile**: `Profile::pages` (`src/core/profile.h`) is saved and loaded with the rest of the profile.
- **Rendering and navigation** (what the stick shows): `src/core/pageview.h/.cpp`, `formatUserPage` and
  `pageTreeInput`. `formatPageValue` (pagetree.cpp) shows a number with a format such as `%.0f kg`
  (one `%d %i %f %e %g`, flags, width, precision; anything else shows the number plainly).
- **Plugin**: `src/plugin/plugin.cpp` (`App::pageOrder`, `stepPage`, `userPageLines`, `wheel`). Order of
  the left wheel: radio pages, the user's top-level pages, Buttons, Brightness.
- **Text of the behaviour** is in `README.md` ("Your own screen pages").
- Tests: `tests/test_core.cpp` (`user_pages_*`, `value_formats_*`), `tests/test_plugin.cpp`.

## What to build

1. **Bridge calls** (`src/app/bridge.h/.cpp`, plain C, same style as the alarm and button calls). Edit through
   `agent->change([](Profile& p){ ... })`, which re-reads the file, applies the edit and saves, so button
   assignments made elsewhere are never lost. Suggested calls:
   - `x52_page_count()`, `x52_page_id(i)`, `x52_page_title(i)`, `x52_page_parent(i)`
   - `x52_page_add(title, parent)` (makes a unique id from the title), `x52_page_remove(i)` (also remove or
     clear links that pointed to it, and sub-pages of it), `x52_page_move(i, direction)` (order of top-level pages),
     `x52_page_set(i, title, parent)`
   - `x52_row_count(page)`, `x52_row_kind/text/format/dataref/target(page, row)`, `x52_row_add(page, kind)`,
     `x52_row_remove`, `x52_row_move`, `x52_row_set(page, row, kind, text, format, dataref, target)`
   - `x52_pages_restore_default()` clears `profile.pages` (the built-in radio, Buttons and Brightness pages
     always exist, so this brings back the default screen).
   - A preview helper is optional but useful: render `formatUserPage` for a page with the live values the app
     reads over UDP (see `Agent::liveStatus`), or with made-up values.
2. **Swift tab** (`src/app/main.swift`): add `(7, "Pages")` to `tabList` and a case in `SettingsView`.
   Layout idea: left, a list of pages (top-level first, sub-pages indented) with Add / Remove / Move up / Move
   down; right, the selected page's title, "Opens from" (top level or inside a page), and its rows. Each row
   has a kind picker (Text / Value / Link); Value rows take a dataref (reuse `CatalogField(kind: 1, ...)`, which
   searches X-Plane's `DataRefs.txt`), a label and a format (with a small example, e.g. `%.0f kg`); Link rows pick
   a target page from a list. Include a **Restore default pages** button with a confirmation.
3. **Show the page as the stick shows it**: a small black 3-line box (like the Test tab's) with the preview.
4. **Tests** in `tests/test_agent.cpp` (the bridge is linked into that test; see
   `buttons_are_programmed_from_the_app_and_found_in_x_plane_lists`): adding pages and rows, unique ids,
   removing a page clears links to it, moving, and that the saved file parses back to the same tree.
5. **README**: replace "write them in the profile file" with the tab, and keep the file format documented.

## Rules learned the hard way

- `@State` does not compile here (no Xcode, only the command line tools): keep view state in the `Model`
  (an `ObservableObject`, see `importNote`, `btn`).
- Never hand `std::find` two separate temporaries (`agent->profiles().begin()` and `.end()`): copy first.
- Empty rows, pages and steps must survive a save and reload (the editor saves at once, with fields still
  blank). The parser accepts them; keep it that way.
- `make` builds everything, runs all tests, and `make install-app` + `make install XPLANE="..."` install.
  If the app build fails with duplicate symbols, delete stray `build/appobj/* 2.o` copies (a Synology sync copy).
- Existing profiles are never overwritten without the user asking; a file with a mistake is never replaced.
