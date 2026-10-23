#if defined _nativevotes_map_cache_included
    #endinput
#endif
#define _nativevotes_map_cache_included

// Catalog validation is expensive in the engine. Retain only successful exact
// resolutions whose BSP metadata has not changed. Aliases are resolved again
// on explicit refresh, so a newly installed exact match can supersede one.
StringMap g_CatalogValidatedFiles;
int g_CatalogWorkshopAppId;

bool Catalog_WorkshopFilePath(const char[] map, int appId, char[] path, int maxlen)
{
    int suffix = StrContains(map, ".ugc");
    if (appId <= 0 || StrContains(map, "workshop/") != 0 || suffix <= 9 || !map[suffix + 4])
        return false;
    for (int i = suffix + 4; map[i]; i++)
        if (map[i] < '0' || map[i] > '9') return false;
    char name[PLATFORM_MAX_PATH];
    strcopy(name, sizeof(name), map[9]);
    name[suffix - 9] = '\0';
    if (StrContains(name, "/") != -1 || StrContains(name, "\\") != -1 || StrContains(name, "..") != -1)
        return false;
    FormatEx(path, maxlen, "../steamapps/workshop/content/%d/%s/%s.bsp", appId, map[suffix + 4], name);
    return true;
}

void Catalog_ResetMapValidationCache()
{
    if (g_CatalogValidatedFiles == null) g_CatalogValidatedFiles = new StringMap();
    else g_CatalogValidatedFiles.Clear();
}

bool Catalog_ReadMapSignature(const char[] map, int signature[2])
{
    char path[PLATFORM_MAX_PATH];
    FormatEx(path, sizeof(path), "maps/%s.bsp", map);
    // Native stat is cheap; Valve Size() can open/resolve the BSP through all
    // search paths and is not an appropriate hot refresh metadata probe.
    signature[0] = FileSize(path);
    if (signature[0] < 0 && StrContains(map, "workshop/") == 0)
    {
        if (g_CatalogWorkshopAppId == 0)
        {
            KeyValues info = new KeyValues("GameInfo");
            g_CatalogWorkshopAppId = -1;
            if (info.ImportFromFile("gameinfo.txt") && info.JumpToKey("FileSystem"))
                g_CatalogWorkshopAppId = info.GetNum("SteamAppId", -1);
            delete info;
        }
        if (Catalog_WorkshopFilePath(map, g_CatalogWorkshopAppId, path, sizeof(path)))
            signature[0] = FileSize(path);
    }
    if (signature[0] < 0 && StrContains(map, "workshop/") != 0)
        signature[0] = FileSize(path, true, "GAME"); // Packed/search-path-only maps.
    signature[1] = GetFileTime(path, FileTime_LastChange);
    return signature[0] >= 0;
}

bool Catalog_ResolveInstalledMap(const char[] map, char[] resolved, int maxlen)
{
    if (g_CatalogValidatedFiles == null) Catalog_ResetMapValidationCache();
    int saved[2], current[2];
    if (g_CatalogValidatedFiles.GetArray(map, saved, sizeof(saved))
        && Catalog_ReadMapSignature(map, current)
        && saved[0] == current[0] && saved[1] == current[1])
    {
        strcopy(resolved, maxlen, map);
        return true;
    }
    if (FindMap(map, resolved, maxlen) == FindMap_NotFound)
    {
        g_CatalogValidatedFiles.Remove(map);
        return false;
    }
    if (Catalog_ReadMapSignature(resolved, current))
        g_CatalogValidatedFiles.SetArray(resolved, current, sizeof(current));
    return true;
}

void Catalog_FormatResolvedDisplayName(const char[] resolved, char[] display, int maxlen)
{
    // Match SourceMod GetMapDisplayName's formatting, without revalidating an
    // already canonical name through FindMap for the second time.
    int separator = FindCharInString(resolved, '/', true);
    if (separator == -1) separator = FindCharInString(resolved, '\\', true);
    strcopy(display, maxlen, resolved[separator + 1]);
    int suffix = StrContains(display, ".ugc");
    if (suffix != -1) display[suffix] = '\0';
}
