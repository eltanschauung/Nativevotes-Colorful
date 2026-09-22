#if defined _nativevotes_map_history_included
    #endinput
#endif
#define _nativevotes_map_history_included

#define MAP_HISTORY_DATA_DIR "data/nativevotes"
#define MAP_HISTORY_FILE_FORMAT "data/nativevotes/map_history_%d.txt"

char g_MapHistoryPath[PLATFORM_MAX_PATH];

void RecentMapHistory_Init()
{
    char directory[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, directory, sizeof(directory), MAP_HISTORY_DATA_DIR);
    if (!DirExists(directory) && !CreateDirectory(directory, 493))
    {
        LogError("[NativeVotes MapChooser] Unable to create map-history directory: %s", directory);
        return;
    }

    int port = 0;
    ConVar hostPort = FindConVar("hostport");
    if (hostPort != null)
    {
        port = hostPort.IntValue;
    }

    BuildPath(Path_SM, g_MapHistoryPath, sizeof(g_MapHistoryPath), MAP_HISTORY_FILE_FORMAT, port);
    RecentMapHistory_Load();
}

void RecentMapHistory_Load()
{
    g_OldMapList.Clear();

    if (!g_MapHistoryPath[0] || !FileExists(g_MapHistoryPath))
    {
        return;
    }

    KeyValues history = new KeyValues("NativeVotesMapHistory");
    if (!history.ImportFromFile(g_MapHistoryPath))
    {
        LogError("[NativeVotes MapChooser] Unable to parse map history: %s", g_MapHistoryPath);
        delete history;
        return;
    }

    if (!history.JumpToKey("maps") || !history.GotoFirstSubKey(false))
    {
        delete history;
        return;
    }

    char map[PLATFORM_MAX_PATH];
    char resolvedMap[PLATFORM_MAX_PATH];
    do
    {
        history.GetString(NULL_STRING, map, sizeof(map));
        TrimString(map);
        if (!map[0])
        {
            continue;
        }

        if (FindMap(map, resolvedMap, sizeof(resolvedMap)) == FindMap_NotFound)
        {
            strcopy(resolvedMap, sizeof(resolvedMap), map);
        }

        if (g_OldMapList.FindString(resolvedMap) == -1)
        {
            g_OldMapList.PushString(resolvedMap);
        }
    }
    while (history.GotoNextKey(false));

    delete history;
    RecentMapHistory_Trim();
}

void RecentMapHistory_Save()
{
    if (!g_MapHistoryPath[0])
    {
        return;
    }

    KeyValues history = new KeyValues("NativeVotesMapHistory");
    history.JumpToKey("maps", true);

    char key[16];
    char map[PLATFORM_MAX_PATH];
    for (int i = 0; i < g_OldMapList.Length; i++)
    {
        IntToString(i, key, sizeof(key));
        g_OldMapList.GetString(i, map, sizeof(map));
        history.SetString(key, map);
    }

    history.Rewind();
    if (!history.ExportToFile(g_MapHistoryPath))
    {
        LogError("[NativeVotes MapChooser] Unable to save map history: %s", g_MapHistoryPath);
    }
    delete history;
}

void RecentMapHistory_Trim()
{
    int limit = g_ConVars[mapvote_exclude].IntValue;
    if (limit < 0)
    {
        limit = 0;
    }

    while (g_OldMapList.Length > limit)
    {
        g_OldMapList.Erase(0);
    }
}

void RecentMapHistory_RecordCurrentMap()
{
    char map[PLATFORM_MAX_PATH];
    GetCurrentMap(map, sizeof(map));
    if (!map[0])
    {
        return;
    }

    int existing = g_OldMapList.FindString(map);
    if (existing != -1)
    {
        g_OldMapList.Erase(existing);
    }
    g_OldMapList.PushString(map);
    RecentMapHistory_Trim();
    RecentMapHistory_Save();
}

bool RecentMapHistory_Contains(const char[] map)
{
    if (g_ConVars[mapvote_exclude].IntValue <= 0 || !map[0])
    {
        return false;
    }

    char resolvedMap[PLATFORM_MAX_PATH];
    if (FindMap(map, resolvedMap, sizeof(resolvedMap)) == FindMap_NotFound)
    {
        strcopy(resolvedMap, sizeof(resolvedMap), map);
    }

    return g_OldMapList.FindString(resolvedMap) != -1;
}

public void ConVarChanged_MapVoteExclude(ConVar convar, const char[] oldValue, const char[] newValue)
{
    RecentMapHistory_Trim();
    RecentMapHistory_Save();

    if (g_MapList != null && g_MapList.Length > 0 && !g_HasVoteStarted && !g_WaitingForVote)
    {
        CreateNextVote();
    }
}
