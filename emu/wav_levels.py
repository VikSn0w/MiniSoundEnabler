#!/usr/bin/env python3
#
#  wav_levels.py  —  where the recorded sound is, and how loud
#
#  Usage: wav_levels.py run.wav [window_seconds]
#
#  Prints each stretch of sound (RMS above a noise floor) with its start
#  time, length and peak and RMS level in dBFS, so a test sequence's
#  tones can be matched up and their levels compared.

import math
import struct
import sys
import wave

FLOOR_DB = -60.0


def db(x):
    return 20 * math.log10(x) if x > 0 else -120.0


def main():
    path = sys.argv[1]
    win = float(sys.argv[2]) if len(sys.argv) > 2 else 0.05
    w = wave.open(path, 'rb')
    ch, width, rate, nframes = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    if width != 2:
        sys.exit('only 16-bit WAV is supported (got %d bytes/sample)' % width)
    raw = w.readframes(nframes)
    samples = struct.unpack('<%dh' % (len(raw) // 2), raw)
    print('%s: %d Hz, %d ch, %.1f s' % (path, rate, ch, nframes / rate))

    step = max(1, int(rate * win)) * ch
    segs, cur = [], None
    for i in range(0, len(samples), step):
        block = samples[i:i + step]
        rms = math.sqrt(sum(s * s for s in block) / len(block)) / 32768
        peak = max(abs(s) for s in block) / 32768
        t = i / ch / rate
        if db(rms) > FLOOR_DB:
            if cur is None:
                cur = [t, t, 0.0, 0.0, 0]
            cur[1] = t + win
            cur[2] = max(cur[2], peak)
            cur[3] += rms * rms
            cur[4] += 1
        elif cur is not None:
            segs.append(cur)
            cur = None
    if cur is not None:
        segs.append(cur)

    if not segs:
        print('  silence throughout')
    for start, end, peak, sumsq, n in segs:
        print('  %7.2f s  %5.2f s long  peak %6.1f dBFS  rms %6.1f dBFS'
              % (start, end - start, db(peak), db(math.sqrt(sumsq / n))))


main()
