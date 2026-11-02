# MacMini Audio Fix v3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the one-shot MacMini Audio Fix INIT into a resident extension that switches speaker/headphones, keeps the amps on, and makes SysBeep and the volume slider work.

**Architecture:**
- **Pure logic, tested on this Mac.** The pin rules and volume clamping live in small modules with no Toolbox calls (`AmpLogic`, `VolumeLogic`). Host unit tests built with clang cover them.
- **Toolbox glue, verified on the Mac mini.** Residency, the Time Manager task, Gestalt and the component wrapper are thin layers over that logic. They're built with Retro68 and checked in three staged test rounds.
- **Stages.** `build.sh <stage>` sets `MMAF_STAGE`:
  1. AmpMonitor only;
  2. adds the volume wrapper (read);
  3. adds forwarding volume changes to the mixer.

**Tech Stack:** C (C99), Retro68 PowerPC toolchain (`~/Retro68-toolchain`), Rez, host `cc` for unit tests, AudioProbe (Retro68 CMake app) for on-device diagnostics.

**Spec:** `docs/superpowers/specs/2026-10-28-macmini-audio-v3-design.md`

## Global Constraints

- **Target system:** Mac OS 9.2.2 on the Mac mini G4 (`mach` 406). The output is one INIT file, "MacMini Audio Fix", type `INIT`, creator `MmAf`.
- **Pins:**
  - `0x70` = speaker amp, `0x6F` = headphone amp, `0x6E` and `0x79` kept on;
  - write `0x05` = on, `0x04` = off; compare bit 0 only;
  - headphone detect `0x67` bit 1: `0` = headphones in.
- **Timer:** 250 ms period. Debounce = 2 consecutive matching reads.
- **Volume:** stored as right:left, each channel clamped to `0x0100`. Initial value `0x01000100`.
- **Never worse than v2:**
  - mac-io not found → change nothing, ICN# 129;
  - resident allocation fails → write `0x05` to all four pins once, ICN# 130;
  - wrapper fails → AmpMonitor still runs, ICN# 130;
  - all running → ICN# 128.
- **Interrupt-time code** (timer tick, component dispatch apart from Open/Close) must not allocate memory or make Toolbox calls other than `PrimeTime`, `DelegateComponentCall` and `SoundComponentSetInfo`.
- **Resident memory:** all long-lived allocations go in the system heap. Call `SetZone(SystemZone())` around them and restore the previous zone afterwards.
- **Gestalt selector:** `'MmAf'`. Its value is a pointer to `MmAfStatus`, `version` = 3.
- **Imports:** only InterfaceLib, NameRegistryLib, DriverServicesLib and SoundLib.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, on branch `macmini-audio-v3`.

## Review Focus

1. **Booting with headphones already in.** Speaker off and jack on from the first moment. *Test:* `test_install_with_headphones_in` (Task 1).
2. **A plug bouncing while it's inserted.** One stray read must not switch outputs. *Test:* `test_single_glitch_ignored` (Task 1).
3. **The driver re-disabling a pin every tick.** We rewrite only that pin and count each fix, so it shows in the status record. *Test:* `test_apply_counts_only_mismatches` (Task 1).
4. **Out-of-range volume** (above `0x0100`, or one channel only). Each channel is clamped on its own and left/right never swap. *Test:* `test_clamp_per_channel` (Task 4).
5. **Captured driver fails to open.** The wrapper uncaptures it so the next open reaches the original driver, and sound is no worse than v2. *Check:* Task 5, step 4 implements the uncapture path; it's exercised on device in Task 5, step 7 if the open fails.

## File Structure

All paths are relative to `MacMiniAudioFix - max sound/`.

| Path | Responsibility |
|---|---|
| `src/AmpLogic.h/.c` | Pure: debounce, desired pin value, apply-with-corrections via read/write callbacks |
| `src/VolumeLogic.h/.c` | Pure: per-channel clamp |
| `src/MacIO.h/.c` | Find mac-io; `MacIORead8`, `MacIOWrite8`, `MacIORead32LE` |
| `src/Status.h` | `MmAfStatus`, flag bits, `kMmAfGestaltSelector` |
| `src/AmpMonitor.h/.c` | Time Manager task driving AmpLogic |
| `src/VolumeWrapper.h/.c` | `sdev` wrapper component around `awgc` |
| `src/Main.c` | INIT entry: startup order, residency, Gestalt, boot icon |
| `Retro68/include/MmAfToolbox.h` | Stand-in declarations missing from the Multiversal headers |
| `Retro68/build.sh` | Builds `src/*.c` with `-DMMAF_STAGE=$1` |
| `tests/` | Host unit tests: `host/MacTypes.h`, `test_amplogic.c`, `test_volumelogic.c`, `Makefile` |
| `AudioProbe/AudioProbe.c` | v4: status readout, Yes/No answers, volume sweep |

---

### Task 1: AmpLogic with host tests

**Files:**
- Create: `tests/host/MacTypes.h`, `tests/test_amplogic.c`, `tests/Makefile`, `src/AmpLogic.h`, `src/AmpLogic.c`

**Interfaces:**
- **Produces** (`src/AmpLogic.h`):

```c
enum { kPinHPDetect = 0x67, kPinAmp6E = 0x6E, kPinHeadphone = 0x6F,
       kPinSpeaker = 0x70, kPinAmp79 = 0x79 };
enum { kAmpOn = 0x05, kAmpOff = 0x04 };

typedef UInt8 (*AmpReadFn)(UInt32 offset);
typedef void  (*AmpWriteFn)(UInt32 offset, UInt8 value);

typedef struct {
    UInt8 stable;       /* debounced state, 1 = headphones in */
    UInt8 candidate;    /* last raw state seen */
    UInt8 count;        /* consecutive reads of candidate */
} AmpDebounce;

void    AmpDebounceInit(AmpDebounce *d, UInt8 detectReg);   /* stable = raw now */
Boolean AmpDebounceUpdate(AmpDebounce *d, UInt8 detectReg); /* returns stable */
Boolean AmpHeadphonesIn(UInt8 detectReg);                   /* bit1 == 0 */
UInt8   AmpDesired(UInt32 pin, Boolean headphonesIn);       /* kAmpOn/kAmpOff */
UInt32  AmpApply(Boolean headphonesIn, AmpReadFn rd, AmpWriteFn wr);
        /* writes only pins whose bit0 differs; returns number written */
```

`AmpApply` visits pins in the order `0x6E, 0x6F, 0x70, 0x79`.

- [ ] **Step 1: Write the host harness and failing tests**

`tests/host/MacTypes.h` typedefs `UInt8`, `UInt16`, `UInt32`, `SInt16`, `SInt32`, `Boolean` (`unsigned char`) and `OSType` from `<stdint.h>`, and defines `true`/`false` if missing.

`tests/test_amplogic.c` uses a fake register array `UInt8 regs[256]` behind `fakeRead`/`fakeWrite`, which log every write. Its tiny `CHECK(cond)` macro counts failures, and `main` returns a nonzero count on failure. Tests:

```c
test_headphones_in_bit:      AmpHeadphonesIn(0x80)==1, AmpHeadphonesIn(0x82)==0
test_desired_table:          AmpDesired(0x70,0)==0x05, AmpDesired(0x70,1)==0x04,
                             AmpDesired(0x6F,0)==0x04, AmpDesired(0x6F,1)==0x05,
                             AmpDesired(0x6E,x)==0x05, AmpDesired(0x79,x)==0x05 for x=0,1
test_debounce_needs_two:     init(0x82); update(0x80)==0; update(0x80)==1
test_single_glitch_ignored:  init(0x82); update(0x80)==0; update(0x82)==0; update(0x82)==0
test_install_with_headphones_in:
                             regs 6E,6F,70,79 = 0x07; init(0x80); AmpApply(1,...)==1;
                             regs[0x70]==0x04 written; no write to 0x6F (already on)
test_apply_counts_only_mismatches:
                             regs 6E=07 6F=04 70=04 79=04; AmpApply(0,...)==2;
                             write log is exactly {0x70:0x05, 0x79:0x05}
```

The `Makefile` target `test` builds with `cc -std=c99 -Wall -Werror -Ihost -I../src` and runs each binary.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -C "MacMiniAudioFix - max sound/tests" test`
Expected: compile error, `AmpLogic.h` not found.

- [ ] **Step 3: Implement `src/AmpLogic.c`**

Include `<MacTypes.h>` only, so the same file builds on the host and with Retro68. The debounce works like this:
- a raw state different from `candidate` resets `count` to 1;
- a matching one increments `count`, saturating at 2;
- when `count` reaches 2, `stable = candidate`.

`AmpDebounceInit` sets `stable = candidate = raw` and `count = 2`.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -C "MacMiniAudioFix - max sound/tests" test`
Expected: `test_amplogic: all passed`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add "MacMiniAudioFix - max sound/src/AmpLogic.h" "MacMiniAudioFix - max sound/src/AmpLogic.c" "MacMiniAudioFix - max sound/tests"
git commit -m "Add AmpLogic pin rules with host tests"
```

---

### Task 2: MacIO, Status, toolbox stand-ins and multi-file build (stage-0 parity)

The goal is to rebuild today's behaviour from the new file layout, so the build pipeline is proven before anything resident is added.

**Files:**
- Create: `src/MacIO.h`, `src/MacIO.c`, `src/Status.h`, `src/Main.c`, `Retro68/include/MmAfToolbox.h`
- Modify: `Retro68/build.sh`, `MacMiniAudioFix.r` (version strings)

**Interfaces:**
- **Produces** (`src/MacIO.h`):

```c
extern UInt32 gMacIOBase;
OSStatus MacIOFind(void);                 /* sets gMacIOBase; noErr or error */
UInt8    MacIORead8(UInt32 offset);
void     MacIOWrite8(UInt32 offset, UInt8 value);
UInt32   MacIORead32LE(UInt32 offset);    /* 32-bit load, byte-swapped */
```

`MacIORead8` and `MacIOWrite8` have the `AmpReadFn` and `AmpWriteFn` signatures.

- **Produces** (`src/Status.h`): the `MmAfStatus` struct exactly as in the spec, plus these counters:

```c
    UInt32 volGets;      /* GetInfo 'volu' calls answered */
    UInt32 hvolGets;     /* GetInfo 'hvol' calls seen (forwarded) */
    UInt32 volSets;      /* SetInfo 'volu' calls */
    SInt32 lastSetErr;   /* result of last forward to the mixer */
```

  together with:

```c
enum { kMmAfResident = 1, kMmAfMonitor = 2, kMmAfWrapper = 4 };
#define kMmAfGestaltSelector 'MmAf'
#define kMmAfStatusVersion 3
```

  The counters are what stage 2 uses to settle the spec's "`volu` or `hvol`" question.

- **Produces** (`Retro68/include/MmAfToolbox.h`), with types and values copied from Apple's Universal Interfaces:
  - `ComponentDescription`;
  - `ComponentParameters` = `{UInt8 flags; UInt8 paramSize; SInt16 what; SInt32 params[1];}`;
  - `ComponentResult`, `ComponentRoutineUPP`, `SoundSource`;
  - the prototypes `RegisterComponent`, `CaptureComponent`, `UncaptureComponent`, `OpenComponent`, `CloseComponent`, `DelegateComponentCall`, `CountComponentInstances`, `SetComponentInstanceStorage`, `FindNextComponent`, `GetComponentInfo`, `NewGestaltValue`, `SoundComponentSetInfo`, `SoundComponentGetInfo`;
  - constants:
    - `registerComponentGlobal = 1`;
    - component selectors `kComponentOpenSelect = -1`, `kComponentCloseSelect = -2`;
    - sound component selectors `kSoundComponentSetSourceSelect = 0x0002`, `kSoundComponentGetInfoSelect = 0x0103`, `kSoundComponentSetInfoSelect = 0x0104`;
    - info selectors `siVolume = 'volu'`, `siHardwareVolume = 'hvol'`;
    - proc infos `uppComponentRoutineProcInfo = 0x000003F0`, `uppTimerProcInfo = 0x0000B802`.

- [ ] **Step 1: Move the mac-io code into `src/MacIO.c`**

Take `FindIOControllerBase`, `ReadGPIO` and `WriteGPIO` from `MacMiniAudioFix.c` v8.0 and rename them per the interface. Add `MacIORead32LE`: a `volatile UInt32` load, `SynchronizeIO()`, then a byte swap. Keep the `assigned-addresses` then `reg` fallback unchanged.

- [ ] **Step 2: Write `src/Main.c` for stage 0**

It contains `ShowINITIcon` from v8.0 and a `main()` that:
1. checks the Name Registry Gestalt;
2. calls `MacIOFind`;
3. writes `kAmpOn` to the four pins;
4. shows ICN# 128.

It should behave exactly like v8.0.

- [ ] **Step 3: Make `Retro68/build.sh` build `src/*.c`**

- **Usage:** `build.sh [stage]`, default stage `3`. Compile every `src/*.c` plus `glue.c` with `-DMMAF_STAGE=$STAGE -I"$SRC/src"`.
- **Link:** add `"$LIBS/libSoundLib.a"`.
- **Output:** name it `MacMini Audio Fix.bin`/`.dsk` as now. Echo the stage in the final message.

- [ ] **Step 4: Set the version strings to 3.0**

In `MacMiniAudioFix.r`, change `vers` 1 and 2 to `0x03, 0x00` / `"3.0"`. Change the ICN# 130 comment to `Partial - speaker with dash`.

- [ ] **Step 5: Build and run the structural checks**

```bash
"MacMiniAudioFix - max sound/Retro68/build.sh" 0
```

Expected:
- `Built ... (stage 0)`.
- `ResInfo -a` lists 11 resources.
- The PEF check script from the earlier verification shows these libraries: NameRegistryLib, DriverServicesLib, InterfaceLib. SoundLib appears only once something imports from it.
- `main` resolves to the data-section descriptor.

Keep that check as `Retro68/check.sh "<bin>"`: a `python3 -I` PEF dump of the libraries, imports and main section, plus `ResInfo -a`. Later tasks reuse it.

- [ ] **Step 6: Commit**

```bash
git add "MacMiniAudioFix - max sound/src" "MacMiniAudioFix - max sound/Retro68" "MacMiniAudioFix - max sound/MacMiniAudioFix.r"
git commit -m "Split INIT into src/ modules and stage-aware Retro68 build"
```

---

### Task 3: Resident AmpMonitor and status record (stage 1)

**Files:**
- Create: `src/AmpMonitor.h`, `src/AmpMonitor.c`
- Modify: `src/Main.c`

**Interfaces:**
- **Consumes:**
  - `AmpDebounce*`, `AmpApply`, `AmpHeadphonesIn` (Task 1);
  - `MacIORead8`/`MacIOWrite8` (Task 2);
  - `MmAfStatus` (Task 2).
- **Produces:**

```c
OSStatus AmpMonitorStart(MmAfStatus *status);  /* applies table now, arms timer */
```

  `Main.c` also produces `static MmAfStatus *StatusCreate(void)`, which zeroes the struct, sets `version` and `volume = 0x01000100`, and calls `NewGestaltValue(kMmAfGestaltSelector, (long)status)`.

- [ ] **Step 1: Implement `AmpMonitorStart`**

The timer task is allocated in the system heap. It's an extended `TMTask`: the AmpMonitor's own struct starts with the `TMTask` and is followed by the `AmpDebounce` and a `MmAfStatus*`.

Its `tmAddr` is `NewRoutineDescriptor(Tick, uppTimerProcInfo, kPowerPCISA)`, created with the zone set to the system zone. Install with `InsXTime`, then `PrimeTime(task, 250)`.

**`Tick(TMTaskPtr t)`** casts `t` to the struct and:
1. calls `AmpDebounceUpdate` on `MacIORead8(kPinHPDetect)`;
2. adds `AmpApply(...)` to `status->corrections`;
3. sets `status->headphones`;
4. increments `status->ticks`;
5. calls `PrimeTime(t, 250)`.

**At install:** `AmpDebounceInit`, one `AmpApply`, then set `kMmAfMonitor`.

- [ ] **Step 2: Rewrite `main()` in the spec's startup order**

1. Name Registry and `MacIOFind`. Failure → ICN# 129, return.
2. Residency:
   - `h = Get1Resource('INIT', 0)`, then `DetachResource(h); HLock(h); HNoPurge(h)`;
   - `StatusCreate()`. If it returns NULL or the Gestalt registration fails, write `kAmpOn` to all four pins, show ICN# 130 and return.
   - Set `kMmAfResident`.
3. `AmpMonitorStart`. Failure → write `kAmpOn` to all four pins, ICN# 130.
4. `#if MMAF_STAGE >= 2` → `VolumeWrapperAttach(status)` (Task 4). Until Task 4 exists, stage 1 only.
5. Icon: ICN# 128 if the monitor is running and (stage < 2, or the wrapper is attached); otherwise 130.

- [ ] **Step 3: Build stage 1 and run the structural checks**

Run: `"MacMiniAudioFix - max sound/Retro68/build.sh" 1 && "MacMiniAudioFix - max sound/Retro68/check.sh" "MacMiniAudioFix - max sound/Retro68/build/MacMini Audio Fix.bin"`
Expected: build OK; the imports include `InsXTime`, `PrimeTime`, `NewGestaltValue`, `NewRoutineDescriptor` and `DetachResource`; 11 resources.

- [ ] **Step 4: Commit**

```bash
git add "MacMiniAudioFix - max sound/src"
git commit -m "Add resident AmpMonitor with headphone switching and status record"
```

---

### Task 4: AudioProbe v4, then hardware round 1

**Files:**
- Modify: `AudioProbe/AudioProbe.c`, `AudioProbe/CMakeLists.txt` (add `../src` to the include path for `Status.h`)

**Interfaces:**
- **Consumes:** `MmAfStatus`, `kMmAfGestaltSelector`, flag bits (Task 2).

- [ ] **Step 1: Show and log the status record**

In AudioProbe, `Gestalt(kMmAfGestaltSelector, &p)`. If found and `version == 3`:
- show a status line in the live area (flags, ticks, corrections, headphones, volume, volGets/hvolGets/volSets, wrapperErr, instancesAtInit);
- write the full record to the log at start, on "Log registers", and after each test.

If not found, show `MmAf status: not installed`.

- [ ] **Step 2: Turn the answer buttons into a generic Yes/No**

Rename the buttons to `Yes` and `No`, log `RESULT <test>: YES|NO`, and show the question for each test in the status line. Tones and beeps ask "Did you hear it?".

- [ ] **Step 3: Add a "Volume sweep" test**

For each of `0x01000100, 0x00C000C0, 0x00800080, 0x00400040, 0x00100010`:
1. `SetDefaultOutputVolume(v)` (log its error);
2. play the 44.1 kHz 16-bit tone for 1 s.

Afterwards, restore the volume read at the start (or `0x01000100` if the read failed). The question is "Did each step get quieter?".

- [ ] **Step 4: Build AudioProbe v4**

Run: `cmake --build "MacMiniAudioFix - max sound/AudioProbe/build"`
Expected: `Built target AudioProbe_APPL`, no warnings.

- [ ] **Step 5: Commit**

```bash
git add "MacMiniAudioFix - max sound/AudioProbe"
git commit -m "AudioProbe v4: MmAf status readout, Yes/No answers, volume sweep"
```

- [ ] **Step 6: Hardware round 1 (the user runs it on the Mac mini)**

Hand over the stage-1 `MacMini Audio Fix.bin` and `AudioProbe.bin`. Pass criteria:
- boot icon ICN# 128;
- plugging and unplugging switches speaker and jack within ~0.5 s;
- booting with headphones in works;
- after sleep and wake, the amps are restored;
- the AudioProbe status shows `ticks` increasing, `corrections` small and `headphones` matching the jack;
- no crash at boot.

Record the results in the commit message of the next task. **On failure:** stop and diagnose before Task 5.

---

### Task 5: VolumeWrapper with volume read (stage 2), then hardware round 2

**Files:**
- Create: `src/VolumeLogic.h`, `src/VolumeLogic.c`, `tests/test_volumelogic.c`, `src/VolumeWrapper.h`, `src/VolumeWrapper.c`
- Modify: `tests/Makefile`, `src/Main.c`

**Interfaces:**
- **Produces:**

```c
UInt32   VolumeClamp(UInt32 rightLeft);          /* each 16-bit half min(.,0x0100) */
OSStatus VolumeWrapperAttach(MmAfStatus *status);
```

- [ ] **Step 1: Write the failing test `tests/test_volumelogic.c`**

```c
test_clamp_full:          VolumeClamp(0x01000100)==0x01000100
test_clamp_per_channel:   VolumeClamp(0x02000080)==0x01000080
                          VolumeClamp(0x00400300)==0x00400100
test_clamp_zero:          VolumeClamp(0)==0
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C "MacMiniAudioFix - max sound/tests" test`
Expected: compile error, `VolumeLogic.h` not found.

- [ ] **Step 3: Implement `VolumeClamp`, then run the tests and verify they pass**

Run: `make -C "MacMiniAudioFix - max sound/tests" test`
Expected: both test binaries pass.

- [ ] **Step 4: Implement `VolumeWrapperAttach` and the dispatch routine**

**Attach steps**, recording the first failing step's error in `status->wrapperErr`:
1. `FindNextComponent` for `sdev`/`awgc`/`appl`, then `GetComponentInfo` to copy its description, name and flags.
2. `status->instancesAtInit = CountComponentInstances(awgc)`.
3. With the zone set to the system zone: create the dispatch UPP with `uppComponentRoutineProcInfo`, then `RegisterComponent(&cd, upp, registerComponentGlobal, nameCopy, NULL, NULL)`.
4. `captured = CaptureComponent(awgc, wrapper)`. Keep `captured` and `wrapper` in resident globals.
5. On success, set `kMmAfWrapper`. On failure, unregister whatever was registered.

**Dispatch** `pascal ComponentResult Dispatch(ComponentParameters *p, Handle storage)`. The arguments sit in `p->params[]` in reverse order, last argument first.

| `p->what` | Handling |
|---|---|
| `kComponentOpenSelect` | `self = (ComponentInstance)p->params[0]`. Open `captured` with `OpenComponent` and store the inner instance in a `NewHandleSys` storage handle via `SetComponentInstanceStorage`. If the open fails: `UncaptureComponent(awgc)`, clear `kMmAfWrapper`, return the error. |
| `kComponentCloseSelect` | `CloseComponent` the inner instance, dispose of the storage handle. |
| `kSoundComponentGetInfoSelect` | `selector = p->params[1]`, `info = (void*)p->params[0]`. If `selector == siVolume`: `*(UInt32*)info = status->volume`, `volGets++`, return `noErr`. If `selector == siHardwareVolume`: `hvolGets++`, then delegate. |
| everything else | `DelegateComponentCall(p, inner)` |

**Stage 2** does no SetInfo handling; it's simply delegated.

- [ ] **Step 5: Build stage 2 and run the structural checks**

Run: `"MacMiniAudioFix - max sound/Retro68/build.sh" 2 && "MacMiniAudioFix - max sound/Retro68/check.sh" "MacMiniAudioFix - max sound/Retro68/build/MacMini Audio Fix.bin"`
Expected: no SoundLib import yet (stage 2 calls no `SoundComponent*` function); the imports include `RegisterComponent`, `CaptureComponent`, `DelegateComponentCall`.

- [ ] **Step 6: Commit**

```bash
git add "MacMiniAudioFix - max sound/src" "MacMiniAudioFix - max sound/tests"
git commit -m "Add volume wrapper around Built-in output (read path)"
```

- [ ] **Step 7: Hardware round 2 (the user runs it on the Mac mini)**

**Pass:** SysBeep beeps with no menu-bar flash; `kMmAfWrapper` is set and `volGets` > 0. Note the values of `instancesAtInit` and `hvolGets`.

**Decision rules:**
- if SysBeep is still silent and `hvolGets > 0` while `volGets == 0`: make GetInfo `siHardwareVolume` return `status->volume` too, rebuild and retest;
- if `kMmAfWrapper` is clear because `instancesAtInit > 0`: stop and report. That's the spec's open question, and its fallback (load later, or attach on first use) needs a design decision from the user.

---

### Task 6: Forward volume to the mixer (stage 3), then hardware round 3

**Files:**
- Modify: `src/VolumeWrapper.c`

**Interfaces:**
- **Consumes:** `VolumeClamp` (Task 5).

- [ ] **Step 1: Handle SetSource and SetInfo `volu`**

- `kSoundComponentSetSourceSelect`: remember `source = (ComponentInstance)p->params[0]` and `sourceID = (SoundSource)p->params[1]` in storage, then delegate.
- `kSoundComponentSetInfoSelect` with `selector == siVolume`:
  1. `v = VolumeClamp((UInt32)p->params[0])` — the value is passed in the info pointer itself;
  2. `status->volume = v`, `volSets++`;
  3. if a source is remembered: `status->lastSetErr = SoundComponentSetInfo(source, sourceID, siVolume, (void*)v)`;
  4. return `noErr`.

Guard all of this with `#if MMAF_STAGE >= 3`.

- [ ] **Step 2: Build stage 3 and run the structural checks**

Run: `"MacMiniAudioFix - max sound/Retro68/build.sh" 3 && "MacMiniAudioFix - max sound/Retro68/check.sh" "MacMiniAudioFix - max sound/Retro68/build/MacMini Audio Fix.bin"`
Expected: build OK; SoundLib listed with `SoundComponentSetInfo`.

- [ ] **Step 3: Commit**

```bash
git add "MacMiniAudioFix - max sound/src/VolumeWrapper.c"
git commit -m "Forward volume changes from the wrapper to the mixer"
```

- [ ] **Step 4: Hardware round 3 (the user runs it on the Mac mini)**

**Pass:**
- in AudioProbe's "Volume sweep", each step gets quieter;
- the Sound control panel slider changes the level;
- SysBeep follows the alert volume;
- the status shows `volSets` > 0 and `lastSetErr == 0`.

**If no step gets quieter:** alerts still work from stage 2. Report `lastSetErr` and stop; the spec says real volume then needs a separate design.

---

### Task 7: Documentation and cleanup

**Files:**
- Modify: `TECHNICAL.txt`, `CW_ProjectSettings.txt`
- Delete: `MacMiniAudioFix.c` (only once round 3 or round 2 has passed)

- [ ] **Step 1: Update `TECHNICAL.txt`**

Add the spec's findings:
- pin map table;
- headphone detect;
- the 32-bit register-read note;
- the SysBeep cause;
- the round 1–3 results.

Replace "Known Limitations" items 1–3 with what v3 now does and what's still open.

- [ ] **Step 2: Point the build instructions at Retro68**

Replace the CodeWarrior "Build Instructions" in `TECHNICAL.txt` with `Retro68/build.sh` usage. Add a note at the top of `CW_ProjectSettings.txt` that it describes the old v8.0 CodeWarrior build.

- [ ] **Step 3: Remove `MacMiniAudioFix.c` and rebuild**

```bash
git rm "MacMiniAudioFix - max sound/MacMiniAudioFix.c" && "MacMiniAudioFix - max sound/Retro68/build.sh"
```

Expected: stage 3 build succeeds without it.

- [ ] **Step 4: Run the full verification**

Run: `make -C "MacMiniAudioFix - max sound/tests" test && "MacMiniAudioFix - max sound/Retro68/check.sh" "MacMiniAudioFix - max sound/Retro68/build/MacMini Audio Fix.bin"`
Expected: all host tests pass; structural checks OK.

- [ ] **Step 5: Commit**

```bash
git add "MacMiniAudioFix - max sound/TECHNICAL.txt" "MacMiniAudioFix - max sound/CW_ProjectSettings.txt"
git commit -m "Document v3 findings and retire the v8.0 source"
```
