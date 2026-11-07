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

## Download and install

Get `MiniSoundEnabler-3.0.img.bin` from the
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

## License

GPLv3. Copyright 2026 Vittorio Picone.
