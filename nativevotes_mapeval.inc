#if defined _nativevotes_mapeval_included
	#endinput
#endif
#define _nativevotes_mapeval_included

#define MAP_EVAL_CONFIG_FILE "configs/mapeval.cfg"
#define MAP_EVAL_DEFAULT_VOTE_WEIGHT 1
#define MAP_EVAL_MAX_VOTE_WEIGHT 3

StringMap g_MapEvalGamemodes;
StringMap g_MapEvalVoteWeights;

void MapEval_Init()
{
	g_MapEvalGamemodes = new StringMap();
	g_MapEvalVoteWeights = new StringMap();
}

int ParseMapEvalVoteWeight(const char[] value)
{
	char trimmed[32];
	strcopy(trimmed, sizeof(trimmed), value);
	TrimString(trimmed);

	if (!trimmed[0])
	{
		return MAP_EVAL_DEFAULT_VOTE_WEIGHT;
	}

	int weight = StringToInt(trimmed);
	if (weight < MAP_EVAL_DEFAULT_VOTE_WEIGHT)
	{
		return MAP_EVAL_DEFAULT_VOTE_WEIGHT;
	}
	if (weight > MAP_EVAL_MAX_VOTE_WEIGHT)
	{
		return MAP_EVAL_MAX_VOTE_WEIGHT;
	}

	return weight;
}

int GetMapEvalVoteWeight(const char[] map)
{
	if (!map[0] || StrEqual(map, VOTE_EXTEND, false) || StrEqual(map, VOTE_DONTCHANGE, false))
	{
		return MAP_EVAL_DEFAULT_VOTE_WEIGHT;
	}

	if (g_MapEvalVoteWeights == null)
	{
		return MAP_EVAL_DEFAULT_VOTE_WEIGHT;
	}

	int weight;
	if (!g_MapEvalVoteWeights.GetValue(map, weight))
	{
		return MAP_EVAL_DEFAULT_VOTE_WEIGHT;
	}

	if (weight < MAP_EVAL_DEFAULT_VOTE_WEIGHT)
	{
		return MAP_EVAL_DEFAULT_VOTE_WEIGHT;
	}
	if (weight > MAP_EVAL_MAX_VOTE_WEIGHT)
	{
		return MAP_EVAL_MAX_VOTE_WEIGHT;
	}

	return weight;
}

void ClearMapEvalConfig()
{
	if (g_MapEvalVoteWeights != null)
	{
		g_MapEvalVoteWeights.Clear();
	}

	if (g_MapEvalGamemodes == null)
	{
		return;
	}

	StringMapSnapshot snapshot = g_MapEvalGamemodes.Snapshot();
	char gamemode[64];

	for (int i = 0; i < snapshot.Length; i++)
	{
		snapshot.GetKey(i, gamemode, sizeof(gamemode));

		int mapsValue;
		if (!g_MapEvalGamemodes.GetValue(gamemode, mapsValue))
		{
			continue;
		}

		ArrayList maps = view_as<ArrayList>(mapsValue);
		if (maps != null)
		{
			delete maps;
		}
	}

	delete snapshot;
	g_MapEvalGamemodes.Clear();
}

void LoadMapEvalConfig()
{
	if (g_MapEvalGamemodes == null)
	{
		g_MapEvalGamemodes = new StringMap();
	}
	if (g_MapEvalVoteWeights == null)
	{
		g_MapEvalVoteWeights = new StringMap();
	}

	ClearMapEvalConfig();

	char path[PLATFORM_MAX_PATH];
	BuildPath(Path_SM, path, sizeof(path), MAP_EVAL_CONFIG_FILE);

	if (!FileExists(path))
	{
		LogMessage("[NativeVotes MapChooser] mapeval config missing, falling back to map list: %s", path);
		return;
	}

	KeyValues kv = new KeyValues("mapEval");
	if (!kv.ImportFromFile(path))
	{
		LogError("[NativeVotes MapChooser] Failed to parse mapeval config: %s", path);
		delete kv;
		return;
	}

	if (!kv.GotoFirstSubKey())
	{
		delete kv;
		return;
	}

	do
	{
		char gamemode[64];
		kv.GetSectionName(gamemode, sizeof(gamemode));
		if (!gamemode[0])
		{
			continue;
		}

		ArrayList maps = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));

		if (kv.GotoFirstSubKey(false))
		{
			do
			{
				char mapName[PLATFORM_MAX_PATH];
				kv.GetSectionName(mapName, sizeof(mapName));
				if (!mapName[0])
				{
					continue;
				}

				// mapeval.cfg now stores maps directly under each gamemode.
				// Legacy tier1/tier2 sections are intentionally ignored.
				if (kv.GotoFirstSubKey(false))
				{
					kv.GoBack();
					continue;
				}

				char weightValue[32];
				kv.GetString(NULL_STRING, weightValue, sizeof(weightValue), "");
				int voteWeight = ParseMapEvalVoteWeight(weightValue);

				maps.PushString(mapName);
				g_MapEvalVoteWeights.SetValue(mapName, voteWeight);
			}
			while (kv.GotoNextKey(false));

			kv.GoBack();
		}

		if (maps.Length > 0)
		{
			g_MapEvalGamemodes.SetValue(gamemode, view_as<int>(maps));
		}
		else
		{
			delete maps;
		}
	}
	while (kv.GotoNextKey());

	delete kv;
}

bool CopyRandomMapEvalMap(char[] buffer, int maxlen)
{
	if (maxlen <= 0)
	{
		return false;
	}

	buffer[0] = '\0';

	if (g_MapEvalGamemodes == null || g_MapEvalGamemodes.Size == 0)
	{
		return false;
	}

	UpdateCurrentMap();

	ArrayList pool = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
	StringMap seen = new StringMap();
	StringMapSnapshot snapshot = g_MapEvalGamemodes.Snapshot();

	char gamemode[64];
	char mapName[PLATFORM_MAX_PATH];
	for (int i = 0; i < snapshot.Length; i++)
	{
		snapshot.GetKey(i, gamemode, sizeof(gamemode));

		int mapsValue;
		if (!g_MapEvalGamemodes.GetValue(gamemode, mapsValue))
		{
			continue;
		}

		ArrayList maps = view_as<ArrayList>(mapsValue);
		if (maps == null)
		{
			continue;
		}

		for (int mapIndex = 0; mapIndex < maps.Length; mapIndex++)
		{
			maps.GetString(mapIndex, mapName, sizeof(mapName));
			TrimString(mapName);

			if (!mapName[0]
				|| (g_CurrentMap[0] && StrEqual(mapName, g_CurrentMap, false))
				|| !IsMapValid(mapName))
			{
				continue;
			}

			int dummy;
			if (seen.GetValue(mapName, dummy))
			{
				continue;
			}

			seen.SetValue(mapName, 1);
			pool.PushString(mapName);
		}
	}

	delete snapshot;
	delete seen;

	bool found = false;
	if (pool.Length > 0)
	{
		pool.GetString(GetRandomInt(0, pool.Length - 1), buffer, maxlen);
		found = true;
	}

	delete pool;
	return found;
}

bool IsMapEvalExcludedByHistory(const char[] mapName)
{
	if (g_ConVars[mapvote_exclude].IntValue <= 0)
	{
		return false;
	}

	return (g_OldMapList.FindString(mapName) != -1);
}

void BuildMapEvalWeightPool(ArrayList maps, int requiredWeight, ArrayList pool, StringMap seen)
{
	if (maps == null || pool == null)
	{
		return;
	}

	char mapName[PLATFORM_MAX_PATH];
	for (int i = 0; i < maps.Length; i++)
	{
		maps.GetString(i, mapName, sizeof(mapName));
		TrimString(mapName);

		if (!mapName[0])
		{
			continue;
		}
		int voteWeight = GetMapEvalVoteWeight(mapName);
		if (voteWeight != requiredWeight)
		{
			continue;
		}
		if (g_CurrentMap[0] && StrEqual(mapName, g_CurrentMap, false))
		{
			continue;
		}
		if (IsMapEvalExcludedByHistory(mapName))
		{
			continue;
		}
		if (!IsMapValid(mapName))
		{
			continue;
		}
		if (seen != null)
		{
			int dummy;
			if (seen.GetValue(mapName, dummy))
			{
				continue;
			}
			seen.SetValue(mapName, 1);
		}

		pool.PushString(mapName);
	}
}

ArrayList FirstAvailableMapEvalPool(ArrayList first, ArrayList second, ArrayList third)
{
	if (first != null && first.Length > 0)
	{
		return first;
	}
	if (second != null && second.Length > 0)
	{
		return second;
	}
	if (third != null && third.Length > 0)
	{
		return third;
	}
	return null;
}

ArrayList SelectMapEvalWeightPool(ArrayList weight3Pool, ArrayList weight2Pool, ArrayList weight1Pool, bool preferHighWeight)
{
	if (preferHighWeight)
	{
		return FirstAvailableMapEvalPool(weight3Pool, weight2Pool, weight1Pool);
	}
	return FirstAvailableMapEvalPool(weight1Pool, weight2Pool, weight3Pool);
}

void AddMapEvalVoteGroup(const char[] gamemode, ArrayList groupNames, ArrayList weight3Pools, ArrayList weight2Pools, ArrayList weight1Pools, StringMap seen)
{
	if (!gamemode[0] || g_MapEvalGamemodes == null)
	{
		return;
	}

	int mapsValue;
	if (!g_MapEvalGamemodes.GetValue(gamemode, mapsValue))
	{
		return;
	}

	ArrayList maps = view_as<ArrayList>(mapsValue);
	if (maps == null)
	{
		return;
	}

	ArrayList weight3Pool = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
	ArrayList weight2Pool = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
	ArrayList weight1Pool = new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));

	BuildMapEvalWeightPool(maps, MAP_EVAL_MAX_VOTE_WEIGHT, weight3Pool, seen);
	BuildMapEvalWeightPool(maps, 2, weight2Pool, seen);
	BuildMapEvalWeightPool(maps, MAP_EVAL_DEFAULT_VOTE_WEIGHT, weight1Pool, seen);

	if (weight3Pool.Length == 0 && weight2Pool.Length == 0 && weight1Pool.Length == 0)
	{
		delete weight3Pool;
		delete weight2Pool;
		delete weight1Pool;
		return;
	}

	groupNames.PushString(gamemode);
	weight3Pools.Push(view_as<int>(weight3Pool));
	weight2Pools.Push(view_as<int>(weight2Pool));
	weight1Pools.Push(view_as<int>(weight1Pool));
}

bool PopulateNextVoteFromMapEval()
{
	if (!g_ConVars[mapvote_mapeval_random].BoolValue)
	{
		return false;
	}

	if (g_MapEvalGamemodes == null || g_MapEvalGamemodes.Size == 0)
	{
		return false;
	}

	UpdateCurrentMap();
	UpdateGameModeFromMap();

	ArrayList groupNames = new ArrayList(ByteCountToCells(64));
	ArrayList weight3Pools = new ArrayList();
	ArrayList weight2Pools = new ArrayList();
	ArrayList weight1Pools = new ArrayList();
	StringMap seen = new StringMap();

	if (g_GameMode[0])
	{
		AddMapEvalVoteGroup(g_GameMode, groupNames, weight3Pools, weight2Pools, weight1Pools, seen);
	}

	StringMapSnapshot snapshot = g_MapEvalGamemodes.Snapshot();
	char gamemode[64];
	for (int i = 0; i < snapshot.Length; i++)
	{
		snapshot.GetKey(i, gamemode, sizeof(gamemode));
		if (g_GameMode[0] && StrEqual(gamemode, g_GameMode, false))
		{
			continue;
		}

		AddMapEvalVoteGroup(gamemode, groupNames, weight3Pools, weight2Pools, weight1Pools, seen);
	}
	delete snapshot;
	delete seen;

	char mapName[PLATFORM_MAX_PATH];
	int limit = g_ConVars[mapvote_include].IntValue;
	while (groupNames.Length > 0 && g_NextMapList.Length < limit)
	{
		int groupIndex = GetRandomInt(0, groupNames.Length - 1);
		ArrayList weight3Pool = view_as<ArrayList>(weight3Pools.Get(groupIndex));
		ArrayList weight2Pool = view_as<ArrayList>(weight2Pools.Get(groupIndex));
		ArrayList weight1Pool = view_as<ArrayList>(weight1Pools.Get(groupIndex));
		bool preferHighWeight = (GetRandomInt(1, 100) <= 60);
		ArrayList pool = SelectMapEvalWeightPool(weight3Pool, weight2Pool, weight1Pool, preferHighWeight);

		if (pool != null && pool.Length > 0)
		{
			int mapIndex = GetRandomInt(0, pool.Length - 1);
			pool.GetString(mapIndex, mapName, sizeof(mapName));
			g_NextMapList.PushString(mapName);
		}

		if (weight3Pool != null)
		{
			delete weight3Pool;
		}
		if (weight2Pool != null)
		{
			delete weight2Pool;
		}
		if (weight1Pool != null)
		{
			delete weight1Pool;
		}

		groupNames.Erase(groupIndex);
		weight3Pools.Erase(groupIndex);
		weight2Pools.Erase(groupIndex);
		weight1Pools.Erase(groupIndex);
	}

	for (int i = 0; i < weight3Pools.Length; i++)
	{
		ArrayList weight3Pool = view_as<ArrayList>(weight3Pools.Get(i));
		ArrayList weight2Pool = view_as<ArrayList>(weight2Pools.Get(i));
		ArrayList weight1Pool = view_as<ArrayList>(weight1Pools.Get(i));
		if (weight3Pool != null)
		{
			delete weight3Pool;
		}
		if (weight2Pool != null)
		{
			delete weight2Pool;
		}
		if (weight1Pool != null)
		{
			delete weight1Pool;
		}
	}

	delete groupNames;
	delete weight3Pools;
	delete weight2Pools;
	delete weight1Pools;

	return (g_NextMapList.Length > 0);
}
