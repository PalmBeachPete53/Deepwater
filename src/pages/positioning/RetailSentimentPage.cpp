#include "pages/positioning/RetailSentimentPage.h"
#include "pages/positioning/RetailModal.h"
#include "pages/positioning/PosKit.h"
#include "positioning/Retail.h"
#include "app/Pages.h"
#include "core/Settings.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QScrollBar>
#include <QToolTip>
#include <QHelpEvent>
#include <QMenu>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <cmath>
#include <algorithm>


using positioning::Retail;
using positioning::RetailSymbol;

namespace positioning {
namespace {
const QColor kLongTxt(0x4f, 0xd4, 0xe8), kShortTxt(0xe2, 0x6c, 0x6a);
const QStringList kMajorsOrder{"EURUSD", "GBPUSD", "USDJPY", "USDCHF", "USDCAD", "AUDUSD", "NZDUSD", "EURGBP", "EURJPY",
                               "EURCHF", "EURAUD", "EURCAD", "EURNZD", "GBPJPY", "GBPCHF", "GBPAUD", "AUDJPY", "AUDNZD",
                               "AUDCAD", "CADJPY", "GBPCAD", "GBPNZD", "AUDCHF", "NZDJPY", "NZDCAD", "NZDCHF", "CADCHF",
                               "CHFJPY", "XAUUSD", "XAGUSD", "BTCUSD", "ETHUSD"};
// group keys (Myfxbook symbol groups); headers are translated where drawn
const QStringList kGroups{QT_TRANSLATE_NOOP("positioning", "FX"), QT_TRANSLATE_NOOP("positioning", "Commodities"),
                          QT_TRANSLATE_NOOP("positioning", "Crypto"), QT_TRANSLATE_NOOP("positioning", "Indices"),
                          QT_TRANSLATE_NOOP("positioning", "Other")};
}

// ------------------------------------------------------------------------------------------------ connection badge
class RetailConnBadge : public QWidget {
public:
  explicit RetailConnBadge(QWidget* p) : QWidget(p) { setCursor(Qt::PointingHandCursor); setFixedHeight(20); }
  std::function<void()> onClick;
  void setText(const QString& t, bool ok) { m_t = t; m_ok = ok; setFixedWidth(int(QFontMetricsF(font_()).horizontalAdvance(t)) + 24); update(); }
protected:
  static QFont font_() { return theme::mono(9, QFont::Medium, 1.0); }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
    p.setPen(QPen(m_hover ? theme::map(0x6a, 0x5c, 0x22) : theme::map(0x34, 0x2e, 0x18), 1)); p.setBrush(theme::map(0x14, 0x0f, 0x05));
    p.drawRoundedRect(r, 2, 2);
    p.setPen(Qt::NoPen); p.setBrush(m_ok ? theme::map(0x4f, 0xd4, 0xe8) : theme::map(0x8a, 0x84, 0x6a)); p.drawEllipse(QPointF(9, height() / 2.0), 2.4, 2.4);
    p.setFont(font_()); p.setPen(m_hover ? theme::map(0xf2, 0xe2, 0xa0) : theme::map(0xb8, 0xae, 0x8a));
    p.drawText(QRectF(16, 0, width() - 18, height()), Qt::AlignVCenter | Qt::AlignLeft, m_t);
  }
  void enterEvent(QEnterEvent*) override { m_hover = true; update(); }
  void leaveEvent(QEvent*) override { m_hover = false; update(); }
  void mousePressEvent(QMouseEvent*) override { if (onClick) onClick(); }
private:
  QString m_t; bool m_ok = false, m_hover = false;
};

// ------------------------------------------------------------------------------------------------ KPI cards
struct KpiData {
  int n = 0, crowdedLong = 0, crowdedShort = 0;
  QString mostLong, mostShort; double mostLongPct = NAN, mostShortPct = NAN;
  double tracked = NAN, lean = NAN;
};
class RetailKpis : public QWidget {
public:
  explicit RetailKpis(QWidget* p) : QWidget(p) { setFixedHeight(82); setMouseTracking(true); }
  static constexpr int kCard = 82;
  // six cards in a row; below 1200 px the labels would be cut, so 3 × 2
  int columns() const { return width() >= 1180 ? 6 : 3; }
  int neededHeight() const { return columns() == 6 ? kCard : 2 * kCard + 7; }
  void setData(const KpiData& d) { m_d = d; update(); }
protected:
  bool event(QEvent* e) override {
    if (e->type() == QEvent::ToolTip) {
      const QPoint hp = static_cast<QHelpEvent*>(e)->pos();
      const int i = int(hp.x() / (width() / qreal(columns()))) + (hp.y() > kCard ? columns() : 0);
      static const char* tips[6] = {QT_TRANSLATE_NOOP("positioning", "Instruments whose retail long share is 65 % or more (Myfxbook community outlook)"),
        QT_TRANSLATE_NOOP("positioning", "Instruments whose retail short share is 65 % or more"), QT_TRANSLATE_NOOP("positioning", "Highest long share"), QT_TRANSLATE_NOOP("positioning", "Highest short share"),
        QT_TRANSLATE_NOOP("positioning", "Open positions tracked by the Myfxbook community outlook, all listed instruments"),
        QT_TRANSLATE_NOOP("positioning", "Crowd lean: long positions / all positions, over every listed instrument (computed by Deepwater)")};
      QToolTip::showText(static_cast<QHelpEvent*>(e)->globalPos(), QCoreApplication::translate("positioning", tips[qBound(0, i, 5)]), this);
      return true;
    }
    return QWidget::event(e);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const int cols = columns();
    const qreal gap = 7.5, w = (width() - (cols - 1) * gap) / cols;
    const QFont lab = theme::mono(10, QFont::Medium, 2.2), val = theme::mono(24, QFont::Bold), sub = theme::mono(10, QFont::Medium, 1.6);
    const QColor yel = theme::map(0xf9, 0xe2, 0x91);
    auto card = [&](int i, const QColor& accent, const QString& l, const QString& v, const QColor& vc, const QString& s, const QColor& sc) {
      const QRectF r((i % cols) * (w + gap), (i / cols) * (kCard + 7), w, kCard);
      p.fillRect(r, pk::col::kPanel());
      p.setPen(QPen(theme::map(0x3a, 0x37, 0x1f), 1)); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(.5, .5, -.5, -.5));
      if (accent.isValid()) {
        p.fillRect(QRectF(r.left(), r.top(), r.width(), 1.5), accent);
        p.fillRect(QRectF(r.left(), r.top() + 1.5, r.width(), 1), theme::alpha(theme::mix(accent, theme::map(QColor(Qt::white)), 0.4), 110));
      }
      p.setPen(QPen(theme::map(0x8c, 0x88, 0x5b), 1.2));
      p.drawLine(QPointF(r.left() + 3, r.top() + 4), QPointF(r.left() + 12, r.top() + 4)); p.drawLine(QPointF(r.left() + 3, r.top() + 4), QPointF(r.left() + 3, r.top() + 13));
      p.setPen(QPen(theme::map(0x5a, 0x55, 0x30), 1.2));
      p.drawLine(QPointF(r.right() - 3, r.bottom() - 4), QPointF(r.right() - 12, r.bottom() - 4)); p.drawLine(QPointF(r.right() - 3, r.bottom() - 4), QPointF(r.right() - 3, r.bottom() - 13));
      p.setFont(lab); p.setPen(theme::map(0x5f, 0x5d, 0x57)); p.drawText(QPointF(r.left() + 12, r.top() + 19), l);
      if (!v.isNull()) {
        p.setFont(val);
        for (int k = 0; k < (theme::isDark() ? 8 : 0); ++k) { p.setPen(theme::alpha(vc, 14)); p.drawText(QPointF(r.left() + 12 + 2 * std::cos(k * 0.785), r.top() + 49 + 2 * std::sin(k * 0.785)), v); }
        p.setPen(vc); p.drawText(QPointF(r.left() + 12, r.top() + 49), v);
      }
      if (!s.isNull()) { p.setFont(sub); p.setPen(sc); p.drawText(QPointF(r.left() + 12, r.top() + 67), s); }
      return r;
    };
    const QColor cy = theme::map(0x08, 0x9a, 0xa4), rd = theme::map(0xb2, 0x42, 0x33), ye = theme::map(0xba, 0x99, 0x00);
    const QString dash = fmt::dash(), of = m_d.n ? QCoreApplication::translate("positioning", "OF %1 INSTRUMENTS").arg(m_d.n) : QCoreApplication::translate("positioning", "NO DATA");
    const QColor sc = theme::map(0x56, 0x53, 0x4c);
    card(0, cy, QCoreApplication::translate("positioning", "CROWDED LONG"), m_d.n ? QString::number(m_d.crowdedLong) : dash, theme::map(kLongTxt), of, sc);
    card(1, rd, QCoreApplication::translate("positioning", "CROWDED SHORT"), m_d.n ? QString::number(m_d.crowdedShort) : dash, theme::map(kShortTxt), of, sc);
    card(2, cy, QCoreApplication::translate("positioning", "MOST CROWDED LONG"), m_d.n ? m_d.mostLong : dash, theme::map(kLongTxt), m_d.n ? QCoreApplication::translate("positioning", "%1% LONG").arg(fmt::whole(m_d.mostLongPct)) : QString(""), sc);
    card(3, rd, QCoreApplication::translate("positioning", "MOST CROWDED SHORT"), m_d.n ? m_d.mostShort : dash, theme::map(kShortTxt), m_d.n ? QCoreApplication::translate("positioning", "%1% SHORT").arg(fmt::whole(m_d.mostShortPct)) : QString(""), sc);
    card(4, ye, QCoreApplication::translate("positioning", "TRACKED POSITIONS"), std::isfinite(m_d.tracked) ? fmt::num(m_d.tracked, 0) : dash, yel, QCoreApplication::translate("positioning", "MYFXBOOK COMMUNITY"), sc);
    const QRectF r = card(5, QColor(), QCoreApplication::translate("positioning", "CROWD LEAN"), QString(), theme::map(QColor(Qt::white)), QString(), sc);
    const QRectF bar(r.left() + 10, r.top() + 45, r.width() - 22, 6);
    if (std::isfinite(m_d.lean)) {
      const qreal lw = bar.width() * m_d.lean / 100;
      QPainterPath clip; clip.addRoundedRect(bar, 3, 3);
      p.save(); p.setClipPath(clip);
      p.fillRect(QRectF(bar.left(), bar.top(), lw, bar.height()), theme::map(0x1f, 0xdb, 0xf2));
      p.fillRect(QRectF(bar.left() + lw, bar.top(), bar.width() - lw, bar.height()), theme::map(0xea, 0x5d, 0x60));
      p.restore();
      p.setFont(sub); p.setPen(theme::map(kLongTxt)); p.drawText(QPointF(r.left() + 10, r.top() + 67), QString("%1% LONG").arg(fmt::whole(m_d.lean)));
      const QString s = QString("%1% SHORT").arg(fmt::whole(100 - m_d.lean));
      p.setPen(theme::map(kShortTxt)); p.drawText(QPointF(r.right() - 12 - QFontMetricsF(sub).horizontalAdvance(s), r.top() + 67), s);
    } else {
      p.setPen(Qt::NoPen); p.setBrush(theme::map(0x1e, 0x1b, 0x12)); p.drawRoundedRect(bar, 3, 3);
    }
  }
private:
  KpiData m_d;
};

// ------------------------------------------------------------------------------------------------ Custom button
class RetailCustomButton : public QWidget {
public:
  explicit RetailCustomButton(QWidget* p) : QWidget(p) { setCursor(Qt::PointingHandCursor); setFixedSize(73, 26); }
  std::function<void()> onClick;
  void setActive(bool a) { m_on = a; update(); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(m_on ? theme::map(0x43, 0x38, 0x1e) : theme::map(0x27, 0x22, 0x15), 1)); p.setBrush(m_on ? theme::map(0x30, 0x24, 0x04) : Qt::transparent);
    p.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 2, 2);
    const QColor c = m_on ? theme::map(0xd5, 0xc7, 0x88) : m_hover ? theme::map(0xc8, 0xc3, 0xb8) : theme::map(0x7d, 0x7a, 0x74);
    p.setPen(QPen(c, 1));
    for (int k = 0; k < 3; ++k) { const qreal y = 9.5 + k * 3.5; p.drawLine(QPointF(9, y), QPointF(17, y)); }
    p.setFont(theme::mono(12)); p.drawText(QRectF(22, 0, width() - 22, height()), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("positioning", "Custom"));
  }
  void enterEvent(QEnterEvent*) override { m_hover = true; update(); }
  void leaveEvent(QEvent*) override { m_hover = false; update(); }
  void mousePressEvent(QMouseEvent*) override { if (onClick) onClick(); }
private:
  bool m_on = false, m_hover = false;
};

// ------------------------------------------------------------------------------------------------ columns
struct Cols {
  qreal bar, barW = 263, lng, sht, delta, pos, status;
  explicit Cols(qreal W) {
    status = W - 11;
    if (W < 1200) {   // small windows: fixed-width numeric columns after a narrower bar, so nothing overlaps the bar
      bar = 110; barW = std::clamp(W * 0.2, 150.0, 263.0);
      lng = bar + barW + 75; sht = lng + 80; delta = sht + 120; pos = delta + 120;
      return;
    }
    // reference positions scaled with the width; the bar grows on wide screens (×2 at most)
    const qreal f = W / 1600.0; bar = 214 * f; barW = 263 * std::clamp(f, 1.0, 2.0);
    lng = 615 * f; sht = 773 * f; delta = 1035 * f; pos = 1278 * f;
  }
};

class RetailHeaderRow : public QWidget {
public:
  explicit RetailHeaderRow(QWidget* p) : QWidget(p) { setFixedHeight(26); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const Cols c(width());
    const QFont f = theme::mono(11, QFont::Bold, 1.2);
    p.setFont(f); p.setPen(theme::map(0xea, 0xdb, 0x9f));
    const QRectF r(0, 0, width(), 16);
    p.drawText(r.adjusted(10, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("positioning", "SYMBOL"));
    p.drawText(r.adjusted(c.bar, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("positioning", "POSITIONING"));
    auto right = [&](qreal x, const QString& t) { p.drawText(QRectF(x - 200, 0, 200, 16), Qt::AlignVCenter | Qt::AlignRight, t); };
    right(c.lng, QCoreApplication::translate("positioning", "LONG")); right(c.sht, QCoreApplication::translate("positioning", "SHORT")); right(c.delta, QCoreApplication::translate("positioning", "24H Δ LONG")); right(c.pos, QCoreApplication::translate("positioning", "POSITIONS")); right(c.status, QCoreApplication::translate("positioning", "STATUS"));
    p.fillRect(QRectF(0, 23, width(), 1), theme::map(0x36, 0x37, 0x22));
  }
};

// ------------------------------------------------------------------------------------------------ table
struct Item {
  bool group = false; QString groupName; int count = 0;
  RetailSymbol s; double delta = NAN; QString deltaTip;
};
class RetailTable : public QWidget {
public:
  explicit RetailTable(QWidget* p) : QWidget(p) { setMouseTracking(true); }
  std::function<void(const QString&)> onOpen;
  static constexpr qreal kRow = 35.7, kGroup = 21;
  void setItems(const QList<Item>& it) {
    m_items = it; m_hover = -1;
    qreal h = 0; for (const auto& x : it) h += x.group ? kGroup + 3 : kRow;
    setMinimumHeight(int(std::ceil(h)) + 4);
    update();
  }
  void setEmptyText(const QString& t) { m_empty = t; update(); }
  void setHoverAt(const QPoint& pt) { m_hover = indexAt(pt.y()); update(); }
protected:
  int indexAt(qreal y) const {
    qreal t = 0;
    for (int i = 0; i < m_items.size(); ++i) { const qreal h = m_items[i].group ? kGroup + 3 : kRow; if (y >= t && y < t + h) return m_items[i].group ? -1 : i; t += h; }
    return -1;
  }
  bool event(QEvent* e) override {
    if (e->type() == QEvent::ToolTip) {
      auto* he = static_cast<QHelpEvent*>(e);
      const int i = indexAt(he->pos().y());
      const Cols c(width());
      if (i >= 0 && he->pos().x() > c.delta - 120 && he->pos().x() < c.delta + 10 && !m_items[i].deltaTip.isEmpty())
        QToolTip::showText(he->globalPos(), m_items[i].deltaTip, this);
      else QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    const int i = indexAt(e->position().y());
    if (i != m_hover) { m_hover = i; setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
  }
  void leaveEvent(QEvent*) override { m_hover = -1; update(); }
  void mousePressEvent(QMouseEvent* e) override {
    const int i = indexAt(e->position().y());
    if (i >= 0 && onOpen) onOpen(m_items[i].s.name);
  }
  void paintEvent(QPaintEvent* ev) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (m_items.isEmpty()) {
      p.setFont(theme::mono(11)); p.setPen(theme::map(0x7e, 0x7a, 0x70));
      p.drawText(rect().adjusted(20, 0, -20, -200), Qt::AlignCenter | Qt::TextWordWrap, m_empty);
      return;
    }
    const Cols c(width());
    qreal y = 0;
    const QFont sym = theme::mono(13, QFont::Bold, 0.4), num = theme::mono(13, QFont::Bold, 0.4), pos = theme::mono(13, QFont::Normal, 0.3), small = theme::mono(12, QFont::Medium);
    for (int i = 0; i < m_items.size(); ++i) {
      const auto& it = m_items[i];
      if (it.group) {
        const QRectF r(0, y + 3, width(), kGroup);
        p.fillRect(r, theme::map(0x14, 0x0f, 0x03));
        p.setFont(theme::mono(11, QFont::Bold)); p.setPen(theme::map(0xf2, 0xdc, 0x99));
        const QString g = QCoreApplication::translate("positioning", it.groupName.toUtf8().constData()).toUpper();
        p.drawText(r.adjusted(10, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, g);
        p.setFont(theme::mono(10)); p.setPen(theme::map(0x47, 0x44, 0x39));
        p.drawText(r.adjusted(10 + QFontMetricsF(theme::mono(11, QFont::Bold)).horizontalAdvance(g) + 8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, "·  " + QString::number(it.count));
        y += kGroup + 3;
        continue;
      }
      const QRectF r(0, y, width(), kRow);
      y += kRow;
      if (!r.intersects(ev->rect())) continue;
      if (i == m_hover) p.fillRect(r, pk::col::kRowHover());
      p.fillRect(QRectF(0, r.bottom() - 1, width(), 1), theme::map(0x1e, 0x1a, 0x0d));
      const qreal cy = r.center().y();
      const auto& s = it.s;
      p.setFont(sym); p.setPen(theme::map(0xfd, 0xfe, 0xfa));
      p.drawText(QRectF(10, r.top(), 200, kRow), Qt::AlignVCenter | Qt::AlignLeft, s.display());
      // long/short bar
      const QRectF bar(c.bar, cy - 4.5, c.barW, 9);
      const double L = qBound(0.0, s.longPct, 100.0);
      QPainterPath clip; clip.addRoundedRect(bar, 4.5, 4.5);
      p.save(); p.setClipPath(clip);
      const qreal lw = bar.width() * L / 100.0;
      QLinearGradient gl(bar.left(), 0, bar.left() + lw, 0); gl.setColorAt(0, theme::map(0x1f, 0x9c, 0xaa)); gl.setColorAt(1, theme::map(0x1e, 0xd0, 0xe1));
      p.fillRect(QRectF(bar.left(), bar.top(), lw, bar.height()), gl);
      QLinearGradient gs(bar.left() + lw, 0, bar.right(), 0); gs.setColorAt(0, theme::map(0xe2, 0x62, 0x56)); gs.setColorAt(1, theme::map(0xad, 0x4a, 0x41));
      p.fillRect(QRectF(bar.left() + lw, bar.top(), bar.width() - lw, bar.height()), gs);
      p.restore();
      auto right = [&](qreal x, const QString& t, const QFont& f, const QColor& col) { p.setFont(f); p.setPen(col); p.drawText(QRectF(x - 200, r.top(), 200, kRow), Qt::AlignVCenter | Qt::AlignRight, t); };
      auto pct = [](double v) { return std::isfinite(v) ? QString("%1%").arg(fmt::whole(v)) : fmt::dash(); };   // lround(NaN): undefined
      right(c.lng, pct(s.longPct), num, theme::map(kLongTxt));
      right(c.sht, pct(s.shortPct), num, theme::map(kShortTxt));
      if (!std::isfinite(it.delta)) right(c.delta, fmt::dash(), small, theme::map(0x4a, 0x47, 0x40));
      else if (it.delta == 0) right(c.delta, QCoreApplication::translate("positioning", "flat"), small, theme::map(0x79, 0x78, 0x71));
      else {
        const bool up = it.delta > 0;
        const QString t = QCoreApplication::translate("positioning", "%1pt").arg(std::abs(int(it.delta)));
        const QColor col = up ? theme::map(kLongTxt) : theme::map(kShortTxt);
        right(c.delta, t, small, col);
        const qreal tw = QFontMetricsF(small).horizontalAdvance(t);
        const qreal ax = c.delta - tw - 9;
        p.setPen(QPen(col, 1.1));
        if (up) { p.drawLine(QPointF(ax, cy + 3), QPointF(ax + 5, cy - 2)); p.drawLine(QPointF(ax + 5, cy - 2), QPointF(ax + 2, cy - 2)); p.drawLine(QPointF(ax + 5, cy - 2), QPointF(ax + 5, cy + 1)); }
        else { p.drawLine(QPointF(ax, cy - 3), QPointF(ax + 5, cy + 2)); p.drawLine(QPointF(ax + 5, cy + 2), QPointF(ax + 2, cy + 2)); p.drawLine(QPointF(ax + 5, cy + 2), QPointF(ax + 5, cy - 1)); }
      }
      right(c.pos, fmt::num(s.totalPositions, 0), pos, theme::map(0x7e, 0x7e, 0x76));
      pk::drawStatusBadge(p, c.status, cy, pk::statusOf(s.longPct), 16);
    }
  }
private:
  QList<Item> m_items; int m_hover = -1; QString m_empty;
};

// ------------------------------------------------------------------------------------------------ login
class RetailLogin : public QWidget {
public:
  explicit RetailLogin(QWidget* p) : QWidget(p) {
    setFixedSize(500, 300);
    auto* v = new QVBoxLayout(this); v->setContentsMargins(26, 24, 26, 20); v->setSpacing(10);
    auto* t = new QLabel(QCoreApplication::translate("positioning", "CONNECT MYFXBOOK"), this); t->setFont(theme::mono(11, QFont::Bold, 3.0)); t->setStyleSheet(pk::mapQss("color:#f2dc99;"));
    auto* d = new QLabel(QCoreApplication::translate("positioning", "Retail positioning comes from the Myfxbook community outlook, which is only available with your own "
                         "Myfxbook account."), this);
    d->setWordWrap(true); d->setFont(theme::sans(12)); d->setStyleSheet(pk::mapQss("color:#9e9990;"));
    const QString fs = pk::mapQss("QLineEdit{background:#140f05;border:1px solid #2b2a1d;border-radius:2px;padding:6px 9px;color:#f5f2eb;"
                       "font-family:'JetBrains Mono';font-size:12px;} QLineEdit:focus{border-color:#7a6a1a;}");
    email = new QLineEdit(this); email->setPlaceholderText(QCoreApplication::translate("positioning", "Myfxbook email")); email->setStyleSheet(fs);
    password = new QLineEdit(this); password->setPlaceholderText(QCoreApplication::translate("positioning", "Password")); password->setEchoMode(QLineEdit::Password); password->setStyleSheet(fs);
    auto* row = new QHBoxLayout; row->setSpacing(10);
    button = new QPushButton(QCoreApplication::translate("positioning", "CONNECT"), this); button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(pk::mapQss(QString("QPushButton{background:#ffd706;color:ON_ACCENT;") + "border:none;border-radius:2px;padding:7px 18px;"
                          "font-family:'JetBrains Mono';font-size:11px;font-weight:700;letter-spacing:1px;}"
                          "QPushButton:hover{background:#ffe24a;} QPushButton:disabled{background:#5a4c08;color:#2a2204;}").replace("ON_ACCENT", theme::isDefault() ? QString("#0c0601") : theme::onAccent().name()));
    cancel = new QPushButton(QCoreApplication::translate("positioning", "CLOSE"), this); cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(pk::mapQss("QPushButton{background:transparent;color:#7d7a74;border:1px solid #272215;border-radius:2px;padding:6px 14px;"
                          "font-family:'JetBrains Mono';font-size:11px;} QPushButton:hover{color:#d8d3c8;}"));
    status = new QLabel(this); status->setFont(theme::mono(11)); status->setWordWrap(true);
    row->addWidget(button); row->addWidget(cancel); row->addWidget(status, 1);
    m_row = row;
    m_fields = new QBoxLayout(QBoxLayout::TopToBottom); m_fields->setSpacing(10);
    m_fields->addWidget(email); m_fields->addWidget(password);
    auto* n = new QLabel(QCoreApplication::translate("positioning", "Your password is sent once to Myfxbook (HTTPS) to open a session and is never stored. Deepwater keeps "
                         "only the session token, cleared when you log out or when it expires. The outlook is requested at most "
                         "once every 30 minutes and every answer is recorded on this computer for the 24 h changes and history."), this);
    n->setWordWrap(true); n->setFont(theme::sans(10)); n->setStyleSheet(pk::mapQss("color:#5e5b53;"));
    v->addWidget(t); v->addWidget(d); v->addLayout(m_fields); v->addLayout(row); v->addWidget(n); v->addStretch(1);
    connect(password, &QLineEdit::returnPressed, button, &QPushButton::click);
  }
  QLineEdit *email, *password; QPushButton *button, *cancel; QLabel* status;
  // short windows (1024x600): the card goes wide and flat — email, password and the buttons on one line — so it fits
  // under the filter bar instead of covering the cards and the table header; the reference card is 500x300
  void setCompact(bool on, int width) {
    if (on != m_compact) {
      m_compact = on;
      if (on) { m_fields->setDirection(QBoxLayout::LeftToRight); m_fields->addWidget(button); m_fields->addWidget(cancel); }
      else { m_fields->setDirection(QBoxLayout::TopToBottom); m_row->insertWidget(0, button); m_row->insertWidget(1, cancel); }
    }
    status->setVisible(!status->text().isEmpty() || !on);
    if (!on) { setFixedSize(500, 300); return; }
    layout()->activate();
    setFixedSize(width, layout()->totalHeightForWidth(width));
  }
  bool compact() const { return m_compact; }
private:
  QBoxLayout *m_fields = nullptr, *m_row = nullptr;
  bool m_compact = false;
public:
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), pk::col::kPanel());
    p.setPen(QPen(theme::map(0x3a, 0x37, 0x1f), 1)); p.drawRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5));
    p.fillRect(QRectF(0, 0, width(), 2), theme::map(0xba, 0x99, 0x00));
  }
};

// ------------------------------------------------------------------------------------------------ page
RetailSentimentPage::RetailSentimentPage(QWidget* parent) : QWidget(parent) {
  setAutoFillBackground(true);
  QPalette pl = palette(); pl.setColor(QPalette::Window, pk::col::kPage()); setPalette(pl);
  m_title = new pk::TitleBar(pk::TitleBar::Users, QCoreApplication::translate("positioning", "RETAIL POSITIONING — CROWD SURVEILLANCE"), this, 34);
  m_title->setPulse(true);   // the status dot pulses (dim on t000.0, bright on t001.0)
  m_conn = new RetailConnBadge(m_title);
  m_title->rightLayout()->addWidget(m_conn);
  m_kpis = new RetailKpis(this);
  m_filter = new QLineEdit(this);
  m_filter->setPlaceholderText(QCoreApplication::translate("positioning", "Filter…"));
  m_filter->setStyleSheet(pk::mapQss("QLineEdit{background:#140f05;border:1px solid #2b2a1d;border-radius:2px;padding:0 6px 0 22px;color:#e8e3da;"
                          "font-family:'JetBrains Mono';font-size:12px;} QLineEdit:focus{border-color:#5c4c14;}"));
  {
    auto* icon = new QLabel(m_filter);
    QPixmap pm(QSize(12, 12) * 2); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(theme::map(0x6a, 0x66, 0x5c), 1.2));
    p.drawEllipse(QPointF(5, 5), 3.6, 3.6); p.drawLine(QPointF(7.7, 7.7), QPointF(10.5, 10.5)); p.end();
    icon->setPixmap(pm); icon->move(7, 7); icon->setStyleSheet("background:transparent;");
  }
  m_cat = new pk::Segmented({QCoreApplication::translate("positioning", "All"), QCoreApplication::translate("positioning", "FX"), QCoreApplication::translate("positioning", "Commodities"), QCoreApplication::translate("positioning", "Crypto")}, pk::Segmented::Yellow, theme::mono(12, QFont::Medium, 0.25), this);
  m_cat->setPadding(8, 20); m_cat->setPillInset(1.5);
  m_custom = new RetailCustomButton(this);
  m_sort = new pk::Segmented({QCoreApplication::translate("positioning", "Majors first"), QCoreApplication::translate("positioning", "Most extreme"), QCoreApplication::translate("positioning", "Most active"), QCoreApplication::translate("positioning", "A-Z")}, pk::Segmented::Yellow, theme::mono(12, QFont::Medium, 0.25), this);
  m_sort->setPadding(8, 20); m_sort->setPillInset(1.5);
  m_head = new RetailHeaderRow(this);
  m_scroll = new pk::Scroll(this);
  m_table = new RetailTable(m_scroll);
  m_scroll->setWidget(m_table);
  m_footer = new pk::Footer(this, 26);
  m_login = new RetailLogin(this);
  m_login->hide();
  m_ticker = new pk::Ticker(this, 66);
  m_title->registerAnimation(m_ticker);

  m_cat->setCurrent(settings::value("positioning.retail.category", 0).toInt());
  m_sort->setCurrent(settings::value("positioning.retail.sort", 0).toInt());
  m_custom->setActive(settings::value("positioning.retail.customOnly", false).toBool());
  connect(m_cat, &pk::Segmented::changed, this, [this](int i) { settings::setValue("positioning.retail.category", i); rebuildRows(); });
  connect(m_sort, &pk::Segmented::changed, this, [this](int i) { settings::setValue("positioning.retail.sort", i); rebuildRows(); });
  connect(m_filter, &QLineEdit::textChanged, this, [this] { rebuildRows(); });
  m_custom->onClick = [this] { editCustom(); };
  m_table->onOpen = [this](const QString& s) { openModal(s); };
  m_conn->onClick = [this] {
    auto& r = Retail::instance();
    if (r.state() == Retail::State::Connected && !r.isFixture()) r.logout();
    else showLogin(true);
  };
  connect(m_login->button, &QPushButton::clicked, this, [this] {
    const QString pw = m_login->password->text();
    m_login->password->clear();                 // the password lives only for this call
    Retail::instance().login(m_login->email->text(), pw);
  });
  connect(m_login->cancel, &QPushButton::clicked, this, [this] { m_forceLogin = false; showLogin(false); });
  connect(&Retail::instance(), &Retail::changed, this, [this] { refresh(); if (m_modal && m_modal->isVisible()) m_modal->refreshData(); });

  m_clock = new QTimer(this);   // "next update" time in the connection badge; runs only while the page is shown
  m_clock->setInterval(30000);
  connect(m_clock, &QTimer::timeout, this, [this] { refresh(); });

  m_forceLogin = qgetenv("DW_RETAIL_LOGIN") == "1";
  refresh();
  const QByteArray open = qgetenv("DW_RETAIL_OPEN"), hv = qgetenv("DW_RETAIL_HOVER"), sc = qgetenv("DW_RETAIL_SCROLL");
  if (!open.isEmpty() || !hv.isEmpty())
    QTimer::singleShot(open.isEmpty() ? 800 : 300, this, [this, open, hv, sc] {
      if (!open.isEmpty()) openModal(QString::fromUtf8(open));
      QTimer::singleShot(open.isEmpty() ? 0 : 3500, this, [this, hv, sc] {
        if (m_modal && m_modal->isVisible()) {
          if (!sc.isEmpty()) m_modal->setScroll(sc.toInt());
          const auto xy = QString::fromUtf8(hv).split(',');
          if (xy.size() == 2) m_modal->setHover(QPoint(xy[0].toInt(), xy[1].toInt()));
          return;
        }
        const auto xy = QString::fromUtf8(hv).split(',');
        if (xy.size() == 2) m_table->setHoverAt(m_table->mapFrom(this, QPoint(xy[0].toInt(), xy[1].toInt())));
      });
    });
}

void RetailSentimentPage::resizeEvent(QResizeEvent*) {
  const int W = width(), H = height();
  m_title->setGeometry(0, 0, W, 34);
  m_kpis->setGeometry(10, 45, W - 20, 82);
  const int kh = m_kpis->neededHeight(), dy = kh - 82;   // two rows of cards on small windows
  m_kpis->setFixedHeight(kh);
  m_kpis->setGeometry(10, 45, W - 20, kh);
  m_filter->setGeometry(10, 134 + dy, 122, 26);
  m_cat->setGeometry(138, 134 + dy, m_cat->sizeHint().width(), 26);
  m_custom->move(m_cat->geometry().right() + 5, 134 + dy);
  m_sort->setGeometry(W - 10 - m_sort->sizeHint().width(), 134 + dy, m_sort->sizeHint().width(), 26);
  m_head->setGeometry(0, 176 + dy, W, 26);
  m_scroll->setGeometry(0, 199 + dy, W, H - 199 - dy - 26);
  m_footer->setGeometry(0, H - 26, W, 26);
  // the login card stays inside the page between the title bar and the footer, even at 1024x600: when the 300 px card
  // does not fit under the filter bar, it turns into the flat one (fields side by side)
  m_login->setCompact(H - 199 - dy - 26 < 300 + 16, std::min(W - 40, 860));
  const int ly = std::clamp(199 + dy + std::min(90, (H - 199 - dy - 26 - m_login->height()) / 2), 40, std::max(40, H - 26 - 8 - m_login->height()));
  m_login->move((W - m_login->width()) / 2, ly);
  if (m_modal) m_modal->setGeometry(rect());
}

void RetailSentimentPage::showEvent(QShowEvent*) { m_ticker->setRunning(true); m_clock->start(); Retail::instance().setActive(this, true); }
void RetailSentimentPage::hideEvent(QHideEvent*) { m_ticker->setRunning(false); m_clock->stop(); Retail::instance().setActive(this, false); }

void RetailSentimentPage::showLogin(bool on) {
  if (on) {
    if (m_login->email->text().isEmpty()) m_login->email->setText(Retail::instance().email());
    // focus only when the card appears: refresh() runs every 30 s and must not pull the focus back while typing elsewhere
    const bool appearing = m_login->isHidden();
    m_login->show(); m_login->raise();
    if (appearing) (m_login->email->text().isEmpty() ? m_login->email : m_login->password)->setFocus();
  } else m_login->hide();
}

void RetailSentimentPage::refresh() {
  auto& r = Retail::instance();
  using S = Retail::State;
  const QString next = r.nextAllowed().toLocalTime().toString("HH:mm");
  switch (r.state()) {
    case S::Connected: m_conn->setText(r.isFixture() ? QString("MYFXBOOK · TEST FIXTURE") : QCoreApplication::translate("positioning", "MYFXBOOK · NEXT UPDATE %1 · LOG OUT").arg(next), true); break;
    case S::LoggingIn: m_conn->setText(QCoreApplication::translate("positioning", "MYFXBOOK · CONNECTING…"), false); break;
    case S::Expired: m_conn->setText(QCoreApplication::translate("positioning", "MYFXBOOK SESSION EXPIRED · CONNECT"), false); break;
    default: m_conn->setText(QCoreApplication::translate("positioning", "MYFXBOOK NOT CONNECTED · CONNECT"), false); break;
  }
  m_login->status->setStyleSheet(pk::mapQss(r.state() == S::Error || r.state() == S::Expired ? "color:#e26c6a;" : "color:#9e9990;"));
  m_login->status->setText(r.state() == S::LoggingIn ? QCoreApplication::translate("positioning", "Connecting…") : r.message());
  m_login->status->setVisible(!m_login->status->text().isEmpty() || !m_login->compact());   // flat card: no empty line
  m_login->button->setEnabled(r.state() != S::LoggingIn);
  const bool noData = r.latest().isEmpty();
  if (r.state() == S::Connected && !m_forceLogin) showLogin(false);
  else if (noData || m_forceLogin) showLogin(true);
  m_login->cancel->setVisible(!noData);
  // footer
  if (noData) { m_footer->setTime(fmt::dash()); m_footer->setBadge(QCoreApplication::translate("positioning", "NO DATA"), pk::Footer::Muted, QCoreApplication::translate("positioning", "Connect your Myfxbook account to load the community outlook")); }
  else {
    m_footer->setTime(r.latest().time.toLocalTime().toString("HH:mm:ss"));
    if (r.isFixture()) m_footer->setBadge("TEST FIXTURE", pk::Footer::Muted, "DW_MYFXBOOK_FIXTURE: recorded test answer, not market data");
    else m_footer->setBadge(QCoreApplication::translate("positioning", "DELAYED"), pk::Footer::Delayed, QCoreApplication::translate("positioning", "Myfxbook community outlook, requested at most every 30 minutes"));
  }
  const auto& hist = r.recordings();
  QString note;
  if (!noData) {
    const QDateTime first = hist.isEmpty() ? r.latest().time : hist.first().time;
    // the reference footer shows only the time: the note appears only while the app's own history is short
    if (first.secsTo(r.latest().time) < 20 * 3600)
      note = QCoreApplication::translate("positioning", "%n snapshot(s) recorded since %1 · 24 h changes appear after one day of recordings",
                                         nullptr, int(hist.size()))
                 .arg(i18n::locale().toString(first.toLocalTime(), "d MMM HH:mm"));
  }
  if (!r.message().isEmpty() && r.state() == S::Connected) note += (note.isEmpty() ? "" : " · ") + r.message();
  m_footer->setNote(note);
  rebuildRows();
}

void RetailSentimentPage::editCustom() {
  auto& r = Retail::instance();
  QStringList custom = settings::value("positioning.retail.custom").toStringList();
  QMenu menu(this);
  auto* only = menu.addAction(QCoreApplication::translate("positioning", "Show my custom list only"));
  only->setCheckable(true); only->setChecked(settings::value("positioning.retail.customOnly", false).toBool());
  menu.addSeparator();
  QList<QAction*> acts;
  for (const auto& s : r.latest().symbols) {
    auto* a = menu.addAction(s.display()); a->setCheckable(true); a->setChecked(custom.contains(s.name)); a->setData(s.name);
    acts << a;
  }
  if (acts.isEmpty()) menu.addAction(QCoreApplication::translate("positioning", "(no instruments yet)"))->setEnabled(false);
  QAction* chosen = menu.exec(m_custom->mapToGlobal(QPoint(0, m_custom->height() + 2)));
  if (!chosen) return;
  if (chosen == only) settings::setValue("positioning.retail.customOnly", only->isChecked());
  else {
    const QString n = chosen->data().toString();
    if (chosen->isChecked()) { if (!custom.contains(n)) custom << n; } else custom.removeAll(n);
    settings::setValue("positioning.retail.custom", custom);
  }
  m_custom->setActive(settings::value("positioning.retail.customOnly", false).toBool());
  rebuildRows();
}

void RetailSentimentPage::rebuildRows() {
  auto& r = Retail::instance();
  const auto& snap = r.latest();
  // KPIs over every instrument of the snapshot
  KpiData k;
  double longPos = 0, allPos = 0;
  for (const auto& s : snap.symbols) {
    ++k.n;
    const auto st = pk::statusOf(s.longPct);
    if (st == pk::Status::CrowdedLong) ++k.crowdedLong;
    if (st == pk::Status::CrowdedShort) ++k.crowdedShort;
    if (std::isfinite(s.longPct) && (!std::isfinite(k.mostLongPct) || s.longPct > k.mostLongPct)) { k.mostLongPct = s.longPct; k.mostLong = s.display(); }
    if (std::isfinite(s.shortPct) && (!std::isfinite(k.mostShortPct) || s.shortPct > k.mostShortPct)) { k.mostShortPct = s.shortPct; k.mostShort = s.display(); }
    if (std::isfinite(s.totalPositions)) { allPos += s.totalPositions; longPos += std::isfinite(s.longPositions) ? s.longPositions : 0; }
  }
  if (allPos > 0) { k.tracked = allPos; k.lean = longPos / allPos * 100; }
  m_kpis->setData(k);

  const int cat = m_cat->current();
  const QString filt = m_filter->text().trimmed().toUpper().remove('/');
  const bool customOnly = settings::value("positioning.retail.customOnly", false).toBool();
  const QStringList custom = settings::value("positioning.retail.custom").toStringList();
  QHash<QString, QList<Item>> groups;
  const QDateTime dayAgo = snap.time.addSecs(-24 * 3600);
  for (const auto& s : snap.symbols) {
    const QString g = s.group();
    if (cat == 1 && g != "FX") continue;
    if (cat == 2 && g != "Commodities") continue;
    if (cat == 3 && g != "Crypto") continue;
    if (!filt.isEmpty() && !s.name.contains(filt)) continue;
    if (customOnly && !custom.contains(s.name)) continue;
    Item it; it.s = s;
    QDateTime when;
    const auto* old = std::isfinite(s.longPct) ? r.symbolAt(s.name, dayAgo, 3 * 3600, &when) : nullptr;
    if (old && std::isfinite(old->longPct)) {   // lround(NaN) is undefined
      it.delta = fmt::roundInt(s.longPct) - fmt::roundInt(old->longPct);
      it.deltaTip = QCoreApplication::translate("positioning", "Long %1% now vs %2% on %3 (this app's recording)").arg(fmt::whole(s.longPct)).arg(fmt::whole(old->longPct)).arg(i18n::locale().toString(when.toLocalTime(), "d MMM HH:mm"));
    } else it.deltaTip = QCoreApplication::translate("positioning", "Needs a recording from about 24 h ago (± 3 h): the app records every Myfxbook answer while it runs.");
    groups[g] << it;
  }
  const int sort = m_sort->current();
  QList<Item> items;
  for (const auto& g : kGroups) {
    auto list = groups.value(g);
    if (list.isEmpty()) continue;
    // unknown values (NaN) sort last: a NaN compared with > breaks the strict weak ordering std::stable_sort needs
    auto desc = [](double x, double y) {
      const bool fx = std::isfinite(x), fy = std::isfinite(y);
      return fx != fy ? fx : (fx && x > y);
    };
    std::stable_sort(list.begin(), list.end(), [sort, desc](const Item& a, const Item& b) {
      if (sort == 1) return desc(std::abs(a.s.longPct - 50), std::abs(b.s.longPct - 50));
      if (sort == 2) return desc(a.s.totalPositions, b.s.totalPositions);
      if (sort == 3) return a.s.name < b.s.name;
      const int ia = kMajorsOrder.indexOf(a.s.name), ib = kMajorsOrder.indexOf(b.s.name);
      if (ia != ib) return (ia < 0 ? 1000 : ia) < (ib < 0 ? 1000 : ib);
      return a.s.name < b.s.name;
    });
    Item head; head.group = true; head.groupName = g; head.count = list.size();
    items << head << list;
  }
  m_table->setItems(items);
  m_table->setEmptyText(snap.isEmpty() ? QString() : QCoreApplication::translate("positioning", "No instrument matches the filter."));
}

void RetailSentimentPage::openModal(const QString& symbol) {
  if (Retail::instance().latest().find(symbol) == nullptr) return;
  if (!m_modal) {
    m_modal = new RetailModal(this);
    connect(m_modal, &RetailModal::closed, this, [this] { m_modal->hide(); });
  }
  m_modal->setGeometry(rect());
  m_modal->open(symbol);
}
}  // namespace positioning

DW_PAGE("positioning/retail-sentiment", positioning::RetailSentimentPage)
