// Charts tab — Paper trading. Port of quant-terminal public/js/panels.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Paper.h"
#include "charts/data/Providers.h"
#include "charts/core/Backtest.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Panels.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/SidePanel.h"
#include "pages/charts/panels/bottom/Common.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

namespace charts::ui::bottom {

static double sgn(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }   // Math.sign
static QJsonObject freshAccount() {
  return {{"initial", 100000}, {"cash", 100000}, {"feePct", 0.05}, {"positions", QJsonObject()},
          {"orders", QJsonArray()}, {"history", QJsonArray()}};
}
static double num(const QJsonObject& o, const char* k) { return o.value(k).toDouble(); }
// An order's side or type as shown (the saved value stays "buy", "sell", "market", "limit", "stop").
static QString word(const QString& w) {
  if (w == "buy") return QCoreApplication::translate("charts", "buy");
  if (w == "sell") return QCoreApplication::translate("charts", "sell");
  if (w == "market") return QCoreApplication::translate("charts", "market");
  if (w == "limit") return QCoreApplication::translate("charts", "limit");
  if (w == "stop") return QCoreApplication::translate("charts", "stop");
  return w;
}

// "ORDER TICKET · BTCUSDT 91,234.50" (the price muted), or the plain title while no price is known.
static void setTicketTitle(QLabel* title, const QString& sym, const std::optional<double>& px) {
  const QString t = QCoreApplication::translate("charts", "Order ticket") + " · " + ticker(sym);
  if (!px) {
    title->setTextFormat(Qt::PlainText);
    title->setText(t.toUpper());   // style::h4 upper-cases its text (CSS text-transform)
    return;
  }
  title->setTextFormat(Qt::RichText);
  title->setText(fmt::esc(t.toUpper()) + QString(" <span style=\"color:%1\">%2</span>").arg(style::v().muted.name(), fmtPrice(*px)));
}

PaperTab::PaperTab(BottomPanel* panel) : QObject(panel), P(panel) {}

ChartPane* PaperTab::replayPane() const {
  ChartPane* a = P->app->active;
  return a && a->replay ? a : nullptr;
}
QString PaperTab::accountKey() const { return replayPane() ? "replayPaper" : "paper"; }

QJsonObject PaperTab::paper() {
  const QString k = accountKey();
  if (!P->app->state.value(k).isObject()) P->app->state[k] = freshAccount();   // ||=
  return P->app->state.value(k).toObject();
}
void PaperTab::setPaper(const QJsonObject& p, const QString& key) {
  P->app->state[key.isEmpty() ? accountKey() : key] = p;
}

std::optional<double> PaperTab::price(const QString& s) const {
  ChartPane* rp = replayPane();
  // the replayed bars are the market they were loaded for: right after a symbol switch cfg already names the new one
  if (rp && s == barsKey(rp).symbol) {
    const int i = rp->replay->index;
    if (i >= 0 && i < rp->bars.size()) return rp->bars[i].close;
    return std::nullopt;
  }
  if (rp) return std::nullopt;
  if (P->app->prices.contains(s)) return P->app->prices.value(s);
  return std::nullopt;
}

QStringList PaperTab::paperSymbols() const {
  const QJsonObject p = P->app->state.value("paper").toObject();
  if (p.isEmpty()) return {};
  QStringList out = p.value("positions").toObject().keys();
  for (const auto& o : p.value("orders").toArray()) out << o.toObject().value("symbol").toString();
  return out;
}

QStringList PaperTab::orderSymbols() const {
  QStringList out;
  for (const auto& o : P->app->state.value("paper").toObject().value("orders").toArray())
    out << o.toObject().value("symbol").toString();
  out.removeDuplicates();
  out.removeAll(QString());
  return out;
}

void PaperTab::fill(const QString& sym, const QString& side, double qty, double px, const QString& note,
                    const QString& acctKey) {
  const QString key = acctKey.isEmpty() ? accountKey() : acctKey;
  QJsonObject p = acctKey.isEmpty() ? paper() : P->app->state.value(key).toObject();
  ChartPane* rp = acctKey.isEmpty() ? replayPane() : nullptr;
  const double signedQ = side == "buy" ? qty : -qty;
  QJsonObject positions = p.value("positions").toObject();
  const QJsonObject pos = positions.contains(sym) ? positions.value(sym).toObject() : QJsonObject{{"qty", 0}, {"avg", 0}};
  const double pq = num(pos, "qty"), pavg = num(pos, "avg");
  const double fee = std::fabs(qty * px) * (num(p, "feePct") / 100);
  double realized = 0;
  if (pq != 0 && sgn(pq) != sgn(signedQ)) {
    const double closing = sgn(signedQ) * std::min(std::fabs(signedQ), std::fabs(pq));
    realized = -closing * (px - pavg);
  }
  const double nq = pq + signedQ;
  if (nq == 0) positions.remove(sym);
  else if (pq == 0 || sgn(nq) != sgn(pq)) positions[sym] = QJsonObject{{"qty", nq}, {"avg", px}};
  else if (sgn(signedQ) == sgn(pq)) positions[sym] = QJsonObject{{"qty", nq}, {"avg", (pq * pavg + signedQ * px) / nq}};
  else positions[sym] = QJsonObject{{"qty", nq}, {"avg", pavg}};
  p["positions"] = positions;
  p["cash"] = num(p, "cash") - (signedQ * px + fee);
  QJsonArray history = p.value("history").toArray();
  // the replay index is checked as in price(): it can sit past the bars while they reload
  const int ri = rp ? rp->replay->index : -1;
  const double time = rp && ri >= 0 && ri < rp->bars.size() ? double(rp->bars[ri].time) * 1000 : double(QDateTime::currentMSecsSinceEpoch());
  history.prepend(QJsonObject{{"time", time}, {"symbol", sym}, {"side", side}, {"qty", qty}, {"price", px},
                              {"fee", fee}, {"realized", realized - fee}, {"note", note}});
  while (history.size() > 500) history.removeLast();
  p["history"] = history;
  setPaper(p, key);
  P->app->persist();
  P->app->toast(QCoreApplication::translate("charts", "Paper %1 %2 %3 at %4").arg(word(side), fmtNum(qty, 4), fmt::esc(ticker(sym)), fmtPrice(px)));
}

void PaperTab::checkOrders(const QString& sym, double px) {
  QJsonObject p = P->app->state.value("paper").toObject();
  if (p.isEmpty() || p.value("orders").toArray().isEmpty()) return;
  bool changed = false;
  const QJsonArray snapshot = p.value("orders").toArray();
  for (const auto& ov : snapshot) {
    const QJsonObject o = ov.toObject();
    if (o.value("symbol").toString() != sym) continue;
    const double op = num(o, "price");
    const bool buy = o.value("side").toString() == "buy";
    const bool limit = o.value("type").toString() == "limit";
    const bool hit = limit ? (buy ? px <= op : px >= op) : (buy ? px >= op : px <= op);
    if (!hit) continue;
    // an earlier fill of this loop may have taken it out (its bracket partner): it must not fill too. Checked only
    // after a fill of this call: before one the orders are the snapshot (this runs on every tick of the symbol).
    if (changed && !P->app->state.value("paper").toObject().value("orders").toArray().contains(o)) continue;
    QJsonArray keep;
    const QJsonValue oco = o.value("oco");
    for (const auto& x : P->app->state.value("paper").toObject().value("orders").toArray()) {
      const QJsonObject xo = x.toObject();
      if (xo == o) continue;
      if (bt::jsTruthy(oco) && xo.value("oco") == oco) continue;
      keep.append(xo);
    }
    QJsonObject cur = P->app->state.value("paper").toObject();
    cur["orders"] = keep;
    P->app->state["paper"] = cur;
    // A limit fills at the first price that reaches it, never worse than its limit: a marketable limit (a buy above
    // the market) fills at the market, as the replay path does with the bar's open.
    const double fillPx = limit ? (buy ? std::min(op, px) : std::max(op, px)) : px;
    fill(sym, o.value("side").toString(), num(o, "qty"), fillPx,
         o.value("note").toString().isEmpty() ? o.value("type").toString() + " order" : o.value("note").toString(),
         "paper");
    addBracket("paper", sym, o.value("side").toString(), num(o, "qty"), fillPx, num(o, "sl"), num(o, "tp"),
               o.value("id").toString());
    changed = true;
    // fill()'s toast is drawn on the Charts page: from another page (orders are watched while it is hidden,
    // SidePanel::syncQuoteTimer) the fill would go unseen
    if (!P->app->isVisible())
      P->app->systemNotify(QCoreApplication::translate("charts", "Paper %1 %2 %3 at %4")
                               .arg(word(o.value("side").toString()), fmtNum(num(o, "qty"), 4), ticker(sym), fmtPrice(fillPx)),
                           QCoreApplication::translate("charts", "Deepwater · Paper trading"));
  }
  if (changed) refresh();
}

void PaperTab::onPrice(const QString& sym, double px) {
  checkOrders(sym, px);
  if (P->isOpen("paper") && P->app->state.value("paper").toObject().value("positions").toObject().contains(sym))
    drawPaperNumbers();
}

double PaperTab::equity() {
  const QJsonObject p = paper();
  double e = num(p, "cash");
  const QJsonObject pos = p.value("positions").toObject();
  for (auto it = pos.begin(); it != pos.end(); ++it) {
    const QJsonObject x = it.value().toObject();
    e += num(x, "qty") * price(it.key()).value_or(num(x, "avg"));
  }
  return e;
}

void PaperTab::drawPaperNumbers() {
  if (!body_ || P->body() != body_) return;
  const QJsonObject p = paper();
  const double eq = equity(), initial = num(p, "initial");
  if (eq_) {
    // Called on every tick of a held symbol: the three stat frames (styled widgets) are rebuilt only when a shown
    // text or colour changed, most ticks move the price below the displayed precision.
    const QString eqText = fmtPrice(eq), cashText = fmtPrice(num(p, "cash"));
    const QString pnlText = fmtPrice(eq - initial) + " (" + fmtPct((eq - initial) / initial * 100) + ")";
    const QString pnlCls = eq >= initial ? "up" : "down";
    const QString shown = eqText + '\n' + cashText + '\n' + pnlText + '\n' + pnlCls;
    if (shown != eqShown_) {
      eqShown_ = shown;
      eq_->clear();
      eq_->addStat(QCoreApplication::translate("charts", "Equity"), eqText);
      eq_->addStat(QCoreApplication::translate("charts", "Cash"), cashText);
      eq_->addStat(QCoreApplication::translate("charts", "Total P&L"), pnlText, pnlCls);
    }
  }
  if (pos_) {
    QList<Row> rows;
    const QJsonObject pos = p.value("positions").toObject();
    for (auto it = pos.begin(); it != pos.end(); ++it) {
      const QJsonObject x = it.value().toObject();
      const double qty = num(x, "qty"), avg = num(x, "avg");
      const double px = price(it.key()).value_or(avg), pnl = qty * (px - avg);
      Row r;
      r.data = it.key();
      Cell close{QCoreApplication::translate("charts", "Close", "close a position")};
      close.button = true;
      close.data = it.key();
      r.cells = {Cell{ticker(it.key()), {}, false, false, true},
                 {qty > 0 ? QCoreApplication::translate("charts", "Long") : QCoreApplication::translate("charts", "Short")},
                 {fmtNum(std::fabs(qty), 4)},
                 {fmtPrice(avg)},
                 {fmtPrice(px)},
                 Cell{fmtPrice(pnl) + " (" + fmtPct((px - avg) / avg * 100 * sgn(qty)) + ")", pnl >= 0 ? "up" : "down"},
                 close};
      rows << r;
    }
    if (rows.isEmpty()) {
      Row r;
      r.span = true;
      r.cells = {Cell{QCoreApplication::translate("charts", "No open positions."), "muted", true}};
      rows << r;
    }
    if (rows != pos_->rows()) pos_->setRows(rows);   // same rows: no re-measure, no repaint, the hover stays
  }
}

void PaperTab::render() {
  const QJsonObject p = paper();
  ChartPane* a = P->app->active;
  const QString sym = a ? a->cfg.value("symbol").toString() : QString();
  std::optional<double> px = price(sym);
  // the chart's last close only when its bars are this symbol's (not while a switch loads)
  if (!px && a && !a->bars.isEmpty() && barsKey(a).symbol == sym) px = a->bars.last().close;
  // a price typed for another symbol, a starting balance typed for the other account, don't carry over
  if (draft_.sym != sym) {
    draft_.sym = sym;
    draft_.priceTyped = false;
  }
  if (draft_.acct != accountKey()) {
    draft_.acct = accountKey();
    draft_.init.clear();
  }
  QWidget* body = P->newBody();
  body_ = body;
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  TwoCol tc = twoCol(body);
  bl->addWidget(tc.root);
  Col left{tc.leftLay};
  const auto& V = style::v();
  auto* title = h4(QCoreApplication::translate("charts", "Order ticket") + " · " + ticker(sym));
  title_ = title;
  setTicketTitle(title, sym, px);
  left.add(title, 0, kH4Bottom);
  QComboBox* type = optSelect({{QCoreApplication::translate("charts", "Market", "order type"), "market"}, {QCoreApplication::translate("charts", "Limit"), "limit"}, {QCoreApplication::translate("charts", "Stop"), "stop"}}, draft_.type, false);
  type->setProperty("qtc", "in");
  type->setFixedHeight(28);
  type_ = type;
  price_ = numInput(draft_.priceTyped ? draft_.price : px ? jsPrecision(*px, 6) : QString(), 110);
  amt_ = numInput(draft_.amt, 100);
  left.add(rowFlex({style::field(QCoreApplication::translate("charts", "Type"), type), style::field(QCoreApplication::translate("charts", "Price"), price_), style::field(QCoreApplication::translate("charts", "Amount ($)"), amt_)}));
  sl_ = numInput(draft_.sl, 90);
  tp_ = numInput(draft_.tp, 90);
  left.add(rowFlex({style::field(QCoreApplication::translate("charts", "Stop loss % (optional)"), sl_), style::field(QCoreApplication::translate("charts", "Take profit % (optional)"), tp_)}), 6);
  auto* buy = style::btn(QCoreApplication::translate("charts", "Buy"), "primary");
  auto* sell = style::btn(QCoreApplication::translate("charts", "Sell / short"), "primary");
  auto colored = [&](QPushButton* b, const QColor& c) {
    b->setStyleSheet(QString("QPushButton{background:%1;border-color:%1;}").arg(c.name(QColor::HexArgb)));
  };
  colored(buy, V.up);
  colored(sell, V.down);
  left.add(rowFlex({buy, sell}), 10);
  left.add(h4(QCoreApplication::translate("charts", "Account")), kH4Top, kH4Bottom);
  init_ = numInput(draft_.init.isEmpty() ? jsNum(num(p, "initial")) : draft_.init, 110);
  fee_ = numInput(jsNum(num(p, "feePct")), 80);
  auto* reset = style::btn(QCoreApplication::translate("charts", "Reset account"), "danger");
  alignEnd(reset);
  left.add(rowFlex({style::field(QCoreApplication::translate("charts", "Starting balance"), init_), style::field(QCoreApplication::translate("charts", "Fee % per trade"), fee_), reset}));
  left.add(hint(replayPane() ? QCoreApplication::translate("charts", "<b>Replay trading.</b> Orders fill at the replayed bars' prices in a separate practice "
                                 "account. Step or play the replay to fill limit and stop orders. Exit replay to go back "
                                 "to your live paper account.")
                             : QCoreApplication::translate("charts", "Simulated fills at the live price. No real money, no exchange connection.")),
           kPMargin, kPMargin);
  left.finish();

  // right column (flex column, 10 px gap: the h4 margins add to it), rebuilt alone by refresh()
  auto* right = new QWidget;
  rightLay_ = new QVBoxLayout(right);
  rightLay_->setContentsMargins(0, 0, 0, 0);
  rightLay_->setSpacing(tc.rightLay->spacing());
  right_ = right;
  tc.rightLay->addWidget(right);
  tc.rightLay->addStretch(1);
  renderRight();

  // the ticket as typed survives the next render() (textEdited for the price: only the user's typing pins it)
  connect(type, &QComboBox::activated, this, [this, type] { draft_.type = optValue(type); });
  connect(price_, &QLineEdit::textEdited, this, [this](const QString& t) {
    draft_.price = t;
    draft_.priceTyped = !t.trimmed().isEmpty();
  });
  connect(amt_, &QLineEdit::textChanged, this, [this](const QString& t) { draft_.amt = t; });
  connect(sl_, &QLineEdit::textChanged, this, [this](const QString& t) { draft_.sl = t; });
  connect(tp_, &QLineEdit::textChanged, this, [this](const QString& t) { draft_.tp = t; });
  connect(init_, &QLineEdit::textEdited, this, [this](const QString& t) { draft_.init = t; });
  connect(buy, &QPushButton::clicked, this, [this] { place("buy"); });
  connect(sell, &QPushButton::clicked, this, [this] { place("sell"); });
  connect(reset, &QPushButton::clicked, this, [this] {
    if (!confirm(P->window(), QCoreApplication::translate("charts", "Reset the paper account? Positions, orders and history are cleared."))) return;
    const double init = inputNumber(init_);
    const double fee = inputNumber(fee_);
    const double i0 = std::isfinite(init) && init ? init : 100000;
    QJsonObject acct = freshAccount();
    acct["initial"] = i0;
    acct["cash"] = i0;
    acct["feePct"] = std::isfinite(fee) && fee ? fee : 0;
    P->app->state[accountKey()] = acct;
    P->app->persist();
    draft_.init.clear();   // the field shows the account's balance again
    render();
  });
  connect(fee_, &QLineEdit::editingFinished, this, [this] {   // onchange
    QJsonObject acct = paper();
    const double fee = inputNumber(fee_);
    acct["feePct"] = std::isfinite(fee) && fee ? fee : 0;
    setPaper(acct);
    P->app->persist();
  });
}

void PaperTab::renderRight() {
  if (!right_) return;
  while (QLayoutItem* it = rightLay_->takeAt(0)) {
    if (it->widget()) it->widget()->deleteLater();
    delete it;
  }
  const QJsonObject p = paper();
  auto rh4 = [&](const QString& t) {
    QLabel* l = h4(t);
    l->setContentsMargins(0, kH4Top, 0, kH4Bottom);
    return l;
  };
  auto* stats = new StatsGrid(130);
  eq_ = stats;
  eqShown_.clear();   // a new, empty grid: drawPaperNumbers() fills it
  rightLay_->addWidget(stats);
  rightLay_->addWidget(rh4(QCoreApplication::translate("charts", "Positions")));
  auto* pos = new Table;
  TableStyle ts;
  ts.wrap = true;
  pos->setStyle(ts);
  pos->setHeader({{QCoreApplication::translate("charts", "Symbol")}, {QCoreApplication::translate("charts", "Side")}, {QCoreApplication::translate("charts", "Qty")}, {QCoreApplication::translate("charts", "Avg price")}, {QCoreApplication::translate("charts", "Last")}, {QCoreApplication::translate("charts", "P&L")}, {QString()}});
  pos_ = pos;
  rightLay_->addWidget(pos);
  rightLay_->addWidget(rh4(QCoreApplication::translate("charts", "Working orders")));
  auto* orders = new Table;
  orders->setStyle(ts);
  orders->setHeader({{QCoreApplication::translate("charts", "Symbol")}, {QCoreApplication::translate("charts", "Side")}, {QCoreApplication::translate("charts", "Type")}, {QCoreApplication::translate("charts", "Price")}, {QCoreApplication::translate("charts", "Qty")}, {QString()}});
  QList<Row> orows;
  for (const auto& ov : p.value("orders").toArray()) {
    const QJsonObject o = ov.toObject();
    Cell cancel{QCoreApplication::translate("charts", "Cancel")};
    cancel.button = true;
    cancel.data = o.value("id").toString();
    // a limit / stop entry that will place its stop loss / take profit when it fills is a bracket too
    const bool bracket = bt::jsTruthy(o.value("oco")) || num(o, "sl") > 0 || num(o, "tp") > 0;
    orows << Row{{Cell{ticker(o.value("symbol").toString()), {}, false, false, true},
                  {word(o.value("side").toString())},
                  {word(o.value("type").toString()) + (bracket ? QCoreApplication::translate("charts", " (bracket)") : QString())},
                  {fmtPrice(num(o, "price"))},
                  {fmtNum(num(o, "qty"), 4)},
                  cancel}};
  }
  if (orows.isEmpty()) {
    Row r;
    r.span = true;
    r.cells = {Cell{QCoreApplication::translate("charts", "None."), "muted", true}};
    orows << r;
  }
  orders->setRows(orows);
  rightLay_->addWidget(orders);
  rightLay_->addWidget(rh4(QCoreApplication::translate("charts", "History")));
  auto* hist = new Table;
  hist->setStyle(ts);
  hist->setHeader({{QCoreApplication::translate("charts", "Time")}, {QCoreApplication::translate("charts", "Symbol")}, {QCoreApplication::translate("charts", "Side")}, {QCoreApplication::translate("charts", "Qty")}, {QCoreApplication::translate("charts", "Price")}, {QCoreApplication::translate("charts", "Realised")}});
  QList<Row> hrows;
  const QJsonArray h = p.value("history").toArray();
  for (int i = 0; i < h.size() && i < 100; ++i) {
    const QJsonObject x = h[i].toObject();
    const double re = num(x, "realized");
    hrows << Row{{{localeDateTime(qint64(num(x, "time")))},
                  {ticker(x.value("symbol").toString())},
                  {word(x.value("side").toString())},
                  {fmtNum(num(x, "qty"), 4)},
                  {fmtPrice(num(x, "price"))},
                  Cell{re ? fmtPrice(re) : QString(), re >= 0 ? "up" : "down"}}};
  }
  hist->setRows(hrows);
  rightLay_->addWidget(hist);
  drawPaperNumbers();

  connect(pos, &Table::buttonClicked, this, [this](int, int, const QString& close) {
    QJsonObject acct = paper();
    const QJsonObject x = acct.value("positions").toObject().value(close).toObject();
    if (x.isEmpty()) return;
    const double qty = num(x, "qty");
    const double px2 = price(close).value_or(num(x, "avg"));
    fill(close, qty > 0 ? "sell" : "buy", std::fabs(qty), px2, "closed");
    acct = paper();
    QJsonArray keep;
    for (const auto& o : acct.value("orders").toArray())
      if (o.toObject().value("symbol").toString() != close) keep << o;
    acct["orders"] = keep;
    setPaper(acct);
    P->app->persist();
    refresh();
  });
  connect(orders, &Table::buttonClicked, this, [this](int, int, const QString& id) {
    QJsonObject acct = paper();
    QJsonArray keep;
    for (const auto& o : acct.value("orders").toArray())
      if (o.toObject().value("id").toString() != id) keep << o;
    acct["orders"] = keep;
    setPaper(acct);
    P->app->persist();
    refresh();
  });
}

void PaperTab::place(const QString& side) {
  ChartPane* a = P->app->active;
  const QString sym = a ? a->cfg.value("symbol").toString() : QString();
  const OrderForm form = readForm();
  std::optional<double> cur = price(sym);
  // the chart's last close only when its bars are this symbol's: right after a switch they are still the previous
  // symbol's, and a market order filled at that price (a quote is asked instead)
  if (!cur && a && !a->bars.isEmpty() && barsKey(a).symbol == sym) cur = a->bars.last().close;
  if (cur) return placeAt(side, sym, *cur, form);
  data::getQuotes({sym}, this, [this, side, sym, form](const data::Result<data::Quotes>& r) {
    double px = 0;
    if (r.ok() && r.value.contains(sym) && r.value.value(sym).error.isEmpty()) px = r.value.value(sym).price;
    placeAt(side, sym, px, form);
  });
}

PaperTab::OrderForm PaperTab::readForm() const {
  OrderForm f;
  if (type_) f.type = optValue(type_);
  f.amt = amt_ ? inputNumber(amt_) : NaN;
  f.sl = sl_ ? inputNumber(sl_) : 0;
  f.tp = tp_ ? inputNumber(tp_) : 0;
  f.price = price_ ? inputNumber(price_) : NaN;
  return f;
}

void PaperTab::placeAt(const QString& side, const QString& sym, double cur, const OrderForm& f) {
  const QString type = f.type;
  const double amt = f.amt;
  if (!(cur > 0 || cur < 0) || !(amt > 0)) return P->app->toast(QCoreApplication::translate("charts", "Need a live price and an amount."), "err");
  const double sl = f.sl, tp = f.tp;
  const QString id = "o" + QString::number(QDateTime::currentMSecsSinceEpoch());
  if (type == "market") {
    const double qty = amt / cur;
    fill(sym, side, qty, cur);
    addBracket(accountKey(), sym, side, qty, cur, sl, tp, id);
  } else {
    const double entry = f.price;
    if (!(entry > 0)) return P->app->toast(QCoreApplication::translate("charts", "Set a price for the order."), "err");
    QJsonObject acct = paper();
    QJsonArray orders = acct.value("orders").toArray();
    // The stop loss / take profit wait on the entry: live before it fills they closed a position not opened yet
    // (a take profit above the market sold at once and opened a short). They are placed when it fills.
    QJsonObject o{{"id", id}, {"symbol", sym}, {"side", side}, {"type", type}, {"price", entry}, {"qty", amt / entry}};
    if (sl > 0) o["sl"] = sl;
    if (tp > 0) o["tp"] = tp;
    orders << o;
    acct["orders"] = orders;
    setPaper(acct);
    P->app->toast(QString(QCoreApplication::translate("charts", "%1 %2 order placed.")).arg(word(type), word(side)));
  }
  P->app->persist();
  refresh();
  if (P->app->panels && P->app->panels->sidePanel) P->app->panels->sidePanel->refreshQuotes();
}

void PaperTab::addBracket(const QString& acctKey, const QString& sym, const QString& side, double qty, double entryPx,
                          double sl, double tp, const QString& baseId) {
  if (!(sl > 0) && !(tp > 0)) return;
  if (!(qty > 0) || !(entryPx > 0)) return;
  const QString exitSide = side == "buy" ? "sell" : "buy";
  const double dir = side == "buy" ? 1 : -1;
  // ids and the OCO group from the entry's id: unique even when several entries fill in the same millisecond
  const QJsonValue oco = sl > 0 && tp > 0 ? QJsonValue("oco" + baseId) : QJsonValue(QJsonValue::Null);
  QJsonObject acct = P->app->state.value(acctKey).toObject();
  QJsonArray orders = acct.value("orders").toArray();
  if (sl > 0)
    orders << QJsonObject{{"id", baseId + "s"}, {"symbol", sym}, {"side", exitSide}, {"type", "stop"},
                          {"price", entryPx * (1 - (dir * sl) / 100)}, {"qty", qty}, {"oco", oco}, {"note", "stop loss"}};
  if (tp > 0)
    orders << QJsonObject{{"id", baseId + "t"}, {"symbol", sym}, {"side", exitSide}, {"type", "limit"},
                          {"price", entryPx * (1 + (dir * tp) / 100)}, {"qty", qty}, {"oco", oco}, {"note", "take profit"}};
  acct["orders"] = orders;
  P->app->state[acctKey] = acct;
  P->app->persist();
}

void PaperTab::refresh() {
  if (!body_ || P->body() != body_ || !P->isOpen("paper")) return;
  ChartPane* a = P->app->active;
  const QString sym = a ? a->cfg.value("symbol").toString() : QString();
  if (sym != draft_.sym || draft_.acct != accountKey()) return render();   // the ticket is for another symbol / account
  renderRight();
  std::optional<double> px = price(sym);
  if (!px && a && !a->bars.isEmpty() && barsKey(a).symbol == sym) px = a->bars.last().close;
  if (title_) setTicketTitle(title_, sym, px);
  // the price field follows the market until the user types in it
  if (price_ && px && !draft_.priceTyped && !price_->hasFocus()) price_->setText(jsPrecision(*px, 6));
}

// Limit and stop orders in the replay account fill when a replayed bar's range reaches them.
void PaperTab::checkReplayOrders(ChartPane* pane, const QJsonObject& bar) {
  const QJsonObject p = P->app->state.value("replayPaper").toObject();
  if (p.isEmpty() || p.value("orders").toArray().isEmpty()) return;
  const double low = num(bar, "low"), high = num(bar, "high"), open = num(bar, "open");
  // the bars replayed are the market they were loaded for (cfg names the new one while a switch loads)
  const QString barsSym = barsKey(pane).symbol;
  for (const auto& ov : p.value("orders").toArray()) {
    const QJsonObject o = ov.toObject();
    if (o.value("symbol").toString() != barsSym) continue;
    // the loop walks a snapshot: an order an earlier fill of this bar removed (the other half of its bracket) is
    // skipped, or the stop loss and the take profit both filled on one bar
    if (!P->app->state.value("replayPaper").toObject().value("orders").toArray().contains(o)) continue;
    const QString t = o.value("type").toString(), s = o.value("side").toString();
    const double op = num(o, "price");
    std::optional<double> px;
    if (t == "limit" && s == "buy" && low <= op) px = std::min(op, open);
    if (t == "limit" && s == "sell" && high >= op) px = std::max(op, open);
    if (t == "stop" && s == "buy" && high >= op) px = std::max(op, open);
    if (t == "stop" && s == "sell" && low <= op) px = std::min(op, open);
    if (!px) continue;
    QJsonObject cur = P->app->state.value("replayPaper").toObject();
    QJsonArray keep;
    const QJsonValue oco = o.value("oco");
    for (const auto& x : cur.value("orders").toArray()) {
      const QJsonObject xo = x.toObject();
      if (xo == o || (bt::jsTruthy(oco) && xo.value("oco") == oco)) continue;
      keep << xo;
    }
    cur["orders"] = keep;
    P->app->state["replayPaper"] = cur;
    fill(o.value("symbol").toString(), s, num(o, "qty"), *px,
         o.value("note").toString().isEmpty() ? t + " order" : o.value("note").toString());
    addBracket("replayPaper", o.value("symbol").toString(), s, num(o, "qty"), *px, num(o, "sl"), num(o, "tp"),
               o.value("id").toString());
  }
}

void PaperTab::onReplayBar(ChartPane* pane, const QJsonObject& bar) {
  if (pane != P->app->active || bar.isEmpty()) return;
  checkReplayOrders(pane, bar);
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (P->isOpen("paper") && now - paperRenderAt_ > 250) {
    paperRenderAt_ = now;
    refresh();   // not render(): the order ticket being typed stays
  }
}

}  // namespace charts::ui::bottom
