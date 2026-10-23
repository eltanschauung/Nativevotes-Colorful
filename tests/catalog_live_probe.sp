#pragma semicolon 1
#pragma newdecls required
#include <sourcemod>
#include <mapchooser>
#include <nativevotes>
#include <profiler>
#include "nativevotes_catalog_api.inc"

public Plugin myinfo={name="NativeVotes live catalog verification",author="Hombre",version="1.0"};
ArrayList g_MapList,g_RandomNominationList;
StringMap g_MapTrie;
#define RANDOM_NOMINATION_MENU_SIZE 50
int checks,failures;
int catalogReloads;

public APLRes AskPluginLoad2(Handle plugin,bool late,char[] error,int length)
{
    MarkNativeAsOptional("NativeVotes_ReloadMapCatalog");
    MarkNativeAsOptional("NativeVotes_CopyMapCatalog");
    MarkNativeAsOptional("NativeVotes_CopyMapCategories");
    MarkNativeAsOptional("NativeVotes_GetMapCatalogSource");
    MarkNativeAsOptional("NativeVotes_GetCatalogMapDisplayName");
    return APLRes_Success;
}
public int MenuHandler_CategorySelect(Menu menu,MenuAction action,int client,int item){return 0;}
public int MenuHandler_MapSelect(Menu menu,MenuAction action,int client,int item){return 0;}
bool IsMapEnabledForNomination(const char[] map){int value;return g_MapTrie.GetValue(map,value)&&value==1;}
#include "catalog_nomination_under_test.inc"

void Check(bool ok,const char[] label)
{
    checks++;
    if(!ok){failures++;PrintToServer("[MapCatalogLiveProbe] FAIL: %s",label);}
}

public void OnPluginStart()
{
    g_MapList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_RandomNominationList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_MapTrie=new StringMap();
    RegAdminCmd("sm_nativevotes_catalog_live_probe",RunLive,ADMFLAG_ROOT);
    RegAdminCmd("sm_nativevotes_catalog_vote_state",VoteState,ADMFLAG_ROOT);
    RegAdminCmd("sm_nativevotes_catalog_refresh_cost",RefreshCost,ADMFLAG_ROOT);
}

public void NativeVotes_OnMapCatalogReloaded(){catalogReloads++;}
public Action RefreshCost(int client,int args)
{
    for(int target=1;target<=MaxClients;target++)
        if(IsClientConnected(target)&&!IsFakeClient(target))return Plugin_Handled;
    int before=catalogReloads;Profiler profile=new Profiler();profile.Start();
    bool ok=NativeVotes_ReloadMapCatalog();profile.Stop();
    PrintToServer("[MapCatalogLiveRefresh] ok=%d forwards=%d elapsed_ms=%.3f",ok,catalogReloads-before,profile.Time*1000.0);
    delete profile;return Plugin_Handled;
}

public Action VoteState(int client,int args)
{
    int port=FindConVar("hostport").IntValue;char path[PLATFORM_MAX_PATH];
    FormatEx(path,sizeof(path),"addons/sourcemod/data/catalog_vote_state_%d.txt",port);
    File f=OpenFile(path,"w");f.WriteLine("vote_in_progress=%d",NativeVotes_IsVoteInProgress());delete f;
    return Plugin_Handled;
}

public Action RunLive(int client,int args)
{
    checks=failures=0;
    NativeVotes_CopyMapCatalog(g_MapList);
    Check(g_MapList.Length>=300,"full production catalog supplied by native");
    char source[64];NativeVotes_GetMapCatalogSource(source,sizeof(source));
    Check(StrEqual(source,"mapeval.cfg"),"primary catalog active");
    ArrayList categories=new ArrayList(ByteCountToCells(64));NativeVotes_CopyMapCategories(categories);
    Menu root=BuildNominationCategoryMenu();
    Check(root.ItemCount==categories.Length+2,"all categories appear after fixed entries");
    char map[PLATFORM_MAX_PATH],category[64],key[PLATFORM_MAX_PATH];
    root.GetItem(0,key,sizeof(key));Check(StrEqual(key,"popular maps"),"Popular first");
    root.GetItem(1,key,sizeof(key));Check(StrEqual(key,"@random"),"Random 50 second");
    Check(root.Pagination>0&&root.Pagination<root.ItemCount,"category root paginates");
    char label[128];int style;
    root.GetTitle(label,sizeof(label));Check(StrEqual(label,"Nominate a map"),"root title");
    for(int i=0;i<categories.Length;i++)
    {
        categories.GetString(i,category,sizeof(category));root.GetItem(i+2,key,sizeof(key),style,label,sizeof(label));
        Check(StrEqual(category,label),"root label preserves catalog spelling");
    }
    delete root;
    for(int i=0;i<categories.Length;i++)
    {
        categories.GetString(i,category,sizeof(category));
        ArrayList maps=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));NativeVotes_CopyMapCatalog(maps,category);
        Menu menu=BuildCategoryNominationMenu(category);
        Check(menu!=null&&menu.ItemCount==maps.Length,"category menu matches shared catalog");
        Check(menu.ExitBackButton,"category returns to root");
        if(menu.ItemCount>7)Check(menu.Pagination>0&&menu.Pagination<menu.ItemCount,"large production category paginates");
        for(int j=0;j<maps.Length;j++)
        {
            maps.GetString(j,map,sizeof(map));menu.GetItem(j,key,sizeof(key));
            Check(StrEqual(map,key)&&IsMapValid(map),"menu map order and validity");
            char cached[PLATFORM_MAX_PATH],expected[PLATFORM_MAX_PATH];
            Check(NativeVotes_GetCatalogMapDisplayName(map,cached,sizeof(cached)),"published catalog includes cached display name");
            GetMapDisplayName(map,expected,sizeof(expected));
            Check(StrEqual(cached,expected),"cached display name matches SourceMod for ordinary and Workshop maps");
        }
        delete menu;delete maps;
    }
    ArrayList popular=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));NativeVotes_CopyMapCatalog(popular,"popular maps");
    Check(popular.Length==50,"Popular contains 50 available maps");
    StringMap seen=new StringMap();
    for(int i=0;i<popular.Length;i++)
    {
        popular.GetString(i,map,sizeof(map));int value;
        Check(!seen.GetValue(map,value)&&g_MapList.FindString(map)!=-1,"Popular unique and in catalog");seen.SetValue(map,1);
    }
    delete seen;
    ArrayList excluded=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));GetExcludeMapList(excluded);
    char current[PLATFORM_MAX_PATH];GetCurrentMap(current,sizeof(current));g_MapTrie.Clear();
    for(int i=0;i<g_MapList.Length;i++)
    {
        g_MapList.GetString(i,map,sizeof(map));
        g_MapTrie.SetValue(map,StrEqual(map,current)||excluded.FindString(map)!=-1?2:1);
    }
    BuildRandomNominationList();Check(g_RandomNominationList.Length==50,"production nr samples 50");
    seen=new StringMap();
    for(int i=0;i<g_RandomNominationList.Length;i++)
    {
        g_RandomNominationList.GetString(i,map,sizeof(map));int value;
        Check(IsMapEnabledForNomination(map)&&!seen.GetValue(map,value),"production nr valid unique enabled maps");seen.SetValue(map,1);
    }
    int port=FindConVar("hostport").IntValue;char path[PLATFORM_MAX_PATH];
    FormatEx(path,sizeof(path),"addons/sourcemod/data/nativevotes_catalog_live_probe_%d.txt",port);
    File f=OpenFile(path,"w");f.WriteLine("checks=%d failures=%d maps=%d categories=%d popular=%d",checks,failures,g_MapList.Length,categories.Length,popular.Length);
    for(int i=0;i<categories.Length;i++){categories.GetString(i,category,sizeof(category));f.WriteLine("category=%s",category);}
    delete f;
    PrintToServer("[MapCatalogLiveProbe] checks=%d failures=%d maps=%d categories=%d popular=%d",checks,failures,g_MapList.Length,categories.Length,popular.Length);
    delete seen;delete excluded;delete popular;delete categories;
    return Plugin_Handled;
}
