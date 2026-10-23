#if defined _nativevotes_mapeval_included
    #endinput
#endif
#define _nativevotes_mapeval_included

#include <textparse>
#include "nativevotes_map_cache.inc"

#define MAP_EVAL_CONFIG_FILE "configs/mapeval.cfg"
#define MAP_EVAL_DEFAULT_VOTE_WEIGHT 1
#define MAP_EVAL_MAX_VOTE_WEIGHT 3
#define MAP_EVAL_POPULAR "popular maps"

ArrayList g_CatalogMaps, g_CatalogCategories, g_CatalogPopular;
StringMap g_CatalogGroups, g_MapEvalVoteWeights;
StringMap g_CatalogDisplayNames;
GlobalForward g_CatalogReloaded;
bool g_CatalogPrimary, g_CatalogLoaded, g_CatalogAttempted;
int g_CatalogParseDepth;
bool g_CatalogParseValid, g_CatalogSawRoot, g_CatalogSawPopular, g_CatalogParsingPopular;
ArrayList g_CatalogParsingGroup;

void MapEval_Init()
{
    g_CatalogMaps = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogCategories = new ArrayList(ByteCountToCells(64));
    g_CatalogPopular = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogGroups = new StringMap();
    g_MapEvalVoteWeights = new StringMap();
    g_CatalogDisplayNames = new StringMap();
    Catalog_ResetMapValidationCache();
    g_CatalogReloaded = new GlobalForward("NativeVotes_OnMapCatalogReloaded", ET_Ignore);
}

void MapEval_RegisterNatives()
{
    CreateNative("NativeVotes_ReloadMapCatalog", Catalog_NativeReload);
    CreateNative("NativeVotes_CopyMapCatalog", Catalog_NativeCopyMaps);
    CreateNative("NativeVotes_CopyMapCategories", Catalog_NativeCopyCategories);
    CreateNative("NativeVotes_GetMapCatalogSource", Catalog_NativeSource);
    CreateNative("NativeVotes_GetCatalogMapDisplayName", Catalog_NativeDisplayName);
}

void Catalog_Clear()
{
    char category[64];
    for (int i = 0; i < g_CatalogCategories.Length; i++)
    {
        g_CatalogCategories.GetString(i, category, sizeof(category));
        int value;
        if (g_CatalogGroups.GetValue(category, value))
        {
            ArrayList maps = view_as<ArrayList>(value);
            delete maps;
        }
    }
    g_CatalogMaps.Clear();
    g_CatalogCategories.Clear();
    g_CatalogPopular.Clear();
    g_CatalogGroups.Clear();
    g_MapEvalVoteWeights.Clear();
    if (g_CatalogDisplayNames != null) g_CatalogDisplayNames.Clear();
}

int ParseMapEvalVoteWeight(const char[] value)
{
    char trimmed[32];
    strcopy(trimmed, sizeof(trimmed), value);
    TrimString(trimmed);
    int level;
    if (!trimmed[0] || StringToIntEx(trimmed, level) != strlen(trimmed) || level < 0)
        return 0;
    return level > 3 ? 3 : level;
}

int GetMapEvalLevel(const char[] map)
{
    int level;
    g_MapEvalVoteWeights.GetValue(map, level);
    return level;
}

int GetMapEvalVoteWeight(const char[] map)
{
    // Vote callers expect baseline 1, whereas catalog levels have baseline 0.
    int level = GetMapEvalLevel(map);
    return level > 1 ? level : 1;
}

bool Catalog_AddMap(const char[] map, ArrayList category, const char[] rawLevel)
{
    char resolved[PLATFORM_MAX_PATH];
    if (!map[0] || !Catalog_ResolveInstalledMap(map, resolved, sizeof(resolved)))
        return true; // Preserve uninstalled config entries on disk, not in menus.
    int previous;
    if (g_MapEvalVoteWeights.GetValue(resolved, previous))
        return true; // Resolve map/workshop aliases to one entry; first rating wins.
    g_MapEvalVoteWeights.SetValue(resolved, ParseMapEvalVoteWeight(rawLevel));
    if (g_CatalogDisplayNames == null) g_CatalogDisplayNames = new StringMap();
    char display[PLATFORM_MAX_PATH];
    Catalog_FormatResolvedDisplayName(resolved, display, sizeof(display));
    g_CatalogDisplayNames.SetString(resolved, display);
    g_CatalogMaps.PushString(resolved);
    category.PushString(resolved);
    return true;
}

bool Catalog_LoadPrimary()
{
    char path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, path, sizeof(path), MAP_EVAL_CONFIG_FILE);
    if (!FileExists(path))
        return false;

    // Valve KeyValues interns key names case-insensitively and may return the
    // spelling first seen during this process. SMC reports the original text,
    // so a case-only configuration edit takes effect without a server restart.
    g_CatalogParseDepth = 0;
    g_CatalogParseValid = true;
    g_CatalogSawRoot = false;
    g_CatalogSawPopular = false;
    g_CatalogParsingPopular = false;
    g_CatalogParsingGroup = null;
    SMCParser parser = new SMCParser();
    parser.OnEnterSection = Catalog_ParseSection;
    parser.OnKeyValue = Catalog_ParseEntry;
    parser.OnLeaveSection = Catalog_ParseLeave;
    SMCError error = parser.ParseFile(path);
    if (error != SMCError_Okay)
    {
        char message[192];
        parser.GetErrorString(error, message, sizeof(message));
        LogMessage("[MapCatalog] Unable to parse %s: %s", MAP_EVAL_CONFIG_FILE, message);
    }
    delete parser;
    bool valid = error == SMCError_Okay && g_CatalogParseValid
        && g_CatalogSawRoot && g_CatalogParseDepth == 0;

    // Popular is an ordered view of the same catalog, never a second map source.
    char map[PLATFORM_MAX_PATH];
    for (int i = g_CatalogPopular.Length - 1; i >= 0; i--)
    {
        g_CatalogPopular.GetString(i, map, sizeof(map));
        int level;
        if (!g_MapEvalVoteWeights.GetValue(map, level))
            g_CatalogPopular.Erase(i);
    }
    return valid && g_CatalogMaps.Length > 0;
}

public SMCResult Catalog_ParseSection(SMCParser parser, const char[] section, bool quoted)
{
    if (g_CatalogParseDepth == 0)
    {
        if (g_CatalogSawRoot || !StrEqual(section, "mapEval", false))
        {
            g_CatalogParseValid = false;
            return SMCParse_HaltFail;
        }
        g_CatalogSawRoot = true;
    }
    else if (g_CatalogParseDepth == 1)
    {
        if (!section[0] || strlen(section) >= 64)
        {
            g_CatalogParseValid = false;
            return SMCParse_HaltFail;
        }
        g_CatalogParsingPopular = StrEqual(section, MAP_EVAL_POPULAR, false);
        if (g_CatalogParsingPopular)
        {
            if (g_CatalogSawPopular)
            {
                g_CatalogParseValid = false;
                return SMCParse_HaltFail;
            }
            g_CatalogSawPopular = true;
        }
        else
        {
            char previous[64];
            for (int i = 0; i < g_CatalogCategories.Length; i++)
            {
                g_CatalogCategories.GetString(i, previous, sizeof(previous));
                if (StrEqual(previous, section, false))
                {
                    g_CatalogParseValid = false;
                    return SMCParse_HaltFail;
                }
            }
            g_CatalogParsingGroup = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
            g_CatalogGroups.SetValue(section, view_as<int>(g_CatalogParsingGroup));
            g_CatalogCategories.PushString(section);
        }
    }
    else
    {
        g_CatalogParseValid = false;
        return SMCParse_HaltFail;
    }
    g_CatalogParseDepth++;
    return SMCParse_Continue;
}

public SMCResult Catalog_ParseEntry(SMCParser parser, const char[] map, const char[] value,
    bool keyQuoted, bool valueQuoted)
{
    if (g_CatalogParseDepth != 2 || !map[0] || strlen(map) >= PLATFORM_MAX_PATH)
    {
        g_CatalogParseValid = false;
        return SMCParse_HaltFail;
    }
    if (g_CatalogParsingPopular)
    {
        char resolved[PLATFORM_MAX_PATH];
        if (Catalog_ResolveInstalledMap(map, resolved, sizeof(resolved))
            && g_CatalogPopular.FindString(resolved) == -1)
            g_CatalogPopular.PushString(resolved);
    }
    else if (!Catalog_AddMap(map, g_CatalogParsingGroup, value))
    {
        g_CatalogParseValid = false;
        return SMCParse_HaltFail;
    }
    return SMCParse_Continue;
}

public SMCResult Catalog_ParseLeave(SMCParser parser)
{
    g_CatalogParseDepth--;
    if (g_CatalogParseDepth == 1)
    {
        g_CatalogParsingGroup = null;
        g_CatalogParsingPopular = false;
    }
    return SMCParse_Continue;
}

bool Catalog_LoadFallback()
{
    ConVar cycle = FindConVar("mapcyclefile");
    char path[PLATFORM_MAX_PATH];
    if (cycle == null) return false;
    cycle.GetString(path, sizeof(path));
    if (StrContains(path, "cfg/") != 0)
        Format(path, sizeof(path), "cfg/%s", path);
    File file = OpenFile(path, "r");
    if (file == null) return false;
    ArrayList maps = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogCategories.PushString("mapcycle");
    g_CatalogGroups.SetValue("mapcycle", view_as<int>(maps));
    char map[PLATFORM_MAX_PATH];
    while (file.ReadLine(map, sizeof(map)))
    {
        int comment = StrContains(map, "//");
        if (comment >= 0) map[comment] = '\0';
        TrimString(map);
        if (!map[0]) continue;
        // Fallback has no ratings: all valid cycle entries retain legacy random
        // eligibility. Duplicate mapcycle lines are harmless and ignored.
        Catalog_AddMap(map, maps, "1");
    }
    delete file;
    return g_CatalogMaps.Length > 0;
}

bool LoadMapEvalConfig()
{
    g_CatalogAttempted = true;
    Catalog_Clear();
    g_CatalogPrimary = Catalog_LoadPrimary();
    bool ok = g_CatalogPrimary;
    if (!ok)
    {
        Catalog_Clear();
        ok = Catalog_LoadFallback();
        LogMessage("[MapCatalog] mapeval.cfg missing/invalid/empty; mapcycle fallback: %d maps", g_CatalogMaps.Length);
    }
    g_CatalogLoaded = ok;
    g_MapList.Clear();
    char map[PLATFORM_MAX_PATH];
    for (int i = 0; i < g_CatalogMaps.Length; i++)
    {
        g_CatalogMaps.GetString(i, map, sizeof(map));
        g_MapList.PushString(map);
    }
    // Do not clear the starting-vote cache of a vote already in progress.
    Call_StartForward(g_CatalogReloaded);
    Call_Finish();
    return ok;
}

void Catalog_BuildRandomPool(ArrayList pool)
{
    pool.Clear();
    UpdateCurrentMap();
    char map[PLATFORM_MAX_PATH];
    for (int i = 0; i < g_CatalogMaps.Length; i++)
    {
        g_CatalogMaps.GetString(i, map, sizeof(map));
        if (GetMapEvalLevel(map) >= 1 && !StrEqual(map, g_CurrentMap, false)
            && !RecentMapHistory_Contains(map))
            pool.PushString(map);
    }
}

bool CopyRandomMapEvalMap(char[] buffer, int maxlen)
{
    if (maxlen <= 0) return false;
    buffer[0] = '\0';
    ArrayList pool = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    Catalog_BuildRandomPool(pool);
    bool found = pool.Length > 0;
    if (found) pool.GetString(GetRandomInt(0, pool.Length - 1), buffer, maxlen);
    delete pool;
    return found;
}

bool PopulateNextVoteFromMapEval()
{
    ArrayList pool = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    Catalog_BuildRandomPool(pool);
    char map[PLATFORM_MAX_PATH];
    int limit = g_ConVars[mapvote_include].IntValue;
    while (pool.Length > 0 && g_NextMapList.Length < limit)
    {
        int index = GetRandomInt(0, pool.Length - 1);
        pool.GetString(index, map, sizeof(map));
        g_NextMapList.PushString(map);
        pool.Erase(index);
    }
    delete pool;
    // A primary file with no eligible maps must NEVER fall back to level 0.
    return g_CatalogLoaded;
}

bool Catalog_EnsureLoaded()
{
    return g_CatalogAttempted ? g_CatalogLoaded : LoadMapEvalConfig();
}

bool Catalog_GetMapDisplayName(const char[] map, char[] display, int maxlen)
{
    if (g_CatalogDisplayNames != null && g_CatalogDisplayNames.GetString(map, display, maxlen))
        return true;
    // Explicit external/admin maps still use the normal engine validation.
    return GetMapDisplayName(map, display, maxlen);
}

public int Catalog_NativeDisplayName(Handle plugin, int params)
{
    char map[PLATFORM_MAX_PATH], display[PLATFORM_MAX_PATH];
    GetNativeString(1, map, sizeof(map));
    bool found = g_CatalogDisplayNames != null
        && g_CatalogDisplayNames.GetString(map, display, sizeof(display));
    SetNativeString(2, found ? display : "", GetNativeCell(3));
    return found;
}

public int Catalog_NativeReload(Handle plugin, int params)
{
    bool loaded = LoadMapEvalConfig();
    if (!g_HasVoteStarted && !g_WaitingForVote) CreateNextVote();
    return loaded;
}

public int Catalog_NativeCopyMaps(Handle plugin, int params)
{
    if (!g_CatalogAttempted) LoadMapEvalConfig();
    ArrayList target = view_as<ArrayList>(GetNativeCell(1));
    char category[64];
    GetNativeString(2, category, sizeof(category));
    ArrayList source = g_CatalogMaps;
    if (StrEqual(category, MAP_EVAL_POPULAR, false)) source = g_CatalogPopular;
    else if (category[0])
    {
        int value;
        if (!g_CatalogGroups.GetValue(category, value)) { target.Clear(); return 0; }
        source = view_as<ArrayList>(value);
    }
    target.Clear();
    char map[PLATFORM_MAX_PATH];
    for (int i = 0; i < source.Length; i++)
    {
        source.GetString(i, map, sizeof(map));
        target.PushString(map);
    }
    return target.Length;
}

public int Catalog_NativeCopyCategories(Handle plugin, int params)
{
    if (!g_CatalogAttempted) LoadMapEvalConfig();
    ArrayList target = view_as<ArrayList>(GetNativeCell(1));
    target.Clear();
    char category[64];
    for (int i = 0; i < g_CatalogCategories.Length; i++)
    {
        g_CatalogCategories.GetString(i, category, sizeof(category));
        int value;
        if (g_CatalogGroups.GetValue(category, value) && view_as<ArrayList>(value).Length > 0)
            target.PushString(category);
    }
    return target.Length;
}

public int Catalog_NativeSource(Handle plugin, int params)
{
    SetNativeString(1, g_CatalogPrimary ? "mapeval.cfg" : "mapcycle.txt (fallback)", GetNativeCell(2));
    return 0;
}
