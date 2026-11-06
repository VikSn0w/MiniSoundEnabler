# Release posts — MacMini Audio Fix 3.0

Ready to paste. Upload these from `release/dist/`:
- `MacMiniAudioFix-3.0.img.bin` (or `.hqx` where only BinHex is accepted);
- `MacMiniAudioFix-3.0-source.zip`.

Replace `[download link]` and `[source link]` with wherever the files
end up (for example the Macintosh Garden page once it's approved).

---

## 1. Mac OS 9 Lives forum (primary)

Board: the Mac mini G4 / unsupported hardware section.

**Title:** MacMini Audio Fix 3.0: speaker, headphone switching, alerts and volume on the Mac mini G4

**Body:**

Hi all,

Here's MacMini Audio Fix 3.0, a system extension that makes the built-in audio on the Mac mini G4 usable under MacOS9Lives.

Under OS 9 the mini's sound driver leaves the output amplifiers switched off: the internal speaker is silent and the headphone jack barely works. The old 8.0 fix turned the amps on once at startup. 3.0 stays resident and adds:

- automatic speaker/headphone switching when you plug in or unplug;
- system alert sounds (they were silent);
- a working volume slider and Control Strip volume. The mini has no hardware volume, so the extension scales the sound in software;
- mute and volume 0 switch the amps off, so there's no hiss.

It only runs on a Mac mini G4. The extension checks the boot ROM, since MacOS9Lives presents the mini as a G4 Cube. On any other Mac it shows a crossed-out icon at startup and does nothing.

Install: drag "MacMini Audio Fix" onto the closed System Folder and restart. Remove any older version first. The disk image has a Read Me with the startup icons and troubleshooting. It also has AudioProbe, the diagnostic tool I used to work all this out. If something doesn't work, run AudioProbe once and post the "AudioProbe Log" it writes.

Tested on an early 2005 mini (PowerMac10,1) with 9.2.2. Reports from late 2005 (10,2) owners are very welcome.

Download: [download link]
Source (GPLv3) and technical notes: [source link]

Thanks to everyone here who made OS 9 on the mini possible in the first place.

---

## 2. Macintosh Garden (app submission form)

- **Title:** MacMini Audio Fix
- **Version:** 3.0
- **Year:** 2026
- **Author:** Vittorio Picone
- **Category:** Utilities → System Extensions (or Sound)
- **License:** Freeware / Open source (GPLv3)
- **Compatibility:** Mac OS 9.2.2 (MacOS9Lives), PowerPC. Mac mini G4 only
- **Files:** `MacMiniAudioFix-3.0.img.bin`, `MacMiniAudioFix-3.0-source.zip`

**Description:**

A system extension that makes the built-in audio of the Mac mini G4 work under Mac OS 9.2.2 with MacOS9Lives. Under OS 9 the mini's sound driver leaves the amplifiers switched off. MacMini Audio Fix turns them on and adds automatic speaker/headphone switching, system alert sounds, a working volume control (in software, as the mini has none in hardware), and true silence on mute.

It checks the boot ROM and does nothing on any Mac other than a Mac mini G4. Includes the AudioProbe diagnostic tool and a Read Me. Source code and technical notes are included in the source archive.

---

## 3. Macintosh Repository (upload form)

- **Name:** MacMini Audio Fix 3.0
- **Category:** System Extensions / Utilities
- **Developer:** Vittorio Picone
- **Year:** 2026
- **Architecture:** PowerPC
- **System requirements:** Mac OS 9.2.2 with MacOS9Lives, Mac mini G4 only
- **License:** GPLv3

**Description:** same as the Macintosh Garden text above.

---

## 4. 68kMLA forum

Board: the PowerPC / "Hacks & Development" section.

**Title:** Released: MacMini Audio Fix 3.0, built-in audio for the Mac mini G4 under OS 9

**Body:**

For anyone running OS 9 on a Mac mini G4 via MacOS9Lives: I've released MacMini Audio Fix 3.0, an extension that gets the built-in audio fully working. That means the speaker and headphone jack, automatic switching between them, alert sounds, volume and mute.

Some technical background, since this crowd may enjoy it. The mini has no TAS3004 codec, so there's no hardware volume at all, and the amps hang off GPIO pins on the KeyLargo mac-io. The extension runs a Time Manager task that watches the headphone-detect GPIO and drives the amp pins. Volume is done by capturing the Sound Manager's per-channel Frequency Equalizer component behind a stand-in that scales each 16-bit buffer. It's built with Retro68 as an accelerated INIT that copies itself into the system heap.

It refuses to run on anything but a Mac mini G4. It reads the boot ROM's model, because MacOS9Lives presents the mini as a PowerMac5,1 Cube.

Download: [download link]
Source (GPLv3), with a long TECHNICAL.md on everything that did and didn't work: [source link]

---

## 5. Emaculation forum

Board: "Mac OS 9 / Classic" general, or QEMU.

**Title:** MacMini Audio Fix 3.0 (real-hardware Mac mini G4 audio extension, developed partly in QEMU)

**Body:**

This one is for real hardware: a Mac OS 9 extension that makes the built-in audio work on the Mac mini G4 under MacOS9Lives. That covers the speaker, headphone switching, alert sounds and volume. It refuses to run on other machines, emulated ones included, so it's of no use inside QEMU itself.

It may interest people here because a lot of the development happened in QEMU. I used `mac99` with Mark Cave-Ayland's screamer branch and an unattended test loop: build, install onto the VM disk from the host, boot headless with the audio recorded to WAV, then measure the levels. That's how the software volume was verified before it went to the real mini. The scripts are in the source archive under `emu/`.

Download: [download link]
Source: [source link]

---

## 6. r/VintageApple

**Title:** I wrote a Mac OS 9 extension that fixes the built-in audio on the Mac mini G4 (speaker, headphone switching, volume)

**Body:**

If you run OS 9 on a Mac mini G4 with MacOS9Lives, you've probably noticed the internal speaker is silent and the headphone jack is barely audible. The driver never turns the amplifiers on.

MacMini Audio Fix 3.0 fixes that. It turns the amps on, switches between speaker and headphones automatically, makes alert sounds play, and makes the volume slider actually work. The mini has no hardware volume, so it's done in software. It only runs on the Mac mini G4 and does nothing anywhere else.

Free and open source (GPLv3). Download and source: [download link]

Happy to answer questions about how it works. Getting volume control working took a lot of digging into the Sound Manager.
