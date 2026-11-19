// Runs the Pine cases of tests/charts/core/pine_tests.cpp in Node with PineTS (real V8, real Intl/ICU), so the C++
// side (PineTS in QuickJS-ng, Intl over QTimeZone, src/charts/core/Pine.cpp) can be compared value by value.
//   npm install --prefix <scratch> pinets@0.13.1      (the release in third_party/pinets/SOURCES.md)
//   node tests/charts/tools/dump-pine.mjs <scratch>/node_modules/pinets
// writes tests/charts/core/fixtures/pine-node.json: { pinets, cases: [{ name, source, bars, options, plots, keys }] }.
// The candles are built as Pine.cpp builds them (closeTime = min(next open, open + timeframe) - 1); plot values are
// packed as Pine.cpp packs them (numbers, true/false = 1/0, anything else null = NaN; per-bar colours when they differ
// from the plot's own).
import { readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";
import path from "node:path";

const pkg = path.resolve(process.argv[2] || "");
const require = createRequire(path.join(pkg, "package.json"));
const { PineTS, Indicator } = require(pkg);
const version = JSON.parse(readFileSync(path.join(pkg, "package.json"), "utf8")).version;
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../..");
const btc = JSON.parse(readFileSync(path.join(root, "quant-terminal/test/fixtures/btc-1d.json"), "utf8"));

// hourly bars over the 2026 summer-time changes (US 8 Mar and 1 Nov, EU 29 Mar and 25 Oct, Sydney 5 Apr and 4 Oct):
// the daily fixture's prices, one bar an hour from 1 Mar 2026 (and again from 20 Oct), 1200 bars each
const hourly = (from) => btc.slice(0, 1200).map((b, i) => ({ ...b, time: from + i * 3600 }));
const SETS = {
  "btc-1d": btc,
  "h1-spring": hourly(Date.UTC(2026, 2, 1) / 1000),
  "h1-autumn": hourly(Date.UTC(2026, 9, 1) / 1000),
};
const STEP = { D: 86400e3, 60: 3600e3 };

const KEY = `//@version=5
indicator("Key indicators", overlay=true)
plot(ta.sma(close, 20), "sma20")
plot(ta.ema(close, 50), "ema50")
plot(ta.ema(close, 200), "ema200")
[bbm, bbu, bbl] = ta.bb(close, 20, 2)
plot(bbu, "bb_upper")
plot(bbl, "bb_lower")
plot(ta.bbw(close, 20, 2), "bbw")
plot(ta.rsi(close, 14), "rsi")
[m, s, h] = ta.macd(close, 12, 26, 9)
plot(m, "macd")
plot(s, "macd_signal")
plot(h, "macd_hist")
plot(ta.atr(14), "atr")
[dip, dim, adx] = ta.dmi(14, 14)
plot(dip, "di_plus")
plot(adx, "adx")
k = ta.sma(ta.stoch(close, high, low, 14), 1)
plot(k, "stoch_k")
plot(ta.sma(k, 3), "stoch_d")
plot(ta.obv, "obv")
plot(ta.vwap(hlc3), "vwap")
`;
const ZONES = `//@version=5
indicator("Zones")
plot(hour(time, "America/New_York"), "ny_hour")
plot(dayofweek(time, "Europe/Paris"), "paris_dow")
plot(hour(time, "Europe/Dublin"), "dublin_hour")
plot(hour(time, "Australia/Sydney"), "sydney_hour")
plot(dayofmonth(time, "Asia/Kolkata"), "kolkata_day")
plot(minute(time, "Asia/Kolkata"), "kolkata_minute")
plot(hour(time), "chart_hour")
plot(dayofweek, "chart_dow")
plot(ta.vwap(hlc3), "vwap_chart_session")
plot(timestamp("America/New_York", 2026, 3, 8, 3, 30), "ts_ny")
`;
const INPUTS = `//@version=5
indicator("Inputs")
len = input.int(14, "Length")
src = input.source(close, "Source")
plot(ta.rsi(src, len), "rsi")
plot(len, "len")
`;
const STYLES = `//@version=5
indicator("Styles", overlay=true)
r = ta.rsi(close, 14)
plot(r, "rsi", color = r > 50 ? color.green : color.red, linewidth=2)
plotshape(ta.crossover(r, 50), "up", shape.triangleup)
hline(70, "Upper")
bgcolor(r > 70 ? color.new(color.red, 80) : na)
var float hi = na
hi := na(hi) ? high : math.max(hi, high)
plot(hi, "running_high", style=plot.style_stepline)
plot(close[1], "prev_close")
plot(bar_index, "bar")
`;
const CASES = [
  { name: "key-1d", source: KEY, bars: "btc-1d", options: { symbol: "BINANCE:BTCUSDT", timeframe: "D", timezone: "UTC", inputs: {} } },
  { name: "zones-spring", source: ZONES, bars: "h1-spring", options: { symbol: "BINANCE:BTCUSDT", timeframe: "60", timezone: "America/New_York", inputs: {} } },
  { name: "zones-autumn", source: ZONES, bars: "h1-autumn", options: { symbol: "BINANCE:BTCUSDT", timeframe: "60", timezone: "Europe/Paris", inputs: {} } },
  { name: "inputs", source: INPUTS, bars: "btc-1d", options: { symbol: "BINANCE:BTCUSDT", timeframe: "D", timezone: "UTC", inputs: { Length: 30 } } },
  { name: "styles", source: STYLES, bars: "btc-1d", options: { symbol: "BINANCE:BTCUSDT", timeframe: "D", timezone: "UTC", inputs: {} } },
];

const out = [];
for (const c of CASES) {
  const bars = SETS[c.bars], n = bars.length, step = STEP[c.options.timeframe];
  const candles = bars.map((b, i) => {
    const t = b.time * 1000, next = i + 1 < n ? bars[i + 1].time * 1000 : t + step;
    return { openTime: t, open: b.open, high: b.high, low: b.low, close: b.close, volume: b.volume, closeTime: Math.min(next, t + step) - 1 };
  });
  const pine = new PineTS(candles, c.options.symbol, c.options.timeframe);
  pine.setTimezone(c.options.timezone);
  const ctx = await pine.run(new Indicator(c.source, c.options.inputs));
  const index = new Map(candles.map((b, i) => [b.openTime, i]));
  const plots = {}, keys = [];
  for (const [key, p] of Object.entries(ctx.plots)) {
    if (key.startsWith("__") && key.endsWith("__")) continue;
    const values = new Array(n).fill(null);
    const col = new Array(n).fill("");
    let first, varies = false;
    p.data.forEach((d, j) => {
      const i = index.has(d.time) ? index.get(d.time) : p.data.length === n ? j : -1;
      if (i < 0) return;
      const x = d.value;
      values[i] = typeof x === "number" && Number.isFinite(x) ? x : x === true ? 1 : x === false ? 0 : null;
      const c = d.options ? d.options.color : undefined;
      if (c === undefined) return;
      col[i] = c == null ? "na" : String(c);
      if (first === undefined) first = col[i]; else if (col[i] !== first) varies = true;
    });
    let color = p.options ? p.options.color : undefined;
    if (!varies && first !== undefined && first !== "na" && color == null) color = first;
    keys.push(key);
    plots[key] = { title: p.title == null ? key : String(p.title), style: (p.options && p.options.style) || "", color: color == null ? "" : String(color), values, colors: varies ? col : null };
  }
  out.push({ name: c.name, source: c.source, bars: c.bars === "btc-1d" ? "btc-1d" : bars, options: c.options, keys, plots });
  console.log(c.name, keys.length, "plots");
}
writeFileSync(path.join(root, "tests/charts/core/fixtures/pine-node.json"), JSON.stringify({ pinets: version, cases: out }));
