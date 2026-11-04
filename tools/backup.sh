#!/bin/bash
# Explicit backups, never a background uploader. Private remote only.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/release.py --check
PRIVATE=''
for attempt in 1 2 3; do
  if PRIVATE="$(gh api 'repos/{owner}/{repo}' --jq .private)"; then break;fi
  sleep 2
done
if [[ "$PRIVATE" != true ]]; then
  echo 'Backup refused: unable to verify a private remote repository.' >&2;exit 1
fi
VERSION="$(python3 -c 'import json;print(json.load(open("version.json"))["version"])')"
git add --all
if ! git diff --cached --quiet; then git commit -m "Theatre console $VERSION: ${1:-backup}";fi
git push origin HEAD
