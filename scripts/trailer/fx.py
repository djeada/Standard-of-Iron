"""Procedural overlay plates for the trailer: embers, dust and light leaks.

Each plate is rendered once (deterministic seed) as a black-background video at
the scope size and screened over footage by ``conform.py``. They are atmosphere
laid over real game frames, never a replacement for them: embers drift up over
fire and battle, dust hangs in the light over the field, and a light leak can
wash across a cut.
"""

from __future__ import annotations

import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

WIDTH = 1920


def _writer(path: Path, width: int, height: int, fps: int) -> subprocess.Popen:
    return subprocess.Popen(
        ["ffmpeg", "-y", "-v", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s",
         f"{width}x{height}", "-r", str(fps), "-i", "-", "-c:v", "prores_ks", "-profile:v",
         "3", "-pix_fmt", "yuv422p10le", str(path)],
        stdin=subprocess.PIPE)


def embers(path: Path, seconds: float, fps: int, height: int, seed: int = 3,
           count: int = 230) -> None:
    rng = np.random.default_rng(seed)
    n = int(seconds * fps)
    x = rng.uniform(0, WIDTH, count)
    y = rng.uniform(0, height * 1.3, count)
    vx = rng.normal(18, 22, count)
    vy = -rng.uniform(45, 150, count)
    size = rng.uniform(1.2, 4.6, count) ** 1.3
    depth = rng.uniform(0.3, 1.0, count)
    phase = rng.uniform(0, 6.28, count)
    life = rng.uniform(2.5, 7.0, count)
    age = rng.uniform(0, life)
    proc = _writer(path, WIDTH, height, fps)
    for f in range(n):
        dt = 1.0 / fps
        age += dt
        wob = np.sin(f * dt * 1.7 + phase) * 22
        x += (vx + wob) * dt * depth
        y += vy * dt * depth
        dead = (age > life) | (y < -20)
        k = int(dead.sum())
        if k:
            x[dead] = rng.uniform(0, WIDTH, k)
            y[dead] = rng.uniform(height * 0.7, height * 1.15, k)
            age[dead] = 0
            life[dead] = rng.uniform(2.5, 7.0, k)
        img = Image.new("RGB", (WIDTH, height))
        d = ImageDraw.Draw(img)
        for i in range(count):
            fade = min(1.0, age[i] / 0.6) * max(0.0, 1.0 - age[i] / life[i])
            flick = 0.65 + 0.35 * math.sin(f * 0.9 + phase[i] * 3)
            a = fade * flick * depth[i]
            if a < 0.03:
                continue
            col = (int(255 * a), int((150 + 60 * depth[i]) * a), int(60 * a))
            r = size[i] * (0.6 + depth[i])
            sx, sy = vx[i] * 0.02 * depth[i], vy[i] * 0.02 * depth[i]
            d.line([(x[i] - sx, y[i] - sy), (x[i], y[i])], fill=col, width=max(1, int(r)))
            d.ellipse([x[i] - r, y[i] - r, x[i] + r, y[i] + r], fill=col)
        glow = img.filter(ImageFilter.GaussianBlur(5))
        out = np.minimum(255, np.asarray(img, np.uint16) * 2 + np.asarray(glow, np.uint16) * 4)
        proc.stdin.write(out.astype(np.uint8).tobytes())
    proc.stdin.close()
    proc.wait()


def dust(path: Path, seconds: float, fps: int, height: int, seed: int = 9,
         count: int = 260) -> None:
    rng = np.random.default_rng(seed)
    n = int(seconds * fps)
    x = rng.uniform(0, WIDTH, count)
    y = rng.uniform(0, height, count)
    vx = rng.normal(9, 6, count)
    vy = rng.normal(-3, 4, count)
    size = rng.uniform(0.6, 2.6, count)
    depth = rng.uniform(0.2, 1.0, count)
    phase = rng.uniform(0, 6.28, count)
    proc = _writer(path, WIDTH, height, fps)
    yy, xx = np.mgrid[0:height, 0:WIDTH]
    haze = np.clip(1.0 - yy / height, 0, 1) ** 1.6
    shaft = (0.5 + 0.5 * np.cos((xx - WIDTH * 0.72) / WIDTH * 3.2)) * haze
    for f in range(n):
        t = f / fps
        x = (x + vx / fps * depth) % WIDTH
        y = (y + (vy + np.sin(t * 0.8 + phase) * 5) / fps * depth) % height
        img = Image.new("L", (WIDTH, height))
        d = ImageDraw.Draw(img)
        for i in range(count):
            a = int(255 * depth[i] * (0.55 + 0.45 * math.sin(t * 1.3 + phase[i])))
            r = size[i] * (0.5 + depth[i] * 1.2)
            d.ellipse([x[i] - r, y[i] - r, x[i] + r, y[i] + r], fill=a)
        motes = np.asarray(img.filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
        light = motes * (0.7 + 1.1 * shaft) + shaft * 0.16 * (0.8 + 0.2 * math.sin(t * 0.4))
        rgb = np.stack([light, light * 0.86, light * 0.66], axis=-1)
        proc.stdin.write((np.clip(rgb, 0, 1) * 255).astype(np.uint8).tobytes())
    proc.stdin.close()
    proc.wait()


def leak(path: Path, seconds: float, fps: int, height: int, seed: int = 5) -> None:
    """A warm anamorphic leak that sweeps across the frame and burns out."""
    rng = np.random.default_rng(seed)
    n = int(seconds * fps)
    yy, xx = np.mgrid[0:height, 0:WIDTH].astype(np.float32)
    cy = height * rng.uniform(0.35, 0.6)
    proc = _writer(path, WIDTH, height, fps)
    for f in range(n):
        p = f / max(1, n - 1)
        env = math.sin(math.pi * p) ** 1.5
        cx = WIDTH * (-0.2 + 1.4 * p)
        blob = np.exp(-(((xx - cx) / (WIDTH * 0.22)) ** 2) - (((yy - cy) / (height * 0.55)) ** 2))
        streak = np.exp(-(((yy - cy) / (height * 0.035)) ** 2)) * np.exp(
            -(((xx - cx) / (WIDTH * 0.7)) ** 2))
        light = (blob * 0.85 + streak * 0.6) * env
        rgb = np.stack([light, light * 0.58, light * 0.26], axis=-1)
        proc.stdin.write((np.clip(rgb, 0, 1) * 255).astype(np.uint8).tobytes())
    proc.stdin.close()
    proc.wait()


PLATES = {"embers": (embers, 24.0), "dust": (dust, 24.0), "leak": (leak, 1.6)}


def plate(kind: str, work: Path, fps: int, height: int) -> Path:
    fn, seconds = PLATES[kind]
    out = work / f"plate_{kind}_{fps}_{height}.mov"
    if not out.exists():
        fn(out, seconds, fps, height)
    return out
