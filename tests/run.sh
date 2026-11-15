#!/bin/sh
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
work=${SMM_TEST_DIR:-$root/../test-out}
mkdir -p "$work"
cc -std=c99 -g -Wall -Wextra -pthread -o "$work/test_core" "$here"/test_core.c "$root"/src/core/*.c || exit 1
"$work/test_core" "$work" "$root/fixtures"
rc=$?
rm -f "$work/test_core"
cc -std=c99 -g -Wall -Wextra -o "$work/test_webp" "$here"/test_webp.c "$root"/src/ui/webp.c || exit 1
"$work/test_webp" "$root/fixtures" || rc=1
rm -f "$work/test_webp"
exit $rc
