/*
 *  AmpLogic.c  —  speaker/headphone amp rules (no Toolbox calls)
 *  Safe to call at interrupt time.
 */
#include "AmpLogic.h"

/* order AmpApply visits the pins in */
static const UInt32 kAmpPins[] = { kPinAmp6E, kPinHeadphone, kPinSpeaker,
                                   kPinAmp79 };

/* A state change counts after this many identical reads in a row */
#define kDebounceReads  2

Boolean AmpHeadphonesIn(UInt8 detectReg)
{
    /* 0x67 bit 1: 1 = nothing plugged in, 0 = headphones in */
    return (detectReg & 0x02) == 0;
}

void AmpDebounceInit(AmpDebounce *d, UInt8 detectReg)
{
    d->stable = d->candidate = AmpHeadphonesIn(detectReg);
    d->count = kDebounceReads;
}

Boolean AmpDebounceUpdate(AmpDebounce *d, UInt8 detectReg)
{
    UInt8 raw = AmpHeadphonesIn(detectReg);

    if (raw != d->candidate) {
        d->candidate = raw;
        d->count = 1;
    } else if (d->count < kDebounceReads) {
        d->count++;
    }
    if (d->count >= kDebounceReads)
        d->stable = d->candidate;
    return d->stable;
}

UInt8 AmpDesired(UInt32 pin, Boolean headphonesIn, Boolean silent)
{
    switch (pin) {
    case kPinSpeaker:   return (headphonesIn || silent) ? kAmpOff : kAmpOn;
    case kPinHeadphone: return (headphonesIn && !silent) ? kAmpOn : kAmpOff;
    default:            return kAmpOn;      /* 0x6E, 0x79: left on, as in v2 */
    }
}

UInt32 AmpApply(Boolean headphonesIn, Boolean silent, AmpReadFn rd, AmpWriteFn wr)
{
    UInt32 written = 0;
    unsigned i;

    for (i = 0; i < sizeof(kAmpPins) / sizeof(kAmpPins[0]); i++) {
        UInt8 want = AmpDesired(kAmpPins[i], headphonesIn, silent);
        if ((rd(kAmpPins[i]) & 0x01) != (want & 0x01)) {
            wr(kAmpPins[i], want);
            written++;
        }
    }
    return written;
}
