// DeepwaterBridge.cs — read-only journal bridge for Deepwater (cTrader cBot).
// Writes the account's closed trades to <Documents>/Deepwater/Deepwater_cTrader_<account>.csv (or the folder
// given in the parameter). Deepwater watches that folder (Track Record › Import › Live sync).
// The cBot never opens, modifies or closes positions. It needs file system access to write the file.
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using cAlgo.API;

namespace cAlgo.Robots
{
    [Robot(TimeZone = TimeZones.UTC, AccessRights = AccessRights.FileSystem)]
    public class DeepwaterBridge : Robot
    {
        [Parameter("Folder (empty = Documents/Deepwater)", DefaultValue = "")]
        public string Folder { get; set; }

        private static readonly DateTime Epoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        protected override void OnStart()
        {
            Positions.Closed += args => WriteHistory();
            WriteHistory();
        }

        private static long Unix(DateTime t)
        {
            return (long)(DateTime.SpecifyKind(t, DateTimeKind.Utc) - Epoch).TotalSeconds;
        }

        private static string Clean(string s)
        {
            return (s ?? "").Replace(";", ",").Replace("\r", " ").Replace("\n", " ");
        }

        private void WriteHistory()
        {
            try
            {
                var dir = string.IsNullOrWhiteSpace(Folder)
                    ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Deepwater")
                    : Folder;
                Directory.CreateDirectory(dir);
                var inv = CultureInfo.InvariantCulture;
                var trades = History.OrderBy(h => h.ClosingTime).ToList();
                // starting balance: balance after the first closed trade minus its net result
                double deposits = trades.Count > 0 ? trades[0].Balance - trades[0].NetProfit : Account.Balance;
                var sb = new StringBuilder();
                sb.Append("# Deepwater bridge 1\n");
                // the robot runs in UTC (TimeZone = UTC above): its times need no offset
                sb.AppendFormat(inv, "# platform=cTrader;login={0};currency={1};broker={2};deposits={3:F2};updated={4};offset=0;times=verified\n",
                    Account.Number, Account.Asset.Name, Clean(Account.BrokerName), deposits, Unix(Server.Time));
                sb.Append("ticket;symbol;side;lots;open_utc;open_price;close_utc;close_price;sl;tp;commission;swap;profit;comment\n");
                foreach (var h in trades)
                {
                    var symbol = Symbols.GetSymbol(h.SymbolName);
                    double lots = symbol != null ? symbol.VolumeInUnitsToQuantity(h.VolumeInUnits) : h.Quantity;
                    int digits = symbol != null ? symbol.Digits : 5;
                    string fmt = "F" + digits;
                    sb.Append(h.ClosingDealId.ToString(inv)).Append(';')
                      .Append(h.SymbolName).Append(';')
                      .Append(h.TradeType == TradeType.Buy ? "buy" : "sell").Append(';')
                      .Append(lots.ToString("0.########", inv)).Append(';')   // 0.001 lot was written 0
                      .Append(Unix(h.EntryTime).ToString(inv)).Append(';')
                      .Append(h.EntryPrice.ToString(fmt, inv)).Append(';')
                      .Append(Unix(h.ClosingTime).ToString(inv)).Append(';')
                      .Append(h.ClosingPrice.ToString(fmt, inv)).Append(";;;")      // stop / target are not in the history
                      .Append(h.Commissions.ToString("F2", inv)).Append(';')
                      .Append(h.Swap.ToString("F2", inv)).Append(';')
                      .Append(h.GrossProfit.ToString("F2", inv)).Append(';')
                      .Append(Clean(h.Label)).Append('\n');
                }
                var name = Path.Combine(dir, "Deepwater_cTrader_" + Account.Number.ToString(inv) + ".csv");
                var tmp = name + ".tmp";
                File.WriteAllText(tmp, sb.ToString(), new UTF8Encoding(false));
                // replaced in one step: with delete then move there was a moment with no file (Deepwater could see
                // the account's history gone) and a crash in between lost it
                if (File.Exists(name)) File.Replace(tmp, name, null);
                else File.Move(tmp, name);
            }
            catch (Exception e)
            {
                Print("Deepwater bridge: " + e.Message);
            }
        }
    }
}
