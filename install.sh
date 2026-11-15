#!/bin/sh
set -eu
repo=sevvy-wevvy/sevs-mod-manager
tag="${SMM_TAG:-dev-latest}"
bin="${SMM_BIN:-$HOME/.local/bin}"
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64) name=linux-x64 ;;
  Linux-aarch64|Linux-arm64) name=linux-arm64 ;;
  Darwin-arm64) name=mac-arm64 ;;
  Darwin-x86_64) name=mac-x64 ;;
  *) echo "no build for $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac
command -v curl >/dev/null || { echo "install curl first, the app uses it too" >&2; exit 1; }
mkdir -p "$bin"
echo "downloading smm-$name"
curl -fsSL "https://github.com/$repo/releases/download/$tag/smm-$name" -o "$bin/smm.part"
chmod +x "$bin/smm.part"
mv "$bin/smm.part" "$bin/smm"
[ "$(uname -s)" = Darwin ] && xattr -d com.apple.quarantine "$bin/smm" 2>/dev/null || true
if [ "$(uname -s)" = Linux ]; then
  apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
  mkdir -p "$apps"
  icons="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"
  mkdir -p "$icons"
  curl -fsSL "https://raw.githubusercontent.com/$repo/dev/assets/icon.png" -o "$icons/sevs-mod-manager.png" || true
  printf '[Desktop Entry]\nType=Application\nName=Sevs Mod Manager\nExec=%s/smm\nIcon=sevs-mod-manager\nStartupWMClass=sevs-mod-manager\nCategories=Game;Utility;\nTerminal=false\n' "$bin" > "$apps/sevs-mod-manager.desktop"
fi
echo "installed to $bin/smm"
case ":$PATH:" in *":$bin:"*) ;; *) echo "add $bin to your PATH to run it as: smm" ;; esac
