# Release posts: Mini Sound Enabler 3.1

Ready to paste. Upload these from `release/dist/`:
- `MiniSoundEnabler-3.1.img.bin` (or `.hqx` where only BinHex is accepted);
- `MiniSoundEnabler-3.1-source.zip`.

The links point to the GitHub release and repository. Swap in the
Macintosh Garden page once it's approved if you prefer.

---

## 1a. Mac OS 9 Lives: reply in the release thread (3.1, FireWire)

The release post below is already up (as 3.0). Reply to the FireWire question with:

Thanks for pointing at that issue. 3.0 most likely had the same problem. Like the original MacMiniAudioFix, it switched on four pins (0x6E, 0x6F, 0x70, 0x79), and it kept re-enabling them while it ran. The mini's own device tree only lists 0x6F (headphone amp), 0x70 (speaker amp) and 0x67 (headphone detect) as audio pins, and in my tests the sound played with 0x6E or 0x79 off. So those two are the likely reason FireWire stopped working.

3.1 is up and doesn't touch them anymore: https://github.com/VikSn0w/MiniSoundEnabler/releases/tag/v3.1

I don't have a FireWire drive to test with. If MickBehn or anyone else could try 3.1 with one, that would settle it. And if your sound goes silent with 3.1, please say so too, along with an AudioProbe log.

---

## 1. Mac OS 9 Lives forum (primary)

Board: the Mac mini G4 / unsupported hardware section. If UnexpectedBomb's
Mini Audio thread is there, this also works as a reply to it.

Title: Mini Sound Enabler 3.1: speaker, headphone switching, alert sounds and slider volume on the Mac mini G4

Body:

Hi all,

I've released Mini Sound Enabler 3.1, a single extension that gets the Mac mini G4's built-in audio fully working under MacOS9Lives. It started as Matouš Pikous's MacMiniAudioFix, which found the amp pins and switched them on at startup. Version 3.1 stays running after startup and adds:

- automatic speaker/headphone switching when you plug in or unplug;
- system alert sounds, which were silent before;
- volume from the normal Sound control panel slider and the Control Strip, with nothing to select. The mini has no hardware volume, so the extension scales the sound in software;
- mute and volume 0 switch the amps off, so there's no hiss.

I know UnexpectedBomb has been working on the same machine with a Control Strip module and a selectable sound output. We arrived at the same pin map (0x6F headphones, 0x70 speaker, 0x67 headphone detect), and the volume ended up working by a different route. Their write-up says capturing a sound component from an extension never gets called. That matches what I saw for the driver and the mixer. The per-channel Frequency Equalizer is different: the Sound Manager opens it later, for each channel, so a stand-in captured in its place does get every buffer and can scale it. The details are in TECHNICAL.md in the source.

Don't run both at once: their Control Strip module and this extension each drive the same amp pins and would fight over them.

Install: remove any older MacMini Audio Fix, drag "Mini Sound Enabler" onto the closed System Folder and restart. It's for the Mac mini G4 only; on other Macs the same pins do something else. The disk image has a Read Me with the startup icons and troubleshooting, plus AudioProbe, the diagnostic app I used throughout. If something doesn't work, run AudioProbe once and post the "AudioProbe Log" it writes.

Tested on an early 2005 mini (PowerMac10,1) with 9.2.2. I'd like to hear from anyone with a late 2005 model (10,2).

Download: https://github.com/VikSn0w/MiniSoundEnabler/releases/tag/v3.1
Source (GPLv3) and technical notes: https://github.com/VikSn0w/MiniSoundEnabler

Thanks to Matouš Pikous for the original fix, to UnexpectedBomb for the write-up, and to everyone here who got OS 9 running on the mini in the first place.

---

## 2. Macintosh Garden (app submission form)

- Title: Mini Sound Enabler
- Version: 3.1
- Year: 2026
- Author: Vittorio Picone, based on MacMiniAudioFix by Matouš Pikous
- Category: Utilities / System Extensions (or Sound)
- License: Freeware / Open source (GPLv3)
- Compatibility: Mac OS 9.2.2 (MacOS9Lives), PowerPC. Mac mini G4 only
- Files: `MiniSoundEnabler-3.1.img.bin`, `MiniSoundEnabler-3.1-source.zip`

Description:

A system extension that makes the built-in audio of the Mac mini G4 work under Mac OS 9.2.2 with MacOS9Lives. Under OS 9 the mini's sound driver leaves the amplifiers switched off. Mini Sound Enabler turns them on and adds automatic speaker/headphone switching, system alert sounds, a working volume control (in software, as the mini has none in hardware), and no hiss when muted.

For the Mac mini G4 only; do not install it on other Macs. Based on MacMiniAudioFix by Matouš Pikous. Includes the AudioProbe diagnostic tool and a Read Me. The source archive has the code and the technical notes.

---

## 3. Macintosh Repository (upload form)

- Name: Mini Sound Enabler 3.1
- Category: System Extensions / Utilities
- Developer: Vittorio Picone (based on MacMiniAudioFix by Matouš Pikous)
- Year: 2026
- Architecture: PowerPC
- System requirements: Mac OS 9.2.2 with MacOS9Lives, Mac mini G4 only
- License: GPLv3

Description: same as the Macintosh Garden text above.

---

## 4. 68kMLA forum

Board: the PowerPC / "Hacks & Development" section.

Title: Released: Mini Sound Enabler 3.1, built-in audio for the Mac mini G4 under OS 9

Body:

For anyone running OS 9 on a Mac mini G4 via MacOS9Lives: I've released Mini Sound Enabler 3.1, an extension that gets the built-in audio fully working. That means the speaker and headphone jack, automatic switching between them, alert sounds, volume and mute.

It builds on Matouš Pikous's MacMiniAudioFix, which found the amp pins. How it works: the mini has no TAS3004 codec, so there's no hardware volume at all, and the amps hang off GPIO pins on the KeyLargo mac-io. The extension runs a Time Manager task that watches the headphone-detect GPIO and drives the amp pins. Volume is done by capturing the Sound Manager's per-channel Frequency Equalizer component behind a stand-in that scales each 16-bit buffer. It's built with Retro68 as an accelerated INIT that copies itself into the system heap.

Download: https://github.com/VikSn0w/MiniSoundEnabler/releases/tag/v3.1
Source (GPLv3), with a long TECHNICAL.md on everything that did and didn't work: https://github.com/VikSn0w/MiniSoundEnabler

---

## 5. Emaculation forum

Board: "Mac OS 9 / Classic" general, or QEMU.

Title: Mini Sound Enabler 3.1 (real-hardware Mac mini G4 audio extension, developed partly in QEMU)

Body:

This one is for real hardware: a Mac OS 9 extension that makes the built-in audio work on the Mac mini G4 under MacOS9Lives. That covers the speaker, headphone switching, alert sounds and volume. It's for the Mac mini G4 only, so it's of no use inside QEMU itself.

It may interest people here because a lot of the development happened in QEMU. I used `mac99` with Mark Cave-Ayland's screamer branch and an unattended test loop: build, install onto the VM disk from the host, boot headless with the audio recorded to WAV, then measure the levels. That's how I checked the software volume before trying it on the real mini. The scripts are in the source archive under `emu/`.

Download: https://github.com/VikSn0w/MiniSoundEnabler/releases/tag/v3.1
Source: https://github.com/VikSn0w/MiniSoundEnabler

---

## 6. r/VintageApple

Title: I wrote a Mac OS 9 extension that fixes the built-in audio on the Mac mini G4 (speaker, headphone switching, volume)

Body:

If you run OS 9 on a Mac mini G4 with MacOS9Lives, you've probably noticed the internal speaker is silent and the headphone jack is barely audible. The driver never turns the amplifiers on.

Mini Sound Enabler 3.1 fixes that. It turns the amps on, switches between speaker and headphones automatically, makes alert sounds play, and makes the volume slider actually work. The mini has no hardware volume, so it's done in software. It's for the Mac mini G4 only.

It builds on Matouš Pikous's original MacMiniAudioFix. Free and open source (GPLv3). Download and source: https://github.com/VikSn0w/MiniSoundEnabler/releases/tag/v3.1

Happy to answer questions about how it works. Getting volume control working took a lot of digging into the Sound Manager.
