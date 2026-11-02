/*
 *  AmpLogic.h  —  speaker/headphone amp rules (no Toolbox calls)
 *
 *  Pure logic shared by the INIT's Time Manager task and the host
 *  unit tests: headphone debounce, which amp pins should be on, and
 *  correcting pins through read/write callbacks.
 */
#pragma once
#include <MacTypes.h>

/* GPIO offsets from mac-io base (Mac Mini G4, Intrepid) */
enum { kPinHPDetect = 0x67, kPinAmp6E = 0x6E, kPinHeadphone = 0x6F,
       kPinSpeaker = 0x70, kPinAmp79 = 0x79 };

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
UInt8   AmpDesired(UInt32 pin, Boolean headphonesIn, Boolean silent);
UInt32  AmpApply(Boolean headphonesIn, Boolean silent, AmpReadFn rd, AmpWriteFn wr);
