# MacMini Audio Fix v3 — Technical Notes

A Mac OS 9 system extension for the **Mac mini G4** (A1103,
PowerMac10,1/10,2) running Mac OS 9.2.2 through the MacOS9Lives
unsupported-hardware patch. It makes the built-in audio usable:

| Feature | Status |
|---|---|
| Internal speaker and headphone jack produce sound | Works |
| Automatic speaker ⇄ headphone switching | Works |
| Amplifiers kept on (if anything turns them off) | Works |
| System alert sounds (SysBeep) play | Works |
| Mute, and volume at zero, are silent | Works (amps switched off) |
| Volume slider / Control Strip keep their setting | Works |
| Intermediate volume levels change loudness | Works (equalizer capture, see [Volume control](#volume-control)) |

Everything below was established on the real machine, using the
AudioProbe diagnostic app in this repository.

---

## 1. The hardware

MacOS9Lives presents the Mac mini to OS 9 as a G4 Cube and gives its
sound hardware a `screamer`-compatible description. I2S audio then
flows into the DAC built into the Intrepid mac-io controller
("Toonie"), and the startup chime plays at full volume. But the Sound
Manager driver leaves the output amplifiers disabled, so the headphone
jack is nearly silent and the speaker is mute. The Mac mini has no
TAS3004 codec: there is no hardware volume, and the amplifiers are
controlled by GPIO pins.

### Which Mac this is

The extension runs only on a Mac mini G4. Because of the Cube
disguise, the device tree root says `model "PowerMac5,1"`, the same as
a real G4 Cube, whose GPIO pins mean something else. The boot ROM node
keeps the real name: `Devices:device-tree:rom:boot-rom`, property
`model`, reads `"Apple PowerMac10,1 4.8.9f4 BootROM built on 03/23/05
…"` on the test machine. The check (`src/MachineLogic.c`, host-tested)
accepts `PowerMac10,<digit>` in that property, or in the root `model`
for a system that doesn't disguise the mini. Anything else gets the
**X** icon and no change to any hardware.

The sound node `mac-io:i2s:i2s-a:sound` (`compatible "AOAKeylargo"`)
also has `layout-id` 58 here; it isn't used by the check.

### mac-io

The mac-io base address comes from the Name Registry: the
`device_type`/`name` `"mac-io"` entry, `assigned-addresses` word 2. On
this machine it is `0x80000000`, both physical and logical.

> FCR and I2S registers are 32-bit little-endian and must be read with
> 32-bit loads. Byte reads return wrong values.

The I2S serial-format register (`mac-io + 0x10010`) reads `0x00003A41`
and never changes, whatever the sample rate. Sample rate and format
are not a problem: 11.127, 22.05, 22.254, 44.1 and 48 kHz, in 8-bit
and 16-bit, all play.

### GPIO pins (byte registers at mac-io + offset)

| Offset | Role | Notes |
|---|---|---|
| `0x70` | **Internal speaker amp** | off → speaker silent |
| `0x6F` | **Headphone jack amp** | off → jack silent |
| `0x6E` | amp-related, no audible effect | kept on |
| `0x79` | amp-related, no audible effect | kept on |
| `0x67` | **Headphone detect** | bit 1: `1` = nothing plugged (`0x82`), `0` = headphones in (`0x80`) |
| `0x51` | follows the jack | `0xFF` out / `0x7F` in |

The pin values are:
- bit 0: output enable;
- bit 2: data high;
- bit 1: status, set by the hardware.

Writing **`0x05`** turns an amp on; it reads back `0x07`. Writing
**`0x04`** turns it off. The amps have also been found switched off
after boot with headphones in. The cause is unknown, and the amp
monitor corrects it.

---

## 2. What the extension does

### Amp monitor (`src/AmpMonitor.c`, `src/AmpLogic.c`)

A Time Manager task runs every **250 ms** at interrupt time. On each tick it:

1. reads `0x67`. A change in headphone state only counts after **two
   consecutive matching reads**, about 0.5 s;
2. works out the wanted state of each pin:

   | | `0x70` speaker | `0x6F` jack | `0x6E`, `0x79` |
   |---|---|---|---|
   | headphones out | on | off | on |
   | headphones in | off | on | on |
   | **silent** (mute, or volume 0) | off | off | on |

3. rewrites only the pins whose bit 0 is wrong, and counts each write
   as a "correction".

Here "silent" means the plain volume × main volume is 0 on both
channels (`VolumeCombine` in `src/VolumeLogic.c`), or Mute is on.

### Volume wrapper (`src/VolumeWrapper.c`)

The only sound output component is `sdev/awgc/appl`, "Built-in". It
answers every volume request (`volu`, `hvol`, `hstp`, `hmut`) with
`badComponentSelector` (−32766). **SysBeep reads `hvol` first, takes
the error as volume 0, and flashes the menu bar instead of beeping.**

The extension registers a component with the same description, with
the `cmpWantsRegisterMessage` flag removed, and captures `awgc` behind
it. Every call is forwarded to the real driver, except GetInfo and
SetInfo for these three selectors:

| Selector | Set by | Kept as |
|---|---|---|
| `volu` (plain volume) | SysBeep around each beep; Control Strip | `status->volume` |
| `hvol` (main volume) | Sound control panel; Control Strip; `SetDefaultOutputVolume` | `status->hwVolume` |
| `hmut` (mute) | Sound control panel | `status->muted` |

The values arrive in `infoPtr` itself, not through a pointer. Clamping
is per channel (right:left, `0x0100` = full).

If the captured driver ever fails to open, the wrapper uncaptures it.
From then on it opens the original driver directly and passes every
call through.

### Status record (`src/Status.h`)

`Gestalt('MmAf')` returns a pointer to a version 4 struct in the
system heap. It holds:
- the flags;
- the timer ticks and corrections;
- the headphone state;
- the volume settings and counters;
- where the code and data landed at startup.

AudioProbe reads it.

---

## 3. Startup: how a Retro68 PowerPC INIT gets to stay resident

This took most of the debugging. Each step below was found on the
hardware.

### 3.1 Packaging: accelerated INIT resource

`INIT 0` is a **32-byte Mixed Mode routine descriptor followed by a
PEF container**, the same layout CodeWarrior's "PPC code resource"
produces:

```
AAFE 0700 00000000 0000 0000      MixedModeMagic, version 7, 1 routine
00000000                          procInfo 0 (no args, no result)
00 01 0007 00000020 00000000 00000000
                                  PowerPC ISA; relative | needs preparing |
                                  native ISA; PEF at +0x20
<PEF container>
```

The 68K INIT loader jumps into the descriptor. Mixed Mode has the Code
Fragment Manager prepare the PEF and calls its `main`. Retro68 can't
build PowerPC code resources itself, so `Retro68/build.sh` does it:
it links with `-nostartfiles -nostdlib` and the ndrv linker script,
converts the result with MakePEF, prepends the descriptor and Rez's it
in as hex data. `glue.c` supplies an empty `__main`.

> **Pitfall: error type 1010 (`dsBadLibrary`) at startup.**
>
> - **Symptom:** with MakePEF's output as-is, startup shows "Code
>   Fragment could not be prepared … error type 1010", even though
>   `GetMemFragment` accepts the same PEF in every load mode from an
>   application.
> - **Fix:** `Retro68/repack_pef.py --tidy` lays the PEF out like
>   CodeWarrior:
>   - section contents 16-byte aligned, loader section first;
>   - zero-fill (bss) written as explicit data;
>   - the export hash table sized to 2 entries and 4-byte aligned;
>   - a non-zero timestamp.
> - **What it isn't:** procInfo, resource attributes and alignment
>   alone did not fix it. Which of the three "tidy" changes is
>   required was not narrowed down; all three are applied.

### 3.2 The INIT-time zone

At startup the INIT resource is loaded into a **temporary INIT-time
application zone**, which is wiped when the Finder starts. Its
fragment is prepared there too, so code, TOC and data all live in that
zone. Anything left running from it would crash later. The Code
Fragment Manager allocates a fragment's data in **whatever zone is
current** when the fragment is prepared.

### 3.3 The resident copy

`main()` in the startup run does the following:

1. Checks that this is a Mac mini G4 ([Which Mac this is](#which-mac-this-is))
   and finds mac-io; if either fails it shows the **X** icon and
   returns.
2. Creates the status record in the system heap and publishes it with
   `NewGestaltValue`.
3. Copies the PEF from resource **`'MmPf' 0`** into a `NewPtrSys`
   block. `MmPf` holds the same PEF as `INIT 0`, untouched. *Why not
   from `INIT 0`?* At startup the Code Fragment Manager relocates
   `INIT 0`'s data section **in place**, so a copy taken from it would
   be relocated twice, and the first call into it executes garbage:
   "illegal instruction".
4. Calls **`MakeDataExecutable`** on the block. `BlockMoveData` only
   goes through the data cache, and without the flush the CPU can run
   stale memory.
5. Calls `GetMemFragment(…, kLoadNewCopy)` **with the system heap as
   the current zone**, so the copy's code and data land in the system
   heap.
6. Sets `copyPending` in the status record and calls the copy's `main`.

The copy's `main()` sees the status record with `copyPending` set. It
checks that its own data and code are in the system heap (`werr`
`-5`/`-6` if not), then starts the amp monitor and the wrapper. If a
second installed copy of the extension finds a record without
`copyPending`, it steps aside.

Any failure after mac-io is found falls back to the v8.0 behaviour:
all four amps on once, and the **dash** icon.

| Boot icon | Meaning |
|---|---|
| speaker (ICN# 128) | everything running |
| speaker with dash (130) | amps handled, but resident part or wrapper not running |
| speaker with X (129) | not a Mac mini G4, or mac-io not found; nothing changed |

---

## 4. Volume control

**What works.** Every Sound Manager channel's component chain contains
an `snfx/eqal` **Frequency Equalizer**, opened per channel.
`src/VolumeCapture.c` captures it behind a stand-in with the same
description. The stand-in forwards every call to a real equalizer, and
scales each 16-bit buffer returned by `GetSourceData` by main ×
plain volume, or silences it when muted. Scaling every channel scales
the output.

The equalizer isn't registered yet when the INIT runs. So the wrapper
retries the capture from task-time GetInfo/SetInfo calls until it
succeeds, which on both the Mac mini and the emulator is the second
attempt. Channels opened before that aren't scaled. On the Mac mini
there were none (`open at init 0`); in the emulator there were 4.

Verified:
- **Emulator:** the volume sweep measures −17.5, −20.0, −23.6, −29.6
  and −41.6 dBFS for 100/75/50/25/6 %, within 0.1 dB of expected.
- **Mac mini (2026-11-06):** volume sweep heard stepping down; the
  slider and Control Strip change the level; 600 buffers scaled in one
  session.

**What failed before** (the driver path):

| Approach | Result |
|---|---|
| Forward volume to the mixer given in `SetSource` | The wrapper never receives `SetSource` |
| Capture the mixer component (`mixr/mixw/appl`) | The driver's mixer is an instance the Sound Manager opened itself; the stand-in is only asked its version |
| `SetSource` on the driver to a scaling stage | Returns garbage; the driver keeps its mixer |
| `SetInfo` `volu`/`hvol` on the Mixer, `sourceID` NULL | −205 (`badChannel`): per-channel volume only |

Open question: the Control Strip sets `volu` with unequal channel
values (e.g. `00A500F4`, right 0xA5 / left 0xF4) that are scaled as
given.

---|---|
| Forward volume to the mixer given in `SetSource` | The wrapper never receives `SetSource`; the driver sets up its own mixer |
| Capture the mixer component (`mixr/mixw/appl`, the only one) behind a scaling stand-in | The stand-in is opened once, asked only its version; the driver's mixer is a different instance the Sound Manager opened itself |
| `GetSource` on the driver | Works: returns that Mixer instance |
| `GetSource` on the mixer | −205: one source per channel |
| `SetSource` on the driver to a scaling stage | Returns garbage; the driver keeps its mixer |
| `SetInfo` `volu`/`hvol` on the Mixer, `sourceID` NULL | −205 (`badChannel`): the mixer only takes per-channel volume |

Untried ideas:
- force the Sound Manager to rebuild its output chain after the mixer
  is captured;
- scale the I2S DMA buffers directly (DBDMA TX at `mac-io + 0x8200`).

The scaling arithmetic is already written and tested: `VolumeCombine`
and `VolumeScale16` in `src/VolumeLogic.c`.

Because silence can be done in hardware, **Mute and volume 0 work**:
the amp monitor switches both output amps off.

---

## 5. Building

### Requirements

- A Retro68 PowerPC toolchain, by default in `~/Retro68-toolchain`;
  set `RETRO68` to use another path.
- `python3`, `xxd`, and a host `cc` for the tests.

### Extension

```
Retro68/build.sh [stage]       # stage 0–3, default 3 (release)
Retro68/check.sh               # resources, PEF main, imports per library
BREADCRUMBS=1 Retro68/build.sh 3   # debug: a dash icon per startup step
EMULATOR=1 Retro68/build.sh 3      # no Mac-mini check, for QEMU only
```

| Stage | Adds |
|---|---|
| 0 | v8.0 one-shot fix (all amps on once) |
| 1 | resident copy, amp monitor, status record |
| 2 | volume wrapper answering reads (alerts play) |
| 3 | volume changes stored, mute in hardware (**release**) |

Each build is copied to `Retro68/build/stage<N>/`. It produces two files:
- `MacMini Audio Fix.bin`: MacBinary, which keeps the resource fork;
- `MacMini Audio Fix.dsk`: a raw HFS image.

Disk Copy 6.5 may refuse the raw image. If so, decode the `.bin` with
StuffIt Expander instead.

### Host tests

```
make -C tests test
```

These cover:
- `AmpLogic`: debounce, the pin table, silent mode, corrections;
- `VolumeLogic`: clamp, combine, 16-bit scaling;
- `HeapCheck`;
- `MachineLogic`: Mac mini, Cube and QEMU model strings.

### AudioProbe (diagnostic app)

```
cmake -S AudioProbe -B AudioProbe/build \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/Retro68-toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
cmake --build AudioProbe/build
```

AudioProbe writes `AudioProbe Log` next to itself. It covers:
- the amp-pin checkboxes and the live headphone-detect readout;
- test tones at several rates and sizes, SysBeep and alert-path tests,
  and a volume sweep, each with Yes/No answers;
- the status record, including where the code and data landed;
- an import check against the stage 3 extension;
- a list of every registered component and how many instances are open.

`AudioProbe/Imports.h` is generated from `check.sh` output; regenerate
it if the extension's imports change.

### Packaging experiments

```
Retro68/variants.sh "path/to/working v8 extension"
```

This builds the INIT packaging variants used to find the error 1010
fix, next to a known-good CodeWarrior build.

---

## 6. Installing

1. Copy `MacMini Audio Fix.bin` to the Mac mini and decode it with
   StuffIt Expander.
2. Put **MacMini Audio Fix** in `System Folder:Extensions:` (replacing
   any older version) and restart.
3. To disable it, hold **Shift** at startup or use Extensions Manager.

---

## 7. Repository layout

```
src/                 extension source
  Main.c             startup: status record, resident copy, fallbacks, icons
  MacIO.c            mac-io lookup and register access
  AmpLogic.c         pin rules (pure, host-tested)
  AmpMonitor.c       Time Manager task
  VolumeWrapper.c    'awgc' wrapper: volume, mute, SysBeep
  VolumeCapture.c    software volume: captures the per-channel equalizer
  VolumeLogic.c      volume clamp / combine / scale (pure, host-tested)
  HeapCheck.c        address-range check (pure, host-tested)
  MachineLogic.c     is this a Mac mini G4? (pure, host-tested)
  Status.h           status record shared with AudioProbe
MacMiniAudioFix.r    vers, STR, ICN#, FREF, BNDL resources
Retro68/             build.sh, check.sh, repack_pef.py, variants.sh, glue.c,
  include/           stand-in headers missing from the Multiversal Interfaces
tests/               host unit tests
AudioProbe/          diagnostic application
docs/superpowers/    v3 design spec (with outcome) and implementation plan
```

## 8. History

- **v8.0 (CodeWarrior):** a one-shot INIT that enabled the four amp
  GPIOs at boot. It found that `0x6E`/`0x6F`/`0x70`/`0x79` are the amp
  enables, after failed attempts at TAS3004 I2C writes, active-low
  GPIO writes and FCR1 bits.
- **v3 (Retro68):**
  - resident amp monitor with headphone switching and keep-alive;
  - SysBeep fix;
  - stored volume settings;
  - hardware mute;
  - a reproducible build on a modern Mac.
