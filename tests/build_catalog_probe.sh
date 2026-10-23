#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
scripting=${1:-$HOME/hlserver/tf2/tf/addons/sourcemod/scripting}
output=${2:-/tmp/nativevotes_catalog_probe.smx}
probe=${3:-catalog_probe.sp}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
for function in BuildNominationCategoryMenu GetCategoryLabel BuildCategoryNominationMenu BuildRandomNominationList; do
    sed -n "/^\(Menu\|void\) $function(/,/^}/p" "$root/nativevotes_nominations.sp"
done > "$work/catalog_nomination_under_test.inc"
sed -n '/^int GetMapEvalStartingVotes(/,/^}/p' "$root/nativevotes_mapchooser.sp" > "$work/catalog_bonus_under_test.inc"
"$scripting/spcomp" "$root/tests/$probe" -i "$root" -i "$work" -i "$scripting/include" -o "$output"
