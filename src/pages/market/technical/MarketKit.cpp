#include "pages/market/technical/MarketKit.h"
#include "ui/Theme.h"
#include "ui/Kit.h"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolTip>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>
#include <algorithm>

namespace mkit {
// Deepwater values in Sem's initialisers, mapped to the current theme; rebuilt when the theme revision changes.
const Sem& sem() {
  static Sem s; static int rev = -1;
  if (rev != theme::revision()) {
    const Sem d;
    QColor* out = &s.cyan; const QColor* in = &d.cyan;
    for (size_t i = 0; i < sizeof(Sem) / sizeof(QColor); ++i) out[i] = theme::map(in[i]);
    rev = theme::revision();
  }
  return s;
}

QString ago(const QDateTime& t, const QDateTime& now) {
  const qint64 s = std::max<qint64>(0, t.secsTo(now));
  if (s < 60) return QCoreApplication::translate("technical", "%1s ago").arg(s);
  if (s < 3600) return QCoreApplication::translate("technical", "%1m ago").arg(s / 60);
  if (s < 86400) return QCoreApplication::translate("technical", "%1h ago").arg(s / 3600);
  return QCoreApplication::translate("technical", "%1d ago").arg(s / 86400);
}

static double relLum(const QColor& c) {
  auto ch = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}
QColor readable(const QColor& text, const QColor& fill) {
  if (theme::isDefault()) return text;
  const double lt = relLum(text), lf = relLum(fill);
  if ((std::max(lt, lf) + 0.05) / (std::min(lt, lf) + 0.05) >= 3.5) return text;
  return lf > 0.22 ? QColor(16, 16, 16) : QColor(255, 255, 255);
}

qreal textWidth(const QFont& f, const QString& s) { return QFontMetricsF(f).horizontalAdvance(s); }

void paintBackdrop(QPainter& p, const QRect& r, int glowTop) {
  p.fillRect(r, sem().page);
  // faint 64 px grid (barely visible in the reference: one level above the page colour)
  p.setPen(theme::map(10, 8, 5));
  for (int x = r.left() + 63; x < r.right(); x += 64) p.drawLine(x, r.top(), x, r.bottom());
  for (int y = r.top() + 63; y < r.bottom(); y += 64) p.drawLine(r.left(), y, r.right(), y);
  // warm glow under the title strip
  QLinearGradient g(0, glowTop, 0, glowTop + 220);
  g.setColorAt(0, theme::map(30, 22, 4, 60)); g.setColorAt(1, theme::map(30, 22, 4, 0));
  p.fillRect(QRect(r.left(), glowTop, r.width(), 220), g);
}

void glowText(QPainter& p, const QPointF& at, const QString& s, const QColor& c, int strength) {
  p.save();
  QColor g = c; g.setAlpha(13);
  p.setPen(g);
  if (!theme::isDark()) strength = 0;   // halos look dirty on light backgrounds
  for (int r = strength; r >= 1; --r)
    for (int k = 0; k < 8; ++k) {
      const double a = k * M_PI / 4;
      p.drawText(at + QPointF(r * std::cos(a), r * std::sin(a)), s);
    }
  p.setPen(c); p.drawText(at, s);
  p.restore();
}

// ------------------------------------------------------------------ TitleStrip
TitleStrip::TitleStrip(const QString& title, Glyph g, int height, QWidget* parent)
    : QWidget(parent), m_title(title), m_glyph(g), m_t0(QDateTime::currentMSecsSinceEpoch()) {
  setFixedHeight(height);
  // opaque gradient over the whole strip: the page backdrop under it (grid + glow) needs no repaint on each dot frame
  setAttribute(Qt::WA_OpaquePaintEvent, theme::map("#181205").alpha() == 255 && theme::map("#0d0802").alpha() == 255);
  auto* h = new QHBoxLayout(this);
  m_row = h;
  h->setContentsMargins(0, 0, 14, 0); h->setSpacing(8);
  m_after = new QHBoxLayout; m_after->setSpacing(8);
  m_right = new QHBoxLayout; m_right->setSpacing(8);
  h->addSpacing(titleRight() + 18);
  h->addLayout(m_after);
  h->addStretch(1);
  h->addLayout(m_right);
  m_tick.setInterval(100);  // 10 fps for the dot glow; the cursor toggles every 10 ticks (1 s)
  connect(&m_tick, &QTimer::timeout, this, [this] {
    const int cy = this->height() / 2;
    update(QRect(31, cy - 7, 14, 14));
    if (++m_frame % 10 == 0) { m_cursorOn = !m_cursorOn; update(QRect(titleRight() - 12, 0, 14, this->height())); }
  });
}
int TitleStrip::titleRight() const {
  const QFont f = theme::mono(10, QFont::Normal, 2.5);
  if (m_room == 0) return 32;   // no title: what follows starts right after the dot (the -18 gap below brings it to 50)
  return int(50 + textWidth(f, shownTitle()) + 4 + 9);
}
QString TitleStrip::shownTitle() const {
  if (m_room < 0) return m_title;
  if (m_room == 0) return {};
  return QFontMetricsF(theme::mono(10, QFont::Normal, 2.5)).elidedText(m_title, Qt::ElideRight, m_room);
}
void TitleStrip::setTitleRoom(int px) {
  if (px == m_room) return;
  m_room = px;
  // the spacing before afterTitleLayout() follows the shown title
  if (auto* sp = m_row->itemAt(0) ? m_row->itemAt(0)->spacerItem() : nullptr) sp->changeSize(titleRight() + 18, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
  m_row->invalidate();
  m_static = QImage();
  update();
}
void TitleStrip::showEvent(QShowEvent*) { m_tick.start(); }
void TitleStrip::hideEvent(QHideEvent*) { m_tick.stop(); }
// The 10 fps tick repaints the dot (and the cursor each second): gradient, glyph and title are drawn once into
// m_static, the dot and the cursor over it (the dot's glow, x 31–45, touches neither the glyph nor the title from x 50).
void TitleStrip::paintEvent(QPaintEvent*) {
  const int H = height(), cy = H / 2;
  const qreal dpr = devicePixelRatioF();
  if (m_static.isNull() || m_static.size() != size() * dpr || m_static.devicePixelRatio() != dpr) {
    m_static = QImage(size() * dpr, QImage::Format_ARGB32_Premultiplied);
    m_static.setDevicePixelRatio(dpr);
    m_static.fill(Qt::transparent);
    QPainter q(&m_static);
    q.setRenderHint(QPainter::Antialiasing);
    paintStatic(q);
  }
  QPainter p(this);
  p.drawImage(QPointF(0, 0), m_static);
  p.setRenderHint(QPainter::Antialiasing);
  // pulsing dot (2 s sine between dim and bright gold)
  const double ph = (QDateTime::currentMSecsSinceEpoch() - m_t0) / 2000.0;
  const double k = 0.5 + 0.5 * std::sin(ph * 2 * M_PI);
  const QColor dot = theme::mix(theme::map("#6e6819"), theme::map("#fbe32c"), k);
  QRadialGradient rg(QPointF(38, cy), 7);
  rg.setColorAt(0, theme::alpha(dot, int(110 * k))); rg.setColorAt(1, theme::alpha(dot, 0));
  p.setPen(Qt::NoPen); p.setBrush(rg); p.drawEllipse(QPointF(38, cy), 7, 7);
  p.setBrush(dot); p.drawEllipse(QPointF(38, cy), 4, 4);
  if (m_cursorOn && m_room != 0) p.fillRect(QRectF(titleRight() - 9, cy - 6.5, 8, 12), theme::pal().accent);
}
void TitleStrip::paintStatic(QPainter& p) {
  const int H = height(), cy = H / 2;
  QLinearGradient g(0, 0, 0, H);
  g.setColorAt(0, theme::map("#181205")); g.setColorAt(1, theme::map("#0d0802"));
  p.fillRect(rect(), g);
  p.fillRect(QRect(0, H - 1, width(), 1), sem().strip);
  if (m_glyph == Pulse) {
    QPainterPath path; const qreal y = cy;
    path.moveTo(12, y); path.lineTo(15.5, y); path.lineTo(18, y - 6); path.lineTo(21, y + 6); path.lineTo(23.5, y); path.lineTo(26, y);
    p.setPen(QPen(theme::map("#d8c97a"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush); p.drawPath(path);
  } else {
    // three small waves (the reference's "≋" liquidity glyph)
    p.setPen(QPen(theme::map("#d7cb88"), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
      const qreal y = cy - 4.5 + i * 4.5;
      QPainterPath w; w.moveTo(14, y);
      w.cubicTo(15.5, y - 1.6, 17, y - 1.6, 18.5, y); w.cubicTo(20, y + 1.6, 21.5, y + 1.6, 23, y);
      w.cubicTo(23.6, y - 0.7, 24.3, y - 0.9, 25, y - 0.6);
      p.drawPath(w);
    }
  }
  // title (the dot and the block cursor: paintEvent)
  const QFont f = theme::mono(10, QFont::Normal, 2.5);
  p.setFont(f); p.setPen(sem().muted);
  const QFontMetricsF fm(f);
  p.drawText(QPointF(50, cy + fm.capHeight() / 2), shownTitle());
}

// ------------------------------------------------------------------ Badge
Badge::Badge(QWidget* parent) : QWidget(parent) { setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }
void Badge::setText(const QString& t) {
  if (t == m_text) return;
  const int w0 = sizeHint().width();
  m_text = t;
  if (sizeHint().width() != w0) updateGeometry();   // a scan's "12 / 150" → "13 / 150" keeps the width (mono digits)
  update();
}
void Badge::setProgress(double f) { if (f == m_progress) return; m_progress = f; update(); }
QSize Badge::sizeHint() const { return QSize(int(textWidth(theme::mono(9, QFont::Bold, 1.0), m_text) + 22), 18); }
void Badge::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  p.setPen(QPen(theme::map("#a08a1c"), 1)); p.setBrush(theme::map("#1d1702")); p.drawRoundedRect(r, 2, 2);
  if (m_progress >= 0) p.fillRect(QRectF(1, height() - 3, (width() - 2) * std::clamp(m_progress, 0.0, 1.0), 2), theme::alpha(theme::pal().accent, 160));
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#fff189")); p.drawEllipse(QPointF(8, height() / 2.0), 1.6, 1.6);
  p.setFont(theme::mono(9, QFont::Bold, 1.0)); p.setPen(readable(theme::map("#fff189"), theme::map("#1d1702")));
  p.drawText(QRectF(13, 0, width() - 13, height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

// ------------------------------------------------------------------ KpiCard
KpiCard::KpiCard(const QString& label, QWidget* parent) : QWidget(parent), m_label(label) {}
void KpiCard::setValue(const QString& v, const QColor& c) { if (v == m_value && c == m_valueColor) return; m_value = v; m_valueColor = c; update(); }
void KpiCard::setSub(const QString& s) { if (s == m_sub) return; m_sub = s; update(); }
void KpiCard::setAccent(const QColor& c) { m_accent = c; update(); }
void KpiCard::setValuePx(int px, int w) { m_valuePx = px; m_valueWeight = w; update(); }
void KpiCard::setCustom(std::function<void(QPainter&, const QRectF&)> f) { m_custom = std::move(f); update(); }
void KpiCard::setLabelPx(int px, qreal sp) { m_labelPx = px; m_labelSpacing = sp; update(); }
// Label lines: one line (spacing tightened if needed); too long → two lines at a word break when the card has no
// sub-line (narrow cards, French "TENDANCE HAUSSIÈRE"); the last line elided if still too long (see labelElided()).
QStringList KpiCard::labelLines(QFont* font) const {
  const double room = width() - 16;
  QFont lf = theme::mono(m_labelPx, QFont::Normal, m_labelSpacing);
  if (textWidth(lf, m_label) > room) lf = theme::mono(m_labelPx, QFont::Normal, 0.6);
  if (font) *font = lf;
  if (textWidth(lf, m_label) <= room || !m_sub.isEmpty() || !m_label.contains(' ')) return {m_label};
  const QStringList words = m_label.split(' ');
  QString first = words[0]; int i = 1;
  while (i < words.size() && textWidth(lf, first + " " + words[i]) <= room) first += " " + words[i++];
  return {first, words.mid(i).join(' ')};
}
bool KpiCard::labelElided() const {
  QFont lf; const QStringList l = labelLines(&lf);
  for (const auto& t : l) if (textWidth(lf, t) > width() - 16) return true;
  return false;
}
bool KpiCard::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip && labelElided()) {
    const QString tip = toolTip().isEmpty() ? m_label : m_label + "\n" + toolTip();
    QToolTip::showText(static_cast<QHelpEvent*>(e)->globalPos(), tip, this);
    return true;
  }
  return QWidget::event(e);
}
void KpiCard::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const QRectF r = QRectF(rect());
  p.fillRect(r, theme::map("#0c0801"));
  p.setPen(QPen(theme::map("#40392a"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
  if (m_accent.isValid()) {
    QLinearGradient g(0, 2, 0, 16);
    g.setColorAt(0, theme::alpha(m_accent, 45)); g.setColorAt(1, theme::alpha(m_accent, 0));
    p.fillRect(QRectF(1, 2, r.width() - 2, 14), g);
    p.fillRect(QRectF(0, 0, r.width(), 2), m_accent);
  }
  // corner brackets: top-left and bottom-right, inset 3 px, 10 px arms
  ui::drawBrackets(p, r.adjusted(3, 3, -3, -3), theme::map("#7a735c"), 9, true, true);
  // label: tighter spacing, then elided, when the card is too narrow (full label in the tooltip, see event())
  QFont lf; const QStringList lines = labelLines(&lf);
  const double room = r.width() - 16;
  p.setFont(lf); p.setPen(theme::map("#62665d"));
  for (int i = 0; i < lines.size(); ++i)
    p.drawText(QPointF(11, lines.size() == 1 ? 20 : 17 + i * 11), QFontMetricsF(lf).elidedText(lines[i], Qt::ElideRight, room));
  const double dy = lines.size() > 1 ? 7 : 0;
  if (m_custom) { m_custom(p, QRectF(11, 26 + dy, r.width() - 22, r.height() - 30 - dy)); return; }
  QFont vf = theme::mono(m_valuePx, m_valueWeight);
  p.setFont(vf);
  const qreal base = m_sub.isEmpty() ? r.height() - 15 : 51;
  glowText(p, QPointF(12, base), m_value, m_valueColor, m_valuePx >= 26 ? 3 : 2);
  if (!m_sub.isEmpty()) {
    p.setFont(theme::sans(10)); p.setPen(theme::map("#737069"));
    p.drawText(QPointF(13, 70), m_sub);
  }
}

void simulateHover(QWidget* root, const QPoint& pt) {
  QWidget* w = root->childAt(pt);
  if (!w) w = root;
  const QPointF local = w->mapFrom(root, QPointF(pt));
  QEnterEvent en(local, root->mapToGlobal(QPointF(pt)), root->mapToGlobal(QPointF(pt)));
  QCoreApplication::sendEvent(w, &en);
  QMouseEvent mv(QEvent::MouseMove, local, root->mapToGlobal(QPointF(pt)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(w, &mv);
}

// ------------------------------------------------------------------ ChipGroup
static QFont chipFont() { return theme::mono(10, QFont::Medium, 1.9); }
static QFont countFont() { return theme::mono(10, QFont::Normal, 1.0); }
ChipGroup::ChipGroup(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true); setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFixedHeight(27);
}
void ChipGroup::setChips(const QList<Chip>& c) { m_chips = c; fitWrap(); updateGeometry(); update(); }
void ChipGroup::setCounts(const QList<int>& counts) {
  for (int i = 0; i < m_chips.size() && i < counts.size(); ++i) m_chips[i].count = counts[i];
  fitWrap(); updateGeometry(); update();
}
void ChipGroup::setWrap(int width, int rowH) {
  if (width == m_wrapW && (width == 0 || rowH == m_rowH)) return;
  m_wrapW = width; m_rowH = width > 0 ? rowH : 27;
  if (width == 0) setFixedHeight(27);
  fitWrap(); updateGeometry(); update();
}
void ChipGroup::fitWrap() {
  if (m_wrapW <= 0) return;
  const auto r = rects();
  setFixedHeight(r.isEmpty() ? m_rowH : int(r.last().bottom()) + 2);
}
void ChipGroup::setCurrent(int i) { if (i == m_cur) return; m_cur = i; update(); }
QList<QRectF> ChipGroup::rects() const {
  QList<QRectF> out; qreal x = 2;
  if (m_wrapW > 0) {   // phones: rows filled left to right, each row's chips widened to the full width
    QList<qreal> nw;
    for (const auto& c : m_chips) {
      qreal w = textWidth(chipFont(), c.label) - 1.9 + 18;
      if (c.count >= 0) w += 5 + textWidth(countFont(), QString::number(c.count)) - 1.0;
      nw << w;
    }
    const qreal avail = m_wrapW - 4;
    qreal y = 2;
    for (int i = 0; i < nw.size();) {
      int j = i; qreal sum = 0;
      while (j < nw.size() && (j == i || sum + 1 + nw[j] <= avail)) { sum += (j > i ? 1 : 0) + nw[j]; ++j; }
      const qreal extra = std::max<qreal>(0, (avail - sum) / (j - i));
      x = 2;
      for (int k = i; k < j; ++k) { out << QRectF(x, y, nw[k] + extra, m_rowH - 4); x += nw[k] + extra + 1; }
      y += m_rowH; i = j;
    }
    return out;
  }
  for (const auto& c : m_chips) {
    qreal w = textWidth(chipFont(), c.label) - 1.9 + 18;
    if (c.count >= 0) w += 5 + textWidth(countFont(), QString::number(c.count)) - 1.0;
    out << QRectF(x, 2, w, height() - 4);
    x += w + 1;
  }
  return out;
}
QRectF ChipGroup::chipRect(int i) const { const auto r = rects(); return i >= 0 && i < r.size() ? r[i] : QRectF(); }
QSize ChipGroup::sizeHint() const {
  const auto r = rects();
  if (m_wrapW > 0) return QSize(m_wrapW, height());
  return QSize(r.isEmpty() ? 10 : int(r.last().right() + 3), 27);
}
void ChipGroup::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), sem().cell);
  p.setPen(QPen(sem().subtle, 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
  const auto rs = rects();
  const QFont f = chipFont(), fc = countFont();
  const QFontMetricsF fm(f);
  for (int i = 0; i < rs.size(); ++i) {
    const auto& c = m_chips[i];
    const bool on = i == m_cur, hov = i == m_hover && !on;
    if (on) {   // gold fill with a soft glow (reference box-shadow)
      p.setPen(Qt::NoPen);
      for (int k = 3; k >= 1; --k) { p.setBrush(theme::alpha(sem().goldFill, 26 + (3 - k) * 14)); p.drawRoundedRect(rs[i].adjusted(-k, -k, k, k), 2 + k, 2 + k); }
      p.setBrush(sem().goldFill); p.drawRoundedRect(rs[i], 2, 2);
    }
    const qreal by = rs[i].center().y() + fm.capHeight() / 2;
    QFont lf = f; if (hov) lf.setWeight(QFont::Bold);
    p.setFont(lf);
    p.setPen(on ? readable(theme::map("#1a1400"), sem().goldFill) : hov ? sem().goldText : (c.color.isValid() ? c.color : sem().chipText));
    qreal tx = rs[i].left() + 9;
    if (m_wrapW > 0) {   // widened chips (phones): label and count centred
      qreal nat = textWidth(f, c.label) - 1.9 + 18;
      if (c.count >= 0) nat += 5 + textWidth(fc, QString::number(c.count)) - 1.0;
      tx += (rs[i].width() - nat) / 2;
    }
    p.drawText(QPointF(tx, by), c.label);
    if (c.count >= 0) {
      p.setFont(fc);
      QColor onCount = theme::map("#3a3000");   // Deepwater: dark amber count on the gold chip
      if (!theme::isDefault()) onCount = theme::alpha(readable(theme::map("#1a1400"), sem().goldFill), 170);
      p.setPen(on ? onCount : hov ? sem().goldText : sem().chipCount);
      p.drawText(QPointF(tx + textWidth(f, c.label) - 1.9 + 5, by), QString::number(c.count));
    }
  }
}
void ChipGroup::mouseMoveEvent(QMouseEvent* e) {
  const auto rs = rects(); int h = -1;
  for (int i = 0; i < rs.size(); ++i) if (rs[i].contains(e->position())) h = i;
  if (h != m_hover) { m_hover = h; update(); }
}
void ChipGroup::mousePressEvent(QMouseEvent* e) {
  const auto rs = rects();
  for (int i = 0; i < rs.size(); ++i)
    if (rs[i].contains(e->position()) && i != m_cur) { m_cur = i; update(); emit changed(i); return; }
}
void ChipGroup::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------ SearchBox
SearchBox::SearchBox(const QString& placeholder, bool upper, QWidget* parent) : QWidget(parent) {
  m_edit = new QLineEdit(this);
  m_edit->setPlaceholderText(placeholder);
  m_edit->setFrame(false);
  m_edit->setFocusPolicy(Qt::ClickFocus);   // no caret at startup (the reference box is not focused)
  m_edit->setFont(upper ? theme::mono(10, QFont::Normal, 1.5) : theme::sans(11));
  m_edit->setStyleSheet(QString("QLineEdit{background:transparent;border:none;padding:0;color:%1;placeholder-text-color:%2;}")
                            .arg(theme::pal().text.name(), theme::map(upper ? "#64635e" : "#4d4741").name()));
  QPalette pl = m_edit->palette(); pl.setColor(QPalette::PlaceholderText, theme::map(upper ? "#64635e" : "#4d4741"));
  pl.setColor(QPalette::Text, theme::pal().text);
  m_edit->setPalette(pl);
  connect(m_edit, &QLineEdit::textChanged, this, &SearchBox::textChanged);
}
QString SearchBox::text() const { return m_edit->text(); }
void SearchBox::setFocusToEdit() { m_edit->setFocus(); }
void SearchBox::clear() { m_edit->clear(); }
void SearchBox::setText(const QString& t) { m_edit->setText(t); }
void SearchBox::resizeEvent(QResizeEvent*) { m_edit->setGeometry(22, 1, width() - 26, height() - 2); }
void SearchBox::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), sem().page);
  p.setPen(QPen(sem().subtle, 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
  const qreal cy = height() / 2.0;
  p.setPen(QPen(theme::map("#64635e"), 1.1));
  p.drawEllipse(QPointF(12, cy - 0.5), 3.2, 3.2);
  p.drawLine(QPointF(14.4, cy + 1.9), QPointF(16.2, cy + 3.7));
}
}
