#if defined _nativevotes_map_history_included
    #endinput
#endif
#define _nativevotes_map_history_included

#define MAP_HISTORY_DATA_DIR "data/nativevotes"
#define MAP_HISTORY_FILE_FORMAT "data/nativevotes/map_history_%d.txt"
#define MAP_HISTORY_EMPTY_INVALIDATION_DELAY 30.0

char g_MapHistoryPath[PLATFORM_MAX_PATH];
int g_RecentMapHistoryValidCount;
Handle g_RecentMapHistoryEmptyTimer = null;

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
    g_RecentMapHistoryValidCount = 0;

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

    g_RecentMapHistoryValidCount = history.GetNum("valid_count", -1);

    if (!history.JumpToKey("maps") || !history.GotoFirstSubKey(false))
    {
        g_RecentMapHistoryValidCount = 0;
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

    if (g_RecentMapHistoryValidCount < 0)
    {
        // History files written before valid_count existed are fully valid.
        g_RecentMapHistoryValidCount = g_OldMapList.Length;
    }
    else if (g_RecentMapHistoryValidCount > g_OldMapList.Length)
    {
        g_RecentMapHistoryValidCount = g_OldMapList.Length;
    }

    RecentMapHistory_Trim();
}

void RecentMapHistory_Save()
{
    if (!g_MapHistoryPath[0])
    {
        return;
    }

    KeyValues history = new KeyValues("NativeVotesMapHistory");
    history.SetNum("valid_count", g_RecentMapHistoryValidCount);
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
        RecentMapHistory_Erase(0);
    }
}

void RecentMapHistory_Erase(int index)
{
    int validStart = g_OldMapList.Length - g_RecentMapHistoryValidCount;
    if (index >= validStart && g_RecentMapHistoryValidCount > 0)
    {
        g_RecentMapHistoryValidCount--;
    }

    g_OldMapList.Erase(index);
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
        RecentMapHistory_Erase(existing);
    }
    g_OldMapList.PushString(map);
    g_RecentMapHistoryValidCount++;
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

    int validStart = g_OldMapList.Length - g_RecentMapHistoryValidCount;
    for (int i = validStart; i < g_OldMapList.Length; i++)
    {
        char recentMap[PLATFORM_MAX_PATH];
        g_OldMapList.GetString(i, recentMap, sizeof(recentMap));
        if (StrEqual(recentMap, resolvedMap, false))
        {
            return true;
        }
    }

    return false;
}

void RecentMapHistory_CopyValid(ArrayList output)
{
    char map[PLATFORM_MAX_PATH];
    int validStart = g_OldMapList.Length - g_RecentMapHistoryValidCount;

    for (int i = validStart; i < g_OldMapList.Length; i++)
    {
        g_OldMapList.GetString(i, map, sizeof(map));
        output.PushString(map);
    }
}

void RecentMapHistory_ArmEmptyTimer()
{
    if (g_RecentMapHistoryEmptyTimer != null || GetClientCount(false) != 0)
    {
        return;
    }

    g_RecentMapHistoryEmptyTimer = CreateTimer(
        MAP_HISTORY_EMPTY_INVALIDATION_DELAY,
        Timer_RecentMapHistoryConfirmEmpty,
        _
    );
}

void RecentMapHistory_CancelEmptyTimer()
{
    if (g_RecentMapHistoryEmptyTimer != null)
    {
        delete g_RecentMapHistoryEmptyTimer;
        g_RecentMapHistoryEmptyTimer = null;
    }
}

public Action Timer_RecentMapHistoryConfirmEmpty(Handle timer)
{
    if (timer != g_RecentMapHistoryEmptyTimer)
    {
        return Plugin_Stop;
    }

    g_RecentMapHistoryEmptyTimer = null;

    if (GetClientCount(false) != 0)
    {
        return Plugin_Stop;
    }

    int invalidated = g_RecentMapHistoryValidCount;
    g_RecentMapHistoryValidCount = 0;
    RecentMapHistory_Save();

    if (g_MapList != null && g_MapList.Length > 0 && !g_HasVoteStarted && !g_WaitingForVote)
    {
        CreateNextVote();
    }

    LogMessage(
        "[NativeVotes MapChooser] Invalidated %d recent-map exclusions after the server remained empty for %.0f seconds.",
        invalidated,
        MAP_HISTORY_EMPTY_INVALIDATION_DELAY
    );

    return Plugin_Stop;
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
