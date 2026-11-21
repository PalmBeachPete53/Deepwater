#!/usr/bin/env python3
"""Checks the footprint history of Data › Technical data against Binance's own REST trades, apart from the C++ code.

For random stored minutes of each market (technical-data.sqlite, fp_days), it asks Binance's aggTrades API for the
same aggregate trade ids (the source of the live Charts footprint), rebuilds the minute in Python (levels of one basis
point, as src/technicaldata/FootprintArchive.h defines them) and compares: trades, first / last id, open / high / low
/ close, bought / sold volume, and every level's bid and ask volume.

  python3 -I tools/check_footprint_history.py <technical-data.sqlite> [minutes per market=12] [seed]

A few dozen requests to api.binance.com / fapi.binance.com, paced; nothing is written. USD-M serves the trades of the
last two days only (error -4166): its minutes are drawn from the stored days that recent.
"""
import json, math, random, sqlite3, struct, sys, time, urllib.error, urllib.request, zlib
from decimal import Decimal

G = math.log(1.0001)
REST = {'spot': 'https://api.binance.com/api/v3/aggTrades', 'perp': 'https://fapi.binance.com/fapi/v1/aggTrades'}


def decode_day(blob, day):
    raw = zlib.decompress(blob[4:])           # qCompress: 4-byte big-endian length, then a zlib stream
    assert raw[:4] == b'FPD1'
    n = struct.unpack_from('<I', raw, 4)[0]
    p = 8
    def col(fmt):
        nonlocal p
        size = struct.calcsize('<' + fmt)
        out = list(struct.unpack_from('<' + fmt * n, raw, p))
        p += size * n
        return out
    mod, o, h, l, c, buy, sell = col('H'), col('d'), col('d'), col('d'), col('d'), col('d'), col('d')
    trades, first, span, base, nl = col('I'), col('q'), col('I'), col('i'), col('H')
    minutes = []
    for i in range(n):
        bids = struct.unpack_from('<%df' % nl[i], raw, p); p += 4 * nl[i]
        asks = struct.unpack_from('<%df' % nl[i], raw, p); p += 4 * nl[i]
        minutes.append(dict(t=day + mod[i] * 60, open=o[i], high=h[i], low=l[i], close=c[i], buy=buy[i], sell=sell[i],
                            trades=trades[i], first=first[i], last=first[i] + span[i], base=base[i], bids=bids, asks=asks))
    assert p == len(raw)
    return minutes


def get(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'footprint-check/1.0'})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        body = e.read()[:200]
        if b'-4166' in body:
            raise TooOld()
        raise SystemExit(f'{url}: HTTP {e.code} {body!r}')


class TooOld(Exception):
    pass


def rest_minute(venue, first, last):
    out, frm = [], first
    while frm <= last:
        page = get(f'{REST[venue]}?symbol=BTCUSDT&fromId={frm}&limit=1000')
        time.sleep(0.25)
        if not page:
            break
        out += [t for t in page if t['a'] <= last]
        frm = page[-1]['a'] + 1
    return out


def rebuild(trades):
    m = dict(buy=Decimal(0), sell=Decimal(0), levels={})
    for t in trades:
        price, qty = Decimal(t['p']), Decimal(t['q'])
        k = math.floor(math.log(float(price)) / G)
        lv = m['levels'].setdefault(k, [Decimal(0), Decimal(0)])
        if t['m']: m['sell'] += qty; lv[0] += qty
        else: m['buy'] += qty; lv[1] += qty
    prices = [float(t['p']) for t in trades]
    m.update(open=prices[0], close=prices[-1], high=max(prices), low=min(prices), trades=len(trades),
             first=trades[0]['a'], last=trades[-1]['a'], t=trades[0]['T'] // 60000 * 60)
    return m


def main():
    db = sqlite3.connect(f'file:{sys.argv[1]}?mode=ro', uri=True)
    per = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    rng = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 20261120)
    bad = 0
    for venue in ('spot', 'perp'):
        days = [r[0] for r in db.execute('SELECT day FROM fp_days WHERE venue = ? ORDER BY day', (venue,))]
        if venue == 'perp':   # USD-M answers trade ids of the last two days only (code -4166)
            days = [d for d in days if d + 86400 >= time.time() - 2 * 86400]
        if not days:
            print(f'{venue}: no day stored'); continue
        checked = levels = 0
        worst = 0.0
        for _ in range(per):
            day = rng.choice(days)
            blob = db.execute('SELECT data FROM fp_days WHERE venue = ? AND day = ?', (venue, day)).fetchone()[0]
            mins = decode_day(blob, day)
            if venue == 'perp':   # the minutes REST still serves
                mins = [m for m in mins if m['t'] >= time.time() - 2 * 86400 + 600] or mins
            stored = rng.choice(mins)
            try:
                r = rebuild(rest_minute(venue, stored['first'], stored['last']))
            except TooOld:
                continue
            when = time.strftime('%Y-%m-%d %H:%M', time.gmtime(stored['t']))
            problems = []
            for k in ('t', 'trades', 'first', 'last', 'open', 'high', 'low', 'close'):
                if stored[k] != r[k]: problems.append(f'{k} {stored[k]} vs {r[k]}')
            for k in ('buy', 'sell'):
                if abs(stored[k] - float(r[k])) > 1e-9 * max(1, float(r[k])): problems.append(f'{k} {stored[k]} vs {r[k]}')
            want = r['levels']
            have = {stored['base'] + i: (b, a) for i, (b, a) in enumerate(zip(stored['bids'], stored['asks'])) if b or a}
            if set(have) != set(want): problems.append(f'levels {sorted(set(have) ^ set(want))[:5]} differ')
            for k, (b, a) in want.items():
                hb, ha = have.get(k, (0, 0))
                for x, y in ((hb, float(b)), (ha, float(a))):
                    err = abs(x - y) / max(y, 1e-9) if y else abs(x)
                    worst = max(worst, err)
                    if err > 1e-6: problems.append(f'level {k}: {x} vs {y}')
            levels += len(want)
            checked += 1
            if problems:
                bad += 1
                print(f'  {venue} {when}: ' + '; '.join(problems[:6]))
        print(f'{venue}: {checked} minutes, {levels} levels compared with Binance REST, worst level error {worst:.2e} (float32)')
    print('ALL MATCH' if bad == 0 else f'{bad} minute(s) differ')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
