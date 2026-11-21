#!/usr/bin/env python3
"""Replays an AI environment's tool calls from its journal and checks they give the very same results (step 7 of the
roadmap, reports/Architecture agents ML évaluation.md).

The journal (<data>/ai/env-N/tool-journal.jsonl, src/ai/Engine.cpp) keeps, for each tool call, its arguments, the
evaluation clock it ran under (as_of), the tool's clock class and the SHA-256 (16 hex digits) of the exact text the model
got. A call made in evaluation mode by a tool that answers as of a moment ("asof") must give the same text again at the
same clock, on the same stored data: this script calls those tools again through the app's MCP server (the stand-in
`claude` of tools/asof_leaks), on a copy of the profile, and compares the fingerprints.

  python3 tools/replay_journal.py --journal <profile>/data/ai/env-1/tool-journal.jsonl --profile <profile> [--work <dir>]

Left out: calls made in the present (as_of 0: the present has moved on), live and timeless tools, refused calls, and
the backtest tools unless --backtests (their answers count the tries of the trial registry, which only grows; a try
recorded since the journal at a moment before it changes the count).
A result can also differ because the data changed underneath: a hole filled later by the collector, a day's archive
rebuilt. The report names each call that differs, with the date of the stored data it read.
"""
import argparse, hashlib, json, os, shutil, sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import check_asof_leaks as leaks   # noqa: E402  (copy_profile, run: the same harness)

MAX_CALLS = 18   # under the app's 20 tool calls a question


def fingerprint(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--journal", required=True)
    ap.add_argument("--profile", required=True, help="the profile whose data the calls read (only copied)")
    ap.add_argument("--app", default=os.path.join(os.path.dirname(HERE), "build", "Deepwater.app", "Contents", "MacOS", "Deepwater"))
    ap.add_argument("--work", default=os.path.join(os.environ.get("TMPDIR", "/tmp"), "replay-journal"))
    ap.add_argument("--backtests", action="store_true")
    args = ap.parse_args()

    rows, skipped = [], {"present": 0, "not asof": 0, "refused or failed": 0, "backtest": 0, "seconds in the clock": 0}
    for line in open(args.journal, encoding="utf-8"):
        try:
            o = json.loads(line)
        except ValueError:
            continue
        if o.get("kind") != "tool" or "result_sha" not in o:
            continue
        if not o.get("as_of"):
            skipped["present"] += 1
        elif o.get("clock") != "asof":
            skipped["not asof"] += 1
        elif o.get("blind"):
            skipped["blind"] = skipped.get("blind", 0) + 1   # what the model saw was masked: not the tool's own text
        elif not o.get("ok"):
            skipped["refused or failed"] += 1
        elif o["tool"].startswith("bt_") and not args.backtests:
            skipped["backtest"] += 1
        elif int(o["as_of"]) % 60:
            skipped["seconds in the clock"] += 1
        else:
            rows.append(o)
    print(f"{len(rows)} call(s) to replay; left out: " + ", ".join(f"{v} {k}" for k, v in skipped.items() if v))
    if not rows:
        return 0

    os.makedirs(args.work, exist_ok=True)
    home = os.path.join(args.work, "home")
    os.makedirs(os.path.join(home, ".local", "bin"), exist_ok=True)
    wrapper = os.path.join(home, ".local", "bin", "claude")
    with open(wrapper, "w") as f:
        f.write(f'#!/bin/sh\nexec /usr/bin/python3 -I "{os.path.join(HERE, "asof_leaks", "fake_claude.py")}" "$@"\n')
    os.chmod(wrapper, 0o755)
    copy = os.path.join(args.work, "profile")
    leaks.copy_profile(args.profile, copy)

    groups = {}
    for o in rows:
        groups.setdefault(int(o["as_of"]), []).append(o)
    same = differ = missing = 0
    for as_of in sorted(groups):
        d_text = datetime.fromtimestamp(as_of, timezone.utc).strftime("%Y-%m-%d %H:%M")
        todo = groups[as_of]
        for k in range(0, len(todo), MAX_CALLS):
            part = todo[k:k + MAX_CALLS]
            calls = [[o["tool"], o.get("arguments") or {}] for o in part]
            got = leaks.run(args.app, home, copy, d_text, calls, os.path.join(args.work, "out.txt"), raw=True)
            for i, o in enumerate(part):
                if i >= len(got):
                    print(f"  MISSING {d_text} {o['tool']} {json.dumps(o.get('arguments'))[:80]}")
                    missing += 1
                    continue
                text = got[i][1]
                if fingerprint(text) == o["result_sha"]:
                    same += 1
                else:
                    differ += 1
                    print(f"  DIFFERS {d_text} {o['tool']} {json.dumps(o.get('arguments'))[:80]} (journal {o['result_sha']}, now {fingerprint(text)})")
        print(f"== {d_text} UTC: {len(todo)} call(s) replayed")
    shutil.rmtree(copy, ignore_errors=True)
    print(f"\n{same} identical, {differ} different, {missing} missing")
    return 0 if differ == 0 and missing == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
