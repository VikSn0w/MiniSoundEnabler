# Testing in QEMU

Most of the extension's behaviour can be tested in an emulated Mac
instead of on the Mac mini: loading at startup, residency, the status
record, the volume wrapper, and above all the Sound Manager → mixer →
driver chain, where software volume is still unsolved.

The emulated Mac is QEMU's `mac99` (a New World G4) with
**Mark Cave-Ayland's screamer** sound device. Mac OS 9.2.2 loads its
own Built-in sound driver for it. Stock QEMU has no Mac sound
hardware.

What emulation **can't** show:
- the Mac mini's amp GPIO pins and headphone detect;
- anything MacOS9Lives does specifically for the mini;
- whether the emulated driver behaves exactly like the one on the mini.

So the final check still happens on the real machine.

## One-time setup

### 1. Build QEMU with screamer

```
git clone --depth 1 --branch screamer-v9.1.0 https://github.com/mcayland/qemu.git ~/Developer/qemu-screamer
cd ~/Developer/qemu-screamer && mkdir build && cd build
/usr/bin/python3 -m pip install --user tomli    # configure needs it on Python 3.9
../configure --python=/usr/bin/python3 --target-list=ppc-softmmu \
    --audio-drv-list=coreaudio --enable-cocoa --disable-docs --disable-werror
ninja qemu-system-ppc
```

macOS's own Python is used because configure needs `distlib`, which
Homebrew's Python 3.15 doesn't provide.

### 2. Create the VM disk

```
mkdir -p ~/Developer/os9-vm
qemu-img create -f raw ~/Developer/os9-vm/os9.img 2G
```

The disk is raw so the HFS tools on this Mac (Retro68's `hmount`,
`hcopy`…) can read and write it while the VM is off.

### 3. Install Mac OS 9.2.2 (once, in the QEMU window)

```
emu/vm.sh install
```

This boots the "MacOS 922 Universal" CD. On the OS 9 desktop:

1. Open **Applications → Drive Setup 1.9.2**. Select the 2 GB disk and
   click **Initialize**. Under **Custom Setup**, choose one partition,
   **Mac OS Standard**, named **OS9**.

   It must be *Mac OS Standard* (HFS), not *Extended*: the host-side
   HFS tools can only read and write HFS.
2. Open **Apple Software Restore** from the CD. Restore
   **MacOS9Lives.img** onto **OS9**, and tick **Erase** if it's offered.
3. Choose **Special → Shut Down**.

Then boot the installed system with:

```
emu/vm.sh run
```

The first boot of the restored system shows the Mac OS Setup
Assistant. Get past it once (⌘Q) in the `emu/vm.sh run` window.

## Running tests unattended

```
emu/test.sh                       # with the extension, tests/default.txt
emu/test.sh --no-extension        # baseline: the system on its own
emu/test.sh my-tests.txt --show   # another test list, with a window
```

The VM must be shut down. `test.sh` does the following:

1. Builds the extension (stage 3) and AudioProbe.
2. Mounts the VM disk on this Mac. The installed system is Mac OS
   Extended, which macOS mounts natively.
3. Installs the extension in `Extensions` and AudioProbe with its test
   list in `Startup Items`.
4. Boots the VM with its sound recorded to `emu/out/run.wav`.
   AudioProbe runs the list and shuts the VM down. After `TIMEOUT`
   seconds (default 300) the VM is stopped, and a screenshot is saved
   to `emu/out/last.png`.
5. Prints AudioProbe's log, and the loudness of each stretch of sound
   in the recording.

A test list has one entry per line:
- a test name exactly as on AudioProbe's buttons;
- `wait <seconds>`;
- `snapshot`;
- a line starting with `#` is skipped.

QEMU only writes the WAV while sound is playing. The silent gaps are
dropped, so the WAV's times don't line up with the log's; the order of
the sounds does. On the emulated screamer, the "Volume sweep" test
measures −2, −5, −11 and −23 dB without the extension. With it, the
sweep is flat, as on the Mac mini.

The emulator has no Mac mini GPIOs. The amp pins and headphone detect
read 0, so hardware mute and speaker/headphone switching can't be
tested here.
