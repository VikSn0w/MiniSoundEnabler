Mini Sound Enabler 3.1
for the Mac mini G4 running Mac OS 9.2.2 (MacOS9Lives)

(Formerly MacMini Audio Fix.)

Under Mac OS 9 the Mac mini G4's internal speaker is silent and the headphone jack barely works, because the sound driver leaves the output amplifiers switched off. This extension switches them on and makes the rest of the built-in audio usable.


NEW IN 3.1

Earlier versions also switched on two pins that have nothing to do with sound, which could stop FireWire drives from working. 3.1 leaves them alone.


WHAT IT DOES

- Sound from the internal speaker and the headphone jack.
- Plug in headphones and the speaker turns off; unplug them and it comes back.
- System alert sounds play.
- The volume slider (Sound control panel, Control Strip) changes the loudness.
- Mute and volume 0 switch the amplifiers off, so there is no hiss.


REQUIREMENTS

- A Mac mini G4: the early 2005 model (1.25 or 1.42 GHz) or the late 2005 one (1.33 or 1.5 GHz). It was tested on an early 2005 model.
- Mac OS 9.2.2 with the MacOS9Lives patches.

Do not install it on any other Mac, including a G4 Cube. It writes to the Mac mini's audio control pins, which mean something else on other machines.


INSTALLING

1. Remove any older version from the Extensions folder. Earlier versions were called "MacMini Audio Fix".
2. Drag "Mini Sound Enabler" onto your closed System Folder. Mac OS puts it in the Extensions folder.
3. Restart.


STARTUP ICONS

Speaker with sound waves: everything is running.

Speaker with a dash: the speaker and headphone jack are switched on, but switching, alerts and volume are not running. Restart once; if it stays, see Problems below.

Speaker crossed out: the sound hardware was not found. Nothing was changed.


UNINSTALLING

Drag "Mini Sound Enabler" from System Folder:Extensions to the Trash and restart. Holding Shift at startup, or turning it off in Extensions Manager, also disables it.


GOOD TO KNOW

- The Mac mini has no hardware volume control. The extension lowers the volume in software, sound by sound, as it is played. At full volume the sound is exactly as it was.
- The Control Strip sometimes sets the left and right channels to slightly different levels. On the mini's mono speaker this makes no difference.
- Sound input is not covered.


PROBLEMS

The Extras folder holds AudioProbe, the diagnostic tool used to develop this extension. Put it in a folder on your hard disk, run it, then quit. It writes a file called "AudioProbe Log" next to itself: send that file along with your report. Its buttons play test sounds; you don't need them for a report.


LICENSE AND SOURCE

Copyright 2026 Matous Pikous and Vittorio Picone.

Mini Sound Enabler grew out of MacMiniAudioFix by Matous Pikous, which found the Mac mini's amplifier pins and switched them on at startup. Vittorio Picone added everything that stays running: switching, alert sounds, volume and mute.

Mini Sound Enabler is free software, released under the GNU General Public License version 3. The source code and full technical notes are in MiniSoundEnabler-3.1-source.zip, published alongside this disk image.

Built with Retro68. Thanks to the MacOS9Lives community for making OS 9 run on the Mac mini at all.
