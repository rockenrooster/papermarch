// Drives a headless, muted Chrome at an iPhone-sized touch viewport (390 x 844 CSS px, 3x) against GLOWTIDE's web build
// (scripts\serve-web.ps1 serving it).
//   node tools\web_check.mjs http://localhost:8091/ captures\web flow|play|reload
// flow: the game's own flow test (--flow-test), its FLOW lines. play: HOME, the level selector, level 5 picked and
// played by touch, frames a second, the save in localStorage. reload: that save read back, then an iPhone's notch and
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
  const done = await waitLog(/FLOW done/, 8 * 60000);
  for (const l of logs) if (/FLOW (FAIL|done)|EXCEPTION|stopped/.test(l)) console.log(l);
  if (!done) console.log('flow test did not finish; last lines:\n' + logs.slice(-8).join('\n'));
  await shot('flow_end');
} else if (mode === 'play') {
  console.log('started:', await load('--silent'));
  await sleep(1500);
  console.log('canvas:', JSON.stringify(await evaluate(`(() => { const c = document.getElementById('canvas'); const r = c.getBoundingClientRect();
    return { css: [r.width, r.height], buffer: [c.width, c.height], status: getComputedStyle(document.getElementById('status')).display }; })()`)));
  await shot('1_home');
  await tap(50, 32);  // HOME's LEVEL chip
  await sleep(500);
  await shot('2_levels');
  await tap(335, 108);  // level 5: the first row's fifth button (8 rows of 5 at 390 x 844)
  await sleep(800);
  await shot('3_home_level5');
  await drag(195, 600, 140, 600, 4000);  // start the level and pour while steering left
  await sleep(500);
  await shot('4_play');
  console.log('frames a second while playing:', await fps());
  await sleep(20000);
  await shot('5_after_20s');
  console.log('localStorage:', JSON.stringify(await evaluate(`Object.keys(localStorage).map(k => k + '=' + localStorage.getItem(k).length + ' chars')`)));
  for (const l of logs) if (/EXCEPTION|error|stopped|audio:|save:|frame /i.test(l)) console.log(l);
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
