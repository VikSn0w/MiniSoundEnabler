/*
 *  Status.h  —  status record the INIT publishes for AudioProbe
 *
 *  Gestalt('MmAf') returns a pointer to this struct in the system heap.
 *  AudioProbe reads it; nothing else depends on it.
 */
#pragma once
#include <MacTypes.h>

#define kMmAfGestaltSelector    'MmAf'
#define kMmAfStatusVersion      4

/* flags */
enum { kMmAfResident = 1, kMmAfMonitor = 2, kMmAfWrapper = 4,
       kMmAfSteppedAside = 8 };     /* captured driver failed; pass-through */

/* wrapperErr values for attach steps that have no OSErr of their own;
   other values are the failing call's OSErr */
enum {
    kMmAfErrNoDriver   = -1,    /* no sdev/awgc/appl component */
    kMmAfErrRegister   = -2,    /* RegisterComponent returned nil */
    kMmAfErrCapture    = -3,    /* CaptureComponent returned nil */
    kMmAfErrInnerOpen  = -4,    /* captured driver failed to open; uncaptured */
    kMmAfErrNotSysHeap = -5,    /* INIT globals outside the system heap */
    kMmAfErrCodeNotSysHeap = -6 /* INIT code outside the system heap */
};

typedef struct {
    UInt16 version;          /* kMmAfStatusVersion */
    UInt16 flags;
    UInt32 macIOBase;
    SInt16 wrapperErr;       /* first failing attach step's error, 0 if none */
    UInt16 instancesAtInit;  /* awgc instances already open at attach */
    UInt32 ticks;            /* AmpMonitor tick counter */
    UInt32 corrections;      /* pin writes made by AmpMonitor */
    UInt8  headphones;       /* debounced state: 1 = in */
    UInt8  muted;            /* SetInfo 'hmut' */
    UInt16 reserved;

    /* Volume settings the wrapper keeps (not applied to the audio) */
    UInt32 volume;           /* plain volume 'volu', right:left */
    UInt32 hwVolume;         /* main volume 'hvol', right:left */
    UInt32 volGets;          /* GetInfo 'volu' answered */
    UInt32 hvolGets;         /* GetInfo 'hvol' answered */
    UInt32 volSets;          /* SetInfo 'volu' */
    UInt32 hvolSets;         /* SetInfo 'hvol' */
    UInt32 muteSets;         /* SetInfo 'hmut' */
    UInt32 lastSetSel;       /* selector of the last change */
    UInt32 lastSetRaw;       /* its value as passed */

    /* Where things landed at startup (residency diagnostics) */
    UInt32 dataAddr;         /* a global in the startup run's data section */
    UInt32 tocAddr;          /* the startup run's TOC */
    UInt32 codeAddr;         /* a function's code */
    UInt32 initAddr;         /* the INIT resource's master pointer target */
    UInt32 sysZone;          /* SystemZone() */
    UInt32 sysLimit;         /* SystemZone()->bkLim */
    UInt32 applZone;         /* ApplicationZone() at INIT time */
    UInt32 applLimit;        /* ApplicationZone()->bkLim */
    UInt32 curZone;          /* GetZone() at INIT time */
    SInt16 copyErr;          /* preparing the resident copy in the system heap */
    UInt16 copyPending;      /* set while the resident copy's main() runs */
    UInt32 copyTOC;          /* the resident copy's TOC (its data section) */
} MmAfStatus;
