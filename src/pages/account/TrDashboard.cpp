#include "pages/account/TrackRecord.h"
#include <QCoreApplication>
#include "pages/account/TrCharts.h"
#include "account/Contracts.h"
#include "ui/Format.h"
#include "core/I18n.h"
#include "ui/Theme.h"
#include "ui/Touch.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QMap>
#include <algorithm>

namespace acct {
namespace {


void chip(QPainter& p, const QPointF& ctr, const QString& t, const QColor& fg, const QColor& bg) {
  const QRectF r(ctr.x() - 10, ctr.y() - 8, 20, 16);
  p.setPen(Qt::NoPen); p.setBrush(bg); p.drawRoundedRect(r, 2, 2);
  p.setFont(mono(11, true)); p.setPen(fg); p.drawText(r, Qt::AlignCenter, t);
}

// Monotone cubic (Fritsch–Carlson) through the points: smooth like the reference, no overshoot.
QPainterPath smoothPath(const QVector<QPointF>& p) {
  QPainterPath path;
  const int n = p.size();
  if (n == 0) return path;
  path.moveTo(p[0]);
  if (n < 3) { for (int i = 1; i < n; ++i) path.lineTo(p[i]); return path; }
  QVector<double> d(n - 1), m(n);
  for (int i = 0; i < n - 1; ++i) d[i] = (p[i + 1].y() - p[i].y()) / std::max(1e-9, p[i + 1].x() - p[i].x());
  m[0] = d[0]; m[n - 1] = d[n - 2];
  for (int i = 1; i < n - 1; ++i) m[i] = d[i - 1] * d[i] <= 0 ? 0 : (d[i - 1] + d[i]) / 2;
  for (int i = 0; i < n - 1; ++i) {
    if (d[i] == 0) { m[i] = m[i + 1] = 0; continue; }
    const double a = m[i] / d[i], b = m[i + 1] / d[i], h = a * a + b * b;
    if (h > 9) { const double t = 3 / std::sqrt(h); m[i] = t * a * d[i]; m[i + 1] = t * b * d[i]; }
  }
  for (auto& v : m) v *= 0.55;           // softer than a full monotone curve: straight runs, rounded turns
  for (int i = 0; i < n - 1; ++i) {
    const double dx = (p[i + 1].x() - p[i].x()) / 3;
    path.cubicTo(QPointF(p[i].x() + dx, p[i].y() + m[i] * dx), QPointF(p[i + 1].x() - dx, p[i + 1].y() - m[i + 1] * dx), p[i + 1]);
  }
  return path;
}

QColor scoreColor(double s) {
  const QColor a = theme::map("#df5f61"), b = theme::map("#e88a4f"), m = theme::map("#eebc52"), g = theme::map("#4caf50");
  const double t = std::clamp(s / 100.0, 0.0, 1.0);
  if (t < 0.33) return theme::mix(a, b, t / 0.33);
  if (t < 0.66) return theme::mix(b, m, (t - 0.33) / 0.33);
  return theme::mix(m, g, (t - 0.66) / 0.34);
}
}

DashboardView::DashboardView(QWidget* parent) : TrView(parent) {
  const QStringList kt{QCoreApplication::translate("account", "NET P&L"), QCoreApplication::translate("account", "TRADE WIN %"),
                       QCoreApplication::translate("account", "PROFIT FACTOR"), QCoreApplication::translate("account", "MAX DRAWDOWN"),
                       QCoreApplication::translate("account", "AVG WIN / LOSS TRADE")};
  const QStringList tips{
    QCoreApplication::translate("account", "Sum of the closed trades' results (profit + commission + swap) in the account currency."),
    QCoreApplication::translate("account", "Winning trades ÷ closed trades. Chips: wins · breakeven · losses."),
    QCoreApplication::translate("account", "Gross profit ÷ gross loss. Ring: share of gross profit (cyan) and gross loss (red)."),
    QCoreApplication::translate("account", "Largest fall of the equity curve from a peak, in money and in % of the equity at that peak. "
                                           "\"now\" is the current distance from the last peak."),
    QCoreApplication::translate("account", "Average winning trade ÷ average losing trade.")};
  for (int i = 0; i < 5; ++i) {
    auto* k = new PaintPanel(this);
    k->setTitle(kt[i], tips[i], c().grey, 10, 2.1);
    m_kpi << k;
  }
  m_score = new PaintPanel(this); m_score->setHighlight(true);
  m_cum = new PaintPanel(this); m_cum->setTitle(QCoreApplication::translate("account", "TRADE NET CUMULATIVE P&L"), QCoreApplication::translate("account", "Running total of the net P&L, one point per trading day."), c().grey, 10, 2.1);
  m_cum->setHoverable(true);
  m_month = new PaintPanel(this); m_month->setTitle(QCoreApplication::translate("account", "MONTH P&L"), QCoreApplication::translate("account", "Net P&L per calendar month (close date, local time)."), c().grey, 10, 2.1);
  m_quarter = new PaintPanel(this); m_quarter->setTitle(QCoreApplication::translate("account", "QUARTER P&L"), QCoreApplication::translate("account", "Net P&L per calendar quarter."), c().grey, 10, 2.1);
  m_last = new PaintPanel(this); m_last->setTitle(QCoreApplication::translate("account", "LAST TRADES P&L"), QCoreApplication::translate("account", "Net P&L of the last 40 trades, oldest on the left."), c().grey, 10, 2.1);
  for (auto* w : {m_month, m_quarter, m_last}) w->setHoverable(true);
  m_read = new PaintPanel(this); m_read->setHighlight(true);

  // ---- KPI cards
  m_kpi[0]->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    const auto& s = m_d.stats;
    p.setFont(mono(30, true)); p.setPen(s.net < 0 ? c().redBig : c().cyanBig);
    p.drawText(QPointF(13, 61), money(s.net, m_d.currency));
    const QString pill = (s.n == 1 ? QCoreApplication::translate("account", "%1 closed trade") : QCoreApplication::translate("account", "%1 closed trades")).arg(s.n);
    p.setFont(mono(11));
    const qreal w = QFontMetricsF(p.font()).horizontalAdvance(pill) + 14;
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(theme::map("#383224")); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(14.5, 72.5, w, 18), 3, 3);
    p.setPen(c().grey); p.drawText(QRectF(14.5, 72.5, w, 18), Qt::AlignCenter, pill);
    Q_UNUSED(r);
  });
  m_kpi[1]->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double anim) {
    const auto& s = m_d.stats;
    p.setFont(mono(28, true)); p.setPen(c().white2);
    p.drawText(QPointF(13, 74), std::isfinite(s.winRate) ? fmt::num(s.winRate * 100, 2) + "%" : fmt::dash());
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF ctr(r.width() - 38, 73);
    const QRectF arc(ctr.x() - 34, ctr.y() - 34, 68, 68);
    const double wr = std::isfinite(s.winRate) ? s.winRate : 0;
    QPen pen(theme::map("#fb867c"), 8, Qt::SolidLine, Qt::FlatCap);
    p.setPen(pen); p.drawArc(arc, 0, 180 * 16);
    pen.setColor(theme::map("#2fe1f5")); p.setPen(pen);
    p.drawArc(arc, 180 * 16, int(-180 * 16 * wr * anim));
    chip(p, QPointF(r.width() - 80, 90), QString::number(s.wins), theme::map("#2fd8ea"), theme::map("#0b2a30"));
    chip(p, QPointF(r.width() - 57, 90), QString::number(s.breakeven), theme::map("#8a8383"), theme::map("#1c1a18"));
    chip(p, QPointF(r.width() - 33, 90), QString::number(s.losses), theme::map("#ff6f66"), theme::map("#2f0b08"));
  });
  m_kpi[2]->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double anim) {
    const auto& s = m_d.stats;
    p.setFont(mono(28, true)); p.setPen(std::isfinite(s.profitFactor) && s.profitFactor < 1 ? c().redBig : theme::map("#3ed2e5"));
    p.drawText(QPointF(13, 74), std::isinf(s.profitFactor) ? "∞" : std::isfinite(s.profitFactor) ? fmt::num(s.profitFactor, 2) : fmt::dash());
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF ctr(r.width() - 47, 64);
    const QRectF ring(ctr.x() - 25, ctr.y() - 25, 50, 50);
    const double tot = s.grossProfit + s.grossLoss;
    const double lossShare = tot > 0 ? s.grossLoss / tot : 0;
    QPen pen(theme::map("#22d8ea"), 8, Qt::SolidLine, Qt::FlatCap);
    p.setPen(pen); p.drawEllipse(ring);
    if (lossShare > 0) {
      pen.setColor(theme::map("#ff897d")); p.setPen(pen);
      p.drawArc(ring, 90 * 16, int(360 * 16 * lossShare * anim));    // counter-clockwise from 12 o'clock
    }
  });
  m_kpi[3]->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    const auto& s = m_d.stats;
    p.setFont(mono(28, true)); p.setPen(c().redBig);
    p.drawText(QPointF(13, 55), money(s.maxDD, m_d.currency, 0));
    p.setFont(mono(11)); p.setPen(c().dim);
    p.drawText(QPointF(14, 73), QCoreApplication::translate("account", "%1 · now %2").arg(std::isfinite(s.maxDDPct) ? fmt::num(s.maxDDPct * 100, 2) + "%" : fmt::dash(), money(s.currentDD, m_d.currency, 0)));   // "—": no starting balance
    // mini drawdown curve hanging from the zero line
    const QRectF sp(185, 32, r.width() - 12 - 185, 44);
    if (s.drawdown.size() >= 2 && s.maxDD < 0) {
      p.setRenderHint(QPainter::Antialiasing);
      QPainterPath line;
      const int n = s.drawdown.size();
      for (int i = 0; i < n; ++i) {
        const QPointF pt(sp.left() + sp.width() * i / (n - 1), sp.top() + s.drawdown[i] / s.maxDD * sp.height());
        if (i == 0) line.moveTo(pt); else line.lineTo(pt);
      }
      QPainterPath area = line; area.lineTo(sp.right(), sp.top()); area.lineTo(sp.left(), sp.top()); area.closeSubpath();
      p.fillPath(area, theme::map("#56200d"));
      p.setPen(QPen(c().redText, 1.5)); p.setBrush(Qt::NoBrush); p.drawPath(line);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen dash(theme::map("#7a3b33"), 1, Qt::DashLine); p.setPen(dash);
    p.drawLine(QPointF(sp.left(), sp.top() + 0.5), QPointF(sp.right(), sp.top() + 0.5));
  });
  m_kpi[4]->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double anim) {
    const auto& s = m_d.stats;
    p.setFont(mono(28, true)); p.setPen(c().white2);
    p.drawText(QPointF(13, 59), std::isfinite(s.winLossRatio) ? fmt::num(s.winLossRatio, 2) : fmt::dash());
    const QRectF bar(94, 35, r.width() - 14 - 94, 6);
    p.setRenderHint(QPainter::Antialiasing);
    const double aw = std::isfinite(s.avgWin) ? s.avgWin : 0, al = std::isfinite(s.avgLoss) ? -s.avgLoss : 0;
    const double share = aw + al > 0 ? aw / (aw + al) : 0.5;
    p.setPen(Qt::NoPen);
    p.setBrush(theme::map("#ff837b")); p.drawRoundedRect(bar, 3, 3);
    QPainterPath clip; clip.addRect(QRectF(bar.left(), bar.top() - 1, bar.width() * share * anim, bar.height() + 2));
    p.save(); p.setClipPath(clip); p.setBrush(theme::map("#21dae8")); p.drawRoundedRect(bar, 3, 3); p.restore();
    p.fillRect(QRectF(bar.left() + bar.width() * share * anim - 1, bar.top(), 2, bar.height()), c().panel);
    p.setFont(mono(11, true));
    p.setPen(theme::map("#21dae8")); p.drawText(QPointF(bar.left(), 57), money(s.avgWin, m_d.currency, 0));
    const QString l = money(s.avgLoss, m_d.currency, 0);
    p.setPen(theme::map("#ff837b"));
    p.drawText(QPointF(bar.right() - QFontMetricsF(p.font()).horizontalAdvance(l), 57), l);
  });

  // ---- performance score
  m_score->setToolTip(QCoreApplication::translate("account",
                      "Deepwater's own score, not an industry standard. Each axis is put on a 0–100 scale:\n"
                      "Win % = win rate · Profit factor = PF ÷ 3 · Avg win/loss = ratio ÷ 2.5 ·\n"
                      "Recovery factor = net P&L ÷ max drawdown ÷ 5 · Max drawdown = 100 at 0%, 0 at −50% ·\n"
                      "Consistency = share of profitable months. Score = plain average of the six.\n"
                      "Green dots: axes at 70 or more."));
  m_score->setPaint([this](QPainter& p, const QRectF& r0, const QPointF&, double anim) {
    const auto& s = m_d.stats;
    p.setRenderHint(QPainter::Antialiasing);
    const qreal dy = std::max(0.0, (r0.height() - 386) / 2.0);   // taller panel: content stays centred
    p.translate(0, dy);
    const QRectF r(0, 0, r0.width(), 386);
    // title with pulsing dot (top of the panel, not centred)
    const double a = 0.55 + 0.45 * (0.5 + 0.5 * std::cos(m_phase * 2 * M_PI));
    p.translate(0, -dy);
    p.setPen(Qt::NoPen); p.setBrush(theme::alpha(theme::map("#e0c566"), int(255 * a)));
    p.drawEllipse(QPointF(15.5, 19.5), 2.5, 2.5);
    p.setFont(mono(10, true, 2)); p.setPen(theme::map("#e9d258"));
    p.drawText(QPointF(22, 23), QCoreApplication::translate("account", "PERFORMANCE SCORE"));
    p.translate(0, dy);
    // radar
    const QPointF ctr(r.width() / 2.0, 163);
    const double R = 91;
    auto pt = [&](int i, double f) { const double ang = (-90 + 60 * i) * M_PI / 180; return ctr + QPointF(std::cos(ang), std::sin(ang)) * R * f; };
    p.setPen(QPen(theme::map("#2a2414"), 1)); p.setBrush(Qt::NoBrush);
    for (int g = 1; g <= 4; ++g) { QPolygonF h; for (int i = 0; i < 6; ++i) h << pt(i, g / 4.0); h << h.first(); p.drawPolyline(h); }
    for (int i = 0; i < 6; ++i) p.drawLine(ctr, pt(i, 1));
    QPolygonF poly;
    for (int i = 0; i < 6 && i < s.axes.size(); ++i) {
      const double v = std::isfinite(s.axes[i].score) ? s.axes[i].score / 100.0 : 0;
      poly << pt(i, v * anim);
    }
    if (poly.size() == 6) {
      p.setBrush(theme::alpha(theme::map("#dcb84a"), 48)); p.setPen(QPen(theme::map("#dcb84a"), 2));
      p.drawPolygon(poly);
      for (int i = 0; i < 6; ++i) {
        const bool good = std::isfinite(s.axes[i].score) && s.axes[i].score >= 70;
        p.setPen(Qt::NoPen); p.setBrush(good ? theme::map("#6aa866") : theme::map("#dcb84a"));
        p.drawEllipse(poly[i], 3, 3);
      }
    }
    p.setFont(cond(11, true)); p.setPen(theme::map("#d6d0c0"));
    for (int i = 0; i < 6 && i < s.axes.size(); ++i) {
      const QPointF lp = pt(i, 126.0 / R);
      p.drawText(QRectF(lp.x() - 60, lp.y() - 8, 120, 16), Qt::AlignCenter, s.axes[i].name);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(QRectF(12, 305, r.width() - 24, 1), theme::map("#2a2414"));
    p.setRenderHint(QPainter::Antialiasing);
    // score line
    const QString sc = std::isfinite(s.score) ? fmt::num(s.score, 1) : fmt::dash();
    const QFont fs = cond(11, false), fb = mono(26, true);
    const qreal w1 = QFontMetricsF(fs).horizontalAdvance(QCoreApplication::translate("account", "SCORE")), w2 = QFontMetricsF(fb).horizontalAdvance(sc),
                w3 = QFontMetricsF(fs).horizontalAdvance("/ 100");
    qreal x = r.width() / 2.0 - (w1 + w2 + w3 + 16) / 2;
    p.setFont(fs); p.setPen(theme::map("#4a463c")); p.drawText(QPointF(x, 341), QCoreApplication::translate("account", "SCORE")); x += w1 + 8;
    p.setFont(fb); p.setPen(std::isfinite(s.score) ? theme::mix(scoreColor(s.score), theme::map("#f0d080"), 0.35) : c().dim);
    p.drawText(QPointF(x, 341), sc); x += w2 + 8;
    p.setFont(fs); p.setPen(theme::map("#4a463c")); p.drawText(QPointF(x, 341), "/ 100");
    const QRectF gb(r.width() / 2.0 - 125, 355, 250, 5);
    QLinearGradient g(gb.topLeft(), gb.topRight());
    g.setColorAt(0, theme::map("#df5f61")); g.setColorAt(0.35, theme::map("#e88a4f")); g.setColorAt(0.65, theme::map("#eebc52")); g.setColorAt(1, theme::map("#4caf50"));
    p.setPen(Qt::NoPen); p.setBrush(g); p.drawRoundedRect(gb, 2.5, 2.5);
    p.setFont(cond(10, false)); p.setPen(c().ghost);
    p.drawText(QRectF(10, 362, r.width() - 20, m_phone ? 44 : 22), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
               m_d.demo ? QCoreApplication::translate("account", "This app's own average of the six metrics above (sample trades) — not an industry-standard score.")
                        : QCoreApplication::translate("account", "This app's own average of the six real metrics above — not an industry-standard score."));
  });

  // ---- cumulative P&L
  m_cum->setPaint([this](QPainter& p, const QRectF& r, const QPointF& mouse, double) {
    const auto& days = m_d.stats.days;
    const int n = days.size();
    if (n == 0) return;
    const QRectF pl(84, 45, r.width() - 84 - 30, r.height() - 45 - 44);
    double lo = 0, hi = 0;
    for (const auto& d : days) { lo = std::min(lo, d.cum); hi = std::max(hi, d.cum); }
    double a0, a1;
    const auto tk = ticks(lo, hi, 5, &a0, &a1);
    auto Y = [&](double v) { return pl.bottom() - (v - a0) / (a1 - a0) * pl.height(); };
    auto X = [&](int i) { return n == 1 ? pl.center().x() : pl.left() + pl.width() * i / (n - 1); };
    p.setFont(chart(11));
    for (double t : tk) {
      const qreal y = std::round(Y(t)) + 0.5;
      if (t != 0) { p.setPen(c().grid); p.drawLine(QPointF(pl.left(), y), QPointF(pl.right(), y)); }
      p.setPen(c().axis);
      p.drawText(QRectF(0, y - 8, pl.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, axisMoney(t, m_d.currency));
    }
    QVector<QPointF> pts;
    for (int i = 0; i < n; ++i) pts << QPointF(X(i), Y(days[i].cum));
    const QPainterPath line = smoothPath(pts);
    const qreal y0 = Y(0);
    QPainterPath area = line; area.lineTo(X(n - 1), y0); area.lineTo(X(0), y0); area.closeSubpath();
    QLinearGradient g(0, pl.top(), 0, y0);
    g.setColorAt(0, theme::map("#0f3131")); g.setColorAt(1, theme::map("#091814"));
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(area, g);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(c().zero, 1, Qt::DashLine));
    p.drawLine(QPointF(pl.left(), std::round(y0) + 0.5), QPointF(pl.right(), std::round(y0) + 0.5));
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c().cyanLine, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    p.drawPath(line);
    const int every = std::max(1, int(std::ceil(n / (m_phone ? 4.0 : 12.0))));   // phones: ~300 px of plot, 80 px labels
    p.setPen(c().axis);
    for (int i = 0; i < n; i += every)
      p.drawText(QRectF(X(i) - 40, pl.bottom() + 8, 80, 16), Qt::AlignCenter, days[i].date.toString(i18n::current() == i18n::Lang::English ? "MM/dd/yy" : "dd/MM/yy"));
    if (std::isfinite(mouse.x()) && pl.adjusted(-6, -10, 6, 10).contains(mouse)) {
      const int i = std::clamp(int(std::round(n == 1 ? 0 : (mouse.x() - pl.left()) / pl.width() * (n - 1))), 0, n - 1);
      const qreal x = std::round(X(i)) + 0.5;
      p.setRenderHint(QPainter::Antialiasing, false);
      p.setPen(QPen(c().dim, 1, Qt::DashLine)); p.drawLine(QPointF(x, pl.top()), QPointF(x, pl.bottom()));
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(theme::map("#7183c4"), 2)); p.setBrush(theme::map("#dfe7fb"));
      p.drawEllipse(QPointF(X(i), Y(days[i].cum)), 4, 4);
      const double dv = days[i].pnl;
      tooltip(p, mouse, {{days[i].date.toString("yyyy-MM-dd"), c().dim},
                         {QCoreApplication::translate("account", "Day "), c().white, false, money(dv, m_d.currency), dv < 0 ? c().redBar : c().cyanBar},
                         {QCoreApplication::translate("account", "Cumulative "), c().white, false, money(days[i].cum, m_d.currency), c().white, true}}, r);
    }
  });

  // ---- month / quarter / last trades
  auto bucketChart = [this](PaintPanel* w, int kind) {
    w->setPaint([this, kind](QPainter& p, const QRectF& r, const QPointF& mouse, double anim) {
      VBars b;
      b.plot = QRectF(74, 43, r.width() - 74 - 21, r.height() - 43 - 41);
      b.yfmt = [this](double v) { return axisMoney(v, m_d.currency); };
      QStringList tipTitle; QVector<int> counts;
      if (kind == 0 || kind == 1) {
        const auto& src = kind == 0 ? m_d.stats.byMonth : m_d.stats.byQuarter;
        for (const auto& bk : src) { b.labels << bk.label; b.values << bk.pnl; counts << bk.count; tipTitle << bk.label; }
        b.labelEvery = kind == 0 ? std::max(1, int(std::ceil(src.size() / 4.0))) : std::max(1, int(std::ceil(src.size() / 6.0)));
        b.barWidth = 30;
      } else {
        const int n = m_d.trades.size(), from = std::max(0, n - 40);
        for (int i = from; i < n; ++i) {
          const auto& t = m_d.trades[i];
          b.labels << t.close.toLocalTime().toString(i18n::current() == i18n::Lang::English ? "MM/dd" : "dd/MM"); b.values << t.net();
          tipTitle << account::displaySymbol(t.symbol) + " · " + account::sideName(t.side);
        }
        b.labelEvery = m_phone ? 10 : 5; b.barWidth = 0.6;   // phones: 8 px per bar, a date every 5 bars touched
      }
      const int h = drawVBars(p, b, anim, mouse);
      if (h >= 0) {
        QList<TipLine> L;
        const double v = b.values[h];
        if (kind < 2) {
          L << TipLine{tipTitle[h], c().white, true}
            << TipLine{(counts[h] == 1 ? QCoreApplication::translate("account", "%1 trade") : QCoreApplication::translate("account", "%1 trades")).arg(counts[h]), c().white};
        } else {
          const auto& t = m_d.trades[std::max(0, int(m_d.trades.size()) - 40) + h];
          L << TipLine{tipTitle[h], c().white, true} << TipLine{QCoreApplication::translate("account", "Closed %1").arg(longDate(t.close)), c().white};
        }
        L << TipLine{money(v, m_d.currency), v < 0 ? c().redBar : c().cyanBar};
        tooltip(p, mouse, L, r);
      }
    });
  };
  bucketChart(m_month, 0); bucketChart(m_quarter, 1); bucketChart(m_last, 2);

  // ---- automated read
  m_read->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#e0c566")); p.drawEllipse(QPointF(15.5, 19.5), 2.5, 2.5);
    p.setFont(mono(10, true, 2)); p.setPen(theme::map("#e9d258"));
    p.drawText(QPointF(22, 23), QCoreApplication::translate("account", "AUTOMATED READ"));
    if (m_d.trades.isEmpty()) return;
    const QStringList lines = readLines();
    p.setFont(cond(15, false));
    // prose lines stay readable (max 900 px); wide panels use two columns
    const int cols = r.width() >= 1900 ? 2 : 1;
    const qreal colW = std::min(900.0, (r.width() - 44 - (cols - 1) * 60) / cols);
    const int perCol = (int(lines.size()) + cols - 1) / cols;
    for (int col = 0; col < cols; ++col) {
      qreal y = 50;
      const qreal x0 = 22 + col * (colW + 60);
      for (int i = col * perCol; i < std::min<int>(lines.size(), (col + 1) * perCol); ++i) {
        const QRectF tr(x0, y, colW, 60);
        const QRectF br = p.boundingRect(tr, Qt::TextWordWrap, lines[i]);
        p.setPen(theme::map("#e6c200")); p.drawText(QPointF(x0 - 8, y + 13), "›");
        p.setPen(theme::map("#d6d0c0"));
        p.drawText(tr, Qt::TextWordWrap, lines[i]);
        y += br.height() + 6;
      }
    }
    p.setFont(cond(10, false)); p.setPen(c().ghost);
    // phones: the note wraps on two lines (relayout() leaves room for them)
    p.drawText(m_phone ? QRectF(22, r.height() - 36, r.width() - 44, 28) : QRectF(22, r.height() - 24, r.width() - 44, 14),
               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
               QCoreApplication::translate("account", "Rule-based read computed by Deepwater from the trades above%1 — not advice.")
               .arg(m_d.demo ? QCoreApplication::translate("account", " (invented sample trades)") : QString()));
  });

  m_pulse.setInterval(100);
  connect(&m_pulse, &QTimer::timeout, this, [this] {
    m_phase = std::fmod(m_phase + 0.1, 1.0);
    m_score->update(QRect(10, 14, 12, 12));
  });
}

QStringList DashboardView::readLines() const {
  const auto& s = m_d.stats;
  const auto& T = m_d.trades;
  if (T.isEmpty()) return {};
  const QString cc = m_d.currency;
  QStringList lines;
  lines << QCoreApplication::translate("account", "%1 over %2 trades, %3 → %4: %5% winners, profit factor %6, average win %7 against an average loss of %8.")
             .arg(signedMoney(s.net, cc)).arg(s.n).arg(longDate(T.first().close).section(',', 0, 0), longDate(T.last().close).section(',', 0, 0))
             .arg(fmt::num(s.winRate * 100, 1)).arg(std::isfinite(s.profitFactor) ? fmt::num(s.profitFactor, 2) : std::isinf(s.profitFactor) ? QString("∞") : fmt::dash())   // NaN: only breakeven trades
             .arg(money(s.avgWin, cc), money(s.avgLoss, cc));
  // drawdown and recovery
  if (s.maxDD < 0) {
    int trough = 0; for (int i = 0; i < s.drawdown.size(); ++i) if (s.drawdown[i] <= s.maxDD) { trough = i; break; }
    int recovered = -1; for (int i = trough; i < s.drawdown.size(); ++i) if (s.drawdown[i] >= 0) { recovered = i; break; }
    // without a starting balance the % is unknown: the sentence gives the money drop only
    lines << (std::isfinite(s.maxDDPct)
                ? QCoreApplication::translate("account", "Deepest drawdown %1 (%2%) reached on %3; %4")
                    .arg(money(s.maxDD, cc), fmt::num(s.maxDDPct * 100, 2), longDate(T[trough].close).section(',', 0, 0))
                : QCoreApplication::translate("account", "Deepest drawdown %1 reached on %2; %3")
                    .arg(money(s.maxDD, cc), longDate(T[trough].close).section(',', 0, 0)))
               .arg(recovered >= 0 ? QCoreApplication::translate("account", "recovered %1 trades later, on %2.").arg(recovered - trough).arg(longDate(T[recovered].close).section(',', 0, 0))
                                   : QCoreApplication::translate("account", "not recovered yet (%1 below the peak).").arg(money(s.currentDD, cc)));
  }
  // best / worst instrument and weekday
  if (s.bySymbol.size() >= 2)
    lines << QCoreApplication::translate("account", "Best instrument %1 (%2 over %3 trades); weakest %4 (%5 over %6).")
               .arg(s.bySymbol.first().label, signedMoney(s.bySymbol.first().pnl, cc)).arg(s.bySymbol.first().count)
               .arg(s.bySymbol.last().label, signedMoney(s.bySymbol.last().pnl, cc)).arg(s.bySymbol.last().count);
  if (s.byWeekday.size() >= 2) {
    auto best = *std::max_element(s.byWeekday.begin(), s.byWeekday.end(), [](auto& a, auto& b) { return a.pnl < b.pnl; });
    auto worst = *std::min_element(s.byWeekday.begin(), s.byWeekday.end(), [](auto& a, auto& b) { return a.pnl < b.pnl; });
    lines << QCoreApplication::translate("account", "Best day %1 (%2), weakest %3 (%4).").arg(best.label, signedMoney(best.pnl, cc), worst.label, signedMoney(worst.pnl, cc));
  }
  // holding time winners vs losers, streaks
  double hw = 0, hl = 0; int nw = 0, nl = 0, run = 0, bestRun = 0, worstRun = 0;
  for (const auto& t : T) {
    const double h = t.heldHours();
    if (t.net() > 0) { if (std::isfinite(h)) { hw += h; ++nw; } run = run > 0 ? run + 1 : 1; }
    else if (t.net() < 0) { if (std::isfinite(h)) { hl += h; ++nl; } run = run < 0 ? run - 1 : -1; }
    else run = 0;                                     // a breakeven trade ends a streak (A14)
    bestRun = std::max(bestRun, run); worstRun = std::min(worstRun, run);
  }
  QString hold;
  if (nw && nl) {
    const double aw = hw / nw, al = hl / nl;
    hold = QCoreApplication::translate("account", "Winners are held %1 on average, losers %2%3. ").arg(held(aw), held(al),
             al > aw * 1.2 ? QCoreApplication::translate("account", " — losers stay open longer")
                           : aw > al * 1.2 ? QCoreApplication::translate("account", " — winners are given room") : QString());
  }
  lines << hold + QCoreApplication::translate("account", "Longest winning streak %1, longest losing streak %2.").arg(bestRun).arg(-worstRun);
  return lines;
}

int DashboardView::relayout(int W, int H) {
  m_phone = ui::compact(W);
  if (m_phone) {
    // phones: one column, every panel the page width; the read is as tall as its wrapped sentences
    const int w = W - 20;
    int y = 10;
    for (auto* k : m_kpi) { k->setGeometry(10, y, w, 110); y += 110 + 11; }
    m_score->setGeometry(10, y, w, 420); y += 420 + 11;
    m_cum->setGeometry(10, y, w, 300); y += 300 + 11;
    for (auto* b : {m_month, m_quarter, m_last}) { b->setGeometry(10, y, w, 260); y += 260 + 11; }
    const QFontMetricsF fm(cond(15, false));
    int readH = 50 + 44;
    for (const auto& l : readLines()) readH += int(std::ceil(fm.boundingRect(QRectF(0, 0, w - 44, 4000), Qt::TextWordWrap, l).height())) + 6;
    m_read->setGeometry(10, y, w, readH);
    return y + readH + 10;
  }
  // KPI cards: one row of five when each card keeps >= 270 px (the drawdown sparkline and split bar need it),
  // otherwise three + two
  int y = 10;
  const qreal cw5 = (W - 20 - 4 * 11.5) / 5.0;
  if (cw5 >= 270) {
    for (int i = 0; i < 5; ++i) m_kpi[i]->setGeometry(QRectF(10 + i * (cw5 + 11.5), y, cw5, 110).toRect());
    y += 110 + 11;
  } else {
    const qreal cw3 = (W - 20 - 2 * 11.5) / 3.0, cw2 = (W - 20 - 11.5) / 2.0;
    for (int i = 0; i < 3; ++i) m_kpi[i]->setGeometry(QRectF(10 + i * (cw3 + 11.5), y, cw3, 110).toRect());
    for (int i = 0; i < 2; ++i) m_kpi[3 + i]->setGeometry(QRectF(10 + i * (cw2 + 11.5), y + 121, cw2, 110).toRect());
    y += 2 * 121;
  }
  // tall windows: the score / cumulative row and the bar row share the extra height (reference: 1077 px of content)
  const int extra = std::max(0, H - 1077);
  const int row2 = 415 + extra * 6 / 10, row3 = 301 + extra * 4 / 10;
  // score panel: one third, at least 420 px (radar labels), at most 700 px (the cumulative chart gets the rest)
  const int sw = std::clamp(int((W - 20 - 12) / 3.0), 420, 700);
  m_score->setGeometry(10, y, sw, row2 - 29);
  m_cum->setGeometry(10 + sw + 12, y, W - 10 - (10 + sw + 12), row2);
  y += row2 + 11;
  const qreal bw = (W - 20 - 24) / 3.0;
  for (int i = 0; i < 3; ++i) (i == 0 ? m_month : i == 1 ? m_quarter : m_last)->setGeometry(QRectF(10 + i * (bw + 12), y, bw, row3).toRect());
  y += row3 + 13;
  const int readH = W - 20 >= 1900 ? 150 : 196;     // two text columns on wide panels need less height
  m_read->setGeometry(10, y, W - 20, readH);
  return y + readH + 10;
}

void DashboardView::setData(const TrData& d) { m_d = d; for (auto* w : findChildren<PaintPanel*>()) w->update(); }

void DashboardView::playEntry() {
  for (auto* w : {m_month, m_quarter, m_last, m_score}) w->animate(500);
  for (auto* k : m_kpi) k->animate(500);
}
void DashboardView::showEvent(QShowEvent*) { m_pulse.start(); }
void DashboardView::hideEvent(QHideEvent*) { m_pulse.stop(); }
}
