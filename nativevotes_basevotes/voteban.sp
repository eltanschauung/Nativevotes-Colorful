void DisplayVoteBanMenu(int client, int target)
{
	g_voteClient[VOTE_CLIENTID] = target;
	g_voteClient[VOTE_USERID] = GetClientUserId(target);
	GetClientName(target, g_voteInfo[VOTE_NAME], sizeof(g_voteInfo[]));
	GetClientAuthId(target, AuthId_Steam2, g_voteInfo[VOTE_AUTHID], sizeof(g_voteInfo[]));
	GetClientIP(target, g_voteInfo[VOTE_IP], sizeof(g_voteInfo[]));

	LogAction(client, target, "\"%L\" initiated a ban vote against \"%L\"", client, target);
	CPrintToChatAllEx(client, "[{lightgreen}NativeVotes{default}] Initiated a ban vote against %N", target);

	g_voteType = ban;

	Handle voteMenu = CreateMenu(Handler_VoteCallback, view_as<MenuAction>(MENU_ACTIONS_ALL));
	SetMenuTitle(voteMenu, "Voteban Player");
	AddMenuItem(voteMenu, VOTE_YES, "Yes");
	AddMenuItem(voteMenu, VOTE_NO, "No");
	SetMenuExitButton(voteMenu, false);
	VoteMenuToAll(voteMenu, 20);
}

void DisplayBanTargetMenu(int client)
{
	Menu menu = new Menu(MenuHandler_Ban);

	char title[100];
	Format(title, sizeof(title), "%T:", "Ban vote", client);
	menu.SetTitle(title);
	menu.ExitBackButton = true;

	AddTargetsToMenu(menu, client, false, false);
	menu.Display(client, MENU_TIME_FOREVER);
}

public Action Command_Voteban(int client, int args)
{
	if (args < 1)
	{
		CReplyToCommand(client, "[{lightgreen}NativeVotes{default}] Usage: sm_voteban <player> [reason]");
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

	char targetArg[64];
	GetCmdArg(1, targetArg, sizeof(targetArg));

	int target = FindTarget(client, targetArg, true, false);
	if (target <= 0)
	{
		return Plugin_Handled;
	}

	g_voteArg[0] = '\0';
	if (args >= 2)
	{
		char text[256];
		GetCmdArgString(text, sizeof(text));

		int len = BreakString(text, targetArg, sizeof(targetArg));
		if (len != -1)
		{
			strcopy(g_voteArg, sizeof(g_voteArg), text[len]);
			TrimString(g_voteArg);
		}
	}

	DisplayVoteBanMenu(client, target);
	return Plugin_Handled;
}

public int MenuHandler_Ban(Menu menu, MenuAction action, int client, int param2)
{
	if (action == MenuAction_Select)
	{
		char info[32];
		menu.GetItem(param2, info, sizeof(info));

		int userid = StringToInt(info);
		int target = GetClientOfUserId(userid);
		if (target > 0)
		{
			g_voteArg[0] = '\0';
			DisplayVoteBanMenu(client, target);
		}
	}
	else if (action == MenuAction_Cancel)
	{
		if (param2 == MenuCancel_ExitBack && hTopMenu != INVALID_HANDLE)
		{
			DisplayTopMenu(hTopMenu, client, TopMenuPosition_LastCategory);
		}
	}
	else if (action == MenuAction_End)
	{
		delete menu;
	}

	return 0;
}

public void AdminMenu_VoteBan(TopMenu topmenu,
	TopMenuAction action,
	TopMenuObject object_id,
	int param,
	char[] buffer,
	int maxlength)
{
	if (action == TopMenuAction_DisplayOption)
	{
		Format(buffer, maxlength, "%T", "Ban vote", param);
	}
	else if (action == TopMenuAction_SelectOption)
	{
		DisplayBanTargetMenu(param);
	}
}
