#!/usr/bin/env python3
"""Checks the perpetual's book depth and open interest stored in technical-data.sqlite against Binance's archives,
apart from the C++ code: for random stored days it downloads that day's bookDepth / metrics zip again, reads the CSV
in Python and compares every snapshot band (depth_days, decoded here) and every 5-minute row (oi table).

  python3 -I tools/check_perp_history.py <technical-data.sqlite> [days per kind=4] [seed]

Two small files a day from data.binance.vision; nothing is written.
"""
import calendar, io, math, random, sqlite3, struct, sys, time, urllib.request, zipfile, zlib

BANDS = [-5, -4, -3, -2, -1, -0.2, 0.2, 1, 2, 3, 4, 5]
URL = 'https://data.binance.vision/data/futures/um/daily/{k}/BTCUSDT/BTCUSDT-{k}-{d}.zip'


def csv_of(kind, day):
    req = urllib.request.Request(URL.format(k=kind, d=day), headers={'User-Agent': 'archive-check/1.0'})
    with urllib.request.urlopen(req, timeout=60) as r:
        z = zipfile.ZipFile(io.BytesIO(r.read()))
    time.sleep(0.3)
    return z.read(z.namelist()[0]).decode().splitlines()[1:]


def ts(s): return calendar.timegm(time.strptime(s, '%Y-%m-%d %H:%M:%S'))


def decode_depth(blob, day0):
    raw = zlib.decompress(blob[4:])
    assert raw[:4] == b'PDD1'
    n = struct.unpack_from('<I', raw, 4)[0]
    secs = struct.unpack_from(f'<{n}I', raw, 8)
    bands = [struct.unpack_from(f'<{n}f', raw, 8 + 4 * n * (1 + k)) for k in range(12)]
    return {day0 + secs[i]: [bands[k][i] for k in range(12)] for i in range(n)}


def close(a, b):
    if b is None: return a is None or math.isnan(a)
    return a is not None and abs(a - b) <= 1e-6 * max(1.0, abs(b))


def main():
    db = sqlite3.connect(f'file:{sys.argv[1]}?mode=ro', uri=True)
    per = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    rng = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 20261120)
    bad = 0
    days = [r[0] for r in db.execute('SELECT day FROM depth_days ORDER BY day')]
    for day0 in rng.sample(days, min(per, len(days))):
        day = time.strftime('%Y-%m-%d', time.gmtime(day0))
        want = {}
        for r in csv_of('bookDepth', day):
            f = r.split(',')
            want.setdefault(ts(f[0]), [None] * 12)[BANDS.index(float(f[1]))] = float(f[2])
        have = decode_depth(db.execute('SELECT data FROM depth_days WHERE day = ?', (day0,)).fetchone()[0], day0)
        diff = [t for t in want if t not in have or not all(close(have[t][k], want[t][k]) for k in range(12))]
        extra = [t for t in have if t not in want]
        print(f'depth {day}: {len(want)} snapshots in the archive, {len(have)} stored, {len(diff)} differ, {len(extra)} extra')
        bad += bool(diff or extra)
    days = [r[0] for r in db.execute('SELECT day FROM oi_days ORDER BY day')]
    for day0 in rng.sample(days, min(per, len(days))):
        day = time.strftime('%Y-%m-%d', time.gmtime(day0))
        want = {}
        for r in csv_of('metrics', day):
            f = r.split(',')
            want[ts(f[0])] = [float(x) if x else None for x in f[2:8]]
        have = {r[0]: list(r[1:]) for r in db.execute(
            'SELECT t, oi, oi_value, top_acc, top_pos, accounts, taker FROM oi WHERE t >= ? AND t < ?', (day0, day0 + 86400))}
        diff = [t for t in want if t not in have or not all(close(have[t][k], want[t][k]) for k in range(6))]
        extra = [t for t in have if t not in want]
        print(f'oi {day}: {len(want)} rows in the archive, {len(have)} stored, {len(diff)} differ, {len(extra)} extra')
        bad += bool(diff or extra)
    print('ALL MATCH' if bad == 0 else f'{bad} day(s) differ')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
