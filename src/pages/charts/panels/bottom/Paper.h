#pragma once
// Charts tab — bottom panel, Paper trading. Port of quant-terminal public/js/panels.js (replayPane, freshAccount,
// paper, livePaper, price, paperSymbols, fill, checkOrders, equity, drawPaperNumbers, renderPaper,
// checkReplayOrders, onReplayBar) (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
//
// While the active chart is in bar replay, paper trading uses a separate practice account (state.replayPaper) that
// fills at the replayed bar's prices, so replay trades never mix with the live paper account (state.paper).
// Accounts are the JS JSON: { initial, cash, feePct, positions: { sym: { qty, avg } }, orders: [...], history: [...] }.
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <optional>

class QLineEdit;
class QComboBox;
class QLabel;
class QVBoxLayout;

namespace charts::ui {
class BottomPanel;
class ChartPane;
namespace bottom {
class StatsGrid;
class Table;

class PaperTab : public QObject {
  Q_OBJECT
public:
  explicit PaperTab(BottomPanel* panel);
  void render();                                          // renderPaper
  void onPrice(const QString& sym, double px);            // app.priceListeners entry
  void onReplayBar(ChartPane* pane, const QJsonObject& bar);
  QStringList paperSymbols() const;
  QStringList orderSymbols() const;                       // the working orders' symbols, without duplicates
  void place(const QString& side);                        // the Buy / Sell buttons

private:
  ChartPane* replayPane() const;
  QString accountKey() const;                             // "replayPaper" | "paper"
  QJsonObject paper();                                    // the current account (created if missing, JS ||=)
  void setPaper(const QJsonObject& p, const QString& key = {});
  std::optional<double> price(const QString& sym) const;
  // acctKey empty: the current account (and the replay bar's time when replaying), else that account.
  void fill(const QString& sym, const QString& side, double qty, double px, const QString& note = {},
            const QString& acctKey = {});
  void checkOrders(const QString& sym, double px);
  void checkReplayOrders(ChartPane* pane, const QJsonObject& bar);
  double equity();
  void drawPaperNumbers();
  // The order form as it was when Buy / Sell was pressed (read before any wait for a price: the form may be rebuilt
  // or edited meanwhile).
  struct OrderForm {
    QString type = "market";
    double amt = 0, sl = 0, tp = 0, price = 0;
  };
  OrderForm readForm() const;
  void placeAt(const QString& side, const QString& sym, double cur, const OrderForm& f);
  // The stop loss / take profit of a filled entry (side: the entry's), sized with the filled qty, priced from the
  // fill. A limit / stop entry carries its sl / tp until it fills (checkOrders, checkReplayOrders).
  void addBracket(const QString& acctKey, const QString& sym, const QString& side, double qty, double entryPx,
                  double sl, double tp, const QString& baseId);
  void renderRight();   // equity, positions, working orders, history (the right column)
  // After a fill, a placed or cancelled order, a replayed bar: the right column and the ticket's price only, so the
  // order ticket being typed is not rebuilt (render() did that up to four times a second while replay played).
  void refresh();

  // The ticket as typed, kept across a full render() (tab switch, collapse, theme, active chart change).
  struct TicketDraft {
    QString type = "market", price, amt = "1000", sl, tp, init;
    QString sym, acct;        // the price typed belongs to that symbol, the starting balance to that account
    bool priceTyped = false;  // false: the price field follows the current price
  } draft_;
  QPointer<QLabel> title_;
  QPointer<QWidget> right_;
  QVBoxLayout* rightLay_ = nullptr;

  BottomPanel* P;
  qint64 paperRenderAt_ = 0;
  QPointer<QWidget> body_;
  QPointer<StatsGrid> eq_;
  QString eqShown_;   // the stats eq_ shows (texts and colours): a tick that changes none of them redraws nothing
  QPointer<Table> pos_;
  QPointer<QComboBox> type_;
  QPointer<QLineEdit> price_, amt_, sl_, tp_, init_, fee_;
};

}  // namespace bottom
}  // namespace charts::ui
