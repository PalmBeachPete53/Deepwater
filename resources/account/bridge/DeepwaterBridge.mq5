//+------------------------------------------------------------------+
//| DeepwaterBridge.mq5 — read-only journal bridge for Deepwater     |
//| Writes the account's closed positions to                         |
//|   <Common data folder>/Files/Deepwater_MT5_<login>.csv            |
//| Deepwater watches that folder (Track Record › Import › Live sync).|
//| The EA never sends, modifies or closes orders.                    |
//+------------------------------------------------------------------+
#property copyright "Deepwater"
#property version   "1.20"
#property description "Read-only: exports closed positions for the Deepwater journal. Sends no orders."

input int ScanSeconds = 5;   // how often the history is checked

int g_lastDeals = -1;

int OnInit()
{
   EventSetTimer(MathMax(1, ScanSeconds));
   WriteHistory();
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) { EventKillTimer(); }

void OnTimer()
{
   if(!HistorySelect(0, TimeTradeServer() + 86400)) return;
   if(HistoryDealsTotal() != g_lastDeals) WriteHistory();
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

// One closed position, built from its deals. In netting mode a reversal (DEAL_ENTRY_INOUT) closes the position
// and opens the opposite one under the same position id: each such "leg" is exported as its own trade, with the
// ticket "<position id>" for the first leg and "<position id>-<n>" for the next ones.
struct Pos
{
   long     id;
   int      leg;                 // 1, 2, … within the position id
   string   symbol;
   bool     buy;
   double   volIn, volOut, priceIn, priceOutSum;
   datetime openTime, closeTime;
   double   sl, tp, commission, swap, profit;
   string   comment;
};

// One line of the file for a closed leg (times moved from server time to UTC: off is the offset now, OffsetAt the
// offset at each time).
string Row(const Pos &p, long off)
{
   int digits = (int)SymbolInfoInteger(p.symbol, SYMBOL_DIGITS);
   if(digits <= 0) digits = 5;
   string ticket = IntegerToString(p.id);
   if(p.leg > 1) ticket += "-" + IntegerToString(p.leg);
   return(ticket + ";" + p.symbol + ";" + (p.buy ? "buy" : "sell") + ";"
          + Lots(p.volIn) + ";"
          + IntegerToString((long)p.openTime - OffsetAt(p.openTime, off)) + ";" + Num(p.priceIn, digits) + ";"
          + IntegerToString((long)p.closeTime - OffsetAt(p.closeTime, off)) + ";" + Num(p.priceOutSum / MathMax(1e-12, p.volOut), digits) + ";"
          + Num(p.sl, digits) + ";" + Num(p.tp, digits) + ";"
          + Num(p.commission, 2) + ";" + Num(p.swap, 2) + ";" + Num(p.profit, 2) + ";"
          + Clean(p.comment) + "\n");
}

void WriteHistory()
{
   if(!HistorySelect(0, TimeTradeServer() + 86400)) return;
   int total = HistoryDealsTotal();
   // server time minus UTC (TimeTradeServer keeps running on weekends), rounded to 15 minutes
   long off = (long)MathRound((double)(TimeTradeServer() - TimeGMT()) / 900.0) * 900;
   // TimeTradeServer() is derived from the last server time the terminal received: only trusted while connected
   bool verified = TerminalInfoInteger(TERMINAL_CONNECTED) != 0 && off >= -12 * 3600 && off <= 14 * 3600;
   double deposits = 0;
   string rows = "";
   Pos list[];
   int n = 0;
   for(int i = 0; i < total; i++)
   {
      ulong deal = HistoryDealGetTicket(i);
      if(deal == 0) continue;
      long type = HistoryDealGetInteger(deal, DEAL_TYPE);
      if(type == DEAL_TYPE_BALANCE) { deposits += HistoryDealGetDouble(deal, DEAL_PROFIT); continue; }
      if(type != DEAL_TYPE_BUY && type != DEAL_TYPE_SELL) continue;
      long pid = HistoryDealGetInteger(deal, DEAL_POSITION_ID);
      long entry = HistoryDealGetInteger(deal, DEAL_ENTRY);
      int k = -1;
      for(int j = n - 1; j >= 0; j--) if(list[j].id == pid) { k = j; break; }
      if(k < 0)
      {
         n++; ArrayResize(list, n, 64); k = n - 1;
         list[k].id = pid; list[k].leg = 1; list[k].symbol = HistoryDealGetString(deal, DEAL_SYMBOL);
         list[k].buy = (type == DEAL_TYPE_BUY); list[k].volIn = 0; list[k].volOut = 0; list[k].priceIn = 0;
         list[k].priceOutSum = 0; list[k].openTime = 0; list[k].closeTime = 0; list[k].sl = 0; list[k].tp = 0;
         list[k].commission = 0; list[k].swap = 0; list[k].profit = 0; list[k].comment = "";
      }
      double vol = HistoryDealGetDouble(deal, DEAL_VOLUME);
      double price = HistoryDealGetDouble(deal, DEAL_PRICE);
      datetime t = (datetime)HistoryDealGetInteger(deal, DEAL_TIME);
      if(entry == DEAL_ENTRY_INOUT)
      {
         // reversal: the part of the volume that closes the open leg ends it (with this deal's profit, which is
         // the closed part's result), the rest opens the next leg in the deal's direction
         double openVol = list[k].volIn - list[k].volOut;
         double closing = MathMin(MathMax(openVol, 0.0), vol);
         list[k].commission += HistoryDealGetDouble(deal, DEAL_COMMISSION) + HistoryDealGetDouble(deal, DEAL_FEE);
         list[k].swap += HistoryDealGetDouble(deal, DEAL_SWAP);
         list[k].profit += HistoryDealGetDouble(deal, DEAL_PROFIT);
         if(closing > 0)
         {
            list[k].priceOutSum += price * closing;
            list[k].volOut += closing;
            list[k].closeTime = t;
            rows += Row(list[k], off);                       // this leg is closed now
         }
         list[k].leg++;
         list[k].buy = (type == DEAL_TYPE_BUY);
         list[k].volIn = vol - closing; list[k].volOut = 0; list[k].priceIn = price; list[k].priceOutSum = 0;
         list[k].openTime = t; list[k].closeTime = 0;
         list[k].sl = HistoryDealGetDouble(deal, DEAL_SL); list[k].tp = HistoryDealGetDouble(deal, DEAL_TP);
         list[k].commission = 0; list[k].swap = 0; list[k].profit = 0;
         list[k].comment = HistoryDealGetString(deal, DEAL_COMMENT);
         continue;
      }
      list[k].commission += HistoryDealGetDouble(deal, DEAL_COMMISSION) + HistoryDealGetDouble(deal, DEAL_FEE);
      list[k].swap += HistoryDealGetDouble(deal, DEAL_SWAP);
      list[k].profit += HistoryDealGetDouble(deal, DEAL_PROFIT);
      if(entry == DEAL_ENTRY_IN)
      {
         list[k].priceIn = (list[k].priceIn * list[k].volIn + price * vol) / MathMax(1e-12, list[k].volIn + vol);
         list[k].volIn += vol;
         if(list[k].openTime == 0) list[k].openTime = t;
         list[k].buy = (type == DEAL_TYPE_BUY);
         list[k].sl = HistoryDealGetDouble(deal, DEAL_SL);
         list[k].tp = HistoryDealGetDouble(deal, DEAL_TP);
         list[k].comment = HistoryDealGetString(deal, DEAL_COMMENT);
      }
      else if(entry == DEAL_ENTRY_OUT || entry == DEAL_ENTRY_OUT_BY)
      {
         list[k].priceOutSum += price * vol;
         list[k].volOut += vol;
         list[k].closeTime = t;
      }
   }
   for(int k = 0; k < n; k++)
   {
      if(list[k].volIn <= 0 || list[k].volOut + 1e-9 < list[k].volIn || list[k].closeTime == 0) continue;   // still open
      rows += Row(list[k], off);
   }
   long login = AccountInfoInteger(ACCOUNT_LOGIN);
   string name = "Deepwater_MT5_" + IntegerToString(login) + ".csv";
   string tmp = name + ".tmp";
   int h = FileOpen(tmp, FILE_WRITE | FILE_TXT | FILE_ANSI | FILE_COMMON);
   if(h == INVALID_HANDLE) { Print("Deepwater bridge: cannot write ", tmp, " (error ", GetLastError(), ")"); return; }
   FileWriteString(h, "# Deepwater bridge 1\n");
   FileWriteString(h, "# platform=MT5;login=" + IntegerToString(login) + ";currency=" + AccountInfoString(ACCOUNT_CURRENCY)
                    + ";broker=" + Clean(AccountInfoString(ACCOUNT_COMPANY)) + ";server=" + Clean(AccountInfoString(ACCOUNT_SERVER))
                    + ";deposits=" + Num(deposits, 2) + ";updated=" + IntegerToString((long)TimeGMT())
                    + ";offset=" + IntegerToString(off) + (verified ? ";times=verified" : "") + "\n");
   FileWriteString(h, "ticket;symbol;side;lots;open_utc;open_price;close_utc;close_price;sl;tp;commission;swap;profit;comment\n");
   FileWriteString(h, rows);
   FileClose(h);
   if(!FileMove(tmp, FILE_COMMON, name, FILE_COMMON | FILE_REWRITE))
   {
      Print("Deepwater bridge: cannot replace ", name, " (error ", GetLastError(), ")");
      return;                                // not written: tried again at the next timer
   }
   g_lastDeals = total;
}
