# Project rules

## Bug fixes

- Every bug fix must be tracked in `CHANGELOG.md` under a "Fixed" heading of the version that contains it.
- Each entry describes the bug (symptom and cause) and the fix, and names the version in which it was fixed.
- Put the changelog entry in the same commit as the fix, and keep it consistent with `VERSION`.

## Diffs

- Do not run `git diff` on every chat message; it wastes tokens.
- Before a commit or release, run `git diff --stat` and check that every bug fix has a changelog entry.
