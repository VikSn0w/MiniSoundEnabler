/*
 *  MachineLogic.c  —  is this a Mac mini G4?  (pure, host-tested)
 */
#include "MachineLogic.h"

/* "PowerMac10," followed by a digit, anywhere in value[0..len) */
static Boolean NamesMacMini(const char *value, UInt32 len)
{
    static const char kKey[] = "PowerMac10,";
    const UInt32 keyLen = sizeof(kKey) - 1;
    UInt32 i, j;

    if (!value) return false;
    for (i = 0; i + keyLen < len; i++) {
        for (j = 0; j < keyLen && value[i + j] == kKey[j]; j++)
            ;
        if (j == keyLen && value[i + keyLen] >= '0' && value[i + keyLen] <= '9')
            return true;
    }
    return false;
}

Boolean MachineIsMacMiniG4(const char *bootRomModel, UInt32 bootRomLen,
                           const char *rootModel, UInt32 rootLen)
{
    return NamesMacMini(bootRomModel, bootRomLen)
        || NamesMacMini(rootModel, rootLen);
}
