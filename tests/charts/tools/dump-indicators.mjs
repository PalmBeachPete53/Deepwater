// Dumps quant-terminal's indicators (public/lib/indicators.js) so the C++ port can be compared value by value.
//   node tests/charts/tools/dump-indicators.mjs
// writes tests/charts/core/fixtures/indicators-btc-1d.json (every indicator, default params, test/fixtures/btc-1d.json)
// and tests/charts/core/fixtures/indicators-edge.json (edge cases: n <= 0, n = NaN, tiny/flat/intraday bars, NaN holes, ta primitives).
// Numbers that JSON cannot hold are written as strings: "NaN", "Inf", "-Inf", and "undef" for undefined.
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const I = await import(path.join(root, "quant-terminal/public/lib/indicators.js"));
const btc = JSON.parse(readFileSync(path.join(root, "quant-terminal/test/fixtures/btc-1d.json"), "utf8"));

function enc(v) {
  if (v === undefined) return "undef";
  if (typeof v === "number") return Number.isFinite(v) ? v : Number.isNaN(v) ? "NaN" : v > 0 ? "Inf" : "-Inf";
  if (typeof v === "function") return "function";
  if (Array.isArray(v)) return Array.from(v, enc); // holes read as undefined
  if (v && typeof v === "object") {
    const o = {};
    for (const k of Object.keys(v)) o[k] = enc(v[k]);
    return o;
  }
  return v;
}
function run(fn) {
  try {
    const r = fn();
    // keys: the result object's keys in their JS order (JSON readers may sort them)
    return r && typeof r === "object" && !Array.isArray(r) ? { result: enc(r), keys: Object.keys(r) } : { result: enc(r) };
  } catch (e) {
    return { error: String(e && e.message) };
  }
}

// ---- definitions, as the JS has them (calc left out)
const defs = I.INDICATORS.map((d) => {
  const o = {};
  for (const k of Object.keys(d)) if (k !== "calc") o[k] = enc(d[k]);
  return o;
});

// ---- every indicator with its defaults on the BTC daily fixture
const full = {};
for (const d of I.INDICATORS) full[d.id] = run(() => I.compute(d.id, btc));
writeFileSync(path.join(root, "tests/charts/core/fixtures/indicators-btc-1d.json"), JSON.stringify({ defs, full }));

// ---- edge cases
const short = btc.slice(0, 60);
const monday = Date.UTC(2026, 0, 5) / 1000;
const sets = {
  empty: [],
  one: btc.slice(0, 1),
  two: btc.slice(0, 2),
  short,
  // the same prices every 6 hours from a Monday: sessions, weeks and months change inside the set
  h6: short.map((b, i) => ({ ...b, time: monday + i * 6 * 3600 })),
  // 15-minute bars over a day boundary
  m15: short.map((b, i) => ({ ...b, time: monday - 7 * 3600 + i * 900 })),
  // flat bars, zero volumes, repeated closes
  flat: short.map((b, i) => {
    if (i % 7 === 3) return { ...b, high: b.close, low: b.close, open: b.close, volume: 0 };
    if (i % 11 === 5) return { ...b, close: short[i - 1].close, volume: 0 };
    return b;
  }),
  // weekly bars before 1970 and around it (negative times)
  weekly: short.slice(0, 40).map((b, i) => ({ ...b, time: -20 * 7 * 86400 + i * 7 * 86400 })),
};
const cases = [];
const add = (id, set, params) => cases.push({ id, set, params, ...run(() => I.compute(id, sets[set], params)) });
const SOURCES = ["close", "open", "high", "low", "volume", "hl2", "hlc3", "ohlc4", "hlcc4"];
for (const d of I.INDICATORS) {
  for (const s of Object.keys(sets)) add(d.id, s, {});
  for (const inp of d.inputs) {
    if (inp.type === "int" || inp.type === "float")
      for (const v of [0, 1, -1, 3, 1000]) add(d.id, "short", { [inp.key]: v });
    // a length that isn't a number (an API caller's "abc"): NaN in the JS formulas
    if (inp.type === "int") add(d.id, "short", { [inp.key]: "abc" });
    if (inp.type === "float") add(d.id, "short", { [inp.key]: 0.37 });
    if (inp.type === "select") for (const o of inp.options) for (const s of ["short", "h6", "m15", "weekly"]) add(d.id, s, { [inp.key]: o });
    if (inp.type === "source") for (const o of SOURCES) add(d.id, "short", { [inp.key]: o });
    if (inp.type === "time")
      for (const v of [0, short[10].time, short[10].time + 1, short[0].time - 1, short.at(-1).time, short.at(-1).time + 1])
        add(d.id, "short", { [inp.key]: v });
  }
}

// ---- ta primitives on a series with NaN holes
const S = short.map((b) => b.close);
for (const k of [0, 5, 30, 31, 32]) S[k] = NaN;
const VOL = short.map((b) => b.volume);
const LENS = [0, 1, -1, 2, 5, 59, 60, 61, NaN];
const ta = [];
const tadd = (fn, args, f) => ta.push({ fn, args: enc(args), ...run(f) });
for (const n of LENS) {
  for (const fn of ["sum", "sma", "ema", "rma", "wma", "hma", "dema", "tema", "stdev", "dev", "highest", "lowest", "change", "rsiOf"])
    tadd(fn, [n], () => I[fn](S, n));
  tadd("vwma", [n], () => I.vwma(S, VOL, n));
  tadd("alma", [n, 0.85, 6], () => I.alma(S, n));
  tadd("alma", [n, 0.3, 2], () => I.alma(S, n, 0.3, 2));
  tadd("linreg", [n, 0], () => I.linreg(S, n, 0));
  tadd("linreg", [n, 2], () => I.linreg(S, n, 2));
  tadd("linreg", [n, NaN], () => I.linreg(S, n, NaN));
  tadd("atr", [n], () => I.atr(sets.flat, n));
}
tadd("tr", [], () => I.tr(sets.flat));
for (const s of SOURCES.concat(["bogus"])) tadd("source", [s], () => I.source(sets.flat, s));
const A = S.map((v, i) => v + 300 * Math.sin(i / 3)), B = short.map((b) => b.open);
tadd("crossover", ["series"], () => I.crossover(A, B));
tadd("crossunder", ["series"], () => I.crossunder(A, B));
tadd("crossover", [43000], () => I.crossover(A, 43000));
tadd("crossunder", [43000], () => I.crossunder(A, 43000));
tadd("candlePatterns", ["short"], () => I.candlePatterns(short));
tadd("candlePatterns", ["flat"], () => I.candlePatterns(sets.flat));
tadd("candlePatterns", ["btc"], () => I.candlePatterns(btc));
const times = [0, -1, -86400, -86401, 86399, 1705449600, 1709164800, 1709251199, 1735689599, 1735689600, 951782400, 4102444800, -2208988800, -62198755200, 8640000000000, 8640000000001];
for (const p of ["day", "week", "month", "year", "auto", ""])
  tadd("periodKey", [p, times], () => times.map((t) => I.periodKey(t, p)));
const extra = {
  macd: run(() => I.ta.macd(S, 12, 26, 9)),
  macdShort: run(() => I.ta.macd(S, 3, 5, 2)),
  supertrend: run(() => I.ta.supertrend(sets.flat, 3, 10)),
  psar: run(() => I.ta.psar(sets.flat, 0.02, 0.02, 0.2)),
  dmi: run(() => I.ta.dmi(sets.flat, 14, 14)),
};
writeFileSync(
  path.join(root, "tests/charts/core/fixtures/indicators-edge.json"),
  JSON.stringify({ sets: enc(sets), S: enc(S), cases, ta, extra }),
);
console.log(`${defs.length} indicators, ${cases.length} edge cases, ${ta.length} ta calls`);
