#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
scripting=${1:-$HOME/hlserver/tf2/tf/addons/sourcemod/scripting}
output=${2:-/tmp/nativevotes-nextmap-probe.smx}
"$scripting/spcomp" "$root/tests/nextmap_probe.sp" -i "$root" -i "$scripting/include" -o "$output"
