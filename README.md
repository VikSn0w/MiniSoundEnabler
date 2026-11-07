# Mini Sound Enabler

A Mac OS 9 system extension that makes the built-in audio work on the
Mac mini G4 running Mac OS 9.2.2 with the MacOS9Lives patches. It was
called MacMini Audio Fix until version 3.0.

Under OS 9 the mini's sound driver leaves the output amplifiers
switched off, so the internal speaker is silent and the headphone jack
barely works. The extension switches them on and adds:

- automatic speaker/headphone switching;
- system alert sounds;
- a working volume slider and Control Strip volume, done in software
  because the mini has no hardware volume;
- mute and volume 0 that switch the amps off, so there's no hiss.

It is for the Mac mini G4 only. On other Macs the pins it writes mean
something else, so don't install it there.

## How it works

MacOS9Lives presents the mini to OS 9 as a G4 Cube, and its sound
reaches the DAC built into the mac-io controller. The amplifiers
behind that DAC are switched by GPIO pins on mac-io: `0x70` for the
speaker and `0x6F` for the headphone jack, and pin `0x67` reports
whether headphones are plugged in. The mini's device tree names these
three as its audio pins. OS 9's driver leaves both amps off.

Up to 3.0 the extension, like the original, also forced pins `0x6E`
and `0x79` on. They aren't audio pins, and forcing them is the likely
reason FireWire drives stopped working with it installed. Since 3.1
they are left alone.

**Amps and switching.** A Time Manager task runs every 250 ms. It reads
the headphone-detect pin, waits for two matching reads so a
half-inserted plug doesn't flap, and sets the speaker and headphone
amps to match. It also switches both off when the sound is muted or
at volume 0, which is why mute has no hiss.

**Alert sounds.** The extension wraps the Built-in sound driver
(`sdev/awgc`) in a component of its own. The driver doesn't answer the
Sound Manager's volume queries, and SysBeep stays silent when it
can't read a volume. The wrapper answers them and remembers what the
slider, the Control Strip and mute set.

**Volume.** The mini has no hardware volume, and the driver pulls its
sound from a mixer the extension can't reach. Every sound channel
does pass through a Frequency Equalizer component (`snfx/eqal`),
though. The extension captures that component behind a stand-in that
forwards every call and scales each 16-bit buffer by the current
volume before it moves on. The equalizer only registers after
startup, so the capture is retried until it succeeds.

**Staying resident.** It's a PowerPC INIT built with Retro68. OS 9
loads INITs into a temporary zone that is wiped when the Finder
starts, so at startup the extension loads a second, untouched copy of
its code (kept in an `'MmPf'` resource) into the system heap and runs
that copy. The status record it publishes under Gestalt `'MmAf'` is
what AudioProbe reads.

If anything after finding the hardware fails, it falls back to
switching both amps on once, as the old 8.0 fix did. TECHNICAL.md has the details.

## Download and install

Get `MiniSoundEnabler-3.1.img.bin` from the
[releases page](../../releases), expand it with StuffIt Expander and
open the image with Disk Copy. Remove any older version from the
Extensions folder, drag **Mini Sound Enabler** onto the closed System
Folder and restart. The Read Me on the image explains the startup
icons and what to do when something doesn't work.

## Building

You need a [Retro68](https://github.com/autc04/Retro68) PowerPC
toolchain (by default in `~/Retro68-toolchain`) and `python3`.

```
Retro68/build.sh            # the extension, into Retro68/build/stage3/
make -C tests test          # host unit tests
release/make_release.sh     # disk image, BinHex, MacBinary, source zip
```

`emu/` runs the extension and the AudioProbe diagnostic app in QEMU
with a recorded sound output; see [emu/README.md](emu/README.md).

[TECHNICAL.md](TECHNICAL.md) covers the hardware, how the extension
stays resident, how the volume control works and what was tried
before it did.

## Credits

Mini Sound Enabler grew out of **MacMiniAudioFix** by
[Matouš Pikous](https://github.com/matouspikous), published in
[MacLegacyLabs](https://github.com/matouspikous/MacLegacyLabs). That
extension found the amp pins and turned them on at startup. Its
hardware lookup, resources and one-shot fallback are the starting
point of this code. Everything that stays resident (switching, alerts,
volume, mute) and the Retro68 build were added by Vittorio Picone.

## License

GPLv3. Copyright 2026 Matouš Pikous and Vittorio Picone.
