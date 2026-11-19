// Reference outputs of quant-terminal's series.js, backtest.js, quant.js and seasonals.js for the C++ port tests.
//   /opt/homebrew/bin/node tests/charts/tools/dump-strategy.mjs
// writes tests/charts/core/fixtures/strategy-btc-1d.json (read by tests/charts/core/strategy_tests.cpp).
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";
import path from "node:path";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const qt = path.join(root, "quant-terminal");
const lib = (f) => pathToFileURL(path.join(qt, "public/lib", f)).href;

// seasonals() asks the clock for the current year: freeze it so the fixture doesn't change on 1 January.
const FIXED_NOW = Date.UTC(2026, 10, 16, 12);
const RealDate = Date;
globalThis.Date = class extends RealDate {
  constructor(...a) { super(...(a.length ? a : [FIXED_NOW])); }
  static now() { return FIXED_NOW; }
};

const S = await import(lib("series.js"));
const B = await import(lib("backtest.js"));
const Q = await import(lib("quant.js"));
const { seasonals } = await import(lib("seasonals.js"));
// rng() is private to quant.js: load a copy that exports it, to dump the raw sequence.
const qsrc = readFileSync(path.join(qt, "public/lib/quant.js"), "utf8")
  .replace('"./backtest.js"', JSON.stringify(lib("backtest.js")))
  .replace('"./indicators.js"', JSON.stringify(lib("indicators.js")));
const { rng } = await import("data:text/javascript;base64," + Buffer.from(qsrc + "\nexport { rng };").toString("base64"));

const bars = JSON.parse(readFileSync(path.join(qt, "test/fixtures/btc-1d.json"), "utf8"));
const attempt = (f) => { try { return { out: f() }; } catch (e) { return { error: e.message }; } };

// ---------------------------------------------------------------- series
const series = {};
series.parseTf = ["1s", "59s", "15m", "4h", "1D", "2D", "1W", "3M", "M", " 5m ", "0m", "banana", "1d", "1Y", "007h"]
  .map((tf) => ({ tf, ...attempt(() => S.parseTf(tf)) }));
const times = [0, 1705449600, 1760531400, 1700000000, -86400 * 3 - 5, 951782400, 4102444799];
series.bucketStart = [];
for (const t of times)
  for (const tf of ["1m", "5m", "1h", "4h", "1D", "3D", "1W", "2W", "1M", "3M", "12M"])
    series.bucketStart.push({ t, tf, out: S.bucketStart(t, tf) });
const N = ["1m", "5m", "15m", "1h", "4h", "1D", "1W", "1M"];
series.nativeBase = [
  ...["7m", "2h", "3D", "1W", "3M", "1M", "30s", "2W", "6h", "45m", "1D"].map((tf) => ({ tf, natives: N })),
  { tf: "2h", natives: ["60s", "1m", "30m"] },
  { tf: "1W", natives: ["1h", "7h"] },
  { tf: "1M", natives: ["2D", "1W"] },
  { tf: "1s", natives: ["1m"] },
].map((x) => ({ ...x, out: S.nativeBase(x.tf, x.natives) }));
series.resample = {};
for (const tf of ["2D", "3D", "1W", "2W", "1M", "3M"]) series.resample[tf] = S.resample(bars, tf);
series.heikinAshi = S.heikinAshi(bars);
series.renko = { auto: S.renko(bars), box1000: S.renko(bars, 1000), short: S.renko(bars.slice(0, 1)), two: S.renko(bars.slice(0, 2)) };
series.lineBreak = { l3: S.lineBreak(bars), l2: S.lineBreak(bars, 2), l1: S.lineBreak(bars, 1), l0: S.lineBreak(bars, 0) };
series.rangeBars = { auto: S.rangeBars(bars), r2000: S.rangeBars(bars, 2000), one: S.rangeBars(bars.slice(0, 1)) };
series.kagi = { auto: S.kagi(bars), r3000: S.kagi(bars, 3000) };
series.pointFigure = { auto: S.pointFigure(bars), b1000r3: S.pointFigure(bars, 1000, 3), b500r2: S.pointFigure(bars, 500, 2) };
const csvBars = bars.slice(0, 12).map((b, i) => (i === 3 ? { ...b, volume: undefined } : b));
series.csv = {
  bars: csvBars.map((b) => ({ ...b, volume: b.volume ?? null })),
  extra: { rsi: [1, 2.5, null, 4, 1e21, 1e-7, -0.1], "ema_value": [0.1 + 0.2, 1 / 3] },
  text: S.toCsv(csvBars, { rsi: [1, 2.5, NaN, 4, 1e21, 1e-7, -0.1], "ema_value": [0.1 + 0.2, 1 / 3] }),
  renkoText: S.toCsv(S.renko(bars.slice(0, 60), 500)),
};
const nums = [0.1, 0.2 + 0.1, 1e21, 1e-7, 1.5e-6, 123456789012345680000, 5e-324, 1.7976931348623157e308, 0.000001,
  100, 43137.94 * 1.0005, -0.000001234, 1 / 3, 2 ** 53, 2 ** 70, -1e-7, 1e20, 12345.678, 9.95];
series.numStr = nums.map((v) => [v, String(v)]);
series.toFixed = [];
for (const v of [0.125, 1.005, -0.001, 2.5, -2.5, 0.5, 1.45, 1e21, 123.456, 0, 10.235, 99.995, -1234.5678, 1e-10,
  0.045, 8.345, 1.0000000000000002, 5e15 + 0.5, 4.35, 1 / 3])
  for (const d of [0, 1, 2, 3]) series.toFixed.push([v, d, v.toFixed(d)]);
series.iso = [0, -1, 1705449600, 253402300799, 253402300800, -62135596800, -62135596801, 951827696]
  .map((t) => [t, new Date(t * 1000).toISOString()]);
series.isIntraday = ["1s", "15m", "23h", "24h", "1D", "1W"].map((tf) => [tf, S.isIntraday(tf)]);

// ---------------------------------------------------------------- backtest
// Synthetic lower-timeframe bars for the magnifier: three per day on the bar's OHLC path, none for the last 100 days.
const subBars = [];
for (const b of bars.slice(0, -100)) {
  const p = b.close >= b.open ? [b.open, b.low, b.high, b.close] : [b.open, b.high, b.low, b.close];
  for (let k = 0; k < 3; k++)
    subBars.push({ time: b.time + k * 28800, open: p[k], high: Math.max(p[k], p[k + 1]), low: Math.min(p[k], p[k + 1]), close: p[k + 1], volume: 1 });
}
const ex = B.EXAMPLE_STRATEGIES;
const cases = [];
for (const [name, s] of Object.entries(ex)) {
  cases.push({ name, strategy: s });
  cases.push({ name: `${name} / ohlc`, strategy: { ...s, intrabar: "ohlc" } });
  cases.push({ name: `${name} / magnifier`, strategy: { ...s, intrabar: "magnifier", stopLossPct: s.stopLossPct || 4, takeProfitPct: 12 }, sub: true });
}
const rsiShort = {
  indicators: { rsi: { id: "rsi", params: { length: 14 } } },
  entryShort: [{ left: "rsi", op: "crossesBelow", right: 70 }],
  exitShort: [{ left: "rsi", op: "crossesBelow", right: 35 }],
  stopLossPct: 5, takeProfitPct: 10, spreadPct: 0.1,
};
cases.push({ name: "RSI short / worst", strategy: rsiShort });
cases.push({ name: "RSI short / ohlc", strategy: { ...rsiShort, intrabar: "ohlc", trailingStopPct: 6 } });
cases.push({ name: "RSI short / magnifier", strategy: { ...rsiShort, intrabar: "magnifier", trailingStopPct: 6 }, sub: true });
const reverse = {
  indicators: { fast: { id: "ema", params: { length: 9 } }, slow: { id: "sma", params: { length: 30 } } },
  entryLong: [{ left: "fast", op: "crossesAbove", right: "slow" }],
  entryShort: [{ left: "fast", op: "crossesBelow", right: "slow" }],
  trailingStopPct: 5, maxBarsInTrade: 20,
};
cases.push({ name: "Reverse + trailing + time exit", strategy: reverse });
cases.push({ name: "Reverse / ohlc", strategy: { ...reverse, intrabar: "ohlc", maxBarsInTrade: 0, trailingStopPct: 0, takeProfitPct: 7 } });
cases.push({ name: "Reverse with exits", strategy: { ...reverse, exitLong: [{ left: "fast", op: "crosses", right: "close" }], exitShort: [{ left: "close", op: ">", right: "slow" }], trailingStopPct: 0 } });
cases.push({
  name: "exitMatch any, sizing, capital",
  strategy: {
    indicators: { bb: { id: "bb", params: { length: 20, mult: 2 } }, r: { id: "rsi", params: {} } },
    entryLong: [{ left: "close", op: "<", right: "bb.lower" }],
    exitLong: [{ left: "close", op: ">", right: "bb.basis" }, { left: "r", op: ">=", right: "65" }],
    exitMatch: "any", positionPct: 50, initialCapital: 2500, feePct: 0.2, slippagePct: 0.1,
  },
});
const t = (i) => bars[i].time;
cases.push({
  name: "drawn lines",
  strategy: {
    entryLong: [{ left: "close", op: "crossesAbove", right: { line: { t1: t(100), p1: 40000, t2: t(400), p2: 70000, extend: "right" } } }],
    exitLong: [{ left: "close", op: "crossesBelow", right: { line: { t1: t(150), p1: 45000, t2: t(600), p2: 90000, extend: "both" } } }],
    entryShort: [{ left: "low", op: "<", right: { line: { t1: t(700), p1: 100000, t2: t(900), p2: 95000 } } }],
    exitShort: [{ left: "close", op: ">", right: "hl2" }],
  },
});
cases.push({
  name: "price fields and numeric strings",
  strategy: {
    entryLong: [{ left: "hlc3", op: ">", right: "ohlc4" }, { left: "volume", op: ">", right: " 20000 " }],
    exitLong: [{ left: "hlcc4", op: "<=", right: "open" }],
    stopLossPct: 3, takeProfitPct: 6, feePct: 0, slippagePct: 0, spreadPct: 0.05, intrabar: "ohlc",
  },
});
cases.push({ name: "no trades", strategy: { entryLong: [{ left: "close", op: ">", right: 1e9 }] } });
cases.push({ name: "empty rules", strategy: {} });
const backtest = cases.map((c) => {
  const r = B.backtest(bars, c.strategy, c.sub ? { subBars } : {});
  return { name: c.name, strategy: c.strategy, sub: !!c.sub, result: r, csv: B.tradesCsv(r.trades) };
});
const errors = [
  { left: "nope", op: ">", right: 1 },
  { left: "macd.nothing", op: ">", right: 1 },
  { left: "close", op: "=>", right: 1 },
  { left: "close", op: ">", right: "zz" },
  { op: ">", right: 1 },
].map((rule) => {
  const ind = { macd: { id: "macd", params: {} } };
  return { rule, ...attempt(() => { const f = B.conditionFn(bars, ind)(rule); return bars.map((_, i) => f(i)).filter(Boolean).length; }) };
});
errors.push({ rule: "unknown indicator", indicators: { x: { id: "nope" } }, ...attempt(() => B.conditionFn(bars, { x: { id: "nope" } })({ left: "x", op: ">", right: 1 })) });
const condRules = [
  { left: "close", op: "crosses", right: "open" },
  { left: "macd.macd", op: "crossesAbove", right: "macd.signal" },
  { left: "macd", op: "crossesBelow", right: 0 },
  { left: "rsi", op: ">=", right: "70" },
  { left: "close", op: "<", right: { line: { t1: t(10), p1: 40000, t2: t(20), p2: 41000, extend: "none" } } },
  { left: "volume", op: "<=", right: "0x7530" },
];
const condInd = { macd: { id: "macd", params: {} }, rsi: { id: "rsi", params: { length: 7 } } };
const conditions = condRules.map((rule) => {
  const f = B.conditionFn(bars, condInd)(rule);
  return { rule, true: bars.flatMap((_, i) => (f(i) ? [i] : [])) };
});
const magnifier = [[14400, 1000 * 14400], [86400, 1000 * 86400], [3600, 300 * 3600], [3600, 500 * 3600], [60, 60000], [86400, 1e9]]
  .map(([a, b]) => ({ tf: a, span: b, out: B.magnifierTf(a, b) }));

// ---------------------------------------------------------------- quant
const quant = {};
quant.normCdf = [-5, -1.96, -1, -0.3, 0, 0.5, 1.96, 3, 8].map((x) => [x, Q.normCdf(x)]);
quant.normInv = [1e-6, 0.001, 0.02, 0.02425, 0.2, 0.5, 0.8, 0.97575, 0.975, 0.999, 0, 1].map((p) => [p, Q.normInv(p)]);
const steady = Array.from({ length: 500 }, (_, i) => 0.002 + 0.001 * Math.sin(i));
const seeded = (s) => () => ((s = (s * 16807) % 2147483647) / 2147483647) - 0.5;
const r42 = seeded(42), noise = Array.from({ length: 2000 }, () => r42() * 0.02);
quant.series = { steady, noise };
quant.moments = [steady, noise, [1, 2, 3, 4, 100], [5, 5, 5]].map((r) => ({ r, out: Q.moments(r) }));
quant.sharpe = [Q.sharpe(steady), Q.sharpe(noise), Q.sharpe([1]), Q.sharpe([2, 2, 2])];
quant.psr = [Q.psr(steady), Q.psr(noise), Q.psr(noise, 0.01), Q.psr(steady.slice(0, 9)), Q.psr(Array(20).fill(1))];
quant.expectedMaxSharpe = [[100, 0.01], [10, 0.01], [1, 0.01], [25, 0.0004], [5, 0]].map(([n, v]) => [n, v, Q.expectedMaxSharpe(n, v)]);
const T = 400;
const good = Array.from({ length: T }, (_, i) => 0.01 + 0.002 * Math.sin(i));
const others = Array.from({ length: 5 }, (_, k) => Array.from({ length: T }, (_, i) => 0.001 * Math.sin(i * (k + 2))));
let s7 = 7; const r7 = () => ((s7 = (s7 * 16807) % 2147483647) / 2147483647) - 0.5;
const noiseM = Array.from({ length: 12 }, () => Array.from({ length: T }, () => r7()));
quant.pbo = [
  { matrix: [good, ...others], blocks: 8 },
  { matrix: noiseM, blocks: 8 },
  { matrix: noiseM.slice(0, 4), blocks: 6 },
  { matrix: noiseM.slice(0, 3), blocks: 5 },
  { matrix: [good], blocks: 8 },
  { matrix: [good.slice(0, 30), good.slice(0, 30)], blocks: 8 },
].map((x) => ({ ...x, out: Q.pbo(x.matrix, x.blocks) }));
quant.rng = [7, 0, 42, -1, 4294967297, 123.9].map((seed) => { const r = rng(seed); return { seed, seq: Array.from({ length: 40 }, () => r()) }; });
const tr = [5, -2, 3, -1, 8, -4, 2, 1, -3, 6];
quant.monteCarlo = [
  { trades: tr, runs: 2000, seed: 7 },
  { trades: tr, runs: 300, seed: 7 },
  { trades: tr.map((x) => x * 3.7), runs: 500, seed: 11 },
  { trades: tr.slice(0, 4), runs: 100, seed: 7 },
].map((x) => ({ ...x, out: Q.monteCarlo(x.trades, x.runs, x.seed) }));
const ema = ex["EMA cross (9/21)"];
const qcCases = [
  { name: "EMA cross 300", strategy: ema, mcRuns: 300 },
  { name: "RSI default runs", strategy: ex["RSI mean reversion (30/55)"] },
  { name: "MACD + trend", strategy: ex["MACD + 200 EMA trend filter"], mcRuns: 200 },
  { name: "Supertrend", strategy: ex["Supertrend follow"], mcRuns: 200 },
  { name: "Bollinger", strategy: ex["Bollinger breakout"], mcRuns: 200 },
  { name: "no tunables", strategy: { indicators: {}, entryLong: [{ left: "close", op: "crossesAbove", right: 30000 }], exitLong: [{ left: "close", op: "crossesBelow", right: 30000 }] }, mcRuns: 100 },
  { name: "sized, no slippage, spread", strategy: { ...ema, slippagePct: 0, positionPct: 20, spreadPct: 0.1 }, mcRuns: 300 },
  { name: "no fees", strategy: { ...ema, slippagePct: 0, feePct: 0 }, mcRuns: 100 },
  { name: "magnifier", strategy: { ...ema, intrabar: "magnifier", stopLossPct: 3 }, mcRuns: 100, sub: true },
];
// Key order matters for the tunables: the text keeps it, the C++ reads it with indicatorOrder().
const orderedText = '{"indicators":{"slow":{"id":"sma","params":{"length":30}},"fast":{"id":"ema","params":{"length":9}}},' +
  '"entryLong":[{"left":"fast","op":"crossesAbove","right":"slow"}],"exitLong":[{"left":"fast","op":"crossesBelow","right":"slow"}]}';
qcCases.push({ name: "key order", strategy: JSON.parse(orderedText), text: orderedText, mcRuns: 100 });
quant.quantChecks = qcCases.map((c) => ({ ...c, out: Q.quantChecks(bars, c.strategy, { mcRuns: c.mcRuns, subBars: c.sub ? subBars : null }) }));

// ---------------------------------------------------------------- seasonals
// A longer synthetic history with a missing year (2019) so a year can't chain to the one before.
const synth = [];
for (let d = RealDate.UTC(2014, 5, 3) / 1000; d < RealDate.UTC(2026, 9, 1) / 1000; d += 86400) {
  const y = new RealDate(d * 1000).getUTCFullYear(), dow = new RealDate(d * 1000).getUTCDay();
  if (y === 2019 || dow === 0 || dow === 6) continue;
  const k = d / 86400;
  const c = 100 + 30 * Math.sin(k / 47) + 10 * Math.cos(k / 13) + k / 200;
  synth.push({ time: d, open: c - 0.5, high: c + 1, low: c - 1, close: c, volume: 1 });
}
const seas = { thisYear: 2026, btc: seasonals(bars), synth: seasonals(synth), synthBars: synth };

const out = { generatedBy: "tests/charts/tools/dump-strategy.mjs", node: process.version, series, backtest, errors, conditions, magnifier, quant, seasonals: seas };
const file = path.join(root, "tests/charts/core/fixtures/strategy-btc-1d.json");
// NaN / Infinity survive as strings so the C++ can tell them from null.
writeFileSync(file, JSON.stringify(out, (k, v) => (typeof v === "number" && !Number.isFinite(v) ? String(v) : v)));
console.log(`wrote ${file}`);
