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
