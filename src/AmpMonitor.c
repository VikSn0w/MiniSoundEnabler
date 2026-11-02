/*
 *  AmpMonitor.c  —  resident speaker/headphone switching and amp keep-alive
 *
 *  A Time Manager task re-armed every 250 ms reads the headphone detect
 *  pin, debounces it and rewrites any amp pin that is in the wrong
 *  state (AmpLogic). The task and its routine descriptor live in the
 *  system heap. Tick runs at interrupt time: GPIO access and PrimeTime
 *  only.
 */
#include <MacTypes.h>
#include <Memory.h>
#include <Timer.h>
#include <MmAfToolbox.h>
#include "AmpLogic.h"
#include "VolumeLogic.h"
#include "AmpMonitor.h"
#include "MacIO.h"

#define kTickMillis     250

typedef struct {
    TMTask      tm;             /* must be first: the Time Manager hands us this */
    AmpDebounce debounce;
    MmAfStatus *status;
} AmpTask;

/* Muted, or the volume (main x plain) at 0 on both channels. The
   volumes are kept by the wrapper; nothing scales the audio itself,
   so silence is done by switching the output amps off. */
static Boolean Silent(const MmAfStatus *status)
{
    return VolumeCombine(status->hwVolume, status->volume, status->muted) == 0;
}

static void Tick(TMTask *t)
{
    AmpTask *task = (AmpTask *)t;
    MmAfStatus *status = task->status;
    Boolean in;

    in = AmpDebounceUpdate(&task->debounce, MacIORead8(kPinHPDetect));
    status->corrections += AmpApply(in, Silent(status), MacIORead8, MacIOWrite8);
    status->headphones = in;
    status->ticks++;
    PrimeTime((QElemPtr)t, kTickMillis);
}

OSStatus AmpMonitorStart(MmAfStatus *status)
{
    AmpTask *task;
    UniversalProcPtr upp;
    THz saveZone;

    saveZone = GetZone();
    SetZone(SystemZone());
    task = (AmpTask *)NewPtrSysClear(sizeof(AmpTask));
    upp = task ? NewRoutineDescriptor((ProcPtr)Tick, uppTimerProcInfo,
                                      kPowerPCISA) : NULL;
    SetZone(saveZone);
    if (!task || !upp) {
        if (task) DisposePtr((Ptr)task);
        return memFullErr;
    }

    task->status = status;
    AmpDebounceInit(&task->debounce, MacIORead8(kPinHPDetect));
    status->corrections += AmpApply(task->debounce.stable, Silent(status),
                                    MacIORead8, MacIOWrite8);
    status->headphones = task->debounce.stable;

    task->tm.tmAddr = (ProcPtr)upp;
    InsXTime((QElemPtr)&task->tm);
    PrimeTime((QElemPtr)&task->tm, kTickMillis);
    status->flags |= kMmAfMonitor;
    return noErr;
}
