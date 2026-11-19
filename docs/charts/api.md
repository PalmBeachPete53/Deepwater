# Local API: drive the Charts tab from your own code

Port of quant-terminal's docs/api.md (MIT, Copyright (c) 2026 Lewis Jackson, see NOTICE.md). Deepwater serves the same
local HTTP API as Quant Terminal, with the same routes and JSON, for as long as the app is running, so your scripts can
use it: pull bars and quotes, run backtests with the quant checks, and set alerts that call your webhook. Quant
Terminal's working examples in Python and Rust (`quant-terminal/examples/python/quant_terminal.py`,
`quant-terminal/examples/rust`) work against Deepwater unchanged; "Quant Terminal must be running (npm start)" reads
"Deepwater must be running".

- **Address:** `http://127.0.0.1:3777`, or the next free port up to 3797 when 3777 is taken (another copy of Deepwater,
  or Quant Terminal itself). Setting the environment variable `PORT=4000` before starting Deepwater picks the first
  port to try (a value that isn't a port number, 1 to 65535, is logged and 3777 used). The examples take another
  address from `QT_URL`. A second copy of Deepwater on the same data folder serves the API but leaves the alert
  checking to the first one, so an alert never fires twice. Both work on the same `alerts.json`: a change made
  through either one (or by hand in the file) is seen by the other, and neither writes over the other's changes.
- **On or off:** the setting `charts.api` (default on, like Quant Terminal, which always serves). Off: no port is opened;
  alerts still run. `DW_CHARTS_API=0` in the environment turns the API and the alert checker off for one run.
- **This computer only.** The server listens on 127.0.0.1 and answers requests addressed to `localhost`, `127.0.0.1` or
  `[::1]`; anything else gets `403`.
- **Writes need one header.** Every `POST`, `PUT`, `PATCH` and `DELETE` carries `x-qt: 1`, and if it carries an
  `Origin` it must be this same host. A web page in your browser can't add that header to a cross-site request, so
  other pages can't drive your terminal. Reads need nothing. No CORS headers are sent.
- **JSON in and out.** Errors come back as `{ "error": "..." }` with a 4xx or 5xx status: `400` for a mistake in the
  request, `404` for an unknown route or alert, `413` for a body over the limit (2 MB of JSON, 20 MB for `/api/state`,
  300 MB for a bar CSV, 100 MB for a signal CSV; answered from the `Content-Length` before the body is read, or at the
  chunk that goes over for a `Transfer-Encoding: chunked` body, and a body refused by the two rules above isn't read
  either; a route that takes no body, like the `GET`s or `DELETE`, ignores one of any size and answers as usual),
  `422` when there aren't enough bars, `502` when the market data source
  failed. Keys of JSON objects come back in alphabetical order (Quant Terminal keeps insertion order; JSON parsers
  don't care).
- **Times** are Unix seconds (UTC). Symbols are `EXCHANGE:TICKER` like the app's search (`BINANCE:BTCUSDT`, `US:AAPL`,
  `FX:EURUSD`, `CME:NQ1!`), and plain tickers (`btc`, `aapl`) are resolved for you.
- **Timeframes:** `1m`, `5m`, `15m`, `1h`, `4h`, `1D`, `1W`, `1M`, and custom ones like `7m`, `2h`, `3D`.
- **Your files** are in Deepwater's data folder, `charts/` (macOS: `~/Library/Application Support/deepwater-terminal/
  Deepwater/charts/`; Windows: `%APPDATA%\deepwater-terminal\Deepwater\charts\`): `state.json`, `alerts.json`,
  `alert-log.json`, `feeds.json`, `imports/`, `custom/` — the same files and shapes as Quant Terminal's `data/` folder.

The API reads data and runs research. Placing real orders isn't part of it. Quant Terminal's PTQ routes (`/api/ptq/*`)
are not in Deepwater.

## Market data

| Request | Returns |
|---|---|
| `GET /api/health` | `{ ok, app: "quant-terminal", server: "Deepwater", version, deepwater, providers, feeds, yahoo, alerts, dataDir, root, platform, qt }`: is it running, which sources answer, which of your own keys are set. `version` is the Quant Terminal release the API follows (1.0.0), `deepwater` the app's version |
| `GET /api/probe` | Asks every source now: `{ name: { ok, ms, error } }` |
| `GET /api/resolve?symbol=btc` | `{ id, exchange, ticker }`: what a typed symbol means |
| `GET /api/search?q=apple` | Up to 30 `{ symbol, name, type, exchange }` matches |
| `GET /api/candles?symbol=BTCUSDT&tf=1h&limit=500` | `{ symbol, tf, provider, candles: [{ time, open, high, low, close, volume }], notes }`, oldest first. `limit` is 10 to 20000 (default 1500) |
| `GET /api/quotes?symbols=BTCUSDT,AAPL` | `{ "BINANCE:BTCUSDT": { price, changePct, volume }, ... }`, up to 200 symbols |
| `GET /api/spread?symbol=BTCUSDT` | `{ symbol, bid, ask, spreadPct, source }` (crypto exchanges only) |
| `GET /api/screener?universe=crypto` | Screener rows for `crypto`, `stocks` or `etf` |
| `GET /api/calendar` | This week's economic events |
| `GET /api/earnings?days=5`, `GET /api/dividends?days=5` | US earnings and dividends for the next trading days |
| `GET /api/news?symbol=AAPL` | `{ items, note }`: headlines |
| `GET /api/indicators` | Every indicator id with its inputs (key, type, default) and output keys |
| `GET /api/financials?symbol=AAPL&frequency=annual` | Income statement, balance sheet, cash flow and ratios |

`provider` and `notes` say where the bars came from (and whether they're delayed), the same text the chart shows.

## Backtests

`POST /api/backtest` runs the strategy tester inside Deepwater (on a worker thread; on the app's main thread when the
strategy uses one of your custom indicators). The strategy is the same JSON the tester's "Edit as JSON" shows:

```json
{
  "symbol": "BTCUSDT",
  "tf": "1D",
  "limit": 1500,
  "strategy": {
    "indicators": { "fast": { "id": "ema", "params": { "length": 9 } }, "slow": { "id": "ema", "params": { "length": 21 } } },
    "entryLong": [{ "left": "fast", "op": "crossesAbove", "right": "slow" }],
    "exitLong": [{ "left": "fast", "op": "crossesBelow", "right": "slow" }],
    "feePct": 0.1, "slippagePct": 0.05
  },
  "quantChecks": true,
  "equity": false
}
```

It returns `{ symbol, tf, provider, bars, from, to, stats, trades }`:

- `stats`: `netProfitPct`, `buyHoldPct`, `maxDrawdownPct`, `totalTrades`, `winRatePct`, `profitFactor`, `sharpe`, `sortino`, `avgTradePct`, `exposurePct` and more.
- `trades`: `[{ side, entryTime, exitTime, entryPrice, exitPrice, pnlPct, bars, entryReason, exitReason }]`.
- `equity` (when you send `"equity": true`): `[{ time, value }]` per bar.
- `quantChecks` (when you send `"quantChecks": true`): `{ passed, total, gates: [{ label, value, pass }], walkForward, monteCarlo }`. The same walk-forward, overfitting, deflated Sharpe, cost stress, nearby settings and Monte Carlo checks as the tester's gold box.

`POST /api/scan` checks rules on the latest bar of many symbols at once (the first 150, four at a time):
`{ symbols: [...], tf, indicators, rules: [{ left, op, right }] }` returns `[{ symbol, pass, passes, close }]`, or
`{ symbol, error }` for a symbol that couldn't be checked.

Indicator ids are the built-in ones (`ema`, `rsi`, `macd`, `bb`...) and your own (`custom:<file name>`). `GET /api/indicators` lists every one with its inputs and outputs.

## Your own indicators

| Request | Does |
|---|---|
| `GET /api/custom` | Your indicator files: `[{ file, id, mtime, error }]` (`error` is `null` when the file loaded) |
| `POST /api/custom` | Body `{ name, code }`. Saves `custom/<name as a slug>.js` and loads it: `{ ok, file, id }`, or `400 { error, file }` when it doesn't follow the format (the file you had before is kept). CommonJS `module.exports =` is taken as `export default`; Pine Script is refused with a note |
| `DELETE /api/custom/<file>` | Deletes the file |
| `GET /custom/<file>` | The file's source |

The format is in [`custom-indicators.md`](custom-indicators.md) (from quant-terminal's docs). Deepwater runs the code in its own JavaScript engine
(QuickJS-ng), so a few error messages are worded differently from Node's.

## Your own data files

| Request | Does |
|---|---|
| `POST /api/import/bars?name=NAME` | Body: an OHLCV CSV. Saves it and returns `{ name, symbol: "CSV:NAME", bars, tf, from, to, skipped }`. Chart or backtest it as `CSV:NAME` on its own timeframe or longer |
| `POST /api/import/signal?name=NAME` | Body: a `time, position` CSV (1, 0, -1). Returns `{ ok, name, id, file, rows, changes, from, to, strategy }`: send that `strategy` to `/api/backtest` with the bars it was made on |
| `GET /api/imports` | Your imported bar files |

The column and time formats are in quant-terminal's README ("Your own data files").

## Layout and watchlists

| Request | Does |
|---|---|
| `GET /api/state` | `state.json`: layouts, watchlists, drawings... as the Charts tab saves them |
| `PUT /api/state` | Merges the top-level keys you send into `state.json` (`{ ...state, ...body }`) and returns `{ ok: true }`. The Charts tab picks the change up at once |

## Alerts and webhooks

Alerts run inside Deepwater every 15 seconds while it is running, whether the Charts tab is open or not.

| Request | Does |
|---|---|
| `GET /api/alerts` | Lists alerts |
| `POST /api/alerts` | Adds one (body below) and returns it with its `id` |
| `PATCH /api/alerts/<id>` | Changes fields, e.g. `{ "active": false }` |
| `DELETE /api/alerts/<id>` | Deletes it |
| `GET /api/alerts/log` | The last 500 times an alert fired |
| `POST /api/alerts/check` | Checks every alert now instead of waiting (answers when the check is done) |
| `GET /api/events` | A live stream (server-sent events, `retry: 3000`, a `: keep-alive` comment every 25 s) with an `alert` event each time one fires |
| `POST /api/webhook-test` | Body `{ url }`: posts a test message there and returns `{ ok, status }` |

```json
{
  "symbol": "BINANCE:BTCUSDT", "tf": "1h",
  "conditions": [{ "left": "close", "op": "crossesAbove", "right": 100000 }],
  "match": "all",
  "indicators": {},
  "trigger": "oncePerBarClose",
  "webhook": "https://your-server.example/hook",
  "message": "BTC closed above 100k"
}
```

- `op`: `>`, `<`, `>=`, `<=`, `crossesAbove`, `crossesBelow`, `crosses`. Up to 5 conditions; `match` is `all` or `any`.
  An operand is a number, a price field, an indicator key, an indicator output (`macd.signal`) or a drawn line
  `{ "line": { "t1", "p1", "t2", "p2", "extend" } }`.
- `trigger`: `once` (then it switches off), `oncePerBar` or `oncePerBarClose`.
- `watchlist`: send the name of one of your watchlists instead of `symbol` and the alert checks every symbol on it. `once` then means once per symbol, and each event's `symbol` says which one fired.
- `webhook`: when it fires, Deepwater POSTs JSON to this URL. Discord, Slack and Telegram URLs get their own message format; any other URL gets `{ text, content, alert, event }`, where `event` is `{ id, alertId, symbol, tf, text, price, barTime, at }`. That's the hook for your own code: point it at a small server of yours and act on the alert there. The host you give is listed in Deepwater's Sources tab under "Your own webhook URLs".

## Not in Deepwater

- `POST /api/shutdown` answers `403 { error }`: Quant Terminal's installer used it to stop a running copy; Deepwater is
  an app you quit from its window.
- The web page (`/`, `/index.html`, the scripts and styles): the Charts tab is the interface. `/` answers a short JSON
  note saying the API is served by Deepwater.
- `/api/ptq/*` (PTQ connector).
