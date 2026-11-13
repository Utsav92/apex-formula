"""Procedural audio for APEX Formula: every sound is synthesised here (no recordings, nothing to license).

Run:  /Applications/Blender.app/Contents/Resources/*/python/bin/python3.13 Audio/gen_audio.py     (uses Blender's bundled numpy)
Out:  Audio/export/*.wav   (16-bit mono, 32 kHz, seamless loops: every component is an integer number of cycles per loop)
  engine_low   - V8-like firing note, fundamental 62 Hz, strong 4th-order harmonics, burble sub-harmonic
  engine_high  - same family at 5x pitch, brighter and raspier; the game crossfades low/high and pitch-shifts both with rpm
  tyre_screech - resonant noise band ~1.6-4.2 kHz with slow wobble
  wind         - pink-ish noise 60-1400 Hz
  crowd        - murmur: band noise 180-2600 Hz with slow swells (4 s loop)
  city         - distant traffic and ventilation rumble (6 s loop)
  gear_click   - short mechanical click (one-shot)
"""
import os
import wave

import numpy as np

SR = 32000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(20261113)


def write(name, x, peak=0.8):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())
    print("WROTE", name, "%.1f s" % (len(x) / SR))


def additive(L, freqs, amps):
    """Sum of sines at the given frequencies (multiples of 1/L) with random phases: perfectly periodic over L seconds."""
    t = np.arange(int(SR * L)) / SR
    x = np.zeros_like(t)
    for f, a in zip(freqs, amps):
        x += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi))
    return x


def noise_band(L, lo, hi, tilt=0.0, n=900):
    step = 1.0 / L
    fs = np.round(rng.uniform(lo, hi, n) / step) * step
    return additive(L, fs, (fs / lo) ** (-tilt))


def engine(f0, hmax, bright, rasp, L=1.0):
    hs = np.arange(1, hmax + 1)
    amp = hs ** (-bright)
    amp = amp * (1.0 + 1.6 * (hs % 4 == 0) + 0.7 * (hs % 2 == 0))                      # firing-order emphasis
    amp = amp * (1.0 + 1.8 * np.exp(-((hs * f0 - 650) / 350) ** 2) + 1.2 * np.exp(-((hs * f0 - 2100) / 700) ** 2))   # intake/exhaust formants
    x = additive(L, hs * f0, amp)
    x += 0.5 * additive(L, [f0 * 0.5, f0 * 1.5], [1.0, 0.6])                              # burble
    x += rasp * noise_band(L, 800, 6000, tilt=0.8, n=500)
    # light amplitude flutter at 1/4 of the fundamental keeps it from sounding like an organ
    t = np.arange(len(x)) / SR
    return x * (1 + 0.12 * np.sin(2 * np.pi * (f0 / 4) * t + 1.0))


write("engine_low", engine(62, 48, 0.75, 0.10))
write("engine_high", engine(310, 34, 0.55, 0.18))

x = noise_band(2.0, 1600, 4200, tilt=-0.3, n=700)
t = np.arange(len(x)) / SR
write("tyre_screech", x * (1 + 0.25 * np.sin(2 * np.pi * 11 * t)) * (1 + 0.3 * np.sin(2 * np.pi * 3 * t + 0.5)))
write("wind", noise_band(3.0, 60, 1400, tilt=0.9, n=800))

x = noise_band(4.0, 180, 2600, tilt=0.6, n=1100)
t = np.arange(len(x)) / SR
write("crowd", x * (0.75 + 0.18 * np.sin(2 * np.pi * 0.5 * t) + 0.12 * np.sin(2 * np.pi * 1.25 * t + 2.0)))
write("city", noise_band(6.0, 35, 420, tilt=1.1, n=700) + 0.25 * noise_band(6.0, 420, 1500, tilt=1.4, n=500))

n = int(SR * 0.09)
t = np.arange(n) / SR
click = np.exp(-t * 90) * np.sin(2 * np.pi * 1700 * t) + 0.6 * np.exp(-t * 160) * rng.standard_normal(n) * 0.5
write("gear_click", click, peak=0.7)

# thunder: a crack that rolls into a long low rumble with slowly shifting bands (one-shot, 6 s)
L = 6.0
n = int(SR * L)
t = np.arange(n) / SR
rumble = noise_band(L, 28, 160, tilt=1.4, n=700) * np.exp(-t / 2.2) * (1 + 0.35 * np.sin(2 * np.pi * 0.9 * t))
crack = noise_band(L, 250, 5000, tilt=0.5, n=500) * np.exp(-t * 7.0)
roll = noise_band(L, 60, 500, tilt=1.0, n=500) * np.exp(-((t - 1.6) / 1.1) ** 2) * 0.6
write("thunder", rumble + 0.9 * crack + roll, peak=0.9)
