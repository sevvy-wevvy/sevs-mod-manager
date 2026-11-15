#!/bin/sh
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
work=${SMM_TEST_DIR:-$root/../test-out}
mkdir -p "$work"
cc -std=c99 -g -Wall -Wextra -pthread -o "$work/test_core" "$here"/test_core.c "$root"/src/core/*.c || exit 1
"$work/test_core" "$work" "$root/fixtures"
rc=$?
rm -f "$work/test_core"
exit $rc
