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

## Map catalog and nomination categories

`configs/mapeval.cfg` is the shared primary source for nominations, random votes,
and empty-server map changes. `mapcycle.txt` remains the missing/invalid-file
fallback. `!nominate` starts with Popular and Random 50, then paginated gamemode
categories; `!nr` uses the same Random 50 browser. `sm_refresh_mapcycle` refreshes
the entire shared catalog without live popularity queries.

Unrated/level-0 maps can be nominated; levels 1–3 additionally allow automatic
random/empty-server selection. Level 2 adds one starting vote and level 3 adds
two. See [catalog migration, generation, and tests](tests/catalog.md).
