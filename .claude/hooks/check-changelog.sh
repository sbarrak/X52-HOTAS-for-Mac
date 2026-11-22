#!/bin/bash
# PreToolUse hook (Bash): block `git commit` of a bug fix unless CHANGELOG.md is part of it.
# A commit counts as a bug fix when its command text mentions fix/bug/hotfix/regression.
cmd=$(jq -r '.tool_input.command // ""')

echo "$cmd" | grep -Eq '(^|[;&|[:space:]])git[[:space:]]+commit' || exit 0
echo "$cmd" | grep -Eiq '\b(fix(es|ed)?|bug(s|fix)?|hotfix|regression)\b' || exit 0

staged=$(git diff --cached --name-only 2>/dev/null)
# With -a/--all, tracked modified files are committed too.
if echo "$cmd" | grep -Eq 'git[[:space:]]+commit[^;&|]*[[:space:]](-[a-zA-Z]*a[a-zA-Z]*|--all)\b'; then
  staged="$staged
$(git diff --name-only 2>/dev/null)"
fi

echo "$staged" | grep -qx 'CHANGELOG.md' && exit 0

echo "Blocked: this looks like a bug-fix commit but CHANGELOG.md is not included. Add a 'Fixed' entry (symptom, cause, fix, version) per CLAUDE.md, stage it, and commit again." >&2
exit 2
