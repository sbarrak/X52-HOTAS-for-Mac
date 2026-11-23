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
