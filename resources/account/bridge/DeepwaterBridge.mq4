//+------------------------------------------------------------------+
//| DeepwaterBridge.mq4 — read-only journal bridge for Deepwater     |
//| Writes the account's closed trades to                            |
//|   <Common data folder>/Files/Deepwater_MT4_<login>.csv            |
//| Deepwater watches that folder (Track Record › Import › Live sync).|
//| The EA never sends, modifies or closes orders.                    |
//| Set the Account History tab to "All History" so that every       |
//| closed trade is visible to the EA.                                |
//+------------------------------------------------------------------+
#property copyright "Deepwater"
#property version   "1.20"
#property strict
#property description "Read-only: exports closed trades for the Deepwater journal. Sends no orders."

input int ScanSeconds = 5;   // how often the history is checked

int      g_lastCount = -1;
double   g_lastProfit = 0;
datetime g_lastServer = 0;       // TimeCurrent() seen at the previous check
datetime g_serverMovedAt = 0;    // TimeLocal() when TimeCurrent() was last seen moving (a tick arrived)

// The offset is kept per account in a terminal global variable. "V2": values saved by the first version of the
// EA are not trusted (it saved stale offsets measured on weekends, e.g. -9 h instead of +2 h).
string OffsetKey() { return("DeepwaterBridgeOffsetV2_" + IntegerToString(AccountNumber())); }

int OnInit()
{
   EventSetTimer(MathMax(1, ScanSeconds));
   WriteHistory();
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) { EventKillTimer(); }

// A tick of the chart's symbol has just arrived: TimeCurrent() is the server's time right now.
void OnTick()
{
   MeasureOffset(true);
   if(g_lastCount < 0) WriteHistory();      // still waiting for a trustworthy offset
}

void OnTimer()
{
   MeasureOffset(false);
   if(OrdersHistoryTotal() != g_lastCount) WriteHistory();
}

// Broker server time minus UTC, in seconds, rounded to 15 minutes, saved only when it can be trusted.
//
// TimeCurrent() is not a clock: it is the server time of the LAST TICK received (any Market Watch symbol).
// Attached on a weekend, at night on a quiet symbol or while disconnected, it can be hours or days old, and
// TimeCurrent() - TimeGMT() is then wrong by that much (the first version saved e.g. -9 h for a UTC+2 server:
// every exported time shifted by 11 h). So the difference is measured only when TimeCurrent() is known to be
// fresh:
//   - inside OnTick() (a tick has just arrived), or
//   - in OnTimer() when TimeCurrent() moved since the previous check and that move was seen less than
//     2 minutes ago (ticks of other Market Watch symbols keep it running while the market is open),
// and only while the terminal is connected. A fresh measurement is rounded to 15 minutes (tick delays of a few
// seconds or minutes disappear) and must be within -12 h .. +14 h. Otherwise the last good value saved for this
// account is used; with none yet, the file is not written (and Deepwater keeps what it has) until the market
// gives a trustworthy tick. TimeGMT() comes from this computer's clock and time zone settings.
void MeasureOffset(bool tickNow)
{
   datetime server = TimeCurrent();
   if(g_lastServer != 0 && server > g_lastServer) g_serverMovedAt = TimeLocal();   // a tick arrived since the last check
   g_lastServer = server;
   bool fresh = tickNow || (g_serverMovedAt != 0 && TimeLocal() - g_serverMovedAt <= 120);
   if(!fresh || !IsConnected() || server <= 0) return;
   long off = (long)MathRound((double)(server - TimeGMT()) / 900.0) * 900;
   if(off < -12 * 3600 || off > 14 * 3600) return;
   GlobalVariableSet(OffsetKey(), (double)off);
}

// true and the offset when a trustworthy one is known (measured now or saved earlier for this account)
bool ServerOffset(long &off)
{
   MeasureOffset(false);
   if(!GlobalVariableCheck(OffsetKey())) return(false);
   off = (long)GlobalVariableGet(OffsetKey());
   return(true);
}

string Clean(string s)
{
   StringReplace(s, ";", ",");
   StringReplace(s, "\r", " ");
   StringReplace(s, "\n", " ");
   return(s);
}

string Num(double v, int digits) { return(DoubleToString(v, digits)); }

// Volumes with every decimal the broker allows (a 0.001 lot written with 2 decimals was 0.00)
string Lots(double v) { return(DoubleToString(v, 8)); }

// The Sunday of week n (1..4) of a month, or its last Sunday (n = 5), at 00:00
datetime NthSunday(int year, int month, int n)
{
   MqlDateTime d;
   ZeroMemory(d);
   d.year = year; d.mon = month; d.day = 1;
   datetime first = StructToTime(d);
   TimeToStruct(first, d);
   datetime t = first + ((7 - d.day_of_week) % 7) * 86400 + (n - 1) * 7 * 86400;
   if(n == 5) { MqlDateTime c; TimeToStruct(t, c); if(c.mon != month) t -= 7 * 86400; }
   return(t);
}
// US summer time: second Sunday of March 02:00 New York (07:00 UTC) to first Sunday of November 02:00 (06:00 UTC)
bool UsSummer(datetime utc)
{
   MqlDateTime d; TimeToStruct(utc, d);
   return(utc >= NthSunday(d.year, 3, 2) + 7 * 3600 && utc < NthSunday(d.year, 11, 1) + 6 * 3600);
}
// EU summer time: last Sunday of March 01:00 UTC to last Sunday of October 01:00 UTC
bool EuSummer(datetime utc)
{
   MqlDateTime d; TimeToStruct(utc, d);
   return(utc >= NthSunday(d.year, 3, 5) + 3600 && utc < NthSunday(d.year, 10, 5) + 3600);
}
// The server's offset at a past server time. The platform gives only the offset of now, and one offset for the whole
// history put every trade of the other daylight-saving season one hour off (Deepwater corrects only shifts above
// 90 minutes). Most brokers keep New York's 17:00 close at midnight (UTC+2 in winter, UTC+3 in US summer time) or
// follow Central European time (UTC+1 / UTC+2): when the offset measured now is the one such a rule gives now, a
// trade gets the rule's offset at its own date. Any other server keeps the offset measured now.
long OffsetAt(datetime server, long offNow)
{
   datetime now = TimeGMT();
   datetime utc = (datetime)((long)server - offNow);   // near a change this is off by the hour that is being decided
   if(offNow == (UsSummer(now) ? 3 : 2) * 3600) return((UsSummer(utc) ? 3 : 2) * 3600);
   if(offNow == (EuSummer(now) ? 2 : 1) * 3600) return((EuSummer(utc) ? 2 : 1) * 3600);
   return(offNow);
}

void WriteHistory()
{
   int total = OrdersHistoryTotal();
   long off = 0;
   if(!ServerOffset(off))
   {
      static bool told = false;
      if(!told) Print("Deepwater bridge: waiting for a live tick to measure the server time offset (market closed or offline?)");
      told = true;
      return;                                // g_lastCount stays as is: tried again at the next tick / timer
   }
   double deposits = 0;
   string rows = "";
   for(int i = 0; i < total; i++)
   {
      if(!OrderSelect(i, SELECT_BY_POS, MODE_HISTORY)) continue;
      int type = OrderType();
      if(type == 6) { deposits += OrderProfit(); continue; }        // balance operation (deposit / withdrawal)
      if(type != OP_BUY && type != OP_SELL) continue;               // pending orders, credit
      if(OrderCloseTime() == 0) continue;
      int digits = (int)MarketInfo(OrderSymbol(), MODE_DIGITS);
      if(digits <= 0) digits = 5;
      rows += IntegerToString(OrderTicket()) + ";" + OrderSymbol() + ";" + (type == OP_BUY ? "buy" : "sell") + ";"
            + Lots(OrderLots()) + ";"
            + IntegerToString((long)OrderOpenTime() - OffsetAt(OrderOpenTime(), off)) + ";" + Num(OrderOpenPrice(), digits) + ";"
            + IntegerToString((long)OrderCloseTime() - OffsetAt(OrderCloseTime(), off)) + ";" + Num(OrderClosePrice(), digits) + ";"
            + Num(OrderStopLoss(), digits) + ";" + Num(OrderTakeProfit(), digits) + ";"
            + Num(OrderCommission(), 2) + ";" + Num(OrderSwap(), 2) + ";" + Num(OrderProfit(), 2) + ";"
            + Clean(OrderComment()) + "\n";
   }
   string name = "Deepwater_MT4_" + IntegerToString(AccountNumber()) + ".csv";
   string tmp = name + ".tmp";
   int h = FileOpen(tmp, FILE_WRITE | FILE_TXT | FILE_ANSI | FILE_COMMON);
   if(h == INVALID_HANDLE) { Print("Deepwater bridge: cannot write ", tmp, " (error ", GetLastError(), ")"); return; }
   FileWriteString(h, "# Deepwater bridge 1\n");
   FileWriteString(h, "# platform=MT4;login=" + IntegerToString(AccountNumber()) + ";currency=" + AccountCurrency()
                    + ";broker=" + Clean(AccountCompany()) + ";server=" + Clean(AccountServer())
                    + ";deposits=" + Num(deposits, 2) + ";updated=" + IntegerToString((long)TimeGMT())
                    + ";offset=" + IntegerToString(off) + ";times=verified\n");   // offset checked above
   FileWriteString(h, "ticket;symbol;side;lots;open_utc;open_price;close_utc;close_price;sl;tp;commission;swap;profit;comment\n");
   FileWriteString(h, rows);
   FileClose(h);
   if(!FileMove(tmp, FILE_COMMON, name, FILE_COMMON | FILE_REWRITE))
   {
      Print("Deepwater bridge: cannot replace ", name, " (error ", GetLastError(), ")");
      return;                                // not written: tried again at the next timer
   }
   g_lastCount = total;
}
