# Fixtures of tests/technicaldata/perp_archive_tests.cpp: the head of Binance's USD-M archives (bookDepth: 40
# snapshots of 2023-01-01, without ±0.2 %, and of 2026-11-18; metrics: 40 rows of 2020-09-01, where each row is
# listed twice, and of 2026-11-18), zipped as Binance does, and the values expected, computed here apart from the C++.
#   python3 make_perp.py --from <folder holding BTCUSDT-bookDepth-<day>.zip / BTCUSDT-metrics-<day>.zip>   (the zips)
#   python3 make_perp.py                                                                                   (expected json)
import calendar, json, sys, time, zipfile
FILES = [('bookDepth', '2023-01-01', 40 * 10 + 1), ('bookDepth', '2026-11-18', 40 * 12 + 1),
         ('metrics', '2020-09-01', 81), ('metrics', '2026-11-18', 41)]
if len(sys.argv) == 3 and sys.argv[1] == '--from':
    for kind, day, n in FILES:
        z = zipfile.ZipFile(f'{sys.argv[2]}/BTCUSDT-{kind}-{day}.zip')
        name = z.namelist()[0]
        lines = z.read(name).decode().splitlines(keepends=True)[:n]
        with zipfile.ZipFile(f'perp-{kind}-{day}-head.zip', 'w', zipfile.ZIP_DEFLATED) as w:
            w.writestr(name, ''.join(lines))
BANDS = [-5, -4, -3, -2, -1, -0.2, 0.2, 1, 2, 3, 4, 5]
def ts(s): return calendar.timegm(time.strptime(s, '%Y-%m-%d %H:%M:%S'))
out = {}
for kind, day, n in FILES:
    z = zipfile.ZipFile(f'perp-{kind}-{day}-head.zip')
    rows = [r.split(',') for r in z.read(z.namelist()[0]).decode().splitlines()[1:]]
    if kind == 'bookDepth':
        snaps = {}
        for r in rows:
            snaps.setdefault(ts(r[0]), [None] * 12)[BANDS.index(float(r[1]))] = float(r[2])
        out[f'{kind}-{day}'] = [[t] + v for t, v in sorted(snaps.items())]
    else:
        seen = {}
        for r in rows:
            seen[ts(r[0])] = [float(x) for x in r[2:8]]
        out[f'{kind}-{day}'] = [[t] + v for t, v in sorted(seen.items())]
json.dump(out, open('perp-expected.json', 'w'), indent=0)
for k, v in out.items(): print(k, len(v), 'rows')
