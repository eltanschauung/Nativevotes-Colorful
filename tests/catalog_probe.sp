#pragma semicolon 1
#pragma newdecls required
#include <sourcemod>

public Plugin myinfo = {name="NativeVotes catalog regression probe", author="Hombre", version="1.0"};
enum { mapvote_include }
ConVar g_ConVars[1], probeCycle;
ArrayList g_MapList, g_NextMapList, g_RandomNominationList;
StringMap g_MapTrie;
char g_CurrentMap[PLATFORM_MAX_PATH], excluded[64];
char probePrimaryAbsolute[PLATFORM_MAX_PATH];
bool g_HasVoteStarted, g_WaitingForVote;
int checks, failures;
bool g_CatalogMenuReady;
int menuBuilds;
int resolutionCalls, validationSize = 32, validationStamp = 1;
char removedMap[64], changedMap[64];
#define RANDOM_NOMINATION_MENU_SIZE 50
#define PRIMARY "addons/sourcemod/data/nativevotes_catalog_probe_primary.cfg"
#define CYCLE "cfg/nativevotes_catalog_probe_cycle.txt"

int ProbeBuildPath(PathType type, char[] buffer, int maxlen, const char[] format, any ...)
{
    #pragma unused type
    #pragma unused format
    strcopy(buffer, maxlen, probePrimaryAbsolute);
    return strlen(buffer);
}
ConVar ProbeFindConVar(const char[] name) { return StrEqual(name,"mapcyclefile") ? probeCycle : null; }
FindMapResult ProbeFindMap(const char[] map, char[] output, int maxlen)
{
    resolutionCalls++;
    if (StrContains(map, "missing_") == 0 || (removedMap[0] && StrEqual(map, removedMap))) return FindMap_NotFound;
    strcopy(output,maxlen,map); return FindMap_Found;
}
int ProbeFileSize(const char[] path, bool valve = false, const char[] id = "GAME")
{
    #pragma unused valve
    #pragma unused id
    char map[64];FormatEx(map,sizeof(map),"maps/%s.bsp",removedMap);
    if(removedMap[0]&&StrEqual(path,map))return -1;
    FormatEx(map,sizeof(map),"maps/%s.bsp",changedMap);
    return changedMap[0]&&StrEqual(path,map)?validationSize:32;
}
int ProbeFileTime(const char[] path, FileTimeMode mode)
{
    #pragma unused mode
    char map[64];FormatEx(map,sizeof(map),"maps/%s.bsp",changedMap);
    return changedMap[0]&&StrEqual(path,map)?validationStamp:1;
}
bool ProbeMapValid(const char[] map) { return StrContains(map,"missing_") != 0; }
void ProbeLog(const char[] format, any ...)
{
    #pragma unused format
}
void UpdateCurrentMap() {}
bool RecentMapHistory_Contains(const char[] map) { return excluded[0] && StrEqual(excluded,map); }
void CreateNextVote() { g_NextMapList.Clear(); PopulateNextVoteFromMapEval(); }
bool IsMapVoteSpecialItem(const char[] map) { return StrContains(map,"##") == 0; }
bool IsMapEnabledForNomination(const char[] map) { int status; return g_MapTrie.GetValue(map,status) && status==1; }

#define BuildPath ProbeBuildPath
#define FindConVar ProbeFindConVar
#define FindMap ProbeFindMap
#define FileSize ProbeFileSize
#define GetFileTime ProbeFileTime
#define IsMapValid ProbeMapValid
#define LogMessage ProbeLog
#include "nativevotes_mapeval.inc"
#undef BuildPath
#undef FindConVar
#undef FindMap
#undef FileSize
#undef GetFileTime
#undef IsMapValid
#undef LogMessage

// Direct adapters for the menu factories; production uses the equivalent natives.
bool NativeVotes_GetCachedMapDisplayName(const char[] map,char[] output,int maxlen)
{
    if(g_CatalogDisplayNames!=null&&g_CatalogDisplayNames.GetString(map,output,maxlen))return true;
    Catalog_FormatResolvedDisplayName(map,output,maxlen);return true;
}
int NativeVotes_CopyMapCategories(ArrayList output)
{
    output.Clear(); char category[64];
    for(int i=0;i<g_CatalogCategories.Length;i++) {g_CatalogCategories.GetString(i,category,sizeof(category));output.PushString(category);}
    return output.Length;
}
int NativeVotes_CopyMapCatalog(ArrayList output,const char[] category="")
{
    output.Clear(); ArrayList source=g_CatalogMaps; int value;
    if(StrEqual(category,"popular maps"))source=g_CatalogPopular;
    else if(category[0]) {if(!g_CatalogGroups.GetValue(category,value))return 0;source=view_as<ArrayList>(value);}
    char map[PLATFORM_MAX_PATH];
    for(int i=0;i<source.Length;i++){source.GetString(i,map,sizeof(map));output.PushString(map);}
    return output.Length;
}
bool NativeVotes_ReloadMapCatalog(){return LoadMapEvalConfig();}
void BuildMapMenu()
{
    menuBuilds++;g_MapTrie.Clear();char map[PLATFORM_MAX_PATH];
    for(int i=0;i<g_MapList.Length;i++){g_MapList.GetString(i,map,sizeof(map));g_MapTrie.SetValue(map,1);}
}
void SyncNominatedMapStatuses(){}
#include "catalog_reload_under_test.inc"
public void NativeVotesProbe_OnCatalogReloaded()
{
    if(g_CatalogMenuReady)ReloadNominationMapList(false);
}
public int MenuHandler_CategorySelect(Menu menu, MenuAction action, int client, int item) {return 0;}
public int MenuHandler_MapSelect(Menu menu, MenuAction action, int client, int item) {return 0;}
#define FindMap ProbeFindMap
#include "catalog_nomination_under_test.inc"
#undef FindMap
#include "catalog_bonus_under_test.inc"

void Check(bool ok,const char[] label)
{
    checks++;
    if(!ok){failures++;PrintToServer("[MapCatalogProbe] FAIL: %s",label);}
}
void WriteSection(File file, const char[] name)
{
    file.WriteLine("\"%s\"", name);
    file.WriteLine("{");
}
void WriteEntry(File file, const char[] name, const char[] value)
{
    file.WriteLine("\"%s\" \"%s\"", name, value);
}
void WritePrimary(int mode=0)
{
    File f=OpenFile(PRIMARY,"w");
    WriteSection(f,"mapEval");
    if(mode==1)
    {
        WriteSection(f,"popular maps");WriteEntry(f,"level3","");f.WriteLine("}");
    }
    else if(mode==2)
    {
        WriteSection(f,"koth");WriteEntry(f,"zero","0");f.WriteLine("}");
    }
    else if(mode==3)
    {
        WriteSection(f,"koth");WriteSection(f,"legacy_tier");
        WriteEntry(f,"zero","1");f.WriteLine("}");f.WriteLine("}");
    }
    else if(mode==4) WriteSection(f,"koth");
    else if(mode==5 || mode==6)
    {
        WriteSection(f,mode==5?"KOTH":"kOtH");WriteEntry(f,"level1","1");f.WriteLine("}");
        WriteSection(f,mode==5?"CTF":"cTf");WriteEntry(f,"level2","2");f.WriteLine("}");
    }
    else
    {
        WriteSection(f,"koth");
        WriteEntry(f,"zero","0");WriteEntry(f,"level1","1");WriteEntry(f,"unset","");f.WriteLine("}");
        WriteSection(f,"control points");
        WriteEntry(f,"level2","2");WriteEntry(f,"level3","3");WriteEntry(f,"missing_map","3");f.WriteLine("}");
        WriteSection(f,"popular maps");WriteEntry(f,"zero","999");WriteEntry(f,"level3","0");f.WriteLine("}");
    }
    f.WriteLine("}");delete f;
}

public void OnPluginStart()
{
    BuildPath(Path_SM,probePrimaryAbsolute,sizeof(probePrimaryAbsolute),"data/nativevotes_catalog_probe_primary.cfg");
    g_MapList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_NextMapList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_RandomNominationList=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_MapTrie=new StringMap();
    g_CatalogMaps=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogCategories=new ArrayList(ByteCountToCells(64));
    g_CatalogPopular=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    g_CatalogGroups=new StringMap();g_MapEvalVoteWeights=new StringMap();
    g_CatalogReloaded=new GlobalForward("NativeVotesProbe_OnCatalogReloaded",ET_Ignore);
    g_ConVars[mapvote_include]=CreateConVar("nativevotes_probe_include","2");
    probeCycle=CreateConVar("nativevotes_probe_cycle","nativevotes_catalog_probe_cycle.txt");
    RegAdminCmd("sm_nativevotes_catalog_probe",RunProbe,ADMFLAG_ROOT);
}

public Action RunProbe(int client,int args)
{
    checks=failures=0;
    File f=OpenFile(CYCLE,"w");f.WriteLine("fallback_a");f.WriteLine("fallback_b");f.WriteLine("fallback_a");delete f;
    Check(ParseMapEvalVoteWeight("")==0,"undefined level is zero");
    Check(ParseMapEvalVoteWeight("0")==0,"explicit zero");
    Check(ParseMapEvalVoteWeight("-1")==0,"legacy demotion is nomination-only");
    Check(ParseMapEvalVoteWeight("nope")==0,"invalid value has no eligibility");
    Check(ParseMapEvalVoteWeight("1")==1 && ParseMapEvalVoteWeight("2")==2 && ParseMapEvalVoteWeight("3")==3,"all supported levels");
    Check(ParseMapEvalVoteWeight("99")==3,"high level clamp");
    WritePrimary();Check(LoadMapEvalConfig() && g_CatalogPrimary,"valid primary loads");
    Check(g_MapList.Length==5,"all installed levels are nominatable; missing map filtered");
    Check(g_CatalogCategories.Length==2,"popular is not a gamemode");
    Check(g_CatalogPopular.Length==2,"popular view retained");
    int before = resolutionCalls;
    Check(Catalog_EnsureLoaded() && resolutionCalls==before,"vote uses existing catalog without engine resolution");
    ArrayList cachedPool=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
    Catalog_BuildRandomPool(cachedPool);delete cachedPool;
    Check(resolutionCalls==before,"random pool trusts validated catalog snapshot");
    LoadMapEvalConfig();
    Check(resolutionCalls==before+1,"unchanged refresh only retries previously missing map");
    strcopy(changedMap,sizeof(changedMap),"level1");validationSize=64;
    before=resolutionCalls;LoadMapEvalConfig();
    Check(resolutionCalls==before+2,"changed BSP size requires engine revalidation");
    validationStamp=2;before=resolutionCalls;LoadMapEvalConfig();
    Check(resolutionCalls==before+2,"same-size BSP modification requires revalidation");
    strcopy(removedMap,sizeof(removedMap),"level2");LoadMapEvalConfig();
    Check(g_MapList.FindString("level2")==-1,"refresh removes a deleted BSP instead of trusting cached validity");
    removedMap[0]=changedMap[0]='\0';validationSize=32;validationStamp=1;LoadMapEvalConfig();
    char cachedDisplay[PLATFORM_MAX_PATH];before=resolutionCalls;
    Check(Catalog_GetMapDisplayName("level3",cachedDisplay,sizeof(cachedDisplay))&&StrEqual(cachedDisplay,"level3")&&resolutionCalls==before,"cached display name requires no engine resolution");
    Catalog_FormatResolvedDisplayName("workshop/cp_example.ugc123",cachedDisplay,sizeof(cachedDisplay));
    Check(StrEqual(cachedDisplay,"cp_example"),"cached display formatting preserves workshop behavior");
    Catalog_FormatResolvedDisplayName("folder\\cp_example.ugc123",cachedDisplay,sizeof(cachedDisplay));
    Check(StrEqual(cachedDisplay,"cp_example"),"cached display formatting supports backslash paths");
    Check(Catalog_WorkshopFilePath("workshop/pl_example.ugc123",440,cachedDisplay,sizeof(cachedDisplay))
        &&StrEqual(cachedDisplay,"../steamapps/workshop/content/440/123/pl_example.bsp"),"Workshop fingerprint uses actual Steam content file");
    Check(!Catalog_WorkshopFilePath("workshop/pl_example.ugc../123",440,cachedDisplay,sizeof(cachedDisplay)),"Workshop ID must contain only digits");
    Check(!Catalog_WorkshopFilePath("workshop/../pl_example.ugc123",440,cachedDisplay,sizeof(cachedDisplay)),"Workshop name cannot traverse directories");
    Check(GetMapEvalLevel("zero")==0 && GetMapEvalLevel("unset")==0,"popular values never set map ratings");
    Check(GetMapEvalStartingVotes("zero")==0 && GetMapEvalStartingVotes("level1")==0,"0/1 have no bonus");
    Check(GetMapEvalStartingVotes("level2")==1 && GetMapEvalStartingVotes("level3")==2,"2/3 have +1/+2 bonus");
    Check(GetMapEvalStartingVotes("##extend##")==0,"special option gets no bonus");
    ArrayList pool=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));Catalog_BuildRandomPool(pool);
    Check(pool.Length==3 && pool.FindString("zero")==-1 && pool.FindString("unset")==-1,"automatic random excludes level 0");
    strcopy(g_CurrentMap,sizeof(g_CurrentMap),"level1");strcopy(excluded,sizeof(excluded),"level2");Catalog_BuildRandomPool(pool);
    Check(pool.Length==1 && pool.FindString("level3")==0,"current and recent excluded");
    g_CurrentMap[0]=excluded[0]='\0';
    char map[PLATFORM_MAX_PATH];
    for(int i=0;i<100;i++){Check(CopyRandomMapEvalMap(map,sizeof(map)) && GetMapEvalLevel(map)>=1,"empty-server picker uses rated catalog");}
    CreateNextVote();Check(g_NextMapList.Length==2,"vote option limit and rated pool");
    g_NextMapList.GetString(0,map,sizeof(map));Check(g_NextMapList.FindString(map)==0,"vote picks have no duplicate first entry");

    Menu root=BuildNominationCategoryMenu();char key[64];root.GetItem(0,key,sizeof(key));Check(StrEqual(key,"popular maps"),"Popular first");
    root.GetItem(1,key,sizeof(key));Check(StrEqual(key,"@random"),"Random 50 second");
    char label[128];int style;
    root.GetTitle(label,sizeof(label));Check(StrEqual(label,"Nominate a map"),"root title has no Categories suffix");
    root.GetItem(2,key,sizeof(key),style,label,sizeof(label));
    Check(StrEqual(label,"koth"),"category capitalization comes directly from config");delete root;
    Menu categoryMenu=BuildCategoryNominationMenu("control points");
    Check(categoryMenu!=null,"category retains exact file spelling for lookup");
    if(categoryMenu!=null)
    {
        categoryMenu.GetTitle(label,sizeof(label));
        Check(StrEqual(label,"Nominate: control points"),"submenu title preserves config capitalization");delete categoryMenu;
    }
    Menu popular=BuildCategoryNominationMenu("popular maps");popular.GetItem(0,key,sizeof(key));Check(StrEqual(key,"zero"),"popular order includes level 0");delete popular;
    for(int c=0;c<9;c++)
    {
        FormatEx(key,sizeof(key),"extra%d",c);ArrayList maps=new ArrayList(ByteCountToCells(PLATFORM_MAX_PATH));
        for(int i=0;i<23;i++){FormatEx(map,sizeof(map),"map%d_%d",c,i);maps.PushString(map);}
        g_CatalogGroups.SetValue(key,view_as<int>(maps));g_CatalogCategories.PushString(key);
        Menu menu=BuildCategoryNominationMenu(key);
        Check(menu.ItemCount==23 && menu.Pagination>0 && menu.Pagination<menu.ItemCount && menu.ExitBackButton,"category pagination and back navigation");delete menu;
    }
    root=BuildNominationCategoryMenu();Check(root.ItemCount==13 && root.Pagination>0 && root.Pagination<root.ItemCount,"root category pagination");delete root;
    Check(BuildCategoryNominationMenu("missing_category")==null,"stale category handled safely");

    g_MapList.Clear();g_MapTrie.Clear();
    for(int i=0;i<75;i++){FormatEx(map,sizeof(map),"random%d",i);g_MapList.PushString(map);g_MapTrie.SetValue(map,i<5?2:1);}
    BuildRandomNominationList();Check(g_RandomNominationList.Length==50,"existing nr returns Random 50");
    StringMap seen=new StringMap();
    for(int i=0;i<g_RandomNominationList.Length;i++)
    {
        g_RandomNominationList.GetString(i,map,sizeof(map));int value;
        Check(IsMapEnabledForNomination(map) && !seen.GetValue(map,value),"nr excludes disabled and duplicate maps");seen.SetValue(map,1);
    }
    delete seen;
    DeleteFile(PRIMARY);Check(LoadMapEvalConfig() && !g_CatalogPrimary && g_MapList.Length==2,"missing primary uses cycle fallback and deduplicates");
    Check(g_CatalogPopular.Length==0 && GetMapEvalLevel("fallback_a")==1,"fallback clears views and retains random eligibility");
    WritePrimary(1);Check(LoadMapEvalConfig() && !g_CatalogPrimary,"popular-only invalid catalog falls back");
    WritePrimary(3);Check(LoadMapEvalConfig() && !g_CatalogPrimary,"nested invalid catalog falls back");
    WritePrimary(4);Check(LoadMapEvalConfig() && !g_CatalogPrimary,"syntactically malformed catalog falls back");
    WritePrimary(2);Check(LoadMapEvalConfig() && g_CatalogPrimary,"all-zero primary remains valid");
    CreateNextVote();Check(g_NextMapList.Length==0 && !CopyRandomMapEvalMap(map,sizeof(map)),"no fallback to level zero for automatic votes or empty changes");
    WritePrimary(5);LoadMapEvalConfig();
    char first[64],second[64];g_CatalogCategories.GetString(0,first,sizeof(first));g_CatalogCategories.GetString(1,second,sizeof(second));
    Check(StrEqual(first,"KOTH") && StrEqual(second,"CTF"),"case-only refresh preserves uppercase config names after lowercase names were interned");
    WritePrimary(6);LoadMapEvalConfig();g_CatalogCategories.GetString(0,first,sizeof(first));g_CatalogCategories.GetString(1,second,sizeof(second));
    Check(StrEqual(first,"kOtH") && StrEqual(second,"cTf"),"second case-only refresh preserves mixed-case config names");
    WritePrimary();Check(LoadMapEvalConfig() && g_CatalogPopular.Length==2 && GetMapEvalLevel("level3")==3,"refresh restores levels/categories/popular together");
    menuBuilds=0;g_CatalogMenuReady=false;
    Check(ReloadNominationMapList(false)&&menuBuilds==1,"nomination consumer initializes once");
    Check(ReloadNominationMapList(true)&&menuBuilds==2,"explicit refresh rebuilds consumer once, not through both forward and caller");
    g_CatalogMenuReady=false;
    delete pool;
    File out=OpenFile("addons/sourcemod/data/nativevotes_catalog_probe_result.txt","w");out.WriteLine("checks=%d failures=%d",checks,failures);delete out;
    PrintToServer("[MapCatalogProbe] checks=%d failures=%d",checks,failures);
    return Plugin_Handled;
}

public void OnPluginEnd() {DeleteFile(PRIMARY);DeleteFile(CYCLE);}
