#if defined _nativevotes_vote_privileges_included
 #endinput
#endif
#define _nativevotes_vote_privileges_included

#undef REQUIRE_PLUGIN
#tryinclude <adminsdb_api>
#define REQUIRE_PLUGIN

#if !defined _adminsdb_api_included
stock int AdminsDB_GetClientWhitelistLevel(int client)
{
	if (client || !client) {}
	return 0;
}
#endif

stock void NativeVotePrefs_Init(bool registerCommands)
{
	if (registerCommands || !registerCommands) {}
}

stock int NativeVotePrefs_GetClientLevel(int client)
{
	if (client <= 0 || client > MaxClients || !IsClientInGame(client) || IsFakeClient(client))
	{
		return 0;
	}

	if (GetFeatureStatus(FeatureType_Native, "AdminsDB_GetClientWhitelistLevel") != FeatureStatus_Available)
	{
		return 0;
	}

	int level = AdminsDB_GetClientWhitelistLevel(client);
	if (level > 3)
	{
		return 3;
	}
	if (level < -3)
	{
		return -3;
	}
	return level;
}

stock bool NativeVotePrefs_IsWhitelisted(int client)
{
	return NativeVotePrefs_GetClientLevel(client) > 0;
}

stock bool NativeVotePrefs_IsBlacklisted(int client)
{
	return NativeVotePrefs_GetClientLevel(client) < 0;
}

stock int NativeVotePrefs_GetMapVoteBonus(int client)
{
	return NativeVotePrefs_GetClientVoteWeight(client) - 1;
}

stock int NativeVotePrefs_GetClientVoteWeight(int client)
{
	int level = NativeVotePrefs_GetClientLevel(client);
	return level > 0 ? 1 + level : 1;
}

stock int NativeVotePrefs_GetRTVWeight(int client)
{
	return NativeVotePrefs_GetClientVoteWeight(client);
}
