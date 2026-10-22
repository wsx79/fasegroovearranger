#!/usr/bin/env bash
# Release text for a tag (v0.12.1): the matching English and Italian changelog sections.
# Usage: release-notes.sh v0.12.1 > notes.md
set -euo pipefail
version="${1#v}"

section() {
  awk -v heading="## ${version}" '
    index($0, heading) == 1 { found = 1; next }
    found && /^## / { exit }
    found { print }
  ' "$1"
}

en=$(section CHANGELOG.md)
it=$(section CHANGELOG.it.md)
if [ -z "$(printf '%s' "$en" | tr -d '[:space:]')" ]; then
  en="See CHANGELOG.md."
fi
if [ -z "$(printf '%s' "$it" | tr -d '[:space:]')" ]; then
  it="Vedi CHANGELOG.it.md."
fi

cat <<NOTES
## ${version}

${en}

## ${version}

${it}

| System | File |
|---|---|
| Windows installer | \`FaseGrooveArranger-Windows-Setup.exe\` |
| Windows zip | \`FaseGrooveArranger-Windows.zip\` |
| macOS | \`FaseGrooveArranger-macOS.zip\` |
| Linux | \`FaseGrooveArranger-Linux.zip\` |
NOTES
