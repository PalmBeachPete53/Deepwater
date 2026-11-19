#include "pages/market/seasonality/SeasonWidgets.h"
#include "pages/market/seasonality/SeasonStyle.h"
#include "ui/Theme.h"
#include "ui/Chart.h"
#include <QPainter>
#include <QCoreApplication>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLineEdit>
#include <QKeyEvent>
#include <QToolTip>
#include <QHelpEvent>
#include <QtMath>
#include <QIcon>
#include <QAction>

using namespace seasonality;

namespace seasonality {

QFont ui(int px, int weight) { return theme::condensed(px, weight); }
QFont mono(int px, int weight, qreal ls) { return theme::mono(px, weight, ls); }

QString signedPct(double v, int dec) {
  if (!std::isfinite(v)) return QString::fromUtf8("—");
  if (std::abs(v) < 0.5 * std::pow(10.0, -dec)) v = 0;
  return (v >= 0 ? "+" : "") + QString::number(v, 'f', dec) + "%";
}

QRectF drawPeriodPill(QPainter& p, qreal right, qreal top, const QString& text) {
  const QFont f = mono(10, QFont::Normal, 1.5);
  const QFontMetricsF fm(f);
  const qreal w = fm.horizontalAdvance(text) + 12, h = 15;
  const QRectF r(right - w, top, w, h);
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen); p.setBrush(col().pillBg); p.drawRoundedRect(r, 2, 2);
  p.setFont(f); p.setPen(col().pillText);
  p.drawText(r.adjusted(1, 0, 0, 0), Qt::AlignCenter, text);
  p.restore();
  return r;
}

void drawCardFrame(QPainter& p, const QRect& r) {
  p.fillRect(r, col().card);
  p.setPen(col().cardBorder); p.setBrush(Qt::NoBrush);
  p.drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
}

void drawCardTitle(QPainter& p, const QPointF& at, const QString& text, bool centred, qreal width) {
  p.setFont(mono(14, QFont::Bold, 2.2));
  p.setPen(col().title);
  if (centred) p.drawText(QRectF(at.x(), at.y() - 10, width, 20), Qt::AlignCenter, text);
  else p.drawText(QRectF(at.x(), at.y() - 10, 2000, 20), Qt::AlignLeft | Qt::AlignVCenter, text);
}

static void chevron(QPainter& p, const QPointF& c, bool up, const QColor& color) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  QPen pen(color, 1.1); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen);
  const qreal d = up ? -1 : 1;
  QPolygonF pl; pl << c + QPointF(-3, -1.5 * d) << c + QPointF(0, 1.5 * d) << c + QPointF(3, -1.5 * d);
  p.drawPolyline(pl);
  p.restore();
}

// ================================================================ ToolBar
ToolBar::ToolBar(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setFixedHeight(39);
  m_rects.resize(Refresh + 1);
}
void ToolBar::setTexts(const QString& a, const QString& y, const QString& c) {
  m_asset = a; m_years = y; m_compare = c; relayout();
}
void ToolBar::setOpen(int item) { m_open = item; update(); }
void ToolBar::setSegment(int seg, bool clicked) { m_seg = seg; m_segClicked = clicked; update(); }
void ToolBar::setToggles(bool a, bool i) { m_actual = a; m_index = i; update(); }
QRect ToolBar::itemRect(int item) const { return (item >= 0 && item < m_rects.size()) ? m_rects[item] : QRect(); }

void ToolBar::relayout() {
  const QFontMetrics fm(ui(12));
  const int top = 6, h = 26;
  int x = 14;
  auto button = [&](int item, const QString& text, bool chev) {
    const int w = fm.horizontalAdvance(text) + 16 + (chev ? 14 : 0);
    m_rects[item] = QRect(x, top, w, h); x += w + 7;
  };
  button(Asset, QCoreApplication::translate("seasonal", "Seasonality: %1").arg(m_asset), true);
  x += 2; m_rects[Help] = QRect(x, top + 5, 16, 16); x += 26;
  x += 9; m_rects[Prev] = QRect(x - 2, top, 16, h); x += 22;
  m_rects[Next] = QRect(x - 2, top, 16, h); x += 20;
  button(Years, m_years, true);
  button(Filter, QCoreApplication::translate("seasonal", "Filter"), true);
  // segmented group: Full H1 H2 Q1 Q2 Q3 Q4
  const QStringList seg = QStringList{QCoreApplication::translate("seasonal", "Full"), QCoreApplication::translate("seasonal", "H1"), QCoreApplication::translate("seasonal", "H2"), QCoreApplication::translate("seasonal", "Q1"), QCoreApplication::translate("seasonal", "Q2"), QCoreApplication::translate("seasonal", "Q3"), QCoreApplication::translate("seasonal", "Q4")};
  const QFontMetrics sm(ui(12));
  int sx = x + 3;
  for (int i = 0; i < 7; ++i) {
    const int w = qMax(17, sm.horizontalAdvance(seg[i]) + 7);
    m_rects[Seg + i] = QRect(sx, top + 3, w, h - 6); sx += w + 3;
  }
  x = sx - 3 + 4 + 7;
  button(Compare, m_compare.isEmpty() ? QCoreApplication::translate("seasonal", "Compare") : QCoreApplication::translate("seasonal", "vs %1").arg(m_compare), true);
  button(Actual, m_actualLabel, false);
  button(Index, QCoreApplication::translate("seasonal", "Index (100)"), false);
  m_rects[Refresh] = QRect(width() - 30, top + 4, 18, 18);
  update();
}

int ToolBar::hit(const QPoint& p) const {
  for (int i = 0; i < m_rects.size(); ++i) if (m_rects[i].adjusted(-2, -2, 2, 2).contains(p)) return i;
  return None;
}

bool ToolBar::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const int i = hit(he->pos());
    QString tip;
    if (i == Help)
      tip = QCoreApplication::translate("seasonal", "Seasonality: the average path of the calendar year over the chosen number of years, from real daily "
            "closes (Yahoo Finance).\nEach year starts at 0% on Jan 1; the curve is the mean of the yearly "
            "cumulative returns.\nPick a period (Full, H1…Q4) or drag on the chart: every card recomputes for it.");
    else if (i == Prev) tip = QCoreApplication::translate("seasonal", "Previous asset");
    else if (i == Next) tip = QCoreApplication::translate("seasonal", "Next asset");
    else if (i == Actual) tip = QCoreApplication::translate("seasonal", "Overlay this year's real path on the seasonal average");
    else if (i == Index) tip = QCoreApplication::translate("seasonal", "Show the path rebased to 100 instead of a % return");
    else if (i == Refresh) tip = QCoreApplication::translate("seasonal", "Reload the daily history");
    else if (i == Compare) tip = QCoreApplication::translate("seasonal", "Overlay the seasonal path of another asset");
    if (tip.isEmpty()) QToolTip::hideText(); else QToolTip::showText(he->globalPos(), tip, this);
    return true;
  }
  return QWidget::event(e);
}

void ToolBar::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  p.fillRect(rect(), c.strip);
  p.fillRect(QRect(0, height() - 1, width(), 1), c.toolRule);
  p.setRenderHint(QPainter::Antialiasing);
  auto boxed = [&](int item, const QString& text, bool chev, bool on) {
    const QRectF r = QRectF(m_rects[item]).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool open = m_open == item;
    p.setPen(QPen(on ? c.ring : c.btnBorder, 1));
    p.setBrush(on ? theme::mix(c.btnBg, c.ring, 0.10) : c.btnBg);
    p.drawRoundedRect(r, 2, 2);
    p.setFont(ui(12));
    const QColor tc = open ? c.open : (m_hover == item ? c.title : c.btnText);
    p.setPen(tc);
    p.drawText(r.adjusted(8, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
    if (chev) chevron(p, QPointF(r.right() - 11, r.center().y() + 0.5), open, tc);
  };
  boxed(Asset, QCoreApplication::translate("seasonal", "Seasonality: %1").arg(m_asset), true, false);
  // help circle
  {
    const QRectF r = QRectF(m_rects[Help]).adjusted(2, 2, -2, -2);
    const QColor hc = m_hover == Help ? c.title : theme::map("#8a877e");
    p.setPen(QPen(hc, 1)); p.setBrush(Qt::NoBrush); p.drawEllipse(r);
    p.setFont(ui(10, QFont::Bold)); p.drawText(r, Qt::AlignCenter, "?");
  }
  p.fillRect(QRect(m_rects[Help].right() + 10, 10, 1, 20), c.btnBorder);
  p.setFont(ui(16));
  for (int it : {int(Prev), int(Next)}) {
    p.setPen(m_hover == it ? c.title : c.btnText);
    const QRectF r = m_rects[it];
    QPen pen(m_hover == it ? c.title : c.btnText, 1.1); pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const QPointF m = r.center() + QPointF(0, 0.5);
    const qreal d = it == Prev ? 1 : -1;
    p.drawPolyline(QPolygonF{m + QPointF(1.5 * d, -3.5), m + QPointF(-1.5 * d, 0), m + QPointF(1.5 * d, 3.5)});
  }
  boxed(Years, m_years, true, false);
  boxed(Filter, QCoreApplication::translate("seasonal", "Filter"), true, false);
  // segmented
  {
    const QRect g = m_rects[Seg].united(m_rects[Seg + 6]).adjusted(-4, -3, 4, 3);
    p.setPen(QPen(c.btnBorder, 1)); p.setBrush(c.btnBg);
    p.drawRoundedRect(QRectF(g).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    const QStringList seg = QStringList{QCoreApplication::translate("seasonal", "Full"), QCoreApplication::translate("seasonal", "H1"), QCoreApplication::translate("seasonal", "H2"), QCoreApplication::translate("seasonal", "Q1"), QCoreApplication::translate("seasonal", "Q2"), QCoreApplication::translate("seasonal", "Q3"), QCoreApplication::translate("seasonal", "Q4")};
    p.setFont(ui(12));
    for (int i = 0; i < 7; ++i) {
      const QRectF r = QRectF(m_rects[Seg + i]).adjusted(0.5, 0.5, -0.5, -0.5);
      const bool active = m_seg == i, hov = m_hover == Seg + i;
      if (active && !m_segClicked) { p.setPen(Qt::NoPen); p.setBrush(c.segActiveBg); p.drawRoundedRect(r, 2, 2); }
      if ((active && m_segClicked) || hov) {
        p.setPen(QPen(active && m_segClicked ? c.ringBright : c.ring, 1)); p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 4, 4);
      }
      p.setPen(active ? c.segActiveText : c.btnText);
      p.drawText(r, Qt::AlignCenter, seg[i]);
    }
  }
  boxed(Compare, m_compare.isEmpty() ? QCoreApplication::translate("seasonal", "Compare") : QCoreApplication::translate("seasonal", "vs %1").arg(m_compare), true, !m_compare.isEmpty());
  boxed(Actual, m_actualLabel, false, m_actual);
  boxed(Index, QCoreApplication::translate("seasonal", "Index (100)"), false, m_index);
  // refresh icon: circular arrows
  {
    const QRectF r = QRectF(m_rects[Refresh]).adjusted(4, 4, -4, -4);
    QPen pen(m_hover == Refresh ? c.title : theme::map("#8a877e"), 1.2); pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawArc(r, 30 * 16, 150 * 16);
    p.drawArc(r, 210 * 16, 150 * 16);
    const QPointF a1(r.center().x() + r.width() / 2 * std::cos(qDegreesToRadians(30.0)),
                     r.center().y() - r.height() / 2 * std::sin(qDegreesToRadians(30.0)));
    p.drawLine(a1, a1 + QPointF(0, -3)); p.drawLine(a1, a1 + QPointF(-3, 0));
    const QPointF a2(r.center().x() - r.width() / 2 * std::cos(qDegreesToRadians(30.0)),
                     r.center().y() + r.height() / 2 * std::sin(qDegreesToRadians(30.0)));
    p.drawLine(a2, a2 + QPointF(0, 3)); p.drawLine(a2, a2 + QPointF(3, 0));
  }
}
void ToolBar::mouseMoveEvent(QMouseEvent* e) {
  const int h = hit(e->position().toPoint());
  if (h != m_hover) { m_hover = h; setCursor(h == None ? Qt::ArrowCursor : Qt::PointingHandCursor); update(); }
}
void ToolBar::mousePressEvent(QMouseEvent* e) {
  const int h = hit(e->position().toPoint());
  if (h != None && e->button() == Qt::LeftButton) emit clicked(h);
}
void ToolBar::leaveEvent(QEvent*) { m_hover = None; update(); }

// ================================================================ Menu
Menu::Menu(QWidget* parent, bool withSearch) : QWidget(parent) {
  setMouseTracking(true);
  m_font = mono(12);
  if (withSearch) {
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QCoreApplication::translate("seasonal", "Search assets..."));
    m_search->setStyleSheet(QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 2px; "
                                    "padding: 2px 4px; color: %3; font-family: '%4'; font-size: 12px; }")
                                .arg(col().menuBg.name(), col().searchFocus.name(), col().title.name(), ui(13).family()));
    QPixmap pm(QSize(14, 14) * 2); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
    {
      QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(col().sub, 1.2)); p.drawEllipse(QPointF(6, 6), 3.8, 3.8);
      p.drawLine(QPointF(8.8, 8.8), QPointF(12, 12));
    }
    m_search->addAction(QIcon(pm), QLineEdit::LeadingPosition);
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textChanged, this, [this] { filter(); });
  }
  hide();
}
void Menu::setRows(const QList<Row>& rows, int selected) {
  m_all = rows;
  m_selKey = (selected >= 0 && selected < rows.size()) ? rows[selected].key : QString();
  filter();
}
void Menu::filter() {
  const QString q = m_search ? m_search->text().trimmed() : QString();
  m_rows.clear();
  for (const auto& r : m_all)
    if (q.isEmpty() || r.text.contains(q, Qt::CaseInsensitive) || r.key.contains(q, Qt::CaseInsensitive) ||
        r.tag.contains(q, Qt::CaseInsensitive) || r.hint.contains(q, Qt::CaseInsensitive)) m_rows << r;
  m_sel = -1;
  for (int i = 0; i < m_rows.size(); ++i) if (m_rows[i].key == m_selKey) m_sel = i;
  m_hover = q.isEmpty() ? m_sel : (m_rows.isEmpty() ? -1 : 0);
  m_scroll = 0;
  if (m_sel > 0 && q.isEmpty() && m_rows.size() > m_maxRows) m_scroll = qBound(0, m_sel, int(m_rows.size()) - m_maxRows);
  relayout();
}
QRect Menu::listRect() const {
  // search menus: fixed 258 px panel like the reference, the last row cut by the bottom edge (it scrolls)
  if (m_search) return QRect(4, 36, width() - 8, 258 - 36 - 2);
  return QRect(4, 6, width() - 8, int(m_rows.size() * 25.5));
}
void Menu::relayout() {
  int h = listRect().bottom() + 6;
  if (!m_note.isEmpty()) {
    const QFontMetrics fm(ui(10));
    const QRect br = fm.boundingRect(QRect(0, 0, width() - 24, 200), Qt::TextWordWrap, m_note);
    h += br.height() + 10;
  }
  if (m_search) m_search->setGeometry(6, 6, width() - 12, 23);
  resize(width(), h);
  update();
}
void Menu::popup(const QPoint& tl, int w) {
  move(tl); resize(w, height()); relayout();
  if (m_search) { m_search->clear(); m_search->setFocus(); filter(); }
  show(); raise();
}
int Menu::rowAt(const QPoint& p) const {
  const QRect lr = listRect();
  if (!lr.contains(p)) return -1;
  const int i = int((p.y() - lr.top()) / 25.5) + m_scroll;
  return i < m_rows.size() ? i : -1;
}
void Menu::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  p.fillRect(rect(), c.menuBg);
  p.setPen(c.menuBorder); p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
  const QRect lr = listRect();
  const int n = qMin<int>(m_rows.size() - m_scroll, int(std::ceil(lr.height() / 25.5)));
  p.save(); p.setClipRect(lr);
  for (int k = 0; k < n; ++k) {
    const int i = k + m_scroll;
    const QRectF r(lr.left(), lr.top() + k * 25.5, lr.width(), 25.5);
    const bool sel = i == m_sel, hov = i == m_hover;
    if (sel || hov) p.fillRect(r.adjusted(0, 1, 0, -1), sel ? ((m_search || m_strong) ? c.menuSel : c.menuSelSoft) : theme::mix(c.menuBg, c.menuSel, 0.55));
    QFont f = m_font; if (sel && m_search) f.setBold(true);
    p.setFont(f);
    p.setPen(sel ? c.title : c.btnText);
    p.drawText(r.adjusted(8, 0, -40, 0), Qt::AlignLeft | Qt::AlignVCenter, m_rows[i].text);
    if (!m_rows[i].tag.isEmpty()) {
      p.setFont(mono(9)); p.setPen(c.dim);
      p.drawText(r.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter, m_rows[i].tag);
    }
  }
  p.restore();
  if (m_search && m_rows.isEmpty()) {
    p.setFont(ui(12)); p.setPen(c.dim);
    p.drawText(lr.adjusted(8, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop, QCoreApplication::translate("seasonal", "No match"));
  }
  if (m_search && m_rows.size() > m_maxRows) {   // thin scroll indicator
    const double frac = double(m_maxRows) / m_rows.size();
    const double h = lr.height() * frac, y = lr.top() + lr.height() * double(m_scroll) / m_rows.size();
    p.fillRect(QRectF(width() - 4, y, 2, h), theme::map("#3a3528"));
  }
  if (!m_note.isEmpty()) {
    p.setFont(ui(10)); p.setPen(c.dim);
    p.drawText(QRect(12, lr.bottom() + 8, width() - 24, height() - lr.bottom() - 8), Qt::TextWordWrap, m_note);
  }
}
void Menu::mouseMoveEvent(QMouseEvent* e) {
  const int i = rowAt(e->position().toPoint());
  if (i != m_hover && i >= 0) { m_hover = i; update(); }
  setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void Menu::mousePressEvent(QMouseEvent* e) {
  const int i = rowAt(e->position().toPoint());
  if (i >= 0) emit picked(m_rows[i].key);
}
void Menu::wheelEvent(QWheelEvent* e) {
  if (m_rows.size() <= m_maxRows || !m_search) { e->accept(); return; }
  const int d = e->angleDelta().y() != 0 ? (e->angleDelta().y() > 0 ? -1 : 1) : (e->pixelDelta().y() > 0 ? -1 : 1);
  m_scroll = qBound(0, m_scroll + d, int(m_rows.size()) - m_maxRows);
  update(); e->accept();
}
void Menu::leaveEvent(QEvent*) { update(); }
bool Menu::eventFilter(QObject* o, QEvent* e) {
  if (o == m_search && e->type() == QEvent::KeyPress) {
    auto* k = static_cast<QKeyEvent*>(e);
    if (k->key() == Qt::Key_Down || k->key() == Qt::Key_Up) {
      if (m_rows.isEmpty()) return true;
      m_hover = qBound(0, m_hover + (k->key() == Qt::Key_Down ? 1 : -1), int(m_rows.size()) - 1);
      if (m_hover < m_scroll) m_scroll = m_hover;
      if (m_hover >= m_scroll + m_maxRows) m_scroll = m_hover - m_maxRows + 1;
      update(); return true;
    }
    if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && m_hover >= 0 && m_hover < m_rows.size()) {
      emit picked(m_rows[m_hover].key); return true;
    }
    if (k->key() == Qt::Key_Escape) { hide(); emit picked(QString()); return true; }
  }
  return QWidget::eventFilter(o, e);
}

// ================================================================ TrendCard
TrendCard::TrendCard(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }

QRectF TrendCard::plot() const { return QRectF(50, 77, width() - 70, height() - 114); }   // 379 at the reference height 493
double TrendCard::xOf(double slot) const { const QRectF r = plot(); return r.left() + (slot + 0.5) * r.width() / kSlots; }
int TrendCard::slotAt(double x) const {
  const QRectF r = plot();
  return qBound(0, int((x - r.left()) / r.width() * kSlots), kSlots - 1);
}
double TrendCard::yOf(double v) const { const QRectF r = plot(); return r.bottom() - (v - m_lo) / (m_hi - m_lo) * r.height(); }

void TrendCard::setPaths(const QVector<double>& smooth, const QVector<double>& raw, int today) {
  m_smooth = smooth; m_raw = raw; m_today = today; m_status.clear();
  computeRange(); update();
}
void TrendCard::setOverlays(const QVector<double>& cmp, const QString& cn, const QColor& cc, const QVector<double>& act,
                            const QString& an) {
  m_compare = cmp; m_compareName = cn; m_compareColor = cc; m_actual = act; m_actualName = an;
  computeRange(); update();
}
void TrendCard::computeRange() {
  double lo = 1e9, hi = -1e9;
  for (const auto* v : {&m_smooth, &m_compare, &m_actual})
    for (double x : *v) if (std::isfinite(x)) { lo = qMin(lo, x); hi = qMax(hi, x); }
  if (lo > hi) { lo = -1; hi = 1; }
  lo = qMin(lo, 0.0); hi = qMax(hi, 0.0);
  const double span = qMax(hi - lo, 0.2);
  m_lo = lo - span * 0.12; m_hi = hi + span * 0.12;
  // tick step: a nice value giving 4–7 labels, from the span actually plotted (at least 0.2): the raw range can be
  // ~0 (a flat path) and gave a step hundreds of times too fine (or 0, an endless loop in paintEvent)
  const double raw = span / 4.5;
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double r = raw / mag;
  m_step = (r < 1.5 ? 1 : r < 3.5 ? 2.5 : r < 7.5 ? 5 : 10) * mag;
  if (std::abs(m_step / mag - 2.5) < 1e-9 && mag < 0.1) m_step = 2 * mag;
}

QRectF TrendCard::clearRect() const { return m_clear; }

void TrendCard::setHover(const QPoint& p) { m_hoverX = p.x(); m_hoverY = p.y(); m_pinned = true; update(); }

void TrendCard::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  drawCardFrame(p, rect());
  drawCardTitle(p, QPointF(11, 21), m_title);
  p.setFont(ui(11)); p.setPen(c.sub);
  p.drawText(QRectF(11, 33, 900, 16), Qt::AlignLeft | Qt::AlignVCenter, m_sub);

  const QRectF pr = plot();
  // month bands and labels
  p.setFont(mono(9));
  for (int m = 1; m <= 12; ++m) {
    const double x0 = xOf(monthFirstSlot(m) - 0.5), x1 = xOf(monthLastSlot(m) + 0.5);
    if (m % 2 == 1) p.fillRect(QRectF(x0, pr.top(), x1 - x0, pr.height() + 2), c.bandOdd);
    p.fillRect(QRectF(std::floor(x0), pr.top(), 1, pr.height() + 2), theme::map("#17150b"));   // month boundary hairline
    p.setPen(c.sub);
    p.drawText(QRectF(x0, pr.bottom() + 1, x1 - x0, 12), Qt::AlignCenter, monthAxis(m));
  }
  // footnote
  p.setFont(ui(11)); p.setPen(c.dim);
  p.drawText(QRectF(11, height() - 26, width() - 22, 14), Qt::AlignLeft | Qt::AlignVCenter,
             QCoreApplication::translate("seasonal", "Shown as a 3-day Gaussian-weighted moving average of the real cumulative return, for readability."));

  if (!m_status.isEmpty() || m_smooth.isEmpty()) {
    p.setFont(mono(11)); p.setPen(c.sub);
    p.drawText(pr, Qt::AlignCenter, m_status.isEmpty() ? QCoreApplication::translate("seasonal", "No data") : m_status);
    return;
  }
  p.setRenderHint(QPainter::Antialiasing);
  // grid + y labels
  p.setFont(mono(10));
  for (double t = std::ceil(m_lo / m_step) * m_step; t <= m_hi + 1e-9; t += m_step) {
    const double y = yOf(t);
    if (y < pr.top() - 1 || y > pr.bottom() + 1) continue;
    if (std::abs(t) > 1e-9) { p.setPen(QPen(c.grid, 1)); p.drawLine(QPointF(pr.left(), y), QPointF(pr.right(), y)); }
    p.setPen(c.sub);
    const QString lab = m_index ? QString::number(100.0 + t, 'f', 2) : signedPct(t, 2);
    p.drawText(QRectF(0, y - 7, pr.left() - 4, 14), Qt::AlignRight | Qt::AlignVCenter, lab);
  }
  // selection band (current period or live drag)
  Period band = m_period;
  if (m_dragging && m_dragFrom >= 0) band = Period::custom(m_dragFrom, m_dragTo);
  const bool showBand = !band.isFull() || m_dragging;
  if (showBand) {
    const double x0 = xOf(band.a - 0.5), x1 = xOf(band.b + 0.5);
    QLinearGradient g(0, pr.top(), 0, pr.bottom());
    g.setColorAt(0, theme::alpha(theme::map("#1b2d2c"), 200)); g.setColorAt(1, theme::alpha(theme::map("#112524"), 150));
    p.fillRect(QRectF(x0, pr.top(), x1 - x0, pr.height()), g);
    QPen dp(c.selEdge, 1, Qt::DotLine); p.setPen(dp);
    p.drawLine(QPointF(x0, pr.top()), QPointF(x0, pr.bottom()));
    p.drawLine(QPointF(x1, pr.top()), QPointF(x1, pr.bottom()));
  }
  // zero line
  { QPen zp(c.zero, 1, Qt::CustomDashLine); zp.setDashPattern({3, 3}); p.setPen(zp);
    p.drawLine(QPointF(pr.left(), yOf(0)), QPointF(pr.right(), yOf(0))); }
  // TODAY marker
  if (m_today >= 0) {
    const double x = xOf(m_today);
    QPen tp(c.today, 1, Qt::CustomDashLine); tp.setDashPattern({2, 3}); p.setPen(tp);
    p.drawLine(QPointF(x, pr.top() - 2), QPointF(x, pr.bottom()));
    p.setFont(mono(8)); p.setPen(c.todayText);
    p.drawText(QRectF(x - 40, pr.top() - 15, 80, 10), Qt::AlignCenter, QCoreApplication::translate("seasonal", "TODAY"));
  }
  if (m_period.isFull() && !m_dragging) {
    p.setFont(mono(8)); p.setPen(c.hint);
    const QString hint = QCoreApplication::translate("seasonal", "DRAG TO SELECT A PERIOD");
    const double hw = QFontMetricsF(p.font()).horizontalAdvance(hint);
    double right = pr.right() + 6;
    if (m_today >= 0) {   // never under the TODAY label (today late in the year on narrow charts)
      const double tx = xOf(m_today);
      const double half = QFontMetricsF(p.font()).horizontalAdvance(QCoreApplication::translate("seasonal", "TODAY")) / 2 + 6;
      if (right - hw < tx + half && right > tx - half) right = tx - half;
    }
    p.drawText(QRectF(right - hw - 2, pr.top() - 19, hw + 2, 10), Qt::AlignRight | Qt::AlignVCenter, hint);
  }
  // curve path
  QPainterPath line; bool started = false; double minY = pr.bottom();
  for (int s = 0; s < kSlots; ++s) {
    if (!std::isfinite(m_smooth[s])) continue;
    const QPointF pt(xOf(s), yOf(m_smooth[s]));
    minY = qMin(minY, pt.y());
    if (!started) { line.moveTo(pt); started = true; } else line.lineTo(pt);
  }
  if (started) {
    QPainterPath area = line;
    area.lineTo(QPointF(area.currentPosition().x(), pr.bottom()));
    area.lineTo(QPointF(line.elementAt(0).x, pr.bottom()));
    area.closeSubpath();
    QLinearGradient g(0, minY, 0, pr.bottom());
    g.setColorAt(0, theme::alpha(c.fillTop, 165)); g.setColorAt(0.55, theme::alpha(c.fillTop, 70));
    g.setColorAt(1, theme::alpha(c.fillBottom, 30));
    p.fillPath(area, g);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme::alpha(c.cyan, 45), 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(line);
    p.setPen(QPen(c.cyan, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(line);
  }
  // overlays
  auto overlay = [&](const QVector<double>& v, const QColor& color, Qt::PenStyle style) {
    QPainterPath pp; bool st = false;
    for (int s = 0; s < v.size(); ++s) {
      if (!std::isfinite(v[s])) continue;
      const QPointF pt(xOf(s), yOf(v[s]));
      if (!st) { pp.moveTo(pt); st = true; } else pp.lineTo(pt);
    }
    p.setPen(QPen(color, 1.3, style, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(pp);
  };
  int legendX = int(pr.left()) + 8;
  auto legend = [&](const QString& name, const QColor& color) {
    p.setFont(mono(9, QFont::Bold, 1)); const QFontMetrics fm(p.font());
    const QRect r(legendX, int(pr.bottom()) - 18, fm.horizontalAdvance(name) + 22, 14);
    p.setPen(QPen(color, 1.5)); p.drawLine(r.left(), r.center().y(), r.left() + 12, r.center().y());
    p.setPen(color); p.drawText(r.adjusted(16, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, name);
    legendX = r.right() + 12;
  };
  if (!m_compare.isEmpty()) { overlay(m_compare, m_compareColor, Qt::SolidLine); }
  if (!m_actual.isEmpty()) { overlay(m_actual, c.actual, Qt::DashLine); }
  if (!m_compare.isEmpty() || !m_actual.isEmpty()) {
    legend(QCoreApplication::translate("seasonal", "SEASONAL AVG"), c.cyan);
    if (!m_compare.isEmpty()) legend(m_compareName.toUpper(), m_compareColor);
    if (!m_actual.isEmpty()) legend(m_actualName.toUpper(), c.actual);
  }
  // badge for the selected period
  m_clear = QRectF(); m_badge = QRectF();
  if (!band.isFull() || m_dragging) {
    const double ch = pathChange(m_raw, band.a, band.b);
    const QString text = QCoreApplication::translate("seasonal", "%1: %2").arg(band.rangeLabel(), signedPct(ch, 2));
    const bool neg = std::isfinite(ch) && ch < 0;
    p.setFont(mono(11)); const QFontMetricsF fm(p.font());
    const double bw = fm.horizontalAdvance(text) + 14, cw = m_dragging ? 0 : 44;
    const double cx = (xOf(band.a - 0.5) + xOf(band.b + 0.5)) / 2;
    double left = qBound(pr.left() + 2, cx - (bw + 6 + cw) / 2, pr.right() - bw - cw - 8);
    const QRectF br(left, pr.top() + 4, bw, 18);
    p.setPen(QPen(neg ? c.badgeRedBorder : c.badgeBorder, 1)); p.setBrush(neg ? c.badgeRedBg : c.badgeBg);
    p.drawRect(br.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setPen(neg ? c.valDown : c.cyan); p.drawText(br, Qt::AlignCenter, text);
    m_badge = br.united(QRectF(br.right(), br.top(), cw + 6, br.height()));
    if (!m_dragging) {
      m_clear = QRectF(br.right() + 6, br.top(), cw, 18);
      p.setPen(QPen(theme::map("#2e2a20"), 1)); p.setBrush(theme::map("#0b0803"));
      p.drawRect(m_clear.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setFont(ui(11, QFont::DemiBold)); p.setPen(c.clearText);
      p.drawText(m_clear, Qt::AlignCenter, QCoreApplication::translate("seasonal", "× Clear"));
    }
  }
  // extremes
  int smax = -1, smin = -1;
  for (int s = 0; s < kSlots; ++s) {
    if (!std::isfinite(m_smooth[s])) continue;
    if (smax < 0 || m_smooth[s] > m_smooth[smax]) smax = s;
    if (smin < 0 || m_smooth[s] < m_smooth[smin]) smin = s;
  }
  auto valueText = [&](double v) { return m_index ? QString::number(100.0 + v, 'f', 2) : signedPct(v, 2); };
  if (smax >= 0) {
    p.setFont(mono(10));
    const QPointF a(xOf(smax), yOf(m_smooth[smax])), b(xOf(smin), yOf(m_smooth[smin]));
    p.setPen(Qt::NoPen); p.setBrush(c.cyan); p.drawEllipse(a, 1.8, 1.8);
    p.setBrush(theme::map("#c8c6c0")); p.drawEllipse(b, 1.8, 1.8);
    QRectF ml(a.x() - 40, a.y() - 19, 80, 12);
    Qt::Alignment al = Qt::AlignCenter;
    if (!m_badge.isNull() && ml.intersects(m_badge.adjusted(-4, -2, 4, 2))) {   // keep clear of the badge: beside the peak
      ml = QRectF(a.x() + 7, a.y() - 6, 80, 12); al = Qt::AlignLeft | Qt::AlignVCenter;
    }
    p.setPen(c.cyan); p.drawText(ml, al, valueText(m_smooth[smax]));
    p.setPen(theme::map("#c8c6c0")); p.drawText(QRectF(b.x() - 40, b.y() + 5, 80, 12), Qt::AlignCenter, valueText(m_smooth[smin]));
  }
  // crosshair
  if (m_hoverX >= pr.left() && m_hoverX <= pr.right() && m_hoverY >= pr.top() - 20 && m_hoverY <= pr.bottom() && !m_dragging) {
    const int s = slotAt(m_hoverX);
    if (std::isfinite(m_smooth[s])) {
      const double x = xOf(s), y = yOf(m_smooth[s]);
      p.setPen(QPen(c.crosshair, 1)); p.drawLine(QPointF(x, pr.top()), QPointF(x, pr.bottom()));
      p.setPen(Qt::NoPen); p.setBrush(theme::map("#c8c6c0")); p.drawEllipse(QPointF(pr.left(), y), 2, 2);
      p.setBrush(c.cyan); p.drawEllipse(QPointF(x, y), 4, 4);
      p.setBrush(theme::map(255, 255, 255)); p.drawEllipse(QPointF(x, y), 1.6, 1.6);
      QStringList extra;
      if (!m_compare.isEmpty() && std::isfinite(m_compare.value(s, NAN))) extra << m_compareName + " " + valueText(m_compare[s]);
      if (!m_actual.isEmpty() && std::isfinite(m_actual.value(s, NAN))) extra << m_actualName + " " + valueText(m_actual[s]);
      const QString date = slotLabel(s), val = valueText(m_smooth[s]);
      const QFontMetricsF f1(ui(11)), f2(mono(11, QFont::Bold)), f3(mono(10));
      double w = qMax(f1.horizontalAdvance(date), f2.horizontalAdvance(val));
      for (const auto& e : extra) w = qMax(w, f3.horizontalAdvance(e));
      w += 16;
      const double h = 38 + extra.size() * 14;
      // reference: box centred on the line just above the curve point; flipped left of the line near the right edge
      double tx = x - w / 2; if (tx + w > pr.right()) tx = x - 6 - w; if (tx < pr.left()) tx = x + 6;
      double ty = y - 14 - h; if (ty < pr.top()) ty = y + 14;
      const QRectF tr(tx, ty, w, h);
      p.setPen(QPen(c.tipBorder, 1)); p.setBrush(theme::alpha(c.tipBg, 240)); p.drawRect(tr.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setFont(ui(11)); p.setPen(c.tipDate); p.drawText(QRectF(tx + 8, ty + 4, w, 14), Qt::AlignLeft | Qt::AlignVCenter, date);
      p.setFont(mono(11, QFont::Bold)); p.setPen(c.cyan);
      p.drawText(QRectF(tx + 8, ty + 19, w, 14), Qt::AlignLeft | Qt::AlignVCenter, val);
      p.setFont(mono(10));
      for (int i = 0; i < extra.size(); ++i) {
        p.setPen(i == 0 && !m_compare.isEmpty() ? m_compareColor : c.actual);
        p.drawText(QRectF(tx + 8, ty + 34 + i * 14, w, 14), Qt::AlignLeft | Qt::AlignVCenter, extra[i]);
      }
    }
  }
}
void TrendCard::mouseMoveEvent(QMouseEvent* e) {
  m_pinned = false;
  m_hoverX = int(e->position().x()); m_hoverY = int(e->position().y());
  if (m_dragging) m_dragTo = slotAt(e->position().x());
  setCursor(m_clear.contains(e->position()) ? Qt::PointingHandCursor : plot().contains(e->position()) ? Qt::CrossCursor : Qt::ArrowCursor);
  update();
}
void TrendCard::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  if (m_clear.contains(e->position())) { emit cleared(); return; }
  if (!m_status.isEmpty() || !plot().contains(e->position())) return;
  m_dragging = true; m_dragFrom = m_dragTo = slotAt(e->position().x());
  update();
}
void TrendCard::mouseReleaseEvent(QMouseEvent* e) {
  if (!m_dragging) return;
  m_dragging = false;
  m_dragTo = slotAt(e->position().x());
  if (std::abs(m_dragTo - m_dragFrom) >= 2) emit periodDragged(Period::custom(m_dragFrom, m_dragTo));
  m_dragFrom = m_dragTo = -1;
  update();
}
void TrendCard::leaveEvent(QEvent*) { if (m_pinned) return; m_hoverX = m_hoverY = -1; update(); }

// ================================================================ WinCard
WinCard::WinCard(QWidget* parent) : QWidget(parent) {}
void WinCard::setStats(const Stats& s, int years) { m_s = s; m_years = years; m_has = true; m_status.clear(); update(); }

// Point on the top ellipse at Qt angle `deg` (counter-clockwise from 3 o'clock).
static QPointF onEllipse(const QPointF& c, double rx, double ry, double deg) {
  const double a = qDegreesToRadians(deg);
  return QPointF(c.x() + rx * std::cos(a), c.y() - ry * std::sin(a));
}

void WinCard::paintPie(QPainter& p, const QPointF& c, double share) {
  const double rx = 140, ry = 84, depth = 22;
  const QRectF top(c.x() - rx, c.y() - ry, 2 * rx, 2 * ry);
  const auto& k = col();
  p.setRenderHint(QPainter::Antialiasing);
  // soft drop shadow
  {
    QRadialGradient g(c + QPointF(0, depth + 6), rx * 1.15);
    const int sa = theme::isDark() ? 1 : 3;   // softer shadow on light themes
    g.setColorAt(0, QColor(0, 0, 0, 150 / sa)); g.setColorAt(0.7, QColor(0, 0, 0, 60 / sa)); g.setColorAt(1, QColor(0, 0, 0, 0));
    p.save(); p.translate(c + QPointF(0, depth + 6)); p.scale(1.0, ry / rx); p.translate(-(c + QPointF(0, depth + 6)));
    p.setPen(Qt::NoPen); p.setBrush(g); p.drawEllipse(c + QPointF(0, depth + 6), rx * 1.15, rx * 1.15);
    p.restore();
  }
  // slices as Qt angle ranges: cyan from 90° clockwise by share*360, red the rest
  struct Slice { double start, sweep; QColor topC, sideC; };
  const double cs = share * 360.0;
  QList<Slice> slices;
  // walls: a darker shade of the top face (measured on Deepwater; other themes derive it from the mapped top, the
  // role mapping would otherwise make the dark wall pale on light themes)
  const QColor cyanSide = theme::isDefault() ? k.pieCyanSide : k.pieCyanTop.darker(230);
  const QColor redSide = theme::isDefault() ? k.pieRedSide : k.pieRedTop.darker(230);
  if (cs > 0) slices << Slice{90.0 - cs, cs, k.pieCyanTop, cyanSide};          // [90-cs, 90]
  if (cs < 360) slices << Slice{90.0 - 360.0, 360.0 - cs, k.pieRedTop, redSide};  // [-270, 90-cs]
  // side walls: the front half is Qt angles 180..360 (mod 360)
  for (const auto& sl : slices) {
    for (int wrap = -1; wrap <= 1; ++wrap) {
      const double a0 = qMax(sl.start + wrap * 360.0, 180.0), a1 = qMin(sl.start + sl.sweep + wrap * 360.0, 360.0);
      if (a1 <= a0) continue;
      QPainterPath w;
      w.moveTo(onEllipse(c, rx, ry, a0));
      for (double a = a0; a <= a1 + 1e-6; a += 2) w.lineTo(onEllipse(c, rx, ry, qMin(a, a1)));
      w.lineTo(onEllipse(c, rx, ry, a1) + QPointF(0, depth));
      for (double a = a1; a >= a0 - 1e-6; a -= 2) w.lineTo(onEllipse(c, rx, ry, qMax(a, a0)) + QPointF(0, depth));
      w.closeSubpath();
      QLinearGradient g(c.x() - rx, 0, c.x() + rx, 0);
      g.setColorAt(0, sl.sideC.darker(135)); g.setColorAt(0.45, sl.sideC.lighter(118)); g.setColorAt(1, sl.sideC.darker(140));
      p.setPen(Qt::NoPen); p.setBrush(g); p.drawPath(w);
    }
  }
  // dark seam on the wall where the slices meet in front
  for (double bd : {90.0 - cs, 90.0}) {
    double a = bd; while (a < 0) a += 360; while (a >= 360) a -= 360;
    if (cs <= 0 || cs >= 360) break;
    if (a > 180 && a < 360) {
      const QPointF t = onEllipse(c, rx, ry, a);
      p.setPen(QPen(theme::map(0, 0, 0, 200), 2.2)); p.drawLine(t, t + QPointF(0, depth));
    }
  }
  // top faces
  for (const auto& sl : slices) {
    QPainterPath t; t.moveTo(c); t.arcTo(top, sl.start, sl.sweep); t.closeSubpath();
    // measured: red #fc6d6d top-left → #e85a5a lower right; cyan #1ddff3 → #15b4c8 (no lighter(): it desaturates)
    QLinearGradient g(top.topLeft(), top.bottomRight());
    const bool red = sl.topC == k.pieRedTop;
    g.setColorAt(0, red ? theme::map("#ff7171") : theme::map("#1fe2f6")); g.setColorAt(1, red ? theme::map("#e45757") : theme::map("#14b0c4"));
    p.setPen(Qt::NoPen); p.setBrush(g); p.drawPath(t);
  }
  // seams on the top
  if (cs > 0 && cs < 360) {
    p.setPen(QPen(theme::map(0, 0, 0, 220), 2.4, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(c, onEllipse(c, rx, ry, 90.0)); p.drawLine(c, onEllipse(c, rx, ry, 90.0 - cs));
  }
  // labels at slice centroids
  auto label = [&](double midDeg, double pct, const QString& word, int years) {
    const QPointF at = onEllipse(c, rx * 0.55, ry * 0.42, midDeg) + QPointF(0, 2);
    p.setPen(QColor(255, 255, 255));   // on the saturated slice: white in every theme
    // reference: percentage in the mono face (slashed zero) over a bold condensed word and the year count
    p.setFont(mono(28, QFont::ExtraBold));
    p.drawText(QRectF(at.x() - 80, at.y() - 32, 160, 32), Qt::AlignHCenter | Qt::AlignBottom, QString::number(pct, 'f', 1) + "%");
    p.setFont(ui(15, QFont::Bold)); QFont f = p.font(); f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2); p.setFont(f);
    p.drawText(QRectF(at.x() - 80, at.y() - 2, 160, 17), Qt::AlignCenter, word);
    p.setFont(ui(14, QFont::DemiBold)); p.setPen(QColor(255, 255, 255, 215));   // on the slice
    p.drawText(QRectF(at.x() - 80, at.y() + 16, 160, 17), Qt::AlignCenter, QCoreApplication::translate("seasonal", "%n year(s)", nullptr, years));
  };
  const int pos = m_s.positive, neg = m_s.negative;
  if (cs > 0) label(90.0 - cs / 2, share * 100.0, QCoreApplication::translate("seasonal", "POSITIVE"), pos);
  if (cs < 360) label(90.0 - cs - (360.0 - cs) / 2, (1.0 - share) * 100.0, QCoreApplication::translate("seasonal", "NEGATIVE"), neg);
}

void WinCard::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  drawCardFrame(p, rect());
  const bool full = m_s.period.isFull();
  // Responsive: ≥ 760 px wide (stacked under the trend chart on narrow windows) → pie on the left 430 px, the
  // figures on the right; taller than the reference 493 px (wide windows) → content centred vertically.
  const bool wide = width() >= 760;
  const double L = wide ? 430.0 : width();
  const double W2 = wide ? width() - 440.0 : width();
  {
    const QString t = QCoreApplication::translate("seasonal", "SEASONAL WIN RATE");
    p.setFont(mono(14, QFont::Bold, 2.2));
    const double tw = QFontMetricsF(p.font()).horizontalAdvance(t);
    const QString pill = m_has && !full ? m_s.period.pill() : QString();
    const double pw = pill.isEmpty() ? 0 : QFontMetricsF(ui(10, QFont::DemiBold)).horizontalAdvance(pill) + 16;
    const double x = (L - tw - pw) / 2;
    drawCardTitle(p, QPointF(x, 21), t);
    if (!pill.isEmpty()) {   // reference: small condensed pill next to this title (the ADVANCED cards use the mono one)
      const QFont pf = ui(10, QFont::DemiBold);
      const double w = QFontMetricsF(pf).horizontalAdvance(pill) + 10;
      const QRectF r(x + tw + 6, 14, w, 14);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(Qt::NoPen); p.setBrush(c.pillBg); p.drawRoundedRect(r, 2, 2);
      p.setFont(pf); p.setPen(c.pillText); p.drawText(r, Qt::AlignCenter, pill);
      p.setRenderHint(QPainter::Antialiasing, false);
    }
  }
  p.save();
  if (!wide) p.translate(0, qMax(0, (height() - 493) / 2));
  if (!m_has || m_s.yearsCounted == 0) {
    p.setFont(mono(11)); p.setPen(c.sub);
    p.drawText(QRectF(0, 60, L, 240), Qt::AlignCenter,
               m_status.isEmpty() ? QCoreApplication::translate("seasonal", "No complete year for this period yet") : m_status);
    if (!m_has) return;
  } else {
    paintPie(p, QPointF(L / 2.0 - 0.5, 156), m_s.winRate);
  }
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(ui(12)); p.setPen(c.caption);
  const QString cap = full ? QCoreApplication::translate("seasonal", "Years with a positive annual return %1 %2 years").arg(QString::fromUtf8("–")).arg(m_s.yearsCounted)
                           : QCoreApplication::translate("seasonal", "Years where the selected period was positive %1 %2 years").arg(QString::fromUtf8("–")).arg(m_s.yearsCounted);
  p.drawText(QRectF(0, 292, L, 16), Qt::AlignCenter, cap);
  if (wide) p.translate(440, (height() - 157) / 2.0 - 322 + 8);
  p.fillRect(QRectF(11, 322, W2 - 22, 1), c.rowRule);
  struct R { QString label, value; QColor color; };
  auto vc = [&](double v) { return !std::isfinite(v) ? c.sub : v < 0 ? c.valDown : c.valUp; };
  const QList<R> rows{
    {full ? QCoreApplication::translate("seasonal", "Annual return") : QCoreApplication::translate("seasonal", "Period return"), signedPct(m_s.pathReturn), vc(m_s.pathReturn)},
    {QCoreApplication::translate("seasonal", "Best month"), m_s.best.month ? monthLong(m_s.best.month) + " " + signedPct(m_s.best.mean) : QString::fromUtf8("—"), vc(m_s.best.mean)},
    {QCoreApplication::translate("seasonal", "Worst month"), m_s.worst.month ? monthLong(m_s.worst.month) + " " + signedPct(m_s.worst.mean) : QString::fromUtf8("—"), vc(m_s.worst.mean)},
  };
  for (int i = 0; i < rows.size(); ++i) {
    const double y = 327 + i * 20.5;
    p.setFont(ui(12)); p.setPen(c.label);
    p.drawText(QRectF(11, y, 200, 16), Qt::AlignLeft | Qt::AlignVCenter, rows[i].label);
    p.setFont(mono(11, QFont::DemiBold)); p.setPen(rows[i].color);
    p.drawText(QRectF(W2 - 311, y, 300, 16), Qt::AlignRight | Qt::AlignVCenter, rows[i].value);
  }
  // volatility ratio sub-card
  const QRectF v(12, 395, W2 - 23, 84);
  const int st = volState(m_s.volRatio);
  const QColor acc = st < 0 ? c.volCyan : st > 0 ? c.volRed : c.volYellow;
  const QColor pillBg = st < 0 ? c.volCyanPillBg : st > 0 ? c.volRedPillBg : c.volYellowPillBg;
  const QColor pillFg = st < 0 ? c.volCyanPill : st > 0 ? c.volRedPill : c.volYellowPill;
  p.fillRect(v, c.volBg);
  p.setBrush(Qt::NoBrush); p.setPen(c.volBorder); p.drawRect(v.adjusted(0.5, 0.5, -0.5, -0.5));
  p.fillRect(QRectF(v.left(), v.top(), 2, v.height()), acc);
  p.setFont(mono(10, QFont::Bold, 2.2)); p.setPen(c.title);
  p.drawText(QRectF(v.left() + 10, v.top() + 7, 200, 14), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("seasonal", "VOLATILITY RATIO"));
  {
    const QString pill = m_s.period.pill();
    const QFont pf = ui(10, QFont::DemiBold);
    const double pw = QFontMetricsF(pf).horizontalAdvance(pill) + 10;
    const QRectF pr(v.right() - 9 - pw, v.top() + 7, pw, 14);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(pillBg); p.drawRoundedRect(pr, 2, 2);
    p.setFont(pf); p.setPen(pillFg); p.drawText(pr, Qt::AlignCenter, pill);
    p.setRenderHint(QPainter::Antialiasing, false);
  }
  p.setFont(mono(26, QFont::Bold)); p.setPen(acc);
  const QString ratio = std::isfinite(m_s.volRatio) ? QString::number(m_s.volRatio, 'f', 2) + QString::fromUtf8("×") : QString::fromUtf8("—");
  p.drawText(QRectF(v.left() + 9, v.top() + 22, 200, 34), Qt::AlignLeft | Qt::AlignVCenter, ratio);
  QString state;
  if (std::isfinite(m_s.volRatio)) {
    const int pct = qRound(std::abs(m_s.volRatio - 1.0) * 100);
    state = st == 0 ? QCoreApplication::translate("seasonal", "NORMAL VOLATILITY") : st < 0 ? QCoreApplication::translate("seasonal", "%1% LESS VOLATILE THAN NORMAL").arg(pct)
                                                           : QCoreApplication::translate("seasonal", "%1% MORE VOLATILE THAN NORMAL").arg(pct);
  }
  p.setFont(mono(8, QFont::Normal, 0.8)); p.setPen(theme::map("#8e8b83"));
  p.drawText(QRectF(v.left() + 150, v.top() + 22, v.width() - 159, 34), Qt::AlignRight | Qt::AlignVCenter, state);
  // σ line: "σ Jan-Jun 0.63% ÷ σ global 0.70%"
  {
    const QFont f = mono(11), fb = mono(11, QFont::Bold);
    double x = v.left() + 10; const double y = v.top() + 58;
    auto put = [&](const QString& s, const QFont& font, const QColor& color) {
      p.setFont(font); p.setPen(color);
      p.drawText(QRectF(x, y, 400, 16), Qt::AlignLeft | Qt::AlignVCenter, s);
      x += QFontMetricsF(font).horizontalAdvance(s);
    };
    auto sig = [](double s) { return std::isfinite(s) ? QString::number(s, 'f', 2) + "%" : QString::fromUtf8("—"); };
    put(QString::fromUtf8("σ ") + m_s.period.pill() + " ", f, c.btnText);
    put(sig(m_s.sigmaPeriod), fb, c.title);
    put(QString::fromUtf8(" ÷ "), f, c.dim);
    put(QString::fromUtf8("σ global "), f, c.btnText);
    put(sig(m_s.sigmaGlobal), fb, c.title);
  }
}

// ================================================================ BarCard
BarCard::BarCard(const QString& title, QWidget* parent) : QWidget(parent), m_title(title) { setMouseTracking(true); }
void BarCard::setBars(const QStringList& labels, const QVector<double>& values, const QVector<bool>& dim, const QStringList& tips) {
  m_labels = labels; m_values = values; m_dim = dim; m_tips = tips; m_status.clear(); update();
}
QRectF BarCard::plot() const { return QRectF(11, 44, width() - 22, 232); }

void BarCard::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  drawCardFrame(p, rect());
  drawCardTitle(p, QPointF(11, 21), m_title);
  if (!m_pill.isEmpty()) drawPeriodPill(p, width() - 12, 14, m_pill);
  const QRectF pr = plot();
  if (!m_status.isEmpty() || m_values.isEmpty()) {
    p.setFont(mono(11)); p.setPen(c.sub);
    p.drawText(pr, Qt::AlignCenter, m_status.isEmpty() ? QCoreApplication::translate("seasonal", "No data") : m_status);
    return;
  }
  double mx = 0;
  for (double v : m_values) if (std::isfinite(v)) mx = qMax(mx, std::abs(v));
  if (mx <= 0) mx = 1;
  const double zero = pr.top() + 116, half = 106;
  // dashed grid at ± 57.5 % of the range, zero line
  p.setPen(QPen(theme::map("#222111"), 1));   // measured: solid faint olive lines at ± 57.5 % of the range
  for (double k : {-0.575, 0.575}) p.drawLine(QPointF(pr.left(), zero - k * half), QPointF(pr.right(), zero - k * half));
  const int n = m_values.size();
  const double cw = pr.width() / n;
  p.setFont(mono(10));
  for (int i = 0; i < n; ++i) {
    const double v = m_values[i];
    const double x0 = pr.left() + i * cw + m_gap / 2, w = cw - m_gap;
    if (std::isfinite(v)) {
      const double h = std::abs(v) / mx * half;
      QColor bc = v >= 0 ? c.up : c.down;
      if (m_dim.value(i, false)) bc = theme::alpha(bc, 90);
      if (i == m_hover) bc = bc.lighter(115);
      p.fillRect(QRectF(x0, v >= 0 ? zero - h : zero, w, qMax(h, 0.8)), bc);
    }
    p.setPen(m_dim.value(i, false) ? c.dim : c.barLabel);
    p.drawText(QRectF(pr.left() + i * cw, height() - 30, cw, 14), Qt::AlignCenter, m_labels.value(i));
  }
  p.setPen(QPen(c.barZero, 1)); p.drawLine(QPointF(pr.left(), zero), QPointF(pr.right(), zero));
  if (!m_note.isEmpty()) {
    p.setFont(ui(10)); p.setPen(c.dim);
    p.drawText(QRectF(pr.left(), 30, pr.width() - 90, 12), Qt::AlignRight | Qt::AlignVCenter, m_note);
  }
  // hover value tip
  if (m_hover >= 0 && m_hover < n) {
    const QString tip = m_tips.value(m_hover, m_labels.value(m_hover) + "  " + signedPct(m_values[m_hover], 3));
    p.setFont(mono(10)); const QFontMetricsF fm(p.font());
    const double w = fm.horizontalAdvance(tip) + 14;
    const double x = qBound(pr.left(), pr.left() + (m_hover + 0.5) * cw - w / 2, pr.right() - w);
    const QRectF r(x, pr.top() - 4, w, 18);
    p.setPen(QPen(c.tipBorder, 1)); p.setBrush(c.tipBg); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setPen(c.title); p.drawText(r, Qt::AlignCenter, tip);
  }
}
void BarCard::mouseMoveEvent(QMouseEvent* e) {
  const QRectF pr = plot();
  int h = -1;
  if (!m_values.isEmpty() && e->position().y() > pr.top() && e->position().x() >= pr.left() && e->position().x() < pr.right())
    h = int((e->position().x() - pr.left()) / (pr.width() / m_values.size()));
  if (h != m_hover) { m_hover = h; update(); }
}
void BarCard::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ================================================================ RiskCard
RiskCard::RiskCard(QWidget* parent) : QWidget(parent) {}
void RiskCard::setStats(const Stats& s) { m_s = s; m_has = true; m_status.clear(); update(); }

void RiskCard::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = col();
  drawCardFrame(p, rect());
  drawCardTitle(p, QPointF(11, 21), QCoreApplication::translate("seasonal", "RISK METRICS"));
  if (m_has && !m_s.period.isFull()) drawPeriodPill(p, width() - 12, 14, m_s.period.pill());
  if (!m_has) {
    p.setFont(mono(11)); p.setPen(c.sub);
    p.drawText(QRectF(0, 40, width(), 230), Qt::AlignCenter, m_status);
    return;
  }
  struct T { QString label, value; int tone; };   // tone: 1 up, -1 down, 0 neutral
  auto tone = [](double v) { return !std::isfinite(v) ? 0 : v < 0 ? -1 : 1; };
  auto num = [](double v, int d) { return std::isfinite(v) ? QString::number(v, 'f', d) : QString::fromUtf8("—"); };
  const QList<T> tiles{
    {QCoreApplication::translate("seasonal", "CUMULATIVE PROFIT"), signedPct(m_s.compounded), tone(m_s.compounded)},
    {QCoreApplication::translate("seasonal", "AVG PERIOD RETURN"), signedPct(m_s.meanReturn), tone(m_s.meanReturn)},
    {QCoreApplication::translate("seasonal", "VOLATILITY"), std::isfinite(m_s.stdReturn) ? QString::number(m_s.stdReturn, 'f', 2) + "%" : QString::fromUtf8("—"), 0},
    {QCoreApplication::translate("seasonal", "MAX DRAWDOWN"), signedPct(m_s.maxDrawdown), std::isfinite(m_s.maxDrawdown) && m_s.maxDrawdown < 0 ? -1 : 0},
    {QCoreApplication::translate("seasonal", "RETURN / RISK"), num(m_s.returnRisk, 2), tone(m_s.returnRisk)},
    {QCoreApplication::translate("seasonal", "BEST PERIOD (%1)").arg(m_s.bestYear.year ? QString::number(m_s.bestYear.year) : QString::fromUtf8("—")),
     signedPct(m_s.bestYear.ret), tone(m_s.bestYear.ret)},
    {QCoreApplication::translate("seasonal", "WORST PERIOD (%1)").arg(m_s.worstYear.year ? QString::number(m_s.worstYear.year) : QString::fromUtf8("—")),
     signedPct(m_s.worstYear.ret), tone(m_s.worstYear.ret)},
  };
  const double tw = (width() - 22 - 8) / 2.0;
  for (int i = 0; i < tiles.size(); ++i) {
    const QRectF r(11 + (i % 2) * (tw + 8), 37 + (i / 2) * 57, tw, 50);
    p.fillRect(r, c.tileBg);
    p.setBrush(Qt::NoBrush); p.setPen(c.tileBorder); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setFont(ui(9, QFont::DemiBold)); QFont lf = p.font(); lf.setLetterSpacing(QFont::AbsoluteSpacing, 0.3); p.setFont(lf);
    p.setPen(c.tileLabel);
    p.drawText(QRectF(r.left() + 8, r.top() + 6, r.width() - 16, 14), Qt::AlignLeft | Qt::AlignVCenter, tiles[i].label);
    p.setFont(mono(15)); p.setPen(tiles[i].tone > 0 ? c.tileUp : tiles[i].tone < 0 ? c.tileDown : c.tileNeutral);
    p.drawText(QRectF(r.left() + 8, r.top() + 22, r.width() - 16, 22), Qt::AlignLeft | Qt::AlignVCenter, tiles[i].value);
  }
  p.setFont(ui(10)); p.setPen(c.dim);
  const QString per = m_s.period.isFull() ? QCoreApplication::translate("seasonal", "full-year") : m_s.period.pill();
  const QString foot = QCoreApplication::translate("seasonal", "Volatility = std. dev. of real %1 period returns. Max drawdown = largest peak-to-trough drop on the real "
                               "average seasonal path, restricted to this period. Cumulative profit = the %1 return compounded over "
                               "every complete year. Return/Risk is a plain ratio, not a risk-free-rate-adjusted Sharpe ratio.").arg(per);
  p.drawText(QRectF(11, 264, width() - 22, 40), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, foot);
}
}
