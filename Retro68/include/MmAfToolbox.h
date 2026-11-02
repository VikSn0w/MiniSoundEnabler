/*
 *  MmAfToolbox.h  —  stand-ins for Retro68
 *  Component Manager, Gestalt and Sound component declarations the
 *  Multiversal Interfaces lack. Types and values follow Apple's
 *  Universal Interfaces; InterfaceLib and SoundLib export the calls.
 */
#pragma once
#include <MacTypes.h>
#include <Multiverse.h>

#ifndef __OSSTATUS_DEFINED
#define __OSSTATUS_DEFINED
typedef SInt32 OSStatus;
#endif

/* ---- Component Manager ---- */

typedef SInt32 ComponentResult;

typedef struct ComponentDescription {
    OSType componentType;
    OSType componentSubType;
    OSType componentManufacturer;
    UInt32 componentFlags;
    UInt32 componentFlagsMask;
} ComponentDescription;

#pragma pack(push, 2)
typedef struct ComponentParameters {
    UInt8  flags;
    UInt8  paramSize;
    SInt16 what;
    SInt32 params[1];   /* arguments in reverse order, last first */
} ComponentParameters;
#pragma pack(pop)

typedef UniversalProcPtr ComponentRoutineUPP;

enum {
    registerComponentGlobal = 1,
    kComponentOpenSelect    = -1,
    kComponentCloseSelect   = -2
};

/* pascal ComponentResult (*)(ComponentParameters *, Handle storage) */
enum { uppComponentRoutineProcInfo = 0x000003F0 };

Component FindNextComponent(Component aComponent, ComponentDescription *looking);
OSErr GetComponentInfo(Component aComponent, ComponentDescription *cd,
                       Handle componentName, Handle componentInfo,
                       Handle componentIcon);
long CountComponentInstances(Component aComponent);
Component RegisterComponent(ComponentDescription *cd,
                            ComponentRoutineUPP componentEntryPoint,
                            short global, Handle componentName,
                            Handle componentInfo, Handle componentIcon);
OSErr UnregisterComponent(Component aComponent);
Component CaptureComponent(Component capturedComponent,
                           Component capturingComponent);
OSErr UncaptureComponent(Component aComponent);
ComponentInstance OpenComponent(Component aComponent);
OSErr CloseComponent(ComponentInstance aComponentInstance);
void SetComponentInstanceStorage(ComponentInstance aComponentInstance,
                                 Handle theStorage);
/* both also accept a component instance in place of the component */
long GetComponentRefcon(Component aComponent);
void SetComponentRefcon(Component aComponent, long theRefcon);
ComponentResult DelegateComponentCall(ComponentParameters *originalParams,
                                      ComponentInstance ci);

/* ---- Gestalt (Mac OS 8.5+) ---- */

OSErr NewGestaltValue(OSType selector, long newValue);

/* ---- Time Manager ---- */

/* void (*)(TMTask *) with the task in A1 */
enum { uppTimerProcInfo = 0x0000B802 };

/* ---- Sound components ---- */

typedef struct SoundSourceRecord *SoundSource;

enum {
    kSoundComponentSetSourceSelect     = 0x0002,
    kSoundComponentGetSourceSelect     = 0x0003,
    kSoundComponentGetSourceDataSelect = 0x0004,
    kSoundComponentGetInfoSelect       = 0x0103,
    kSoundComponentSetInfoSelect       = 0x0104
};

#define siVolume            'volu'
#define siHardwareVolume    'hvol'
#define siHardwareMute      'hmut'

/* What a sound source (the mixer) hands its output in GetSourceData */
typedef struct SoundComponentData {
    long    flags;
    OSType  format;         /* 'twos' = signed big-endian PCM */
    short   numChannels;
    short   sampleSize;     /* bits */
    UInt32  sampleRate;     /* UnsignedFixed */
    long    sampleCount;    /* frames */
    Byte   *buffer;
    long    reserved;
} SoundComponentData, *SoundComponentDataPtr;

ComponentResult SoundComponentSetSource(ComponentInstance ti, SoundSource sourceID,
                                        ComponentInstance source);
ComponentResult SoundComponentGetSource(ComponentInstance ti, SoundSource sourceID,
                                        ComponentInstance *source);

ComponentResult SoundComponentGetInfo(ComponentInstance ti, SoundSource sourceID,
                                      OSType selector, void *infoPtr);
ComponentResult SoundComponentSetInfo(ComponentInstance ti, SoundSource sourceID,
                                      OSType selector, void *infoPtr);
