#!/usr/bin/env python3
"""Create FreshLab's original formant-synthesized adlibs. Standard library only.
No recordings, speaker models, sample libraries, or external services are used.
Outputs are covered by Voices/VOICE-LICENSE.txt (CC0-1.0).
"""
import math
import random
import struct
import wave
from pathlib import Path
from make_previews import write_voice_preview

RATE = 44100
ROOT = Path(__file__).resolve().parents[1]
VOWELS = {
    'ae': (720, 1720, 2450), 'eh': (530, 1840, 2480),
    'ee': (290, 2280, 3010), 'ah': (760, 1100, 2450),
    'oh': (480, 880, 2450), 'oo': (330, 680, 2250),
}
# Phonetic glides, rather than a speech model. These deliberately sound electronic.
PHRASES = {
    'yeah': [('ee', .09), ('ae', .32), ('eh', .17)],
    'hey': [('noise', .075), ('eh', .25), ('ee', .20)],
    'whoa': [('oo', .16), ('oh', .34), ('ah', .17)],
    'go': [('burst', .035), ('oh', .28), ('oo', .22)],
    'ah': [('ah', .48)],
    'brr': [('trill', .47), ('oo', .10)],
}
STYLES = {
    'sub': (89, .86, .06, .20),
    'spark': (226, 1.17, .045, .07),
    'haze': (165, 1.03, .27, .05),
    'circuit': (124, 1.00, .015, .54),
}

def make(style, phrase, seed):
    base, formant_scale, breath, metallic = STYLES[style]
    rng = random.Random(seed)
    segments = PHRASES[phrase]
    length = sum(d for _, d in segments)
    count = int((length + .09) * RATE)
    # Three resonators in parallel, excited by a band-limited harmonic glottal source.
    state = [[0.0, 0.0] for _ in range(3)]
    phase = 0.0
    result = []
    for i in range(count):
        t = i / RATE
        at = 0.0
        index = len(segments) - 1
        for j, (_, duration) in enumerate(segments):
            if t < at + duration:
                index = j
                break
            at += duration
        phoneme, duration = segments[index]
        frac = max(0.0, min(1.0, (t - at) / duration))
        key = phoneme if phoneme in VOWELS else ('oo' if phoneme == 'trill' else 'eh')
        nextkey = segments[min(index + 1, len(segments) - 1)][0]
        nextkey = nextkey if nextkey in VOWELS else key
        blend = max(0, (frac - .5) * 2)
        f0 = base * (1.16 - .24 * min(1, t / length))
        f0 *= 1 + .012 * math.sin(2 * math.pi * 5.2 * t)
        phase = (phase + f0 / RATE) % 1.0
        glottal = sum(math.sin(2 * math.pi * phase * k) / (k ** 1.35) for k in range(1, 22))
        noise = rng.uniform(-1, 1)
        source = glottal * .32 + noise * breath
        if phoneme == 'noise':
            source = noise * .38
        elif phoneme == 'burst':
            source = noise * .48 * math.exp(-frac * 5)
        elif phoneme == 'trill':
            source *= .22 + .78 * max(0, math.sin(2 * math.pi * 31 * t))
        out = 0.0
        for band in range(3):
            hz = (VOWELS[key][band] * (1-blend) + VOWELS[nextkey][band] * blend) * formant_scale
            radius = math.exp(-math.pi * (85 + band * 45) / RATE)
            c = 2 * radius * math.cos(2 * math.pi * hz / RATE)
            y = source * (1-radius) + c * state[band][0] - radius*radius*state[band][1]
            state[band][1] = state[band][0]
            state[band][0] = y
            out += y * (1, .65, .35)[band]
        out = out * (1-metallic) + out * math.cos(2*math.pi*base*1.47*t)*metallic
        env = min(1, t/.018) * min(1, max(0, length+.035-t)/.08)
        result.append(math.tanh(out * 3) * env)
    peak = max(abs(x) for x in result) or 1
    return [x / peak * .82 for x in result]

def write(path, samples):
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h'*len(samples), *(round(x*32767) for x in samples)))

def main():
    destination = ROOT / 'Voices'
    destination.mkdir(exist_ok=True)
    for si, style in enumerate(STYLES):
        for pi, phrase in enumerate(PHRASES):
            audio = make(style, phrase, 1000 + si*20 + pi)
            write(destination / f'{style}_{phrase}.wav', audio)
    write_voice_preview(ROOT / 'docs' / 'voice-preview.wav')
    print(f'Created {len(STYLES)*len(PHRASES)} original voices')

if __name__ == '__main__':
    main()
