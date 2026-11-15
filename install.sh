#!/bin/sh
set -eu
repo=sevvy-wevvy/sevs-mod-manager
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
if [ -n "${SMM_TAG:-}" ]; then base="https://github.com/$repo/releases/download/$SMM_TAG"; else base="https://github.com/$repo/releases/latest/download"; fi
curl -fsSL "$base/smm-$name" -o "$bin/smm.part"
chmod +x "$bin/smm.part"
mv "$bin/smm.part" "$bin/smm"
[ "$(uname -s)" = Darwin ] && xattr -d com.apple.quarantine "$bin/smm" 2>/dev/null || true
"$bin/smm" --register || echo "could not register the launcher entry, run smm --register later" >&2
echo "installed to $bin/smm"
case ":$PATH:" in *":$bin:"*) ;; *) echo "add $bin to your PATH to run it as: smm" ;; esac
