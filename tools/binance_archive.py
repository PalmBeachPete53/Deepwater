#!/usr/bin/env python3
"""Downloads daily files from Binance's public market-data archive (data.binance.vision) and unzips them.

    tools/binance_archive.py <out_dir> <market> <kind> <symbol> <from YYYY-MM-DD> <to YYYY-MM-DD> [interval]

market: um (USD-M futures) or cm (COIN-M futures); kind: klines (needs an interval, e.g. 5m), metrics,
liquidationSnapshot (COIN-M only, 2023-06-25 … 2024-10-14). Files land as <out_dir>/<market>/<kind>/<symbol>/<name>.csv.
A file already there is not asked again; a day the archive doesn't have (404) is noted and skipped. One request at a
time with a pause (the archive is a public bucket; no key, nothing sent but the file's path).
"""
import datetime, io, os, sys, time, urllib.error, urllib.request, zipfile

def main():
    if len(sys.argv) < 7:
        print(__doc__)
        return 2
    out, market, kind, symbol, d0, d1 = sys.argv[1:7]
    interval = sys.argv[7] if len(sys.argv) > 7 else ""
    day = datetime.date.fromisoformat(d0)
    last = datetime.date.fromisoformat(d1)
    folder = os.path.join(out, market, kind, symbol)
    os.makedirs(folder, exist_ok=True)
    got = missing = cached = 0
    while day <= last:
        stem = f"{symbol}-{interval}-{day}" if kind == "klines" else f"{symbol}-{kind}-{day}"
        target = os.path.join(folder, stem + ".csv")
        if os.path.exists(target):
            cached += 1
        else:
            path = f"data/futures/{market}/daily/{kind}/{symbol}/" + (f"{interval}/" if kind == "klines" else "") + stem + ".zip"
            req = urllib.request.Request("https://data.binance.vision/" + path, headers={"User-Agent": "Deepwater/1.0 (desktop market terminal)"})
            try:
                with urllib.request.urlopen(req, timeout=30) as r:
                    z = zipfile.ZipFile(io.BytesIO(r.read()))
                    with open(target + ".part", "wb") as f:
                        f.write(z.read(z.namelist()[0]))
                    os.replace(target + ".part", target)
                    got += 1
            except urllib.error.HTTPError as e:
                if e.code != 404:
                    raise
                missing += 1
            time.sleep(0.15)
        day += datetime.timedelta(days=1)
    print(f"{market}/{kind}/{symbol}: {got} downloaded, {cached} already there, {missing} not in the archive")
    return 0

if __name__ == "__main__":
    sys.exit(main())
