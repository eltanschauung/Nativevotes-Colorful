#if defined _nativevotes_nextmap_included
    #endinput
#endif
#define _nativevotes_nextmap_included

// This module owns the command and bare-chat alias. sm_nextmap remains the
// SourceMod core ConVar; registering a command with that name would collide.
void NativeVotes_NextMapInit()
{
	RegConsoleCmd("nextmap", Command_NativeVotesNextMap, "Show the next map or pending end-map vote.");
	AddCommandListener(Listener_NativeVotesNextMapChat, "say");
	AddCommandListener(Listener_NativeVotesNextMapChat, "say_team");
}

public Action Command_NativeVotesNextMap(int client, int args)
{
	if (client > 0 && !IsClientInGame(client)) return Plugin_Handled;
	NativeVotes_PrintNextMap(client, false);
	return Plugin_Handled;
}

public Action Listener_NativeVotesNextMapChat(int client, const char[] command, int argc)
{
	if (client <= 0 || !IsClientInGame(client)) return Plugin_Continue;
	char message[256];
	GetCmdArgString(message, sizeof(message));
	StripQuotes(message);
	TrimString(message);
	if (!StrEqual(message, "nextmap", false)) return Plugin_Continue;
	// Raw say listeners must not bypass Filters/BaseComm's gag policy.
	if (GetFeatureStatus(FeatureType_Native, "BaseComm_IsClientGagged") == FeatureStatus_Available
		&& BaseComm_IsClientGagged(client)) return Plugin_Continue;
	ReplySource previous = SetCmdReplySource(SM_REPLY_TO_CHAT);
	NativeVotes_PrintNextMap(client, true);
	SetCmdReplySource(previous);
	// Filters owns the original chat's rendering/suppression. Do not print it
	// again or depend on OnClientSayCommand_Post for the next-map response.
	return Plugin_Continue;
}

void NativeVotes_PrintNextMap(int client, bool fromBareChat)
{
	char message[PLATFORM_MAX_PATH + 96];
	char map[PLATFORM_MAX_PATH];
	if (g_NextMapConVar != null) g_NextMapConVar.GetString(map, sizeof(map));
	TrimString(map);
	if (!map[0] || (EndOfMapVoteEnabled() && !HasEndOfMapVoteFinished()))
	{
		strcopy(message, sizeof(message), "{lightgreen}[Nominations]{default} The next map has not been selected; a vote is pending.");
	}
	else
	{
		char display[PLATFORM_MAX_PATH];
		if (!NativeVotes_GetCachedMapDisplayName(map, display, sizeof(display)))
			strcopy(display, sizeof(display), map);
		FormatEx(message, sizeof(message), "{lightgreen}[Nominations]{default} Next map: {gold}%s{default}", display);
	}
	ConVar show = fromBareChat && client > 0 ? FindConVar("sm_trigger_show") : null;
	if (show != null && show.BoolValue) CPrintToChatAll("%s", message);
	else CReplyToCommand(client, "%s", message);
}
