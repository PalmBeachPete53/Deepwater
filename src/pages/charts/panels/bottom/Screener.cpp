// Charts tab — Screener. Port of quant-terminal public/js/panels.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Screener.h"
#include "charts/core/Backtest.h"
#include "charts/data/Scan.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/RuleBuilder.h"
#include "pages/charts/panels/bottom/Common.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace charts::ui::bottom {


void loadScreenerRows(const QString& universe, QObject* ctx,
                      std::function<void(const QList<QJsonObject>&, const QString&)> cb) {
  // GET /api/screener?universe=…: crypto → cryptoScreener, etf → Nasdaq ETFs, anything else → Nasdaq stocks
  auto done = [cb](const data::Result<QList<data::ScreenerRow>>& r) {
    if (!r.ok()) return cb({}, r.error->message);
    QList<QJsonObject> rows;
    for (const auto& x : r.value) rows << x.toJson();
    cb(rows, {});
  };
  if (universe == "crypto") data::cryptoScreener(ctx, done);
  else data::nasdaqScreener(universe == "etf" ? "etf" : "stocks", ctx, done);
}

static double jn(const QJsonObject& r, const QString& k) {   // r[k] as a number; NaN = undefined / not a number
  const QJsonValue v = r.value(k);
  return v.isDouble() ? v.toDouble() : NaN;
}

ScreenerTab::ScreenerTab(BottomPanel* panel) : QObject(panel), P(panel) {
  sc.rule = QJsonObject{{"left", QJsonObject{{"kind", "ind"}, {"id", "rsi"}, {"params", QJsonObject{{"length", 14}}},
                                             {"output", "value"}}},
                        {"op", "<"},
                        {"right", QJsonObject{{"kind", "value"}, {"v", 30}}}};
}

void ScreenerTab::load() {
  if (box_) {
    while (QLayoutItem* it = boxLay_->takeAt(0)) {
      if (it->widget()) it->widget()->deleteLater();
      delete it;
    }
    auto* l = hint(QCoreApplication::translate("charts", "Loading…"));
    l->setContentsMargins(12, 10, 12, 10);
    boxLay_->addWidget(l);
  }
  const int seq = ++loadSeq_;
  const QString u = sc.universe;
  loadScreenerRows(u, this, [this, seq](const QList<QJsonObject>& rows, const QString& err) {
    if (seq != loadSeq_) return;
    if (!err.isEmpty()) {
      sc.rows.clear();
      P->app->toast(fmt::esc(fmt::tx(err)), "err");
    } else {
      sc.rows = rows;
      sc.scan.reset();
      // render(), not draw(): the Sector select is built from these rows (it showed the previous market's sectors).
      // Not with no rows: render() would load again.
      if (!rows.isEmpty() && P->isOpen("screener") && P->body() == body_) return render();
    }
    draw();
  });
}

QList<QJsonObject> ScreenerTab::filtered(bool withScan) const {
  const auto& f = sc.filters;
  const QString term = f.value("q").toString().toLower();
  auto has = [&](const char* k) { return f.contains(k) && !f.value(k).isNull(); };
  auto fv = [&](const char* k) { return f.value(k).toDouble(); };
  QList<QJsonObject> out;
  for (const QJsonObject& r : sc.rows) {
    const QString hay = (r.value("ticker").toString() + " " + r.value("name").toString()).toLower();
    if (!term.isEmpty() && !hay.contains(term)) continue;
    if (has("minPrice") && !(jn(r, "price") >= fv("minPrice"))) continue;
    if (has("maxPrice") && !(jn(r, "price") <= fv("maxPrice"))) continue;
    if (has("minChg") && !(jn(r, "changePct") >= fv("minChg"))) continue;
    if (has("maxChg") && !(jn(r, "changePct") <= fv("maxChg"))) continue;
    if (has("minVol") && !(jn(r, "volume") >= fv("minVol"))) continue;
    if (has("minCap")) {
      const double cap = jn(r, "marketCap");
      if (!((std::isnan(cap) || cap == 0 ? 0 : cap) >= fv("minCap"))) continue;
    }
    if (!f.value("sector").toString().isEmpty() && r.value("sector").toString() != f.value("sector").toString()) continue;
    if (withScan && sc.scan && !sc.scan->value(r.value("symbol").toString()).value("pass").toBool()) continue;
    out << r;
  }
  const QString k = sc.sort;
  const int dir = sc.dir;
  std::stable_sort(out.begin(), out.end(), [k, dir](const QJsonObject& a, const QJsonObject& b) {
    double c;
    if (a.value(k).isString()) c = QString::localeAwareCompare(a.value(k).toString(), bt::jsString(b.value(k)));
    else {
      double x = jn(a, k), y = jn(b, k);
      if (std::isnan(x)) x = 0;
      if (std::isnan(y)) y = 0;
      c = x - y;
    }
    return dir * c < 0;
  });
  return out;
}

void ScreenerTab::setUniverse(const QString& u) {
  sc.universe = u;
  sc.filters.clear();
  // the previous market's rows and scan go: the Sector filter was built from them and a scan still running for
  // them would fill this market's table with its answer (no match: an empty table)
  sc.rows.clear();
  sc.scan.reset();
  ++scanSeq_;
  render();   // no rows: render() loads them
}

void ScreenerTab::render() {
  body_ = nullptr;   // the old form's last focus-out (its body hides in newBody) must not write over sc
  QWidget* body = P->newBody();
  body_ = body;
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  auto* content = new QWidget;
  auto* cl = new QVBoxLayout(content);
  cl->setContentsMargins(0, 0, 0, 0);
  cl->setSpacing(0);
  QScrollArea* area = nullptr;
  bl->addWidget(scrollBody(content, &area));
  area_ = area;

  QSet<QString> sset;
  for (const auto& r : sc.rows)
    if (!r.value("sector").toString().isEmpty()) sset.insert(r.value("sector").toString());
  QStringList sectors(sset.begin(), sset.end());
  std::sort(sectors.begin(), sectors.end());

  auto* pad = new QWidget;
  auto* pl = new QVBoxLayout(pad);
  pl->setContentsMargins(12, 10, 12, 10);
  pl->setSpacing(8);
  QList<QWidget*> row1;
  QComboBox* u = optSelect({{QCoreApplication::translate("charts", "Crypto (USDT pairs)"), "crypto"}, {QCoreApplication::translate("charts", "US stocks"), "us"}, {QCoreApplication::translate("charts", "US ETFs"), "etf"}},
                           sc.universe, false);
  u->setProperty("qtc", "in");
  u->setFixedHeight(28);
  row1 << style::field(QCoreApplication::translate("charts", "Market"), u);
  connect(u, &QComboBox::activated, this, [this, u](int) { setUniverse(optValue(u)); });
  auto text = [&](const QString& k, int w) {
    QLineEdit* e = style::input();
    e->setFixedWidth(w);
    e->setText(sc.filters.value(k).toString());
    connect(e, &QLineEdit::textEdited, this, [this, k](const QString& v) {
      sc.filters[k] = v.trimmed().isEmpty() ? QJsonValue() : QJsonValue(v.trimmed());
      draw();
    });
    return e;
  };
  auto n = [&](const QString& k, const QString& label, int w = 80) {
    const QJsonValue cur = sc.filters.value(k);
    QLineEdit* e = numInput(cur.isDouble() ? jsNum(cur.toDouble()) : QString(), w);
    connect(e, &QLineEdit::textEdited, this, [this, k, e](const QString& v) {
      // JS: v === "" ? null : Number(v), where an invalid number field reads as "" (no filter, never NaN)
      Q_UNUSED(v);
      const auto x = inputValue(e);
      sc.filters[k] = x ? QJsonValue(*x) : QJsonValue();
      draw();
    });
    return style::field(label, e);
  };
  row1 << style::field(QCoreApplication::translate("charts", "Search"), text("q", 120));
  row1 << n("minPrice", QCoreApplication::translate("charts", "Price from")) << n("maxPrice", QCoreApplication::translate("charts", "Price to")) << n("minChg", QCoreApplication::translate("charts", "Change % from"), 70)
       << n("maxChg", QCoreApplication::translate("charts", "Change % to"), 70) << n("minVol", QCoreApplication::translate("charts", "Min volume"), 100);
  if (sc.universe != "crypto") row1 << n("minCap", QCoreApplication::translate("charts", "Min market cap"), 110);
  if (sectors.size() > 1) {
    QList<Opt> o{{QCoreApplication::translate("charts", "All"), ""}};
    for (const auto& s : sectors) o.append({s, s});
    QComboBox* s = optSelect(o, sc.filters.value("sector").toString());
    connect(s, &QComboBox::activated, this, [this, s](int) {
      sc.filters["sector"] = optValue(s).isEmpty() ? QJsonValue() : QJsonValue(optValue(s));
      draw();
    });
    row1 << style::field(QCoreApplication::translate("charts", "Sector"), s);
  }
  auto* refresh = style::btn(QCoreApplication::translate("charts", "Refresh"));
  alignEnd(refresh);
  connect(refresh, &QPushButton::clicked, this, [this] { load(); });
  row1 << refresh;
  pl->addWidget(rowFlex(row1));

  auto* tfHint = new QLabel(QCoreApplication::translate("charts", "Technical filter:"));
  tfHint->setProperty("qtc", "hint");
  auto* rl = new rules::RuleList(QJsonArray{sc.rule});
  rl->setOuterMargins(false);
  // the rule and the timeframe are kept as they are edited: a re-render rebuilds them from sc (only Scan read them)
  connect(rl, &rules::RuleList::changed, this, [this, body, rl] {
    const QJsonArray rs = rl->rules();
    if (body_ == body && !rs.isEmpty()) sc.rule = rs.first().toObject();
  });
  for (auto* b : rl->findChildren<QAbstractButton*>())   // $("#scRule .x")?.remove()
    if (b->property("qtc").toString() == "icon-x" || b->text() == QString(QChar(0xd7))) b->hide();
  rule_ = rl;
  QList<Opt> tfs;
  for (const char* t : {"1h", "4h", "1D", "1W"}) tfs.append(Opt{t, t});
  QComboBox* tf = optSelect(tfs, sc.tf);
  tfSel_ = tf;
  connect(tf, &QComboBox::activated, this, [this, body, tf] {
    if (body_ == body) sc.tf = optValue(tf);
  });
  auto* scanB = style::btn(QCoreApplication::translate("charts", "Scan top 100"), "primary");
  alignEnd(scanB);
  scanBtn_ = scanB;
  connect(scanB, &QPushButton::clicked, this, [this] { runScan(); });
  QList<QWidget*> row2{tfHint, rl, style::field(QCoreApplication::translate("charts", "On", "screener: the timeframe it scans"), tf), scanB};
  if (sc.scan) {
    auto* clear = style::btn(QCoreApplication::translate("charts", "Clear scan"));
    alignEnd(clear);
    connect(clear, &QPushButton::clicked, this, [this] {
      sc.scan.reset();
      render();
    });
    row2 << clear;
  }
  pl->addWidget(rowFlex(row2));
  cl->addWidget(pad);

  auto* box = new QWidget;
  boxLay_ = new QVBoxLayout(box);
  boxLay_->setContentsMargins(0, 0, 0, 0);
  boxLay_->setSpacing(0);
  box_ = box;
  cl->addWidget(box);
  cl->addStretch(1);
  if (sc.rows.isEmpty()) load();
  else draw();
}

void ScreenerTab::runScan() {
  if (!rule_) return;
  const QJsonArray rs = rule_->rules();
  const QJsonObject rule = rs.isEmpty() ? sc.rule : rs.first().toObject();
  sc.rule = rule;
  sc.tf = optValue(tfSel_);
  const QJsonObject c = rules::compileRules(QList<QPair<QString, QJsonArray>>{{"rules", QJsonArray{rule}}});
  QList<QJsonObject> rows = filtered(false);
  std::stable_sort(rows.begin(), rows.end(),
                   [](const QJsonObject& a, const QJsonObject& b) { return jn(b, "volume") - jn(a, "volume") < 0; });
  QJsonArray targets;
  for (int i = 0; i < rows.size() && i < 100; ++i) targets << rows[i].value("symbol").toString();
  if (scanBtn_) scanBtn_->setText(QString(QCoreApplication::translate("charts", "Scanning %1…")).arg(targets.size()));
  const int seq = ++scanSeq_;
  const QString universe = sc.universe;
  data::scan(targets, sc.tf, c.value("indicators").toObject(), c.value("rules").toArray(), this,
             [this, targets, seq, universe](const QJsonArray& res) {
              // a newer scan, or another market, came meanwhile: this answer is not about the table shown
              if (seq != scanSeq_ || universe != sc.universe) return;
              QHash<QString, QJsonObject> m;
              int hits = 0;
              for (const auto& v : res) {
                const QJsonObject h = v.toObject();
                m.insert(h.value("symbol").toString(), h);
                if (h.value("pass").toBool()) ++hits;
              }
              sc.scan = m;
              P->app->toast(QString(QCoreApplication::translate("charts", "%1 of %2 match.")).arg(hits).arg(targets.size()));
              if (P->isOpen("screener")) render();
            });
}

void ScreenerTab::draw() {
  if (!box_ || P->body() != body_) return;
  while (QLayoutItem* it = boxLay_->takeAt(0)) {
    if (it->widget()) it->widget()->deleteLater();
    delete it;
  }
  const QList<QJsonObject> rows = filtered();
  const bool crypto = sc.universe == "crypto";
  QList<QPair<QString, QString>> cols = {{"ticker", QCoreApplication::translate("charts", "Symbol")},
                                         {"name", QCoreApplication::translate("charts", "Name")},
                                         {"price", QCoreApplication::translate("charts", "Price")},
                                         {"changePct", QCoreApplication::translate("charts", "Change %")},
                                         {"volume", crypto ? QCoreApplication::translate("charts", "Volume (USDT)") : QCoreApplication::translate("charts", "Volume")}};
  if (!crypto) cols += {{"marketCap", QCoreApplication::translate("charts", "Market cap")}, {"sector", QCoreApplication::translate("charts", "Sector")}};
  auto* table = new Table;
  QList<Cell> head;
  for (const auto& [k, l] : cols) {
    Cell c{l + (sc.sort == k ? (sc.dir < 0 ? " ↓" : " ↑") : QString())};
    c.left = k == "name" || k == "sector";
    head << c;
  }
  table->setHeader(head);
  QList<Row> out;
  for (int i = 0; i < rows.size() && i < 500; ++i) {
    const QJsonObject& r = rows[i];
    Row row;
    row.data = r.value("symbol").toString();
    const double chg = jn(r, "changePct");
    row.cells = {Cell{r.value("ticker").toString(), {}, false, false, true},
                 Cell{r.value("name").toString().left(40), "muted", true},
                 {fmtPrice(jn(r, "price"))},
                 Cell{fmtPct(chg), chg >= 0 ? "up" : "down"},
                 {fmtNum(jn(r, "volume"), 0)}};
    if (!crypto) {
      row.cells << Cell{fmtNum(jn(r, "marketCap"), 0)};
      row.cells << Cell{r.value("sector").toString(), "muted", true};
    }
    out << row;
  }
  table->setRows(out);
  table->setStickyIn(area_);
  connect(table, &Table::headerClicked, this, [this, cols](int c) {
    const QString k = cols.value(c).first;
    if (k.isEmpty()) return;
    sc.dir = sc.sort == k ? -sc.dir : -1;
    sc.sort = k;
    draw();
  });
  connect(table, &Table::rowClicked, this, [this, table](int r) {
    const QString s = table->rows().value(r).data;
    if (!s.isEmpty()) P->app->setSymbol(s);
  });
  boxLay_->addWidget(table);
  QString note = (rows.size() == 1 ? QCoreApplication::translate("charts", "%1 match%2. ")
                                    : QCoreApplication::translate("charts", "%1 matches%2. "))
                     .arg(rows.size())
                     .arg(rows.size() > 500 ? QCoreApplication::translate("charts", " (showing 500)") : QString());
  note += QCoreApplication::translate("charts", "Click a row to open its chart.");
  if (!crypto) note += " " + QCoreApplication::translate("charts", "Live prices for the 200 largest companies; the rest show the last close.");
  auto* h = hint(fmt::esc(note));
  h->setContentsMargins(12, 10, 12, 10);
  boxLay_->addWidget(h);
}

}  // namespace charts::ui::bottom
