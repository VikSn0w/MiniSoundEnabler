/*
 *  AmpLogic.h  —  speaker/headphone amp rules (no Toolbox calls)
 *
 *  Pure logic shared by the INIT's Time Manager task and the host
 *  unit tests: headphone debounce, which amp pins should be on, and
 *  correcting pins through read/write callbacks.
 */
#pragma once
#include <MacTypes.h>

/* GPIO offsets from mac-io base (Mac Mini G4, Intrepid). The device
   tree names these three as the audio pins ("headphone-detect",
   "headphone-mute", "amp-mute"). Up to 3.0 the extension also forced
   0x6E and 0x79 on. They aren't audio pins (sound played with either
   one off), and forcing them is the likely cause of FireWire drives
   not working (MacLegacyLabs issue 1), so they are no longer touched. */
enum { kPinHPDetect = 0x67, kPinHeadphone = 0x6F, kPinSpeaker = 0x70 };

/* bit 0 = output enable, bit 2 = data HIGH */
enum { kAmpOn = 0x05, kAmpOff = 0x04 };

typedef UInt8 (*AmpReadFn)(UInt32 offset);
typedef void  (*AmpWriteFn)(UInt32 offset, UInt8 value);

typedef struct {
    UInt8 stable;       /* debounced state, 1 = headphones in */
    UInt8 candidate;    /* last raw state seen */
    UInt8 count;        /* consecutive reads of candidate */
} AmpDebounce;

void    AmpDebounceInit(AmpDebounce *d, UInt8 detectReg);
Boolean AmpDebounceUpdate(AmpDebounce *d, UInt8 detectReg);
Boolean AmpHeadphonesIn(UInt8 detectReg);
/* silent: muted or volume 0 - both outputs off, so Mute works in hardware */
/* pin: kPinSpeaker or kPinHeadphone */
UInt8   AmpDesired(UInt32 pin, Boolean headphonesIn, Boolean silent);
UInt32  AmpApply(Boolean headphonesIn, Boolean silent, AmpReadFn rd, AmpWriteFn wr);
