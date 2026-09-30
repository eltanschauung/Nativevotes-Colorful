#if defined _nativevotes_performance_included
 #endinput
#endif
#define _nativevotes_performance_included
#tryinclude <kogasa_performance>
#if !defined _kogasa_performance_included
stock int KogasaPerfBegin() { return -1; }
stock void KogasaPerfEnd(int slot, const char[] scope)
{
    #pragma unused slot, scope
}
#endif
