#include <QCoreApplication>
#include "pages/market/watch/WatchList.h"
#include "pages/market/watch/WatchStyle.h"
#include "pages/market/watch/WatchIcons.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "ui/Kit.h"
#include <QPainter>
#include <QPainterPath>
#include <QLineEdit>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QLinearGradient>

namespace watch {
using namespace watch::col;
// text on a filled accent (active pill / segment): Deepwater's measured near-black, the theme's own elsewhere
static QColor onAccentText() { return theme::isDefault() ? QColor(0x0b, 0x08, 0x00) : theme::onAccent(); }

static void text(QPainter& p, qreal x, qreal baseline, const QString& s) { p.drawText(QPointF(x, baseline), s); }
static qreal adv(const QFont& f, const QString& s) { return QFontMetricsF(f).horizontalAdvance(s); }

// ------------------------------------------------------------------ HeroHeader
HeroHeader::HeroHeader(QWidget* parent) : QWidget(parent) { setAttribute(Qt::WA_TransparentForMouseEvents); }
void HeroHeader::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  if (!m_compact) {
    const QFont eb = theme::mono(10, QFont::Normal, 3.0);
    p.setFont(eb); p.setPen(theme::map(QColor(0x5e, 0x59, 0x56))); text(p, 20, 31, QCoreApplication::translate("watch", "MARKET WATCH"));
  }
  QFont t = theme::mono(m_compact ? 32 : 51, QFont::ExtraBold); t.setLetterSpacing(QFont::AbsoluteSpacing, m_compact ? -0.5 : -0.8);
  const QString title = QCoreApplication::translate("watch", "MARKETS");
  const qreal base = m_compact ? 43 : 92, sub = m_compact ? 43 : 99;
  p.setFont(t); p.setPen(col::text()); text(p, 19, base, title);
  const qreal x0 = 20 + adv(t, title) + (m_compact ? 16 : 19);
  const QFont nb = theme::mono(m_compact ? 12 : 13, QFont::Bold, 2.0), sb = theme::mono(m_compact ? 12 : 13, QFont::Normal, 2.6);
  const QString n = QString::number(m_count);
  p.setFont(nb); p.setPen(theme::map(QColor(0xff, 0xf9, 0xf6))); text(p, x0, sub, n);
  p.setFont(sb); p.setPen(theme::map(QColor(0x8a, 0x85, 0x7c)));
  const QString live = QCoreApplication::translate("watch", "LIVE MARKETS · QUOTES REFRESH EVERY 20S");
  text(p, x0 + adv(nb, n) + 7, sub, QFontMetricsF(sb).elidedText(live, Qt::ElideRight, width() - 20 - (x0 + adv(nb, n) + 7)));
  // gold rule fading to the right, faint halo above and below
  const qreal y = m_compact ? 56.5 : 119.5, w = width() - 20;
  QLinearGradient g(20, 0, w, 0);
  g.setColorAt(0, theme::map(QColor(0xd6, 0xd0, 0xb7))); g.setColorAt(0.18, theme::map(QColor(0xac, 0xa4, 0x8f)));
  g.setColorAt(0.38, theme::map(QColor(0x72, 0x6d, 0x5e))); g.setColorAt(0.58, theme::map(QColor(0x2d, 0x2c, 0x24)));
  g.setColorAt(0.95, theme::map(QColor(0x15, 0x10, 0x06))); g.setColorAt(1, theme::map(QColor(0x15, 0x10, 0x06, 0)));
  QLinearGradient h(20, 0, w, 0);
  h.setColorAt(0, theme::map(QColor(0x43, 0x38, 0x1e, 160))); h.setColorAt(0.5, theme::map(QColor(0x2d, 0x26, 0x00, 60))); h.setColorAt(1, theme::map(QColor(0, 0, 0, 0)));
  p.setPen(QPen(QBrush(h), 3)); p.drawLine(QPointF(20, y), QPointF(w, y));
  p.setPen(QPen(QBrush(g), 1)); p.drawLine(QPointF(20, y), QPointF(w, y));
}

// ------------------------------------------------------------------ SearchBox
SearchBox::SearchBox(QWidget* parent) : QWidget(parent), m_edit(new QLineEdit(this)) {
  m_edit->setFont(theme::mono(17));
  m_edit->setFrame(false);
  m_edit->setStyleSheet(QString("QLineEdit{background:transparent;border:none;padding:0;margin:0;color:%1;"
                                "selection-background-color:%2;font-family:\"JetBrains Mono\";font-size:17px;}")
                            .arg(theme::map("#edeae3").name(), theme::map("#4a3d05").name()));
  QPalette pl = m_edit->palette(); pl.setColor(QPalette::PlaceholderText, theme::map(QColor(0x4a, 0x4a, 0x47))); m_edit->setPalette(pl);
  m_edit->setClearButtonEnabled(false);
  connect(m_edit, &QLineEdit::textChanged, this, &SearchBox::textChanged);
  setCursor(Qt::IBeamCursor);
}
void SearchBox::setCount(int n) { m_edit->setPlaceholderText(QCoreApplication::translate("watch", "Search %1 markets — EUR/USD, Gold, Nvidia, Bitcoin…").arg(n)); }
void SearchBox::resizeEvent(QResizeEvent*) { m_edit->setGeometry(41, 1, width() - 41 - 52, height() - 2); }
void SearchBox::mousePressEvent(QMouseEvent*) { m_edit->setFocus(); }
void SearchBox::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const QRectF r = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
  p.fillRect(rect(), theme::map(QColor(0x03, 0x03, 0x02)));
  p.setPen(QPen(m_edit->hasFocus() ? theme::map(QColor(0x4a, 0x40, 0x1a)) : theme::map(QColor(0x1a, 0x1b, 0x14)), 1)); p.drawRect(r);
  // magnifier
  const QPointF c(23, height() / 2.0 - 1);
  p.setPen(QPen(theme::map(QColor(0xd4, 0xb0, 0x20)), 1.6)); p.setBrush(Qt::NoBrush);
  p.drawEllipse(c, 5.6, 5.6);
  p.drawLine(c + QPointF(4.2, 4.2), c + QPointF(7.6, 7.6));
  // "/" key hint
  const QRectF k(width() - 38, height() / 2.0 - 12, 23, 24);
  p.setPen(QPen(theme::map(QColor(0x20, 0x1a, 0x15)), 1)); p.drawRoundedRect(k.adjusted(.5, .5, -.5, -.5), 2, 2);
  p.setFont(theme::mono(12)); p.setPen(theme::map(QColor(0x5a, 0x56, 0x50))); p.drawText(k, Qt::AlignCenter, "/");
}

// ------------------------------------------------------------------ CategoryPills
static const QFont& pillFont() { static QFont f = theme::mono(12, QFont::Bold, 1.6); return f; }
static const QFont& badgeFont() { static QFont f = theme::mono(10, QFont::Medium, 0.8); return f; }
CategoryPills::CategoryPills(QWidget* parent) : QWidget(parent) { setMouseTracking(true); setCursor(Qt::PointingHandCursor); }
void CategoryPills::setItems(const QStringList& l, const QList<int>& c) { m_labels = l; m_counts = c; updateGeometry(); update(); }
void CategoryPills::setCurrent(int i) { if (i != m_cur) { m_cur = i; update(); } }
QList<QRectF> CategoryPills::rects() const {
  QList<QRectF> out; qreal x = 0, y = 0;
  const qreal h = m_wrapW > 0 ? 34 : 28;
  for (int i = 0; i < m_labels.size(); ++i) {
    const qreal w = 11 + adv(pillFont(), m_labels[i]) + 7 + adv(badgeFont(), QString::number(m_counts.value(i))) + 8 + 9;
    if (m_wrapW > 0 && x > 0 && x + w > m_wrapW) { x = 0; y += h + 8; }   // phone: next row
    out << QRectF(x, y, w, h); x += w + 8;
  }
  return out;
}
QSize CategoryPills::sizeHint() const {
  const auto r = rects();
  if (m_wrapW > 0) return QSize(m_wrapW, r.isEmpty() ? 34 : int(r.last().bottom()) + 1);
  return QSize(r.isEmpty() ? 0 : int(r.last().right()) + 1, 28);
}
void CategoryPills::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const auto rs = rects();
  for (int i = 0; i < rs.size(); ++i) {
    const QRectF r = rs[i]; const bool on = i == m_cur, hov = i == m_hover;
    if (on) p.fillRect(r, gold());
    else {
      p.fillRect(r, theme::map(QColor(0x06, 0x03, 0x03)));
      p.setPen(QPen(hov ? theme::map(QColor(0x4a, 0x40, 0x1a)) : subtle(), 1)); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(.5, .5, -.5, -.5));
    }
    p.setFont(pillFont()); p.setPen(on ? onAccentText() : hov ? gold() : muted());
    const qreal lw = adv(pillFont(), m_labels[i]);
    p.drawText(QRectF(r.left() + 11, r.top(), lw + 4, r.height()), Qt::AlignLeft | Qt::AlignVCenter, m_labels[i]);
    const QString n = QString::number(m_counts.value(i));
    const QRectF b(r.left() + 11 + lw + 7, r.center().y() - 8, adv(badgeFont(), n) + 8, 16);
    p.setPen(Qt::NoPen); p.setBrush(on ? goldDark() : theme::map(QColor(0x1d, 0x1a, 0x14))); p.drawRoundedRect(b, 1.5, 1.5);
    p.setFont(badgeFont()); p.setPen(on ? onAccentText() : theme::map(QColor(0x8a, 0x87, 0x80)));
    p.drawText(b, Qt::AlignCenter, n);
  }
}
void CategoryPills::mousePressEvent(QMouseEvent* e) {
  const auto rs = rects();
  for (int i = 0; i < rs.size(); ++i) if (rs[i].contains(e->position())) { if (i != m_cur) { m_cur = i; update(); emit changed(i); } return; }
}
void CategoryPills::mouseMoveEvent(QMouseEvent* e) {
  const auto rs = rects(); int h = -1;
  for (int i = 0; i < rs.size(); ++i) if (rs[i].contains(e->position())) h = i;
  if (h != m_hover) { m_hover = h; update(); }
}
void CategoryPills::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------ Segmented
static const QFont& segLabelFont() { static QFont f = theme::mono(10, QFont::Normal, 2.3); return f; }
static const QFont& segFont() { static QFont f = theme::mono(11, QFont::Bold, 1.4); return f; }
Segmented::Segmented(const QString& label, const QStringList& items, QWidget* parent)
    : QWidget(parent), m_label(label), m_items(items) { setMouseTracking(true); }
void Segmented::setCurrent(int i) { if (i != m_cur) { m_cur = i; update(); } }
qreal Segmented::labelWidth() const { return adv(segLabelFont(), m_label) + 10; }
QList<QRectF> Segmented::rects() const {
  QList<QRectF> out; qreal x = labelWidth() + 4;
  for (const auto& s : m_items) {
    const qreal w = adv(segFont(), s) + (m_tall ? 26 : 18);
    out << QRectF(x, 3, w, m_tall ? 30 : 18); x += w + 2;
  }
  return out;
}
QSize Segmented::sizeHint() const { const auto r = rects(); return QSize(int(r.last().right()) + 5, m_tall ? 36 : 24); }
void Segmented::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.setFont(segLabelFont()); p.setPen(label());
  p.drawText(QRectF(0, 0, labelWidth(), height()), Qt::AlignLeft | Qt::AlignVCenter, m_label);
  const auto rs = rects();
  const QRectF grp(labelWidth(), 0, rs.last().right() + 4 - labelWidth(), height());
  p.setPen(QPen(theme::map(QColor(0x20, 0x22, 0x12)), 1)); p.setBrush(Qt::NoBrush); p.drawRect(grp.adjusted(.5, .5, -.5, -.5));
  for (int i = 0; i < rs.size(); ++i) {
    const bool on = i == m_cur;
    if (on) { p.setPen(Qt::NoPen); p.setBrush(gold()); p.drawRoundedRect(rs[i], 1.5, 1.5); }
    p.setFont(segFont()); p.setPen(on ? onAccentText() : i == m_hover ? gold() : muted());
    p.drawText(rs[i], Qt::AlignCenter, m_items[i]);
  }
}
void Segmented::mousePressEvent(QMouseEvent* e) {
  const auto rs = rects();
  for (int i = 0; i < rs.size(); ++i) if (rs[i].adjusted(-1, -3, 1, 3).contains(e->position())) { if (i != m_cur) { m_cur = i; update(); emit changed(i); } return; }
}
void Segmented::mouseMoveEvent(QMouseEvent* e) {
  const auto rs = rects(); int h = -1;
  for (int i = 0; i < rs.size(); ++i) if (rs[i].adjusted(-1, -3, 1, 3).contains(e->position())) h = i;
  if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
}
void Segmented::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------ StatCard
StatCard::StatCard(Kind k, QWidget* parent) : QWidget(parent), m_kind(k) {}
void StatCard::setPeriod(const QString& p) { m_period = p; update(); }
void StatCard::setBreadth(int up, int down, int flat) {
  m_up = up; m_down = down; m_flat = flat; m_has = up + down + flat > 0;
  setToolTip(QCoreApplication::translate("watch", "%1 up · %2 down · %3 unchanged").arg(up).arg(down).arg(flat));
  update();
}
void StatCard::setAverage(double pct, int n) { m_pct = pct; m_n = n; m_has = std::isfinite(pct); update(); }
void StatCard::setMarket(const QString& s, const QString& name, double pct) {
  m_sym = s; m_name = name; m_pct = pct; m_has = std::isfinite(pct);
  setToolTip(QString("%1 · %2 · %3").arg(s, name, fmt::pct(pct)));   // the symbol may be elided on small windows
  update();
}
void StatCard::clear() { m_has = false; update(); }
void StatCard::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), theme::map(QColor(0x0d, 0x08, 0x02)));
  p.setPen(QPen(theme::map(QColor(0x3c, 0x38, 0x1c)), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5));
  const QString names[] = {QCoreApplication::translate("watch", "BREADTH"), QCoreApplication::translate("watch", "AVERAGE MOVE"), QCoreApplication::translate("watch", "LEADER"), QCoreApplication::translate("watch", "LAGGARD")};
  // narrow cards (1024 px, French): the letter spacing tightens before the title is elided
  const QString head = QString("%1 · %2").arg(names[m_kind], m_period);
  QFont lf = theme::mono(10, QFont::Normal, 2.2);
  for (qreal sp : {1.0, 0.3}) if (adv(lf, head) > width() - 20) lf = theme::mono(10, QFont::Normal, sp);
  p.setFont(lf); p.setPen(theme::map(QColor(0x5a, 0x53, 0x48)));
  p.drawText(QPointF(11, 20), QFontMetricsF(lf).elidedText(head, Qt::ElideRight, width() - 20));
  const QColor dimc(0x45, 0x41, 0x3a);
  if (!m_has) {
    p.setFont(theme::mono(20, QFont::Medium)); p.setPen(dimc); p.drawText(QPointF(11, 49), fmt::dash());
    return;
  }
  switch (m_kind) {
    case Breadth: {
      const int tot = m_up + m_down + m_flat; const qreal w = width() - 22;
      const qreal wu = tot ? w * m_up / tot : 0, wf = tot ? w * m_flat / tot : 0;
      p.fillRect(QRectF(11, 31, wu, 3), theme::map(QColor(0x37, 0xd8, 0xee)));
      p.fillRect(QRectF(11 + wu, 31, wf, 3), theme::map(QColor(0x5a, 0x56, 0x4e)));
      p.fillRect(QRectF(11 + wu + wf, 31, w - wu - wf, 3), theme::map(QColor(0xe8, 0x6e, 0x60)));
      const QFont f = theme::mono(13);
      p.setFont(f);
      QPainterPath up; up.moveTo(19, 43); up.lineTo(25, 54); up.lineTo(13, 54); up.closeSubpath();
      p.fillPath(up, theme::map(QColor(0x2d, 0xd5, 0xee)));
      QString us = QCoreApplication::translate("watch", "%1 up").arg(m_up);
      QString s = QCoreApplication::translate("watch", "%1 down").arg(m_down);
      // narrow card (long French words): numbers only, the full text is in the card's tooltip
      if (35 + adv(f, us) + 24 + adv(f, s) + 20 > width() - 11) { us = QString::number(m_up); s = QString::number(m_down); }
      p.setPen(theme::map(QColor(0x2d, 0xd5, 0xee))); p.drawText(QPointF(35, 53), us);
      const qreal r = width() - 11;
      QPainterPath dn; dn.moveTo(r - 12, 43); dn.lineTo(r, 43); dn.lineTo(r - 6, 54); dn.closeSubpath();
      p.fillPath(dn, theme::map(QColor(0xeb, 0x70, 0x63)));
      p.setPen(theme::map(QColor(0xeb, 0x70, 0x63))); p.drawText(QPointF(r - 20 - adv(f, s), 53), s);
      break;
    }
    case Average: {
      p.setFont(theme::mono(24, QFont::Bold)); p.setPen(sign(m_pct)); p.drawText(QPointF(11, 49), fmt::pct(m_pct));
      p.setFont(theme::mono(10)); p.setPen(dimc); p.drawText(QPointF(11, 65), QCoreApplication::translate("watch", "across %1 markets shown").arg(m_n));
      break;
    }
    case Leader: case Laggard: {
      QFont f = theme::mono(22, QFont::Bold); p.setFont(f); p.setPen(theme::map(QColor(0xf0, 0xf0, 0xee)));
      QString s = m_sym;
      const qreal maxw = m_phone ? width() - 22 - 7 - adv(theme::mono(13, QFont::Medium), fmt::pct(m_pct)) : width() - 120;
      s = QFontMetricsF(f).elidedText(s, Qt::ElideRight, maxw);
      p.drawText(QPointF(11, 49), s);
      p.setFont(theme::mono(13, QFont::Medium)); p.setPen(sign(m_pct));
      p.drawText(QPointF(11 + adv(f, s) + 7, 49), fmt::pct(m_pct));
      p.setFont(theme::mono(10)); p.setPen(dimc);
      p.drawText(QPointF(11, 67), m_phone ? QFontMetricsF(theme::mono(10)).elidedText(m_name, Qt::ElideRight, width() - 20) : m_name);
      break;
    }
  }
}

// ------------------------------------------------------------------ MarketPanel
static constexpr int kHead = 57;   // header + column header, rows start here
MarketPanel::MarketPanel(const QString& title, bool tall, QWidget* parent) : QWidget(parent), m_title(title), m_tall(tall) {
  setMouseTracking(true);
}
void MarketPanel::setRows(const QVector<Row>& rows) {
  const bool sameSize = rows.size() == m_rows.size();
  m_rows = rows;
  if (!sameSize) updateGeometry();
  update();
}
void MarketPanel::setColumns(int n) { m_cols = std::max(1, n); updateGeometry(); update(); }
int MarketPanel::perColumn() const { return m_cols <= 1 ? int(m_rows.size()) : (int(m_rows.size()) + m_cols - 1) / m_cols; }
int MarketPanel::heightFor() const {
  const int pitch = m_tall ? 49 : 33;
  const int n = std::max(1, perColumn());
  return kHead + n * pitch + 6;
}
QRectF MarketPanel::rowRect(int i) const {
  const int pitch = m_tall ? 49 : 33, pc = std::max(1, perColumn());
  const int c = m_cols <= 1 ? 0 : i / pc, k = m_cols <= 1 ? i : i % pc;
  const qreal cw = (width() - 2.0) / m_cols;
  return QRectF(1 + c * cw, kHead + k * pitch, cw, pitch);
}
int MarketPanel::rowAt(const QPointF& p) const {
  for (int i = 0; i < m_rows.size(); ++i) if (rowRect(i).contains(p)) return i;
  return -1;
}
void MarketPanel::paintEvent(QPaintEvent* ev) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  p.fillRect(rect(), panel());
  p.setPen(QPen(panelBorder(), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5));
  ui::drawBrackets(p, QRectF(rect()).adjusted(3, 3, -3, -3), bracket(), 8);
  // header: • TITLE ⁿ          avg ±x.xx%
  p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0xe6, 0xc8, 0x3a))); p.drawEllipse(QPointF(14, 17), 2.2, 2.2);
  const QFont tf = theme::mono(13, QFont::Bold, 2.2);
  double s = 0; int n = 0;
  for (const auto& r : m_rows) if (std::isfinite(r.chg)) { s += r.chg; ++n; }
  const QFont af = theme::mono(11);
  const QString avgWord = QCoreApplication::translate("watch", "avg") + " ", v = n ? fmt::pct(s / n) : QString();
  // title elided before the average (longer French titles on narrow columns); the word "avg" goes first
  qreal avgLeft = width() - 9 - adv(af, v) - adv(af, avgWord);
  const QString count = QString::number(m_total);
  const qreal cntW = adv(theme::mono(9), count) + 8;
  bool showWord = true;
  if (n && 23 + adv(tf, m_title) + cntW > avgLeft) { showWord = false; avgLeft = width() - 9 - adv(af, v) - 6; }
  const QString title = QFontMetricsF(tf).elidedText(m_title, Qt::ElideRight, (n ? avgLeft : width() - 12) - 23 - cntW);
  p.setFont(tf); p.setPen(goldText()); p.drawText(QPointF(23, 22), title);
  p.setFont(theme::mono(9)); p.setPen(theme::map(QColor(0x48, 0x45, 0x3c)));
  p.drawText(QPointF(23 + adv(tf, title) + 4, 16), count);
  if (n) {
    p.setFont(af); p.setPen(sign(s / n));
    const qreal vx = width() - 9 - adv(af, v);
    p.drawText(QPointF(vx, 22), v);
    if (showWord) p.drawText(QPointF(vx - adv(af, avgWord), 22), avgWord);
  }
  // column headers (repeated per column)
  const qreal cw = (width() - 2.0) / m_cols;
  const QFont hf = theme::mono(9, QFont::Normal, 1.6);
  p.setFont(hf); p.setPen(theme::map(QColor(0x57, 0x53, 0x4c)));
  for (int c = 0; c < m_cols; ++c) {
    const qreal x0 = 1 + c * cw;
    p.drawText(QPointF(x0 + 10, 48), QCoreApplication::translate("watch", "MARKET"));
    const qreal lastR = x0 + cw * 0.72, chgR = x0 + cw - 9;
    const QString last = QCoreApplication::translate("watch", "LAST");
    p.drawText(QPointF(lastR - adv(hf, last) + 1.6, 48), last);
    p.drawText(QPointF(chgR - adv(hf, m_period) + 1.6, 48), m_period);
  }
  p.fillRect(QRectF(1, 54, width() - 2, 1), theme::map(QColor(0x1a, 0x15, 0x0a)));
  if (m_rows.isEmpty()) {
    p.setFont(theme::mono(11)); p.setPen(theme::map(QColor(0x45, 0x41, 0x3a)));
    p.drawText(QRectF(0, kHead, width(), 33), Qt::AlignCenter, QCoreApplication::translate("watch", "No match"));
    return;
  }
  const QFont sf = theme::mono(13, QFont::Bold), lf = theme::mono(13, QFont::Medium), cf = theme::mono(12), nf = theme::sans(9);
  const qreal dpr = devicePixelRatioF();
  for (int i = 0; i < m_rows.size(); ++i) {
    const QRectF rr = rowRect(i);
    if (!rr.intersects(ev->rect())) continue;
    const auto& r = m_rows[i];
    const int k = m_cols <= 1 ? i : i % std::max(1, perColumn());
    if (i == m_hover) p.fillRect(rr.adjusted(0, 0, 0, 0), hover());
    else if (k % 2 == 1) p.fillRect(rr, theme::map(QColor(0x12, 0x0d, 0x04)));
    const qreal cy = rr.center().y();
    const QPixmap ic = instrumentIcon(r.symbol, m_tall ? 16 : 12, dpr);
    const qreal iw = ic.width() / ic.devicePixelRatio(), ih = ic.height() / ic.devicePixelRatio();
    p.drawPixmap(QPointF(rr.left() + 9, cy - ih / 2 + (m_tall ? 0 : 0)), ic);
    const qreal tx = rr.left() + 9 + std::max<qreal>(iw, 17) + 4;
    const qreal lastR = rr.left() + rr.width() * 0.72, chgR = rr.right() - 8;
    // symbol (elided before LAST)
    const QString last = fmt::num(r.last, r.decimals);
    p.setFont(lf);
    const qreal lastW = adv(lf, last);
    p.setFont(sf); p.setPen(symbol());
    const qreal symMax = lastR - lastW - 8 - tx;
    const QString sym = QFontMetricsF(sf).elidedText(r.label, Qt::ElideRight, symMax);
    if (m_tall) {
      p.drawText(QPointF(tx, cy - 2), sym);
      p.setFont(nf); p.setPen(theme::map(theme::isDark() ? QColor(0x3b, 0x37, 0x2f) : QColor(0x5a, 0x56, 0x4e)));   // company name: one step brighter on light themes
      p.drawText(QPointF(tx, cy + 12), QFontMetricsF(nf).elidedText(r.sub, Qt::ElideRight, symMax));
    } else p.drawText(QPointF(tx, cy + 5), sym);
    p.setFont(lf); p.setPen(std::isfinite(r.last) ? theme::map(QColor(0xf4, 0xf1, 0xea)) : theme::map(QColor(0x45, 0x41, 0x3a)));
    p.drawText(QPointF(lastR - lastW, cy + 5), last);
    const QString ch = std::isfinite(r.chg) ? fmt::pct(r.chg) : fmt::dash();
    p.setFont(cf); p.setPen(std::isfinite(r.chg) ? (r.chg < 0 || ch.startsWith('-') ? down() : r.chg > 0 ? up() : muted()) : theme::map(QColor(0x45, 0x41, 0x3a)));
    p.drawText(QPointF(chgR - adv(cf, ch), cy + 5), ch);
  }
}
void MarketPanel::mouseMoveEvent(QMouseEvent* e) {
  const int h = rowAt(e->position());
  if (h != m_hover) {
    if (m_hover >= 0) update(rowRect(m_hover).toAlignedRect());
    m_hover = h;
    if (h >= 0) update(rowRect(h).toAlignedRect());
    setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }
}
void MarketPanel::mousePressEvent(QMouseEvent* e) {
  const int i = rowAt(e->position());
  if (i >= 0 && e->button() == Qt::LeftButton) emit open(m_rows[i].symbol);
}
void MarketPanel::leaveEvent(QEvent*) { if (m_hover >= 0) { update(rowRect(m_hover).toAlignedRect()); m_hover = -1; } }
bool MarketPanel::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const int i = rowAt(he->pos());
    if (i >= 0 && !m_rows[i].chgTip.isEmpty() && he->pos().x() > rowRect(i).left() + rowRect(i).width() * 0.74)
      QToolTip::showText(he->globalPos(), m_rows[i].chgTip, this);
    else if (i >= 0) {
      const auto* r = &m_rows[i];
      QToolTip::showText(he->globalPos(), r->sub.isEmpty() ? r->label : r->label + " · " + r->sub, this);
    } else QToolTip::hideText();
    return true;
  }
  return QWidget::event(e);
}
}
