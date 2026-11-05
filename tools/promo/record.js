// Drives the real gensexuell UI inside director.html and records it frame by frame via CDP screencast.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const OUT = path.join(__dirname, 'out', 'frames');
const PORT = process.env.PORT || 8765;

// stand-in for the JUCE backend so the UI behaves like it does in the plugin (the real engine makes sound; the video is silent)
const stub = () => {
  if (!location.pathname.endsWith('/Resources/index.html')) return;
  const L = {};
  const fire = (n, p) => (L[n] || []).forEach(f => f(p));
  let level = 0, notes = 0, t0 = 0;
  setInterval(() => { level = notes ? Math.min(0.55, level + 0.12) : level * 0.82;
    fire('host', { type: 'meter', out: level, voices: notes }); }, 50);
  window.__JUCE__ = { backend: {
    addEventListener(n, f) { (L[n] ||= []).push(f); },
    emitEvent(n, p) {
      if (n === 'ui' && p.type === 'hello') setTimeout(() => fire('host', { type: 'state', net: true }), 30);
      if (n === 'ui' && p.type === 'note') notes = Math.max(0, notes + (p.on ? 1 : -1));
      if (n === '__juce__invoke') {
        let result = null, delay = 20;
        if (p.name === 'textSeed') {
          delay = 900;
          const words = p.params[0].toLowerCase().split(/[\s,]+/).filter(Boolean);
          result = { known: words, unknown: [], note: 40, takes: [0.93, 0.88, 0.84, 0.79].map(fit => ({ fit, genes: {} })) };
        }
        if (p.name === 'genoSet') result = { pitch: 55, note: 33 };
        if (p.name === 'genoRun') { if (p.params[0] && !t0) t0 = performance.now(); result = true; }
        if (p.name === 'genoGet') result = {};
        if (p.name === 'genoPoll') {
          const el = t0 ? (performance.now() - t0) / 1000 : 0;
          const strands = [0, 1, 2, 3].map(s => { const n = Math.min(7, Math.floor(el * 2.2 + s * 0.4));
            const scores = Array.from({ length: n }, (_, i) => Math.min(0.96, 0.42 + 0.5 * (1 - Math.exp(-(i + 1) * (0.32 + s * 0.05)))));
            return { scores, generation: n * 3, done: n >= 7 }; });
          result = { strands, tested: Math.round(el * 410), running: true, netDim: 800, featDim: 800 };
        }
        setTimeout(() => fire('__juce__complete', { promiseId: p.resultId, result }), delay);
      }
    },
  } };
};

(async () => {
  fs.rmSync(OUT, { recursive: true, force: true }); fs.mkdirSync(OUT, { recursive: true });
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1, locale: 'de-DE' });
  await page.addInitScript(stub);
  await page.goto(`http://127.0.0.1:${PORT}/tools/promo/director.html`);
  const ui = page.frameLocator('#ui');
  await ui.locator('#bulbSvg').waitFor();
  await page.evaluate(() => document.fonts.ready);
  await page.waitForTimeout(800);

  // ---- recording
  const cdp = await page.context().newCDPSession(page);
  const frames = [];
  cdp.on('Page.screencastFrame', async ({ data, metadata, sessionId }) => {
    const f = path.join(OUT, `f${String(frames.length).padStart(5, '0')}.jpg`);
    fs.writeFileSync(f, Buffer.from(data, 'base64'));
    frames.push({ f, t: metadata.timestamp });
    cdp.send('Page.screencastFrameAck', { sessionId }).catch(() => {});
  });
  await cdp.send('Page.startScreencast', { format: 'jpeg', quality: 92, maxWidth: 1920, maxHeight: 1080 });
  const W = ms => page.waitForTimeout(ms);
  const T0 = Date.now(); const mark = n => console.log(n, ((Date.now() - T0) / 1000).toFixed(1));
  const ev = (fn, ...a) => page.evaluate(([fn, a]) => window[fn](...a), [fn, a]);

  // cursor helpers: page coordinates, visible cursor follows the real mouse
  let cx = 1500, cy = 900;
  const move = async (x, y, ms = 600) => {
    const steps = Math.max(2, Math.round(ms / 50));
    const x0 = cx, y0 = cy;
    for (let k = 1; k <= steps; k++) {
      const e = k / steps, s = e < .5 ? 2 * e * e : 1 - (-2 * e + 2) ** 2 / 2;
      cx = x0 + (x - x0) * s; cy = y0 + (y - y0) * s;
      await page.mouse.move(cx, cy); await ev('cur', cx, cy, true);
    }
  };
  const down = async () => { await ev('curDown', true); await page.mouse.down(); };
  const up = async () => { await ev('curDown', false); await page.mouse.up(); };
  const click = async (x, y, ms) => { await move(x, y, ms); await down(); await W(110); await up(); };
  const center = async loc => { const b = await loc.boundingBox(); return [b.x + b.width / 2, b.y + b.height / 2]; };

  // bulb geometry from the key squares (radius 248 in svg units)
  const geo = async () => {
    const svgB = await ui.locator('#bulbSvg').boundingBox();
    const [kx, ky] = await center(ui.locator('[data-key="0"] rect'));
    const c = [svgB.x + svgB.width / 2, null];
    // seed centre: key 0 and key 6 sit opposite each other
    const [k6x, k6y] = await center(ui.locator('[data-key="6"] rect'));
    const mx = (kx + k6x) / 2, my = (ky + k6y) / 2;
    const scale = Math.hypot(kx - k6x, ky - k6y) / 2 / 248;
    return { mx, my, scale, a0: Math.atan2(ky - my, kx - mx) };
  };
  const pt = (g, i, r) => { const a = g.a0 + i * Math.PI / 6; return [g.mx + Math.cos(a) * r * g.scale, g.my + Math.sin(a) * r * g.scale]; };
  const pullBranch = async (g, i, rFrom, rTo, ms = 900) => {
    const [x0, y0] = pt(g, i, rFrom); await move(x0, y0, 500); await down();
    const steps = Math.round(ms / 50);
    for (let k = 1; k <= steps; k++) { const e = k / steps; const [x, y] = pt(g, i + Math.sin(e * Math.PI) * 0.15, rFrom + (rTo - rFrom) * e);
      cx = x; cy = y; await page.mouse.move(x, y); await ev('cur', x, y, true); }
    await W(150); await up();
  };

  // ---------------- 0: intro
  await ev('card', 'intro', true); await W(3600);
  await ev('card', 'intro', false); await W(400);
  await ev('showUI', true); await W(1400);

  // ---------------- 1: grow
  mark(' 1: grow');
  await ev('cap', '01', 'Wachsen lassen', '12 Äste um einen Samen. Je länger, desto weiter vom Original.');
  let g = await geo();
  await ev('cam', 1.22, g.mx, g.my); await W(1500);
  g = await geo();
  await pullBranch(g, 2, 70, 215, 1000); await W(350);
  await pullBranch(g, 10, 70, 225, 1000); await W(500);
  // click a few keys to audition
  for (const k of [10, 2]) { await click(...pt(g, k, 248), 350); await W(200); }
  // SOW in the centre
  await click(g.mx, g.my, 500); await W(1200);
  await ev('cam', 1); await W(1500);

  // ---------------- 2: describe
  mark(' 2: describe');
  await ev('cap', '02', 'Beschreiben', 'Deutsch oder Englisch eintippen. Vier Takes pro Klick.');
  const desc = ui.locator('#desc');
  let [dx, dy] = await center(desc);
  await ev('cam', 1.45, dx + 120, dy + 60); await W(1500);
  [dx, dy] = await center(desc);
  await click(dx, dy, 700);
  await desc.pressSequentially('dunkler, weicher Bass', { delay: 75 }); await W(400);
  const [gx, gy] = await center(ui.locator('#bGrow'));
  await click(gx, gy, 500); await W(2000);
  const chips = ui.locator('#takes .chip');
  if (await chips.count() > 2) { await click(...await center(chips.nth(1)), 500); await W(700); await click(...await center(chips.nth(2)), 400); await W(900); }
  await ev('cam', 1); await W(1500);

  // ---------------- 3: sample
  mark(' 3: sample');
  await ev('cap', '03', 'Sample mitbringen', 'One-Shot reinziehen. Ein neuronales Netz findet die Gene.');
  const [sx, sy] = await center(ui.locator('#bSound'));
  await ev('cam', 1.45, sx + 120, sy + 40); await W(1500);
  await move(...await center(ui.locator('#bSound')), 800);
  await page.evaluate(async () => {
    const w = document.getElementById('ui').contentWindow, d = w.document, t = d.getElementById('bSound');
    const buf = await (await fetch('/tools/promo/out/bass-oneshot.wav')).arrayBuffer();
    const dt = new w.DataTransfer(); dt.items.add(new w.File([buf], 'bass-oneshot.wav', { type: 'audio/wav' }));
    t.dispatchEvent(new w.DragEvent('dragover', { dataTransfer: dt, bubbles: true, cancelable: true }));
    await new Promise(r => setTimeout(r, 500));
    t.dispatchEvent(new w.DragEvent('drop', { dataTransfer: dt, bubbles: true, cancelable: true }));
  });
  await W(500);
  const dlg = ui.locator('#dlgSound'); await dlg.waitFor();
  await ev('cam', 1); await W(1450);
  const db = await dlg.boundingBox();
  await ev('cam', 1.45, db.x + db.width / 2, db.y + db.height / 2 + 30); await W(1500);
  const st = ui.locator('#gStart');
  await st.waitFor(); await W(300);
  await click(...await center(st), 800); await W(3600);
  const dots = ui.locator('#gStrands circle');
  if (await dots.count()) { await click(...await center(dots.last()), 700); await W(900); }
  await ev('cap', ''); await ev('cur', cx, cy, false); await W(600);

  // ---------------- 4: features
  mark(' 4: features');
  await ev('card', 'feat', true); await W(4200);
  // ---------------- 5: outro
  mark(' 5: outro');
  await ev('card', 'outro', true); await W(300); await ev('card', 'feat', false); await W(5200);

  await cdp.send('Page.stopScreencast');
  await W(200);
  // concat list with real frame durations
  let list = '';
  for (let i = 0; i < frames.length; i++) {
    const d = i + 1 < frames.length ? frames[i + 1].t - frames[i].t : 0.5;
    list += `file '${frames[i].f}'\nduration ${d.toFixed(4)}\n`;
  }
  list += `file '${frames[frames.length - 1].f}'\n`;
  fs.writeFileSync(path.join(__dirname, 'out', 'frames.txt'), list);
  console.log('frames', frames.length, 'seconds', (frames.at(-1).t - frames[0].t).toFixed(1));
  await browser.close();
})();
