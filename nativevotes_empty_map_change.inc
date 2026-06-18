#if defined _nativevotes_empty_map_change_included
	#endinput
#endif
#define _nativevotes_empty_map_change_included

Handle g_EmptyMapChangeTimer = null;

public void ConVarChanged_EmptyMapChange(ConVar convar, const char[] oldValue, const char[] newValue)
{
	RestartEmptyMapChangeTimer();
}

void StopEmptyMapChangeTimer()
{
	if (g_EmptyMapChangeTimer != null)
	{
		KillTimer(g_EmptyMapChangeTimer);
		g_EmptyMapChangeTimer = null;
	}
}

void RestartEmptyMapChangeTimer()
{
	StopEmptyMapChangeTimer();

	float minutes = g_ConVars[nativevotes_emptymapchange].FloatValue;
	if (minutes <= 0.0)
	{
		return;
	}

	g_EmptyMapChangeTimer = CreateTimer(minutes * 60.0, Timer_EmptyMapChange, _, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
	LogMessage("[NativeVotes MapChooser] Empty map change enabled; checking every %.1f minutes.", minutes);
}

int CountConnectedHumans()
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

public Action Timer_EmptyMapChange(Handle timer)
{
	if (timer != g_EmptyMapChangeTimer)
	{
		return Plugin_Stop;
	}

	if (g_ConVars[nativevotes_emptymapchange].FloatValue <= 0.0)
	{
		g_EmptyMapChangeTimer = null;
		return Plugin_Stop;
	}

	if (CountConnectedHumans() > 0)
	{
		return Plugin_Continue;
	}

	char nextMap[PLATFORM_MAX_PATH];
	if (!CopyRandomMapEvalMap(nextMap, sizeof(nextMap)))
	{
		LogError("[NativeVotes MapChooser] Empty map change skipped: no valid mapeval.cfg maps available.");
		return Plugin_Continue;
	}

	g_EmptyMapChangeTimer = null;
	LogMessage("[NativeVotes MapChooser] Server empty for configured interval; changing to %s.", nextMap);
	ForceChangeLevel(nextMap, "Server was empty for configured interval");
	return Plugin_Stop;
}
