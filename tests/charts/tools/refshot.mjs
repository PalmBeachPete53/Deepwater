// Screenshot the ORIGINAL quant-terminal (running locally) through Brave's DevTools protocol, for side-by-side
// comparison with the C++ port. Plain --screenshot hangs on this page (the /api/events stream never idles).
//   node tests/charts/tools/refshot.mjs <url> <out.png> [width=1600] [height=1000] [waitMs=6000] [script.js]
// script.js (optional): JavaScript evaluated in the page after load (e.g. click a tab), then waitMs again.
import { spawn } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const [url, out, w = "1600", h = "1000", waitMs = "6000", script] = process.argv.slice(2);
if (!url || !out) { console.error("usage: refshot.mjs <url> <out.png> [w] [h] [waitMs] [script.js]"); process.exit(2); }
const BRAVE = "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser";
const port = 9300 + Math.floor(Math.random() * 500);
const prof = fs.mkdtempSync(path.join(os.tmpdir(), "refshot-"));
const proc = spawn(BRAVE, ["--headless=new", "--disable-gpu", "--hide-scrollbars", `--remote-debugging-port=${port}`,
  `--user-data-dir=${prof}`, `--window-size=${w},${h}`, "about:blank"], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let ws, seq = 0; const waiting = new Map();
const send = (method, params = {}) => new Promise((res, rej) => {
  const id = ++seq; waiting.set(id, { res, rej }); ws.send(JSON.stringify({ id, method, params }));
});
try {
  let target;
  for (let i = 0; i < 50 && !target; i++) {
    await sleep(200);
    try { target = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find((t) => t.type === "page"); } catch {}
  }
  if (!target) throw new Error("Brave did not start");
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((r) => ws.addEventListener("open", r, { once: true }));
  ws.addEventListener("message", (m) => { const j = JSON.parse(m.data); const p = waiting.get(j.id); if (p) { waiting.delete(j.id); j.error ? p.rej(new Error(j.error.message)) : p.res(j.result); } });
  await send("Emulation.setDeviceMetricsOverride", { width: +w, height: +h, deviceScaleFactor: 1, mobile: false });
  await send("Page.enable");
  await send("Page.navigate", { url });
  await sleep(+waitMs);
  if (script) {
    await send("Runtime.evaluate", { expression: fs.readFileSync(script, "utf8"), awaitPromise: true });
    await sleep(+waitMs);
  }
  const { data } = await send("Page.captureScreenshot", { format: "png" });
  fs.writeFileSync(out, Buffer.from(data, "base64"));
  console.log(`wrote ${out}`);
} finally {
  try { ws?.close(); } catch {}
  proc.kill();
  fs.rmSync(prof, { recursive: true, force: true });
}
