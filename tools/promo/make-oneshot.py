"""Writes out/bass-oneshot.wav, the one-shot the promo video drops on Sound → Seed."""
import os, wave
import numpy as np

sr = 44100
t = np.arange(int(sr * 1.6)) / sr
x = sum(np.sin(2 * np.pi * 55 * k * t) / k * np.exp(-t * (2 + k * 1.5)) for k in range(1, 9))
x *= np.minimum(1, t * 400)
x = x / np.abs(x).max() * 0.8
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
os.makedirs(out, exist_ok=True)
with wave.open(os.path.join(out, 'bass-oneshot.wav'), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes((x * 32767).astype('<i2').tobytes())
