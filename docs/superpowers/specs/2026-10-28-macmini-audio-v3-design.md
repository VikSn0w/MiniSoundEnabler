# MacMini Audio Fix v3 — Design

Date: 2026-10-28
Status: approved design, awaiting spec review

## Goal

Turn the one-shot MacMini Audio Fix INIT into a resident extension that:

1. switches between the internal speaker and the headphone jack automatically;
2. keeps the amplifiers enabled if anything turns them off after boot;
3. makes system alert sounds (SysBeep) play;
4. makes the Sound control panel's volume slider change the output level.

Constraint: v3 must never leave the machine worse off than the current
extension. Anything that fails to install falls back to today's behaviour.

## What we know (from AudioProbe v1–v3 on the Mac mini, 2026-10-28)

Hardware: Mac mini G4, OS 9.2.2 (MacOS9Lives), `mach` 406, mac-io at
physical = logical `0x80000000`.

### Amp pins

Tested by ear with headphones plugged in and unplugged.

| Pin | Turned off → heard | Role |
|---|---|---|
| `0x6E` | speaker + jack | no audible effect |
| `0x6F` | speaker only | headphone jack amp |
| `0x70` | jack only | internal speaker amp |
| `0x79` | speaker + jack | no audible effect |

Pin values: `0x05` written = enabled (reads back `0x07`), `0x04` = disabled.

### Headphone detect

`0x67` bit 1: `1` = headphones out (`0x82`), `0` = headphones in (`0x80`).
`0x51` changes alongside it (`0xFF` out / `0x7F` in).

### Amps get turned off after boot

AudioProbe v2 started with headphones in and found `6E=04 6F=07 70=04 79=04`.
The INIT had set all four to `07`. Plugging headphones in at run time (v3)
did not change any pin, so the cause is something else, possibly booting
or waking with headphones in. Not confirmed.

### Audio clock and formats

- The I2S serial format register (`mac-io + 0x10010`) reads `0x00003A41`
  and never changed during any test.
- Every test tone was heard: 11.127, 22.254, 22.05, 44.1 and 48 kHz, in
  8-bit and 16-bit. Sample rate and format are not a problem.
- Note: 32-bit mac-io registers must be read with 32-bit loads; byte
  reads gave wrong values.

### SysBeep

There is a single sound output component: `sdev/awgc/appl`, named
"Built-in", flags `8000000C`.

Default-output info:

| Selector | Result |
|---|---|
| `volu`, `hvol`, `hstp`, `hmut` | `-32766` (badComponentSelector) |
| `srat` | 44.1 kHz |
| `ssiz` | 16 |
| `chan` | 2 |
| `hwfm` | answers (`twos`) |
| `hwbs` | answers |

- `GetDefaultOutputVolume` fails with `-32766`.
- `SetDefaultOutputVolume` and `SetSysBeepVolume` return `noErr`.
- `sysBeepState` is `0001` (enabled). Alert volume is `0x01000100` (full).
- `SndPlay` of `snd 1` is heard, with or without a `volumeCmd` first.
  SysBeep is silent in every variant, headphones in or out.
- When an alert should beep, **the menu bar flashes**.

Conclusion: SysBeep reads the output volume, gets an error, treats it as
zero and flashes the menu bar instead of beeping. The same missing volume
support is why the volume slider does nothing.

## Architecture

There is still one INIT file, "MacMini Audio Fix" v3, built with the
existing Retro68 accelerated-INIT packaging (`Retro68/build.sh`). That
packaging is a Mixed Mode routine descriptor followed by a PEF in `INIT 0`.

| File | Responsibility | Depends on |
|---|---|---|
| `Main.c` | INIT entry. Finds mac-io, starts AmpMonitor then VolumeWrapper, publishes status, shows the boot icon | MacIO, AmpMonitor, VolumeWrapper |
| `MacIO.c/.h` | Locates mac-io in the Name Registry; GPIO read/write; 32-bit LE register reads | NameRegistryLib, DriverServicesLib |
| `AmpMonitor.c/.h` | Headphone detection and amp keep-alive on a Time Manager task | MacIO |
| `VolumeWrapper.c/.h` | Sound output component wrapping `awgc`; supplies software volume | Component Manager, SoundLib |
| `Status.h` | Layout of the status record shared with AudioProbe | — |

`AudioProbe` stays in the repo as the diagnostic tool and gains a status
readout (v4).

### Staying resident

After `main()` runs, the INIT's code must stay in memory:

- the INIT resource is detached and locked in the system heap;
- all long-lived state (monitor globals, the Time Manager task, the
  component's globals, routine descriptors) is allocated with
  `NewPtrSys`.

This is the riskiest mechanical step, so test stage 1 checks it on its own.

### Startup order

1. Check that the Name Registry is present and find mac-io. If either
   fails: X icon, return.
2. Allocate resident state. If that fails: write `0x05` to all four amp
   pins once (today's behaviour), show the dash icon, return.
3. Start AmpMonitor. It applies the pin table immediately, then arms its
   timer.
4. Attach VolumeWrapper. If that fails, the dash icon is shown.
5. Publish the status record. Show the OK icon if both parts are running.

### Boot icons

All three already exist in `MacMiniAudioFix.r`.

| ICN# | Meaning |
|---|---|
| 128, speaker | both parts running |
| 130, speaker with dash | amps handled; volume wrapper not attached, or not resident (one-shot fallback) |
| 129, speaker with X | mac-io or Name Registry not found; nothing changed |

## AmpMonitor

- A Time Manager task with a 250 ms period, re-armed with `PrimeTime`
  each tick.
- It runs at interrupt time, so it only reads and writes GPIO bytes:
  no allocation, no Toolbox calls other than `PrimeTime`.
- **Each tick:**
  1. Read `0x67`, bit 1. A change in headphone state only takes effect
     after two consecutive matching reads (~0.5 s debounce).
  2. Desired pin state:

     | | `0x70` speaker | `0x6F` jack | `0x6E` | `0x79` |
     |---|---|---|---|---|
     | headphones out | on | off | on | on |
     | headphones in | off | on | on | on |

  3. For each pin whose output-enable bit (bit 0) differs from the desired
     state, write `0x05` (on) or `0x04` (off). Pins already correct are
     not written.
- **At install:** apply the table immediately using a single read of
  `0x67`.
- **Out of scope:** a setting to play both outputs together, and fading
  between outputs.
- **Known risk:** if the sound driver keeps rewriting the pins, the two
  could alternate and cause clicks. Mitigation if seen in testing: back
  off for a few seconds after correcting a pin.

## VolumeWrapper

### Attach

1. Find the component: `FindNextComponent` for type `sdev`, subtype
   `awgc`, manufacturer `appl`.
2. Record `CountComponentInstances` for the status record. If the count
   is above zero, the Sound Manager already has the real driver open and
   capture won't affect that instance.
3. Register the wrapper as a global component with the same
   type/subtype/manufacturer. Its dispatch routine runs through a routine
   descriptor in the system heap.
4. Call `CaptureComponent(awgc, wrapper)`.
5. If any step fails: undo what was done, report the failure in the
   status record, show the dash icon.

### Open and close

- Wrapper open opens the captured `awgc` and keeps that instance.
- If that open fails, the wrapper calls `UncaptureComponent` and returns
  the error, so the next open goes straight to the original driver.
- Close closes the inner instance.

### Delegation

Every selector is forwarded with `DelegateComponentCall`, except the
following:

| Selector | Wrapper behaviour |
|---|---|
| SetSource | forward, and remember the source component (the mixer) |
| GetInfo `volu` | return the stored volume, `noErr` |
| SetInfo `volu` | clamp each channel to `0x0100`, store, and forward as SetInfo `siVolume` to the remembered source so the mixer scales samples; return `noErr` |
| GetInfo / SetInfo `hvol`, `hstp`, `hmut` | forward (stays unsupported), so the Sound Manager sees no hardware volume |

- The stored volume starts at `0x01000100`. The Sound Manager's restore
  of the saved volume at boot arrives as a SetInfo `volu`.
- If testing shows SysBeep reads `hvol` rather than `volu`, the wrapper
  answers `hvol` from the same stored value. Stage 2 decides this.
- **Open question for stage 3:** whether forwarding `siVolume` to the
  mixer scales the output. If it does not, alerts are still fixed
  (stage 2), and real volume needs a separate design.
- **Out of scope:** a working Mute checkbox.

## Status record

The INIT registers Gestalt selector `'MmAf'`, which returns a pointer to
a struct in the system heap:

```c
typedef struct {
    UInt16 version;          /* 3 */
    UInt16 flags;            /* bit0 resident, bit1 monitor running,
                                bit2 wrapper attached */
    UInt32 macIOBase;
    SInt16 wrapperErr;       /* first failing step's error, 0 if none */
    UInt16 instancesAtInit;  /* awgc instances already open at attach */
    UInt32 volume;           /* stored right:left volume */
    UInt32 ticks;            /* AmpMonitor tick counter */
    UInt32 corrections;      /* pin writes made by AmpMonitor */
    UInt8  headphones;       /* debounced state: 1 = in */
} MmAfStatus;
```

AudioProbe v4 reads it, shows it live and writes it to the log. This
gives visibility into the INIT without any logging from inside it.

## Testing

### On this Mac (every build)

Structural checks like the current build's:

- all resources present: `INIT 0`, `ICN#` 128–130, `vers`, `STR `,
  `BNDL`, `FREF`;
- the PEF `main` symbol resolves to the descriptor;
- imports come only from the expected libraries: InterfaceLib,
  NameRegistryLib, DriverServicesLib, SoundLib.

A QEMU mac99 boot is optional and only shows that the INIT loads without
crashing.

### On the Mac mini (one round per stage)

1. **Resident AmpMonitor and status record** (no wrapper):
   - plug and unplug headphones: outputs switch within ~0.5 s;
   - boot with headphones in;
   - sleep, then wake: amps restored.
   - AudioProbe v4 status: `ticks` increasing, `corrections` sensible,
     `headphones` matches the jack.
2. **Wrapper with volume read:** SysBeep beeps, no menu-bar flash.
   Status shows attached, `instancesAtInit`, `volume`.
3. **Wrapper with volume set:** move the Sound control panel slider.
   The new AudioProbe test plays a tone at stepped volumes; for each,
   answer whether it got quieter.

## Build and repository layout

- Sources move into `MacMiniAudioFix - max sound/src/`:
  `Main.c`, `MacIO.c/.h`, `AmpMonitor.c/.h`, `VolumeWrapper.c/.h`,
  `Status.h`.
- `MacMiniAudioFix.c` (v8.0) is kept until v3 is verified, then removed.
- `Retro68/build.sh`:
  - compiles all sources;
  - adds SoundLib to the link;
  - adds stand-in declarations for the Component Manager, Time Manager
    and Sound component calls the Multiversal headers lack, alongside
    `NameRegistry.h` and `DriverServices.h`.
- `MacMiniAudioFix.r` changes: version strings → 3.0; the ICN# 130
  comment updated to "partial".
- `TECHNICAL.txt` gains the findings above: pin map, headphone detect,
  SysBeep cause.

## Outcome (2026-11-02)

Tested on the Mac mini:

| Goal | Result |
|---|---|
| Speaker/headphone switching | Works |
| Amps kept on | Works |
| SysBeep plays | Works: the wrapper answers `hvol`, which is what SysBeep reads |
| Volume slider / Control Strip / mute change the level | Not achieved |

The volume controls keep their settings, but the level doesn't change.

Changes from this design:

- **Packaging.** The INIT's PEF is rewritten to CodeWarrior's layout.
  MakePEF's layout fails at startup with error type 1010.
- **Residency.** The INIT is resident through a copy of its PEF, from
  `'MmPf'` 0, prepared in the system heap. The startup run lives in
  the temporary INIT-time zone, and its data is relocated in place.
- **Volume settings.** The wrapper stores `volu`, `hvol` and `hmut`
  separately. It never forwards them to the mixer.
- **Status record.** It is version 4, with residency and volume
  counters.

Software volume was investigated in depth and not solved. The driver
pulls from an Apple Mixer instance that the Sound Manager opens
outside the capture. The driver ignores SetSource, and the mixer
takes volume only per channel. The details are in TECHNICAL.txt,
under "Volume control".
