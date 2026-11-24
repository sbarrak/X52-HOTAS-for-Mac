# Project rules

## Bug fixes

- Every bug fix must be tracked in `CHANGELOG.md` under a "Fixed" heading of the version that contains it.
- Each entry describes the bug (symptom and cause) and the fix, and names the version in which it was fixed.
- Put the changelog entry in the same commit as the fix, and keep it consistent with `VERSION`.

## Diffs

- Do not run `git diff` on every chat message; it wastes tokens.
- Before a commit or release, run `git diff --stat` and check that every bug fix has a changelog entry.

## Versions

- Only the third number goes up, by one, with each change: 1.2.3, 1.2.4 ... 1.2.10 ... 1.2.100 (it has no upper limit and never rolls over into the second number).
- The app's update check only looks at the first two numbers (x.y), so a third-number change never asks users to download again.
- Never raise the second number (y) or the first on your own. When a change is big enough to deserve it (a new feature people should update for), say so to the user and let them decide.

## Issues

- Every update to the software (new feature, change, bug fix) is recorded as a GitHub issue, so the issues document how the app evolved.
- Open the issue before or together with the branch, one issue per branch. Name the version it ships in (see `VERSION` and `CHANGELOG.md`) and describe what changes and why. For a bug, give the symptom and the cause.
- The branch's pull request closes its issue (`Closes #N` in the description), and the commit or PR mentions the issue number.
- Docs-only and housekeeping branches get an issue too, kept short. A change that does not touch the app (rules, docs, tooling) gets no version bump, so its issue names no version.
- Issues are public (the repository is public): no personal data, no local paths, no secrets.
- Before a release, check that every branch merged since the last release has its issue.

## Branches

- Once a branch's pull request is merged into `main`, delete the branch right away, on GitHub and locally, without waiting to be asked (`gh pr merge --delete-branch`, or `git push origin --delete <branch>` and `git branch -d <branch>`). Then switch to `main` and pull.

## Builds

- If the code changed, run `make` (it builds the app and plugin and runs the tests) just before every commit, so the committed change is known to build and a current app is in `bin/` to test.
- Do not commit when the build or the tests fail.

## Feature checklist

For a change to buttons, controls or profiles, besides the code and its tests:

- **Profiles:** add the change to the generator (`profiles/tools/aircraft.py`) and regenerate the ready-made profiles (`python3 profiles/tools/build_profiles.py`). Keep their header at `x52pro-profile 3` so they still get the soft stick curves when read. Bump the plugin's own profile version (`Profile::version`) and add a migration, so every older profile gets the change when read, unless the user programmed those buttons.
- **Every profile on disk:** the three hand-made files in `profiles/` (`default.x52`, `A330_new.x52`, `aircraft-Cessna_CitationX_mine.x52`), `my_profiles/out/profiles/`, and the user's X-Plane folder (`.../Output/preferences/X52Pro/`, including their own profiles). Back that folder up first. Edit only the lines the change adds.
- **Button maps:** the PDFs and Word files in `docs/mapping/` and `my_profiles/out/mapping/` come from a generator outside the repo, so patch their text in place with `tools/patch_maps.py` (edit its `RULES`; it edits the PDF page text with `pypdf` and the Word `document.xml`, needs `pypdf` and `reportlab` in a virtual environment outside the repo) and look at a rendered page afterwards. Back the folders up first. Keep `profiles/tools/build_sheets.py` in step.
- **Documents:** `README.md`, `profiles/README.md`, `my_profiles/README.md`, `CHANGELOG.md`, the manual (`docs/manual/build_manual.js`, rebuilt with `node`; it needs the `docx` npm package, installed outside the repo) and the in-app help text.
- **Testing in X-Plane:** run `make install` when the user wants to try the plugin, and tell them to restart X-Plane. Prefer X-Plane commands over keystrokes for buttons made by the plugin (keystrokes need macOS permission and queue up behind a busy simulator).
- **Housekeeping:** do not leave long-running commands in the background (no `find /`); stop them when done. If git fails with "Stale NFS file handle", the folder is on a synced drive: retry, and check `git status` for a half-finished checkout before going on.

## Releases

- Every merged change that raised `VERSION` gets its release as soon as it is merged, without waiting to be asked: tag `vX.Y.Z` on the merge commit in `main`, run `make release`, and create the GitHub release from that tag with `X52-Pro-X.Y.Z.zip` attached, as was done for 1.2.5.
- The release title is `X52 Pro X.Y.Z`. The notes say what was fixed or changed in that version, with the issue number, and link to `CHANGELOG.md`.
- A change that does not raise `VERSION` (rules, docs, tooling) gets no tag and no release.

## End of chat

- At the end of every chat, ask the user whether the chat can be archived.
- Before asking, check the chat for unresolved topics (open questions, unfinished work, unverified results, promised follow-ups). If there are any, list them concisely and ask what to do with each.
- If nothing is left, confirm that the chat can be archived.

## Agent coordination

Several agents (Claude, ChatGPT or others) may work on this repo at the same time. GitHub is the shared task log, so no agent needs a log file in the repo.

- **Task log:** each piece of work has one issue. An open issue means in progress; a closed issue means done (its PR merged, or the work was dropped). The issue names its branch and the version it ships in.
- **Status:** post a comment on the issue when work starts, when it is blocked, and when it is handed off.
- **Before starting:** read the open issues and PRs. If another open branch touches the same files, comment on that issue and ask the user before starting.
- **One writer per branch:** only the agent that owns a branch pushes to it. Other agents review by PR comment and never push to someone else's branch.
- **Review:** a review is a PR comment that says what was checked and what was found. It does not approve or merge.
- **Merging:** agents never merge. Merging stays with the user.
- **Public repo:** issues, PRs and comments contain no personal data, local paths or secrets.
