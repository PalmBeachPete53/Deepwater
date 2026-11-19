#include "pages/account/TrackRecord.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include "account/Contracts.h"
#include "ui/Format.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLineEdit>
#include <QHelpEvent>
#include <QToolTip>
#include <algorithm>

namespace acct {

// The closed-trades table: header + rows, painted; only the exposed rows are drawn.
class TradeTable : public QWidget {
public:
  enum Col { Opened, Closed, Instrument, Side, Size, Entry, Exit, Stop, Target, Held, R, Strategy, Session, Pnl, NCols };
  explicit TradeTable(TradeLogView* /*owner*/, QWidget* parent) : QWidget(parent) { setMouseTracking(true); }
  void setRows(const QList<account::Trade>& rows, const QString& ccy) { m_rows = rows; m_ccy = ccy; sortRows(); update(); }
  int rowsHeight() const { return kTop + int(std::ceil(m_rows.size() * kPitch)) + 6; }
  std::function<void(const QString&)> onEdit, onDelete;
protected:
  static constexpr int kTop = 31;            // header band height
  static constexpr double kPitch = 36.4;
  struct ColDef { QString name; bool right; qreal x; };
  bool compact() const { return width() < 1440; }
  bool narrow() const { return width() < 1200; }   // also drops SESSION (row tooltip)
  QList<ColDef> cols() const {
    QList<ColDef> out{{QCoreApplication::translate("account", "OPENED"), false, -10000}, {QCoreApplication::translate("account", "CLOSED"), false, 0}, {QCoreApplication::translate("account", "INSTRUMENT"), false, 0}, {QCoreApplication::translate("account", "SIDE"), false, 0},
                      {QCoreApplication::translate("account", "SIZE"), true, 0}, {QCoreApplication::translate("account", "ENTRY"), true, 0}, {QCoreApplication::translate("account", "EXIT"), true, 0}, {QCoreApplication::translate("account", "STOP", "stop loss column"), true, 0}, {QCoreApplication::translate("account", "TARGET"), true, 0},
                      {QCoreApplication::translate("account", "HELD"), true, 0}, {QCoreApplication::translate("account", "R", "R multiple column"), true, 0}, {QCoreApplication::translate("account", "STRATEGY"), false, 0}, {QCoreApplication::translate("account", "SESSION"), false, -10000},
                      {QCoreApplication::translate("account", "P&L"), true, width() - 103.0}};
    if (!compact()) {
      // positions measured at 1600 px (table starts at x 10), scaled with the width
      const qreal s = width() / 1580.0;
      const qreal xs[13] = {22, 162, 303, 433, 579, 682, 784, 886, 988, 1067, 1141, 1159, 1255};
      for (int i = 0; i < 13; ++i) out[i].x = (xs[i] - 10) * s;
      return out;
    }
    // narrow windows: slots with minimum widths, the leftover spread evenly; OPENED (and SESSION below 1200 px)
    // move to the row tooltip
    struct Slot { int col; qreal w; };
    QList<Slot> sl{{Closed, 120}, {Instrument, 100}, {Side, 50}, {Size, 48}, {Entry, 74}, {Exit, 74}, {Stop, 74},
                   {Target, 74}, {Held, 50}, {R, 62}, {Strategy, 92}};
    if (!narrow()) sl << Slot{Session, 92};
    qreal total = 0; for (const auto& x : sl) total += x.w;
    const qreal avail = width() - 103 - 92 - 12;           // P&L slot (right edge at width - 103)
    const qreal extra = std::max(0.0, avail - total) / sl.size();
    qreal x = 12;
    for (const auto& x0 : sl) {
      const qreal w = x0.w + extra;
      out[x0.col].x = out[x0.col].right ? x + w - 12 : x;
      x += w;
    }
    return out;
  }
  qreal rowTop(int i) const { return std::round(kTop + i * kPitch); }
  void sortRows() {
    auto key = [&](const account::Trade& t) -> QVariant {
      switch (m_sort) {
        case Opened: return t.open; case Closed: return t.close; case Instrument: return account::displaySymbol(t.symbol);
        case Side: return int(t.side); case Size: return t.lots; case Entry: return t.entry; case Exit: return t.exit;
        case Stop: return t.sl; case Target: return t.tp; case Held: return t.heldHours(); case R: return t.rMultiple();
        case Strategy: return t.strategy; case Session: return account::session(t.open); default: return t.net();
      }
    };
    // empty values (no stop → R, no size…) are NAN: they compare "unordered" with everything, which broke the
    // strict weak ordering and jumbled the sort (A14). They go last in both directions.
    auto missing = [](const QVariant& v) {
      return (v.typeId() == QMetaType::Double && !std::isfinite(v.toDouble())) || (v.typeId() == QMetaType::QDateTime && !v.toDateTime().isValid());
    };
    std::stable_sort(m_rows.begin(), m_rows.end(), [&](const account::Trade& a, const account::Trade& b) {
      const QVariant ka = key(a), kb = key(b);
      const bool ma = missing(ka), mb = missing(kb);
      if (ma || mb) return !ma && mb;
      const auto o = QVariant::compare(ka, kb);
      const int cmp = o == QPartialOrdering::Less ? -1 : o == QPartialOrdering::Greater ? 1 : 0;
      return m_desc ? cmp > 0 : cmp < 0;
    });
  }
  void paintEvent(QPaintEvent* e) override {
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    const auto cl = cols();
    // header
    p.setFont(mono(10, true, compact() ? 1.0 : 2.4));   // narrow tables: tighter headers
    for (int i = 0; i < cl.size(); ++i) {
      const bool sorted = i == m_sort;
      p.setPen(sorted ? c().white : (i == m_hoverCol ? theme::map("#f0d070") : theme::map("#d9b54a")));
      QString t = cl[i].name;
      if (sorted) t += m_desc ? " ↓" : " ↑";
      if (cl[i].x < -1000) continue;
      const QRectF r = cl[i].right ? QRectF(cl[i].x - 150, 0, 150, kTop) : QRectF(cl[i].x, 0, 150, kTop);
      p.drawText(r, (cl[i].right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, t);
    }
    p.fillRect(QRectF(0, kTop - 1, width(), 1), theme::map("#2a2f25"));
    const int first = std::max(0, int((e->rect().top() - kTop) / kPitch) - 1);
    const int last = std::min<int>(m_rows.size() - 1, int((e->rect().bottom() - kTop) / kPitch) + 1);
    const QFont fDate = mono(width() >= 1440 ? 13 : width() >= 1346 ? 12 : 11), fInst = mono(14, true), fBadge = mono(10, true, 1), fPnl = mono(15, true);
    const QFontMetricsF fmS(mono(13));
    for (int i = first; i <= last; ++i) {
      const auto& t = m_rows[i];
      const qreal top = rowTop(i), bot = rowTop(i + 1);
      const qreal cy = (top + bot) / 2;
      const double v = t.net();
      const auto sp = account::spec(t.symbol);
      const int dec = std::min(5, std::max(sp.decimals, 2));
      p.fillRect(QRectF(1, top, 3, bot - top - 1), v > 0 ? theme::map("#43d9ed") : v < 0 ? theme::map("#f88c83") : c().dim);
      p.fillRect(QRectF(0, bot - 1, width(), 1), theme::map("#32372d"));
      auto text = [&](int col, const QString& s, const QColor& colr, const QFont& f, qreal maxW = 0) {
        if (cl[col].x < -1000) return;                     // column hidden at this width
        p.setFont(f); p.setPen(colr);
        const QString shown = maxW > 0 ? QFontMetricsF(f).elidedText(s, Qt::ElideRight, maxW) : s;
        const QRectF r = cl[col].right ? QRectF(cl[col].x - 200, top, 200, bot - top - 1) : QRectF(cl[col].x, top, 300, bot - top - 1);
        p.drawText(r, (cl[col].right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, shown);
      };
      if (!compact()) text(Opened, tradeDate(t.open), c().dim, fDate);
      text(Closed, tradeDate(t.close), c().white, fDate);
      text(Instrument, sp.display, c().white, fInst, cl[Side].x - cl[Instrument].x - 8);
      // side badge
      {
        const bool lng = t.side == account::Side::Long;
        const QRectF b(cl[Side].x, cy - 9, 42, 18);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(lng ? theme::map("#1d7f88") : theme::map("#862d26"), 1)); p.setBrush(lng ? theme::map("#052423") : theme::map("#2f0a0a"));
        p.drawRoundedRect(b.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setFont(fBadge); p.setPen(lng ? theme::map("#25a2ae") : theme::map("#e0605a"));
        p.drawText(b, Qt::AlignCenter, lng ? "LONG" : "SHORT");
      }
      text(Size, trimNum(t.lots, 2), c().white, fDate);
      text(Entry, trimNum(t.entry, dec), c().white, fDate);
      text(Exit, trimNum(t.exit, dec), c().white, fDate);
      text(Stop, trimNum(t.sl, dec), theme::map("#d0706a"), fDate);
      text(Target, trimNum(t.tp, dec), theme::map("#5fb877"), fDate);
      text(Held, held(t.heldHours()), c().grey, fDate);
      const double r = t.rMultiple();
      text(R, std::isfinite(r) ? fmt::signedNum(r, 2) + "R" : fmt::dash(), !std::isfinite(r) ? c().dim : r < 0 ? c().redText : theme::map("#3fb8e8"), fDate);
      text(Strategy, t.strategy.isEmpty() ? fmt::dash() : t.strategy, c().grey, mono(12), (cl[Session].x > -1000 ? cl[Session].x : width() - 103 - 92) - cl[Strategy].x - 6);
      text(Session, account::session(t.open), c().faint, mono(12));
      text(Pnl, signedMoney(v, m_ccy), v < 0 ? c().redText : theme::map("#3fc6ee"), fPnl);
      // actions
      const bool hov = i == m_hoverRow;
      drawEye(p, QPointF(width() - 45, cy), hov && m_hoverAct == 1 ? c().white : c().faint);
      drawTrash(p, QPointF(width() - 22, cy), hov && m_hoverAct == 2 ? c().redText : c().faint);
    }
  }
  static void drawEye(QPainter& p, const QPointF& ctr, const QColor& col) {
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(col, 1.2)); p.setBrush(Qt::NoBrush);
    QPainterPath e; e.moveTo(ctr + QPointF(-6.5, 0)); e.quadTo(ctr + QPointF(0, -6.5), ctr + QPointF(6.5, 0));
    e.quadTo(ctr + QPointF(0, 6.5), ctr + QPointF(-6.5, 0));
    p.drawPath(e); p.drawEllipse(ctr, 2, 2);
    p.restore();
  }
  static void drawTrash(QPainter& p, const QPointF& ctr, const QColor& col) {
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(col, 1.2)); p.setBrush(Qt::NoBrush);
    p.drawLine(ctr + QPointF(-5.5, -4), ctr + QPointF(5.5, -4));
    p.drawLine(ctr + QPointF(-2, -4), ctr + QPointF(-2, -5.5)); p.drawLine(ctr + QPointF(-2, -5.5), ctr + QPointF(2, -5.5));
    p.drawLine(ctr + QPointF(2, -5.5), ctr + QPointF(2, -4));
    p.drawRect(QRectF(ctr.x() - 4, ctr.y() - 4, 8, 9.5));
    p.drawLine(ctr + QPointF(-1.3, -1.5), ctr + QPointF(-1.3, 3.5)); p.drawLine(ctr + QPointF(1.3, -1.5), ctr + QPointF(1.3, 3.5));
    p.restore();
  }
  int rowAt(qreal y) const { if (y < kTop) return -1; const int i = int((y - kTop) / kPitch); return i < m_rows.size() ? i : -1; }
  int colAt(qreal x) const {
    const auto cl = cols();
    for (int i = 0; i < cl.size(); ++i) {
      const qreal w = QFontMetricsF(mono(10, true, compact() ? 1.0 : 2.4)).horizontalAdvance(cl[i].name) + 16;
      const QRectF r = cl[i].right ? QRectF(cl[i].x - w, 0, w, kTop) : QRectF(cl[i].x - 4, 0, w, kTop);
      if (r.contains(x, kTop / 2)) return i;
    }
    return -1;
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    const QPointF pt = e->position();
    const int row = rowAt(pt.y());
    int act = 0;
    if (row >= 0) { if (std::abs(pt.x() - (width() - 45)) < 9) act = 1; else if (std::abs(pt.x() - (width() - 22)) < 9) act = 2; }
    const int col = pt.y() < kTop ? colAt(pt.x()) : -1;
    if (row != m_hoverRow || act != m_hoverAct || col != m_hoverCol) {
      const int old = m_hoverRow;
      m_hoverRow = row; m_hoverAct = act; m_hoverCol = col;
      if (old >= 0) update(QRect(width() - 60, int(rowTop(old)), 60, 40));
      if (row >= 0) update(QRect(width() - 60, int(rowTop(row)), 60, 40));
      update(QRect(0, 0, width(), kTop));
    }
    setCursor(act || col >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }
  void leaveEvent(QEvent*) override { m_hoverRow = -1; m_hoverAct = 0; m_hoverCol = -1; update(); }
  void mousePressEvent(QMouseEvent* e) override {
    const QPointF pt = e->position();
    if (pt.y() < kTop) {
      const int col = colAt(pt.x());
      if (col < 0) return;
      if (col == m_sort) m_desc = !m_desc; else { m_sort = col; m_desc = col == Closed || col == Opened || col == Pnl || col == R; }
      sortRows(); update();
      return;
    }
    const int row = rowAt(pt.y());
    if (row < 0) return;
    if (std::abs(pt.x() - (width() - 45)) < 9 && onEdit) onEdit(m_rows[row].id);
    else if (std::abs(pt.x() - (width() - 22)) < 9 && onDelete) onDelete(m_rows[row].id);
  }
  bool event(QEvent* e) override {
    if (e->type() == QEvent::ToolTip) {
      auto* he = static_cast<QHelpEvent*>(e);
      const int row = rowAt(he->pos().y());
      QString tip;
      if (row >= 0) {
        const auto& t = m_rows[row];
        if (std::abs(he->pos().x() - (width() - 45)) < 9) tip = QCoreApplication::translate("account", "View / edit this trade");
        else if (std::abs(he->pos().x() - (width() - 22)) < 9) tip = QCoreApplication::translate("account", "Delete this trade");
        else if (he->pos().x() > cols()[Pnl].x - 120 && he->pos().x() < cols()[Pnl].x)
          tip = QCoreApplication::translate("account", "Profit %1 · commission %2 · swap %3").arg(money(t.pnl, m_ccy), money(t.commission, m_ccy), money(t.swap, m_ccy));
        else tip = QCoreApplication::translate("account", "Opened %1 · closed %2 · %3 session%4").arg(longDate(t.open), longDate(t.close), account::session(t.open),
                                                                    t.comment.isEmpty() ? QString() : "\n" + t.comment);
      }
      if (tip.isEmpty()) QToolTip::hideText(); else QToolTip::showText(he->globalPos(), tip, this);
      return true;
    }
    return QWidget::event(e);
  }
private:
  QList<account::Trade> m_rows;
  QString m_ccy;
  int m_sort = Closed; bool m_desc = true;
  int m_hoverRow = -1, m_hoverAct = 0, m_hoverCol = -1;
};

TradeLogView::TradeLogView(QWidget* parent) : TrView(parent) {
  const QStringList labels{QCoreApplication::translate("account", "CLOSED TRADES"), QCoreApplication::translate("account", "WINS / LOSSES"),
                           QCoreApplication::translate("account", "WIN RATE"), QCoreApplication::translate("account", "NET P&L"),
                           QCoreApplication::translate("account", "OPEN POSITIONS"), QCoreApplication::translate("account", "UNREALIZED")};
  for (int i = 0; i < 6; ++i) {
    auto* k = new PaintPanel(this);
    k->setPaint([this, i, labels](QPainter& p, const QRectF&, const QPointF&, double) {
      p.setFont(mono(9, false, 2.2)); p.setPen(c().dim);
      p.drawText(QPointF(12, 18), labels[i]);
      const auto& s = m_d.stats;
      QString v; QColor col = theme::map("#fbf9e5");
      switch (i) {
        case 0: v = QString::number(s.n); break;
        case 1: v = QString("%1 / %2").arg(s.wins).arg(s.losses); break;
        case 2: v = std::isfinite(s.winRate) ? fmt::num(s.winRate * 100, 1) + "%" : fmt::dash(); break;
        case 3: v = signedMoney(s.net, m_d.currency); col = s.net < 0 ? c().redBig : theme::map("#2ed8e5"); break;
        case 4: v = m_d.demo ? "0" : fmt::dash(); col = theme::map("#f9f1b1"); break;
        default: v = fmt::dash(); col = theme::map("#6fcad7"); break;
      }
      p.setFont(mono(20, true)); p.setPen(col);
      p.drawText(QPointF(13, 44), v);
    });
    if (i >= 4) k->setToolTip(QCoreApplication::translate("account", "The journal holds closed trades only: open positions and their unrealized P&L are not tracked."));
    m_kpi << k;
  }
  m_panel = new PaintPanel(this);
  m_panel->setPaint([this](QPainter& p, const QRectF&, const QPointF&, double) {
    p.setFont(mono(16, true, 1.5)); p.setPen(c().white);
    p.drawText(QPointF(12, 27), QCoreApplication::translate("account", "CLOSED TRADES"));
    const qreal w = QFontMetricsF(p.font()).horizontalAdvance(QCoreApplication::translate("account", "CLOSED TRADES"));
    p.setFont(mono(10, false, 2)); p.setPen(theme::map("#4a463c"));
    p.drawText(QPointF(12 + w + 10, 26), QString("%1 / %2").arg(m_table ? m_tableCount : 0).arg(m_d.trades.size()));
  });
  m_pills = new PaintPanel(m_panel);
  m_pills->setFrame(false); m_pills->setHoverable(true);
  const QStringList names{QCoreApplication::translate("account", "ALL"), QCoreApplication::translate("account", "WINS"),
                          QCoreApplication::translate("account", "LOSSES"), QCoreApplication::translate("account", "BREAKEVEN")};
  m_pillNames = names;
  auto pillRects = [names](const QRectF& r) {
    QList<QRectF> out; const QFontMetricsF fm(mono(10, true, 1.6));
    qreal x = r.right();
    QList<qreal> ws; for (const auto& n : names) ws << fm.horizontalAdvance(n) + 18;
    qreal total = 0; for (qreal w : ws) total += w;
    x = r.right() - total - 4;
    for (qreal w : ws) { out << QRectF(x, 2, w, r.height() - 4); x += w; }
    return out;
  };
  m_pills->setPaint([this, pillRects, names](QPainter& p, const QRectF& r, const QPointF& mouse, double) {
    p.setRenderHint(QPainter::Antialiasing);
    const auto rs = pillRects(r);
    p.setPen(theme::map("#2a2618")); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rs.first().left() - 2, 0.5, rs.last().right() - rs.first().left() + 4, r.height() - 1), 3, 3);
    p.setFont(mono(10, true, 1.6));
    for (int i = 0; i < 4; ++i) {
      const bool on = i == m_outcome, hov = std::isfinite(mouse.x()) && rs[i].contains(mouse);
      if (on) { p.setPen(Qt::NoPen); p.setBrush(theme::map("#ffda44")); p.drawRoundedRect(rs[i], 3, 3); }
      p.setPen(on ? theme::onAccent() : hov ? c().white : theme::map("#73725d"));
      p.drawText(rs[i].translated(1, 0), Qt::AlignCenter, names[i]);
    }
  });
  connect(m_pills, &PaintPanel::clicked, this, [this, pillRects](const QPointF& pt) {
    const auto rs = pillRects(QRectF(m_pills->rect()));
    for (int i = 0; i < 4; ++i) if (rs[i].contains(pt) && i != m_outcome) { m_outcome = i; m_pills->update(); refilter(); }
  });
  m_search = new QLineEdit(m_panel);
  m_search->setPlaceholderText(QCoreApplication::translate("account", "Search instrument, strategy"));
  m_search->setTextMargins(16, 0, 0, 0);
  m_search->setStyleSheet(qss("QLineEdit { background:#080602; border:1px solid #2a2618; border-radius:2px; color:#ece8de;"
                          " font-family:'JetBrains Mono'; font-size:11px; letter-spacing:1px; padding:2px 6px; }"
                          "QLineEdit:focus { border-color:#6a5f3c; }"));
  auto* mag = new PaintPanel(m_search);
  mag->setFrame(false); mag->setAttribute(Qt::WA_TransparentForMouseEvents);
  mag->setGeometry(6, 5, 14, 14);
  mag->setPaint([](QPainter& p, const QRectF&, const QPointF&, double) {
    p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(c().faint, 1.2)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(5.5, 6.5), 3.6, 3.6); p.drawLine(QPointF(8.2, 9.2), QPointF(10.8, 11.8));
  });
  connect(m_search, &QLineEdit::textChanged, this, [this] { refilter(); });
  m_table = new TradeTable(this, m_panel);
  m_table->onEdit = [this](const QString& id) { emit tradeEdit(id); };
  m_table->onDelete = [this](const QString& id) { emit tradeDelete(id); };
}

void TradeLogView::refilter() {
  QList<account::Trade> rows;
  const QString q = m_search->text().trimmed();
  for (const auto& t : m_d.trades) {
    const double v = t.net();
    if (m_outcome == 1 && v <= 0) continue;
    if (m_outcome == 2 && v >= 0) continue;
    if (m_outcome == 3 && v != 0) continue;
    if (!q.isEmpty() && !account::displaySymbol(t.symbol).contains(q, Qt::CaseInsensitive) && !t.symbol.contains(q, Qt::CaseInsensitive) &&
        !t.strategy.contains(q, Qt::CaseInsensitive) && !t.comment.contains(q, Qt::CaseInsensitive)) continue;
    rows << t;
  }
  m_tableCount = rows.size();
  m_table->setRows(rows, m_d.currency);
  m_panel->update();
  emit relayoutNeeded();
}

int TradeLogView::relayout(int W, int) {
  // very wide windows: the table is capped at 2200 px and centred, otherwise the 15 columns drift ~200 px apart
  const int cw0 = std::min(W - 20, 2200), x0 = (W - cw0) / 2;
  const qreal cw = (cw0 - 5 * 8.6) / 6.0;
  for (int i = 0; i < 6; ++i) m_kpi[i]->setGeometry(QRectF(x0 + i * (cw + 8.6), 10, cw, 57).toRect());
  const int pw = cw0;
  const int th = m_table->rowsHeight();
  const int ph = 43 + th + 8;
  m_panel->setGeometry(x0, 79, pw, ph);  // title row, then the table from y 43
  // narrow panels: the search box gets shorter before the pills would reach the title
  const int sw = pw < 1100 ? 200 : 248;
  m_search->setGeometry(pw - 12 - sw, 9, sw, 24);
  qreal pillW = 8;                                   // 245 px in English, wider for longer labels
  for (const auto& n : m_pillNames) pillW += QFontMetricsF(mono(10, true, 1.6)).horizontalAdvance(n) + 18;
  const int pw2 = std::max(245, int(std::ceil(pillW)));
  m_pills->setGeometry(pw - 12 - sw - 8 - pw2, 9, pw2, 23);
  m_table->setGeometry(0, 43, pw, th);
  return 79 + ph + 10;
}

void TradeLogView::setData(const TrData& d) {
  m_d = d;
  for (auto* k : m_kpi) k->update();
  refilter();
}
}
