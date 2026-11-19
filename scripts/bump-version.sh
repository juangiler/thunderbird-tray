#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/bump-version.sh <version> [release-date]

Updates all versioned project files:
  - CMakeLists.txt                               (project VERSION)
  - src/main.cpp                                 (QApplication application version)
  - resources/io.github.juangiler.thunderbird-tray.metainfo.xml
                                                 (<release version="..." date="...">)

Arguments:
  <version>       Semantic version, e.g. 0.0.1
  [release-date]  ISO date for the AppStream release entry (default: today, YYYY-MM-DD)

Examples:
  scripts/bump-version.sh 0.0.1
  scripts/bump-version.sh 0.1.0 2026-11-19
EOF
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" || $# -lt 1 || $# -gt 2 ]]; then
  usage
  [[ $# -ge 1 && (${1:-} != "-h" && ${1:-} != "--help") ]] || exit 0
  exit 1
fi

VERSION="$1"
DATE="${2:-$(date +%F)}"

if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.+-]+)?$ ]]; then
  echo "error: version '$VERSION' is not a valid semantic version (X.Y.Z)" >&2
  exit 1
fi

if ! [[ "$DATE" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; then
  echo "error: release date '$DATE' must be YYYY-MM-DD" >&2
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 - "$VERSION" "$DATE" <<'PY'
import pathlib
import re
import sys

version, date = sys.argv[1], sys.argv[2]

updates = [
    (
        pathlib.Path("CMakeLists.txt"),
        r'(\bVERSION\s+)[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.+-]+)?',
        r'\g<1>' + version,
        "CMake project VERSION",
    ),
    (
        pathlib.Path("src/main.cpp"),
        r'(app\.setApplicationVersion\(QStringLiteral\(")[^"]+("\)\);)',
        r'\g<1>' + version + r'\g<2>',
        "QApplication version",
    ),
    (
        pathlib.Path("resources/io.github.juangiler.thunderbird-tray.metainfo.xml"),
        r'(<release\s+version=")[^"]+("\s+date=")[^"]+(">)',
        r'\g<1>' + version + r'\g<2>' + date + r'\g<3>',
        "AppStream metainfo release",
    ),
]

for path, pattern, repl, label in updates:
    text = path.read_text(encoding="utf-8")
    new, count = re.subn(pattern, repl, text, count=1)
    if count != 1:
        raise SystemExit(f"error: could not update {label} in {path}")
    if new != text:
        path.write_text(new, encoding="utf-8")
    print(f"updated {path}: {label} -> {version}")

print(f"version bump complete: {version} ({date})")
PY
