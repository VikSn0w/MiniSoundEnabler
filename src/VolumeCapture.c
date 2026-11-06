/*
 *  VolumeCapture.c  —  software volume by capturing sound components
 *
 *  The Built-in driver can't apply a volume and pulls from a Mixer
 *  instance the Sound Manager opens outside any capture (TECHNICAL.md,
 *  "Volume control"). Components the Sound Manager opens per channel,
 *  after startup, can be captured: each target below is captured behind
 *  a stand-in with the same description. The stand-in forwards every
 *  call to a real instance, and scales each 16-bit buffer handed back by
 *  GetSourceData by main x plain volume (0 when muted). Scaling every
 *  channel scales the output.
 *
 *  If a captured component fails to open, the stand-in uncaptures it and
 *  from then on opens the original, as a pure pass-through.
 *
 *  GetSourceData runs at interrupt time: no allocation, only arithmetic
 *  on the buffer and DelegateComponentCall.
 */
#include <MacTypes.h>
#include <Memory.h>
#include <MmAfToolbox.h>
#include "VolumeLogic.h"
#include "VolumeCapture.h"

#define kFormatTwos             'twos'
#define kBadComponentInstance   ((ComponentResult)0x80008001)
#define kInvalidComponentID     (-3000)

/* What to capture: type, subtype (0 = any) */
static const struct { OSType type, subType; } kTargets[] = {
    { 'snfx', 'eqal' },         /* Frequency Equalizer, one per channel */
};
#define kNumTargets (sizeof(kTargets) / sizeof(kTargets[0]))

typedef struct {
    ComponentInstance real;     /* the real instance behind this stand-in */
    long index;                 /* which capture slot */
} StandInStorage;

typedef struct {
    Component original;
    Component captured;         /* what CaptureComponent gave back */
    Boolean   steppedAside;     /* captured one failed to open */
} Slot;

static MmAfStatus *gStatus;
static Slot        gSlots[kMaxCaptures];
static Boolean     gCaptureDone;

static ComponentResult StandInOpen(ComponentInstance self)
{
    long i = GetComponentRefcon((Component)self);
    ComponentInstance real = NULL;
    Handle storage;
    Slot *slot;

    if (i < 0 || i >= kMaxCaptures || !gSlots[i].original)
        return kInvalidComponentID;
    slot = &gSlots[i];
    if (!slot->steppedAside) {
        real = OpenComponent(slot->captured);
        if (!real) {
            UncaptureComponent(slot->original);
            slot->steppedAside = true;
            gStatus->capErr = -4;
        }
    }
    if (!real && slot->steppedAside)
        real = OpenComponent(slot->original);
    if (!real)
        return kInvalidComponentID;

    storage = NewHandleSysClear(sizeof(StandInStorage));
    if (!storage) {
        CloseComponent(real);
        return memFullErr;
    }
    HLock(storage);
    ((StandInStorage *)*storage)->real = real;
    ((StandInStorage *)*storage)->index = i;
    SetComponentInstanceStorage(self, storage);
    gStatus->capOpens[i]++;
    return noErr;
}

static ComponentResult StandInClose(Handle storage)
{
    if (storage) {
        StandInStorage *s = (StandInStorage *)*storage;
        if (s->real) CloseComponent(s->real);
        DisposeHandle(storage);
    }
    return noErr;
}

static void ScaleBuffer(long i, SoundComponentData *data)
{
    UInt32 gain;

    gStatus->capFormat[i] = data->format;
    if (data->format != kFormatTwos || data->sampleSize != 16 || !data->buffer)
        return;
    gain = VolumeCombine(gStatus->hwVolume, gStatus->volume, gStatus->muted);
    if (gain == 0x01000100)
        return;
    VolumeScale16((SInt16 *)data->buffer, data->sampleCount, data->numChannels,
                  gain);
    gStatus->capScaled[i]++;
}

static ComponentResult Dispatch(ComponentParameters *p, Handle storage)
{
    StandInStorage *s;
    ComponentResult r;

    if (p->what == kComponentOpenSelect)
        return StandInOpen((ComponentInstance)p->params[0]);
    if (p->what == kComponentCloseSelect)
        return StandInClose(storage);
    if (!storage)
        return kBadComponentInstance;
    s = (StandInStorage *)*storage;
    gStatus->capCalls[s->index]++;

    r = DelegateComponentCall(p, s->real);
    if (p->what == kSoundComponentGetSourceDataSelect && r == noErr) {
        /* GetSourceData(ti, SoundComponentDataPtr *sourceData) */
        SoundComponentDataPtr data = *(SoundComponentDataPtr *)p->params[0];
        gStatus->capData[s->index]++;
        if (data) ScaleBuffer(s->index, data);
    }
    return r;
}

static void CaptureTargets(MmAfStatus *status);

void VolumeCaptureAttach(MmAfStatus *status)
{
    gStatus = status;
    CaptureTargets(status);
}

void VolumeCaptureRetry(void)
{
    if (gCaptureDone || !gStatus || gStatus->capAttempts >= 1000)
        return;
    CaptureTargets(gStatus);
}

static void CaptureTargets(MmAfStatus *status)
{
    ComponentDescription look, cd;
    Component found[kMaxCaptures], c, standIn;
    UniversalProcPtr upp;
    THz saveZone = GetZone();
    Handle name;
    unsigned t;
    int n = 0, i;

    status->capAttempts++;

    /* list every match first: registering and capturing changes what
       FindNextComponent returns */
    for (t = 0; t < kNumTargets; t++) {
        look.componentType = kTargets[t].type;
        look.componentSubType = kTargets[t].subType;
        look.componentManufacturer = 0;
        look.componentFlags = 0;
        look.componentFlagsMask = 0;
        c = NULL;
        while (n < kMaxCaptures && (c = FindNextComponent(c, &look)) != NULL)
            found[n++] = c;
    }
    if (n == 0)
        return;
    gCaptureDone = true;

    SetZone(SystemZone());
    upp = NewRoutineDescriptor((ProcPtr)Dispatch, uppComponentRoutineProcInfo,
                               kPowerPCISA);
    SetZone(saveZone);
    if (!upp) { status->capErr = memFullErr; return; }

    for (i = 0; i < n; i++) {
        SetZone(SystemZone());
        name = NewHandleSys(0);
        SetZone(saveZone);
        if (!name || GetComponentInfo(found[i], &cd, name, NULL, NULL) != noErr)
            continue;
        status->capType[i] = cd.componentType;
        status->capSub[i] = cd.componentSubType;
        status->capOpenAtInit[i] = CountComponentInstances(found[i]);
        cd.componentFlags = WrapperComponentFlags(cd.componentFlags);
        cd.componentFlagsMask = 0;
        standIn = RegisterComponent(&cd, upp, registerComponentGlobal, name,
                                    NULL, NULL);
        if (!standIn) { status->capErr = -2; continue; }
        gSlots[i].original = found[i];
        SetComponentRefcon(standIn, i);
        gSlots[i].captured = CaptureComponent(found[i], standIn);
        if (!gSlots[i].captured) {
            gSlots[i].original = NULL;
            UnregisterComponent(standIn);
            status->capErr = -3;
            continue;
        }
        status->capCount++;
    }
}
