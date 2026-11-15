#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
what="${1:-}"
cc="${CC:-cc}"
out=build
ext=
libs=(-lm)
case "$what" in
win)
  cc="${CC:-x86_64-w64-mingw32-gcc}"
  out=build-win
  ext=.exe
  libs=(-lws2_32 -lgdi32 -lole32 -lshell32 -ladvapi32 -static)
  ;;
*)
  case "$(uname -s)" in
  Linux) libs+=(-ldl -lpthread) ;;
  Darwin) libs+=(-framework Cocoa -framework CoreGraphics -lpthread) ;;
  esac
  ;;
esac
mkdir -p "$out"
srcs=(src/main.c src/goome_impl.c src/ui/*.c src/core/*.c)
if [ "$what" = win ]; then
  "${WINDRES:-x86_64-w64-mingw32-windres}" assets/smm.rc -O coff -o "$out/smm_rc.o"
  srcs+=("$out/smm_rc.o")
fi
$cc -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-truncation \
  -o "$out/smm$ext" "${srcs[@]}" "${libs[@]}"
echo "built $out/smm$ext"
case "$what" in
test) sh tests/run.sh ;;
shots)
  export SMM_DEMO=1 SMM_FIXTURES="$PWD/fixtures" SMM_DEMO_DIR="$PWD/../demo-tmp" SMM_SHOT="${SMM_SHOT:-$PWD/../shots/new}"
  if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null; then xvfb-run -a "$out/smm"; else "$out/smm"; fi
  ;;
esac
