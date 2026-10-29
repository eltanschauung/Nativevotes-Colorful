#pragma semicolon 1
#pragma newdecls required
#include <sourcemod>

int checks, failures, replies, broadcasts, registrations, listeners, lookups;
bool voteEnabled = true, voteFinished, showAll, gagged, gagNative = true;
char nextMap[PLATFORM_MAX_PATH] = "workshop/cp_test.ugc123";
char text[256], output[PLATFORM_MAX_PATH + 96];
ReplySource replySource = SM_REPLY_TO_CONSOLE;
methodmap TestConVar < Handle
{
    public void GetString(char[] value, int size) { strcopy(value, size, nextMap); }
    property bool BoolValue
    {
        public get() { return showAll; }
    }
}
TestConVar g_NextMapConVar = view_as<TestConVar>(1);
stock void TestRegConsoleCmd(const char[] name, ConCmd callback, const char[] description)
{
    #pragma unused callback
    #pragma unused description
    registrations++;
    if (!StrEqual(name, "nextmap")) failures++;
}
stock void TestAddCommandListener(CommandListener callback, const char[] command)
{
    #pragma unused callback
    if (StrEqual(command, "say")) listeners |= 1;
    else if (StrEqual(command, "say_team")) listeners |= 2;
    else failures++;
}
stock bool TestIsInGame(int client) { return client == 1; }
stock int TestGetCmdArgString(char[] value, int size) { return strcopy(value, size, text); }
stock bool EndOfMapVoteEnabled() { return voteEnabled; }
stock bool HasEndOfMapVoteFinished() { return voteFinished; }
stock bool NativeVotes_GetCachedMapDisplayName(const char[] map, char[] display, int size)
{
    #pragma unused map
    lookups++;
    strcopy(display, size, "cp_test");
    return true;
}
stock TestConVar TestFindConVar(const char[] name)
{
    if (!StrEqual(name, "sm_trigger_show")) failures++;
    return view_as<TestConVar>(2);
}
stock FeatureStatus TestFeatureStatus(FeatureType feature, const char[] name)
{
    if (feature != FeatureType_Native || !StrEqual(name, "BaseComm_IsClientGagged")) failures++;
    return gagNative ? FeatureStatus_Available : FeatureStatus_Unavailable;
}
stock bool BaseComm_IsClientGagged(int client)
{
    if (!gagNative || client != 1) failures++;
    return gagged;
}
stock ReplySource TestSetReplySource(ReplySource value)
{
    ReplySource old = replySource;
    replySource = value;
    return old;
}
stock void CReplyToCommand(int client, const char[] format, any ...)
{
    if (client < 0 || client > 1) failures++;
    replies++;
    VFormat(output, sizeof(output), format, 3);
}
stock void CPrintToChatAll(const char[] format, any ...)
{
    broadcasts++;
    VFormat(output, sizeof(output), format, 2);
}
#define ConVar TestConVar
#define RegConsoleCmd TestRegConsoleCmd
#define AddCommandListener TestAddCommandListener
#define IsClientInGame TestIsInGame
#define GetCmdArgString TestGetCmdArgString
#define FindConVar TestFindConVar
#define GetFeatureStatus TestFeatureStatus
#define SetCmdReplySource TestSetReplySource
#include "nativevotes_nextmap.inc"

void Check(bool pass, const char[] label)
{
    checks++;
    if (!pass) { failures++; PrintToServer("[NextMapProbe] FAIL: %s", label); }
}
public void OnPluginStart()
{
    NativeVotes_NextMapInit();
    Check(registrations == 1 && listeners == 3, "one command owner and public/team say listeners");
    strcopy(text, sizeof(text), "nextmap");
    Check(Listener_NativeVotesNextMapChat(1, "say", 1) == Plugin_Continue && replies == 1,
        "bare alias replies exactly once without owning Filters chat rendering");
    Check(StrContains(output, "vote is pending") != -1 && lookups == 0, "pending vote does not display premature map");
    Check(replySource == SM_REPLY_TO_CONSOLE, "chat reply source restored after dispatch");
    voteFinished = true;
    strcopy(text, sizeof(text), "\"  NeXtMaP  \"");
    Listener_NativeVotesNextMapChat(1, "say_team", 1);
    Check(replies == 2 && StrContains(output, "{gold}cp_test{default}") != -1 && lookups == 1,
        "quoted/case-insensitive team chat uses shared cached map display");
    strcopy(text, sizeof(text), "nextmap extra");
    Listener_NativeVotesNextMapChat(1, "say", 1);
    strcopy(text, sizeof(text), "!nextmap");
    Listener_NativeVotesNextMapChat(1, "say", 1);
    strcopy(text, sizeof(text), "/nextmap");
    Listener_NativeVotesNextMapChat(1, "say", 1);
    Check(replies == 2, "arguments and registered chat triggers are not double-dispatched by bare listener");
    strcopy(text, sizeof(text), "nextmap");
    Listener_NativeVotesNextMapChat(0, "say", 1);
    Listener_NativeVotesNextMapChat(2, "say", 1);
    Check(replies == 2, "console say and invalid clients cannot enter bare alias");
    gagged = true;
    Listener_NativeVotesNextMapChat(1, "say", 1);
    Check(replies == 2, "bare alias cannot bypass BaseComm/Filters gag");
    gagged = false; gagNative = false;
    Listener_NativeVotesNextMapChat(1, "say", 1);
    Check(replies == 3, "optional gag native absence is safe");
    showAll = true;
    Listener_NativeVotesNextMapChat(1, "say", 1);
    Check(replies == 3 && broadcasts == 1, "public trigger setting broadcasts bare reply once");
    Command_NativeVotesNextMap(1, 0);
    Command_NativeVotesNextMap(0, 0);
    Check(replies == 5 && broadcasts == 1, "registered command remains private and works for console");
    Command_NativeVotesNextMap(2, 0);
    Check(replies == 5, "invalid command client rejected");
    voteEnabled = false; voteFinished = false;
    Command_NativeVotesNextMap(0, 0);
    Check(StrContains(output, "cp_test") != -1, "configured map displayed when end-map votes disabled");
    nextMap[0] = '\0';
    Command_NativeVotesNextMap(0, 0);
    Check(StrContains(output, "vote is pending") != -1, "empty next-map value handled safely");
    g_NextMapConVar = null;
    Command_NativeVotesNextMap(0, 0);
    Check(StrContains(output, "vote is pending") != -1, "unavailable convar handled safely");
    PrintToServer("[NextMapProbe] %d checks, %d failures", checks, failures);
}
