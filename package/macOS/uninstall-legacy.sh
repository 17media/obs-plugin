#!/bin/bash

set -euo pipefail

LEGACY_APP_PLUGIN="/Applications/OBS.app/Contents/PlugIns/obs-17live.plugin"
LEGACY_APP_PLUGIN_DSYM="/Applications/OBS.app/Contents/PlugIns/obs-17live.plugin.dSYM"

detect_preferred_language() {
  local locale="${LANG:-en_US}"

  case "$locale" in
    zh*|ZH*)
      echo "zh-TW"
      ;;
    ja*|JA*)
      echo "ja-JP"
      ;;
    *)
      echo "en-US"
      ;;
  esac
}

print_message() {
  local key="$1"
  local lang="$2"

  case "$lang:$key" in
    zh-TW:intro)
      echo "此腳本會移除安裝在官方 OBS.app 套件內的舊版檔案："
      ;;
    ja-JP:intro)
      echo "このスクリプトは、公式 OBS.app バンドル内にインストールされた旧バージョンのファイルを削除します:"
      ;;
    en-US:intro)
      echo "This script removes legacy installs inside the official OBS.app bundle:"
      ;;
    zh-TW:requires_admin)
      echo "此操作需要管理員權限。"
      ;;
    ja-JP:requires_admin)
      echo "この操作には管理者権限が必要です。"
      ;;
    en-US:requires_admin)
      echo "It requires admin privileges."
      ;;
    zh-TW:removed)
      echo "已移除:"
      ;;
    ja-JP:removed)
      echo "削除しました:"
      ;;
    en-US:removed)
      echo "Removed:"
      ;;
    zh-TW:not_found)
      echo "未找到:"
      ;;
    ja-JP:not_found)
      echo "見つかりませんでした:"
      ;;
    en-US:not_found)
      echo "Not found:"
      ;;
    zh-TW:done)
      echo "完成。"
      ;;
    ja-JP:done)
      echo "完了しました。"
      ;;
    en-US:done)
      echo "Done."
      ;;
    *)
      echo ""
      ;;
  esac
}

PREFERRED_LANGUAGE="$(detect_preferred_language)"

echo "$(print_message intro "$PREFERRED_LANGUAGE")"
echo "  $LEGACY_APP_PLUGIN"
echo "  $LEGACY_APP_PLUGIN_DSYM"
echo
echo "$(print_message requires_admin "$PREFERRED_LANGUAGE")"
echo

if [ -d "$LEGACY_APP_PLUGIN" ]; then
  sudo rm -rf "$LEGACY_APP_PLUGIN"
  echo "$(print_message removed "$PREFERRED_LANGUAGE") $LEGACY_APP_PLUGIN"
else
  echo "$(print_message not_found "$PREFERRED_LANGUAGE") $LEGACY_APP_PLUGIN"
fi

if [ -d "$LEGACY_APP_PLUGIN_DSYM" ]; then
  sudo rm -rf "$LEGACY_APP_PLUGIN_DSYM"
  echo "$(print_message removed "$PREFERRED_LANGUAGE") $LEGACY_APP_PLUGIN_DSYM"
else
  echo "$(print_message not_found "$PREFERRED_LANGUAGE") $LEGACY_APP_PLUGIN_DSYM"
fi

echo
echo "$(print_message done "$PREFERRED_LANGUAGE")"
