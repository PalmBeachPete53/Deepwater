#include "pages/account/AccountKit.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QVariantAnimation>
#include <QMenu>
#include <QRegularExpression>
#include <QLocale>
#include <QCursor>
#include <QTimer>
#include <QApplication>
#include <algorithm>

namespace acct {

const Colors& c() {
  // Deepwater values (the struct's defaults) projected onto the current theme; rebuilt on a theme change
  static Colors k; static int rev = -1;
  if (rev != theme::revision()) {
    const Colors d;
    k = d;
    QColor* dst = reinterpret_cast<QColor*>(&k);
    const QColor* src = reinterpret_cast<const QColor*>(&d);
    for (size_t i = 0; i < sizeof(Colors) / sizeof(QColor); ++i) dst[i] = theme::map(src[i]);
    rev = theme::revision();
  }
  return k;
}
QString qss(QString css) {
  static const QRegularExpression hex("#[0-9a-fA-F]{6}\\b");
  QString out; qsizetype last = 0;
  for (auto it = hex.globalMatch(css); it.hasNext();) {
    const auto m = it.next();
    out += css.mid(last, m.capturedStart() - last) + theme::map(QColor(m.captured())).name();
    last = m.capturedEnd();
  }
  return out + css.mid(last);
}

QFont mono(int px, bool bold, qreal sp) { return theme::mono(px, bold ? QFont::Bold : QFont::Normal, sp); }
QFont chart(int px, bool bold) { return theme::sans(px, bold ? QFont::DemiBold : QFont::Normal); }
QFont cond(int px, bool bold) { return theme::condensed(px, bold ? QFont::Bold : QFont::DemiBold); }

QString ccySymbol(const QString& ccy) {
  const QString k = ccy.toUpper();
  if (k == "USD" || k.isEmpty()) return "$";
  if (k == "EUR") return "€";
  if (k == "GBP") return "£";
  if (k == "JPY") return "¥";
  if (k == "AUD") return "A$";
  if (k == "CAD") return "C$";
  if (k == "NZD") return "NZ$";
  return k + " ";
}
QString money(double v, const QString& ccy, int d) {
  if (!std::isfinite(v)) return fmt::dash();
  const QString n = fmt::num(std::abs(v), d);
  const bool zero = n == fmt::num(0, d);
  return (v < 0 && !zero ? "-" : "") + ccySymbol(ccy) + n;
}
QString signedMoney(double v, const QString& ccy, int d) {
  if (!std::isfinite(v)) return fmt::dash();
  const QString n = fmt::num(std::abs(v), d);
  if (n == fmt::num(0, d)) return ccySymbol(ccy) + n;
  return (v < 0 ? "-" : "+") + ccySymbol(ccy) + n;
}
QString axisMoney(double v, const QString& ccy) {
  const double a = std::abs(v);
  QString n;
  if (a >= 1e6) n = fmt::num(a / 1e6, 1) + "M";
  else if (a >= 1000) n = fmt::num(a / 1000, 1) + "K";
  else n = fmt::num(a, a == std::floor(a) ? 0 : 2);
  return (v < -1e-9 ? QString(QChar(0x2212)) : QString()) + ccySymbol(ccy) + n;
}
QString trimNum(double v, int maxDec) {
  if (!std::isfinite(v)) return fmt::dash();
  QString s = fmt::num(v, maxDec);
  if (s.contains('.')) { while (s.endsWith('0')) s.chop(1); if (s.endsWith('.')) s.chop(1); }
  return s;
}
static QString monthShort(int m) {
  // English as in the reference ("Sept"); other languages: the locale's short month name ("sept.")
  // French: 3–4 letters without the locale's trailing dot, so table dates keep the English width
  if (i18n::current() == i18n::Lang::French) {
    static const char* f[] = {"janv", "févr", "mars", "avr", "mai", "juin", "juil", "août", "sept", "oct", "nov", "déc"};
    return QString::fromUtf8(f[std::clamp(m, 1, 12) - 1]);
  }
  if (i18n::current() != i18n::Lang::English) return i18n::locale().monthName(std::clamp(m, 1, 12), QLocale::ShortFormat);
  static const char* n[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept", "Oct", "Nov", "Dec"};
  return n[std::clamp(m, 1, 12) - 1];
}
QString tradeDate(const QDateTime& utc) {
  if (!utc.isValid()) return fmt::dash();
  const QDateTime t = utc.toLocalTime();
  return QString("%1 %2 %3, %4").arg(t.date().day(), 2, 10, QChar('0')).arg(monthShort(t.date().month()))
      .arg(t.date().year() % 100, 2, 10, QChar('0')).arg(t.toString("HH:mm"));
}
QString longDate(const QDateTime& utc) {
  if (!utc.isValid()) return fmt::dash();
  const QDateTime t = utc.toLocalTime();
  return QString("%1 %2 %3, %4").arg(t.date().day()).arg(monthShort(t.date().month())).arg(t.date().year()).arg(t.toString("HH:mm"));
}
QString unitLabel(const QString& u) {
  // the words are listed for lupdate; symbols (oz, lb, MMBtu) stay as they are
  static const char* const words[] = {QT_TRANSLATE_NOOP("account", "units"), QT_TRANSLATE_NOOP("account", "barrels"),
                                      QT_TRANSLATE_NOOP("account", "coins"), QT_TRANSLATE_NOOP("account", "shares"),
                                      QT_TRANSLATE_NOOP("account", "contracts"), QT_TRANSLATE_NOOP("account", "gallons"),
                                      QT_TRANSLATE_NOOP("account", "bushels"), QT_TRANSLATE_NOOP("account", "tonnes")};
  for (const char* w : words) if (u == QLatin1String(w)) return QCoreApplication::translate("account", w);
  return u;
}
QString tagLabel(const QString& t) {
  static const char* const tags[] = {QT_TRANSLATE_NOOP("account", "METAL"), QT_TRANSLATE_NOOP("account", "INDEX"),
                                     QT_TRANSLATE_NOOP("account", "ENERGY"), QT_TRANSLATE_NOOP("account", "AGRI"),
                                     QT_TRANSLATE_NOOP("account", "STOCK"), QT_TRANSLATE_NOOP("account", "BOND")};
  for (const char* w : tags) if (t == QLatin1String(w)) return QCoreApplication::translate("account", w);
  return t;
}
QString held(double h) {
  if (!std::isfinite(h)) return fmt::dash();
  if (h < 1) return QString("%1m").arg(qRound(h * 60));
  if (h >= 100) return QCoreApplication::translate("account", "%1d", "days held").arg(fmt::num(h / 24, 1));
  return fmt::num(h, 1) + "h";
}

void panelFrame(QPainter& p, const QRectF& r, bool hi) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, false);
  const QRectF b = r.adjusted(0, 0, -1, -1);
  p.fillRect(r, c().panel);
  if (hi) {
    // faint vertical yellow glow at the edges
    QLinearGradient g(r.topLeft(), r.topRight());
    g.setColorAt(0, theme::map(80, 60, 5, 22)); g.setColorAt(0.04, theme::map(0, 0, 0, 0));
    g.setColorAt(0.96, theme::map(0, 0, 0, 0)); g.setColorAt(1, theme::map(80, 60, 5, 22));
    p.fillRect(r, g);
    p.setPen(theme::map("#201805"));
    p.drawRect(b);
    QPen pen(c().bracketHi, 2); pen.setCapStyle(Qt::FlatCap); p.setPen(pen);
    const qreal L = 12;
    const QRectF q = r.adjusted(1, 1, -1, -1);
    auto corner = [&](QPointF o, qreal dx, qreal dy) {
      p.drawLine(o, o + QPointF(dx * L, 0)); p.drawLine(o, o + QPointF(0, dy * L));
    };
    corner(q.topLeft(), 1, 1); corner(q.topRight(), -1, 1); corner(q.bottomLeft(), 1, -1); corner(q.bottomRight(), -1, -1);
  } else {
    p.setPen(c().borderSide);
    p.drawRect(b);
    p.setPen(c().border);
    p.drawLine(b.topLeft(), b.topRight());
    QPen pen(c().bracketTL, 2); pen.setCapStyle(Qt::FlatCap); p.setPen(pen);
    const QPointF tl = r.topLeft() + QPointF(7, 7);
    p.drawLine(tl, tl + QPointF(9, 0)); p.drawLine(tl - QPointF(0, 1), tl + QPointF(0, 9));
    pen.setColor(c().bracketBR); p.setPen(pen);
    const QPointF br = r.bottomRight() + QPointF(-7, -7);
    p.drawLine(br, br - QPointF(9, 0)); p.drawLine(br + QPointF(0, 1), br - QPointF(0, 9));
  }
  p.restore();
}

void infoIcon(QPainter& p, const QRectF& r) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(theme::map("#6b665a"), 1));
  p.setBrush(Qt::NoBrush);
  const QPointF ctr = r.center();
  p.drawEllipse(ctr, 4.5, 4.5);
  p.drawLine(QPointF(ctr.x(), ctr.y() - 0.5), QPointF(ctr.x(), ctr.y() + 2.5));
  p.drawPoint(QPointF(ctr.x(), ctr.y() - 2.2));
  p.restore();
}

QRectF panelTitle(QPainter& p, const QRectF& panel, const QString& title, bool info, const QColor& col, int px, qreal sp) {
  p.save();
  const QFont f = mono(px, false, sp);
  p.setFont(f);
  p.setPen(col.isValid() ? col : c().grey);
  const QFontMetricsF fm(f);
  const QPointF at(panel.left() + 13, panel.top() + 19 + fm.ascent() / 2 - 1);
  p.drawText(at, title);
  QRectF ir;
  if (info) {
    const qreal x = at.x() + fm.horizontalAdvance(title) + 3;
    ir = QRectF(x, panel.top() + 19 - 5.5, 11, 11);
    infoIcon(p, ir);
  }
  p.restore();
  return ir;
}

void tooltip(QPainter& p, const QPointF& at, const QList<TipLine>& lines, const QRectF& bounds) {
  if (lines.isEmpty()) return;
  p.save();
  const QFont f = mono(12, false, 0.3), fb = mono(12, true, 0.3);   // reference glyph pitch ≈7.5 px
  const qreal lh = 19, pad = 12;
  qreal w = 0;
  for (const auto& l : lines) {
    const qreal a = QFontMetricsF(l.bold ? fb : f).horizontalAdvance(l.text);
    const qreal b = l.value.isEmpty() ? 0 : QFontMetricsF(l.valueBold ? fb : f).horizontalAdvance(l.value);
    w = std::max(w, a + b);
  }
  const QSizeF sz(w + 2 * pad, lines.size() * lh + 2 * pad - 4);
  QRectF box(at + QPointF(22, 22), sz);
  if (box.right() > bounds.right() - 10) box.moveRight(at.x() - 26);   // flips left before reaching the panel edge
  if (box.bottom() > bounds.bottom() - 2) box.moveBottom(at.y() - 12);
  if (box.left() < bounds.left() + 2) box.moveLeft(bounds.left() + 2);
  if (box.top() < bounds.top() + 2) box.moveTop(bounds.top() + 2);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(c().tipBorder, 1));
  p.setBrush(c().tipBg);
  p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
  const QFontMetricsF fm(f);
  for (int i = 0; i < lines.size(); ++i) {
    const auto& l = lines[i];
    const qreal y = box.top() + pad - 2 + fm.ascent() + i * lh;
    p.setFont(l.bold ? fb : f);
    p.setPen(l.color.isValid() ? l.color : c().white);
    p.drawText(QPointF(box.left() + pad, y), l.text);
    if (!l.value.isEmpty()) {
      const qreal x = box.left() + pad + QFontMetricsF(l.bold ? fb : f).horizontalAdvance(l.text);
      p.setFont(l.valueBold ? fb : f);
      p.setPen(l.valueColor.isValid() ? l.valueColor : c().white);
      p.drawText(QPointF(x, y), l.value);
    }
  }
  p.restore();
}

// ---------------------------------------------------------------- PaintPanel
static QPoint g_fake(-1, -1);
static bool g_hasFake = false;
void PaintPanel::setFakeCursor(const QPoint& g) { g_fake = g; g_hasFake = true; }
bool PaintPanel::hasFakeCursor() { return g_hasFake; }
QPoint PaintPanel::fakeCursor() { return g_fake; }

PaintPanel::PaintPanel(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent, false);
}
void PaintPanel::animate(int ms) {
  if (!m_va) {
    m_va = new QVariantAnimation(this);
    m_va->setStartValue(0.0); m_va->setEndValue(1.0); m_va->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_va, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_anim = v.toDouble(); update(); });
  }
  m_va->stop(); m_va->setDuration(ms); m_anim = 0; m_va->start();
}
QPointF PaintPanel::mousePos() const {
  if (!m_hover) return QPointF(NAN, NAN);
  const QPoint g = g_hasFake ? g_fake : QCursor::pos();
  const QPoint l = mapFromGlobal(g);
  if (!rect().contains(l) || !isVisible()) return QPointF(NAN, NAN);
  if (!g_hasFake && !visibleRegion().contains(l)) return QPointF(NAN, NAN);
  if (!g_hasFake && QWidget::window() && !window()->underMouse()) return QPointF(NAN, NAN);
  return QPointF(l) + QPointF(0.5, 0.5);
}
void PaintPanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing);
  const QRectF r = rect();
  if (m_frame) panelFrame(p, r, m_hi);
  if (!m_title.isEmpty()) m_infoRect = panelTitle(p, r, m_title, !m_info.isEmpty(), m_tcol, m_tpx, m_tsp);
  if (m_fn) m_fn(p, r, mousePos(), m_anim);
}
void PaintPanel::mouseMoveEvent(QMouseEvent*) { if (m_hover) update(); }
void PaintPanel::mousePressEvent(QMouseEvent* e) { emit clicked(e->position()); }
void PaintPanel::leaveEvent(QEvent*) { if (m_hover) update(); }
bool PaintPanel::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    if (!m_info.isEmpty() && m_infoRect.adjusted(-4, -4, 4, 4).contains(he->pos())) QToolTip::showText(he->globalPos(), m_info, this);
    else QToolTip::hideText();
    return true;
  }
  return QWidget::event(e);
}

// ---------------------------------------------------------------- ComboChip
ComboChip::ComboChip(QWidget* parent) : QWidget(parent) {
  setCursor(Qt::PointingHandCursor);
  setFixedHeight(26);
}
void ComboChip::setItems(const QStringList& items, int cur) { m_items = items; m_cur = std::clamp(cur, 0, int(items.size()) - 1); updateGeometry(); update(); }
void ComboChip::setCurrent(int i) { if (i == m_cur || i < 0 || i >= m_items.size()) return; m_cur = i; updateGeometry(); update(); }
QSize ComboChip::sizeHint() const {
  const QFontMetrics fm(chart(11));
  return QSize(fm.horizontalAdvance(currentText()) + 10 + 24, 26);
}
void ComboChip::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  p.setPen(m_hov ? theme::map("#4a4428") : theme::map("#2b2817"));
  p.setBrush(theme::map("#120d02"));
  p.drawRoundedRect(r, 3, 3);
  p.setFont(chart(11));
  p.setPen(theme::map("#d6d0c0"));
  p.drawText(QRectF(10, 0, width() - 30, height()), Qt::AlignVCenter | Qt::AlignLeft, currentText());
  // chevron
  QPen pen(theme::map("#f4f1ea"), 2); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin); p.setPen(pen);
  const QPointF ch(width() - 12, height() / 2.0 + 1);
  QPolygonF poly{ch + QPointF(-4, -2.5), ch + QPointF(0, 1.5), ch + QPointF(4, -2.5)};
  p.drawPolyline(poly);
}
void ComboChip::mousePressEvent(QMouseEvent*) {
  QMenu m(this);
  styleMenu(&m);
  for (int i = 0; i < m_items.size(); ++i) {
    auto* a = m.addAction(m_items[i]);
    a->setCheckable(true); a->setChecked(i == m_cur); a->setData(i);
  }
  if (auto* a = m.exec(mapToGlobal(QPoint(0, height() + 2)))) {
    const int i = a->data().toInt();
    if (i != m_cur) { m_cur = i; updateGeometry(); update(); emit changed(i); }
  }
}
void ComboChip::enterEvent(QEnterEvent*) { m_hov = true; update(); }
void ComboChip::leaveEvent(QEvent*) { m_hov = false; update(); }

void installTestHooks(QWidget* page) {
  const QString clicks = qEnvironmentVariable("DW_CLICK");
  int k = 0;
  for (const auto& c : clicks.split(';', Qt::SkipEmptyParts)) {
    const auto xy = c.split(',');
    if (xy.size() != 2) continue;
    const QPoint pt(xy[0].toInt(), xy[1].toInt());
    QTimer::singleShot(800 + 400 * k++, page, [page, pt] {
      QWidget* w = page->childAt(pt);
      if (!w) w = page;
      const QPoint local = w->mapFrom(page, pt);
      if (w->focusPolicy() & Qt::ClickFocus) w->setFocus(Qt::MouseFocusReason);   // as a real click would
      QMouseEvent press(QEvent::MouseButtonPress, local, w->mapToGlobal(local), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(w, &press);
      QMouseEvent release(QEvent::MouseButtonRelease, local, w->mapToGlobal(local), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent(w, &release);
    });
  }
  // DW_GRAB_MODAL=file.png: saves the open dialog (opened with DW_CLICK) 2.5 s after start, then closes it
  if (qEnvironmentVariableIsSet("DW_GRAB_MODAL")) {
    QTimer::singleShot(2500, page, [] {
      if (QWidget* w = QApplication::activeModalWidget()) { w->grab().save(qEnvironmentVariable("DW_GRAB_MODAL")); w->close(); }
    });
  }
  if (qEnvironmentVariableIsSet("DW_HOVER")) {
    QTimer::singleShot(1500, page, [page] {
      const auto xy = qEnvironmentVariable("DW_HOVER").split(',');
      if (xy.size() == 2) PaintPanel::setFakeCursor(page->mapToGlobal(QPoint(xy[0].toInt(), xy[1].toInt())));
      page->update();
      for (auto* w : page->findChildren<QWidget*>()) w->update();
    });
  }
}

void styleMenu(QMenu* m) {
  m->setStyleSheet(qss(
    "QMenu { background:#0f0a03; border:1px solid #3a351b; color:#ece8de; font-family:'JetBrains Mono'; font-size:12px; padding:4px 0; }"
    "QMenu::item { padding:6px 22px 6px 22px; }"
    "QMenu::item:selected { background:#433603; color:#f0d040; }"
    "QMenu::item:disabled { color:#5d584c; }"
    "QMenu::separator { height:1px; background:#2d2a15; margin:4px 8px; }"
    "QMenu::indicator { width:10px; height:10px; left:6px; }"
    "QMenu::indicator:checked { background:#fdd51f; border-radius:2px; }"));
}

QVector<double> ticks(double lo, double hi, int n, double* oLo, double* oHi) {
  lo = std::min(lo, 0.0); hi = std::max(hi, 0.0);
  if (hi - lo < 1e-9) { hi = lo + 1; }
  const double raw = (hi - lo) / std::max(1, n);
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double f = raw / mag;
  const double step = (f <= 1 ? 1 : f <= 2 ? 2 : f <= 2.5 ? 2.5 : f <= 3 ? 3 : f <= 5 ? 5 : 10) * mag;
  const double a = std::floor(lo / step + 1e-9) * step, b = std::ceil(hi / step - 1e-9) * step;
  QVector<double> out;
  for (double v = a; v <= b + step * 1e-6; v += step) out << (std::abs(v) < step * 1e-9 ? 0.0 : v);
  if (oLo) *oLo = a;
  if (oHi) *oHi = b;
  return out;
}
}
