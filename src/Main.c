/*
 *  Main.c  —  Mini Sound Enabler v3  INIT entry
 *
 *  Mac OS 9 System Extension for Mac Mini G4 audio.
 *  Stage 0 (MMAF_STAGE 0) is the v8.0 one-shot fix. Stage 1 stays
 *  resident and runs AmpMonitor; stages 2 and 3 add the volume wrapper.
 *  Any failure after mac-io is found falls back to the v8.0 fix.
 *
 *  Build: Retro68/build.sh <stage>
 *
 *  Based on MacMiniAudioFix by Matouš Pikous (MacLegacyLabs, GPLv3);
 *  v3 by Vittorio Picone.
 */

#include <Types.h>
#include <Memory.h>
#include <Resources.h>
#include <Gestalt.h>
#include <OSUtils.h>
#include <Icons.h>
#include <Quickdraw.h>
#include <NameRegistry.h>
#include <MmAfToolbox.h>
#include "MacIO.h"
#include "AmpLogic.h"
#include "AmpMonitor.h"
#include "Status.h"
#include "HeapCheck.h"

#ifndef MMAF_STAGE
#define MMAF_STAGE 3
#endif

#if MMAF_STAGE >= 2
#include "VolumeWrapper.h"
#endif

#define kIconOK_ID      128
#define kIconFail_ID    129
#define kIconSkip_ID    130

#define kPristinePEFType 'MmPf'     /* the PEF again, never prepared by startup */

static const UInt32 kAllAmps[] = { kPinAmp6E, kPinHeadphone, kPinSpeaker,
                                   kPinAmp79 };

/* ---- INIT Icon ---- */

static void ShowINITIcon(short iconID, Boolean advance)
{
    Handle      iconHandle;
    short       hPos;
    Rect        iconRect;
    GrafPort    myPort;
    GrafPtr     savePort;

    iconHandle = GetResource('ICN#', iconID);
    if (iconHandle == nil) return;
    hPos = *(short *)0x92C;
    if (hPos == 0) hPos = 8;
    GetPort(&savePort);
    OpenPort(&myPort);
    iconRect.left   = hPos;
    iconRect.right  = hPos + 32;
    iconRect.bottom = myPort.portBits.bounds.bottom - 8;
    iconRect.top    = iconRect.bottom - 32;
    PlotIconID(&iconRect, atNone, ttNone, iconID);
    if (advance) {
        hPos += 40;
        *(short *)0x92C = hPos;
    }
    ClosePort(&myPort);
    SetPort(savePort);
    ReleaseResource(iconHandle);
}

/* Debug builds (build.sh with BREADCRUMBS=1) draw a dash icon at each
   startup step, so the number of dashes on screen shows how far
   startup got before a crash. */
#ifdef MMAF_BREADCRUMBS
#define BREADCRUMB()    ShowINITIcon(kIconSkip_ID, true)
#else
#define BREADCRUMB()
#endif

/* v8.0 behaviour: every amp pin on, once */
static void EnableAllAmps(void)
{
    unsigned i;
    for (i = 0; i < sizeof(kAllAmps) / sizeof(kAllAmps[0]); i++)
        MacIOWrite8(kAllAmps[i], kAmpOn);
}

#if MMAF_STAGE >= 1
/* At startup the system loads this INIT into a temporary zone that is
   wiped when the Finder starts, and prepares its code fragment there,
   so this run's code and globals can't outlive main(). The resident
   parts run from a second copy: the PEF is copied into the system heap
   (from 'MmPf' 0, which startup never touches) and prepared with the
   system heap as the current zone, which puts
   the copy's code and data there too. Its main() sees the pending flag
   in the status record and does the resident setup. */

/* Status record in the system heap, published as Gestalt 'MmAf' */
static MmAfStatus *StatusCreate(void)
{
    MmAfStatus *status;
    THz saveZone = GetZone();

    SetZone(SystemZone());
    status = (MmAfStatus *)NewPtrSysClear(sizeof(MmAfStatus));
    SetZone(saveZone);
    if (!status) return NULL;

    status->version = kMmAfStatusVersion;
    status->macIOBase = gMacIOBase;
    status->volume = 0x01000100;
    status->hwVolume = 0x01000100;
    if (NewGestaltValue(kMmAfGestaltSelector, (long)status) != noErr) {
        DisposePtr((Ptr)status);
        return NULL;
    }
    return status;
}

/* Where this (startup) run's pieces are, for AudioProbe */
static void RecordPlacement(MmAfStatus *status, Handle self)
{
    THz sys = SystemZone(), appl = ApplicationZone();

    status->dataAddr = (UInt32)&gMacIOBase;
    status->tocAddr = ((UInt32 *)(void *)EnableAllAmps)[1];
    status->codeAddr = ((UInt32 *)(void *)EnableAllAmps)[0];
    status->initAddr = (UInt32)*self;
    status->sysZone = (UInt32)sys;
    status->sysLimit = (UInt32)sys->bkLim;
    status->applZone = (UInt32)appl;
    status->applLimit = (UInt32)appl->bkLim;
    status->curZone = (UInt32)GetZone();
}

/* Copy our PEF into the system heap, prepare it there and run its main()
   as the resident copy. The copy's PEF block and connection are never
   released. Returns the GetMemFragment error, or noErr. */
static OSErr RunResidentCopy(MmAfStatus *status)
{
    THz saveZone = GetZone();
    Handle pristine;
    Size length;
    ConnectionID conn = 0;
    Ptr pef, mainAddr = NULL;
    Str255 errName;
    Str63 name;
    OSErr err;

    /* Not *self + 32: at startup the Code Fragment Manager relocated that
       PEF's data section in place, so preparing a copy of it would add
       every address twice. 'MmPf' 0 holds the same PEF, untouched. */
    pristine = Get1Resource(kPristinePEFType, 0);
    if (!pristine) return resNotFound;
    length = GetHandleSize(pristine);
    pef = NewPtrSys(length);
    if (!pef) {
        ReleaseResource(pristine);
        return memFullErr;
    }
    BlockMoveData(*pristine, pef, length);
    ReleaseResource(pristine);
    /* The code section runs in place from this block. BlockMoveData only
       goes through the data cache, so push it to memory and drop stale
       instruction-cache lines, or the CPU may execute old memory
       (illegal instruction at startup). */
    MakeDataExecutable(pef, length);

    name[0] = 9;
    BlockMoveData("MmAf copy", name + 1, 9);
    BREADCRUMB();                               /* 1: about to prepare copy */
    SetZone(SystemZone());
    err = GetMemFragment(pef, length, name, kLoadNewCopy, &conn, &mainAddr,
                         errName);
    SetZone(saveZone);
    if (err != noErr) {
        DisposePtr(pef);
        return err;
    }
    status->copyTOC = ((UInt32 *)mainAddr)[1];

    BREADCRUMB();                               /* 2: copy prepared */
    status->copyPending = 1;
    ((void (*)(void))(void *)mainAddr)();       /* resident copy's main() */
    status->copyPending = 0;
    return noErr;
}

/* The timer and the wrapper run this copy's code and use its globals
   after startup, so both must be in the system heap. Returns 0 if they
   are, otherwise the kMmAfErr code for the one that isn't. */
static SInt16 CheckResidentMemory(void)
{
    THz sys = SystemZone();
    /* a PowerPC function pointer is a transition vector: code, then TOC */
    Ptr code = *(Ptr *)(void *)EnableAllAmps;

    if (!AddressInRange(&gMacIOBase, sys, sys->bkLim))
        return kMmAfErrNotSysHeap;
    if (!AddressInRange(code, sys, sys->bkLim))
        return kMmAfErrCodeNotSysHeap;
    return 0;
}

/* main() of the resident copy: start everything that outlives startup */
static void ResidentStart(MmAfStatus *status)
{
    BREADCRUMB();                               /* 3: copy's main() running */
    if ((status->wrapperErr = CheckResidentMemory()) != 0)
        return;
    status->flags |= kMmAfResident;
    BREADCRUMB();                               /* 4: copy is in system heap */
    if (AmpMonitorStart(status) != noErr)
        return;
    BREADCRUMB();                               /* 5: amp monitor started */
#if MMAF_STAGE >= 2
    VolumeWrapperAttach(status);
    BREADCRUMB();                               /* 6: wrapper attached */
#endif
}
#endif

/* ---- Main ---- */

void main(void)
{
    long gestaltResult;
#if MMAF_STAGE >= 1
    MmAfStatus *status;
    Handle self;
    long existing;
#endif

    if (Gestalt(gestaltNameRegistryVersion, &gestaltResult) != noErr
        || MacIOFind() != noErr) {
        ShowINITIcon(kIconFail_ID, true);
        return;
    }

#if MMAF_STAGE >= 1
    if (Gestalt(kMmAfGestaltSelector, &existing) == noErr) {
        status = (MmAfStatus *)existing;
        if (status->copyPending) {
            ResidentStart(status);      /* we are the resident copy */
            return;
        }
        /* another copy of this extension is already installed */
        ShowINITIcon(kIconSkip_ID, true);
        return;
    }

    /* Anything that fails from here on falls back to the v8.0 fix */
    self = Get1Resource('INIT', 0);
    if (!self || (status = StatusCreate()) == NULL) {
        EnableAllAmps();
        ShowINITIcon(kIconSkip_ID, true);
        return;
    }
    HLock(self);
    RecordPlacement(status, self);

    status->copyErr = RunResidentCopy(status);
    HUnlock(self);
    if (!(status->flags & kMmAfMonitor)) {
        EnableAllAmps();
        ShowINITIcon(kIconSkip_ID, true);
        return;
    }
#if MMAF_STAGE >= 2
    ShowINITIcon((status->flags & kMmAfWrapper) ? kIconOK_ID : kIconSkip_ID,
                 true);
#else
    ShowINITIcon(kIconOK_ID, true);
#endif

#else
    EnableAllAmps();
    ShowINITIcon(kIconOK_ID, true);
#endif
}
