"""Trains the sound -> seed net (features -> genes) on dataset.bin from DatasetGen and writes ../Resources/net.bin.

numpy only: MLP NFEAT -> 512 -> 256 -> genes, ReLU, sigmoid out, MSE, Adam.
usage: py -I train.py [dataset.bin] [epochs]
"""
import os
import struct
import sys
import time

import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "dataset.bin")
epochs = int(sys.argv[2]) if len(sys.argv) > 2 else 40

with open(path, "rb") as f:
    n, nfeat, ngenes = struct.unpack("<3i", f.read(12))
    X = np.frombuffer(f.read(n * nfeat * 4), np.float32).reshape(n, nfeat)
    Y = np.frombuffer(f.read(n * ngenes * 4), np.float32).reshape(n, ngenes)

rng = np.random.default_rng(1)
order = rng.permutation(n)
val = order[: n // 20]
tr = order[n // 20 :]
sizes = [nfeat, 512, 256, ngenes]
W = [(rng.standard_normal((o, i)) * np.sqrt(2 / i)).astype(np.float32) for i, o in zip(sizes, sizes[1:])]
B = [np.zeros(o, np.float32) for o in sizes[1:]]
params = W + B
m = [np.zeros_like(p) for p in params]
v = [np.zeros_like(p) for p in params]


def forward(x):
    acts = [x]
    for l in range(len(W)):
        z = acts[-1] @ W[l].T + B[l]
        acts.append(np.maximum(z, 0) if l < len(W) - 1 else 1 / (1 + np.exp(-z)))
    return acts


def mse(idx):
    return float(np.mean((forward(X[idx])[-1] - Y[idx]) ** 2))


baseline = float(np.mean((Y[val] - Y[tr].mean(0)) ** 2))
print(f"{n} samples, {nfeat} features, {ngenes} genes; predict-the-mean baseline MSE {baseline:.4f}")

step, lr, batch, t0 = 0, 1e-3, 256, time.time()
for epoch in range(epochs):
    rng.shuffle(tr)
    for s in range(0, len(tr), batch):
        idx = tr[s : s + batch]
        acts = forward(X[idx])
        out = acts[-1]
        delta = 2 * (out - Y[idx]) / len(idx) * out * (1 - out)
        grads_w, grads_b = [], []
        for l in reversed(range(len(W))):
            grads_w.insert(0, delta.T @ acts[l])
            grads_b.insert(0, delta.sum(0))
            if l:
                delta = (delta @ W[l]) * (acts[l] > 0)
        step += 1
        for p, g, mm, vv in zip(params, grads_w + grads_b, m, v):
            mm *= 0.9
            mm += 0.1 * g
            vv *= 0.999
            vv += 0.001 * g * g
            p -= lr * (mm / (1 - 0.9**step)) / (np.sqrt(vv / (1 - 0.999**step)) + 1e-8)
    if epoch in (epochs // 2, epochs * 3 // 4):
        lr *= 0.3
    print(f"epoch {epoch + 1:2d}  val MSE {mse(val):.4f}  ({time.time() - t0:.0f}s)", flush=True)

out = os.path.join(here, "..", "Resources", "net.bin")
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "wb") as f:
    f.write(b"SDNN" + struct.pack("<i", len(W)))
    for w, b in zip(W, B):
        f.write(struct.pack("<2i", w.shape[1], w.shape[0]))
        f.write(np.ascontiguousarray(w, np.float32).tobytes())
        f.write(b.astype(np.float32).tobytes())
print(f"wrote {out}: val MSE {mse(val):.4f} vs baseline {baseline:.4f}")
