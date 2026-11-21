#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QDesktopServices>
#include <QUrl>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QSvgRenderer>
#include <QHash>
#include <QFile>

namespace ui {

void drawBrackets(QPainter& p, const QRectF& r, const QColor& c, qreal len, bool tl, bool br, bool tr, bool bl) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(c, 2); pen.setCapStyle(Qt::SquareCap); p.setPen(pen);
  const QRectF q = r.adjusted(1, 1, -1, -1);
  if (tl) { p.drawLine(q.topLeft(), q.topLeft() + QPointF(len, 0)); p.drawLine(q.topLeft(), q.topLeft() + QPointF(0, len)); }
  if (tr) { p.drawLine(q.topRight(), q.topRight() - QPointF(len, 0)); p.drawLine(q.topRight(), q.topRight() + QPointF(0, len)); }
  if (bl) { p.drawLine(q.bottomLeft(), q.bottomLeft() + QPointF(len, 0)); p.drawLine(q.bottomLeft(), q.bottomLeft() - QPointF(0, len)); }
  if (br) { p.drawLine(q.bottomRight(), q.bottomRight() - QPointF(len, 0)); p.drawLine(q.bottomRight(), q.bottomRight() - QPointF(0, len)); }
  p.restore();
}

// ---------------------------------------------------------------- Panel
Panel::Panel(const QString& title, QWidget* parent) : QFrame(parent) {
  setAttribute(Qt::WA_StyledBackground, false);
  auto* v = new QVBoxLayout(this);
  v->setContentsMargins(0, 0, 0, 0); v->setSpacing(0);
  m_header = new QWidget(this);
  m_header->setFixedHeight(34);
  auto* h = new QHBoxLayout(m_header);
  h->setContentsMargins(12, 6, 10, 0); h->setSpacing(6);
  m_title = label(title.toUpper(), theme::title(), theme::pal().accentText, m_header);
  m_info = label("ⓘ", theme::mono(11), theme::map("#5f5a50"), m_header);
  m_info->hide();
  h->addWidget(m_title); h->addWidget(m_info); h->addStretch(1);
  m_right = new QHBoxLayout; m_right->setSpacing(6); h->addLayout(m_right);
  v->addWidget(m_header);
  m_body = new QVBoxLayout; m_body->setContentsMargins(12, 4, 12, 10); m_body->setSpacing(6);
  v->addLayout(m_body, 1);
  if (title.isEmpty()) m_header->hide();
}
void Panel::setTitle(const QString& t) { m_title->setText(t.toUpper()); m_header->setVisible(!t.isEmpty()); }
void Panel::setInfo(const QString& tip) { m_info->setToolTip(tip); m_info->setVisible(!tip.isEmpty()); }
void Panel::setBrackets(bool tl, bool br) { m_tl = tl; m_br = br; update(); }
void Panel::setHighlighted(bool on) { m_hi = on; update(); }
void Panel::setTitleHeight(int px) { m_header->setFixedHeight(px); }
void Panel::setPadding(int l, int t, int r, int b) { m_body->setContentsMargins(l, t, r, b); }
void Panel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = theme::pal();
  const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  p.fillRect(rect(), c.panel);
  p.setPen(QPen(m_hi ? c.borderHi : c.border, 1));
  p.drawRect(r);
  drawBrackets(p, QRectF(rect()).adjusted(3, 3, -3, -3), m_hi ? c.accent : c.bracket, 10, m_tl, m_br);
}

// ---------------------------------------------------------------- PillBar
PillBar::PillBar(const QStringList& items, QWidget* parent, Style s) : QWidget(parent), m_items(items), m_style(s) {
  setMouseTracking(true);
  setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
// an empty list keeps index 0 (qBound(0, x, -1) is an assertion failure: Data › Technical data's footprint pills of a
// pair recording no footprint closed the app, 2026-11-20)
void PillBar::setItems(const QStringList& items) {
  m_items = items;
  m_counts.clear();
  m_cur = items.isEmpty() ? 0 : qBound(0, m_cur, int(items.size()) - 1);
  updateGeometry();
  update();
}
void PillBar::setCounts(const QList<int>& c) { m_counts = c; updateGeometry(); update(); }
void PillBar::setCurrent(int i, bool emitSignal) {
  if (i == m_cur) return;
  m_cur = i; update();
  if (emitSignal) emit changed(i);
}
QList<QRectF> PillBar::layoutRects() const {
  QList<QRectF> out;
  const QFontMetricsF fm(theme::mono(m_px, QFont::Medium, 1.0));
  const QFontMetricsF fc(theme::mono(m_px));
  qreal x = 0; const qreal h = m_px + 14;
  for (int i = 0; i < m_items.size(); ++i) {
    qreal w = fm.horizontalAdvance(m_items[i]) + 18;
    if (i < m_counts.size() && m_counts[i] >= 0) w += fc.horizontalAdvance(QString::number(m_counts[i])) + 6;
    out << QRectF(x, 0, w, h);
    x += w + 2;
  }
  return out;
}
QSize PillBar::sizeHint() const {
  const auto r = layoutRects();
  return QSize(r.isEmpty() ? 0 : int(r.last().right() + 1), m_px + 14);
}
void PillBar::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& c = theme::pal();
  const auto rects = layoutRects();
  const QFont f = theme::mono(m_px, QFont::Medium, 1.0), fc = theme::mono(m_px);
  for (int i = 0; i < rects.size(); ++i) {
    const QRectF r = rects[i];
    const bool on = i == m_cur, hov = i == m_hover;
    if (on) {
      if (m_style == Filled) { p.setPen(Qt::NoPen); p.setBrush(c.accentDim); p.drawRoundedRect(r, 2, 2); }
      else if (m_style == Boxed) { p.setPen(QPen(c.accent, 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2); }
      else { p.fillRect(QRectF(r.left() + 6, r.bottom() - 2, r.width() - 12, 2), c.accent); }
    } else if (hov) { p.setPen(Qt::NoPen); p.setBrush(c.panelAlt); p.drawRoundedRect(r, 2, 2); }
    p.setFont(f);
    p.setPen(on ? c.accent : hov ? c.text : c.textDim);
    const QFontMetricsF fm(f);
    const qreal tw = fm.horizontalAdvance(m_items[i]);
    p.drawText(QPointF(r.left() + 9, r.center().y() + fm.ascent() / 2 - 1), m_items[i]);
    if (i < m_counts.size() && m_counts[i] >= 0) {
      p.setFont(fc); p.setPen(on ? theme::alpha(c.accent, 150) : c.textMute);
      p.drawText(QPointF(r.left() + 9 + tw + 6, r.center().y() + fm.ascent() / 2 - 1), QString::number(m_counts[i]));
    }
  }
}
void PillBar::mousePressEvent(QMouseEvent* e) {
  const auto rects = layoutRects();
  for (int i = 0; i < rects.size(); ++i)
    if (rects[i].contains(e->position())) { if (i != m_cur) { m_cur = i; update(); emit changed(i); } return; }
}
void PillBar::mouseMoveEvent(QMouseEvent* e) {
  const auto rects = layoutRects(); int h = -1;
  for (int i = 0; i < rects.size(); ++i) if (rects[i].contains(e->position())) h = i;
  if (h != m_hover) { m_hover = h; update(); }
}
void PillBar::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ---------------------------------------------------------------- LiveBadge
LiveBadge::LiveBadge(QWidget* parent) : QWidget(parent), m_anim(new QVariantAnimation(this)) {
  m_anim->setStartValue(0.0); m_anim->setEndValue(1.0); m_anim->setDuration(1600); m_anim->setLoopCount(-1);
  connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_phase = v.toReal(); update(); });
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
void LiveBadge::setState(bool live, const QString& text) { m_live = live; m_text = text; updateGeometry(); update(); }
QSize LiveBadge::sizeHint() const { return QSize(QFontMetrics(theme::mono(10, QFont::Bold, 1)).horizontalAdvance(m_text) + 26, 18); }
void LiveBadge::showEvent(QShowEvent*) { m_anim->start(); }
void LiveBadge::hideEvent(QHideEvent*) { m_anim->stop(); }
void LiveBadge::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const auto& c = theme::pal();
  const QColor col = m_live ? c.accent : c.textMute;
  p.setPen(Qt::NoPen); p.setBrush(theme::alpha(col, 40)); p.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 2, 2);
  const QPointF dot(9, height() / 2.0);
  if (m_live) { p.setBrush(theme::alpha(col, int(120 * (1 - m_phase)))); p.drawEllipse(dot, 2.5 + 4 * m_phase, 2.5 + 4 * m_phase); }
  p.setBrush(col); p.drawEllipse(dot, 2.5, 2.5);
  p.setFont(theme::mono(10, QFont::Bold, 1)); p.setPen(col);
  p.drawText(QRectF(16, 0, width() - 18, height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

QLabel* label(const QString& text, const QFont& f, const QColor& c, QWidget* parent) {
  auto* l = new QLabel(parent);
  l->setTextFormat(Qt::PlainText);
  l->setText(text);
  l->setFont(f);
  QPalette pl = l->palette(); pl.setColor(QPalette::WindowText, c); l->setPalette(pl);
  l->setStyleSheet(QString("color:%1;background:transparent;").arg(c.name(QColor::HexArgb)));
  return l;
}

QMessageBox::StandardButton askPlain(QWidget* parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons,
                                     QMessageBox::StandardButton defaultButton) {
  QMessageBox box(QMessageBox::Question, title, text, buttons, parent);
  box.setTextFormat(Qt::PlainText);
  if (defaultButton != QMessageBox::NoButton) box.setDefaultButton(defaultButton);
  return QMessageBox::StandardButton(box.exec());
}
void warnPlain(QWidget* parent, const QString& title, const QString& text) {
  QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
  box.setTextFormat(Qt::PlainText);
  box.exec();
}
QString plainTip(const QString& text) { return text.isEmpty() ? text : Qt::convertFromPlainText(text, Qt::WhiteSpaceNormal); }

// ---------------------------------------------------------------- StatusOverlay
StatusOverlay::StatusOverlay(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  parent->installEventFilter(this);
  hide();
}
void StatusOverlay::showLoading(const QString& what) { m_text = what + "…"; m_error = false; setGeometry(parentWidget()->rect()); raise(); show(); update(); }
void StatusOverlay::showError(const QString& msg) { m_text = msg; m_error = true; setGeometry(parentWidget()->rect()); raise(); show(); update(); }
void StatusOverlay::clear() { hide(); }
bool StatusOverlay::eventFilter(QObject* o, QEvent* e) {
  if (o == parentWidget() && e->type() == QEvent::Resize) setGeometry(parentWidget()->rect());
  return false;
}
void StatusOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setFont(theme::mono(11, QFont::Normal, 1));
  p.setPen(m_error ? theme::pal().down : theme::pal().textMute);
  p.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, m_text);
}

// ---------------------------------------------------------------- flags
QString currencyToIso2(const QString& ccy) {
  static const QHash<QString, QString> m{
    {"USD", "us"}, {"EUR", "eu"}, {"GBP", "gb"}, {"JPY", "jp"}, {"CHF", "ch"}, {"CAD", "ca"}, {"AUD", "au"},
    {"NZD", "nz"}, {"CNY", "cn"}, {"CNH", "cn"}, {"HKD", "hk"}, {"SGD", "sg"}, {"SEK", "se"}, {"NOK", "no"},
    {"DKK", "dk"}, {"PLN", "pl"}, {"HUF", "hu"}, {"CZK", "cz"}, {"TRY", "tr"}, {"ZAR", "za"}, {"MXN", "mx"},
    {"BRL", "br"}, {"INR", "in"}, {"KRW", "kr"}, {"RUB", "ru"}, {"ILS", "il"}, {"THB", "th"}, {"IDR", "id"},
    {"SAR", "sa"}, {"AED", "ae"}, {"ARS", "ar"}, {"CLP", "cl"}, {"COP", "co"}, {"TWD", "tw"}, {"MYR", "my"},
    {"PHP", "ph"}};
  return m.value(ccy.toUpper());
}
static QString iso3to2(const QString& c) {
  static const QHash<QString, QString> m{
    {"USA", "us"}, {"GBR", "gb"}, {"JPN", "jp"}, {"CHE", "ch"}, {"CAN", "ca"}, {"AUS", "au"}, {"NZL", "nz"},
    {"CHN", "cn"}, {"DEU", "de"}, {"FRA", "fr"}, {"ITA", "it"}, {"ESP", "es"}, {"IND", "in"}, {"BRA", "br"},
    {"KOR", "kr"}, {"RUS", "ru"}, {"MEX", "mx"}, {"IDN", "id"}, {"TUR", "tr"}, {"SAU", "sa"}, {"ARG", "ar"},
    {"ZAF", "za"}, {"SWE", "se"}, {"NOR", "no"}, {"DNK", "dk"}, {"POL", "pl"}, {"NLD", "nl"}, {"SGP", "sg"},
    {"HKG", "hk"}, {"EMU", "eu"}, {"EA", "eu"}, {"EUR", "eu"}, {"EUZ", "eu"}};
  return m.value(c.toUpper());
}
QPixmap flag(const QString& code, QSize size, qreal dpr) {
  QString iso = code.toLower();
  if (code.size() == 3) { const QString a = currencyToIso2(code); iso = a.isEmpty() ? iso3to2(code) : a; }
  if (code.compare("EA", Qt::CaseInsensitive) == 0 || code.compare("EZ", Qt::CaseInsensitive) == 0) iso = "eu";
  if (code.compare("UK", Qt::CaseInsensitive) == 0) iso = "gb";
  static QHash<QString, QPixmap> cache;
  const QString key = QString("%1_%2x%3_%4").arg(iso).arg(size.width()).arg(size.height()).arg(dpr);
  if (auto it = cache.constFind(key); it != cache.cend()) return *it;
  QPixmap pm(size * dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent);
  QSvgRenderer svg(QString(":/flags/%1.svg").arg(iso));
  if (svg.isValid()) {
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip; clip.addRoundedRect(QRectF(0, 0, size.width(), size.height()), 1.5, 1.5);
    p.setClipPath(clip);
    svg.render(&p, QRectF(0, 0, size.width(), size.height()));
  }
  cache.insert(key, pm);
  return pm;
}

bool openWebLink(const QString& link, const QString& base) {
  QUrl u(link.trimmed());
  if (u.isRelative() && !base.isEmpty()) {
    const QUrl b(base.trimmed());
    const QString bs = b.scheme().toLower();
    if (bs == "http" || bs == "https") u = b.resolved(u);
  }
  const QString scheme = u.scheme().toLower();
  if (!u.isValid() || (scheme != "http" && scheme != "https") || u.host().isEmpty()) return false;
  return QDesktopServices::openUrl(u);
}
}
