# Account

The Account tab is about your own trading. It holds a journal of your closed trades with its statistics, filled from MetaTrader 4, MetaTrader 5 or cTrader statements, from a read-only live bridge or by hand, and a calculator that sizes a position from your risk before you enter. Everything stays on this computer, and no broker password is ever asked.

## Track Record

Track Record is your trading journal: it answers how you have really been trading, trade by trade and in aggregate. Its toolbar holds three views, **DASHBOARD**, **TRADE LOG** and **ADVANCED DETAILS**, then **FILTERS**, a year selector, the account box with the account's name and starting balance, the date of the last import, and the **+ LOG TRADE** and **IMPORT TRADES** buttons. The view you used last is reopened.

Every view shows the trades of the selected account, after the year and the filters. **FILTERS** opens a bar with **All instruments**, a side choice (**Long & short**, **Long only**, **Short only**), **All strategies**, and the count **n of m trades**. The year selector offers **All years** and every year that has trades, by close date in local time. With no account or no trade, a box offers **IMPORT TRADES** and **+ LOG TRADE**; when the filters leave nothing, the view says **NO TRADE MATCHES THE FILTERS**.

### Accounts

Clicking the account box opens a menu with your accounts (name, balance and currency; the checked one is shown), followed by **Add account…**, **Edit account…** and **Remove account…**. The last entry, **Add demo account with sample trades** (or **Show the demo account (sample trades)** once it exists), loads 60 invented trades on a 10,000 USD balance; the demo account is clearly labelled and never real.

The account form asks for a **Name**, the **Account currency** as a 3-letter code, the **Starting balance** and the **Platform** (**Manual**, MT4, MT5, cTrader or **Other**). The starting balance is your deposits minus your withdrawals, and every % figure in the journal is computed from it.

Removing an account deletes it and its trades after a confirmation, and cannot be undone. A live-sync account removed this way is not recreated until you choose the bridge folder again.

### Dashboard

The dashboard opens with five cards; hovering a title gives its definition.

| Card | What it measures |
|---|---|
| **NET P&L** | Sum of net results (profit + commission + swap) in the account currency, with the number of closed trades |
| **TRADE WIN %** | Winning trades ÷ closed trades; chips for wins · breakeven · losses |
| **PROFIT FACTOR** | Gross profit ÷ gross loss; the ring shows the share of gross profit (cyan) and gross loss (red) |
| **MAX DRAWDOWN** | Largest fall of the equity curve from a peak, in money and in % of the equity at that peak (starting balance + cumulative P&L); **now** is the current distance from the last peak |
| **AVG WIN / LOSS TRADE** | Average winning trade ÷ average losing trade, with both averages |

The **PERFORMANCE SCORE** turns six statistics into a radar on Deepwater's own 0–100 scale, with green dots at 70 or more, and gives their plain average. The scale is Deepwater's own, not a standard measure:

| Axis | 0–100 rule |
|---|---|
| Win % | Win rate itself |
| Profit factor | Profit factor ÷ 3 (3 or more = 100) |
| Avg win/loss | Average win ÷ average loss ÷ 2.5 (2.5 or more = 100) |
| Recovery factor | Net P&L ÷ max drawdown ÷ 5 (5 or more = 100) |
| Max drawdown | 100 at no drawdown, 0 at a 50% drawdown |
| Consistency | Share of profitable months |

Below it, **TRADE NET CUMULATIVE P&L** draws the running total, one point per trading day; hovering gives the date, the day's P&L and the total. **MONTH P&L** and **QUARTER P&L** show the net P&L per calendar month and quarter (close date, local time), and **LAST TRADES P&L** shows the last 40 trades, oldest on the left. Hovering a bar gives its value and number of trades, or, for a single trade, its instrument, side and close date.

The **AUTOMATED READ** sums all this up in rule-based sentences: totals and averages, the deepest drawdown and whether it was recovered, the best and weakest instrument, the best and weakest day, the average holding time of winners against losers, and the longest winning and losing streaks, a breakeven trade ending a streak. It is not advice, and no language model writes it.

### Trade Log

The Trade Log is the journal itself, one row per trade. Its cards give **CLOSED TRADES**, **WINS / LOSSES**, **WIN RATE**, **NET P&L**, **OPEN POSITIONS** and **UNREALIZED**; since the journal holds closed trades only, the last two always read **—**. The outcome pills **ALL**, **WINS**, **LOSSES** and **BREAKEVEN** and a search field over instrument, strategy and comment narrow the rows.

The columns are **OPENED**, **CLOSED**, **INSTRUMENT**, **SIDE**, **SIZE**, **ENTRY**, **EXIT**, **STOP**, **TARGET**, **HELD**, **R**, **STRATEGY**, **SESSION** and **P&L**, followed by an eye to view or edit the trade and a bin to delete it after a confirmation. **R** measures the result in units of the risk taken, signed by side, and stays empty without a stop:

R = (exit − entry) ÷ (entry − stop)

**SESSION** is taken from the open time in UTC: Asian 21–07, London 07–12, NY Overlap 12–16, New York 16–21.

Clicking a header sorts by it (dates, R and P&L descending first), and a second click reverses the order; empty values go last, and by default the newest close comes first. Hovering a row gives the open and close times, the session and the comment, and hovering the P&L splits it into profit, commission and swap. Narrow windows drop columns, first OPENED and SESSION, then STOP and TARGET; the trade's form still shows them.

### Advanced Details

Advanced Details looks for patterns in your results. Its header, **ADVANCED DETAILS · n CLOSED TRADES**, holds a **$ / % BALANCE** switch that shows money or % of the starting balance; the % needs a starting balance, and without one a tooltip says where to set it.

**DRAWDOWN · MAX** draws how far the equity curve sits below its previous peak after each trade. Two scatter plots put one dot per trade and set its net P&L against the open hour in local time (**TRADE TIME PERFORMANCE**) or against the time held, on a log scale (**TRADE DURATION PERFORMANCE**). **P&L BY WEEKDAY** and **NUMBER OF TRADES BY WEEKDAY** group trades by close day in local time, leaving out days without trades.

The last row lists the **TOP 5 TRADES** and **BOTTOM 5 TRADES** by net P&L with their close date, then **TOP 5 SYMBOLS P&L** and **NUMBER OF TRADES · TOP 5 SYMBOLS**. Hovering any dot or bar gives its details.

### Logging a trade

**+ LOG TRADE** opens the trade form, creating an account first if there is none; the eye of an existing trade opens the same form. Its fields are:

- **Instrument**, typed to search the catalogue;
- **Side**: **↗ LONG** or **↘ SHORT**;
- **Opened (local time)** and **Closed (local time)**;
- **Entry price**, **Exit price** and **Size (lots)**;
- **P&L**;
- **Stop loss (optional)** and **Take profit (optional)**;
- **Commission** and **Swap**;
- **Strategy**: your past strategies, Breakout, Mean reversion, News, Reversal, Trend following, or a new one;
- **Comment**.

If you leave **P&L** empty, Deepwater estimates it as price move × lots × contract size, converted into the account currency at today's live rate rather than the rate of the trade day; the form prints the contract size used. Before saving, it checks that there is an instrument, that entry, exit and size are positive, that the close is not before the open, that there is a P&L or an estimate, and that the stop differs from the entry.

### Importing statements

**IMPORT TRADES** opens a dialog in two parts; the first, **FROM A STATEMENT FILE**, reads a statement exported from your platform:

- MT4: Account History › right click › All History, then Save as Detailed Report (.htm).
- MT5: History › right click › Report › HTML.
- cTrader: History › export CSV.
- Any CSV with a header row (symbol, side, open/close time, prices, size, profit).

Statements are written in the broker's server time, so set **Statement times are in** to its offset, from UTC−12 to UTC+14; MetaTrader servers are often UTC+2 in winter and UTC+3 in summer, while cTrader files give their own zone. Changing the offset updates the preview.

**CHOOSE FILE…** shows a preview: the format, the number of closed trades, the first and last date, the deposits, the account number, the rows not read and any warnings. **Into** picks an existing account (shown with its currency) or **New account (from the file)**, prefilled from the file. If the statement is in another currency than the account, Deepwater asks before importing, because profits are not converted.

**IMPORT** adds only the trades not yet in the journal and reports how many were new, so importing the same file twice is harmless; importing it again with a corrected offset corrects the times of the trades already there. Excel and PDF reports are not read, and files over 64 MB are refused.

### Live sync (read-only bridge)

The second part of the dialog keeps the journal up to date without exports. **SAVE BRIDGE FILES…** writes a small Expert Advisor for MT4 and MT5 and a cBot for cTrader, with a README.txt, for you to install in your platform. They only write your closed trades to a file and never send orders.

**CHOOSE FOLDER…** tells Deepwater where those files are written, MetaTrader's Common/Files or Documents/Deepwater for cTrader, and lists the matching folders found on this computer. From then on Deepwater watches the folder and adds new trades whenever a file changes. Each file feeds one account, created the first time the file is seen, and the status line says what was synced. **STOP** ends the sync.

A trade you delete from a bridge account is not added again, and a starting balance you typed is kept.

### Where the data is kept

The journal is a local file and is never sent anywhere. If it cannot be read, it is set aside, the journal starts empty and a message says where the old file is. All statistics use the net P&L (profit + commission + swap) in the account currency, with dates in local time. Contract sizes follow the usual CFD conventions listed under Risk Calculator; your broker's may differ.

## Risk Calculator

The Risk Calculator answers the question to ask before entering a trade: how many lots can I take so that hitting the stop costs exactly what I decided to risk? The title, **RISK CALCULATOR — POSITION SIZING DESK**, carries a badge with the instrument. The **TRADE PARAMETERS** sit on the left, with any error shown under its field, and the results on the right: **POSITION**, **RISK**, **REWARD**, **TRADE MAP**, **RISK / REWARD RATIO** and a note on the contract and conversion used.

### Entering the trade

Click **INSTRUMENT** to open a searchable list of FX majors, other pairs, metals, indices, energy, agriculture, crypto and stocks (bonds are not offered); Enter picks the first match and Esc closes. Choosing an instrument prefills the levels from the live price: the entry at the live price, the stop 50 pips below for FX or about 0.5% below otherwise, and the take profit twice the stop distance above. Type over them freely.

**ACCOUNT BALANCE** starts from the value you typed last, otherwise from the starting balance of the current journal account, otherwise from 10,000. **RISK PER TRADE** is a % of the balance or an amount in the account currency, chosen with the switch on its right. The account currency is that of the current journal account, or USD without one. Then come **ENTRY PRICE**, **STOP LOSS** and **TAKE PROFIT (OPTIONAL)**. Numbers accept a comma as the decimal mark, and the balance, risk, unit and instrument are remembered.

### Reading the results

The side follows from the stop: below the entry the trade is **↗ LONG**, above it **↘ SHORT**. The calculator first works out what one lot loses at the stop, then divides your risk by it:

Loss at the stop for 1.00 lot = stop distance × contract size × price unit × conversion rate

**LOT SIZE** = risk ÷ that loss, rounded **down** to 0.01 lot

The minimum is 0.01 lot, shown in red when even that risks more than you asked. **POSITION SIZE** gives the lots × contract size in units, ounces, barrels and so on, and **STOP DISTANCE** gives the distance in price. **RISK** is the money at risk with the rounded size and its % of the balance; when rounding lowered it, the target % is shown beside it. **REWARD** is the money at the take profit, or says **no take profit set** or **take profit on the stop side**. The **TRADE MAP** places **STOP**, **ENTRY** and **TARGET** on one scale.

The ratio compares the reward with the risk:

**RISK / REWARD RATIO** = 1 : (|take profit − entry| ÷ stop distance)

It is drawn on a 1:0 to 1:4 scale and graded **POOR** below 1, **MARGINAL** below 1.5, **ACCEPTABLE** below 3 and **EXCELLENT** from 3. **BREAKEVEN WIN RATE** = 1 ÷ (1 + R) tells you how often such a trade must win just to break even.

### Contract sizes (1.00 lot)

| Instrument | Size |
|---|---|
| Currency pairs | 100,000 units of the base currency |
| Gold, platinum, palladium | 100 oz |
| Silver | 5,000 oz |
| Copper | 25,000 lb |
| Crude oil | 1,000 barrels |
| Natural gas | 10,000 MMBtu |
| Heating oil, gasoline | 42,000 gallons |
| Corn, wheat, soybeans | 5,000 bushels (prices in cents) |
| Coffee | 37,500 lb (price in cents) |
| Sugar | 112,000 lb (price in cents) |
| Cotton | 50,000 lb (price in cents) |
| Live cattle | 40,000 lb (price in cents) |
| Cocoa | 10 tonnes |
| Indices | 1 unit per point, in the index's currency |
| Crypto | 1 coin |
| Stocks | 1 share |

### Where the data comes from

The live price and the conversion rates come from the app's quotes (Yahoo Finance, every 20 s while the page is visible), used direct, inverted or through USD. Until a rate arrives the page says **Waiting for the live … rate.**; it also asks you to **Enter an entry price and a stop loss.**, warns that **The stop loss must differ from the entry price.** or that **The take profit is on the stop's side of the entry.**, and requires a positive balance and risk.

The math is deterministic and leaves out spread, commission, swap and margin. Lots step by 0.01, and contract sizes may differ from your broker's, which is why the page prints the one it assumes. Nothing is sent to a broker: to keep the trade, log it in Track Record once it is closed.
