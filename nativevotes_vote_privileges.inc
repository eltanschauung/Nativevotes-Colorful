#if defined _nativevotes_vote_privileges_included
 #endinput
#endif
#define _nativevotes_vote_privileges_included

#include <clientprefs>

#define NATIVEVOTE_PREF_WHITELIST_COOKIE "nativevotes_vote_whitelist"
#define NATIVEVOTE_PREF_BLACKLIST_COOKIE "nativevotes_vote_blacklist"

Handle g_NativeVoteWhitelistCookie = null;
Handle g_NativeVoteBlacklistCookie = null;

void NativeVotePrefs_Init(bool registerCommands)
{
	g_NativeVoteWhitelistCookie = RegClientCookie(NATIVEVOTE_PREF_WHITELIST_COOKIE, "NativeVotes vote whitelist flag", CookieAccess_Private);
	g_NativeVoteBlacklistCookie = RegClientCookie(NATIVEVOTE_PREF_BLACKLIST_COOKIE, "NativeVotes vote blacklist flag", CookieAccess_Private);

	if (registerCommands)
	{
		RegAdminCmd("sm_votewhitelist", Command_VoteWhitelist, ADMFLAG_GENERIC, "sm_votewhitelist <#userid|name> - Toggle vote whitelist status for a client.");
		RegAdminCmd("sm_voteblacklist", Command_VoteBlacklist, ADMFLAG_GENERIC, "sm_voteblacklist <#userid|name> - Toggle vote blacklist status for a client.");
	}
}

bool NativeVotePrefs_IsCookieReady(int client)
{
	return client > 0
		&& client <= MaxClients
		&& IsClientInGame(client)
		&& !IsFakeClient(client)
		&& AreClientCookiesCached(client);
}

bool NativeVotePrefs_GetClientBool(int client, Handle cookie)
{
	if (cookie == null || !NativeVotePrefs_IsCookieReady(client))
	{
		return false;
	}

	char value[8];
	GetClientCookie(client, cookie, value, sizeof(value));
	return StringToInt(value) != 0;
}

void NativeVotePrefs_SetClientBool(int client, Handle cookie, bool enabled)
{
	if (cookie == null || client <= 0 || client > MaxClients || !AreClientCookiesCached(client))
	{
		return;
	}

	SetClientCookie(client, cookie, enabled ? "1" : "0");
}

bool NativeVotePrefs_IsWhitelisted(int client)
{
	if (NativeVotePrefs_GetClientBool(client, g_NativeVoteBlacklistCookie))
	{
		return false;
	}

	return NativeVotePrefs_GetClientBool(client, g_NativeVoteWhitelistCookie);
}

bool NativeVotePrefs_IsBlacklisted(int client)
{
	return NativeVotePrefs_GetClientBool(client, g_NativeVoteBlacklistCookie);
}

Action NativeVotePrefs_ToggleClientFlag(int client, int args, Handle cookie, const char[] commandName, const char[] label)
{
	if (args < 1)
	{
		ReplyToCommand(client, "[NativeVotes] Usage: %s <#userid|name>", commandName);
		return Plugin_Handled;
	}

	if (cookie == null)
	{
		ReplyToCommand(client, "[NativeVotes] Clientprefs cookie is not available.");
		return Plugin_Handled;
	}

	char targetArg[64];
	GetCmdArg(1, targetArg, sizeof(targetArg));

	int target = FindTarget(client, targetArg, true, false);
	if (target <= 0)
	{
		return Plugin_Handled;
	}

	if (!AreClientCookiesCached(target))
	{
		ReplyToCommand(client, "[NativeVotes] Clientprefs are not loaded for %N yet.", target);
		return Plugin_Handled;
	}

	bool enabled = !NativeVotePrefs_GetClientBool(target, cookie);
	NativeVotePrefs_SetClientBool(target, cookie, enabled);

	ReplyToCommand(client, "[NativeVotes] %N vote %s %s.", target, label, enabled ? "enabled" : "disabled");
	LogAction(client, target, "\"%L\" set vote %s for \"%L\" to %d", client, label, target, enabled ? 1 : 0);
	return Plugin_Handled;
}

public Action Command_VoteWhitelist(int client, int args)
{
	return NativeVotePrefs_ToggleClientFlag(client, args, g_NativeVoteWhitelistCookie, "sm_votewhitelist", "whitelist");
}

public Action Command_VoteBlacklist(int client, int args)
{
	return NativeVotePrefs_ToggleClientFlag(client, args, g_NativeVoteBlacklistCookie, "sm_voteblacklist", "blacklist");
}

int NativeVotePrefs_GetMapVoteBonus(int client)
{
	return NativeVotePrefs_IsWhitelisted(client) ? 1 : 0;
}

int NativeVotePrefs_GetRTVWeight(int client)
{
	return NativeVotePrefs_IsWhitelisted(client) ? 2 : 1;
}
