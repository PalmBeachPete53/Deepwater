# Charts

Charts is a full charting terminal. It draws live candles from public sources on up to 16 charts at once, with 92 built-in indicators plus your own in JavaScript or Pine Script, drawing tools, and order flow for Binance markets. Around the charts you will find alerts, a screener, a strategy tester with quant checks, bar replay and a paper trading account.

## Page layout

### The areas of the page

The top bar runs across the page. From left to right it holds the symbol, the timeframes, the chart type, indicators, compare, layout, sync, replay and alert; on the right come the log scale, image, CSV, settings, the light / dark switch and the data status dot. When the window gets narrow, it first drops the button labels, which stay in the tooltips, then shrinks the two lists, and finally scrolls sideways.

The drawing bar on the left holds the drawing tools, followed by three buttons that act on the selected drawing. The centre shows one to 16 chart panes, each with its legend at the top left and its range navigator at the bottom.

Under the charts, the bottom panel groups seven tools in tabs: **Strategy tester**, **Screener**, **Heatmap**, **Seasonals**, **Financials**, **Paper trading** and **Indicator editor**. The arrow on its right, **Show / hide panel**, folds it down to its tab row. On the right, a 300 px side panel holds the **Watchlist**, **Alerts**, **Calendar** and **News**; it is hidden when the page is 900 px wide or less.

### Top bar buttons

Most buttons act on the active chart, the one you last clicked.

| Button | Action |
|---|---|
| Symbol name | Opens the symbol search (typing any letter or digit on the page does the same) |
| **1m 5m 15m 1h 4h 1D 1W** | Timeframe of the active chart |
| Field next to them | Any timeframe, then Enter: `1s`, `7m`, `2h`, `3D`, `2W`, `3M` (m = minutes, M = months) |
| **Chart type** | 16 types, see Chart types |
| Gear next to it | **Footprint settings** (only while the active chart is a footprint) |
| **Indicators** | Indicator picker |
| **Compare** | Adds another symbol to the active chart as a line |
| **Chart layout** | 1, 2 side by side, 2 stacked, 3, 4, 6, 8 or 16 charts |
| **Sync** | Changing the symbol changes it on every chart |
| **Replay** | Starts bar replay (then click the start bar), or stops it |
| **Alert** | Opens **Create alert** for the active chart |
| **Log** | Logarithmic price scale for the active chart |
| Camera | Saves a PNG of the active chart, titled with symbol and timeframe |
| Download | Saves the active chart's bars and every indicator value as CSV |
| Gear | **Settings** (general and chart style) |
| Moon | Light / dark rendering of this tab |
| Dot | **Data status**: green when every source answers; its tooltip names the failing ones (fallbacks are then in use). Checked every minute |

## Layouts and charts

### Layouts and the active chart

**Chart layout** arranges 1 to 16 charts; with three, a large chart sits on the left and two stacked charts on the right. A new chart copies the first chart's symbol, starts without indicators and takes its own timeframe, in this order: 1D, 4h, 1h, 15m, 1W, 5m, 30m, 2h. Charts removed by a smaller layout keep their settings, so choosing a larger layout again brings them back as they were.

Clicking a chart makes it the active one, outlined when there are several. The top bar, the drawing bar, the tester, the paper ticket and the alert dialog all act on it. Each chart keeps its own symbol, timeframe, type, indicators, compared symbols, log scale and footprint settings.

### Legend and compared symbols

The legend at the top left of each chart tells you what you are looking at. Its first line reads ticker · timeframe · source, followed by O, H, L, C and the change of the bar under the crosshair, or of the last bar when the crosshair is elsewhere. Clicking the name opens the symbol search.

Each indicator then gets a line with its name, its settings and its values at the crosshair, and three buttons: **Settings** (gear), **Show/hide** (eye) and **Remove**. A hidden indicator is greyed, one that is still computing shows "loading…", and errors show in red. Notes in orange explain which source was used and why (a fallback, delayed data, missing volume), and how far back the footprint goes.

**Compare** draws another symbol on the active chart as a coloured line. The price scale then switches to percent change, so that the series share one scale. Each compared symbol has its own legend line, whose **Remove** button takes it off.

### Range navigator

Under each chart, a 36 px strip shows a mini curve of the closes over the whole loaded history, with a window marking the part on screen. Dragging inside the window scrolls at the same zoom. Pressing outside it centres the window there, and you can keep dragging to scroll.

Dragging one of the window's handles zooms horizontally; the narrowest window, 30 px, is the deepest zoom, 5 bars. The curve itself never zooms: only the window changes.

### Countdown to bar close

Under the last price label, a countdown gives the time left before the forming bar closes, as `mm:ss`, `hh:mm:ss`, or days and hours. It appears for crypto on every timeframe, for forex on intraday timeframes, and for other markets up to 30-minute bars. It is not shown in replay, nor on Renko, line break, Kagi, point & figure or range bars. If you would rather not see it, turn off **Countdown to bar close** in the settings.

## Symbols and data

### Finding a symbol

To change symbol, click its name in the top bar or simply start typing a letter or a digit anywhere on the page. Type a name or a ticker (BTC, AAPL, EURUSD, ^FTSE, VOD.L…) and the results show ticker, name, type and exchange. ↑ and ↓ move in the list and Enter opens the selection; when nothing matches, Enter opens the text exactly as you typed it. **Compare** uses the same search.

### Symbol forms and sources

A symbol is written as a prefix and a ticker, and the prefix decides where the bars come from. When a source fails, Deepwater moves on to the next one in the list, and the legend tells you which one it ended up using.

| Form | Market | Sources, in order |
|---|---|---|
| `BINANCE:BTCUSDT` | Crypto spot | Binance, then Coinbase and Kraken (USD pairs), then Binance.US |
| `BINANCE:BTCUSDT.P` | Binance USD-M perpetual | Binance futures, then Bybit |
| `COINBASE:BTC-USD`, `KRAKEN:…` | That exchange | That exchange (Kraken: last 720 bars) |
| `US:AAPL` | US stocks and ETFs | Daily: Nasdaq, then Yahoo; intraday: Yahoo, then Nasdaq (today only, no volume) |
| `FX:EURUSD` | Forex | Yahoo, then Kraken |
| `FX:XAUUSD`, `FX:XAGUSD` | Gold, silver | Yahoo; gold then follows PAXG on Binance or Coinbase |
| `CME:ES1!` (ES, MES, NQ, MNQ) | Index futures | Yahoo, delayed about 10 minutes |
| `YAHOO:^FTSE`, `YAHOO:GC=F` | Global indices, futures, non-US stocks | Yahoo |
| `CSV:NAME` | Your imported bars | Your file (see Strategy tester) |
| `BTCUSDT/ETHUSDT` | Spread | Both symbols' bars at matching times; operators `/`, `*`, ` + `, ` - ` (one per spread) |

If you have your own Alpaca, Polygon or OANDA keys, Deepwater uses them before the public sources, which then serve as the fallback; `OANDA:` symbols need an OANDA token. They go in a file named feeds.json in the charts folder of Deepwater's data folder, and a key you add there is picked up without restarting.

### Loading and live updates

Each chart loads 3,000 bars by default, and **Bars to load per chart** in the settings takes it anywhere from 200 to 20,000. Binance charts then update through Binance's live stream, while other charts ask for new bars every 3 s on crypto exchanges and every 10 s elsewhere. A failed load is retried after 15 s, then 30 s, and so on up to once every 5 minutes.

While the tab is hidden, live updates stop. When you come back, a bar that closed in the meantime triggers a reload.

## Chart types and style

### Chart types

**Chart type** offers 16 ways to draw the bars.

| Type | What it draws |
|---|---|
| **Candles**, **Hollow candles**, **Bars** | OHLC bars |
| **Heikin Ashi** | Averaged candles |
| **Line**, **Line with markers**, **Step line**, **Area**, **Baseline**, **Columns** | Closes (baseline: above / below the close 150 bars back) |
| **Renko**, **Line break**, **Range bars**, **Kagi**, **Point & figure** | Price-only charts: box, range and reversal from ATR 14; line break uses 3 lines; P&F reverses after 3 boxes |
| **Footprint** | Order flow inside each candle (see Order flow) |

Volume shows at the bottom of the price pane, except on Renko, line break, Kagi and point & figure. Indicators that colour bars, such as Pine's `barcolor` or RSI Candle Colours, recolour only candles, hollow candles and bars.

### Settings

The gear on the right of the top bar opens **Settings**, which has two parts: general settings and chart style. The general ones apply to every chart.

| Field | Choices |
|---|---|
| **Bars to load per chart** | 200 to 20,000 (default 3,000) |
| **Time axis** | **My local time** or **UTC** (intraday timeframes) |
| **Volume on the price chart** | **Show** / **Hide** |
| **After placing a drawing** | **Back to cursor** / **Keep the tool** |
| **Crosshair across charts** | **Independent** / **Synced** (same time on every chart) |
| **Countdown to bar close** | **Show** / **Hide** |

### Chart style

Chart style changes the look of the candles and of the canvas behind them. You can set the candle body colours for up and down bars, turn the candle borders and wicks on or off and give them their own colours, and choose the background, the vertical and horizontal grid lines (each on or off, with a colour), the crosshair, and the colour and size of the scale text, from 9 to 16 (default 11).

Colours accept transparency, and borders and wicks follow the body colours until you set them. Volume bars follow recoloured candles at 45 % opacity. Only what differs from the theme is saved, so the light / dark switch and Deepwater's themes still recolour everything else. **Save** applies your changes, and **Reset chart style** goes back to the theme's colours and size.

## Indicators

### Adding and setting indicators

**Indicators** opens the picker, where indicators are grouped by family and can be searched by name in English or French. Each entry says whether it draws **on price** or in its **own pane**, and there is no limit per chart. **New custom indicator** opens the **Indicator editor** to write your own. Anchored indicators such as Anchored VWAP ask you to click the anchor bar once added.

The gear on an indicator's legend line opens its settings: the inputs, the **Timeframe** (**Same as chart**, 5m, 15m, 1h, 4h, 1D, 1W, 1M) and the **Colours** of each output. **Defaults** resets inputs and colours, and **Apply** saves. A higher timeframe shows its value only once each of its bars has closed, so it never looks ahead; a timeframe below the chart's is refused.

A **source** input can be a price (close, open, high, low, hl2, hlc3, ohlc4, hlcc4) or any output of another indicator on the chart, which gives you an indicator on an indicator, drawn in that indicator's pane. Time inputs have **Pick on chart**, so you can choose the bar with a click. Fixed levels such as RSI 30 and 70 are drawn as dashed lines. To resize panes, drag the separator between them; their heights are kept.

The tables below list the 92 built-in indicators by family, as the picker groups them.

### Moving averages

| Indicator | Meaning |
|---|---|
| Moving Average (SMA) | Plain average of the last N prices |
| Exponential Moving Average (EMA) | Average weighting recent prices more |
| Weighted Moving Average (WMA) | Linearly weighted average |
| Volume Weighted Moving Average (VWMA) | Average weighted by volume |
| Hull Moving Average (HMA) | Fast, smooth average with little lag |
| Double EMA (DEMA) | EMA with reduced lag |
| Triple EMA (TEMA) | EMA with still less lag |
| Smoothed Moving Average (SMMA / RMA) | Wilder's smoothing |
| Arnaud Legoux Moving Average (ALMA) | Gaussian-weighted average (offset, sigma) |
| Least Squares Moving Average (LSMA) | End point of a linear regression |
| McGinley Dynamic | Average that adapts its speed to the market |
| MA Cross | Fast and slow averages with cross markers |
| Moving Average Ribbon | MA 20, 50, 100 and 200 |
| Guppy Multiple Moving Average | Two groups of short and long EMAs |

### Volume

| Indicator | Meaning |
|---|---|
| Volume | Volume histogram with its average |
| Volume Weighted Average Price (VWAP) | Average price weighted by volume, reset each day, week, month or year |
| Anchored VWAP | VWAP from a bar you pick, with two standard-deviation bands |
| Time Weighted Average Price (TWAP) | Plain average of the period's prices |
| Money Flow Index (MFI) | RSI weighted by volume |
| On Balance Volume (OBV) | Running total of up-day minus down-day volume |
| Chaikin Money Flow | Buying vs selling pressure over N bars |
| Accumulation/Distribution | Running total of volume weighted by close location |
| Chaikin Oscillator | Momentum of accumulation/distribution |
| Price Volume Trend | Volume weighted by percent price change, summed |
| Ease of Movement | Price move per unit of volume |
| Elder Force Index | Price change × volume |
| Klinger Oscillator | Volume-based trend oscillator with signal |
| Volume Oscillator | Gap between a fast and a slow volume average |
| Net Volume | Volume signed by the bar's direction |
| Volume Profile, visible range (VPVR) | Volume by price over the bars on screen, with POC and value area |
| Session Volume Profile (SVP) | Volume by price for each day, week or month |

### Volatility

| Indicator | Meaning |
|---|---|
| Bollinger Bands | Average ± N standard deviations |
| Bollinger Bands %B | Position of the price inside the bands |
| Bollinger BandWidth | Width of the bands |
| BBTrend | Short vs long Bollinger bands: trend direction and strength |
| Keltner Channels | Average ± N ATR |
| Donchian Channels | Highest high and lowest low over N bars |
| Envelope | Average ± a fixed percent |
| Average True Range (ATR) | Average bar range including gaps |
| Standard Deviation | Dispersion of prices |
| Historical Volatility | Annualised standard deviation of returns |
| Relative Volatility Index | RSI computed on standard deviation |

### Momentum

| Indicator | Meaning |
|---|---|
| Relative Strength Index (RSI) | Gains vs losses, 0 to 100 |
| RSI Candle Colours | Colours candles by RSI zone |
| Connors RSI | RSI, streak RSI and rank of the change, combined |
| Stochastic | Close within the recent range (%K, %D) |
| Stochastic RSI | Stochastic applied to RSI |
| MACD | Gap between two EMAs, signal and histogram |
| Commodity Channel Index (CCI) | Distance from the average in mean deviations |
| Williams %R | Close within the recent range, 0 to −100 |
| Fisher Transform | Price turned into a near-normal oscillator |
| Rate Of Change (ROC) | Percent change over N bars |
| Momentum | Price change over N bars |
| Chande Momentum Oscillator | Up minus down moves over their sum |
| Rank Correlation Index | Rank correlation of price with time |
| TRIX | Rate of change of a triple EMA |
| Awesome Oscillator | 5 vs 34 average of the bar midpoints |
| Ultimate Oscillator | Buying pressure over three windows |
| True Strength Index | Double-smoothed momentum |
| SMI Ergodic Indicator/Oscillator | TSI with signal and oscillator |
| Price Oscillator (PPO) | MACD in percent |
| Coppock Curve | Smoothed sum of two long ROCs |
| Know Sure Thing | Weighted sum of four smoothed ROCs |
| Detrended Price Oscillator | Price minus a shifted average: cycles |
| Elder Ray (Bull/Bear Power) | High and low against an EMA |
| Balance of Power | Close minus open over the range |
| Relative Vigor Index | Close-open vs high-low, smoothed |

### Trend

| Indicator | Meaning |
|---|---|
| Directional Movement (DMI / ADX) | +DI, −DI and trend strength |
| Average Directional Index (ADX) | Trend strength only |
| Aroon | Bars since the highest high and the lowest low |
| Vortex Indicator | Upward vs downward movement |
| Choppiness Index | Trending (low) vs choppy (high) |
| Chop Zone | Colour by the angle of the 34 EMA |
| Trend Strength Index | Correlation of price with time, −1 to +1 |
| Mass Index | Range expansion that precedes reversals |
| Parabolic SAR | Trailing stop dots that flip on reversal |
| Supertrend | ATR band that follows the trend |
| Volatility Stop | ATR trailing stop |
| Chande Kroll Stop | Long and short ATR stops |
| Ichimoku Cloud | Conversion, base, leading spans (cloud), lagging span |
| Williams Alligator | Jaw, teeth and lips, three shifted averages |
| Pivot Points Standard | P, R1–R3, S1–S3 of the previous day, week, month or year |
| Previous Day High/Low | Previous day's, week's or month's high and low |
| Pivot Points High Low | Swing highs and lows marked on the chart |
| Zig Zag | Swings larger than a percent (chart only: it uses later bars) |

### Patterns, quant and order flow

| Indicator | Meaning |
|---|---|
| Candlestick Patterns | Marks classic candle patterns |
| Williams Fractals | Marks fractal highs and lows |
| Markov Regime (bull, bear, sideways) | Regime from the trailing return; odds of the next regime from past transitions |
| Liquidity heatmap | Binance order book depth over time (see Order flow) |
| Liquidation heatmap | Estimated liquidation levels (see Order flow) |
| Liquidity levels (SMC) | Buy-side and sell-side liquidity, sweeps (see Order flow) |
| Big Trades | Large aggressive orders as bubbles (see Order flow) |

## Drawings

### Drawing tools

Each tool takes one to three clicks on the price pane.

| Tool | Clicks | Notes |
|---|---|---|
| **Trend line**, **Ray**, **Extended line** | 2 | Segment, extended right, extended both ways |
| **Horizontal line**, **Horizontal ray**, **Vertical line** | 1 | |
| **Parallel channel** | 3 | |
| **Rectangle** | 2 | |
| **Fib retracement** | 2 | Levels 0, 0.236, 0.382, 0.5, 0.618, 0.786, 1, 1.618 |
| **Price range / measure** | 2 | Change, percent and number of bars |
| **Long position**, **Short position** | 2 | First click = entry, second = stop and width; target at 2 R; shows entry, stop, target and R:R |
| **Text note** | 1 | Asks for the text |

### Placing and managing drawings

Pick a tool in the drawing bar, then click on the price pane, the only place drawings live. Between clicks the shape follows the mouse, and panning and zooming are off while a tool is armed. Once the drawing is placed the cursor comes back, unless you chose **Keep the tool** in the settings.

With the cursor, click a drawing to select it. Delete or Backspace, or the bin button, removes it, and Esc deselects it. **Alert when price crosses the selected line** opens the alert dialog with that line already chosen; it works with trend lines, rays, extended lines, horizontal lines and rays, and a channel's first line. **Remove all drawings on this symbol** asks for confirmation first.

Drawings belong to the symbol rather than to a chart: every chart of that symbol shows them, and they come back whenever you open it again. Bear in mind that a drawing cannot be moved or recoloured once placed.

## Order flow

### Which markets

Most order flow tools read an exchange's own trades and order book, and today that means Binance. Footprint, Big Trades and the liquidity heatmap need a Binance chart, spot or USD-M perpetual (`.P`), loaded from Binance itself rather than a fallback source, and without compared symbols. Anywhere else, the legend says why nothing is drawn.

The liquidation heatmap needs a Binance crypto chart, since it reads Binance USD-M statistics. Liquidity levels (SMC), on the other hand, work on every chart, because they use the bars only.

Once the tab has been hidden for 30 s, the footprint, Big Trades and the heatmaps stop and close their connections, except a Big Trades with an alert and a heatmap that records. They restart when you come back.

### Footprint

The **Footprint** chart type opens up each candle to show the volume traded at each price, built from Binance trades. On loading, it fetches up to 300,000 recent trades, about 1 to 4 hours of BTCUSDT, then follows the live trades. Older bars stay plain candles, unless the market's footprint history is stored in [Data › Technical data](help:data) at 1 basis point levels. The legend says from when the footprint is complete, and a candle with a complete footprint takes its open, high, low, close and volume from its own trades.

How much you see depends on the zoom: numbers when the rows are tall enough, shading alone when they get narrower, and plain candles when they are too narrow. The gear next to **Chart type** opens **Footprint settings**.

| Footprint setting | Choices |
|---|---|
| **Display** | **Bid x Ask** (sells at the bid, buys at the ask), **Delta** (ask − bid), **Volume profile** |
| **Price rows** | **Auto (follows the zoom)** or 1 to 10,000 ticks per row |
| **Imbalance ratio** | Default 3: buyers at a price at least that many times the sellers one row below (and the reverse) |
| **Minimum volume for an imbalance** | Rows below it never count |
| **Stacked imbalances: rows in a row** | 2 to 8 (default 3), drawn as a strip right of the cells |
| **Point of control (POC)** | Outline of the row with the most volume |
| **Delta and volume under the candle** | Show / hide |
| **Colours: buyers, sellers, POC** | Theme colours until changed |

### Big Trades

Big Trades marks large aggressive orders as bubbles, placed at each order's average price and moment; the colour gives the side and the size gives the volume. Hovering a bubble shows its side, time, volume, average price, price range and executions, and the legend shows the current threshold.

To find those orders, Deepwater first groups trades together, as set by **Grouping, ms** and **Prices in one order** (the same price only, or a sweep over several levels). The **Threshold** then decides what counts as big. In **Auto**, it is a percentile of recent orders over a window in minutes, optionally adjusted to volatility, and it needs a warm-up first; in **Manual**, you give a minimum volume. Either way you can add a maximum.

The look settings choose the **Marker** (circle, square, diamond or text), the smallest and largest size, the smallest opacity, and whether large markers show their volume inside. **Alert** can be **Visual** (a message and a system notification), **Sound**, or both, for orders at least N times the display threshold (default 3). Alerts come at most once a second, and the others are counted in the next one.

### Liquidity heatmap

The liquidity heatmap draws the order book under the candles: the brighter a price, the more size rests there. The best bid and ask run as two lines, and hatching marks the moments the book was not synced. **Order book** picks the chart's own market, or the spot or USD-M perpetual book of the same pair.

On screen you get 100 ms columns for the last 10 minutes and 1 s columns for the last 6 hours, plus whatever earlier sessions recorded. **Record to disk (1 s)** is on by default, and **Days kept on disk** is 7.

To tune the look, you can set the price step of a cell, whether the prices sharing a cell show the largest size or their sum, the palette (Deep, Thermal, Mono), a log or linear colour scale, the auto contrast percentiles, quantity limits, the contrast curve and the opacity.

### Liquidation heatmap

The liquidation heatmap estimates where leveraged positions opened on these bars would be liquidated; brighter means more. It is built from Binance open interest, in 5-minute points, and from the long/short accounts ratio. Binance keeps 30 days of open interest and Deepwater keeps what it collects while it runs, so the legend says how many bars the estimate rests on.

**Positions** shows both sides, longs or shorts. **Leverage mix** can be standard (5–100×), high (25–125×), low (2–20×), or **Calibrated on real liquidations**, which needs 30 real liquidations within the bars. The remaining settings cover the typical position, longs vs shorts, candle tilt, the fade half-life, the share removed when the price reaches a level, price rows, palette, contrast and opacity. **Real liquidations (dots)** adds the liquidations Binance published, red for longs and green for shorts, sized by quantity.

### Liquidity levels (SMC)

This indicator marks buy-side liquidity above swing highs and sell-side liquidity below swing lows, stronger where highs or lows are equal. A level is swept when the price goes through it, and is then greyed for 20 bars. Levels are drawn as zones or lines, with labels; their opacity follows a score built from members, tests, age and timeframe, and **Minimum score shown** hides the weaker ones.

The swings come from **Pivots**: a window of bars on each side, a percent reversal or an ATR reversal. You also set the tolerance for equal highs and lows, the zone thickness (in ATR, percent or ticks), and whether a level is **Swept by** a wick or a close. **Detection timeframe** is the chart's own or anything from 15m to 1W, on closed bars only. On top of the swings, the indicator can draw the previous day and week levels, with the day starting at 00:00 UTC or 17:00 New York, and the Asia, London and New York session levels, each with an ICT variant.

## Alerts

### Creating an alert

You can create an alert from **Alert** in the top bar, from the alert button of the drawing bar with a line selected, or from **New alert** in the side panel. The dialog starts with the **Symbol**, what to **Check** (**Just this symbol** or **Every symbol on** a watchlist) and the **Timeframe**.

Then come up to five **Conditions**, each made of a left side, an operator and a right side. A side can be a value, a price field (open, high, low, close, volume), any indicator output with its own settings, or a drawn line. The operators are is above, is below, is at or above, is at or below, crosses above, crosses below and crosses. **Fire when** decides whether **all are true** or **any is true**, and **Trigger** whether the alert fires **Only once**, **Once per bar** or **Once per bar close**.

Finally, you can add a **Message (optional)** and, under **Also send to (optional)**, a Discord, Slack or Telegram webhook URL; **Test** sends a test message.

### When an alert fires

Alerts are checked every 15 seconds while the app runs, even when this tab is closed. A fired alert shows a message for 12 s, beeps and sends a system notification.

The **Alerts** tab of the side panel lists each alert with its state (**Active** or **Stopped**), its trigger, its webhook and the number of times it fired, along with **Stop** / **Restart** and **Delete**. Below the list, **Recent** shows the last 30 firings.

## Side panel

### Watchlist

The watchlist shows lists of symbols with their price and day change, refreshed every 5 s while the tab is shown. The default list, **Main**, holds BTC, ETH, SOL, AAPL, NVDA, SPY, QQQ and EUR/USD. **+ List** creates a new list and **Delete** removes the current one, though one must always remain.

To add a symbol to the current list, type it in **Add symbol (Enter)**; × removes it. Clicking a symbol opens it on the active chart.

### Calendar and news

The calendar has three views. **Economic** shows this week's releases with forecast and previous, filtered to **All**, **Medium + high** or **High impact**. **Earnings** and **Dividends** cover the next five trading days, and **My watchlists** keeps only your symbols.

**News** gives headlines for the active chart: crypto from Cointelegraph and Decrypt, US stocks from Nasdaq. Click a headline to open the article.

## Strategy tester

### Building a strategy

The tester runs a rule-based strategy on the bars of the active chart. The quickest start is **Load a strategy…**, which offers **Examples** (EMA cross 9/21, RSI mean reversion 30/55, Supertrend follow, MACD + 200 EMA trend filter, Bollinger breakout), your **Saved** strategies and the **Pine strategies**.

A strategy rests on four condition lists: **Buy (enter long) when**, **Sell (exit long) when**, **Enter short when** and **Exit short when**. Entry conditions must all be true, and a list left empty is skipped. Signals use the bar's close and fill at the next bar's open. The rest of the form shapes exits, risk and costs:

- **Exits**: **Exit when** all or any exit condition is true, and **Exit after this many bars**.
- **Risk**: **Stop loss %**, **Take profit %** and **Trailing stop %**.
- **Money**: **Starting capital**, then the position size, the fee and slippage per side and the spread from bid to ask, each in percent; **Live spread** reads the current bid and ask (crypto).
- **Fills inside a bar**, for stops and targets: stop first when a bar touches both, TradingView's path (open, nearer extreme, far extreme, close), or **Bar magnifier**, which uses the real lower-timeframe bars inside each bar.

**Save strategy** names it and keeps it, and **Edit as JSON** shows it in the same format AI agents write.

You can also bring your own data. **Import bars (CSV)** takes time, open, high, low, close and volume; the bars then chart as `CSV:NAME`, on their timeframe and longer ones. **Import signal (CSV)** takes time and position (1 long, 0 flat, −1 short, from that bar's close) and loads it as rules.

### Running a test

**Run on chart** tests the strategy on the active chart's symbol and timeframe. The bars are the chart's own, unless [Data › Technical data](help:data) stores that market and timeframe (BTCUSDT on Binance, 1m to 1W): the test then uses its newest 100,000 bars, or 20,000 for a Pine strategy. In replay, it only sees the chart's bars up to the replayed one.

The results open with the figures below, followed by an equity curve against buy and hold and the trade list (side, entry, exit, prices, P&L %, bars, exit reason).

| Result | What it tells you |
|---|---|
| **Net profit** | What the strategy made or lost over the test, in % of the starting capital |
| **Buy and hold** | What simply holding the market from the first to the last bar made, in % |
| **Max drawdown** | The deepest fall of equity from a peak, in % |
| **Trades** | Number of trades |
| **Win rate** | Share of trades that made money |
| **Profit factor** | Gross gains divided by gross losses |
| **Sharpe** | Average return per bar divided by its volatility, annualised |
| **Sortino** | The same, counting only the downside volatility |
| **Avg trade** | Average result of a trade, in % |
| **Time in market** | Share of the bars spent in a position, in % |

Entries and exits are marked on the chart; **Clear chart marks** removes them and **Export trades (CSV)** saves the list.

Every backtest, yours and the AI agents' alike, is recorded in the **Trial registry**. Its line gives the number of strategies tried on this market and timeframe, and the deflated Sharpe of the best one: the more versions you try, the more likely the best is luck, and a deflated Sharpe above 0.95 says its edge is likely real.

### Quant checks

A good backtest can still be luck or overfitting. **Run quant checks** re-runs the strategy about 25 times with nearby settings, on your computer, and grades it on five checks.

| Check | Passes when |
|---|---|
| Walk-forward | Sharpe above 0 on data the settings were never picked on |
| Overfitting (PBO) | Under 50 % chance the best-looking settings are below average on unseen data |
| Deflated Sharpe | Probability above 0.95 that the edge is real after the versions tried |
| Cost stress | Return above 0 with double fees, more slippage and double spread |
| Nearby settings | 60 % or more of nearby settings keep half the Sharpe |

Below the grades you also get the walk-forward table, a grid of net returns for pairs of settings with yours outlined, a Monte Carlo of the trades and the probabilistic Sharpe. Walk-forward, overfitting and nearby settings need a whole-number setting to vary, such as an indicator length, so they do not run on fixed rules or on an imported signal.

## Bottom panel tools

### Screener

The screener sweeps a whole market at once: **Crypto (USDT pairs)**, **US stocks** or **US ETFs**. You can narrow it with **Search**, price and change % ranges, **Min volume**, and for stocks **Min market cap** and **Sector**. The table shows symbol, name, price, change % and volume, plus market cap and sector for stocks, with 500 rows at most; click a header to sort, or a row to open its chart.

**Technical filter** adds one condition, checked **On** 1h, 4h, 1D or 1W. **Scan top 100** runs it on the 100 most traded rows, and **Clear scan** removes it.

### Heatmap, Seasonals and Financials

**Heatmap** draws tiles for the **US stocks (200 largest)** or **Crypto (100 most traded)**, sized by market cap or volume and coloured by today's change; a click opens the chart. **Seasonals** shows the active symbol's monthly moves in percent for each calendar year, from daily bars, over 3, 6, 10 or 20 years, with the average and the share of years up.

**Financials** covers US-listed stocks: income statement, balance sheet, cash flow, ratios and key stats, annual or quarterly, with a revenue / net income chart. The figures come from Nasdaq, in thousands of US dollars.

## Paper trading

### Placing orders

Paper trading lets you practise on live prices with no real money and no exchange connection; for real orders, see [Account](help:account). The ticket trades the active chart's symbol. Choose the **Type** (**Market**, **Limit** or **Stop**), the **Price** and the **Amount ($)**, fill in **Stop loss % (optional)** and **Take profit % (optional)** if you want them, then press **Buy** or **Sell / short**.

Market orders fill at the live price. Limit and stop orders fill when the live price reaches them, and a limit never fills worse than its price. A stop loss or take profit becomes an exit order once the entry fills, and when you set both they cancel each other, shown as **(bracket)**. Orders stay watched while other pages are shown, and a fill there sends a system notification.

### The account

The account shows **Equity**, **Cash** and **Total P&L**, then **Positions** with **Close**, **Working orders** with **Cancel**, and **History** with the last 100 fills. **Starting balance** defaults to 100,000 and is applied by **Reset account**; **Fee % per trade** defaults to 0.05.

## Bar replay

Bar replay takes a chart back in time, so you can practise or test as if the market were unfolding again. Press **Replay** in the top bar, then click the bar to start from: the later bars disappear.

The replay bar steps back, plays or pauses, and steps forward, at a **Speed** from 0.5x to 10x (default 2x). **Jump to…** picks a new start bar, and **Exit** returns to live, reloading if a bar closed meanwhile. When there are several charts, **All charts** replays every chart to the same moment, and the others show **Synced replay**.

During replay, indicators, higher-timeframe values, order flow layers, the tester and the paper ticket only see bars up to the replayed one. The ticket also switches to a separate practice account: orders fill at the replayed bars' prices, with limit and stop orders filling as the replay advances. Exiting replay brings back the live paper account.

## Pine Script and custom indicators

### Indicator editor

The **Indicator editor** in the bottom panel is where you write your own indicators. Start with **New indicator**, which opens a JavaScript template, or with **New Pine Script**; give it a name and code, then choose **Save** or **Save and add to chart**. Each file is checked before it is saved, and errors show under the code.

**Your indicators** lists what you have written, with **Edit**, **Add to chart** and **Delete**. Custom indicators also appear in the picker under **Custom**, and an example, EMA ribbon squeeze, is installed the first time you use the editor. AI agents can write indicators for you too (see [AI](help:ai)).

### Pine Script support

You can paste a TradingView script, version 5 or 6: it runs in the background inside Deepwater, through PineTS. Its inputs become the indicator's settings. The first source input becomes **source**, while colour, timeframe, symbol, session and text inputs keep their defaults.

The script's `plot`, `plotshape`, `plotchar`, `plotarrow` and `barcolor` calls are drawn, along with levels and per-bar colours; `bgcolor`, `fill`, labels, lines, boxes, tables and polylines are not drawn yet. A script that runs too long or uses too much memory is stopped, with a message in the legend, and a Pine library draws nothing.

A `strategy()` script appears under **Pine strategies** in the tester. Its own entries, exits, sizing and costs apply there, filled the way TradingView fills orders, and you change its inputs in the editor. Quant checks vary its first two whole-number inputs.

## Shortcuts and mouse

### Keyboard

These keys work on the Charts page; the shortcuts shared by the whole application are in [Deepwater](help:start).

| Key | Action |
|---|---|
| Any letter, digit or ^ | Symbol search starting with that character |
| ↑ / ↓, Enter | Move in the search results, open |
| Enter (timeframe field) | Apply the typed timeframe |
| Esc | Close the dialog, else back to the cursor, else deselect the drawing |
| Delete / Backspace | Delete the selected drawing |

### Mouse

| Gesture | Action |
|---|---|
| Wheel (vertical) | Zoom around the pointer |
| Horizontal wheel, Shift + wheel | Scroll in time |
| Drag on the chart | Scroll in time and price |
| Drag the price scale | Stretch prices |
| Drag the time scale | Stretch time |
| Double-click the price scale | Auto-fit prices again |
| Double-click the time scale | Reset the time scale |
| Drag between panes | Resize indicator panes |

## Saving

Everything on this page is saved on your computer, in the charts folder of Deepwater's data folder, a moment after each change and when you leave the tab. That covers the layout, the active chart, each chart's settings, the drawings of each symbol, the watchlists, the settings and chart style, your saved strategies and the paper accounts. Alerts, custom indicators, imported CSV files and heatmap recordings are kept in the same folder as files.
