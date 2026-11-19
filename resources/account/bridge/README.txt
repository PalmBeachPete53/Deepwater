Deepwater bridge — live, read-only sync of closed trades
=========================================================

The bridge is a tiny program that runs inside your trading platform and writes the list of your closed
trades to a file. Deepwater watches the folder and adds new trades to the journal. No password is given
to Deepwater, and the bridge never sends, modifies or closes an order.

These sources were written for Deepwater and have not been compiled by its authors on every platform
build: compile them in your platform and report any error.

MetaTrader 4
  1. File › Open Data Folder › MQL4 › Experts: copy DeepwaterBridge.mq4 there.
  2. Navigator › Expert Advisors › right click › Refresh, then open it in MetaEditor and press Compile.
  3. Drag DeepwaterBridge onto any chart (algo trading does not need to be enabled: the EA only reads).
  4. In the Account History tab, right click › All History, so the EA sees every closed trade.
  The file appears in the Common data folder: <Terminal common folder>/Files/Deepwater_MT4_<login>.csv
  (Windows: %APPDATA%\MetaQuotes\Terminal\Common\Files).

MetaTrader 5
  Same steps with DeepwaterBridge.mq5 in MQL5 › Experts. File: Common/Files/Deepwater_MT5_<login>.csv

cTrader
  1. Automate › New cBot, paste DeepwaterBridge.cs, Build.
  2. Add an instance on any chart and start it; grant file system access when asked.
  The file goes to Documents/Deepwater/Deepwater_cTrader_<account>.csv (or the folder you set in the
  cBot's parameter). cTrader's history has no stop loss / take profit: R multiples stay empty.

Deepwater
  Track Record › IMPORT TRADES › Live sync › choose the folder that holds the Deepwater_*.csv files.
  Each file becomes one account in the journal (MT4 123456, MT5 7654321, …).

Server time
  MetaTrader writes times in the broker's server time. The MT4 EA measures the server's offset from UTC only
  when a live tick has just arrived (not on weekends or while disconnected) and waits for one before writing
  the file the first time. The header then says "times=verified"; Deepwater uses such a file to correct the
  times of trades synced earlier by an older bridge with a wrong offset (shifts over 90 minutes).

File format (one line per closed trade, times in UTC seconds):
  ticket;symbol;side;lots;open_utc;open_price;close_utc;close_price;sl;tp;commission;swap;profit;comment
