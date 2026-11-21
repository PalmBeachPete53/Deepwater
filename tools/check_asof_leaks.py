#!/usr/bin/env python3
"""Leak test of the agents' evaluation clock (step 3 of the roadmap, reports/Architecture agents ML évaluation.md).

An environment in evaluation mode at moment D must answer exactly the same whatever happened after D. So, for each
decision moment D, the same tool calls run twice through the app's MCP server (a stand-in `claude`, tools/asof_leaks):
  A  on a copy of a profile as it is,
  B  on a copy whose future was made up at random: bars closed after D with random prices and volumes, the indicator
     rows after D deleted, the series' "last bar" and counts pushed forward, holes invented after D, the footprint /
     depth / open-interest days starting at or after D deleted, the open-interest rows after D randomised, and tries
     over later data added to the trial registry; the ML tools build and read a data set cut at D.
Every answer of A and B must be identical (a difference = something read after D: a LEAK). Then the check of the check:
B runs again with a leak planted on purpose (DW_PLANT_LEAK=<tool>: that tool ignores the clock) and that difference must
be found. Last, refusals: a time asked after D and a tool that only knows the present must be refused.

  python3 tools/check_asof_leaks.py --profile <a DW_PROFILE folder holding data> [--work <scratch folder>]

Never point --profile at the user's own data: it is only read (copied), but use a scratch profile anyway.
A day's archive that begins before D (D not at midnight) is left as it is: its minutes after D are not made up, so a
leak inside that one day would not be seen; the moments at midnight cover it.
"""
import argparse, json, os, re, shutil, signal, sqlite3, subprocess, sys, time
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PAIR = "BINANCE:BTCUSDT"


def calls_at(d_text):
    later = datetime.fromtimestamp(stamp(d_text) + 3 * 86400, timezone.utc).strftime("%Y-%m-%d %H:%M")
    return [
        ["get_technical_coverage", {}],
        ["get_technical_data", {"market": "BTCUSDT", "timeframe": "1h", "bars": 30}],
        ["get_technical_data", {"market": "BTCUSDT", "timeframe": "1D", "bars": 10}],
        ["get_technical_data", {"market": "BTCUSDT", "timeframe": "5m", "bars": 24, "columns": ["rsi14", "ema20"]}],
        ["get_footprint", {"market": "BTCUSDT", "venue": "spot", "timeframe": "1h", "bars": 12, "ladder": "last"}],
        ["get_footprint", {"market": "BTCUSDT", "venue": "perp", "timeframe": "15m", "bars": 8}],
        ["get_order_book", {"market": "BTCUSDT"}],
        ["get_open_interest", {"market": "BTCUSDT", "step": "1h", "rows": 24}],
        ["bt_run", {"example": "EMA cross (9/21)", "market": "BTCUSDT", "timeframe": "1h", "bars": 3000}],
        ["bt_quant_checks", {"example": "EMA cross (9/21)", "market": "BTCUSDT", "timeframe": "1h", "bars": 3000}],
        ["bt_trials", {}],
        # must be refused
        ["get_technical_data", {"market": "BTCUSDT", "timeframe": "1h", "bars": 5, "end": later}],
        ["get_quotes", {}],
        # ML (data sets cut at the moment)
        ["ml_load_dataset", {"source": "technical-data", "key": "BINANCE:BTCUSDT", "interval": "1h", "rows": 50000}],
        ["ml_read_dataset", {"dataset_id": "technical-data-binance_btcusdt-1h-50000-asof-" + stamp_id(d_text), "last_rows": 12}],
        ["ml_datasets", {}],
        ["ml_load_dataset", {"source": "prices", "key": "EURUSD"}],   # must be refused: not cut at the moment
    ]


REFUSED = {11, 12, 16}   # indexes of the calls above that must be refused


def stamp_id(d_text):
    return datetime.strptime(d_text, "%Y-%m-%d %H:%M").strftime("%Y%m%d%H%M")


def stamp(d_text):
    return int(datetime.strptime(d_text, "%Y-%m-%d %H:%M").replace(tzinfo=timezone.utc).timestamp())


def copy_profile(src, dst):
    if os.path.exists(dst):
        shutil.rmtree(dst)
    # the ML runs and the Charts recordings are large and read by no tool tested here
    shutil.copytree(src, dst, ignore=lambda d, names: [n for n in names if (d.endswith("/data") and n == "ml") or n == "depth"])


def perturb(profile, d):
    db = sqlite3.connect(os.path.join(profile, "data", "technical-data", "technical-data.sqlite"))
    rnd = "((abs(random()) % 2001) - 1000) / 20000.0"   # ±5 %
    db.executescript(f"""
        UPDATE bars SET o = o * (1 + {rnd}), h = h * (1 + {rnd}), l = l * (1 + {rnd}), c = c * (1 + {rnd}), v = v * (1 + {rnd}),
                        tbv = tbv * (1 + {rnd}) WHERE t + tf > {d};
        DELETE FROM ind WHERE t + tf > {d};
        UPDATE sync SET last = last + 7 * tf, count = count + 7 WHERE last + tf > {d};
        UPDATE ind_sync SET last = last + 3 * tf WHERE last + tf > {d};
        INSERT OR IGNORE INTO gaps (market, tf, from_t, to_t, missing, checked)
            SELECT market, tf, {d} + 10 * tf, {d} + 20 * tf, 9, 1 FROM sync;
        DELETE FROM fp_days WHERE day >= {d};
        DELETE FROM depth_days WHERE day >= {d};
        DELETE FROM oi_days WHERE day >= {d};
        UPDATE oi SET oi = oi * (1 + {rnd}), oi_value = oi_value * (1 + {rnd}), top_pos = top_pos * (1 + {rnd}),
                      taker = taker * (1 + {rnd}) WHERE t > {d};
    """)
    db.commit()
    db.close()
    reg = os.path.join(profile, "data", "trials", "trials.sqlite")
    if os.path.exists(reg):   # tries over later data: an agent at D must not see them
        r = sqlite3.connect(reg)
        for k in range(3):
            r.execute("INSERT INTO trials (at, kind, origin, who, place, family, config, label, market, timeframe, period_from, period_to, "
                      "n, sr, skew, kurt, sharpe, net_pct) VALUES (?, 'backtest', 'agent', 'environment 3 · future', 'bt_run', ?, ?, ?, ?, '1h', "
                      "?, ?, 2000, ?, 0, 3, ?, ?)",
                      (d + 86400 * (5 + k), f"bt|{PAIR}|3600", f"future{k}", f"rules: made-up future try {k}", PAIR,
                       d - 86400 * 60, d + 86400 * (4 + k), 0.2 + 0.1 * k, 9.0 + k, 50.0 + k))
        r.commit()
        r.close()


def run(app, home, profile, d_text, calls, out_file, extra_env=None, timeout=300, raw=False):
    calls_file = out_file + ".calls.json"
    with open(calls_file, "w") as f:
        json.dump(calls, f)
    if os.path.exists(out_file):
        os.remove(out_file)
    env = dict(os.environ, HOME=home, PATH=os.path.join(home, ".local", "bin") + ":" + os.environ.get("PATH", ""),
               FAKE_CLAUDE_TOOLS=calls_file, FAKE_CLAUDE_OUT=out_file, DW_PROFILE=profile,
               DW_CACHE_DIR=os.path.join(profile, "cache"), DW_NO_PRELOAD="1", DW_AI_SLOTS="0:integration/claude-code",
               DW_AI_SKILLS="0:read_data,backtest,ml", DW_AI_CHAT="0", DW_AI_ASK="go", DW_AI_ASOF=f"0:{d_text}")
    env.pop("DW_PLANT_LEAK", None)
    env.update(extra_env or {})
    p = subprocess.Popen([app, "--page", "ai/agents", "--size", "1400x900", "--lang", "en"], env=env,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    try:
        while time.time() - t0 < timeout:
            if os.path.exists(out_file) and open(out_file, encoding="utf-8").read().count("\n===== ") + 1 >= len(calls):
                time.sleep(1)
                break
            if p.poll() is not None:
                break
            time.sleep(1)
    finally:
        p.send_signal(signal.SIGTERM)
        try:
            p.wait(10)
        except subprocess.TimeoutExpired:
            p.kill()
    return answers(out_file, raw)


def answers(out_file, raw=False):
    """(head, text) per call; raw: the text exactly as the model got it (replay_journal.py fingerprints it)."""
    if not os.path.exists(out_file):
        return []
    parts = re.split(r"^===== ", open(out_file, encoding="utf-8").read(), flags=re.M)[1:]
    out = []
    for p in parts:
        head, _, body = p.partition("\n")
        body = body[:-1] if body.endswith("\n") else body   # the newline the stand-in writes after each answer
        out.append((head, body if raw else normalize(body)))
    return out


def normalize(text):
    return re.sub(r"\(\d+(\.\d+)? s\)", "(… s)", text)   # how long the quant checks took


def first_diff(a, b):
    la, lb = a.splitlines(), b.splitlines()
    for i in range(max(len(la), len(lb))):
        x, y = (la[i] if i < len(la) else "∅"), (lb[i] if i < len(lb) else "∅")
        if x != y:
            return f"line {i + 1}:\n      A: {x[:160]}\n      B: {y[:160]}"
    return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--app", default=os.path.join(ROOT, "build", "Deepwater.app", "Contents", "MacOS", "Deepwater"))
    ap.add_argument("--work", default=os.path.join(os.environ.get("TMPDIR", "/tmp"), "asof-leaks"))
    ap.add_argument("--moments", nargs="*", default=["2026-11-10 00:00", "2026-11-14 13:20"])
    ap.add_argument("--plant", default="get_technical_data", help="the tool whose clock is switched off for the check of the check")
    args = ap.parse_args()
    os.makedirs(args.work, exist_ok=True)
    home = os.path.join(args.work, "home")
    os.makedirs(os.path.join(home, ".local", "bin"), exist_ok=True)
    wrapper = os.path.join(home, ".local", "bin", "claude")
    with open(wrapper, "w") as f:
        f.write(f'#!/bin/sh\nexec /usr/bin/python3 -I "{os.path.join(HERE, "asof_leaks", "fake_claude.py")}" "$@"\n')
    os.chmod(wrapper, 0o755)

    failures = 0
    for d_text in args.moments:
        d = stamp(d_text)
        calls = calls_at(d_text)
        print(f"\n== evaluation moment {d_text} UTC ({len(calls)} calls)")
        pa, pb = os.path.join(args.work, "A"), os.path.join(args.work, "B")
        copy_profile(args.profile, pa)
        copy_profile(args.profile, pb)
        perturb(pb, d)
        ra = run(args.app, home, pa, d_text, calls, os.path.join(args.work, "a.txt"))
        rb = run(args.app, home, pb, d_text, calls, os.path.join(args.work, "b.txt"))
        if len(ra) != len(calls) or len(rb) != len(calls):
            print(f"  FAIL: answers missing (A {len(ra)}, B {len(rb)} of {len(calls)})")
            failures += 1
            continue
        for i, ((head, ta), (_, tb)) in enumerate(zip(ra, rb)):
            name = head.split(" ")[0]
            if i in REFUSED:
                ok = ta.startswith("refused") and tb.startswith("refused")
                print(f"  {'ok  ' if ok else 'FAIL'} refused: {head[:90]}")
                failures += not ok
                continue
            if ta.startswith("error") or ta.startswith("refused"):
                print(f"  note: {name} answered «{ta.splitlines()[0][:110]}»")
            if ta == tb:
                print(f"  ok   same answer whatever the future: {head[:90]}")
            else:
                print(f"  LEAK {head[:90]}\n    {first_diff(ta, tb)}")
                failures += 1
        # the check of the check: a tool with its clock switched off must be caught
        copy_profile(args.profile, pb)
        perturb(pb, d)
        rp = run(args.app, home, pb, d_text, calls, os.path.join(args.work, "p.txt"), {"DW_PLANT_LEAK": args.plant})
        caught = [h for (h, t), (_, ta) in zip(rp, ra) if h.split(" ")[0] == args.plant and t != ta]
        print(f"  {'ok  ' if caught else 'FAIL'} planted leak in {args.plant}: {'caught' if caught else 'NOT caught'} ({len(caught)} call(s) differ)")
        failures += not caught
    for p in ("A", "B"):
        shutil.rmtree(os.path.join(args.work, p), ignore_errors=True)
    print(f"\n{'PASS' if failures == 0 else 'FAIL'}: {failures} problem(s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
