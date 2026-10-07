"""Generates the factory preset chains for WallVox and Wall Chop.
Edit here, then run:  python3 make_presets.py   (writes vox/ and chop/ factory_presets.json)"""
import json, os

def m(type_, on=True, **params):
    return {"type": type_, "on": on, "params": {k.replace("_", " "): v for k, v in params.items()}}

def P(name, category, chain, in_=0, out=0):
    return {"name": name, "category": category, "in": in_, "out": out, "chain": chain}

# ---------- reusable building blocks
def tune(speed=30, amount=1.0, natural=0.25, **k): return m("tune", Speed=speed, Amount=amount, Natural=natural, **k)
def eq(**k): return m("eq", **k)
def deess(thr=-27, rng=8, freq=6500): return m("deess", Threshold=thr, Range=rng, Frequency=freq)
def fet(inp=12, out=0, att=3, rel=5, ratio="4:1", mix=1.0): return m("fet", Input=inp, Output=out, Attack=att, Release=rel, Ratio=ratio, Mix=mix)
def opto(pr=50, gain=4, mode="Compress", emph=0.3, mix=1.0): return m("opto", Peak_Reduction=pr, Gain=gain, Mode=mode, Emphasis=emph, Mix=mix)
def varimu(inp=6, thr=-18, time="2", out=0, mix=1.0): return m("varimu", Input=inp, Threshold=thr, Time=time, Output=out, Mix=mix)
def comp(thr=-18, ratio=4, att=5, rel=120, knee=6, makeup=0, mix=1.0): return m("comp", Threshold=thr, Ratio=ratio, Attack=att, Release=rel, Knee=knee, Makeup=makeup, Mix=mix)
def pult(**k): return m("pultec", **k)
def tubeeq(**k): return m("tubeeq", **k)
def sat(type_="Tape", drive=0.2, tone=0.0, mix=1.0, out=0): return m("sat", Type=type_, Drive=drive, Tone=tone, Mix=mix, Output=out)
def double(amount=0.3, width=1.0, detune=9, delay=16): return m("double", Amount=amount, Width=width, Detune=detune, Delay=delay)
def delay(time="1/8", fb=0.3, mix=0.15, mode="Ping-Pong", lowcut=250, highcut=6000, duck=0.55, wow=0.15):
    return m("delay", Time=time, Feedback=fb, Mix=mix, Mode=mode, Low_Cut=lowcut, High_Cut=highcut, Ducking=duck, Wow=wow)
def reverb(type_="Plate", decay=1.8, pre=25, size=0.6, highcut=7000, lowcut=200, width=1.0, mix=0.2):
    return m("reverb", Type=type_, Decay=decay, **{"Pre-Delay": pre}, Size=size, High_Cut=highcut, Low_Cut=lowcut, Width=width, Mix=mix)

vox = [
    P("Empty Chain", "Start", []),
    P("Pop Lead", "Pop", [
        tune(25, 0.9, 0.2), eq(**{"Low Cut": 90, "Low Mid Freq": 350, "Low Mid": -2.5, "High Mid Freq": 3200, "High Mid": 2, "High": 1.5, "Air": 3}),
        fet(12, 0, 3, 5, "4:1"), opto(45, 3), deess(-28, 8), sat("Tape", 0.15), double(0.25),
        delay("1/4", 0.25, 0.14, duck=0.6), reverb("Plate", 1.6, 30, 0.6, 7500, mix=0.18)]),
    P("Pop Airy Ballad", "Pop", [
        tune(40, 0.8, 0.4), eq(**{"Low Cut": 85, "Low Mid Freq": 300, "Low Mid": -2}), opto(50, 4),
        pult(**{"High Freq": "12 kHz", "High Boost": 5, "Bandwidth": 6}), deess(-26, 8),
        delay("1/4", 0.3, 0.15), reverb("Hall", 2.8, 40, 0.85, 6500, mix=0.26)]),
    P("Silky R&B", "Pop", [
        tune(20, 0.85, 0.35), eq(**{"Low Cut": 90, "Low Mid Freq": 400, "Low Mid": -2.5, "Air": 4}), fet(10, 0, 4, 5, "4:1"),
        opto(45, 3), pult(**{"High Freq": "12 kHz", "High Boost": 4, "Bandwidth": 7}), deess(-28, 8), double(0.2),
        delay("1/4", 0.3, 0.14), reverb("Plate", 2.2, 30, 0.7, 8000, mix=0.2)]),
    P("EDM Topline", "EDM", [
        tune(5, 1.0, 0.0), eq(**{"Low Cut": 140, "Low Mid Freq": 350, "Low Mid": -3, "High Mid Freq": 3500, "High Mid": 3, "Air": 4}),
        fet(14, 0, 5, 6, "8:1"), comp(-22, 4, 8, 80, 6, 4), deess(-30, 9), sat("Tube", 0.25, 0.1, 0.6), double(0.35),
        delay("1/8 Dotted", 0.4, 0.2, wow=0.1), reverb("Hall", 3.2, 20, 0.9, 8000, 300, mix=0.28)]),
    P("EDM Festival Vox", "EDM", [
        tune(0, 1.0, 0.0), eq(**{"Low Cut": 160, "High Mid Freq": 3500, "High Mid": 4, "Air": 5}), fet(14, 0, 5, 7, "All", 0.5),
        opto(50, 4), deess(-30, 10), delay("1/16", 0.35, 0.2), reverb("Hall", 3.8, 15, 1.0, 9000, 350, mix=0.3)]),
    P("Rap Upfront", "Rap", [
        tune(80, 0.25, 0.5), eq(**{"Low Cut": 100, "Low": 1.5, "Low Mid Freq": 300, "Low Mid": -3, "High Mid Freq": 3500, "High Mid": 3, "High": 2}),
        fet(14, 0, 4, 6, "8:1"), opto(40, 3), deess(-28, 8), sat("Tape", 0.3), double(0.08),
        delay("1/8", 0.15, 0.06, duck=0.8), reverb("Room", 0.7, 10, 0.4, 8000, mix=0.06)]),
    P("Rap Melodic Trap", "Rap", [
        tune(0, 1.0, 0.0), eq(**{"Low Cut": 110, "Low Mid Freq": 350, "Low Mid": -3, "High Mid Freq": 3500, "High Mid": 3.5, "Air": 4}),
        fet(14, 0, 5, 6, "4:1"), comp(-20, 3, 10, 120, 6, 2), deess(-28, 8), sat("Tube", 0.2, 0, 0.7), double(0.2),
        delay("1/8", 0.35, 0.2, duck=0.6, wow=0.2), reverb("Plate", 1.4, 20, 0.5, mix=0.15)]),
    P("Rap Crunchy", "Rap", [
        tune(40, 0.3, 0.4), eq(**{"Low Cut": 130, "High Mid Freq": 3000, "High Mid": 4}), fet(16, 0, 5, 6, "All", 0.6),
        sat("Crunch", 0.55, 0.2, 0.6), deess(-28, 8), delay("1/8", 0.2, 0.08, duck=0.8), reverb("Room", 0.8, 10, 0.4, mix=0.08)]),
    P("Worship Lead", "Worship", [
        tune(45, 0.7, 0.5), eq(**{"Low Cut": 90, "Low": 1, "Low Mid Freq": 350, "Low Mid": -2, "High Mid Freq": 2800, "High Mid": 1.5, "Air": 3}),
        opto(50, 4), varimu(4, -16, "5 Auto"), pult(**{"High Freq": "10 kHz", "High Boost": 3, "Bandwidth": 6}), deess(-26, 7),
        double(0.12), delay("1/4", 0.3, 0.15, duck=0.6), reverb("Hall", 3.0, 35, 0.9, 6500, mix=0.3)]),
    P("Worship Intimate", "Worship", [
        tune(60, 0.5, 0.6), eq(**{"Low Cut": 80, "Low": 2, "Low Mid Freq": 300, "Low Mid": -1.5, "High Mid Freq": 2800, "High Mid": 1}),
        opto(45, 3), deess(-26, 7), sat("Tape", 0.1), delay("1/8 Dotted", 0.2, 0.08), reverb("Plate", 1.8, 25, 0.6, 7000, mix=0.2)]),
    P("Vintage Soul", "Character", [
        tubeeq(Low=2, **{"Mid Freq": 1500}, Mid=1, High=2, Air=1, Drive=0.4), varimu(8, -20, "2"), opto(40, 2),
        delay("1/16", 0.1, 0.1, "Mono", wow=0.4), reverb("Chamber", 1.4, 15, 0.5, 6000, mix=0.18)]),
    P("Clean & Natural", "Character", [
        eq(**{"Low Cut": 80}), comp(-18, 2.5, 15, 150, 6, 2), deess(-27, 6), reverb("Room", 0.9, 10, 0.5, mix=0.08)]),
    P("Hard Tune Robot", "Character", [
        tune(0, 1.0, 0.0), eq(**{"Low Cut": 120, "High Mid": 2, "Air": 3}), fet(12, 0, 4, 6, "4:1"), deess(-28, 8),
        delay("1/8", 0.3, 0.15), reverb("Plate", 1.2, 15, 0.5, mix=0.14)]),
]

def chop_base(): return [tune(20, 0.6, 0.3), eq(**{"Low Cut": 110, "Low Mid Freq": 350, "Low Mid": -3, "High Mid Freq": 3500, "High Mid": 3, "Air": 5}), fet(12, 0, 4, 6, "4:1"), deess(-28, 8)]
def space(dmix=0.12, rmix=0.25, dtime="1/8", rtype="Hall", decay=3.0): return [delay(dtime, 0.3, dmix), reverb(rtype, decay, 20, 0.8, 8000, 300, mix=rmix)]
def stutter(repeat="Every Bar", length="1/4", slice_="1/16", drop=0, decay=0, mix=1.0): return m("stutter", Repeat=repeat, Length=length, Slice=slice_, Pitch_Drop=drop, Decay=decay, Mix=mix)
def gate(pattern="Trance 1", rate="1/16", length=0.6, depth=1.0): return m("gate", Pattern=pattern, Rate=rate, Length=length, Depth=depth)
def bender(mode="Shift", pitch=0, formant=0, mix=1.0): return m("bender", Mode=mode, Pitch=pitch, Formant=formant, Mix=mix)
def filt(type_="Low Pass", cutoff=2000, reso=0.3, rate="1 Bar", sweep=0.0): return m("filter", Type=type_, Cutoff=cutoff, Resonance=reso, Sweep_Rate=rate, Sweep=sweep)

chop = [p for p in vox] + [
    P("Stutter Fill 1/16", "Chop - Stutter", chop_base() + [stutter("Every Bar", "1/4", "1/16")] + space()),
    P("Stutter 1/32 Roll", "Chop - Stutter", chop_base() + [stutter("Every Bar", "1/2", "1/32", decay=0.15)] + space()),
    P("Stutter Pitch Drop", "Chop - Stutter", chop_base() + [stutter("Every 2 Bars", "1 Bar", "1/8", 1.5, 0.2)] + space()),
    P("Triplet Repeat", "Chop - Stutter", chop_base() + [stutter("Every Bar", "1/2", "1/16 T")] + space()),
    P("Trance Gate", "Chop - Gate", chop_base() + [gate("Trance 1", "1/16", 0.6)] + space(0.1, 0.35)),
    P("3-3-2 Gate", "Chop - Gate", chop_base() + [gate("Trance 2", "1/16", 0.7)] + space()),
    P("Offbeat Pump", "Chop - Gate", chop_base() + [gate("Offbeat", "1/16", 0.9, 0.9)] + space(0.2, 0.25, "1/8 Dotted")),
    P("1/16 Machine Chop", "Chop - Gate", chop_base() + [gate("1/16 Chop", "1/16", 0.45)] + space()),
    P("Build Up Gate", "Chop - Gate", chop_base() + [gate("Build Up", "1/32", 0.5), filt("High Pass", 300, 0.4, "1 Bar", 0.5)] + space()),
    P("Chipmunk Chop", "Chop - Bender", chop_base() + [bender("Shift", 7, 6), stutter("Every Bar", "1/8", "1/32")] + space()),
    P("Pitched Down Hook", "Chop - Bender", chop_base() + [bender("Shift", -5, -4)] + space()),
    P("Deep Demon", "Chop - Bender", chop_base() + [bender("Shift", -12, -6), sat("Crunch", 0.4, 0, 0.4)] + space(0.08, 0.2)),
    P("Baby Voice", "Chop - Bender", chop_base() + [bender("Shift", 5, 9)] + space()),
    P("Robot Lead", "Chop - Bender", chop_base() + [bender("Robot", 0, 0)] + space(0.2, 0.2)),
    P("Octave Up Lead", "Chop - Bender", chop_base() + [bender("Shift", 12, 0, 0.6), double(0.4)] + space()),
    P("Filter Sweep Build", "Chop - Filter", chop_base() + [filt("High Pass", 400, 0.5, "1 Bar", 0.6)] + space()),
    P("Wobble Vox", "Chop - Filter", chop_base() + [filt("Low Pass", 1500, 0.7, "1/8", 0.6)] + space()),
    P("Telephone Chop", "Chop - Filter", chop_base() + [filt("Band Pass", 1500, 0.6), gate("1/16 Chop", "1/16", 0.5)] + space(0.12, 0.1, rtype="Room", decay=0.8)),
    P("Tape Stop Drop (switch ON)", "Chop - FX", chop_base() + [m("tapestop", on=False, **{"Stop Time": 1200})] + space()),
    P("Festival Chop", "Chop - FX", chop_base() + [bender("Shift", 12, 3, 0.5), gate("Trance 2", "1/16", 0.7)]
      + [delay("1/8 Dotted", 0.45, 0.25), reverb("Hall", 3.5, 20, 0.95, 9000, 300, mix=0.35)]),
]

OUT_GAIN = {'Trance Gate': 4, '3-3-2 Gate': 5, 'Offbeat Pump': 5, '1/16 Machine Chop': 4, 'Pitched Down Hook': 3, 'Deep Demon': 5, 'Robot Lead': 3, 'Octave Up Lead': 6, 'Telephone Chop': 14, 'Festival Chop': 8, 'Stutter 1/32 Roll': 2, 'Vintage Soul': 2}
for p in chop:
    if p["name"] in OUT_GAIN: p["out"] = OUT_GAIN[p["name"]]

here = os.path.dirname(os.path.abspath(__file__))
for folder, presets in (("vox", vox), ("chop", chop)):
    for p in presets:
        assert len(p["chain"]) <= 10, p["name"]
    with open(os.path.join(here, folder, "factory_presets.json"), "w") as f:
        json.dump(presets, f, indent=1)
    print(folder, len(presets), "presets")
