// Drives a headless, muted Chrome at an iPhone-sized touch viewport (390 x 844 CSS px, 3x) against PAPERMARCH's web build
// (scripts\serve-web.ps1 serving it).
//   node tools\web_check.mjs http://localhost:8092/ captures\web flow|play|reload
// flow: the game's own flow test (--flow-test), its FLOW lines. play: level 1 won by touch, NEXT, the frame rate while
// the best line plays, the save in localStorage. reload: that save read back, then an iPhone's notch and
// home bar (safe-area insets 59 and 34 CSS px) reaching the game. Screenshots in the out folder.
import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';

const [, , base, outDir, mode = 'play'] = process.argv;
mkdirSync(outDir, { recursive: true });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const port = 9300 + Math.floor(Math.random() * 400);
const chrome = spawn('C:/Program Files/Google/Chrome/Application/chrome.exe', [
  '--headless=new', '--mute-audio', `--remote-debugging-port=${port}`, `--user-data-dir=${outDir}/profile`,
  '--no-first-run', '--no-default-browser-check', '--autoplay-policy=no-user-gesture-required', '--window-size=390,844',
  'about:blank'], { stdio: 'ignore' });

let page;
for (let i = 0; i < 100 && !page; ++i) {
  try { page = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find((t) => t.type === 'page'); } catch { await sleep(200); }
}
if (!page) { console.log('no chrome'); process.exit(2); }
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r));
let nextId = 0;
const pending = new Map();
const logs = [];
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
  if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.args.map((a) => a.value ?? a.description).join(' '));
  if (m.method === 'Runtime.exceptionThrown') logs.push('EXCEPTION ' + (m.params.exceptionDetails.exception?.description ?? m.params.exceptionDetails.text));
});
const send = (method, params = {}) => new Promise((r) => { const id = ++nextId; pending.set(id, r); ws.send(JSON.stringify({ id, method, params })); });
const evaluate = async (expr) => (await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true })).result?.result?.value;
async function waitLog(re, ms) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { if (logs.some((l) => re.test(l))) return true; await sleep(250); }
  return false;
}
async function shot(name) {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  writeFileSync(`${outDir}/${name}.png`, Buffer.from(r.result.data, 'base64'));
}
async function tap(x, y) {  // CSS px
  await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y, id: 1 }] });
  await sleep(60);
  await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  await sleep(250);
}
async function drag(x0, y0, x1, y1, ms) {  // a held finger moving
  await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: x0, y: y0, id: 2 }] });
  const steps = Math.max(2, Math.round(ms / 50));
  for (let i = 1; i <= steps; ++i) {
    const t = i / steps;
    await send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: x0 + (x1 - x0) * t, y: y0 + (y1 - y0) * t, id: 2 }] });
    await sleep(50);
  }
  await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
}
async function load(args) {
  logs.length = 0;
  await send('Page.navigate', { url: base + (args ? '?args=' + encodeURIComponent(args) : '') });
  return waitLog(/first frame swapped/, 60000);
}
const fps = () => evaluate(`new Promise(res => { let n = 0; const t0 = performance.now();
  function f() { n++; const dt = performance.now() - t0; if (dt < 3000) requestAnimationFrame(f); else res(Math.round(n / dt * 10000) / 10); }
  requestAnimationFrame(f); })`);

await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 3, mobile: true });
await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });

if (mode === 'flow') {
  console.log('started:', await load('--no-audio --flow-test'));
  const done = await waitLog(/FLOW \d+ checks/, 8 * 60000);
  for (const l of logs) if (/FLOW (FAIL|\d+ checks)|EXCEPTION|stopped/.test(l)) console.log(l);
  if (!done) console.log('flow test did not finish; last lines:\n' + logs.slice(-8).join('\n'));
  await shot('flow_end');
} else if (mode === 'play') {
  // Level 1 by touch: a finger put on the squad (the game logs where: --log-sites), held and swept left and right until
  // the level ends; NEXT if it was won. Then the frame rate while the bot plays a busy level (--bot).
  console.log('started:', await load('--silent --log-sites --level 1'));
  await sleep(1500);
  console.log('canvas:', JSON.stringify(await evaluate(`(() => { const c = document.getElementById('canvas'); const r = c.getBoundingClientRect();
    return { css: [r.width, r.height], buffer: [c.width, c.height], dpr: devicePixelRatio, status: getComputedStyle(document.getElementById('status')).display }; })()`)));
  await shot('1_level1');
  const dpr = await evaluate('devicePixelRatio');
  const line = [...logs].reverse().find((l) => /PMSITES DRAG:/.test(l));
  const m = line && /DRAG:(\d+),(\d+)/.exec(line);
  const x0 = m ? Number(m[1]) / dpr : 195, y0 = m ? Number(m[2]) / dpr : 650;
  await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: x0, y: y0, id: 3 }] });
  let ended = false, shotTaken = false;
  const t0 = Date.now();
  while (!ended && Date.now() - t0 < 90000) {
    const t = (Date.now() - t0) / 1000;
    await send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: x0 + Math.sin(t * 0.9) * 110, y: y0, id: 3 }] });
    await sleep(50);
    if (!shotTaken && t > 7) { await shot('2_battle'); shotTaken = true; }
    ended = logs.some((l) => /(won|lost) level 1 /.test(l));
  }
  await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  const won = logs.some((l) => /won level 1 /.test(l));
  console.log('level 1 by touch:', won ? 'won' : (ended ? 'lost' : 'did not end in 90 s'));
  await sleep(2500);
  await shot('3_result');
  if (won) {
    await sleep(500);
    const next = [...logs].reverse().find((l) => /PMSITES NEXT:/.test(l));
    const n = next && /NEXT:(\d+),(\d+)/.exec(next);
    if (n) await tap(Number(n[1]) / dpr, Number(n[2]) / dpr);
    console.log('NEXT tapped:', !!n, 'level 2 started:', await waitLog(/level 2 \(/, 8000));
    await sleep(1500);
    await shot('4_level2');
    console.log('localStorage:', JSON.stringify(await evaluate(`Object.keys(localStorage).map(k => k + '=' + localStorage.getItem(k).length + ' chars')`)));
  }
  console.log('started (bot):', await load('--silent --bot best --level 30'));
  await sleep(4000);
  console.log('frames a second while the bot plays level 30:', await fps());
  await sleep(4000);
  await shot('5_bot');
  for (const l of logs) if (/EXCEPTION|error|stopped|audio:|won level|frame /i.test(l)) console.log(l);
} else if (mode === 'reload') {
  // The save of an earlier run in this profile, read back; then an iPhone's notch and home bar (59 and 34 CSS px).
  console.log('started:', await load('--silent'));
  for (const l of logs) if (/save:/.test(l)) console.log(l);
  const r = await send('Emulation.setSafeAreaInsetsOverride', { insets: { top: 59, bottom: 34 } });
  console.log('safe area override:', r.error ? r.error.message : 'ok');
  await send('Page.reload');
  logs.length = 0;
  await waitLog(/first frame swapped/, 60000);
  await sleep(1000);
  console.log('insets css:', await evaluate(`['--sat','--sab'].map(v => getComputedStyle(document.documentElement).getPropertyValue(v)).join(' ')`));
  for (const l of logs) if (/frame |save:/.test(l)) console.log(l);
  await shot('6_notch');
}
ws.close();
chrome.kill();
process.exit(0);
