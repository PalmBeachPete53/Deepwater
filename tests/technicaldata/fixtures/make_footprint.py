# Expected minutes of the footprint fixtures (tests/technicaldata/footprint_archive_tests.cpp), computed here apart
# from the C++ code: the first trades of Binance's archives of 2026-11-18 (spot: 300, USD-M: 300 after the header),
# zipped as Binance does (*-head.zip) and as a stream would write them (*-head-descriptor.zip: CRC after the data).
# Run from this folder: python3 make_footprint.py  →  footprint-expected.json
# The zips themselves: python3 make_footprint.py --from <folder holding the two downloaded days>, named
# spot_BTCUSDT-aggTrades-2026-11-18.zip and futures_BTCUSDT-aggTrades-2026-11-18.zip (data.binance.vision).
import io, json, math, sys, zipfile, decimal
if len(sys.argv) == 3 and sys.argv[1] == '--from':
    for venue, f in [('spot', 'spot_BTCUSDT-aggTrades-2026-11-18.zip'), ('perp', 'futures_BTCUSDT-aggTrades-2026-11-18.zip')]:
        z = zipfile.ZipFile(f'{sys.argv[2]}/{f}')
        name = z.namelist()[0]
        with z.open(name) as fh:
            head = [fh.readline()] if venue == 'perp' else []
            body = [fh.readline() for _ in range(6000)]
        data = b''.join(head + body[::20])
        with zipfile.ZipFile(f'{venue}-2026-11-18-head.zip', 'w', zipfile.ZIP_DEFLATED) as w:
            w.writestr(name, data)
        buf = io.BytesIO()
        class NoSeek(io.RawIOBase):
            def writable(self): return True
            def write(self, b): return buf.write(b)
        with zipfile.ZipFile(NoSeek(), 'w', zipfile.ZIP_DEFLATED) as w:
            with w.open(name, 'w') as e: e.write(data)
        open(f'{venue}-2026-11-18-head-descriptor.zip', 'wb').write(buf.getvalue())
G = math.log(1.0001)
out = {}
for venue in ('spot', 'perp'):
    z = zipfile.ZipFile(f'{venue}-2026-11-18-head.zip')
    rows = z.read(z.namelist()[0]).decode().splitlines()
    minutes = {}
    for r in rows:
        if not r[:1].isdigit():
            continue
        f = r.split(',')
        tid, price, qty, t = int(f[0]), decimal.Decimal(f[1]), decimal.Decimal(f[2]), int(f[5])
        ms = t // 1000 if t > 10**14 else t
        maker = f[6].lower() == 'true'
        m = minutes.setdefault(ms // 60000 * 60, {'t': ms // 60000 * 60, 'open': float(price), 'high': float(price), 'low': float(price),
                                                   'buy': decimal.Decimal(0), 'sell': decimal.Decimal(0), 'trades': 0,
                                                   'first': tid, 'levels': {}})
        m['high'] = max(m['high'], float(price)); m['low'] = min(m['low'], float(price)); m['close'] = float(price); m['last'] = tid
        m['trades'] += 1
        k = math.floor(math.log(float(price)) / G)
        lv = m['levels'].setdefault(k, [decimal.Decimal(0), decimal.Decimal(0)])
        if maker: m['sell'] += qty; lv[0] += qty
        else: m['buy'] += qty; lv[1] += qty
    out[venue] = [{**m, 'buy': float(m['buy']), 'sell': float(m['sell']),
                   'levels': [[k, float(v[0]), float(v[1])] for k, v in sorted(m['levels'].items())]}
                  for m in sorted(minutes.values(), key=lambda m: m['t'])]
json.dump(out, open('footprint-expected.json', 'w'), indent=1)
for v in out: print(v, len(out[v]), 'minutes', sum(m['trades'] for m in out[v]), 'trades')
