#!/usr/bin/env python3
# Stand-in for `claude` used by tools/check_asof_leaks.py (never shipped): answers --version and auth status; with -p it
# calls Deepwater's MCP server with the tool calls listed in $FAKE_CLAUDE_TOOLS ([["tool", {args}], ...]), as Claude Code
# would, writes each answer to $FAKE_CLAUDE_OUT ("===== tool args" then the text), and ends with a one-line answer.
import json, os, sys, time, urllib.request, uuid

a = sys.argv[1:]
if a[:1] == ["--version"]:
    print("2.1.328 (Claude Code)"); sys.exit(0)
if a[:2] == ["auth", "status"]:
    print(json.dumps({"loggedIn": True, "authMethod": "claude.ai"})); sys.exit(0)
question = sys.stdin.read()


def out(o):
    sys.stdout.write(json.dumps(o) + "\n"); sys.stdout.flush()


def say(text):
    out({"type": "stream_event", "event": {"type": "content_block_start", "content_block": {"type": "text"}}})
    out({"type": "stream_event", "event": {"type": "content_block_delta", "delta": {"type": "text_delta", "text": text}}})
    out({"type": "stream_event", "event": {"type": "content_block_stop"}})


out({"type": "system", "subtype": "init", "session_id": str(uuid.uuid4()), "claude_code_version": "2.1.328", "model": "fake"})
if os.environ.get("FAKE_CLAUDE_TOOLS") and "--mcp-config" in a:
    cfg = json.load(open(a[a.index("--mcp-config") + 1]))["mcpServers"]["deepwater"]
    for n, (name, args) in enumerate(json.load(open(os.environ["FAKE_CLAUDE_TOOLS"]))):
        say(f"call {name}\n")
        body = json.dumps({"jsonrpc": "2.0", "id": n + 1, "method": "tools/call", "params": {"name": name, "arguments": args}}).encode()
        req = urllib.request.Request(cfg["url"], data=body, headers=dict(cfg["headers"], **{"Content-Type": "application/json",
                                                                                          "Accept": "application/json, text/event-stream"}))
        try:
            r = urllib.request.urlopen(req, timeout=700).read()
            try:
                txt = json.loads(r)["result"]["content"][0]["text"]
            except Exception:
                txt = r.decode("utf-8", "replace")
        except Exception as e:
            txt = f"(call failed: {e})"
        with open(os.environ["FAKE_CLAUDE_OUT"], "a", encoding="utf-8") as fo:
            fo.write(f"===== {name} {json.dumps(args, sort_keys=True)}\n{txt}\n")
if os.environ.get("FAKE_CLAUDE_EVAL"):   # the evaluation protocol's tests: remembered closes, or a decision at random
    import random, re
    if "Knowledge probe" in question:
        months = re.findall(r"^(20\d\d-\d\d)$", question, re.M)
        say("\n".join(m + (": 90000" if m < "2025-07" else ": unknown") for m in months) + "\n")
    else:
        say("I read the bars.\nDECISION: " + random.choice(["LONG", "SHORT", "FLAT"]) + "\n")
else:
    say("done\n")
out({"type": "result", "subtype": "success", "is_error": False, "result": "done", "duration_ms": 1, "num_turns": 1,
     "total_cost_usd": 0.0, "usage": {"output_tokens": 1}})
time.sleep(0.2)
