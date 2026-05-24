#define MAPS_COUNT 5

Menu g_MapList;
int g_mapCount;
ArrayList g_SelectedMaps;
int g_MapListSerial = -1;

int LoadMapList(Menu menu)
{
	menu.RemoveAllItems();

	ArrayList maps = new ArrayList(PLATFORM_MAX_PATH);
	Handle result = ReadMapList(maps, g_MapListSerial, "sm_votemap menu");
	if (result == INVALID_HANDLE)
	{
		delete maps;
		return 0;
	}

	char mapName[PLATFORM_MAX_PATH];
	char displayName[PLATFORM_MAX_PATH];
	int count = maps.Length;

	for (int i = 0; i < count; i++)
	{
		maps.GetString(i, mapName, sizeof(mapName));
		GetMapDisplayName(mapName, displayName, sizeof(displayName));
		menu.AddItem(mapName, displayName);
	}

	delete maps;
	return count;
}

void DisplayVoteMapMenu(int client, int mapCount, char maps[MAPS_COUNT][PLATFORM_MAX_PATH])
{
	char mapsList[MAPS_COUNT * (PLATFORM_MAX_PATH + 1)];
	for (int i = 0; i < mapCount; i++)
	{
		Format(mapsList, sizeof(mapsList), "%s %s", mapsList, maps[i]);
	}

	LogAction(client, -1, "\"%L\" initiated a map vote for%s.", client, mapsList);
	CPrintToChatAllEx(client, "[{lightgreen}NativeVotes{default}] Initiated a map vote");

	g_voteType = map;
	g_voteClient[VOTE_CLIENTID] = client;
	g_voteClient[VOTE_USERID] = GetClientUserId(client);

	Handle voteMenu = CreateMenu(Handler_VoteCallback, view_as<MenuAction>(MENU_ACTIONS_ALL));

	if (mapCount == 1)
	{
		GetMapDisplayName(maps[0], g_voteInfo[VOTE_NAME], sizeof(g_voteInfo[]));
		SetMenuTitle(voteMenu, "Change Map To");
		AddMenuItem(voteMenu, maps[0], "Yes");
		AddMenuItem(voteMenu, VOTE_NO, "No");
	}
	else
	{
		g_voteInfo[VOTE_NAME][0] = '\0';
		SetMenuTitle(voteMenu, "Map Vote");

		for (int i = 0; i < mapCount; i++)
		{
			char displayName[PLATFORM_MAX_PATH];
			GetMapDisplayName(maps[i], displayName, sizeof(displayName));
			AddMenuItem(voteMenu, maps[i], displayName);
		}
	}

	SetMenuExitButton(voteMenu, false);
	VoteMenuToAll(voteMenu, 20);
}

public Action Command_Votemap(int client, int args)
{
	if (args < 1)
	{
		CReplyToCommand(client, "[{lightgreen}NativeVotes{default}] Usage: sm_votemap <mapname> [mapname2] ... [mapname5]");
		return Plugin_Handled;
	}

	if (Internal_IsVoteInProgress())
	{
		CReplyToCommand(client, "%t", "Vote in Progress");
		return Plugin_Handled;
	}

	if (!TestVoteDelay(client))
	{
		return Plugin_Handled;
	}

	char maps[MAPS_COUNT][PLATFORM_MAX_PATH];
	int mapCount = args > MAPS_COUNT ? MAPS_COUNT : args;

	for (int i = 0; i < mapCount; i++)
	{
		GetCmdArg(i + 1, maps[i], sizeof(maps[]));
		if (!IsMapValid(maps[i]))
		{
			CReplyToCommand(client, "[{lightgreen}NativeVotes{default}] Map not found: %s", maps[i]);
			return Plugin_Handled;
		}
	}

	DisplayVoteMapMenu(client, mapCount, maps);
	return Plugin_Handled;
}

public int MenuHandler_Map(Menu menu, MenuAction action, int client, int param2)
{
	if (action == MenuAction_Select)
	{
		char mapName[PLATFORM_MAX_PATH];
		menu.GetItem(param2, mapName, sizeof(mapName));

		if (!IsMapValid(mapName))
		{
			CPrintToChat(client, "[{lightgreen}NativeVotes{default}] Map is no longer valid: %s", mapName);
			return 0;
		}

		char maps[MAPS_COUNT][PLATFORM_MAX_PATH];
		strcopy(maps[0], sizeof(maps[]), mapName);
		DisplayVoteMapMenu(client, 1, maps);
	}
	else if (action == MenuAction_Display)
	{
		char title[100];
		Format(title, sizeof(title), "%T", "Please select a map", client);
		SetPanelTitle(view_as<Panel>(param2), title);
	}

	return 0;
}

public void AdminMenu_VoteMap(TopMenu topmenu,
	TopMenuAction action,
	TopMenuObject object_id,
	int param,
	char[] buffer,
	int maxlength)
{
	if (action == TopMenuAction_DisplayOption)
	{
		Format(buffer, maxlength, "%T", "Map vote", param);
	}
	else if (action == TopMenuAction_SelectOption)
	{
		g_MapList.Display(param, MENU_TIME_FOREVER);
	}
}
