# Unified MapEval catalog

MapChooser owns the catalog. Nominations obtains snapshots through
`nativevotes_catalog_api.inc`; it never separately reads the configuration.
`sm_refresh_mapcycle` / `sm_reload_nominations` refresh the same catalog, menus,
levels, Popular view, and the next automatic vote pool without restarting.

`configs/mapeval.cfg` is primary. Missing, invalid, nested legacy-tier, or empty
primary configuration falls back to the current `mapcyclefile` (normally
`cfg/mapcycle.txt`). Fallback does not scan all installed maps. If neither source
contains installed maps, no automatic map can be selected.

## Levels and nomination menus

| Config value | Nomination | Automatic vote / empty server | Starting votes |
| --- | --- | --- | --- |
| empty / 0 | yes | no | 0 |
| 1 | yes | yes | 0 |
| 2 | yes | yes | +1 |
| 3 | yes | yes | +2 |

Values above 3 clamp to 3; malformed and negative values behave as 0. Existing
negative values remain written verbatim, but the obsolete -1 vote-demotion rule
is gone. During mapcycle fallback, all cycle entries have level-1 eligibility
and no bonus. Current/recent-map exclusions, nomination ownership, and client
vote weights remain independent of MapEval levels. Higher levels do not gain
extra probability in random selection.

The root title is `Nominate a map`. The menu starts with Popular and Random 50, followed by the
nonempty gamemode groups in configuration order. All menus paginate; category
submenus offer Back. The existing `!nr` and Random 50 invoke the same function.
These are nomination browsers, so they can show all levels; automatic vote and
empty-server selection are the paths restricted to level >= 1.

Category names retain the spelling in the file, including case-only edits on
refresh. The runtime parser is SourceMod SMC, not Valve KeyValues: Valve's
case-insensitive key-name symbol pool can return the first spelling seen by the
process rather than the latest configuration text. No hardcoded capitalization
or title-casing overrides are applied.

`popular maps` is an ordered view, not a gamemode or a second source of ratings.
Use valid KeyValues entries such as `"cp_example" ""`; bare single strings are
not valid scalar KeyValues entries. Missing/uninstalled maps are preserved on
disk but excluded from runtime menus. Map/workshop aliases resolve to one entry
with the first configured rating winning.

## Generating the static catalog

`tools/update_mapeval.py` imports cycle entries with empty values and preserves
all existing map values. It classifies BSP gameplay entities (including raw
Valve LZMA entity lumps), rather than treating every cp_ map as symmetric CP.
Cart-linked capture triggers distinguish Payload from Snowplow; asymmetric CP
win restrictions distinguish A/D; Sulfur's KOTH helper does not make it KOTH.
Known family fallback is used only where mode entities are unavailable.

The analytics input is a JSON export with `map_name`, `player_hours`,
`avg_players`, and `sessions`, using the same qualifying-session calculation as
`KogasaFrontend.MapsDb.fetch_map_detail_rows`:

```sql
SELECT map_name, COUNT(*) AS sessions,
       ROUND(AVG(avg_players), 2) AS avg_players,
       ROUND(SUM(player_seconds) / 3600, 1) AS player_hours
FROM map_statistics_sessions
WHERE peak_players >= 8 AND duration >= 600
  AND end_reason IN ('map_end', 'historical', 'synthetic_backfill')
GROUP BY map_name;
```

Unlike the page's display limit, export all qualifying maps before ranking.
Payload player-hours are multiplied by 0.70 before selecting 50 unique maps.
Ties use average players, session count, then map name. No popularity/database
query runs during nominations. The committed snapshot was generated 2026-10-22.

```bash
python3 tools/update_mapeval.py \
  --existing /path/to/current/mapeval.cfg --mapcycle /path/to/mapcycle.txt \
  --maps /path/to/tf/maps --analytics /path/to/analytics.json \
  --output configs/mapeval.cfg --report /tmp/mapeval-report.json
```

The retired `sm_mapvote_mapeval_random` bypass no longer overrides level rules.
The old `empty_training_maps.cfg` and `sm_emptymaps_file` setting are not used;
NativeVotes empty-server changes use the rated shared catalog. The MGE server's
existing `sm_nativevotes_emptymapchange 0` remains disabled.

## Tests

```bash
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash tests/build_catalog_probe.sh
bash tests/build_catalog_probe.sh "$HOME/hlserver/tf2/tf/addons/sourcemod/scripting" \
  /tmp/nativevotes_catalog_live_probe.smx catalog_live_probe.sp
```

Load the isolated probe on MGE and run `sm_nativevotes_catalog_probe`. Its
private fixture files test missing/invalid primary fallback, zero-only pools,
ratings/bonuses, shared snapshots, menu pagination, and production Random 50.
It never modifies production configuration or changes maps. Expected:
`checks=197 failures=0`. The malformed-file case uses a private fixture, not the
live file. The case regressions reparse lowercase, uppercase, and mixed-case
category names within one process.

After deployment, `sm_nativevotes_catalog_live_probe` verifies the actual shared
natives and every category/Popular menu using real SourceMod Menu objects,
including pagination and existing `!nr` selection. It does not open menus on
humans, cast votes, or change maps. Human-visible menu rendering still warrants
a normal `!nominate` check. Unload both temporary probe SMXs afterward.
