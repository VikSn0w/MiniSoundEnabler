/*
 *  MacIO.h  —  mac-io (Intrepid) access for the Mac Mini G4
 */
#pragma once
#include <MacTypes.h>
#include <NameRegistry.h>

extern UInt32 gMacIOBase;

OSStatus MacIOFind(void);                   /* sets gMacIOBase */
UInt8    MacIORead8(UInt32 offset);
void     MacIOWrite8(UInt32 offset, UInt8 value);
UInt32   MacIORead32LE(UInt32 offset);      /* 32-bit load, byte-swapped */
