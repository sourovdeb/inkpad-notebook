#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMPDIR:-/tmp}/pbtypewriter-host-tests
mkdir -p "$OUT"

c++ -std=c++14 -Wall -Wextra -Werror \
  -I"$ROOT/src/device" \
  "$ROOT/tests/model_match_test.cpp" \
  "$ROOT/src/device/device_match.cpp" \
  "$ROOT/src/device/input_scan.cpp" \
  -o "$OUT/model_match_test"
"$OUT/model_match_test"

c++ -std=c++14 -Wall -Wextra -Werror \
  -I"$ROOT/src/device" \
  "$ROOT/tests/keymap_test.cpp" \
  -o "$OUT/keymap_test"
(cd "$ROOT" && "$OUT/keymap_test")

c++ -std=c++14 -Wall -Wextra -Werror \
  -I"$ROOT/src/core" \
  "$ROOT/tests/text_core_test.cpp" \
  "$ROOT/src/core/text_core.cpp" \
  -o "$OUT/text_core_test"
"$OUT/text_core_test"

c++ -std=c++14 -Wall -Wextra -Werror \
  -I"$ROOT/src/core" \
  "$ROOT/tests/workspace_test.cpp" \
  "$ROOT/src/core/workspace_core.cpp" \
  -o "$OUT/workspace_test"
"$OUT/workspace_test"

# This fixture lives only under /tmp. Never use a reader's document folder.
rm -rf /tmp/pb-workspace-ui-test
c++ -std=c++14 -Wall -Wextra -Werror \
  -I"$ROOT/tests/stubs" -I"$ROOT/src/device" -I"$ROOT/src/core" \
  "$ROOT/tests/workspace_ui_test.cpp" "$ROOT/src/core/workspace_core.cpp" \
  -o "$OUT/workspace_ui_test"
"$OUT/workspace_ui_test" "$ROOT/kbLayouts/fr.keys"
