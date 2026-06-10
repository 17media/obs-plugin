#!/bin/bash

set -euo pipefail

LEGACY_APP_PLUGIN="/Applications/OBS.app/Contents/PlugIns/obs-17live.plugin"
LEGACY_APP_PLUGIN_DSYM="/Applications/OBS.app/Contents/PlugIns/obs-17live.plugin.dSYM"

echo "This script removes legacy installs inside the official OBS.app bundle:"
echo "  $LEGACY_APP_PLUGIN"
echo "  $LEGACY_APP_PLUGIN_DSYM"
echo
echo "It requires admin privileges."
echo

if [ -d "$LEGACY_APP_PLUGIN" ]; then
  sudo rm -rf "$LEGACY_APP_PLUGIN"
  echo "Removed: $LEGACY_APP_PLUGIN"
else
  echo "Not found: $LEGACY_APP_PLUGIN"
fi

if [ -d "$LEGACY_APP_PLUGIN_DSYM" ]; then
  sudo rm -rf "$LEGACY_APP_PLUGIN_DSYM"
  echo "Removed: $LEGACY_APP_PLUGIN_DSYM"
else
  echo "Not found: $LEGACY_APP_PLUGIN_DSYM"
fi

echo
echo "Done."
