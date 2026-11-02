/*
 *  AudioProbe.c  —  Mac Mini G4 audio test app for Mac OS 9
 *
 *  Diagnostic tool for the MacMini Audio Fix work. Answers:
 *  1. Which amp GPIO drives the internal speaker and which the
 *     headphone jack (checkboxes toggle each pin).
 *  2. Whether GPIO 0x67 tracks the headphone jack (live readout).
 *  3. Why system alert sounds are silent: plays a tone at several
 *     sample rates / sample sizes plus SysBeep, and records what the
 *     I2S clock registers do while each one plays.
 *
 *  Everything is appended to "AudioProbe Log" next to the app.
 *  After each tone, click "Yes" or "No" so the log says
 *  what you heard.
 *
 *  Build: Retro68 PowerPC (see CMakeLists.txt)
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Sound.h>
#include <Gestalt.h>
#include <OSUtils.h>
#include <Memory.h>
#include <NameRegistry.h>
#include <DriverServices.h>
#include <stdio.h>
#include <string.h>
#include "Status.h"

/* ---- Hardware ---- */

#define GPIO_HP_DETECT  0x67
#define GPIO_HP_OTHER   0x51
#define GPIO_AMP_6D     0x6D
#define FCR1            0x3C
#define I2S_IER         0x10000
#define I2S_FORMAT      0x10010     /* serial format: clock source + dividers */
#define I2S_WORDSIZE    0x10020

#ifndef initMono
#define initMono        0x0080      /* Sound.h: not in Multiversal headers */
#endif

/* Sound Manager 3 / Component Manager calls the Multiversal headers lack
   (InterfaceLib and SoundLib export them) */
typedef struct ComponentDescription {
    OSType componentType;
    OSType componentSubType;
    OSType componentManufacturer;
    UInt32 componentFlags;
    UInt32 componentFlagsMask;
} ComponentDescription;

Component FindNextComponent(Component aComponent, ComponentDescription *looking);
OSErr GetComponentInfo(Component aComponent, ComponentDescription *cd,
                       Handle componentName, Handle componentInfo,
                       Handle componentIcon);
OSErr GetSoundOutputInfo(Component outputDevice, OSType selector, void *infoPtr);
OSErr SndSetSysBeepState(short sysBeepState);

#ifndef volumeCmd
#define volumeCmd               46
#endif
#define kSoundOutputDeviceType  'sdev'
#define sysBeepEnable           0x0001
#define kFullVolume             0x01000100  /* right:left, 0x0100 = unity */

#define AMP_ON          0x05        /* output enable + data HIGH */
#define AMP_OFF         0x04        /* output disabled, data HIGH */

#define kNumAmps        4
static const UInt32 kAmpPins[kNumAmps] = { 0x6E, 0x6F, 0x70, 0x79 };

static UInt32   gMacIOBase = 0;     /* address used for access */
static UInt32   gPhysBase = 0;      /* from assigned-addresses */
static UInt32   gLogicalBase = 0;   /* from AAPL,address, if present */

static UInt8 Read8(UInt32 offset)
{
    UInt8 v = *(volatile UInt8 *)(gMacIOBase + offset);
    SynchronizeIO();
    return v;
}

static void Write8(UInt32 offset, UInt8 value)
{
    *(volatile UInt8 *)(gMacIOBase + offset) = value;
    SynchronizeIO();
}

static UInt32 Read32LE(UInt32 offset)
{
    /* mac-io registers are little-endian and must be read 32 bits
       at a time (byte reads of FCR/I2S registers are not reliable) */
    UInt32 v = *(volatile UInt32 *)(gMacIOBase + offset);
    SynchronizeIO();
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

static OSStatus FindMacIO(void)
{
    RegEntryID      entry;
    RegEntryIter    cookie;
    Boolean         done = false;
    OSStatus        err;
    UInt32          prop[20];
    RegPropertyValueSize sz;

    err = RegistryEntryIterateCreate(&cookie);
    if (err != noErr) return err;
    err = RegistryEntrySearch(&cookie, kRegIterSubTrees, &entry, &done,
                              "device_type", "mac-io", 7);
    if (err != noErr || done) {
        RegistryEntryIterateDispose(&cookie);
        err = RegistryEntryIterateCreate(&cookie);
        if (err != noErr) return err;
        err = RegistryEntrySearch(&cookie, kRegIterSubTrees, &entry, &done,
                                  "name", "mac-io", 6);
    }
    RegistryEntryIterateDispose(&cookie);
    if (err != noErr || done) return -1;

    sz = sizeof(prop);
    if (RegistryPropertyGet(&entry, "assigned-addresses", prop, &sz) == noErr)
        gPhysBase = prop[2];
    else {
        sz = sizeof(prop);
        if (RegistryPropertyGet(&entry, "reg", prop, &sz) == noErr)
            gPhysBase = prop[2];
    }
    sz = sizeof(prop);
    if (RegistryPropertyGet(&entry, "AAPL,address", prop, &sz) == noErr)
        gLogicalBase = prop[0];

    /* Prefer the logical address the OS mapped for drivers; the INIT
       uses the physical one, which is mapped 1:1 on this machine. */
    gMacIOBase = gLogicalBase ? gLogicalBase : gPhysBase;
    return gMacIOBase ? noErr : -1;
}

/* ---- Log ---- */

static FILE *gLog;

static void Log(const char *s)
{
    if (!gLog) gLog = fopen("AudioProbe Log", "a");
    if (gLog) { fputs(s, gLog); fputc('\n', gLog); fflush(gLog); }
}

static void LogRegisters(const char *when)
{
    char line[256];
    int i, n;

    n = snprintf(line, sizeof(line), "  [%s] amps", when);
    for (i = 0; i < kNumAmps; i++)
        n += snprintf(line + n, sizeof(line) - n, " %02lX=%02X",
                      kAmpPins[i], Read8(kAmpPins[i]));
    snprintf(line + n, sizeof(line) - n,
             " 6D=%02X | 67=%02X 51=%02X | FCR1=%08lX IER=%08lX FMT=%08lX WS=%08lX",
             Read8(GPIO_AMP_6D), Read8(GPIO_HP_DETECT), Read8(GPIO_HP_OTHER),
             Read32LE(FCR1), Read32LE(I2S_IER), Read32LE(I2S_FORMAT),
             Read32LE(I2S_WORDSIZE));
    Log(line);
}

/* ---- Tones ---- */

enum { kTone11k8, kTone22k8, kTone22k16, kTone44k8, kTone44k16, kTone48k16,
       kSysBeep, kSndPlay1, kBeepEnabled, kSndVolCmd, kSetVolsBeep,
       kVolSweep, kNumTests };

static const char *kTestNames[kNumTests] = {
    "11.127 kHz 8-bit", "22.254 kHz 8-bit", "22.05 kHz 16-bit",
    "44.1 kHz 8-bit", "44.1 kHz 16-bit", "48 kHz 16-bit", "SysBeep",
    "SndPlay snd 1", "SysBeep, beep on", "snd 1 + volumeCmd",
    "Set vols + SysBeep", "Volume sweep"
};

/* Volume sweep: output volume steps, loudest first */
static const UInt32 kSweepLevels[] = {
    0x01000100, 0x00C000C0, 0x00800080, 0x00400040, 0x00100010
};
static const Fixed kRates[kNumTests] = {
    0x2B7745D1, 0x56EE8BA3, 0x56220000,
    0xAC440000, 0xAC440000, 0xBB800000
};
static const UInt16 kRateHz[kNumTests] = {
    11127, 22254, 22050, 44100, 44100, 48000
};
static const short kBits[kNumTests] = { 8, 8, 16, 8, 16, 16 };

/* sin(x) for x in [-pi, pi]; no libm needed */
static float Sine(float x)
{
    float x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72))));
}

#define kToneHz     440
#define kToneSecs   2

#define kMaxSeen    8
static UInt32 gSeen[kMaxSeen];
static int    gSeenCount;

static void NoteFormat(void)
{
    UInt32 v = Read32LE(I2S_FORMAT);
    int i;
    for (i = 0; i < gSeenCount; i++)
        if (gSeen[i] == v) return;
    if (gSeenCount < kMaxSeen) gSeen[gSeenCount++] = v;
}

/* Watch the I2S format register for a while; returns early if done() */
static void WatchFor(UInt32 ticks, SndChannelPtr chan)
{
    UInt32 start = TickCount();
    SCStatus st;

    while (TickCount() - start < ticks) {
        NoteFormat();
        if (chan && TickCount() - start > 20) {
            if (SndChannelStatus(chan, sizeof(st), &st) == noErr
                && !st.scChannelBusy)
                break;
        }
    }
}

static OSErr PlayTone(int t, int secs);

/* ---- MacMini Audio Fix status record (Gestalt 'MmAf') ---- */

static MmAfStatus *GetStatus(void)
{
    long p = 0;
    if (Gestalt(kMmAfGestaltSelector, &p) != noErr || !p) return NULL;
    if (((MmAfStatus *)p)->version != kMmAfStatusVersion) return NULL;
    return (MmAfStatus *)p;
}

static void DescribeStatus(char *s, size_t size)
{
    MmAfStatus *st = GetStatus();
    if (!st) { snprintf(s, size, "MmAf status: not installed"); return; }
    snprintf(s, size,
             "MmAf %c%c%c%c t=%lu fix=%lu hp=%s main=%08lX plain=%08lX mute=%u "
             "werr=%d open=%u",
             (st->flags & kMmAfResident) ? 'R' : '-',
             (st->flags & kMmAfMonitor) ? 'M' : '-',
             (st->flags & kMmAfWrapper) ? 'W' : '-',
             (st->flags & kMmAfSteppedAside) ? 'S' : '-',
             st->ticks, st->corrections, st->headphones ? "in" : "out",
             st->hwVolume, st->volume, st->muted,
             st->wrapperErr, st->instancesAtInit);
}

static const char *Where(MmAfStatus *st, UInt32 a)
{
    if (a >= st->sysZone && a < st->sysLimit) return "system heap";
    if (a >= st->applZone && a < st->applLimit) return "INIT-time appl zone";
    return "elsewhere";
}

static void TypeStr(char *dst, OSType t);

static void LogStatus(const char *when)
{
    char s[200];
    char line[220];
    MmAfStatus *st;

    DescribeStatus(s, sizeof(s));
    snprintf(line, sizeof(line), "  [%s] %s", when, s);
    Log(line);

    st = GetStatus();
    if (!st) return;
    /* volume settings the extension answers and keeps */
    snprintf(line, sizeof(line),
             "    volume: reads volu=%lu hvol=%lu  changes volu=%lu hvol=%lu mute=%lu",
             st->volGets, st->hvolGets, st->volSets, st->hvolSets, st->muteSets);
    Log(line);
    if (st->volSets || st->hvolSets || st->muteSets) {
        UInt32 sel = st->lastSetSel;
        snprintf(line, sizeof(line),
                 "    last change: '%c%c%c%c' value %08lX",
                 (char)(sel >> 24), (char)(sel >> 16),
                 (char)(sel >> 8), (char)sel, st->lastSetRaw);
        Log(line);
    }

    /* where the INIT's pieces landed at startup */
    if (strcmp(when, "start") != 0) return;
    snprintf(line, sizeof(line),
             "    zones: system %08lX-%08lX  appl %08lX-%08lX  current %08lX",
             st->sysZone, st->sysLimit, st->applZone, st->applLimit, st->curZone);
    Log(line);
    snprintf(line, sizeof(line), "    data  %08lX (%s)", st->dataAddr, Where(st, st->dataAddr));
    Log(line);
    snprintf(line, sizeof(line), "    TOC   %08lX (%s)", st->tocAddr, Where(st, st->tocAddr));
    Log(line);
    snprintf(line, sizeof(line), "    code  %08lX (%s)", st->codeAddr, Where(st, st->codeAddr));
    Log(line);
    snprintf(line, sizeof(line), "    INIT  %08lX (%s)", st->initAddr, Where(st, st->initAddr));
    Log(line);
    snprintf(line, sizeof(line), "    copy  err=%d TOC %08lX (%s)", st->copyErr,
             st->copyTOC, Where(st, st->copyTOC));
    Log(line);
}

static void RunTest(int t)
{
    char line[160];
    int n, i;
    SndChannelPtr chan = NULL;
    SndCommand cmd;
    OSErr err = noErr;

    snprintf(line, sizeof(line), "TEST %s", kTestNames[t]);
    Log(line);
    gSeenCount = 0;
    NoteFormat();
    LogRegisters("before");

    if (t == kSysBeep) {
        SysBeep(30);
        WatchFor(3 * 60, NULL);
    } else if (t == kSndPlay1) {
        /* 'snd ' 1 from the System file, played directly rather than
           through the alert path; log its header so we know the format */
        Handle h = GetResource('snd ', 1);
        if (!h) { Log("  no 'snd ' 1"); return; }
        HLock(h);
        snprintf(line, sizeof(line),
                 "  snd 1: size=%ld first words %04X %04X %04X %04X %04X %04X",
                 GetHandleSize(h),
                 ((UInt16 *)*h)[0], ((UInt16 *)*h)[1], ((UInt16 *)*h)[2],
                 ((UInt16 *)*h)[3], ((UInt16 *)*h)[4], ((UInt16 *)*h)[5]);
        Log(line);
        err = SndPlay(NULL, h, false);
        WatchFor(60, NULL);
        HUnlock(h);
        ReleaseResource(h);
    } else if (t == kBeepEnabled) {
        /* is the Sound Manager's beep switched off? */
        short state = -1;
        SndGetSysBeepState(&state);
        snprintf(line, sizeof(line), "  sysBeepState=%04X", state);
        Log(line);
        if (!(state & sysBeepEnable))
            err = SndSetSysBeepState(state | sysBeepEnable);
        SysBeep(30);
        WatchFor(3 * 60, NULL);
        if (state != -1 && !(state & sysBeepEnable))
            SndSetSysBeepState(state);
    } else if (t == kSndVolCmd) {
        /* snd 1 on our own channel after a volume command, the way the
           alert path scales by the alert volume */
        Handle h = GetResource('snd ', 1);
        if (!h) { Log("  no 'snd ' 1"); return; }
        HLock(h);
        err = SndNewChannel(&chan, sampledSynth, initMono, NULL);
        if (err == noErr) {
            cmd.cmd = volumeCmd;
            cmd.param1 = 0;
            cmd.param2 = kFullVolume;
            err = SndDoImmediate(chan, &cmd);
            snprintf(line, sizeof(line), "  volumeCmd err=%d", err);
            Log(line);
            err = SndPlay(chan, h, false);
            WatchFor(60, NULL);
            SndDisposeChannel(chan, true);
        }
        HUnlock(h);
        ReleaseResource(h);
    } else if (t == kSetVolsBeep) {
        OSErr e1 = SetSysBeepVolume(kFullVolume);
        OSErr e2 = SetDefaultOutputVolume(kFullVolume);
        snprintf(line, sizeof(line),
                 "  SetSysBeepVolume err=%d  SetDefaultOutputVolume err=%d",
                 e1, e2);
        Log(line);
        SysBeep(30);
        WatchFor(3 * 60, NULL);
    } else if (t == kVolSweep) {
        long saved = 0;
        OSErr getErr = GetDefaultOutputVolume(&saved);
        if (getErr != noErr) saved = kFullVolume;
        for (i = 0; i < (int)(sizeof(kSweepLevels) / sizeof(kSweepLevels[0])); i++) {
            OSErr setErr = SetDefaultOutputVolume(kSweepLevels[i]);
            snprintf(line, sizeof(line), "  step %08lX: SetDefaultOutputVolume err=%d",
                     kSweepLevels[i], setErr);
            Log(line);
            err = PlayTone(kTone44k16, 1);
        }
        SetDefaultOutputVolume(saved);
        snprintf(line, sizeof(line), "  restored %08lX (get err=%d)", saved, getErr);
        Log(line);
    } else {
        err = PlayTone(t, kToneSecs);
    }

    LogRegisters("after");
    n = snprintf(line, sizeof(line), "  err=%d  FMT seen during test:", err);
    for (i = 0; i < gSeenCount; i++)
        n += snprintf(line + n, sizeof(line) - n, " %08lX", gSeen[i]);
    Log(line);
    LogStatus("after");
}

/* Play a 440 Hz tone in test t's format for secs seconds */
static OSErr PlayTone(int t, int secs)
{
    long frames, i;
    Ptr samples;
    SndChannelPtr chan = NULL;
    SndCommand cmd;
    ExtSoundHeader hdr;
    OSErr err;

    {
        frames = (long)kRateHz[t] * secs;
        samples = NewPtrClear(frames * (kBits[t] / 8));
        if (!samples) { Log("  out of memory"); return memFullErr; }

        for (i = 0; i < frames; i++) {
            /* phase in [-pi, pi) */
            float ph = (float)((i * (long)kToneHz) % kRateHz[t]) / kRateHz[t];
            float s = Sine((ph - 0.5f) * 6.2831853f) * 0.5f;
            if (kBits[t] == 8)
                ((UInt8 *)samples)[i] = (UInt8)(128 + (int)(s * 127));
            else
                ((SInt16 *)samples)[i] = (SInt16)(s * 32767);
        }

        memset(&hdr, 0, sizeof(hdr));
        hdr.samplePtr = samples;
        hdr.numChannels = 1;
        hdr.sampleRate = kRates[t];
        hdr.baseFrequency = 60;
        if (kBits[t] == 8) {
            /* standard header: numChannels field is the byte length */
            hdr.numChannels = frames;
            hdr.encode = stdSH;
        } else {
            hdr.encode = extSH;
            hdr.numFrames = frames;
            hdr.sampleSize = 16;
        }

        err = SndNewChannel(&chan, sampledSynth, initMono, NULL);
        if (err == noErr) {
            cmd.cmd = bufferCmd;
            cmd.param1 = 0;
            cmd.param2 = (long)&hdr;
            err = SndDoCommand(chan, &cmd, false);
            WatchFor((secs + 2) * 60, chan);
            SndDisposeChannel(chan, true);
        }
        DisposePtr(samples);
    }
    return err;
}

/* ---- UI ---- */

static WindowPtr      gWindow;
static ControlHandle  gAmpBox[kNumAmps];
static ControlHandle  gTestBtn[kNumTests];
static ControlHandle  gHeardBtn, gSilentBtn, gSnapBtn, gQuitBtn;
static int            gPendingTest = -1;
static Boolean        gQuit;
static char           gStatus[96] = "Ready.";

static void PStr(Str255 dst, const char *src)
{
    size_t n = strlen(src);
    if (n > 255) n = 255;
    dst[0] = n;
    memcpy(dst + 1, src, n);
}

static void DrawCStr(short h, short v, const char *s)
{
    Str255 p;
    PStr(p, s);
    MoveTo(h, v);
    DrawString(p);
}

static ControlHandle MakeControl(short l, short t, short r, short b,
                                 const char *title, short proc)
{
    Rect rc;
    Str255 p;
    SetRect(&rc, l, t, r, b);
    PStr(p, title);
    return NewControl(gWindow, &rc, p, true, 0, 0, 1, proc, 0);
}

#define kLiveTop    320

static void DrawLive(void)
{
    Rect r;
    char s[128];
    UInt8 hp = Read8(GPIO_HP_DETECT);
    int i;

    SetPort(gWindow);
    TextFont(4);    /* Monaco */
    TextSize(9);

    /* values next to the amp checkboxes */
    for (i = 0; i < kNumAmps; i++) {
        SetRect(&r, 150, 32 + i * 22, 230, 50 + i * 22);
        EraseRect(&r);
        snprintf(s, sizeof(s), "= %02X", Read8(kAmpPins[i]));
        DrawCStr(152, 45 + i * 22, s);
        SetControlValue(gAmpBox[i], (Read8(kAmpPins[i]) & 0x01) ? 1 : 0);
    }

    SetRect(&r, 250, 30, 510, 130);
    EraseRect(&r);
    snprintf(s, sizeof(s), "0x67 = %02X  headphones %s", hp,
             (hp & 0x02) ? "OUT" : "IN");
    DrawCStr(252, 45, s);
    snprintf(s, sizeof(s), "0x51 = %02X   0x6D = %02X",
             Read8(GPIO_HP_OTHER), Read8(GPIO_AMP_6D));
    DrawCStr(252, 61, s);

    SetRect(&r, 10, kLiveTop, 510, kLiveTop + 64);
    EraseRect(&r);
    snprintf(s, sizeof(s), "FCR1 %08lX  I2S IER %08lX",
             Read32LE(FCR1), Read32LE(I2S_IER));
    DrawCStr(12, kLiveTop + 14, s);
    snprintf(s, sizeof(s), "I2S FORMAT %08lX  WORDSIZE %08lX",
             Read32LE(I2S_FORMAT), Read32LE(I2S_WORDSIZE));
    DrawCStr(12, kLiveTop + 28, s);
    DescribeStatus(s, sizeof(s));
    DrawCStr(12, kLiveTop + 42, s);
    DrawCStr(12, kLiveTop + 58, gStatus);

    TextFont(0);
    TextSize(12);
}

static void DrawWindow(void)
{
    char s[96];

    SetPort(gWindow);
    TextFont(0);
    TextSize(12);
    DrawCStr(12, 22, "Amp pins (checked = output enabled)");
    DrawCStr(252, 22, "Headphone detect");
    DrawCStr(12, 136, "Test tones (440 Hz, 2 s)");
    DrawCStr(12, kLiveTop - 6, "Audio clock registers");
    TextFont(4);
    TextSize(9);
    snprintf(s, sizeof(s), "mac-io phys %08lX  logical %08lX",
             gPhysBase, gLogicalBase);
    DrawCStr(252, 110, s);
    DrawControls(gWindow);
    DrawLive();
}

static void SetAnswerButtons(Boolean on)
{
    HiliteControl(gHeardBtn, on ? 0 : 255);
    HiliteControl(gSilentBtn, on ? 0 : 255);
}

static void SetStatus(const char *s)
{
    strncpy(gStatus, s, sizeof(gStatus) - 1);
    DrawLive();
}

static void HandleControl(ControlHandle c)
{
    char line[96];
    int i;

    for (i = 0; i < kNumAmps; i++) {
        if (c == gAmpBox[i]) {
            Boolean on = !(Read8(kAmpPins[i]) & 0x01);
            Write8(kAmpPins[i], on ? AMP_ON : AMP_OFF);
            snprintf(line, sizeof(line), "PIN %02lX -> %s (readback %02X)",
                     kAmpPins[i], on ? "ON" : "OFF", Read8(kAmpPins[i]));
            Log(line);
            LogRegisters("now");
            SetStatus(line);
            return;
        }
    }
    for (i = 0; i < kNumTests; i++) {
        if (c == gTestBtn[i]) {
            snprintf(line, sizeof(line), "Playing %s...", kTestNames[i]);
            SetStatus(line);
            RunTest(i);
            gPendingTest = i;
            snprintf(line, sizeof(line), i == kVolSweep
                     ? "Did each step of the %s get quieter?"
                     : "Did you hear %s?", kTestNames[i]);
            SetStatus(line);
            SetAnswerButtons(true);
            return;
        }
    }
    if ((c == gHeardBtn || c == gSilentBtn) && gPendingTest >= 0) {
        snprintf(line, sizeof(line), "  RESULT %s: %s",
                 kTestNames[gPendingTest],
                 c == gHeardBtn ? "YES" : "NO");
        Log(line);
        gPendingTest = -1;
        SetAnswerButtons(false);
        SetStatus(line + 2);
    } else if (c == gSnapBtn) {
        Log("SNAPSHOT");
        LogRegisters("now");
        LogStatus("now");
        SetStatus("Registers written to log.");
    } else if (c == gQuitBtn) {
        gQuit = true;
    }
}

static void TypeStr(char *dst, OSType t)
{
    int i;
    for (i = 0; i < 4; i++) {
        char c = (t >> (24 - 8 * i)) & 0xFF;
        dst[i] = (c >= ' ' && c < 127) ? c : '.';
    }
    dst[4] = 0;
}

/* Every sound output component, then what the default one answers */
static void LogSoundOutput(void)
{
    static const OSType kSelectors[] = {
        'volu', 'hvol', 'hstp', 'hmut', 'srat', 'ssiz', 'chan',
        'hwfm', 'hwbs'
    };
    char line[200], ty[5], sub[5], man[5];
    ComponentDescription look, cd;
    Component comp = NULL;
    Handle name = NewHandle(0);
    short state = -1;
    UInt32 info[64];
    int i;

    SndGetSysBeepState(&state);
    snprintf(line, sizeof(line), "sysBeepState=%04X", state);
    Log(line);

    memset(&look, 0, sizeof(look));
    look.componentType = kSoundOutputDeviceType;
    while ((comp = FindNextComponent(comp, &look)) != NULL) {
        char cname[64] = "";
        memset(&cd, 0, sizeof(cd));
        GetComponentInfo(comp, &cd, name, NULL, NULL);
        if (name && GetHandleSize(name) > 0) {
            int n = **(UInt8 **)name;
            if (n > 63) n = 63;
            memcpy(cname, *name + 1, n);
            cname[n] = 0;
        }
        TypeStr(ty, cd.componentType);
        TypeStr(sub, cd.componentSubType);
        TypeStr(man, cd.componentManufacturer);
        snprintf(line, sizeof(line),
                 "sdev %08lX  %s/%s/%s  flags=%08lX  \"%s\"",
                 (UInt32)comp, ty, sub, man, cd.componentFlags, cname);
        Log(line);
    }
    if (name) DisposeHandle(name);

    for (i = 0; i < (int)(sizeof(kSelectors) / sizeof(kSelectors[0])); i++) {
        OSErr err;
        memset(info, 0, sizeof(info));
        err = GetSoundOutputInfo(NULL, kSelectors[i], info);
        TypeStr(ty, kSelectors[i]);
        snprintf(line, sizeof(line),
                 "default output '%s': err=%d  %08lX %08lX",
                 ty, err, info[0], info[1]);
        Log(line);
    }
}

/* Can this system resolve every import of the stage 3 INIT?
   A missing library or symbol makes the INIT's code fragment fail to
   prepare at startup. */
#include "Imports.h"

static void CheckImports(void)
{
    char line[160];
    Str255 pname, errName;
    ConnectionID conn = 0;
    Ptr mainAddr, symAddr;
    SymClass symClass;
    OSErr err = noErr;
    int i, missing = 0, checked = 0;

    Log("Import check (stage 3 INIT):");
    for (i = 0; kImports[i]; i++) {
        if (kImports[i][0] == '@') {
            PStr(pname, kImports[i] + 1);
            errName[0] = 0;
            err = GetSharedLibrary(pname, 'pwpc', kReferenceCFrag, &conn,
                                   &mainAddr, errName);
            if (err != noErr) {
                errName[errName[0] + 1] = 0;
                snprintf(line, sizeof(line), "  LIBRARY %s: err=%d (%s)",
                         kImports[i] + 1, err, (char *)errName + 1);
                Log(line);
                missing++;
            }
            continue;
        }
        if (err != noErr) continue;     /* library already reported */
        checked++;
        PStr(pname, kImports[i]);
        if (FindSymbol(conn, pname, &symAddr, &symClass) != noErr) {
            snprintf(line, sizeof(line), "  MISSING %s", kImports[i]);
            Log(line);
            missing++;
        }
    }
    snprintf(line, sizeof(line), "  %d symbols checked, %d problems", checked,
             missing);
    Log(line);
}

long CountComponentInstances(Component aComponent);

/* Every registered component, with how many instances are open. Shows
   which sound components are in use and how many copies are open. */
static void LogAllComponents(void)
{
    char line[200], ty[5], sub[5], man[5];
    ComponentDescription look, cd;
    Component comp = NULL;
    Handle name = NewHandle(0);
    int count = 0;

    memset(&look, 0, sizeof(look));
    Log("Components (type/subtype/manufacturer  flags  open  name):");
    while ((comp = FindNextComponent(comp, &look)) != NULL) {
        char cname[64] = "";
        memset(&cd, 0, sizeof(cd));
        GetComponentInfo(comp, &cd, name, NULL, NULL);
        if (name && GetHandleSize(name) > 0) {
            int n = **(UInt8 **)name;
            if (n > 63) n = 63;
            memcpy(cname, *name + 1, n);
            cname[n] = 0;
        }
        TypeStr(ty, cd.componentType);
        TypeStr(sub, cd.componentSubType);
        TypeStr(man, cd.componentManufacturer);
        snprintf(line, sizeof(line), "  %s/%s/%s  %08lX  %ld  \"%s\"",
                 ty, sub, man, cd.componentFlags,
                 CountComponentInstances(comp), cname);
        Log(line);
        count++;
    }
    snprintf(line, sizeof(line), "  %d components", count);
    Log(line);
    if (name) DisposeHandle(name);
}

static void LogStartup(void)
{
    char line[160];
    long mach = 0, sndVer = 0, beepVol = -1, outVol = -1;
    OSErr beepErr, outErr;

    Gestalt('mach', &mach);
    Gestalt('snd ', &sndVer);
    beepErr = GetSysBeepVolume(&beepVol);
    outErr = GetDefaultOutputVolume(&outVol);
    Log("");
    Log("==== AudioProbe start (v5) ====");
    snprintf(line, sizeof(line),
             "mach=%ld  sndAttr=%08lX  alertVol=%08lX (err %d)  outputVol=%08lX (err %d)",
             mach, sndVer, beepVol, beepErr, outVol, outErr);
    Log(line);
    snprintf(line, sizeof(line), "mac-io phys=%08lX logical=%08lX using=%08lX",
             gPhysBase, gLogicalBase, gMacIOBase);
    Log(line);
    LogRegisters("start");
    LogSoundOutput();
    LogStatus("start");
    CheckImports();
    LogAllComponents();
}

int main(void)
{
    Rect r;
    Str255 title;
    EventRecord ev;
    WindowPtr w;
    ControlHandle c;
    UInt32 lastDraw = 0;
    int i;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    SetRect(&r, 40, 60, 560, 460);
    PStr(title, "AudioProbe");
    gWindow = NewCWindow(NULL, &r, title, true, noGrowDocProc,
                         (WindowPtr)-1, true, 0);
    SetPort(gWindow);

    if (FindMacIO() != noErr) {
        TextFont(0);
        DrawCStr(12, 30, "Could not find the mac-io controller.");
        DrawCStr(12, 50, "Click to quit.");
        while (!Button()) ;
        return 0;
    }

    for (i = 0; i < kNumAmps; i++) {
        char t[16];
        snprintf(t, sizeof(t), "Pin %02lX", kAmpPins[i]);
        gAmpBox[i] = MakeControl(12, 32 + i * 22, 140, 50 + i * 22, t,
                                 checkBoxProc);
    }
    for (i = 0; i < kNumTests; i++) {
        short col = i % 3, row = i / 3;
        gTestBtn[i] = MakeControl(12 + col * 168, 146 + row * 26,
                                  172 + col * 168, 166 + row * 26,
                                  kTestNames[i], pushButProc);
    }
    gHeardBtn  = MakeControl(12, 258, 112, 278, "Yes", pushButProc);
    gSilentBtn = MakeControl(122, 258, 222, 278, "No", pushButProc);
    gSnapBtn   = MakeControl(240, 258, 380, 278, "Log registers", pushButProc);
    gQuitBtn   = MakeControl(400, 258, 500, 278, "Quit", pushButProc);
    SetAnswerButtons(false);

    LogStartup();

    while (!gQuit) {
        if (WaitNextEvent(everyEvent, &ev, 6, NULL)) {
            switch (ev.what) {
            case mouseDown:
                switch (FindWindow(ev.where, &w)) {
                case inDrag:
                    DragWindow(w, ev.where, &qd.screenBits.bounds);
                    break;
                case inGoAway:
                    if (TrackGoAway(w, ev.where)) gQuit = true;
                    break;
                case inContent:
                    if (w != FrontWindow()) { SelectWindow(w); break; }
                    SetPort(w);
                    GlobalToLocal(&ev.where);
                    if (FindControl(ev.where, w, &c)
                        && TrackControl(c, ev.where, NULL))
                        HandleControl(c);
                    break;
                }
                break;
            case keyDown:
                if ((ev.modifiers & cmdKey)
                    && (ev.message & charCodeMask) == 'q')
                    gQuit = true;
                break;
            case updateEvt:
                w = (WindowPtr)ev.message;
                BeginUpdate(w);
                DrawWindow();
                EndUpdate(w);
                break;
            }
        }
        if (TickCount() - lastDraw > 15) {
            DrawLive();
            lastDraw = TickCount();
        }
    }

    Log("==== AudioProbe quit ====");
    if (gLog) fclose(gLog);
    return 0;
}
