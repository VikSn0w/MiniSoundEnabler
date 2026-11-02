/*
 *  VolumeLogic.h  —  Sound Manager volume values (no Toolbox calls)
 *
 *  A volume is right:left, 16 bits each; 0x0100 is unity (full).
 */
#pragma once
#include <MacTypes.h>

#define kVolumeUnity    0x0100

UInt32 VolumeClamp(UInt32 rightLeft);   /* each half min(half, 0x0100) */

/* Component flags for the wrapper, from the Built-in driver's flags:
   everything except cmpWantsRegisterMessage, which the wrapper can't
   answer until it is attached */
#define kCmpWantsRegisterMessage    0x80000000
UInt32 WrapperComponentFlags(UInt32 builtInFlags);

/* Gain actually applied to the samples: main (hardware) volume times the
   plain volume SysBeep sets around a beep, per channel; 0 when muted. */
UInt32 VolumeCombine(UInt32 hwVolume, UInt32 volume, Boolean muted);

/* Scale interleaved 16-bit samples in place. Left channel (and mono)
   uses the low word of rightLeft, right channel the high word. */
void VolumeScale16(SInt16 *samples, UInt32 frames, UInt16 channels,
                   UInt32 rightLeft);
