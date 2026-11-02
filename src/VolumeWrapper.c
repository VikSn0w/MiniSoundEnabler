/*
 *  VolumeWrapper.c  —  software volume for the Built-in sound output
 *
 *  The Built-in output ('sdev'/'awgc'/'appl') answers every volume
 *  request with badComponentSelector, so SysBeep reads the volume as
 *  zero and flashes the menu bar instead of beeping. This registers a
 *  component with the same description, captures 'awgc' behind it and
 *  forwards every call to it, except GetInfo (and, stage 3, SetInfo)
 *  for 'volu' (plain volume, set by SysBeep around a beep), 'hvol' (main
 *  volume: Sound control panel, Control Strip, GetDefaultOutputVolume)
 *  and 'hmut' (mute), which are answered and stored here, each on its
 *  own. That makes alerts play and the volume controls keep their
 *  settings.
 *
 *  The stored volume is not applied to the audio. The driver pulls
 *  straight from an Apple Mixer instance the Sound Manager opens itself,
 *  ignores SetSource, and that mixer only takes volume per channel
 *  (TECHNICAL.md, "Volume control").
 *
 *  The dispatch routine can be called at interrupt time; only Open and
 *  Close allocate or call the Component Manager.
 */
#include <MacTypes.h>
#include <Memory.h>
#include <MmAfToolbox.h>
#include "VolumeLogic.h"
#include "VolumeWrapper.h"

#define kSoundOutputDeviceType  'sdev'
#define kBuiltInSubType         'awgc'
#define kAppleManufacturer      'appl'

#define kBadComponentInstance   ((ComponentResult)0x80008001)
#define kInvalidComponentID     (-3000)

#ifndef MMAF_STAGE
#define MMAF_STAGE 3
#endif

typedef struct {
    ComponentInstance  inner;       /* the captured Built-in driver */
} WrapperStorage;

static MmAfStatus  *gStatus;
static Component    gBuiltIn;   /* original 'awgc' */
static Component    gCaptured;  /* what CaptureComponent gave back */
static Component    gWrapper;   /* our registration */
static Boolean      gAttached;      /* capture done; set at the end of attach */
static Boolean      gSteppedAside;  /* captured driver failed: pass-through only */

static ComponentResult WrapperOpen(ComponentInstance self)
{
    Handle storage;
    WrapperStorage *s;
    ComponentInstance inner = NULL;

    if (!gAttached)
        return kInvalidComponentID;
    if (!gSteppedAside) {
        inner = OpenComponent(gCaptured);
        if (!inner) {
            /* Release the driver and stop handling volume. We can't
               unregister from inside our own Open, so from now on the
               wrapper just passes every call to the original driver. */
            UncaptureComponent(gBuiltIn);
            gSteppedAside = true;
            gStatus->flags = (gStatus->flags & ~kMmAfWrapper) | kMmAfSteppedAside;
            gStatus->wrapperErr = kMmAfErrInnerOpen;
        }
    }
    if (gSteppedAside)
        inner = OpenComponent(gBuiltIn);
    if (!inner)
        return kInvalidComponentID;
    storage = NewHandleSysClear(sizeof(WrapperStorage));
    if (!storage) {
        CloseComponent(inner);
        return memFullErr;
    }
    HLock(storage);
    s = (WrapperStorage *)*storage;
    s->inner = inner;
    SetComponentInstanceStorage(self, storage);
    return noErr;
}

static ComponentResult WrapperClose(Handle storage)
{
    if (storage) {
        WrapperStorage *s = (WrapperStorage *)*storage;
        if (s->inner) CloseComponent(s->inner);
        DisposeHandle(storage);
    }
    return noErr;
}

/* GetInfo for the selectors the wrapper keeps; false = not ours */
static Boolean GetVolumeInfo(OSType selector, void *info)
{
    switch (selector) {
    case siVolume:
        *(UInt32 *)info = gStatus->volume;
        gStatus->volGets++;
        return true;
    case siHardwareVolume:
        *(UInt32 *)info = gStatus->hwVolume;
        gStatus->hvolGets++;
        return true;
    case siHardwareMute:
        *(short *)info = gStatus->muted;
        return true;
    }
    return false;
}

/* SetInfo for the same selectors; the value itself is passed in infoPtr
   (AudioProbe v4.5: raw 01000100, 00100010 ...) */
static Boolean SetVolumeInfo(OSType selector, UInt32 value)
{
    switch (selector) {
    case siVolume:
        gStatus->volume = VolumeClamp(value);
        gStatus->volSets++;
        break;
    case siHardwareVolume:
        gStatus->hwVolume = VolumeClamp(value);
        gStatus->hvolSets++;
        break;
    case siHardwareMute:
        gStatus->muted = (value != 0);
        gStatus->muteSets++;
        break;
    default:
        return false;
    }
    gStatus->lastSetSel = selector;
    gStatus->lastSetRaw = value;
    return true;
}

static ComponentResult Dispatch(ComponentParameters *p, Handle storage)
{
    WrapperStorage *s;

    if (p->what == kComponentOpenSelect)
        return WrapperOpen((ComponentInstance)p->params[0]);
    if (p->what == kComponentCloseSelect)
        return WrapperClose(storage);
    if (!storage)
        return kBadComponentInstance;
    s = (WrapperStorage *)*storage;
    if (gSteppedAside)
        return DelegateComponentCall(p, s->inner);

    switch (p->what) {
    case kSoundComponentGetInfoSelect:
        /* GetInfo(ti, sourceID, selector, infoPtr): reversed in params */
        if (GetVolumeInfo((OSType)p->params[1], (void *)p->params[0]))
            return noErr;
        break;
#if MMAF_STAGE >= 3
    case kSoundComponentSetInfoSelect:
        /* SetInfo(ti, sourceID, selector, infoPtr) */
        if (SetVolumeInfo((OSType)p->params[1], (UInt32)p->params[0]))
            return noErr;
        break;
#endif
    }
    return DelegateComponentCall(p, s->inner);
}

OSStatus VolumeWrapperAttach(MmAfStatus *status)
{
    ComponentDescription look, cd;
    Handle name;
    UniversalProcPtr upp;
    THz saveZone;
    OSErr err;

    gStatus = status;

    look.componentType = kSoundOutputDeviceType;
    look.componentSubType = kBuiltInSubType;
    look.componentManufacturer = kAppleManufacturer;
    look.componentFlags = 0;
    look.componentFlagsMask = 0;
    gBuiltIn = FindNextComponent(NULL, &look);
    if (!gBuiltIn) return status->wrapperErr = kMmAfErrNoDriver;
    status->instancesAtInit = CountComponentInstances(gBuiltIn);

    /* name handle and routine descriptor must outlive this INIT */
    saveZone = GetZone();
    SetZone(SystemZone());
    name = NewHandleSys(0);
    upp = NewRoutineDescriptor((ProcPtr)Dispatch, uppComponentRoutineProcInfo,
                               kPowerPCISA);
    SetZone(saveZone);
    if (!name || !upp) return status->wrapperErr = memFullErr;

    err = GetComponentInfo(gBuiltIn, &cd, name, NULL, NULL);
    if (err != noErr) return status->wrapperErr = err;
    cd.componentFlags = WrapperComponentFlags(cd.componentFlags);
    cd.componentFlagsMask = 0;

    gWrapper = RegisterComponent(&cd, upp, registerComponentGlobal,
                                 name, NULL, NULL);
    if (!gWrapper) return status->wrapperErr = kMmAfErrRegister;

    gCaptured = CaptureComponent(gBuiltIn, gWrapper);
    if (!gCaptured) {
        UnregisterComponent(gWrapper);
        return status->wrapperErr = kMmAfErrCapture;
    }

    gAttached = true;
    status->flags |= kMmAfWrapper;
    return noErr;
}
