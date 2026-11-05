#!/usr/bin/env bash
# Records the promo video from the real UI (Resources/index.html) and writes out/gensexuell-promo.mp4.
# Needs: node with playwright + chromium, python3 with numpy, ffmpeg.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$(cd "$here/../.." && pwd)"
port="${PORT:-8765}"
python3 "$here/make-oneshot.py"
python3 -m http.server "$port" --bind 127.0.0.1 --directory "$root" >/dev/null 2>&1 &
srv=$!; trap 'kill $srv' EXIT; sleep 1
PORT="$port" node "$here/record.js"
dur=$(awk '/^duration/{s+=$2} END{printf "%.2f", s}' "$here/out/frames.txt")
ffmpeg -loglevel error -y -f concat -safe 0 -i "$here/out/frames.txt" \
  -vf "fps=30,format=yuv420p,fade=t=in:st=0:d=0.6,fade=t=out:st=$(echo "$dur - 0.8" | bc):d=0.8" \
  -c:v libx264 -preset slow -crf 18 -movflags +faststart -t "$dur" "$here/out/gensexuell-promo.mp4"
echo "$here/out/gensexuell-promo.mp4"
