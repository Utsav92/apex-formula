"""Original soundtrack + effects mix for the 120 s showcase. Run with Blender's Python (has numpy):
  /Applications/Blender.app/Contents/MacOS/Blender -b -P Audio/mix_showcase.py
Uses the project's own synthesised stems (Audio/export/*.wav), synthesises original music, and macOS `say` for commentary lines (placeholder voice).
Out: Audio/export/showcase_mix.wav (48 kHz stereo)."""
import numpy as np, wave, os, subprocess, tempfile
R = 48000; T = 120; N = R * T
here = os.path.dirname(os.path.abspath(__file__)); EX = os.path.join(here, "export")
rng = np.random.default_rng(2026)

def load(n):
    w = wave.open(os.path.join(EX, n + ".wav")); a = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32) / 32768
    sr = w.getframerate(); x = np.linspace(0, len(a) - 1, int(len(a) * R / sr)); return np.interp(x, np.arange(len(a)), a)

def loop(a, secs, pitch=1.0):
    n = int(secs * R); idx = (np.arange(n) * pitch).astype(np.int64) % len(a); return a[idx]

def env(t0, t1, fi=1.0, fo=1.0, g=1.0):
    e = np.zeros(N, np.float32); i0, i1 = max(int(t0 * R), 0), min(int(t1 * R), N); n = i1 - i0
    if n <= 0: return e
    c = np.ones(n, np.float32); fi_n, fo_n = int(fi * R), int(fo * R)
    if fi_n: c[:fi_n] = np.linspace(0, 1, fi_n)[:n]
    if fo_n: c[-fo_n:] = np.minimum(c[-fo_n:], np.linspace(1, 0, fo_n))
    e[i0:i1] = c * g; return e

def place(sig, t0, g=1.0):
    out = np.zeros(N, np.float32); i = int(t0 * R); m = min(len(sig), N - i)
    if m > 0: out[i:i + m] = sig[:m] * g
    return out

def lp(x, fc):
    a = np.exp(-2 * np.pi * fc / R); y = np.empty_like(x); s = 0.0
    for i in range(len(x)):  # one-pole
        s = (1 - a) * x[i] + a * s; y[i] = s
    return y

mix = np.zeros(N, np.float32)
# --- ambience -------------------------------------------------------------------------------------------------------------------------------
city = loop(load("city"), T); mix += city * env(0, 30, 0.5, 4, 0.55) + city * env(30, T, 4, 3, 0.12)
wind = loop(load("wind"), T); mix += wind * env(0, 10, 0.5, 2, 0.5)
crowd = loop(load("crowd"), T); mix += crowd * (env(20, 46, 3, 3, 0.55) + env(98, 120, 2, 3, 0.75))
# --- engines: gain per scene; pitch ramp on the grid (revving), steady on track, quieter in the pit lane --------------------------------------
lo, hi = load("engine_low"), load("engine_high")
def eng(t0, t1, p0, p1, g, fi=0.5, fo=0.5):
    n = int((t1 - t0) * R); pitch = np.linspace(p0, p1, n); phase = np.cumsum(pitch); a = lo[phase.astype(np.int64) % len(lo)] * 0.6 + hi[phase.astype(np.int64) % len(hi)] * np.clip((p1 - 1) * 1.5, 0, 1)
    return place(a * env(0, t1 - t0, fi, fo, g)[:n] if False else a * np.interp(np.arange(n), [0, fi * R, n - fo * R, n], [0, 1, 1, 0]), t0, g)
# --- effects -------------------------------------------------------------------------------------------------------------------------------
tyre = load("tyre_screech"); mix += place(tyre[:int(2.2 * R)], 30.2, 0.35) + place(tyre, 52, 0.18) + place(tyre, 107.5, 0.3)
gear = load("gear_click")
for t in np.concatenate([np.arange(31, 40, 1.7), np.arange(44, 66, 2.6), np.arange(100, 111, 2.2)]): mix += place(gear, float(t), 0.6)
thunder = load("thunder"); mix += place(thunder, 77.0, 0.45)
rain = rng.standard_normal(N).astype(np.float32); rain = rain - lp(rain, 900); rain = lp(rain, 9000); mix += rain * env(76, 89, 3, 2.5, 0.07)
# wheel guns: four short pulsed bursts at the pit scene (impact-wrench: buzzing carrier, pulsed)
for k, t0 in enumerate([92.9, 93.35, 94.2, 94.6, 95.2]):
    n = int(0.28 * R); tt = np.arange(n) / R; saw = np.sign(np.sin(2 * np.pi * 410 * tt)) * 0.5 + np.sin(2 * np.pi * 820 * tt) * 0.2
    mix += place(saw * (np.sin(2 * np.pi * 26 * tt) > -0.3) * np.hanning(n).astype(np.float32), t0, 0.22)
mix += place(rng.standard_normal(int(0.04 * R)).astype(np.float32) * 0.6, 91.0, 0.5)  # jack thud
# --- original music: Am - F - C - G pads, sub bass, drums by section ----------------------------------------------------------------------------
t = np.arange(N) / R; music = np.zeros(N, np.float32)
chords = [(220.0, 261.63, 329.63), (174.61, 220.0, 261.63), (261.63, 329.63, 392.0), (196.0, 246.94, 293.66)]
bar = 4 * 60 / 110.0
pad = np.zeros(N, np.float32)
for b in range(int(T / bar) + 1):
    f = chords[b % 4]; i0 = int(b * bar * R); i1 = min(int((b + 1) * bar * R), N)
    if i0 >= N: break
    tt = t[i0:i1]; w = sum(np.sin(2 * np.pi * x * tt) + 0.5 * np.sin(2 * np.pi * 2.005 * x * tt) for x in f) / 6
    pad[i0:i1] += (w * np.minimum(1, np.minimum((tt - tt[0]) * 3, (tt[-1] - tt) * 3))).astype(np.float32)
sub = np.zeros(N, np.float32)
for b in range(int(T / bar) + 1):
    f = chords[b % 4][0] / 4; i0 = int(b * bar * R); i1 = min(int((b + 1) * bar * R), N)
    if i0 >= N: break
    sub[i0:i1] = np.sin(2 * np.pi * f * t[i0:i1]) * np.minimum(1, (t[i1 - 1] - t[i0:i1]) * 6)
beat = 60 / 110.0
kick = np.zeros(N, np.float32); hat = np.zeros(N, np.float32); riser = np.zeros(N, np.float32)
kn = int(0.22 * R); ktt = np.arange(kn) / R; kick_s = np.sin(2 * np.pi * (45 + 90 * np.exp(-ktt * 28)) * ktt) * np.exp(-ktt * 14)
hn = int(0.05 * R); hat_s = (rng.standard_normal(hn) * np.exp(-np.arange(hn) / R * 120)).astype(np.float32)
for k in range(int(T / beat)):
    tb = k * beat
    if 20 <= tb < 112 or 112 <= tb < 113: kick += place(kick_s.astype(np.float32), tb, 0.9)
    if tb >= 42 and tb < 112: kick += place(kick_s.astype(np.float32), tb + beat / 2, 0.45) if (k % 2 == 1) else 0
    if tb >= 30 and tb < 112: hat += place(hat_s, tb + beat / 2, 0.5)
    if tb >= 42 and tb < 112: hat += place(hat_s, tb + beat / 4, 0.25) + place(hat_s, tb + 3 * beat / 4, 0.25)
    if 88 <= tb < 100 and k % 2 == 0: kick += place(kick_s.astype(np.float32), tb, 0.0)  # pit: keep drums sparse, precise
sweep = rng.standard_normal(int(20 * R)).astype(np.float32); riser[:len(sweep)] = (sweep - lp(sweep, 400)) * np.linspace(0, 1, len(sweep)) ** 2
music = pad * (env(0, 20, 4, 0, 0.28) + env(20, 76, 0, 0, 0.30) + env(76, 88, 2, 2, 0.22) + env(88, 100, 0, 0, 0.14) + env(100, 114, 0, 0, 0.36) + env(112, 120, 0, 4, 0.30))
music += sub * (env(20, 42, 2, 0, 0.35) + env(42, 76, 0, 0, 0.42) + env(100, 114, 0, 0.5, 0.5))
music += kick * 0.55 + hat * 0.12 + riser * 0.04 * env(0, 30, 0, 0, 1)
# title stinger at 112: hit + shimmer
hit = (np.sin(2 * np.pi * 55 * np.arange(int(3 * R)) / R) * np.exp(-np.arange(int(3 * R)) / R * 1.2)).astype(np.float32)
music += place(hit, 112.0, 0.7)
# --- commentary (macOS `say`, placeholder voice) ----------------------------------------------------------------------------------------------
LINES = [(1.0, "New York City. The toughest street circuit on the calendar."),
 (6.0, "Skyscrapers on every side. No room for error."),
 (11.0, "Carbon fibre, sharp front wings, over a thousand horsepower."),
 (16.0, "Ten teams. Twenty drivers. One street circuit."),
 (21.0, "The grid is set, and the grandstands are packed."),
 (25.5, "Five red lights..."),
 (29.3, "Lights out! And away we go!"),
 (33.5, "Wheel to wheel into turn one!"),
 (38.0, "Over three hundred kilometres an hour between the towers."),
 (42.5, "Flat out through Midtown."),
 (46.0, "He's in the slipstream, going for the move!"),
 (50.0, "Late on the brakes, side by side!"),
 (54.5, "Three cars, nose to tail."),
 (59.0, "Perfect exit. He gets the power down early."),
 (63.5, "Right on the limit, every corner."),
 (68.5, "The sun drops over Manhattan."),
 (73.0, "Golden hour in New York. Spectacular."),
 (77.0, "And now the rain arrives in Times Square."),
 (81.5, "Spray everywhere. They can barely see."),
 (85.5, "One mistake, and it's all over."),
 (89.0, "Box, box, box! Pit stop!"),
 (92.5, "Jacks up. Wheel guns on. Four new tyres."),
 (95.8, "Two point one seconds! A stunning stop!"),
 (98.8, "Released, and back in the fight."),
 (101.0, "Final lap. Three cars. One victory."),
 (105.0, "He defends. The door is closed."),
 (108.5, "Last corner, around the outside. Incredible!"),
 (111.6, "He takes the chequered flag! APEX FORMULA wins the New York Grand Prix!"),
 (116.5, "A victory for the history books.")]
PREV_END = 0.0; voice = np.zeros(N, np.float32); tmp = tempfile.mkdtemp()
for i, (ts, line) in enumerate(LINES):
    aiff = os.path.join(tmp, f"l{i}.aiff"); wav = os.path.join(tmp, f"l{i}.wav")
    subprocess.run(["say", "-v", "Daniel", "-r", "205", "-o", aiff, line], check=True)
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", aiff, "-ar", str(R), "-ac", "1", "-sample_fmt", "s16", wav], check=True)
    w = wave.open(wav); a = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32) / 32768
    ts = max(ts, PREV_END + 0.35); PREV_END = ts + len(a) / R
    print("LINE", i, round(ts,1), round(PREV_END,1)); voice += place(a, ts, 0.9)
    # duck everything else under each line
    duck = env(ts - 0.2, ts + len(a) / R + 0.3, 0.2, 0.4, 0.45); mix *= (1 - duck); music *= (1 - duck * 0.8)
# Mix bus: effects and engines are rolled off above 4.5 kHz (the synthesised stems are harsh), music softened, commentary kept clear. Then a soft limiter and
# a loudness target of about -22 dBFS RMS with peaks under -6 dBFS, so it is comfortable to listen to at normal volume.
fx = lp(mix, 4500) * 0.55
mus = lp(music, 7000) * 0.45
out = fx + mus + voice * 0.9
out = np.tanh(out * 1.2) / 1.2
rms = float(np.sqrt(np.mean(out ** 2))); out = out * (10 ** (-22 / 20) / max(rms, 1e-6))
peak = float(np.max(np.abs(out)))
if peak > 0.5: out = out * (0.5 / peak)
st = np.stack([out, out * 0.98], 1)   # near-mono: stems are mono
pcm = (np.clip(st, -1, 1) * 32767).astype(np.int16)
w = wave.open(os.path.join(EX, "showcase_mix.wav"), "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(R); w.writeframes(pcm.tobytes()); w.close()
print("MIX_OK", len(pcm) / R, "s peak", float(np.max(np.abs(out))), "rms", float(np.sqrt(np.mean(out ** 2))))
