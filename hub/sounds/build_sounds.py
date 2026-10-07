"""Builds the Wall Chords real-instrument pack from two public-domain (CC0) libraries:
Versilian Community Sample Library (VCSL) and VSCO 2 Community Edition, both by Versilian Studios.

usage: python build_sounds.py <vcsl dir> <vsco dir> <out dir>
Each instrument becomes <out>/<name>/instrument.json + .ogg files.
Pitch is checked from the audio itself, so file-name octave conventions don't matter."""
import json, os, re, subprocess, sys, tempfile
import numpy as np
import soundfile as sf

VCSL, VSCO, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
NOTE_RE = re.compile(r'(?<![A-Za-z])([A-G])(#|b)?(-?\d)(?=[_\.\s])')
PC = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}

INSTRUMENTS = [
    dict(name="grand_piano", title="Grand Piano", dirs=[(VCSL, "Chordophones/Zithers/Grand Piano, Kawai/Sustains")],
         layers=[(["v2", "v1", "v3"], 0, 80), (["v4", "v3"], 81, 127)], max_sec=6.0, release=0.5),
    dict(name="soft_piano", title="Soft Piano", dirs=[(VSCO, "Keys/Upright Nr1")],
         layers=[(["pp", "mf"], 0, 127)], max_sec=6.0, release=0.45),
    dict(name="electric_piano", title="Electric Piano", dirs=[(VCSL, "Electrophones/TX81Z/FM Piano")],
         layers=[(["vl2", "vl1", "vl3"], 0, 90), (["vl3", "vl2"], 91, 127)], max_sec=5.0, release=0.4),
    dict(name="strings", title="Strings", dirs=[(VSCO, "Strings/Cello Section/susvib"), (VSCO, "Strings/Viola Section/susvib"),
                                                 (VSCO, "Strings/Violin Section/susVib")],
         layers=[(["v2", "v1"], 0, 127)], max_sec=8.0, loop=True, attack=0.12, release=0.6,
         section_ranges=[(0, 54), (55, 64), (65, 127)]),
    dict(name="pizzicato", title="Pizzicato", dirs=[(VSCO, "Strings/Cello Section/pizzT"), (VSCO, "Strings/Violin Section/Pizz")],
         layers=[(["v2", "v1"], 0, 127)], max_sec=2.5, release=0.2, section_ranges=[(0, 59), (60, 127)]),
    dict(name="harp", title="Harp", dirs=[(VCSL, "Chordophones/Composite Chordophones/Concert Harp")],
         layers=[(["mf", "f"], 0, 95), (["f", "mf"], 96, 127)], max_sec=5.0, release=0.4),
    dict(name="vibraphone", title="Vibraphone", dirs=[(VCSL, "Idiophones/Struck Idiophones/Vibraphone/Soft Mallets")],
         layers=[(["v2", "v1", "v3"], 0, 127)], max_sec=5.0, release=0.4),
    dict(name="organ", title="Organ", dirs=[(VSCO, "Keys/Organ/Loud")],
         layers=[([], 0, 127)], max_sec=6.0, loop=True, attack=0.01, release=0.25),
    dict(name="upright_bass", title="Upright Bass", dirs=[(VSCO, "Strings/Solo Contrabass/Pizz")],
         layers=[(["v1", "v2"], 0, 85), (["v3", "v2"], 86, 127)], max_sec=3.0, release=0.2),
]


def yin_midi(x, sr):
    """Detect the fundamental of a sample (mono float array) as a MIDI number, or None."""
    start = int(0.08 * sr)
    seg = x[start:start + 8192]
    if len(seg) < 4096 or np.max(np.abs(seg)) < 1e-3:
        return None
    seg = seg / np.max(np.abs(seg))
    W, tmin, tmax = 2048, int(sr / 2000), int(sr / 25)
    d = np.zeros(tmax + 1)
    for tau in range(1, tmax + 1):
        diff = seg[:W] - seg[tau:tau + W]
        d[tau] = np.dot(diff, diff)
    cum = np.cumsum(d[1:])
    dn = np.ones_like(d)
    dn[1:] = d[1:] * np.arange(1, tmax + 1) / np.maximum(cum, 1e-12)
    for tau in range(tmin, tmax):
        if dn[tau] < 0.15:
            while tau + 1 < tmax and dn[tau + 1] < dn[tau]:
                tau += 1
            f = sr / tau
            return 69 + 12 * np.log2(f / 440.0)
    return None


def name_note(fname):
    m = None
    for m in NOTE_RE.finditer(fname):
        pass  # last match wins (note names come after prefixes)
    if not m:
        return None
    pc = (PC[m.group(1)] + (1 if m.group(2) == '#' else -1 if m.group(2) == 'b' else 0)) % 12
    return pc + 12 * (int(m.group(3)) + 1)   # scientific: C4 = 60


def layer_token(fname, tokens):
    low = fname.lower()
    for t in tokens:
        if re.search(r'_' + re.escape(t.lower()) + r'\d?(?=[_\.])', low):
            return t
    return None


def round_robin_ok(fname):
    m = re.search(r'_rr(\d)', fname.lower())
    return m is None or m.group(1) == '1'


def build(spec):
    files = []
    for si, (base, rel) in enumerate(spec["dirs"]):
        d = os.path.join(base, rel)
        if not os.path.isdir(d):
            print("  missing", d)
            continue
        for f in sorted(os.listdir(d)):
            if f.lower().endswith(".wav") and round_robin_ok(f):
                files.append((si, os.path.join(d, f)))

    # read, detect pitch, resolve octave convention
    infos = []
    for si, path in files:
        try:
            x, sr = sf.read(path, always_2d=True, dtype='float32')
        except Exception as e:
            print("  skip", path, e)
            continue
        mono = x.mean(axis=1)
        det = yin_midi(mono, sr)
        nm = name_note(os.path.basename(path))
        infos.append(dict(section=si, path=path, data=x, sr=sr, det=det, name=nm))
    offsets = [round((i["det"] - i["name"]) / 12) * 12 for i in infos if i["det"] is not None and i["name"] is not None
               and abs(((i["det"] - i["name"]) + 6) % 12 - 6) < 0.6]
    off = int(np.median(offsets)) if offsets else 0
    for i in infos:
        if i["name"] is not None:
            i["root"] = i["name"] + off
        elif i["det"] is not None:
            i["root"] = int(round(i["det"]))
        else:
            i["root"] = None
    infos = [i for i in infos if i["root"] is not None and 12 <= i["root"] <= 108]
    print(f"  {len(infos)} usable samples, octave offset {off:+d}")

    # section preference (e.g. cellos low, violins high)
    ranges = spec.get("section_ranges")
    if ranges:
        infos = [i for i in infos if ranges[min(i["section"], len(ranges) - 1)][0] <= i["root"] <= ranges[min(i["section"], len(ranges) - 1)][1]]

    # pick one file per (root, layer)
    roots = sorted(set(i["root"] for i in infos))
    if not roots:
        print("  no usable samples - skipped (the plugin falls back to its synth for this one)")
        return
    chosen = []
    for li, (tokens, vlo, vhi) in enumerate(spec["layers"]):
        for r in roots:
            cands = [i for i in infos if i["root"] == r]
            pick = None
            for t in tokens:
                pick = next((i for i in cands if layer_token(os.path.basename(i["path"]), [t])), None)
                if pick:
                    break
            if pick is None:
                pick = cands[0]
            chosen.append((li, r, pick, vlo, vhi))

    peak = max(float(np.max(np.abs(c[2]["data"]))) for c in chosen) or 1.0
    os.makedirs(os.path.join(OUT, spec["name"]), exist_ok=True)
    zones = []
    for li, r, info, vlo, vhi in chosen:
        x, sr = info["data"], info["sr"]
        # trim leading silence, cap length, fade out
        env = np.abs(x).max(axis=1)
        nz = np.where(env > peak * 0.003)[0]
        s0 = max(0, (nz[0] if len(nz) else 0) - int(0.004 * sr))
        y = x[s0:s0 + int(spec["max_sec"] * sr)].copy()
        fade = min(len(y), int(0.25 * sr))
        y[-fade:] *= np.linspace(1, 0, fade)[:, None]
        y *= 0.9 / peak
        fname = f"{r}_{li}.ogg"
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
            sf.write(tmp.name, y, sr)
            subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", tmp.name, "-ar", "44100", "-c:a", "libvorbis", "-q:a", "4",
                            os.path.join(OUT, spec["name"], fname)], check=True)
            os.unlink(tmp.name)
        zones.append(dict(file=fname, root=r, vlo=vlo, vhi=vhi))

    # key ranges: split halfway between neighbouring roots
    for li in range(len(spec["layers"])):
        lz = sorted([z for z in zones if z["file"].endswith(f"_{li}.ogg")], key=lambda z: z["root"])
        for k, z in enumerate(lz):
            z["lo"] = 0 if k == 0 else (lz[k - 1]["root"] + z["root"]) // 2 + 1
            z["hi"] = 127 if k == len(lz) - 1 else (z["root"] + lz[k + 1]["root"]) // 2
    meta = dict(name=spec["title"], loop=spec.get("loop", False), attack=spec.get("attack", 0.002),
                release=spec.get("release", 0.35), gain=1.0, zones=zones)
    with open(os.path.join(OUT, spec["name"], "instrument.json"), "w") as f:
        json.dump(meta, f, indent=1)
    print(f"  wrote {len(zones)} zones, roots {roots[0]}..{roots[-1]}")


os.makedirs(OUT, exist_ok=True)
only = [x for x in os.environ.get("ONLY", "").split(",") if x]
for spec in INSTRUMENTS:
    if only and spec["name"] not in only:
        continue
    print(spec["title"])
    build(spec)
with open(os.path.join(OUT, "CREDITS.txt"), "w") as f:
    f.write("Instrument samples from the Versilian Community Sample Library (VCSL) and VSCO 2 Community Edition,\n"
            "by Versilian Studios LLC and contributors, released under CC0 (public domain). Thank you!\n")
