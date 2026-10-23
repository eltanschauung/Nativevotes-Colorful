#pragma semicolon 1
#pragma newdecls required
#include <sourcemod>
#include <mapchooser>
#include <profiler>

public Plugin myinfo={name="Isolated NativeVotes catalog cost probe",author="Hombre",version="1.0"};
enum {mapvote_include,excludeold,excludecurrent}
ConVar g_ConVars[3];
ArrayList g_MapList,g_NextMapList,g_RandomNominationList,history;
StringMap g_MapTrie;
Menu g_MapMenu;
char g_CurrentMap[PLATFORM_MAX_PATH];
bool g_HasVoteStarted,g_WaitingForVote,busy;
int engineCalls,run;
#define RANDOM_NOMINATION_MENU_SIZE 50
#define MAPSTATUS_ENABLED 1
#define MAPSTATUS_DISABLED 2
#define MAPSTATUS_EXCLUDE_CURRENT 4
#define MAPSTATUS_EXCLUDE_PREVIOUS 8

FindMapResult MeasuredFind(const char[] map,char[] output,int maxlen)
{
    engineCalls++;
    return FindMap(map,output,maxlen);
}
bool MeasuredDisplay(const char[] map,char[] output,int maxlen)
{
    engineCalls++;
    return GetMapDisplayName(map,output,maxlen);
}
void UpdateCurrentMap(){GetCurrentMap(g_CurrentMap,sizeof(g_CurrentMap));}
bool RecentMapHistory_Contains(const char[] map){return history.FindString(map)!=-1;}
void CreateNextVote(){g_NextMapList.Clear();PopulateNextVoteFromMapEval();}
bool IsMapEnabledForNomination(const char[] map)
{
    int status;return g_MapTrie.GetValue(map,status)&&(status&MAPSTATUS_DISABLED)==0;
}
void RecordSlowNominationOperation(const char[] operation,float startedAt,int entryCount)
{
    #pragma unused operation
    #pragma unused startedAt
    #pragma unused entryCount
}
public int MenuHandler_MapSelect(Menu menu,MenuAction action,int client,int item){return 0;}
bool NativeVotes_GetCachedMapDisplayName(const char[] map,char[] output,int maxlen)
{
    return Catalog_GetMapDisplayName(map,output,maxlen);
}

#define FindMap MeasuredFind
#define GetMapDisplayName MeasuredDisplay
#include "nativevotes_mapeval.inc"
#include "catalog_nomination_under_test.inc"
#undef FindMap
#undef GetMapDisplayName

bool Empty()
{
    for(int client=1;client<=MaxClients;client++)
        if(IsClientConnected(client)&&!IsFakeClient(client))return false;
    return true;
}
float Lap(Profiler stage)
{
    stage.Stop();float ms=stage.Time*1000.0;stage.Start();return ms;
}
public void OnPluginStart()
{
    RegAdminCmd("sm_nativevotes_catalog_cost",Start,ADMFLAG_ROOT);
    g_MapList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_NextMapList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_MapTrie=new StringMap();
    g_CatalogMaps=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogCategories=new ArrayList(ByteCountToCells(64));
    g_CatalogPopular=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogGroups=new StringMap();g_MapEvalVoteWeights=new StringMap();
    history=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_ConVars[mapvote_include]=FindConVar("sm_mapvote_include");
    g_ConVars[excludeold]=FindConVar("sm_nominate_excludeold");
    g_ConVars[excludecurrent]=FindConVar("sm_nominate_excludecurrent");
}
public Action Start(int client,int args)
{
    if(busy||!Empty()){PrintToServer("[MapCatalogCost] Refused: busy or human connected");return Plugin_Handled;}
    busy=true;run=0;Catalog_ResetMapValidationCache();
    char path[PLATFORM_MAX_PATH];
    FormatEx(path,sizeof(path),"data/nativevotes_catalog_cost_%d.txt",FindConVar("hostport").IntValue);
    BuildPath(Path_SM,path,sizeof(path),"%s",path);
    File out=OpenFile(path,"w");delete out;
    CreateTimer(0.2,Measure,_,TIMER_FLAG_NO_MAPCHANGE);
    return Plugin_Handled;
}
public Action Measure(Handle timer)
{
    if(!Empty()){busy=false;PrintToServer("[MapCatalogCost] Stopped: human connected");return Plugin_Stop;}
    engineCalls=0;history.Clear();GetExcludeMapList(history);
    Profiler total=new Profiler(),stage=new Profiler();total.Start();stage.Start();
    // Private parser and consumer state: never call the live reload native or
    // dispatch the catalog forward into production nomination consumers.
    Catalog_Clear();bool ok=Catalog_LoadPrimary();g_CatalogAttempted=true;g_CatalogLoaded=ok;
    float parseMs=Lap(stage);
    g_MapList.Clear();char map[PLATFORM_MAX_PATH],display[PLATFORM_MAX_PATH];
    for(int i=0;i<g_CatalogMaps.Length;i++){g_CatalogMaps.GetString(i,map,sizeof(map));g_MapList.PushString(map);}
    BuildMapMenu();float menuMs=Lap(stage);
    BuildRandomNominationList();float randomMs=Lap(stage);
    CreateNextVote();total.Stop();float refreshMs=total.Time*1000.0;
    int refreshCalls=engineCalls;

    total.Start();bool ready=Catalog_EnsureLoaded();CreateNextVote();
    Menu vote=new Menu(MenuHandler_MapSelect);
    for(int i=0;i<g_NextMapList.Length;i++)
    {
        g_NextMapList.GetString(i,map,sizeof(map));Catalog_GetMapDisplayName(map,display,sizeof(display));vote.AddItem(map,display);
    }
    delete vote;total.Stop();float voteMs=total.Time*1000.0;
    int voteCalls=engineCalls-refreshCalls;
    char result[512];
    FormatEx(result,sizeof(result),"run=%d cold=%d ok=%d maps=%d parse_ms=%.3f menu_ms=%.3f random_ms=%.3f refresh_ms=%.3f vote_ms=%.3f refresh_engine_calls=%d vote_engine_calls=%d",run,run==0,ok&&ready,g_MapList.Length,parseMs,menuMs,randomMs,refreshMs,voteMs,refreshCalls,voteCalls);
    PrintToServer("[MapCatalogCost] %s",result);
    char path[PLATFORM_MAX_PATH];BuildPath(Path_SM,path,sizeof(path),"data/nativevotes_catalog_cost_%d.txt",FindConVar("hostport").IntValue);
    File out=OpenFile(path,"a");out.WriteLine("%s",result);delete out;
    stage.Stop();delete total;delete stage;
    run++;
    if(run<3)CreateTimer(1.0,Measure,_,TIMER_FLAG_NO_MAPCHANGE);
    else {busy=false;PrintToServer("[MapCatalogCost] Complete: no live reloads, votes or map changes");}
    return Plugin_Stop;
}
