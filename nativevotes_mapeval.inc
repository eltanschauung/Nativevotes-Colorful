#if defined _nativevotes_mapeval_included
    #endinput
#endif
#define _nativevotes_mapeval_included

#define MAP_EVAL_CONFIG_FILE "configs/mapeval.cfg"
#define MAP_EVAL_DEFAULT_VOTE_WEIGHT 1
#define MAP_EVAL_MAX_VOTE_WEIGHT 3
#define MAP_EVAL_POPULAR "popular maps"

ArrayList g_CatalogMaps, g_CatalogCategories, g_CatalogPopular;
StringMap g_CatalogGroups, g_MapEvalVoteWeights;
GlobalForward g_CatalogReloaded;
bool g_CatalogPrimary, g_CatalogLoaded, g_CatalogAttempted;

void MapEval_Init()
{
    g_CatalogMaps = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogCategories = new ArrayList(ByteCountToCells(64));
    g_CatalogPopular = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogGroups = new StringMap();
    g_MapEvalVoteWeights = new StringMap();
    g_CatalogReloaded = new GlobalForward("NativeVotes_OnMapCatalogReloaded", ET_Ignore);
}

void MapEval_RegisterNatives()
{
    CreateNative("NativeVotes_ReloadMapCatalog", Catalog_NativeReload);
    CreateNative("NativeVotes_CopyMapCatalog", Catalog_NativeCopyMaps);
    CreateNative("NativeVotes_CopyMapCategories", Catalog_NativeCopyCategories);
    CreateNative("NativeVotes_GetMapCatalogSource", Catalog_NativeSource);
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
    if (!map[0] || FindMap(map, resolved, sizeof(resolved)) == FindMap_NotFound)
        return true; // Preserve uninstalled config entries on disk, not in menus.
    int previous;
    if (g_MapEvalVoteWeights.GetValue(resolved, previous))
        return true; // Resolve map/workshop aliases to one entry; first rating wins.
    g_MapEvalVoteWeights.SetValue(resolved, ParseMapEvalVoteWeight(rawLevel));
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
    KeyValues kv = new KeyValues("mapEval");
    if (!kv.ImportFromFile(path))
    {
        delete kv;
        return false;
    }
    char rootName[64];
    kv.GetSectionName(rootName, sizeof(rootName));
    if (!StrEqual(rootName, "mapEval", false) || !kv.GotoFirstSubKey())
    {
        delete kv;
        return false;
    }
    bool valid = true;
    do
    {
        char category[64];
        kv.GetSectionName(category, sizeof(category));
        if (StrEqual(category, MAP_EVAL_POPULAR, false))
        {
            if (kv.GotoFirstSubKey(false))
            {
                do
                {
                    char map[PLATFORM_MAX_PATH], resolved[PLATFORM_MAX_PATH];
                    kv.GetSectionName(map, sizeof(map));
                    if (FindMap(map, resolved, sizeof(resolved)) != FindMap_NotFound
                        && g_CatalogPopular.FindString(resolved) == -1)
                        g_CatalogPopular.PushString(resolved);
                } while (kv.GotoNextKey(false));
                kv.GoBack();
            }
            continue;
        }
        int duplicate;
        if (!category[0] || g_CatalogGroups.GetValue(category, duplicate))
        {
            valid = false;
            break;
        }
        ArrayList maps = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
        g_CatalogGroups.SetValue(category, view_as<int>(maps));
        g_CatalogCategories.PushString(category);
        if (kv.GotoFirstSubKey(false))
        {
            do
            {
                char map[PLATFORM_MAX_PATH], rawLevel[32];
                kv.GetSectionName(map, sizeof(map));
                if (kv.GotoFirstSubKey(false))
                {
                    kv.GoBack();
                    valid = false;
                    break;
                }
                kv.GetString(NULL_STRING, rawLevel, sizeof(rawLevel), "");
                if (!Catalog_AddMap(map, maps, rawLevel))
                {
                    valid = false;
                    break;
                }
            } while (kv.GotoNextKey(false));
            kv.GoBack();
        }
        if (!valid) break;
    } while (kv.GotoNextKey());
    delete kv;
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
            && !RecentMapHistory_Contains(map) && IsMapValid(map))
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
