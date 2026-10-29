# Nativevotes-Colorful

This is a heavily modified version of Nativevotes that accomplishes the following:
- Adds CPrintToChat support for visual flair
- Adds supplementary commands such as !nr (nominate random), nominate aliases such as !n, !unrtv and !unnominate features with aliases
- Adds a system called mapeval, which gives selected maps a configurable number of starting votes for quality control in voting
- Mapeval is also where this plugin will take maps from when a random vote is made, rather than random votes being filled by the selected mapcycle
- Adds a vote whitelist and blacklist system, vote whitelisted clients gain additional weight in their votes and rtv use, while the blacklist system (TBC) will subtract voting power.
- Adds data collection features for analysis of vote trends
- Adds optional playtime gating for rockthevote and nominations, playtime is queried from [WhaleTracker](https://github.com/eltanschauung/Whaletracker)
- Shows raw participation and weighted map totals separately in the optional progress hint, including whitelist and mapeval bonuses

MapChooser supplies weighted progress through the optional
`NativeVotes_OnBuildProgress` forward declared in
`nativevotes_progress.inc`. Other vote owners can leave the forward
unimplemented and NativeVotes will continue displaying raw counts.

Credit to Powerlord and all related parties: https://github.com/Heapons/sourcemod-nativevotes-updated/

See [kogasa.tf](https://kogasa.tf)

## Next-map ownership

Map Nominations owns the `nextmap` console command, `!nextmap` and `/nextmap`
chat triggers, and the exact bare `nextmap` alias in both public/team chat.
The bare alias uses a `say` listener rather than the chat post-forward, while
Filters remains responsible for rendering the original message. Gagged clients
cannot use the bare alias. `sm_trigger_show` retains its private/public behavior
for bare-chat replies. Pending votes are reported instead of a premature map.

`sm_nextmap` remains SourceMod's core ConVar, not a second registered command.
Deploy the companion Kogasatopia `basetriggers.sp` without its old next-map
handlers. Activate both together (BaseTriggers first, then Map Nominations), or
restart the server, so the old provider cannot duplicate replies.

## Map catalog and nomination categories

`configs/mapeval.cfg` is the shared primary source for nominations, random votes,
and empty-server map changes. `mapcycle.txt` remains the missing/invalid-file
fallback. `!nominate` starts with Popular and Random 50, then paginated gamemode
categories; `!nr` uses the same Random 50 browser. `sm_refresh_mapcycle` refreshes
the entire shared catalog without live popularity queries.

Unrated/level-0 maps can be nominated; levels 1–3 additionally allow automatic
random/empty-server selection. Level 2 adds one starting vote and level 3 adds
two. See [catalog migration, generation, and tests](tests/catalog.md).
