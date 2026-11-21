#include "pages/account/TrackRecord.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include "pages/account/TrCharts.h"
#include "account/Contracts.h"
#include "ui/Format.h"
#include "core/I18n.h"
#include "ui/Touch.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace acct {
namespace {
QString pctAxis(double v) {
  const double a = std::abs(v);
  return (v < -1e-9 ? QString(QChar(0x2212)) : QString()) + fmt::num(a, a < 10 && a != std::floor(a) ? 1 : (a < 10 ? 1 : 0)) + "%";
}
}

AdvancedView::AdvancedView(QWidget* parent) : TrView(parent) {
  m_head = new PaintPanel(this);
  m_head->setFrame(false);
  m_head->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    p.setFont(mono(10, false, 2.3)); p.setPen(theme::map("#4a463c"));
    p.drawText(m_phone ? QRectF(0, 0, r.width(), 27) : QRectF(0, 0, r.width() - 140, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
               (m_d.stats.n == 1 ? QCoreApplication::translate("account", "ADVANCED DETAILS · %1 CLOSED TRADE") : QCoreApplication::translate("account", "ADVANCED DETAILS · %1 CLOSED TRADES")).arg(m_d.stats.n));
    const QRectF box = toggleRect();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(theme::map("#2a2618")); p.setBrush(Qt::NoBrush); p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
    const QRectF a(box.left() + 1, box.top() + 1, m_phone ? 44 : 26, box.height() - 2), b(a.right(), a.top(), box.right() - 1 - a.right(), a.height());
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#2e2808"));
    p.drawRect(m_pct ? b : a);
    p.setFont(mono(11, true, 2));
    p.setPen(m_pct ? c().dim : theme::map("#eed882")); p.drawText(a.translated(1, 0), Qt::AlignCenter, "$");
    p.setPen(m_pct ? theme::map("#eed882") : c().dim); p.drawText(b.translated(1, 0), Qt::AlignCenter, QCoreApplication::translate("account", "% BALANCE"));
  });
  connect(m_head, &PaintPanel::clicked, this, [this](const QPointF& pt) {
    const QRectF box = toggleRect();
    const qreal x0 = box.left();
    if (pt.x() < x0 || (m_phone && !box.contains(pt))) return;
    const bool pct = pt.x() > x0 + (m_phone ? 45 : 27);
    if (pct == m_pct) return;
    if (pct && m_d.balance <= 0) { m_head->setToolTip(QCoreApplication::translate("account", "Set the account's starting balance (account box › Edit account) to see % of balance.")); return; }
    m_pct = pct; setData(m_d);
  });
  const QStringList titles{QCoreApplication::translate("account", "DRAWDOWN"), QCoreApplication::translate("account", "TRADE TIME PERFORMANCE"), QCoreApplication::translate("account", "TRADE DURATION PERFORMANCE"), QCoreApplication::translate("account", "P&L BY WEEKDAY"), QCoreApplication::translate("account", "TOP 5 TRADES"),
                           QCoreApplication::translate("account", "TOP 5 SYMBOLS P&L"), QCoreApplication::translate("account", "NUMBER OF TRADES BY WEEKDAY"), QCoreApplication::translate("account", "BOTTOM 5 TRADES"), QCoreApplication::translate("account", "NUMBER OF TRADES · TOP 5 SYMBOLS")};
  const QStringList tips{
    QCoreApplication::translate("account", "Distance of the equity curve below its previous peak after each trade."),
    QCoreApplication::translate("account", "Each dot is a trade: open hour (local time) against its net P&L."),
    QCoreApplication::translate("account", "Each dot is a trade: time held (log scale) against its net P&L."),
    QCoreApplication::translate("account", "Net P&L by weekday of the close (local time). Days without trades are left out."),
    QCoreApplication::translate("account", "The five best trades by net P&L, with their close date."),
    QCoreApplication::translate("account", "The five instruments with the highest total net P&L."),
    QCoreApplication::translate("account", "Number of closed trades by weekday of the close."),
    QCoreApplication::translate("account", "The five worst trades by net P&L, with their close date."),
    QCoreApplication::translate("account", "Number of trades of the five instruments with the highest P&L.")};
  for (int i = 0; i < 9; ++i) {
    auto* w = new PaintPanel(this);
    w->setTitle(titles[i], tips[i]);
    w->setHoverable(true);
    m_charts << w;
  }
  rebuild();
}

// $ / % BALANCE switch: right of the header line; phones: on its own row under the title, 34 px tall (finger)
QRectF AdvancedView::toggleRect() const {
  const qreal w = m_head->width();
  return m_phone ? QRectF(w - 170, 35, 170, 34) : QRectF(w - 118, 1, 118, 25);
}

void AdvancedView::rebuild() {
  auto conv = [this](double v) { return m_pct && m_d.balance > 0 ? v / m_d.balance * 100.0 : v; };
  auto axisFmt = [this](double v) { return m_pct ? pctAxis(v) : axisMoney(v, m_d.currency); };
  auto valueText = [this](double money) {
    QString s = acct::money(money, m_d.currency);
    if (m_d.balance > 0) s += " · " + fmt::num(money / m_d.balance * 100, 2) + "%";
    return s;
  };
  auto tradeTip = [valueText](const account::Trade& t) {
    const double v = t.net();
    return QList<TipLine>{
      {account::displaySymbol(t.symbol), c().white, true, QString(" · %1 %2").arg(account::sideName(t.side), trimNum(t.lots, 2)), c().white},
      {QCoreApplication::translate("account", "Opened %1").arg(longDate(t.open)), c().white},
      {QCoreApplication::translate("account", "Held %1").arg(held(t.heldHours())), c().white},
      {valueText(v), v < 0 ? c().redBar : c().cyanBar}};
  };

  // 1. drawdown
  m_charts[0]->setPaint([this, conv, axisFmt](QPainter& p, const QRectF& r, const QPointF& mouse, double) {
    const auto& s = m_d.stats;
    const auto& T = m_d.trades;
    if (T.isEmpty()) return;
    const QRectF pl(78, 40, r.width() - 78 - 28, r.height() - 40 - 42);
    double a0, a1;
    const auto tk = ticks(conv(s.maxDD), 0, 5, &a0, &a1);
    const qint64 t0 = T.first().open.isValid() ? std::min(T.first().open.toSecsSinceEpoch(), T.first().close.toSecsSinceEpoch()) : T.first().close.toSecsSinceEpoch();
    const qint64 t1 = std::max(t0 + 3600, T.last().close.toSecsSinceEpoch());
    auto X = [&](qint64 t) { return pl.left() + double(t - t0) / double(t1 - t0) * pl.width(); };
    auto Y = [&](double v) { return pl.top() + (a1 - v) / (a1 - a0) * pl.height(); };
    p.setFont(chart(11));
    p.setRenderHint(QPainter::Antialiasing, false);
    for (double t : tk) {
      const qreal y = std::round(Y(t)) + 0.5;
      p.setPen(c().axis); p.drawText(QRectF(0, y - 8, pl.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, axisFmt(t));
    }
    // month ticks
    QDate m = QDateTime::fromSecsSinceEpoch(t0).toLocalTime().date(); m = QDate(m.year(), m.month(), 1).addMonths(1);
    const QDate end = QDateTime::fromSecsSinceEpoch(t1).toLocalTime().date();
    const bool multiYear = QDateTime::fromSecsSinceEpoch(t0).date().year() != end.year();
    int months = 0; for (QDate d = m; d <= end; d = d.addMonths(1)) ++months;
    const int every = std::max(1, int(std::ceil(months / (m_phone ? 4.0 : 7.0))));   // phones: ~300 px of plot
    int k = 0;
    for (QDate d = m; d <= end; d = d.addMonths(1), ++k) {
      if (k % every) continue;
      const qreal x = std::round(X(QDateTime(d, QTime(0, 0)).toSecsSinceEpoch())) + 0.5;
      p.setPen(c().grid); p.drawLine(QPointF(x, pl.top()), QPointF(x, pl.bottom()));
      p.setPen(c().axis);
      p.drawText(QRectF(x - 30, pl.bottom() + 8, 60, 16), Qt::AlignCenter, i18n::locale().toString(d, multiYear ? "MMM yy" : "MMM"));
    }
    QPainterPath line;
    line.moveTo(X(t0), Y(0));
    for (int i = 0; i < T.size(); ++i) line.lineTo(X(T[i].close.toSecsSinceEpoch()), Y(conv(s.drawdown[i])));
    QPainterPath area = line; area.lineTo(X(T.last().close.toSecsSinceEpoch()), Y(0)); area.closeSubpath();
    QLinearGradient g(0, Y(0), 0, Y(conv(s.maxDD)));
    g.setColorAt(0, theme::map("#34170f")); g.setColorAt(1, theme::map("#6f3331"));
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(area, g);
    p.setPen(QPen(theme::map("#e8c020"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    p.drawPath(line);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(c().axisLine); p.drawLine(QPointF(pl.left() - 0.5, pl.top()), QPointF(pl.left() - 0.5, pl.bottom()));
    if (std::isfinite(mouse.x()) && pl.adjusted(-4, -4, 4, 4).contains(mouse)) {
      int best = 0; qreal bd = 1e9;
      for (int i = 0; i < T.size(); ++i) { const qreal d = std::abs(X(T[i].close.toSecsSinceEpoch()) - mouse.x()); if (d < bd) { bd = d; best = i; } }
      const qreal x = std::round(X(T[best].close.toSecsSinceEpoch())) + 0.5, y = Y(conv(s.drawdown[best]));
      p.setPen(QPen(c().dim, 1, Qt::DashLine)); p.drawLine(QPointF(x, pl.top()), QPointF(x, pl.bottom()));
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(theme::map(QColor(Qt::white)), 2)); p.setBrush(theme::map("#f3d860")); p.drawEllipse(QPointF(x, y), 4, 4);
      const double dd = s.drawdown[best];
      QString v = money(dd, m_d.currency);
      if (m_d.balance > 0) v += " · " + fmt::num(dd / m_d.balance * 100, 2) + "%";
      tooltip(p, mouse, {{longDate(T[best].close), c().dim}, {QCoreApplication::translate("account", "Drawdown "), c().white, false, v, dd < 0 ? c().redBar : c().white, true}}, r);
    }
  });

  // 2-3. scatters
  auto scatter = [this, conv, axisFmt, tradeTip](int idx, bool duration) {
    m_charts[idx]->setPaint([this, conv, axisFmt, tradeTip, duration](QPainter& p, const QRectF& r, const QPointF& mouse, double) {
      const auto& T = m_d.trades;
      if (T.isEmpty()) return;
      const QRectF pl(80, 40, r.width() - 80 - 29, r.height() - 40 - 47);
      double lo = 0, hi = 0;
      for (const auto& t : T) { lo = std::min(lo, conv(t.net())); hi = std::max(hi, conv(t.net())); }
      double a0, a1;
      const auto tk = ticks(lo, hi, 5, &a0, &a1);
      auto Y = [&](double v) { return pl.top() + (a1 - v) / (a1 - a0) * pl.height(); };
      double x0 = 0, x1 = 24;
      if (duration) {
        x0 = -1; x1 = 1;   // log10 hours: 6 minutes .. 10 hours, widened to the data
        for (const auto& t : T) { const double h = t.heldHours(); if (h > 0) { x0 = std::min(x0, std::floor(std::log10(h))); x1 = std::max(x1, std::ceil(std::log10(h))); } }
      }
      auto xOf = [&](const account::Trade& t) -> double {
        if (duration) { const double h = std::max(1.0 / 60, t.heldHours()); return std::log10(h); }
        const QTime tm = t.open.toLocalTime().time(); return tm.hour() + tm.minute() / 60.0;
      };
      auto X = [&](double v) { return pl.left() + (v - x0) / (x1 - x0) * pl.width(); };
      p.setFont(chart(11));
      p.setRenderHint(QPainter::Antialiasing, false);
      for (double t : tk) {
        const qreal y = std::round(Y(t)) + 0.5;
        p.setPen(c().axisLine); p.drawLine(QPointF(pl.left() - 4, y), QPointF(pl.left(), y));
        p.setPen(c().axis); p.drawText(QRectF(0, y - 8, pl.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, axisFmt(t));
      }
      if (duration) {
        for (int e = int(x0); e <= int(x1); ++e) {
          const qreal x = std::round(X(e)) + 0.5;
          const double h = std::pow(10.0, e);
          p.setPen(c().axis);
          p.drawText(QRectF(x - 30, pl.bottom() + 8, 60, 16), Qt::AlignCenter, h < 1 ? QString("%1m").arg(qRound(h * 60)) : fmt::num(h, 1) + "h");
        }
      } else {
        const int hs = pl.width() / 12 < 33.5 ? 4 : 2;   // 2 h steps need ≥ 34 px (the reference has 34)
        for (int h = hs; h <= 22; h += hs) {
          const qreal x = std::round(X(h)) + 0.5;
          p.setPen(c().axis);
          p.drawText(QRectF(x - 30, pl.bottom() + 8, 60, 16), Qt::AlignCenter, QString("%1:00").arg(h, 2, 10, QChar('0')));
        }
      }
      p.setPen(c().axisLine); p.drawLine(QPointF(pl.left() - 0.5, pl.top()), QPointF(pl.left() - 0.5, pl.bottom()));
      p.setPen(QPen(theme::map("#605c55"), 1, Qt::DashLine));
      const qreal yz = std::round(Y(0)) + 0.5;
      p.drawLine(QPointF(pl.left(), yz), QPointF(pl.right(), yz));
      p.setRenderHint(QPainter::Antialiasing);
      int hov = -1; qreal hd = 10;
      for (int i = 0; i < T.size(); ++i) {
        const QPointF q(X(xOf(T[i])), Y(conv(T[i].net())));
        if (std::isfinite(mouse.x())) { const qreal d = std::hypot(q.x() - mouse.x(), q.y() - mouse.y()); if (d < hd) { hd = d; hov = i; } }
        p.setPen(Qt::NoPen); p.setBrush(T[i].net() < 0 ? c().redDot : c().cyanDot);
        p.drawEllipse(q, 4, 4);
      }
      if (hov >= 0) {
        const QPointF q(X(xOf(T[hov])), Y(conv(T[hov].net())));
        p.setPen(QPen(theme::map(QColor(Qt::white)), 1.5)); p.setBrush(Qt::NoBrush); p.drawEllipse(q, 6, 6);
        tooltip(p, mouse, tradeTip(T[hov]), r);
      }
    });
  };
  scatter(1, false); scatter(2, true);

  // 4-9. bar charts
  auto weekdayChart = [this, conv, axisFmt, valueText](int idx, bool counts) {
    PaintPanel* w = m_charts[idx];
    w->setPaint([this, conv, axisFmt, valueText, counts, w](QPainter& p, const QRectF& r, const QPointF& mouse, double anim) {
      HBars b;
      b.plot = QRectF(107, 36, r.width() - 107 - 32, r.height() - 36 - w->property("padB").toInt());
      for (const auto& w : m_d.stats.byWeekday) { b.labels << w.label; b.values << (counts ? w.count : conv(w.pnl)); }
      if (counts) { b.countAxis = true; for (int i = 0; i < b.values.size(); ++i) b.colors << c().neutralBar; b.xfmt = [](double v) { return QString::number(int(v)); }; }
      else b.xfmt = axisFmt;
      if (m_phone) b.nticks = 4;                    // phones: ~270 px of axis for 80 px labels
      const int h = drawHBars(p, b, anim, mouse);
      if (h >= 0) {
        const auto& w = m_d.stats.byWeekday[h];
        QList<TipLine> L{{w.label, c().white, true}, {(w.count == 1 ? QCoreApplication::translate("account", "%1 trade") : QCoreApplication::translate("account", "%1 trades")).arg(w.count), c().white}};
        if (!counts) L << TipLine{valueText(w.pnl), w.pnl < 0 ? c().redBar : c().cyanBar};
        tooltip(p, mouse, L, QRectF(r.left(), b.plot.top(), r.width(), b.plot.height() + 4));   // flips above near the plot bottom (t028)
      }
    });
  };
  weekdayChart(3, false); weekdayChart(6, true);

  auto tradesChart = [this, conv, axisFmt, tradeTip](int idx, bool bottom) {
    PaintPanel* w = m_charts[idx];
    w->setPaint([this, conv, axisFmt, tradeTip, bottom, w](QPainter& p, const QRectF& r, const QPointF& mouse, double anim) {
      const QList<account::Trade>& pick = bottom ? m_bottom5 : m_top5;   // picked in setData, not at every paint
      HBars b;
      b.plot = QRectF(115, 36, r.width() - 115 - 32, r.height() - 36 - w->property("padB").toInt());
      for (const auto& t : pick) {
        b.labels << account::displaySymbol(t.symbol);
        b.sub << t.close.toLocalTime().toString("dd/MM/yyyy");
        b.values << conv(t.net());
      }
      b.anchorRight = bottom; b.nticks = m_phone ? 4 : bottom ? 5 : 6; b.xfmt = axisFmt;
      if (pick.isEmpty()) {
        p.setFont(chart(12)); p.setPen(c().faint);
        p.drawText(r, Qt::AlignCenter, bottom ? QCoreApplication::translate("account", "No losing trade") : QCoreApplication::translate("account", "No winning trade"));
        return;
      }
      const int h = drawHBars(p, b, anim, mouse);
      if (h >= 0) tooltip(p, mouse, tradeTip(pick[h]), QRectF(r.left(), b.plot.top(), r.width(), b.plot.height() + 4));   // flips above near the plot bottom (t028)
    });
  };
  tradesChart(4, false); tradesChart(7, true);

  auto symbolsChart = [this, conv, axisFmt, valueText](int idx, bool counts) {
    PaintPanel* w = m_charts[idx];
    w->setPaint([this, conv, axisFmt, valueText, counts, w](QPainter& p, const QRectF& r, const QPointF& mouse, double anim) {
      HBars b;
      b.plot = QRectF(107, 36, r.width() - 107 - 32, r.height() - 36 - w->property("padB").toInt());
      const auto top = m_d.stats.bySymbol.mid(0, 5);
      for (const auto& s : top) { b.labels << s.label; b.values << (counts ? s.count : conv(s.pnl)); }
      if (counts) { b.countAxis = true; for (int i = 0; i < b.values.size(); ++i) b.colors << c().neutralBar; b.xfmt = [](double v) { return QString::number(int(v)); }; }
      else b.xfmt = axisFmt;
      if (m_phone) b.nticks = 4;
      const int h = drawHBars(p, b, anim, mouse);
      if (h >= 0) {
        const auto& s = top[h];
        QList<TipLine> L{{s.label, c().white, true}, {(s.count == 1 ? QCoreApplication::translate("account", "%1 trade") : QCoreApplication::translate("account", "%1 trades")).arg(s.count), c().white}};
        L << TipLine{valueText(s.pnl), s.pnl < 0 ? c().redBar : c().cyanBar};
        tooltip(p, mouse, L, QRectF(r.left(), b.plot.top(), r.width(), b.plot.height() + 4));   // flips above near the plot bottom (t028)
      }
    });
  };
  symbolsChart(5, false); symbolsChart(8, true);
}

int AdvancedView::relayout(int W, int H) {
  m_phone = ui::compact(W);
  if (m_phone) {
    // phones: one column of charts; the header's $ / % switch gets its own row
    m_head->setGeometry(10, 10, W - 20, 69);
    int y = 10 + 69 + 10;
    for (auto* ch : m_charts) { ch->setGeometry(10, y, W - 20, 280); ch->setProperty("padB", 39); y += 280 + 11; }
    return y - 11 + 10;
  }
  m_head->setGeometry(10, 10, W - 20, 27);
  // three columns down to 1200 px; below, two columns (the last chart spans both)
  const int cols = W >= 1200 ? 3 : 2;
  const qreal cw = (W - 20 - (cols - 1) * 10) / qreal(cols);
  const int base[3] = {302, 279, 290};                // reference row heights (rows of the 3-column grid)
  const int rows = (9 + cols - 1) / cols;
  // tall windows: rows share the extra height (reference content: 950 px)
  const int need = 47 + (cols == 3 ? 302 + 11 + 279 + 11 + 290 : rows * 300 + (rows - 1) * 11) + 10;
  const int extra = cols == 3 ? std::max(0, H - need) / 3 : 0;
  int y = 47;
  for (int row = 0; row < rows; ++row) {
    const int h = cols == 3 ? base[row] + extra : 300;
    for (int col = 0; col < cols; ++col) {
      const int i = row * cols + col;
      if (i >= 9) break;
      const bool span = cols == 2 && i == 8;
      m_charts[i]->setGeometry(QRectF(10 + col * (cw + 10), y, span ? W - 20 : cw, h).toRect());
      m_charts[i]->setProperty("padB", cols == 3 && row == 2 ? 50 : 39);   // reference: 204 px plots in 279/290 px panels
    }
    y += h + (row + 1 < rows ? 11 : 0);
  }
  return y + 10;
}

void AdvancedView::setData(const TrData& d) {
  m_d = d;
  // the five best and worst trades: every paint of their charts (each mouse move, each animation frame) copied and
  // sorted all the trades
  for (const bool bottom : {false, true}) {
    QList<account::Trade> list = m_d.trades;
    std::reverse(list.begin(), list.end());                       // newest first for ties
    std::stable_sort(list.begin(), list.end(), [bottom](const account::Trade& a, const account::Trade& b) {
      return bottom ? a.net() < b.net() : a.net() > b.net();
    });
    QList<account::Trade>& pick = bottom ? m_bottom5 : m_top5;
    pick.clear();
    for (const auto& t : list) { if (pick.size() == 5) break; if (bottom ? t.net() < 0 : t.net() > 0) pick << t; }
  }
  if (m_d.balance <= 0) m_pct = false;
  m_head->update();
  const double dd = m_d.stats.maxDD;
  m_charts[0]->setTitle(QCoreApplication::translate("account", "DRAWDOWN · MAX %1").arg(m_pct ? fmt::num(dd / m_d.balance * 100, 2) + "%" : money(dd, m_d.currency)),
                        QCoreApplication::translate("account", "Distance of the equity curve below its previous peak after each trade."));
  for (auto* w : m_charts) w->update();
}

void AdvancedView::playEntry() { for (int i = 3; i < 9; ++i) m_charts[i]->animate(500); }
}
