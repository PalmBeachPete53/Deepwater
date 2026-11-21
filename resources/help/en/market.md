# Market

The Market tab follows prices and reads them. It holds a live board of 127 markets with a full sheet for each instrument, multi-year seasonality, a fundamental score of the eight major currencies, a technical scan of every market and a map of the liquidity pools of one instrument. Every score and signal is computed by Deepwater from public data; the rules are given below and in the pages' tooltips.

## Markets Watch

Markets Watch is the live board: every market of the catalogue on one screen, grouped by category, with its last price and its change. The **MARKETS** title gives the number of markets shown and the poll rate (**QUOTES REFRESH EVERY 20S**). Below it come a search field and the category pills, from **ALL** and **FOREX** to **BONDS**, each with its count of markets; the counts follow the search.

| Category | Markets |
|---|---|
| Forex | 42 pairs (majors, crosses, exotics) |
| Indices | 23 (S&P 500, Nasdaq 100, Dow, DAX, FTSE, VIX, Nikkei, Hang Seng, DXY…) |
| Commodities | 18 (metals, energy, agriculture) |
| Stocks | 28 US large caps |
| Crypto | 12 |
| Bonds | 4 US yields (13-week, 5-, 10-, 30-year) |

### Reading the board

Four cards sum up the markets shown, once the category and the search are applied. **BREADTH** counts the markets up, down and unchanged, a change that rounds to 0.00% counting as unchanged; its tooltip gives the three counts. **AVERAGE MOVE** is the mean change, and **LEADER** and **LAGGARD** name the markets with the best and the worst change.

The **CHANGE OVER** selector sets the period: **1D**, **1W** or **1M**, and the cards then say **TODAY**, **1 WEEK** or **1 MONTH**. The **ORDER** selector sorts the rows: **DEFAULT** puts the majors first, then the crosses and the rest; **PERFORMANCE** puts the best change first and the markets without a change last; **A-Z** is alphabetical.

Each category has its own panel, with its title, its number of markets and the **avg** change of its rows, then the columns **MARKET**, **LAST** and the change. Stocks show the company name under the ticker. The panels spread over 3 to 8 columns depending on the width of the window, and when you select a single category its panel fills the width.

### How changes are computed

The 1D change is Yahoo Finance's own, against the previous close. The two longer periods are measured by Deepwater against a past close:

1W = last price ÷ close of the last session on or before the date 7 days before the quote − 1

1M follows the same rule with the last session on or before the date one month earlier. Hovering a 1W or 1M change shows the reference close and its date; hovering anywhere else on the row gives the market's full name.

### Searching and opening a market

Press `/` to put the cursor in the search field, whatever your keyboard layout; Esc clears it and Enter opens the first match. The search looks at the label (EUR/USD), the name (Gold, Nvidia), the symbol and the Yahoo ticker. When it finds nothing in a panel, that panel says **No match**.

Clicking a row opens the instrument sheet described below. The list scrolls smoothly with the mouse wheel or the trackpad, and the category, the change period and the order you chose are remembered.

### The instrument sheet

The sheet opens over the list and gathers what Deepwater knows about one market. Its header names the market, and the company for a stock; × or Esc closes it and takes you back to the list. Each card loads on its own and, when it has nothing to show, says why.

The price chart sits at the top left (see the next section). Next to it, **Retail Position** gives the share of Myfxbook community traders who are long and short. It needs a Myfxbook login, entered under Positioning › Retail Sentiment or in the [Sources](help:sources) tab; without one the card reads **DATA SOURCE NOT CONNECTED.**, and markets Myfxbook does not cover say so. Its tooltip gives the source and the time of the snapshot.

**COT Position** shows how the large speculators (non-commercials) of the CFTC legacy report are placed in the latest week, as long ÷ (long + short). A currency pair shows one pie per currency future; other markets use their own future (S&P 500 → E-mini, DXY → US dollar index…). The tooltip gives the contracts long and short, the weekly change and the report date, and markets without a CFTC contract say so.

**Data Compare** sets side by side the interest rate, quarterly GDP growth, annual inflation, the unemployment rate and the OECD business confidence (100 = its long-run average), either for the two currencies of a pair or for the home economy of any other market. The figures come from the [Economy](help:economy) services, and hovering a value gives its period and source. The six **Technical Heatmap** tiles (trend, momentum, volatility, volume, structure, liquidity) are the same reads as on the Technical Heatmap page, from 2 years of daily candles; their tooltips give the numbers behind each tile.

**Index Performance** follows two lines since January 2017, both rebased to 0% on their first common day; hovering gives the date and both values. For a pair of two major currencies, each line is an index of one currency: its mean log change against the other majors. Any other market is set against a benchmark:

| Market | Benchmark |
|---|---|
| Stock | S&P 500 |
| Index | S&P 500 (Nasdaq 100 for the S&P 500 itself) |
| Crypto | Bitcoin (Nasdaq 100 for Bitcoin) |
| Commodity | Gold (DXY for gold) |
| Other pair | DXY |
| Bond | S&P 500 |

**Meeting Compare** gives, for each central bank, the odds the rate futures price for its next decision, in five buckets: **-50+bps**, **-25bps**, **No change**, **+25bps** and **+50+bps**. The tooltip gives the meeting date and the source; banks without market pricing say so (see [Economy](help:economy) › Rates).

Two seasonality cards close the sheet. **Seasonality · By Month** shows the average return of each month over up to 16 years, with the current month outlined; hovering a bar gives its value and the number of years. **Seasonality · Year Path** draws the average path of the finished years against the current year.

### The sheet's chart

The chart opens on 4h. Its toolbar holds four favourite timeframes (1m, 30m, 1h, 4h) and a menu with all of them: 1m, 5m, 15m, 30m, 1h, 4h, **1D**, **1W** and **1M**. Then come buttons for bars, candles and area, with a menu holding the full list: **Bars**, **Candles**, **Hollow candles**, **Line**, **Area** and **Heikin Ashi**. The circled + overlays another market, chosen from a list that suits the category, until you pick **Remove compare**. The **Indicators** menu offers **Volume**, SMA 20, SMA 50, SMA 200, EMA 21 and **Bollinger Bands 20, 2**, and the camera copies the chart image to the clipboard.

How far back the chart goes depends on the timeframe, and the last bar always follows the live quote:

| Timeframe | History loaded |
|---|---|
| 1m | 5 days |
| 5m to 30m | 1 month |
| 1h | 6 months |
| 4h | 1 year, built from hourly bars |
| 1D | 5 years |
| 1W, 1M | everything available |

The range buttons under the chart jump to a span with a bar size that suits it: **1D** (1m bars), **5D** (5m), **1M** (30m), **3M** (1h), **6M** (4h), **YTD** and **1Y** (daily), **5Y** (weekly) and **All** (monthly). Beside them, the **auto**, **log** and **%** toggles set the scale, and a clock shows your local time and UTC offset.

The drawing bar on the left holds a crosshair, an emoji tool and these drawing tools:

| Tool | How it works |
|---|---|
| Trend line | 2 clicks |
| Fibonacci retracement | 2 clicks; levels 0, 0.236, 0.382, 0.5, 0.618, 0.786, 1 |
| XABCD pattern | 5 clicks |
| Long position box | One click; target and stop at 2:1 |
| Brush | Drag |
| Text | Asks for the text |
| Measure | Drag: price change, % and number of bars |
| Zoom | Drag across the bars to fit them |
| Magnet | Snaps to the nearest open, high, low or close |

With the mouse, drag to pan, use the wheel to zoom around the pointer, or swipe horizontally to pan. Dragging the price axis stretches the scale and double-clicking it returns to auto scale; double-clicking the plot resets the view. Hovering near the bottom of the plot reveals zoom in, zoom out, scroll and reset buttons, and clicking the OHLC legend folds it. A right-click offers **Add horizontal line at** the price under the pointer, **Hide drawings** / **Show drawings**, **Remove all drawings** and **Reset chart view**.

### Where the data comes from

Quotes come from Yahoo Finance, polled every 20 s while the list is visible; the list stops polling while a sheet is open or the page is hidden. The 1W and 1M references are Yahoo daily closes of the last 3 months, kept 30 minutes and asked again after a failure. A closed market shows its last trade and its change against the previous close, and a market without a quote shows **—**. No key is needed, except the Myfxbook login for the retail card.

The drawings and chart settings of the sheet are not saved: they go when you open another market.

## Seasonality

Seasonality answers a simple question: how has this asset usually behaved at this time of year? Under the **Seasonality Lab** title, a toolbar chooses the asset, the years and the period. The **SUMMARY** part below holds the seasonal trend chart and the **SEASONAL WIN RATE** card; the **ADVANCED** part holds the average return by weekday and by month, the return of each year over the period, and the **RISK METRICS**.

### Choosing what to study

The asset menu opens the toolbar, labelled **Seasonality:** followed by the asset's name. It lists the whole catalogue, FX majors first, with a search field: the arrow keys move, Enter picks and Esc closes. The arrows next to it step to the previous or next asset, and the ⓘ explains the page. The years menu offers **10 years**, **15 years** or **20 years** (20 by default), and the **Filter** menu chooses between **All years** and **Exclude outlier years (>1.5σ)**; the filter only changes the month averages.

The period buttons **Full**, **H1**, **H2** and **Q1** to **Q4** choose the whole year, a half or a quarter, and every card is recomputed for that period. **Compare** overlays the seasonal average of another asset (**None** removes it). The **actual** toggle, labelled with the current year, overlays this year's real path, and **Index (100)** shows the path rebased to 100 instead of as a % return. The reload button sits on the right. The asset, the years and the filter are remembered.

### The seasonal trend chart

The chart's title gives the asset, the number of years really used and the date range. The curve is the average path, drawn as a 3-day Gaussian moving average for readability; its highest and lowest points are labelled and **TODAY** marks the date. Hovering gives the date and the value, along with the compare and actual values when they are shown.

To study any stretch of the year, drag across the chart over at least 2 days: a badge gives the period and the change of the average path over it, and **× Clear** returns to the full year.

### How the statistics are computed

Daily closes are placed on a calendar of 366 days, and a day without a close carries the last one. A year is used when it has a close in the 10 days before 1 January, which becomes its base, or in its own first 10 days; years without history are dropped. Each year then draws a path:

Year path = close of the day ÷ base − 1

The average path is the mean of the paths of the finished years. The current year is left out of it and shown on its own as the actual overlay. For the statistics, the return of a year over the chosen period is:

Period return = close at the end ÷ close the day before the start − 1

A year counts once it covers the whole period, so the current year only counts once the period is over.

| Figure | Rule |
|---|---|
| Win rate | Share of counted years with a positive period return |
| **Annual return** / **Period return** | Change of the average path over the period: (1 + p(end)) ÷ (1 + p(day before start)) − 1 |
| **Best month** / **Worst month** | Highest and lowest average month inside the period |
| **VOLATILITY RATIO** | Std. dev. of daily returns inside the period ÷ std. dev. of all daily returns; below 0.95 calmer, above 1.05 more volatile, else normal |
| Average return by weekday | Mean daily return by weekday over the days inside the period, current year included; tooltip with the number of days |
| Average return by month | Mean month return over the counted years; with the outlier filter, years more than 1.5 std. dev. from that month's mean are left out |
| Individual period returns | Period return of each year; the current year is dimmed (**to date, not counted**) |
| **CUMULATIVE PROFIT** | (1 + r₁) × (1 + r₂) × … − 1 over the counted years |
| **AVG PERIOD RETURN** / **VOLATILITY** | Mean and std. dev. of the period returns |
| **MAX DRAWDOWN** | Largest peak-to-trough fall of the average path inside the period |
| **RETURN / RISK** | Mean ÷ std. dev. (a plain ratio, not a Sharpe ratio) |
| **BEST PERIOD** / **WORST PERIOD** | Best and worst year for the period |

### Where the data comes from

The page reads Yahoo Finance daily closes for the chosen years plus one month, keeps them 30 minutes, and asks again when you press the reload button. If the history is missing or too short, the chart says **No daily history for…** or **Not enough daily history for a seasonal average**; a compare asset without history is named in the chart's subtitle.

## Fundamental Heatmap

The Fundamental Heatmap asks which of the eight major currencies their economy supports most. The strip at the top, **FUNDAMENTALS — MACRO FILE BY CURRENCY**, has an ⓘ and a badge on its right whose tooltip gives the scoring rules. Below it, eight score cards (USD, EUR, GBP, JPY, CHF, CAD, AUD, NZD) are sorted strongest first, each with its rank, flag, score, a bar centred on zero and a bias: **BULLISH**, **NEUTRAL** or **BEARISH**.

The rest of the page is the **MACRO FILE** of the selected currency's economy: the monetary policy card, the automated read, the pair comparator, the heatmap itself, the release detail, a footnote and the method. The strongest currency is selected until you pick another, and your pick is remembered.

### The monetary policy card

The card opens with the **POLICY RATE** (or **POLICY RATE · TOP OF** a range), the prior rate and the date of the last decision. **LAST MOVE** says **HIKE**, **HOLD** or **CUT**, and **NEXT MEETING** counts down (**IN** n days, **TODAY**, **PASSED**). An inflation gauge sets headline and core CPI against the bank's target.

Two history charts follow, the policy rate and inflation against the target, over **5Y**, **10Y** (the default), **20Y** or **MAX**. Hovering gives the value, along with the change on the month for the rate or the gap to target for inflation.

### How a currency is scored

Each currency gets six signals, each between −1 and +1, a positive value supporting the currency. Two tools recur in the rules: tanh squashes a gap into the ±1 range, and the trend is +1, −1 or 0 when the latest release is above, below or equal to the previous one.

| Column | Signal | Word |
|---|---|---|
| **RATES** | 0.7 × last move (hike +1, hold 0, cut −1) + 0.3 × tanh(real rate ÷ 2), real rate = policy − CPI | HIKE / HOLD / CUT |
| **INFLATION** | 0.7 × tanh(CPI − target midpoint) + 0.3 × trend; above target supports (hawkish bank) | ABOVE / NEAR / BELOW TARGET (band ± 0.5 pt) |
| **GROWTH** | 0.7 × tanh(GDP q/q ÷ 0.4) + 0.3 × trend | EXPANDING (> 0.1%) / FLAT / CONTRACTING (< −0.1%) |
| **LABOUR** | −0.7 × tanh(Δ unemployment ÷ 0.15) − 0.3 × tanh(z vs 5-year mean) | IMPROVING / STABLE / SOFTENING (± 0.1 pt) |
| **ACTIVITY** | 0.7 × tanh((index − neutral) ÷ scale) + 0.3 × trend; PMI: neutral 50, scale 3; business confidence: neutral 100, scale 1 | EXPANSION / CONTRACTION |
| **SENTIMENT** | 0.6 × tanh(Δ consumer confidence ÷ (2 × median change)) + 0.4 × tanh(z vs 5 years) | IMPROVING / STEADY / WEAKENING |

The inflation targets are 2% for most banks (above target from 2.5, below from 1.5), 2–3% for the RBA (above 3.0, below 2.0) and 0–2% for the SNB (above 2.5, below 0.0).

A currency's level is 100 × the mean of its available signals, and it gets no score with fewer than 3 signals. Because currencies only move against each other, the score is the level minus the mean level of the eight:

Score = level − mean level of the eight currencies

A score of +8 or more reads **BULLISH**, −8 or less **BEARISH**, and anything between **NEUTRAL**. In the heatmap, cells are bright teal from 0.5, dim teal from 0.15, grey between −0.15 and 0.15, dim red from −0.15 down and red from −0.5 down; a missing figure shows **—**.

### The heatmap, the read and the comparator

The **FUNDAMENTAL HEATMAP** has one row per currency: the **CURRENCY** with its score, the six signal columns and the **BIAS**. Hovering a cell gives the rule applied and the signal value, and hovering the score or the bias shows how the score was built. Clicking a row selects the currency, as the cards do.

The **AUTOMATED READ**, marked **RULES**, puts the file into words: a headline on policy, inflation and the strongest or weakest other signal, a paragraph on the latest releases, and the three strongest signals explained. It is built from rules, with no language model.

The **PAIR COMPARATOR** sets the selected currency against any of the other seven, picked by tabs. It compares seven rows: policy rate, headline CPI, core CPI, GDP q/q, unemployment, activity index and retail sales y/y. Higher is better on every row except unemployment, and the verdict names the currency that leads on more rows (**Fundamentals favour…**, or **Fundamentals balanced**), with both scores. A row missing on either side is not counted, and two different activity indices are not compared.

Finally, the **RELEASE DETAIL** of the currency unfolds on a click into a table with the columns **INDICATOR**, **LATEST**, **PREVIOUS**, **PERIOD**, **RELEASED** and **SOURCE**; a missing figure shows why.

### Where the data comes from

Policy rates, decisions and meetings come from the central-bank feeds; the rate history and CPI from the BIS (monthly); GDP, unemployment, business and consumer confidence from the OECD, with World Bank annual unemployment as a fallback, marked **(yr)**. The euro area is the aggregate. See [Economy](help:economy) for these services. The page recomputes whenever they update, and no key is needed. While the data loads, the cards say **LOADING** and the file **Waiting for macro data…**.

No free PMI is used, so activity is the OECD business confidence index, shown as **BCI**. Core CPI and retail sales are missing for some economies, which then show **—** with the reason.

## Technical Heatmap

The Technical Heatmap scans every market of the catalogue on daily candles and tells you, at a glance, which are trending, stretched or close to a liquidity pool. The strip at the top, **TECHNICAL SURVEILLANCE — MULTI-SIGNAL SCAN**, carries a badge that shows the progress while loading, then the number of instruments computed; its tooltip lists the instruments without usable history.

Six cards count, over every loaded instrument, those in a bullish trend, in a bearish trend, overbought, oversold and near liquidity, and **MARKET LEAN** draws bullish ÷ (bullish + bearish) as a bar. Below come the filters, the table, with **INSTRUMENT** (name and live price), the six reads and a legend under the rows, and on the right the **SIGNAL FILE** of the selected instrument.

### The six reads

Every read uses daily candles. ATR14% is the 14-day average true range as a % of the price.

| Column | Rule |
|---|---|
| **TREND** | gap = (EMA20 − EMA50) ÷ EMA50; ≥ +0.10% BULLISH, ≤ −0.10% BEARISH, else NEUTRAL; intensity = tanh(\|gap\| ÷ (2 × ATR14%)) |
| **MOMENTUM** | RSI(14) ≥ 70 OVERBOUGHT, ≤ 30 OVERSOLD; ROC(10) as context |
| **VOLATILITY** | percentile of today's ATR14% among its last 252 days: ≥ 80 HIGH, ≤ 20 LOW, else NORMAL; detail = 20-day realised volatility, annualised (√365 crypto, √252 others) |
| **VOLUME** | last complete day ÷ mean of the 20 days before; ≥ 1.50 ABOVE AVG, < 0.75 BELOW AVG, else AVERAGE; N/A without volume |
| **STRUCTURE** | last two 5-bar swing highs and lows: both higher BULLISH, both lower BEARISH, else RANGING |
| **LIQUIDITY** | equal highs / lows clustered within max(0.06%, ¼ ATR14%); swept in the last 3 days RECENTLY SWEPT; nearest untested pool within 1 × ATR NEAR POOL, else POOL ABOVE / POOL BELOW; none NO POOL |

Many markets have no volume of their own, so Deepwater reads it from a proxy and the cell shows the ticker used. FX legs use currency ETFs (FXE, FXB, FXY, FXF, FXC, FXA, UUP), a cross taking the mean of its two legs. US indices, metals, oil, gas, gasoline, wheat, soybeans and sugar use ETFs (SPY, QQQ, DIA, IWM, GLD, SLV, PPLT, PALL, CPER, USO, BNO, UNG, UGA, WEAT, SOYB, CANE). Yields and the VIX have none, and other markets use their own volume.

A cell's hue gives its meaning (cyan bullish, red bearish, yellow elevated, grey subdued, violet near a pool) and its intensity the magnitude.

### The signal file

The signal file shows the selected instrument's id, name, live price and trend pill, then a radar of the six reads on a 0–100 scale where 50 is neutral. The trend is 50 ± 50 × intensity, momentum is the RSI, volatility the ATR percentile, volume 50 × the ratio, structure 85, 15 or 50 (bullish, bearish, ranging), and liquidity:

Liquidity = 100 × exp(−|distance| ÷ (2 × ATR14%))

Under the radar the six reads are listed with their details, and hovering the panel gives the full rules.

### Working with the table

The filters narrow the table: category chips (**ALL**, **FX**, **COMMODITIES**, **INDICES**, **STOCKS**, **CRYPTO**, **YIELDS**, with their counts), trend chips (**ANY TREND**, **BULLISH**, **BEARISH**) and **SEARCH INSTRUMENT…**, which matches the name or the symbol. After a filter change the first row is selected; when the filters leave nothing, the table says **NO INSTRUMENT MATCHES THESE FILTERS.**

Clicking a row shows its signal file. Clicking a column header sorts by it, descending first (alphabetically for **INSTRUMENT**), and a second click reverses the order; by default the table is sorted by trend, strongest first. Hovering a cell gives the numbers behind the read, and hovering the instrument shows all six reads. The wheel scrolls the table. On a phone, tapping a row opens the signal file over the page, and **✕** closes it.

### Where the data comes from

The scan reads 2 years of Yahoo Finance daily candles for the 127 markets of the catalogue and the volume proxies, and runs again every 30 minutes while the page is visible; prices follow the live quotes every 20 s. The first scan takes a moment, and the table fills as the series arrive (**LOADING DAILY CANDLES…**). A market without usable daily history is left out and listed in the badge's tooltip.

## Liquidity

The Liquidity page maps where stop orders are likely to sit on one instrument, the pools above equal highs and below equal lows, and tracks when price sweeps them. The strip at the top, **LIQUIDITY MAPPING — STOP POOLS & SWEEPS**, holds the instrument button and the live price. Clicking the button opens the whole catalogue with category tags and a search field; Esc closes it, and your choice is remembered.

Three cards sum up the map. **STOP-RUN LIKELIHOOD** says how close the price is to a pool, the second card counts the **UNTESTED POOLS** out of all those detected, and the third counts the **SWEEPS (VISIBLE HISTORY)**, with the time since the last one. Below them come the hourly chart with its pools, a panel with three tabs and a method note; the footer gives the time of the last computation and a **DELAYED** badge.

### The chart

The chart draws hourly candles on a bar scale, so weekends leave no gaps. It shows the 4 nearest untested pools on each side of the price and the 3 most recent pools swept by a candle on screen. Buy-side pools (BSL) are dotted red, sell-side pools (SSL) dotted cyan, and the previous day's high and low (PDH / PDL) dashed gold. Each level has a tag on the price axis (**BSL ×N** and its price), beside the last price tag.

The wheel zooms around the pointer and dragging pans; a dashed crosshair gives the price and the date in UTC. Drag the grip at the bottom right to change the chart's height.

### The panel tabs

**LIQUIDITY POOLS** lists the untested pools and those taken in the last 30 days, by price, with a **LAST** band at the current price. Each row gives the side and the level, a dot for each candle that traded through the level (+n beyond 5), **SWEPT** or **BROKEN**, and the distance from the price in %.

**SWEEP ALERTS** lists the swept pools, most recent first: **LIQUIDITY SWEEP DETECTED**, the side and level, the **Reversal confirmation:** and the time since. **SESSION EXTREMES** gives the **WINDOW**, **HIGH** and **LOW** of the latest Sydney, Asian (Tokyo), London and New York sessions (in UTC), of the previous day and of the previous week.

### How pools and sweeps are detected

A swing is a 3-bar fractal: a high above the highs of the bars before and after it, and the mirror for lows. Swing highs are grouped while they stay within 0.06% of the group's first price, and a group of 2 swings or more makes a buy-side pool at its highest price; lows give sell-side pools at their lowest price. ×N is the number of swings in the pool.

The first later candle that trades beyond the level decides its state. If it closes back on the other side, the pool is **SWEPT**; if it closes beyond, the pool is **BROKEN**; until then it is untested.

After a sweep, the reversal confirmation measures the move back from the level over the next 6 candles, in ATR(14) of the sweep candle: 1.5 or more is Strong, 0.75 or more Moderate, anything less Weak. It stays Pending while the 6 candles are not all there and the move is still under 1.5.

The stop-run likelihood is the distance from the price to the nearer untested pool, in hourly ATR(14): 2 or less is High, 6 or less Medium, more is Low. It is a disclosed rule, not a backtested probability, and its tooltip gives the pool and the distance. Sessions run in local hours with daylight saving: Sydney 07–16, Tokyo 09–18, London 08–17, New York 08–17.

### Where the data comes from

The page reads 2 years of Yahoo Finance hourly candles and reloads them every 5 minutes while it is visible; the price follows the live quotes. Without candles, the chart and the first card say **Hourly candles unavailable** with the reason. A market whose pools have all been traded through shows **No untested pool**. The thresholds are Deepwater's own and are printed on the page.
