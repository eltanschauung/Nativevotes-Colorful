#if defined _nativevotes_catalog_api_included
    #endinput
#endif
#define _nativevotes_catalog_api_included

// MapChooser is the sole catalog owner. No consumer independently parses files.
native bool NativeVotes_ReloadMapCatalog();
native int NativeVotes_CopyMapCatalog(ArrayList maps, const char[] category = "");
native int NativeVotes_CopyMapCategories(ArrayList categories);
native void NativeVotes_GetMapCatalogSource(char[] source, int maxlen);
native bool NativeVotes_GetCatalogMapDisplayName(const char[] map, char[] display, int maxlen);
forward void NativeVotes_OnMapCatalogReloaded();

stock bool NativeVotes_GetCachedMapDisplayName(const char[] map, char[] display, int maxlen)
{
    if (NativeVotes_GetCatalogMapDisplayName(map, display, maxlen)) return true;
    return GetMapDisplayName(map, display, maxlen);
}
