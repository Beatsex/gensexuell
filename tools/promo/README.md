# Promo video

A ~60 s product video (1920 × 1080, 30 fps, silent), recorded from the real UI in `Resources/index.html`.

```
tools/promo/make-video.sh
```

writes `tools/promo/out/gensexuell-promo.mp4`.

- `director.html` holds the stage: intro card, the UI in an iframe, captions, a camera that zooms in, the feature grid and the end card.
- `record.js` drives the UI with Playwright (pull branches, SOW, type a description and GROW, drop a one-shot on Sound → Seed and start the search) and records it via the CDP screencast.
- The page has no JUCE backend in the browser, so `record.js` puts in a small stand-in that answers `textSeed`, `genoSet`, `genoPoll` and the meter with plausible values. The takes and strand scores in the video come from it, not from the engine.
- There is no sound. Put audio recorded from the plugin under it in an editor.

Needs Node with Playwright and Chromium, Python 3 with NumPy, and ffmpeg.
