/*
 *  VolumeLogic.c  —  Sound Manager volume values (no Toolbox calls)
 */
#include "VolumeLogic.h"

UInt32 VolumeClamp(UInt32 rightLeft)
{
    UInt32 right = rightLeft >> 16;
    UInt32 left = rightLeft & 0xFFFF;

    if (right > kVolumeUnity) right = kVolumeUnity;
    if (left > kVolumeUnity) left = kVolumeUnity;
    return (right << 16) | left;
}

UInt32 WrapperComponentFlags(UInt32 builtInFlags)
{
    return builtInFlags & ~(UInt32)kCmpWantsRegisterMessage;
}

UInt32 VolumeCombine(UInt32 hwVolume, UInt32 volume, Boolean muted)
{
    UInt32 right, left;

    if (muted) return 0;
    hwVolume = VolumeClamp(hwVolume);
    volume = VolumeClamp(volume);
    right = ((hwVolume >> 16) * (volume >> 16)) >> 8;
    left = ((hwVolume & 0xFFFF) * (volume & 0xFFFF)) >> 8;
    return (right << 16) | left;
}

void VolumeScale16(SInt16 *samples, UInt32 frames, UInt16 channels,
                   UInt32 rightLeft)
{
    SInt32 left = rightLeft & 0xFFFF;
    SInt32 right = rightLeft >> 16;
    UInt32 i;

    if (channels == 2) {
        for (i = 0; i < frames; i++) {
            samples[2 * i] = (SInt16)(samples[2 * i] * left / kVolumeUnity);
            samples[2 * i + 1] = (SInt16)(samples[2 * i + 1] * right / kVolumeUnity);
        }
    } else {
        for (i = 0; i < frames * channels; i++)
            samples[i] = (SInt16)(samples[i] * left / kVolumeUnity);
    }
}
