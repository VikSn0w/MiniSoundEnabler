/*
 *  MacIO.c  —  mac-io (Intrepid) access for the Mac Mini G4
 *
 *  The base address comes from the Name Registry. GPIO pins are single
 *  bytes; FCR and I2S registers are 32-bit little-endian and must be
 *  read with 32-bit loads.
 *
 *  Based on MacMiniAudioFix by Matouš Pikous (MacLegacyLabs, GPLv3).
 */
#include <MacTypes.h>
#include <NameRegistry.h>
#include <DriverServices.h>
#include "MacIO.h"

UInt32 gMacIOBase = 0;

UInt8 MacIORead8(UInt32 offset)
{
    volatile UInt8 *a;
    UInt8 v;
    a = (volatile UInt8 *)(gMacIOBase + offset);
    v = *a;
    SynchronizeIO();
    return v;
}

void MacIOWrite8(UInt32 offset, UInt8 value)
{
    volatile UInt8 *a;
    a = (volatile UInt8 *)(gMacIOBase + offset);
    *a = value;
    SynchronizeIO();
}

UInt32 MacIORead32LE(UInt32 offset)
{
    UInt32 v = *(volatile UInt32 *)(gMacIOBase + offset);
    SynchronizeIO();
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

OSStatus MacIOFind(void)
{
    RegEntryID      entry;
    RegEntryIter    cookie;
    Boolean         done = false;
    OSStatus        err;
    UInt32          regProp[20];
    RegPropertyValueSize sz;

    err = RegistryEntryIterateCreate(&cookie);
    if (err != noErr) return err;
    err = RegistryEntrySearch(&cookie, kRegIterSubTrees,
                               &entry, &done,
                               "device_type", "mac-io", 7);
    if (err != noErr || done) {
        RegistryEntryIterateDispose(&cookie);
        err = RegistryEntryIterateCreate(&cookie);
        if (err != noErr) return err;
        err = RegistryEntrySearch(&cookie, kRegIterSubTrees,
                                   &entry, &done,
                                   "name", "mac-io", 6);
    }
    RegistryEntryIterateDispose(&cookie);
    if (err != noErr || done) return -1;

    sz = sizeof(regProp);
    err = RegistryPropertyGet(&entry, "assigned-addresses",
                               regProp, &sz);
    if (err != noErr) {
        sz = sizeof(regProp);
        err = RegistryPropertyGet(&entry, "reg", regProp, &sz);
        if (err != noErr) return err;
    }
    gMacIOBase = regProp[2];
    return (gMacIOBase != 0) ? noErr : (OSStatus)-1;
}
