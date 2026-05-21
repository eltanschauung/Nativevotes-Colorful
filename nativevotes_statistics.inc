#if defined _nativevotes_statistics_included
  #endinput
#endif
#define _nativevotes_statistics_included

#define NATIVEVOTES_STATISTICS_TABLE "nativevotes_statistics_events"

Database g_NativeVoteStatsDb = null;
ConVar g_NativeVoteStatsEnabled = null;
int g_NativeVoteStatsVoteSequence = 0;
char g_NativeVoteStatsVoteId[96];

stock void NativeVoteStats_Init()
{
    g_NativeVoteStatsEnabled = FindConVar("sm_nativevotes_statistics_logging");
    if (g_NativeVoteStatsEnabled == null)
    {
        g_NativeVoteStatsEnabled = CreateConVar(
            "sm_nativevotes_statistics_logging",
            "1",
            "Write native vote behavior events to nativevotes_statistics_events.",
            _,
            true,
            0.0,
            true,
            1.0);
    }

    NativeVoteStats_Connect();
}

stock void NativeVoteStats_Connect()
{
    if (g_NativeVoteStatsDb != null)
    {
        return;
    }

    SQL_TConnect(NativeVoteStats_OnConnect, "default");
}

public void NativeVoteStats_OnConnect(Handle owner, Handle hndl, const char[] error, any data)
{
    if (hndl == null)
    {
        if (error[0])
        {
            LogError("[NativeVotes Statistics] Database connect failed: %s", error);
        }
        return;
    }

    g_NativeVoteStatsDb = view_as<Database>(hndl);
    NativeVoteStats_EnsureSchema();
}

stock void NativeVoteStats_EnsureSchema()
{
    if (g_NativeVoteStatsDb == null)
    {
        return;
    }

    char query[2048];
    FormatEx(query, sizeof(query),
        "CREATE TABLE IF NOT EXISTS " ... NATIVEVOTES_STATISTICS_TABLE ... " ("
        ... "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
        ... "created_at INT NOT NULL,"
        ... "host_port INT NOT NULL DEFAULT 0,"
        ... "map_session_id VARCHAR(64) NOT NULL DEFAULT '',"
        ... "current_map VARCHAR(128) NOT NULL DEFAULT '',"
        ... "map_name VARCHAR(128) NOT NULL DEFAULT '',"
        ... "event_type VARCHAR(32) NOT NULL,"
        ... "client_steamid64 VARCHAR(32) NOT NULL DEFAULT '',"
        ... "client_name VARCHAR(128) NOT NULL DEFAULT '',"
        ... "map_elapsed_seconds INT NOT NULL DEFAULT 0,"
        ... "minute_bucket INT NOT NULL DEFAULT 0,"
        ... "vote_id VARCHAR(96) NOT NULL DEFAULT '',"
        ... "option_index INT NOT NULL DEFAULT -1,"
        ... "votes INT NOT NULL DEFAULT 0,"
        ... "total_votes INT NOT NULL DEFAULT 0,"
        ... "eligible_clients INT NOT NULL DEFAULT 0,"
        ... "details VARCHAR(255) NOT NULL DEFAULT '',"
        ... "KEY idx_type_created (event_type, created_at),"
        ... "KEY idx_map_type_created (map_name, event_type, created_at),"
        ... "KEY idx_current_map_created (current_map, created_at),"
        ... "KEY idx_vote_id (vote_id)"
        ... ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    SQL_TQuery(g_NativeVoteStatsDb, NativeVoteStats_OnSchemaComplete, query);
}

public void NativeVoteStats_OnSchemaComplete(Database db, DBResultSet results, const char[] error, any data)
{
    if (error[0])
    {
        LogError("[NativeVotes Statistics] Schema failed: %s", error);
    }
}

stock void NativeVoteStats_BeginVote(const char[] details)
{
    char sessionId[64];
    NativeVoteStats_GetSessionId(sessionId, sizeof(sessionId));

    g_NativeVoteStatsVoteSequence++;
    Format(g_NativeVoteStatsVoteId, sizeof(g_NativeVoteStatsVoteId), "%s-%d", sessionId, g_NativeVoteStatsVoteSequence);

    NativeVoteStats_LogEvent("vote_started", "", 0, -1, 0, 0, NativeVoteStats_CountHumanClients(), details);
}

stock void NativeVoteStats_LogEvent(
    const char[] eventType,
    const char[] mapName,
    int client,
    int optionIndex,
    int votes,
    int totalVotes,
    int eligibleClients,
    const char[] details)
{
    if (g_NativeVoteStatsEnabled != null && !g_NativeVoteStatsEnabled.BoolValue)
    {
        return;
    }

    if (g_NativeVoteStatsDb == null)
    {
        NativeVoteStats_Connect();
        return;
    }

    int now = GetTime();
    int mapElapsed = RoundToFloor(GetGameTime());
    if (mapElapsed < 0)
    {
        mapElapsed = 0;
    }

    char currentMap[PLATFORM_MAX_PATH];
    GetCurrentMap(currentMap, sizeof(currentMap));

    char targetMap[PLATFORM_MAX_PATH];
    if (mapName[0])
    {
        strcopy(targetMap, sizeof(targetMap), mapName);
    }
    else
    {
        strcopy(targetMap, sizeof(targetMap), currentMap);
    }

    char sessionId[64];
    NativeVoteStats_GetSessionId(sessionId, sizeof(sessionId));

    char steamid[32];
    char clientName[MAX_NAME_LENGTH];
    steamid[0] = '\0';
    clientName[0] = '\0';

    if (client > 0 && client <= MaxClients && IsClientConnected(client))
    {
        GetClientAuthId(client, AuthId_SteamID64, steamid, sizeof(steamid), true);
        GetClientName(client, clientName, sizeof(clientName));
    }

    char escapedEvent[64];
    char escapedSession[128];
    char escapedCurrentMap[256];
    char escapedTargetMap[256];
    char escapedSteamId[64];
    char escapedClientName[256];
    char escapedVoteId[192];
    char escapedDetails[512];
    SQL_EscapeString(g_NativeVoteStatsDb, eventType, escapedEvent, sizeof(escapedEvent));
    SQL_EscapeString(g_NativeVoteStatsDb, sessionId, escapedSession, sizeof(escapedSession));
    SQL_EscapeString(g_NativeVoteStatsDb, currentMap, escapedCurrentMap, sizeof(escapedCurrentMap));
    SQL_EscapeString(g_NativeVoteStatsDb, targetMap, escapedTargetMap, sizeof(escapedTargetMap));
    SQL_EscapeString(g_NativeVoteStatsDb, steamid, escapedSteamId, sizeof(escapedSteamId));
    SQL_EscapeString(g_NativeVoteStatsDb, clientName, escapedClientName, sizeof(escapedClientName));
    SQL_EscapeString(g_NativeVoteStatsDb, g_NativeVoteStatsVoteId, escapedVoteId, sizeof(escapedVoteId));
    SQL_EscapeString(g_NativeVoteStatsDb, details, escapedDetails, sizeof(escapedDetails));

    char query[2048];
    FormatEx(query, sizeof(query),
        "INSERT INTO " ... NATIVEVOTES_STATISTICS_TABLE ... " "
        ... "(created_at, host_port, map_session_id, current_map, map_name, event_type, client_steamid64, client_name, map_elapsed_seconds, minute_bucket, vote_id, option_index, votes, total_votes, eligible_clients, details) "
        ... "VALUES (%d, %d, '%s', '%s', '%s', '%s', '%s', '%s', %d, %d, '%s', %d, %d, %d, %d, '%s')",
        now,
        NativeVoteStats_GetHostPort(),
        escapedSession,
        escapedCurrentMap,
        escapedTargetMap,
        escapedEvent,
        escapedSteamId,
        escapedClientName,
        mapElapsed,
        mapElapsed / 60,
        escapedVoteId,
        optionIndex,
        votes,
        totalVotes,
        eligibleClients,
        escapedDetails);
    SQL_TQuery(g_NativeVoteStatsDb, NativeVoteStats_OnWriteComplete, query);
}

public void NativeVoteStats_OnWriteComplete(Database db, DBResultSet results, const char[] error, any data)
{
    if (error[0])
    {
        LogError("[NativeVotes Statistics] Write failed: %s", error);
    }
}

stock void NativeVoteStats_GetSessionId(char[] buffer, int maxlen)
{
    int startedAt = GetTime() - RoundToFloor(GetGameTime());
    Format(buffer, maxlen, "%d-%d", NativeVoteStats_GetHostPort(), startedAt);
}

stock int NativeVoteStats_GetHostPort()
{
    ConVar hostport = FindConVar("hostport");
    if (hostport == null)
    {
        return 0;
    }

    return hostport.IntValue;
}

stock int NativeVoteStats_CountHumanClients()
{
    int count = 0;
    for (int client = 1; client <= MaxClients; client++)
    {
        if (IsClientConnected(client) && !IsFakeClient(client))
        {
            count++;
        }
    }
    return count;
}
