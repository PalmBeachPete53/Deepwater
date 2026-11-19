#!/usr/bin/env python3
# Synthetic Dukascopy files for tests/technicaldata (no Dukascopy data in the repository): EURUSD hour files of
# 2024-01 and 2024-02, minute files of 2024-03-01 … 2024-03-05, in Dukascopy's own format (LZMA "alone", rows of
# 24 bytes big-endian: seconds from the file's start, open, close, low, high in 1e-5, volume float; the closed market
# repeated with no volume). A hole: 2024-02-14 10:00 and 11:00 UTC traded nothing.
#   python3 tests/technicaldata/fixtures/make_dukascopy.py
import datetime as dt, lzma, os, random, struct
from zoneinfo import ZoneInfo

NY = ZoneInfo("America/New_York")
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dukascopy", "EURUSD")
rnd = random.Random(20240101)
price = 110000

def closed(t):
    d = dt.datetime.fromtimestamp(t, NY)
    m = d.hour * 60 + d.minute
    return d.isoweekday() == 6 or (d.isoweekday() == 5 and m >= 17 * 60) or (d.isoweekday() == 7 and m < 17 * 60)

def bar(t, step):
    global price
    if closed(t) or (1707904800 <= t < 1707912000):   # 2024-02-14 10:00–12:00 UTC
        return (price, price, price, price, 0.0)
    o = price
    c = o + rnd.randint(-40, 40)
    h = max(o, c) + rnd.randint(0, 15)
    l = min(o, c) - rnd.randint(0, 15)
    price = c
    return (o, c, l, h, round(rnd.uniform(5, 50), 2))

def write(path, start, n, step):
    rows = b""
    for i in range(n):
        o, c, l, h, v = bar(start + i * step, step)
        rows += struct.pack(">5if", i * step, o, c, l, h, v)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(lzma.compress(rows, format=lzma.FORMAT_ALONE))

for y, m in [(2024, 1), (2024, 2)]:
    s = int(dt.datetime(y, m, 1, tzinfo=dt.timezone.utc).timestamp())
    e = int(dt.datetime(y + (m == 12), m % 12 + 1, 1, tzinfo=dt.timezone.utc).timestamp())
    write(os.path.join(ROOT, str(y), "%02d" % (m - 1), "BID_candles_hour_1.bi5"), s, (e - s) // 3600, 3600)
for d in [1, 2, 3, 4, 5]:
    s = int(dt.datetime(2024, 3, d, tzinfo=dt.timezone.utc).timestamp())
    write(os.path.join(ROOT, "2024", "02", "%02d" % d, "BID_candles_min_1.bi5"), s, 1440, 60)
