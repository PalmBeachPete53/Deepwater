// Charts tab — Financials. Port of quant-terminal public/js/research.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Financials.h"
#include "charts/data/MarketInfo.h"
#include "charts/core/Backtest.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/bottom/Common.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHelpEvent>
#include <QJsonArray>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolTip>
#include <QVBoxLayout>
#include <cmath>
#include <optional>

namespace charts::ui::bottom {


namespace {
std::optional<double> toNum(const QJsonValue& v) {   // "$1,234" -> 1234, "(56)" -> -56
  static const QRegularExpression junk("[$,()]");
  const QString str = (v.isNull() || v.isUndefined()) ? QString() : bt::jsString(v).trimmed();
  QString c = str;
  c.remove(junk);
  const double n = data::jsParseFloat(c);
  if (!std::isfinite(n)) return std::nullopt;
  return str.startsWith('(') ? -std::fabs(n) : n;
}
QString money(std::optional<double> v) { return v ? "$" + jsFixed(*v / 1e6, 1) + "B" : QStringLiteral("—"); }

struct Period { QString p; std::optional<double> r, n; };

// .fin-bars: one column per period, revenue and net income bars side by side, two small labels below.
class FinBars : public QWidget {
public:
  QList<Period> periods;
  double max = 1;
  FinBars() { setFixedHeight(150); setMouseTracking(true); }
protected:
  QRectF colRect(int i) const {
    const int n = int(periods.size());
    const double cw = (width() - 10.0 * (n - 1)) / std::max(1, n);
    return QRectF(i * (cw + 10), 0, cw, height());
  }
  static constexpr double kSub = 14 * 2;   // two .sub lines (10 px, line-height 1.4)
  QPair<QRectF, QRectF> bars(int i) const {
    const QRectF c = colRect(i);
    const double stackH = c.height() - kSub;
    const double bw = c.width() * 0.38;
    const double x0 = c.center().x() - (2 * bw + 3) / 2;
    const auto& x = periods[i];
    const double rh = std::max(1.0, std::fabs(x.r.value_or(0)) / max * stackH);
    const double nh = std::max(1.0, std::fabs(x.n.value_or(0)) / max * stackH);
    return {QRectF(x0, stackH - rh, bw, rh), QRectF(x0 + bw + 3, stackH - nh, bw, nh)};
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& V = style::v();
    auto top = [&](const QRectF& r, const QColor& c) {
      QPainterPath path;
      const double rad = std::min(3.0, r.height());
      path.moveTo(r.bottomLeft());
      path.lineTo(r.left(), r.top() + rad);
      path.quadTo(r.topLeft(), QPointF(r.left() + rad, r.top()));
      path.lineTo(r.right() - rad, r.top());
      path.quadTo(r.topRight(), QPointF(r.right(), r.top() + rad));
      path.lineTo(r.bottomRight());
      path.closeSubpath();
      p.fillPath(path, c);
    };
    QFont f = style::font(10);
    p.setFont(f);
    for (int i = 0; i < periods.size(); ++i) {
      const auto [rb, nb] = bars(i);
      top(rb, QColor("#2962ff"));   // revenue: the caption says blue (the CSS --accent of quant-terminal)
      top(nb, periods[i].n.value_or(0) < 0 ? V.down : V.up);
      const QRectF c = colRect(i);
      p.setPen(V.muted);
      const QFontMetricsF fm(f);
      const QString a = fm.elidedText(periods[i].p, Qt::ElideRight, c.width());
      const QString b = fm.elidedText(money(periods[i].r) + " · " + money(periods[i].n), Qt::ElideRight, c.width());
      p.drawText(QRectF(c.left(), c.bottom() - kSub, c.width(), 14), Qt::AlignCenter, a);
      p.drawText(QRectF(c.left(), c.bottom() - 14, c.width(), 14), Qt::AlignCenter, b);
    }
  }
  bool event(QEvent* e) override {
    if (e->type() == QEvent::ToolTip) {   // the bars' title attributes
      auto* he = static_cast<QHelpEvent*>(e);
      for (int i = 0; i < periods.size(); ++i) {
        const auto [rb, nb] = bars(i);
        if (rb.contains(he->pos())) return QToolTip::showText(he->globalPos(), QCoreApplication::translate("charts", "Revenue %1").arg(money(periods[i].r)), this), true;
        if (nb.contains(he->pos())) return QToolTip::showText(he->globalPos(), QCoreApplication::translate("charts", "Net income %1").arg(money(periods[i].n)), this), true;
      }
      QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }
};

QLabel* padHint(const QString& text) {
  auto* l = hint(fmt::esc(text));
  l->setContentsMargins(12, 10, 12, 10);
  l->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  return l;
}
}  // namespace

FinancialsTab::FinancialsTab(BottomPanel* panel) : QObject(panel), P(panel) {}

void FinancialsTab::render(const QString& freq, const QString& tab) {
  const QString sym = P->app->active ? P->app->active->cfg.value("symbol").toString() : QString();
  QWidget* body = P->newBody();
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  if (exchange(sym) != "US") {
    bl->addWidget(padHint(QCoreApplication::translate("charts", "Company financials are for US-listed stocks. Open one (AAPL, NVDA, JPM…) to see its income "
                            "statement, balance sheet, cash flow and key stats.")));
    bl->addStretch(1);
    return;
  }
  auto* loading = padHint(QCoreApplication::translate("charts", "Loading financials…"));
  bl->addWidget(loading);
  bl->addStretch(1);
  const int seq = ++seq_;
  QPointer<QWidget> pbody(body);
  data::financials(sym, freq, this, [=, this](const data::Result<QJsonValue>& r) {
    if (seq != seq_ || !pbody || P->body() != pbody) return;
    if (!r.ok()) {
      loading->setText(fmt::esc(fmt::tx(r.error->message)));
      return;
    }
    if ((P->app->active ? P->app->active->cfg.value("symbol").toString() : QString()) != sym) return;
    if (!r.value.isObject()) return;   // null (offline): the JS throws reading f.income and keeps "Loading financials…"
    const QJsonObject f = r.value.toObject();
    const QList<QPair<QString, QPair<QString, QJsonValue>>> tables = {
        {"income", {QCoreApplication::translate("charts", "Income statement"), f.value("income")}},
        {"balance", {QCoreApplication::translate("charts", "Balance sheet"), f.value("balance")}},
        {"cashflow", {QCoreApplication::translate("charts", "Cash flow"), f.value("cashflow")}},
        {"ratios", {QCoreApplication::translate("charts", "Ratios"), f.value("ratios")}}};
    QJsonValue t;
    for (const auto& x : tables)
      if (x.first == tab) t = x.second.second;
    // revenue and net income bars, oldest first
    const QJsonObject income = f.value("income").toObject();
    auto find = [&](const QRegularExpression& re) -> std::optional<QJsonObject> {
      for (const auto& v : income.value("rows").toArray())
        if (re.match(v.toObject().value("label").toString()).hasMatch()) return v.toObject();
      return std::nullopt;
    };
    const auto rev = find(QRegularExpression("^total revenue", QRegularExpression::CaseInsensitiveOption));
    auto ni = find(QRegularExpression("^net income$", QRegularExpression::CaseInsensitiveOption));
    if (!ni) ni = find(QRegularExpression("net income", QRegularExpression::CaseInsensitiveOption));

    // el.innerHTML = …
    while (QLayoutItem* it = bl->takeAt(0)) {
      if (it->widget()) it->widget()->deleteLater();
      delete it;
    }
    TwoCol tc = twoCol(nullptr);
    bl->addWidget(tc.root);
    Col left{tc.leftLay};
    left.add(h4(ticker(sym) + " " + QCoreApplication::translate("charts", "key stats")), 0, kH4Bottom);
    auto* stats = new QWidget;
    auto* g = new QGridLayout(stats);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(6);
    const QJsonArray st = f.value("stats").toArray();
    for (int i = 0; i < st.size(); ++i) {
      const QJsonObject x = st[i].toObject();
      auto* cell = new QLabel(QString("<span style=\"color:%1\">%2</span><br><b>%3</b>")
                                  .arg(style::v().muted.name(), fmt::esc(x.value("label").toString()),
                                       fmt::esc(x.value("value").toString())));
      cell->setFont(style::font(12));
      textColor(cell);
      g->addWidget(cell, i / 2, i % 2);
    }
    g->setColumnStretch(0, 1);
    g->setColumnStretch(1, 1);
    left.add(stats);
    if (!f.value("nextEarnings").toString().isEmpty())
      left.add(hint(fmt::esc(fmt::tx(f.value("nextEarnings").toString()))), kPMargin, kPMargin);
    if (rev && !income.isEmpty()) {
      auto* fb = new FinBars;
      const QJsonArray periods = income.value("periods").toArray();
      const QJsonArray rv = rev->value("values").toArray();
      const QJsonArray nv = ni ? ni->value("values").toArray() : QJsonArray();
      for (int k = int(periods.size()) - 1; k >= 0; --k)
        fb->periods << Period{periods[k].toString(), toNum(rv.at(k)), ni ? toNum(nv.at(k)) : std::nullopt};
      for (const auto& x : fb->periods) fb->max = std::max(fb->max, std::fabs(x.r.value_or(0)));
      left.add(fb, 12, 4);
      left.add(hint(QCoreApplication::translate("charts", "Blue: revenue. Green: net income.")));
    }
    left.finish();

    // right: table buttons, frequency, table, note
    QList<QWidget*> row;
    for (const auto& x : tables) {
      auto* b = style::btn(x.second.first, x.first == tab ? "small primary" : "small");
      b->setEnabled(x.second.second.isObject());
      const QString k = x.first;
      connect(b, &QPushButton::clicked, this, [this, freq, k] { render(freq, k); });
      row << b;
    }
    row << nullptr;
    QComboBox* fs = optSelect({{QCoreApplication::translate("charts", "Annual"), "annual"}, {QCoreApplication::translate("charts", "Quarterly"), "quarterly"}}, freq);
    connect(fs, &QComboBox::activated, this, [this, fs, tab](int) { render(optValue(fs), tab); });
    row << fs;
    tc.rightLay->addWidget(rowFlex(row));
    if (t.isObject()) {
      const QJsonObject to = t.toObject();
      auto* table = new Table;
      TableStyle ts;
      ts.wrap = true;
      table->setStyle(ts);
      QList<Cell> head{Cell{QString(), {}, true}};
      for (const auto& p : to.value("periods").toArray()) head << Cell{p.toString()};
      table->setHeader(head);
      QList<Row> rows;
      for (const auto& rv : to.value("rows").toArray()) {
        const QJsonObject ro = rv.toObject();
        Row r;
        r.cells << Cell{ro.value("label").toString(), {}, true};
        for (const auto& v : ro.value("values").toArray()) r.cells << Cell{v.isString() ? v.toString() : bt::jsString(v)};
        rows << r;
      }
      table->setRows(rows);
      table->setStickyIn(tc.rightArea);
      tc.rightLay->addWidget(table);
    } else {
      tc.rightLay->addWidget(hint(QCoreApplication::translate("charts", "Not available for this company.")));
    }
    tc.rightLay->addWidget(hint(fmt::esc(fmt::tx(f.value("note").toString())) + " " + QCoreApplication::translate("charts", "Source: Nasdaq.")));
    tc.rightLay->addStretch(1);
  });
}

}  // namespace charts::ui::bottom
