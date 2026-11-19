#include "pages/market/technical/HeatTable.h"
#include "pages/market/technical/MarketKit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QCoreApplication>
#include <cmath>

namespace {
constexpr double kPitch = 63.5, kCellH = 58, kBodyTop = 35, kHeaderH = 26, kGap = 6;
QFont nameFont() { return theme::mono(14, QFont::Bold); }
QFont priceFont() { return theme::mono(10); }
// tight = narrow tables (1024-1280 px windows): letter spacing drops before any column is dropped
QFont wordFont(bool tight = false) { return theme::mono(11, QFont::Bold, tight ? 0.3 : 1.5); }
QFont detailFont(bool tight = false) { return theme::mono(10, QFont::Normal, tight ? 0.0 : 0.6); }
QFont headFont(bool bold, bool tight = false) { return theme::mono(10, bold ? QFont::Bold : QFont::Normal, tight ? 0.5 : 3.0); }
QColor kSelBorder() { return theme::map("#b49a3c"); }
}

HeatTable::HeatTable(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  m_anim.setInterval(33);
  connect(&m_anim, &QTimer::timeout, this, [this] {
    const int first = int(m_scroll / kPitch);
    const int vis = int(viewportHeight() / kPitch) + 2;
    if (m_fade.elapsed() > vis * 12 + 130) { m_fading = false; m_anim.stop(); }
    update(QRect(0, int(kBodyTop), width(), height()));
    Q_UNUSED(first);
  });
}

namespace {
bool samePaint(const HeatTable::Row& a, const HeatTable::Row& b) {
  if (a.symbol != b.symbol || a.name != b.name || a.price != b.price || a.cells.size() != b.cells.size()) return false;
  for (int c = 0; c < a.cells.size(); ++c) {
    const auto &x = a.cells[c], &y = b.cells[c];
    if (x.word != y.word || x.detail != y.detail || x.fill != y.fill || x.text != y.text) return false;
  }
  return true;
}
}

void HeatTable::setRows(const QVector<Row>& rows, bool filterChanged) {
  const double oldScroll = m_scroll, oldContent = contentHeight();
  const QVector<Row> old = std::move(m_rows);
  const QVector<double> oldX = m_x, oldW = m_w;
  m_rows = rows;
  for (auto& r : m_rows)
    if (r.cells.isEmpty()) for (int c = 0; c < heat::ColumnCount; ++c) r.cells << heat::cell(r.read, heat::Column(c));
  if (filterChanged) {
    m_scroll = 0;
    m_fading = isVisible(); m_fade.start();
    if (m_fading) m_anim.start();
  }
  layoutColumns(); clampScroll();
  // A scan rebuilds the table every 120 ms while rows arrive, mostly out of view: when the rows on screen, the
  // columns and the scroll are unchanged and the legend is out of view, only the scroll bar moved.
  const double vh = viewportHeight();
  bool onlyBar = !filterChanged && !m_fading && m_scroll == oldScroll && m_x == oldX && m_w == oldW && !old.isEmpty() && !m_rows.isEmpty() &&
                 oldContent - m_scroll > vh + 70 && contentHeight() - m_scroll > vh + 70;
  if (onlyBar) {
    const int first = std::max(0, int(m_scroll / kPitch));
    const int last = int((m_scroll + vh) / kPitch) + 1;
    for (int i = first; i <= last && onlyBar; ++i) {
      const bool inOld = i < old.size(), inNew = i < m_rows.size();
      if (inOld != inNew || (inNew && !samePaint(old[i], m_rows[i]))) onlyBar = false;
    }
  }
  if (onlyBar) update(QRect(width() - 4, int(kBodyTop), 4, height() - int(kBodyTop)));
  else update();
}
void HeatTable::setSelected(const QString& s) { if (s == m_selected) return; m_selected = s; update(); }
void HeatTable::setPrice(const QString& sym, const QString& price) {
  for (int i = 0; i < m_rows.size(); ++i) {
    if (m_rows[i].symbol != sym) continue;
    if (m_rows[i].price == price) return;
    m_rows[i].price = price;
    const double y = kBodyTop + i * kPitch - m_scroll;
    if (y + kCellH >= kBodyTop && y <= height()) update(QRect(int(m_x[0]), int(y), int(m_w[0]) + 1, int(kCellH) + 1));
    return;
  }
}
void HeatTable::setSort(int c, bool d) { m_sortCol = c; m_desc = d; update(); }
void HeatTable::setLoadingText(const QString& t) {
  if (t == m_loading) return;
  m_loading = t;
  if (m_rows.isEmpty()) update(QRect(0, int(kBodyTop), width(), height()));   // the text shows only with no rows
}

void HeatTable::layoutColumns() {
  const double x0 = 5, x1 = width() - 4;
  QVector<double> nat(7, 0);
  m_hidden = QVector<bool>(7, false);
  auto need = [&] { double s = 0; int n = 0; for (int c = 0; c < 7; ++c) if (!m_hidden[c]) { s += nat[c]; ++n; } return s + (n - 1) * kGap; };
  auto natural = [&](bool tight) {
    const QFontMetricsF fn(nameFont()), fp(priceFont()), fw(wordFont(tight)), fd(detailFont(tight)), fh(headFont(true, tight));
    const double pad = tight ? 12 : 40, hpad = tight ? 10 : 24;
    nat[0] = fh.horizontalAdvance(QCoreApplication::translate("technical", "INSTRUMENT")) + hpad;
    for (int c = 1; c < 7; ++c) nat[c] = fh.horizontalAdvance(heat::columnTitle(heat::Column(c - 1)) + " ▾") + hpad;
    for (const auto& r : m_rows) {
      // tight: long names are elided (full name in the tile's tooltip), the tile keeps at least the price
      nat[0] = std::max(nat[0], tight ? std::min(fn.horizontalAdvance(r.name) + 30, std::max(110.0, fp.horizontalAdvance(r.price) + 30))
                                      : std::max(fn.horizontalAdvance(r.name), fp.horizontalAdvance(r.price)) + 30);
      for (int c = 1; c < 7; ++c) {
        const auto& k = r.cells[c - 1];
        // tight: the detail line ("×0.91 · BRK-B") may be elided, the cell tooltip has the numbers
        nat[c] = std::max(nat[c], (tight ? fw.horizontalAdvance(k.word) : std::max(fw.horizontalAdvance(k.word), fd.horizontalAdvance(k.detail))) + pad);
      }
    }
    if (m_rows.isEmpty()) nat[0] = std::max(nat[0], tight ? 110.0 : 150.0);   // reference empty state: header widths
  };
  natural(false);
  m_tight = need() > (x1 - x0);
  if (m_tight) natural(true);
  // still too narrow: low-priority columns are dropped (Structure, then Volume); their reads stay in the instrument
  // tile's tooltip and in the signal file of the selected row
  for (int c : {5, 4}) if (need() > (x1 - x0)) m_hidden[c] = true;
  QVector<double> wgt(7, 1.0); wgt[0] = 2.0;
  double sum = 0, wsum = 0; int shown = 0;
  for (int c = 0; c < 7; ++c) if (!m_hidden[c]) { sum += nat[c]; wsum += nat[c] * wgt[c]; ++shown; }
  const double avail = (x1 - x0) - (shown - 1) * kGap;
  const double extra = avail - sum;
  QVector<double> ww(7, 0);
  for (int c = 0; c < 7; ++c)
    if (!m_hidden[c]) ww[c] = nat[c] + (extra > 0 ? extra * nat[c] * wgt[c] / wsum : extra * nat[c] / sum);
  // wide windows: the instrument column stops at 340 px, the heat columns take the rest
  if (ww[0] > 340) {
    const double spare = ww[0] - 340; ww[0] = 340;
    double rest = 0; for (int c = 1; c < 7; ++c) rest += ww[c];
    for (int c = 1; c < 7; ++c) if (rest > 0) ww[c] += spare * ww[c] / rest;
  }
  m_x.resize(7); m_w.resize(7);
  double x = x0;
  for (int c = 0; c < 7; ++c) {
    if (m_hidden[c]) { m_x[c] = x; m_w[c] = 0; continue; }
    double w = ww[c];
    m_x[c] = std::round(x); m_w[c] = std::round(x + w) - std::round(x);
    x += w + kGap;
  }
  prepareText();
}

// What paintEvent draws as text depends on the rows and the column widths only: elided once here, not per paint.
void HeatTable::prepareText() {
  m_text.resize(m_rows.size());
  if (m_x.size() < 7) return;
  const QFontMetricsF mn(nameFont()), mw(wordFont(m_tight)), md(detailFont(m_tight));
  const double ls = m_tight ? 0.3 : 1.5, lsd = m_tight ? 0.0 : 0.6;
  for (int i = 0; i < m_rows.size(); ++i) {
    const auto& row = m_rows[i];
    auto& t = m_text[i];
    t.name = mn.elidedText(row.name, Qt::ElideRight, m_w[0] - 16);
    for (int c = 1; c < 7 && c - 1 < row.cells.size(); ++c) {
      const auto& k = row.cells[c - 1];
      t.word[c - 1] = mw.elidedText(k.word, Qt::ElideRight, m_w[c] - 8);
      t.detail[c - 1] = md.elidedText(k.detail, Qt::ElideRight, m_w[c] - 8);
      t.tw[c - 1] = mw.horizontalAdvance(t.word[c - 1]) - ls;
      t.dw[c - 1] = md.horizontalAdvance(t.detail[c - 1]) - lsd;
    }
  }
}

QRect HeatTable::tileRect(int i) const {
  if (i < 0 || i >= m_rows.size() || m_x.isEmpty()) return {};
  const double y = kBodyTop + i * kPitch - m_scroll;
  return QRectF(m_x[0], y, m_w[0], kCellH).toAlignedRect().adjusted(-1, -1, 1, 1);
}

double HeatTable::contentHeight() const {
  if (m_rows.isEmpty()) return 72 + legendHeight();
  return m_rows.size() * kPitch - (kPitch - kCellH) + 16 + legendHeight();
}
double HeatTable::viewportHeight() const { return std::max(0.0, height() - kBodyTop); }
void HeatTable::clampScroll() { m_scroll = std::clamp(m_scroll, 0.0, std::max(0.0, contentHeight() - viewportHeight())); }
void HeatTable::resizeEvent(QResizeEvent*) { layoutColumns(); clampScroll(); }
void HeatTable::hideEvent(QHideEvent*) { m_anim.stop(); m_fading = false; }

void HeatTable::wheelEvent(QWheelEvent* e) {
  const double d = !e->pixelDelta().isNull() ? e->pixelDelta().y() : e->angleDelta().y() / 120.0 * kPitch * 1.5;
  const double before = m_scroll;
  m_scroll -= d; clampScroll();
  if (m_scroll != before) update();
  e->accept();
}

int HeatTable::rowAt(const QPointF& p) const {
  if (p.y() < kBodyTop) return -1;
  const double y = p.y() - kBodyTop + m_scroll;
  const int i = int(y / kPitch);
  if (i < 0 || i >= m_rows.size() || std::fmod(y, kPitch) > kCellH) return -1;
  if (p.x() < m_x.value(0) || p.x() > m_x.value(6) + m_w.value(6)) return -1;
  return i;
}

void HeatTable::mousePressEvent(QMouseEvent* e) {
  if (e->position().y() < kHeaderH + 2) {
    for (int c = 0; c < 7; ++c)
      if (m_w[c] > 0 && e->position().x() >= m_x[c] && e->position().x() <= m_x[c] + m_w[c]) { emit headerClicked(c); return; }
    return;
  }
  const int i = rowAt(e->position());
  if (i >= 0) emit rowClicked(m_rows[i].symbol);
}

void HeatTable::mouseMoveEvent(QMouseEvent* e) {
  const int i = rowAt(e->position());
  int h = -1;
  if (e->position().y() < kHeaderH + 2)
    for (int c = 0; c < 7; ++c) if (m_w[c] > 0 && e->position().x() >= m_x[c] && e->position().x() <= m_x[c] + m_w[c]) h = c;
  if (i != m_hover || h != m_hoverHeader) {
    // hover only changes the instrument tile's border and the header title colour
    QRect dirty = tileRect(m_hover) | tileRect(i);
    if (h != m_hoverHeader) dirty |= QRect(0, 0, width(), int(kHeaderH) + 1);
    m_hover = i; m_hoverHeader = h;
    update(dirty);
  }
  setCursor(i >= 0 || h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  // tooltip: the numbers behind the hovered cell
  QString tip;
  if (i >= 0) for (int c = 1; c < 7; ++c)
    if (m_w[c] > 0 && e->position().x() >= m_x[c] && e->position().x() <= m_x[c] + m_w[c]) tip = m_rows[i].cells[c - 1].tip;
  if (i >= 0 && e->position().x() <= m_x[0] + m_w[0]) {   // instrument tile: name + the reads of dropped columns
    tip = m_rows[i].name;
    for (int c = 1; c < 7; ++c) if (m_hidden.value(c)) {
      const auto& k = m_rows[i].cells[c - 1];
      tip += QStringLiteral("\n%1: %2%3").arg(heat::columnTitle(heat::Column(c - 1)), k.word, k.detail.isEmpty() ? QString() : " " + k.detail);
    }
  }
  if (tip.isEmpty()) QToolTip::hideText(); else QToolTip::showText(e->globalPosition().toPoint(), tip, this);
}
void HeatTable::leaveEvent(QEvent*) {
  QRect dirty = tileRect(m_hover);
  if (m_hoverHeader >= 0) dirty |= QRect(0, 0, width(), int(kHeaderH) + 1);
  m_hover = -1; m_hoverHeader = -1;
  if (!dirty.isEmpty()) update(dirty);
}

static void tile(QPainter& p, const QRectF& r, const QColor& fill, const QColor& border) {
  QPainterPath path; path.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 2.5, 2.5);
  p.fillPath(path, fill);
  if (fill.lightness() > 20) {   // inset glow: brighter towards the edges (reference CSS inset shadow)
    const QColor edge = theme::mix(fill, theme::map(255, 255, 255), 0.16);
    for (int k = 1; k <= 7; ++k) {
      QColor c = edge; c.setAlpha(70 - k * 9);
      p.setPen(QPen(c, 1)); p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(r.adjusted(k + 0.5, k + 0.5, -k - 0.5, -k - 0.5), 2, 2);
    }
  }
  p.setPen(QPen(border, 1)); p.setBrush(Qt::NoBrush); p.drawPath(path);
}

// Legend items wrap onto a second line when the table is narrower than the legend.
double HeatTable::legendHeight() const {
  const QFont f = theme::mono(9, QFont::Normal, 2.0), fb = theme::mono(9, QFont::Bold, 2.0);
  double w = 26 + mkit::textWidth(f, QCoreApplication::translate("technical", "LEGEND"));
  for (const char* t : {QT_TRANSLATE_NOOP("technical", "BULLISH LEAN"), QT_TRANSLATE_NOOP("technical", "BEARISH LEAN"), QT_TRANSLATE_NOOP("technical", "ELEVATED VS OWN BASELINE"), QT_TRANSLATE_NOOP("technical", "SUBDUED"), QT_TRANSLATE_NOOP("technical", "NEAR A LIQUIDITY POOL")}) w += 26 + mkit::textWidth(fb, QCoreApplication::translate("technical", t));
  w += mkit::textWidth(f, QCoreApplication::translate("technical", "INTENSITY = MAGNITUDE OF THE READ"));
  return w > width() - 4 ? 34 : 18;
}

void HeatTable::paintLegend(QPainter& p, double y) {
  const QFont f = theme::mono(9, QFont::Normal, 2.0), fb = theme::mono(9, QFont::Bold, 2.0);
  double x = 1;
  auto wrap = [&](double w) { if (x + w > width() - 4 && x > 20) { x = 13; y += 16; } };
  const QColor dim = theme::map("#5a574d");
  p.setPen(QPen(dim, 1)); p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(x + 4, y - 3.2), 3.6, 3.6);
  x += 12; p.setFont(f); p.setPen(dim); p.drawText(QPointF(x, y), QCoreApplication::translate("technical", "LEGEND")); x += mkit::textWidth(f, QCoreApplication::translate("technical", "LEGEND")) + 14;
  const struct { const char* t; QColor c; } items[] = {
    {QT_TRANSLATE_NOOP("technical", "BULLISH LEAN"), theme::map("#2cd5ea")}, {QT_TRANSLATE_NOOP("technical", "BEARISH LEAN"), theme::map("#f48284")}, {QT_TRANSLATE_NOOP("technical", "ELEVATED VS OWN BASELINE"), theme::map("#f8d734")},
    {QT_TRANSLATE_NOOP("technical", "SUBDUED"), theme::map("#b0b1b9")}, {QT_TRANSLATE_NOOP("technical", "NEAR A LIQUIDITY POOL"), theme::map("#afa3e5")}};
  for (const auto& it : items) {
    const QString t = QCoreApplication::translate("technical", it.t);
    wrap(12 + mkit::textWidth(fb, t));
    p.fillRect(QRectF(x, y - 6, 6, 6), it.c);
    x += 12; p.setFont(fb); p.setPen(it.c); p.drawText(QPointF(x, y), t);
    x += mkit::textWidth(fb, t) + 14;
  }
  wrap(mkit::textWidth(f, QCoreApplication::translate("technical", "INTENSITY = MAGNITUDE OF THE READ")));
  p.setFont(f); p.setPen(theme::map("#4a4842")); p.drawText(QPointF(x, y), QCoreApplication::translate("technical", "INTENSITY = MAGNITUDE OF THE READ"));
}

void HeatTable::paintEvent(QPaintEvent* ev) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  // header
  if (ev->rect().top() < kBodyTop) {
    for (int c = 0; c < 7; ++c) {
      if (m_w[c] <= 0) continue;
      const QRectF r(m_x[c], 0, m_w[c], kHeaderH);
      p.fillRect(r, theme::map("#0c0a07"));
      const bool active = c == m_sortCol, hov = c == m_hoverHeader;
      const QFont f = headFont(active, m_tight);
      p.setFont(f);
      const QFontMetricsF fm(f);
      const double arrowW = active ? 12 : 0;
      const QString t = fm.elidedText(c == 0 ? QCoreApplication::translate("technical", "INSTRUMENT") : heat::columnTitle(heat::Column(c - 1)),
                                      Qt::ElideRight, r.width() - 10 - arrowW);
      const double by = r.center().y() + fm.capHeight() / 2;
      const QColor col = active ? theme::map("#f3dc7d") : hov ? mkit::sem().goldText : theme::map("#6b6a66");
      const double tw = fm.horizontalAdvance(t) - (m_tight ? 0.5 : 3.0);
      const double tx = c == 0 ? r.left() + 7 : r.center().x() - (tw + arrowW) / 2;
      if (active) mkit::glowText(p, QPointF(tx, by), t, col, 1); else { p.setPen(col); p.drawText(QPointF(tx, by), t); }
      if (active) {
        p.setPen(Qt::NoPen); p.setBrush(theme::map("#f3d64a"));
        const double ax = tx + tw + 8, ay = by - 3.5;
        QPolygonF tri; if (m_desc) tri << QPointF(ax - 2.5, ay - 1.2) << QPointF(ax + 2.5, ay - 1.2) << QPointF(ax, ay + 1.8);
        else tri << QPointF(ax - 2.5, ay + 1.8) << QPointF(ax + 2.5, ay + 1.8) << QPointF(ax, ay - 1.2);
        p.drawPolygon(tri);
      }
    }
  }
  // body
  p.setClipRect(QRectF(0, kBodyTop, width(), height() - kBodyTop));
  const double vh = viewportHeight();
  const int first = std::max(0, int(m_scroll / kPitch));
  const int last = std::min<int>(m_rows.size() - 1, int((m_scroll + vh) / kPitch) + 1);
  const qint64 el = m_fading ? m_fade.elapsed() : 1 << 30;
  const QFont fn = nameFont(), fp = priceFont(), fw = wordFont(m_tight), fd = detailFont(m_tight);
  const QFontMetricsF mw(fw);
  const double capW = mw.capHeight();
  for (int i = first; i <= last; ++i) {
    const auto& row = m_rows[i];
    const double y = kBodyTop + i * kPitch - m_scroll;
    // rows fade in top → bottom: 12 ms stagger, 110 ms each (measured on frame 17.5: row 1 full, row 12 ≈ 40 %)
    const double a = std::clamp((el - (i - first) * 12.0) / 110.0, 0.0, 1.0);
    if (a <= 0) continue;
    p.setOpacity(a);
    const bool sel = row.symbol == m_selected, hov = i == m_hover;
    // instrument tile
    const QRectF ir(m_x[0], y, m_w[0], kCellH);
    tile(p, ir, sel ? theme::map("#282103") : mkit::sem().cell, sel ? theme::map("#4d4318") : hov ? theme::map("#3a3420") : mkit::sem().subtle);
    p.setFont(fn); p.setPen(sel ? theme::map("#f7dc72") : theme::map("#fffffd"));
    const RowText& txt0 = m_text[i];
    p.drawText(QPointF(ir.left() + 9, y + 25), txt0.name);
    p.setFont(fp); p.setPen(sel ? theme::map("#8f8550") : theme::map("#5b5a56"));
    p.drawText(QPointF(ir.left() + 10, y + 43), row.price);
    for (int c = 1; c < 7; ++c) {
      if (m_w[c] <= 0) continue;
      const auto& k = row.cells[c - 1];
      const QRectF r(m_x[c], y, m_w[c], kCellH);
      const QColor fill = k.fill.isValid() ? k.fill : theme::map("#0b0a06");
      const QColor border = sel ? kSelBorder() : k.fill.isValid() ? theme::mix(k.fill, theme::map(255, 255, 255), 0.07) : mkit::sem().subtle;
      tile(p, r, fill, border);
      const QColor txt = mkit::readable(k.text, fill);
      p.setPen(txt);
      // a cell squeezed under its word: elided (the cell tooltip has the numbers, the signal file the words)
      const QString& word = txt0.word[c - 1];
      const double tw = txt0.tw[c - 1];
      if (k.detail.isEmpty()) {
        p.setFont(fw);
        p.drawText(QPointF(r.center().x() - tw / 2, y + kCellH / 2 + capW / 2), word);
      } else {
        p.setFont(fw);
        p.drawText(QPointF(r.center().x() - tw / 2, y + 24), word);
        p.setFont(fd);
        QColor dc = txt; dc.setAlpha(theme::isDefault() ? 185 : 215); p.setPen(dc);
        p.drawText(QPointF(r.center().x() - txt0.dw[c - 1] / 2, y + 41), txt0.detail[c - 1]);
      }
    }
  }
  p.setOpacity(1);
  if (m_rows.isEmpty()) {
    const QFont f = theme::mono(11, QFont::Normal, 3.0);
    p.setFont(f); p.setPen(theme::map("#6a6967"));
    const QString msg = m_loading.isEmpty() ? QCoreApplication::translate("technical", "NO INSTRUMENT MATCHES THESE FILTERS.") : m_loading;
    const double tw = mkit::textWidth(f, msg) - 3;
    const double x0 = m_x.value(0), x1 = m_x.value(6) + m_w.value(6);
    p.drawText(QPointF((x0 + x1) / 2 - tw / 2, kBodyTop + 32 - m_scroll), msg);
    paintLegend(p, kBodyTop + 71 - m_scroll);
  } else {
    paintLegend(p, kBodyTop + m_rows.size() * kPitch - (kPitch - kCellH) + 18 - m_scroll);
  }
  // thin scrollbar
  const double ch = contentHeight();
  if (ch > vh + 1) {
    const double hh = std::max(24.0, vh * vh / ch);
    const double hy = kBodyTop + (vh - hh) * (m_scroll / (ch - vh));
    p.setPen(Qt::NoPen); p.setBrush(theme::map(58, 50, 32, 150));
    p.drawRoundedRect(QRectF(width() - 2.5, hy, 2.5, hh), 1.2, 1.2);
  }
}
