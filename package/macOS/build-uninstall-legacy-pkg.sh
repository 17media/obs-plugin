#!/bin/bash

set -euo pipefail

VERSION="${1:-0.0.0}"
OUT_NAME="${2:-obs-17live-uninstall-legacy.pkg}"

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/uninstall-legacy/misc"

/bin/chmod +x "$SCRIPTS_DIR/postinstall"

/usr/bin/pkgbuild \
  --nopayload \
  --identifier "com.17live.obsplugin.legacy-uninstaller" \
  --version "$VERSION" \
  --scripts "$SCRIPTS_DIR" \
  "$OUT_NAME"
