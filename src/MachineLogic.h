/*
 *  MachineLogic.h  —  is this a Mac mini G4?  (pure, host-tested)
 */
#pragma once
#include <MacTypes.h>

/* MacOS9Lives makes the Mac mini G4 report itself as a G4 Cube
   ("PowerMac5,1") in the device tree root, so OS 9 will boot. The boot
   ROM's own "model" property keeps the real identity, for example
   "Apple PowerMac10,1 4.8.9f4 BootROM built on 03/23/05 ...". Both Mac
   mini G4 models are PowerMac10,x (10,1 and 10,2); a real Cube's boot
   ROM says PowerMac5,1.

   Each argument is a property value as read (not necessarily
   NUL-terminated) with its length, or NULL/0 if it couldn't be read.
   True if either names a PowerMac10,x. */
Boolean MachineIsMacMiniG4(const char *bootRomModel, UInt32 bootRomLen,
                           const char *rootModel, UInt32 rootLen);
