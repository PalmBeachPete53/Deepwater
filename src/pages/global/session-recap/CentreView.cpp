#include "pages/global/session-recap/CentreView.h"
#include "core/Settings.h"
#include "ui/Format.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QLinearGradient>
#include "core/I18n.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QTimeZone>
#include <cmath>

namespace wire::recap {
namespace {
constexpr int kMargin = 21, kTimelineH = 413, kCardH = 137;
// colours measured on Reference/Global/session-recap.mp4 (docs/reference/Global/session-recap.md)
inline QColor kPosText() { return theme::map("#7bb9be"); }
inline QColor kNegText() { return theme::map("#bf8d8f"); }
inline QColor kPosCard() { return theme::map("#93d6dc"); }
inline QColor kNegCard() { return theme::map("#e39f9b"); }
inline QColor kPosLine() { return theme::map("#1bdbeb"); }
inline QColor kNegLine() { return theme::map("#fd7f83"); }
inline QColor kPosSpark() { return theme::map("#3ed3e0"); }
inline QColor kNegSpark() { return theme::map("#e08c8b"); }
inline QColor kGold() { return theme::map("#cdc392"); }
inline QColor kBody() { return theme::map("#a6a59f"); }
inline QColor kSecText() { return theme::map("#b4afa6"); }
inline QColor kMid() { return theme::map("#84837f"); }

QFont titleFont() { QFont f = theme::condensed(37, QFont::Bold); f.setLetterSpacing(QFont::AbsoluteSpacing, 3.4); return f; }
QFont leadFont() { return theme::condensed(17, QFont::Medium); }
QFont secFont() { return theme::condensed(15, QFont::Medium); }

// word-wrapped paragraph: returns line count; draws when p != nullptr (first baseline at y)
int paragraph(QPainter* p, const QString& text, const QFont& f, qreal x, qreal y, qreal width, qreal lineH) {
  QTextLayout tl(text, f);
  tl.beginLayout();
  int n = 0;
  QList<QTextLine> lines;
  for (;;) { QTextLine l = tl.createLine(); if (!l.isValid()) break; l.setLineWidth(width); l.setPosition(QPointF(0, n * lineH)); lines << l; ++n; }
  tl.endLayout();
  if (p) {
    const qreal asc = QFontMetricsF(f).ascent();
    tl.draw(p, QPointF(x, y - asc));
  }
  return n;
}
QString pctText(double v) { return std::isfinite(v) ? fmt::pct(v) : fmt::dash(); }
QColor signText(double v) { return v >= 0 ? kPosText() : kNegText(); }
}

CentreView::CentreView(QWidget* parent) : SmoothScroll(parent) {}

void CentreView::setWindow(const Window& w, const QVector<Stat>& st, const QList<wire::Release>& rel,
                           const QList<wire::Item>& headlines, bool connected, bool loading, const QString& note) {
  const bool same = w.key() == m_w.key();
  m_w = w; m_st = st; m_rel = rel; m_wire = headlines; m_connected = connected; m_loading = loading; m_note = note;
  if (!same) m_userTraced = false;
  if (!same || !m_userTraced || m_traced < 0 || m_traced >= m_st.size() || !m_st[m_traced].ok()) {
    const auto ch = chipAssets();
    m_traced = ch.isEmpty() ? -1 : ch.first();
  }
  m_read = compose(m_w, m_st, m_rel, m_wire, m_lang);
  relayout();
  viewport()->update();
}
void CentreView::setTraced(int a) { if (a >= 0 && a < m_st.size()) { m_traced = a; m_userTraced = true; viewport()->update(); } }
void CentreView::setLang(const QString& l) {
  m_lang = l;
  m_read = compose(m_w, m_st, m_rel, m_wire, m_lang);
  relayout();
  viewport()->update();
}
void CentreView::setHoverX(int x) { m_hoverX = x; viewport()->update(); }

QList<int> CentreView::chipAssets() const {
  QList<int> idx;
  for (int i = 0; i < m_st.size(); ++i) if (m_st[i].ok()) idx << i;
  std::sort(idx.begin(), idx.end(), [&](int a, int b) { return std::abs(m_st[a].chg) > std::abs(m_st[b].chg); });
  return idx.mid(0, 6);
}

void CentreView::resizeEvent(QResizeEvent* e) { SmoothScroll::resizeEvent(e); relayout(); }

void CentreView::relayout() {
  Geo g;
  g.w = viewport()->width();
  const int L = kMargin, R = g.w - kMargin;
  g.card = QRect(L, 136, R - L, 0);
  const int inner = g.card.left() + 18;
  const qreal leadW = qMin<qreal>(g.card.width() >= 1500 ? 900 : 660, g.card.width() - 36);
  const int leadLines = paragraph(nullptr, m_read.lead, leadFont(), 0, 0, leadW, 26);
  g.leadH = leadLines * 26;
  const int y0 = g.card.top() + 107 + (leadLines - 1) * 26 + 39;
  // wide views (card ≥ 1500 px): the sections flow in two columns so prose lines stay ≤ 900 px
  const int ncol = g.card.width() >= 1500 ? 2 : 1;
  const int colW = (g.card.width() - 36 - (ncol - 1) * 40) / ncol;
  const int perCol = (int(m_read.sections.size()) + ncol - 1) / ncol;
  for (int k = 0; k < m_read.sections.size(); ++k) {
    const int col = k / qMax(1, perCol);
    const int x = g.card.left() + 18 + col * (colW + 40);
    g.secX << x; g.secRight << x + colW;
  }
  QVector<int> colY(ncol, y0);
  for (int k = 0; k < m_read.sections.size(); ++k) {
    const auto& s = m_read.sections[k];
    const int col = k / qMax(1, perCol);
    const int textX = g.secX[k] + 186, textW = qMin(900, g.secRight[k] - 7 - textX);
    const int lines = paragraph(nullptr, s.text, secFont(), 0, 0, textW, 23);
    const int h = qMax(22 + (int(s.assets.size()) - 1) * 19, (lines - 1) * 23 + 5) + 37;
    g.secTop << colY[col]; g.secH << h;
    colY[col] += h;
  }
  int y = y0;
  for (int v : colY) y = qMax(y, v);
  g.card.setBottom(y - 3);
  Q_UNUSED(inner)
  g.extra = qBound(0, (g.w - 900) / 7, 150);   // the chart grows on wide views
  g.timeline = QRect(L, g.card.bottom() + 14, R - L, kTimelineH + g.extra);
  g.plot = QRect(g.timeline.left() + 15, g.timeline.top() + 69, g.timeline.width() - 84, 250 + g.extra);   // reference: grid/fill 69–319, values 85–298
  // chips
  const QFont fc = theme::mono(11);
  const QFontMetricsF fm(fc);
  qreal cx = g.timeline.left() + 13 + QFontMetricsF(theme::mono(10, QFont::Normal, 2.2)).horizontalAdvance(QCoreApplication::translate("wire", "WINDOW TIMELINE")) + 14;
  for (int a : chipAssets()) {
    const qreal w = fm.horizontalAdvance(assetName(a) + " " + pctText(m_st[a].chg)) + 12;
    if (cx + w > g.timeline.right() - 10) break;
    g.chips << QRectF(cx, g.timeline.top() + 16, w, 19);
    cx += w + 6;
  }
  // grid 4x3
  const int gt = g.timeline.bottom() + 39;
  const int cw = (R - L - 3 * 8) / 4;
  const int ch = qBound(kCardH, int(cw * 0.42), 220);   // taller sparklines when the cards get wide
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 4; ++c) g.cards << QRect(L + c * (cw + 8), gt + r * (ch + 9), cw, ch);
  g.grid = QRect(L, gt, R - L, 3 * ch + 18);
  g.height = g.grid.bottom() + 26;
  // EN / FR toggle
  g.en = QRectF(g.card.right() - 79, g.card.top() + 19, 29, 20);
  g.fr = QRectF(g.card.right() - 47, g.card.top() + 19, 27, 20);
  m_geo = g;
  setContentHeight(g.height);
}

qreal CentreView::xOf(qint64 t) const {
  const QRectF r = m_geo.plot;
  const double a = m_w.start.toSecsSinceEpoch(), b = m_w.end.toSecsSinceEpoch();
  return r.left() + (t - a) / qMax(1.0, b - a) * r.width();
}

QVector<QPointF> CentreView::points(int a, const QRectF& r, double lo, double hi) const {
  QVector<QPointF> out;
  if (a < 0 || a >= m_st.size() || !m_st[a].ok()) return out;
  const auto& s = m_st[a];
  const double t0 = m_w.start.toSecsSinceEpoch(), t1 = m_w.end.toSecsSinceEpoch();
  auto X = [&](double t) { return r.left() + (qBound(t0, t, t1) - t0) / qMax(1.0, t1 - t0) * r.width(); };
  auto Y = [&](double v) { return r.bottom() - (v - lo) / qMax(1e-12, hi - lo) * r.height(); };
  out << QPointF(X(s.t.first()), Y(s.open));
  for (int i = 0; i < s.t.size(); ++i) out << QPointF(X(s.t[i] + 900), Y(s.c[i]));
  return out;
}

int CentreView::hoverIndex() const {
  if (m_hoverX < 0 || m_traced < 0 || !m_st.value(m_traced).ok()) return -1;
  const auto& s = m_st[m_traced];
  int best = -1; qreal bd = 1e9;
  for (int i = 0; i <= s.t.size(); ++i) {
    const qreal x = xOf(i == 0 ? s.t.first() : s.t[i - 1] + 900);
    if (std::abs(x - m_hoverX) < bd) { bd = std::abs(x - m_hoverX); best = i; }
  }
  return best;
}

// ---------------------------------------------------------------------------------------------- painting
void CentreView::paintEvent(QPaintEvent* e) {
  QPainter p(viewport());
  p.fillRect(e->rect(), theme::map("#060503"));
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(0, -offset());
  // The crosshair repaints only the timeline card (see mouseMoveEvent): the title, the read (text layouts) and the
  // grid of sparklines are painted only when the repaint reaches them.
  const QRect dirty = e->rect().translated(0, offset());
  if (dirty.top() <= m_geo.card.bottom() + 1) { paintTitle(p); paintRead(p); }
  paintTimeline(p);
  if (dirty.bottom() > m_geo.timeline.bottom()) paintGrid(p);
}
void CentreView::updateTimeline() { viewport()->update(m_geo.timeline.translated(0, -offset()).adjusted(-2, -2, 2, 2)); }

void CentreView::paintTitle(QPainter& p) {
  const int L = kMargin;
  const QString code = m_w.week ? QCoreApplication::translate("wire", "WEEK") : m_w.code();
  p.setFont(theme::mono(10, QFont::Normal, 3.2)); p.setPen(theme::map("#585855"));
  p.drawText(QPointF(L, 32), QCoreApplication::translate("wire", "SESSION FILE · %1").arg(code));
  qreal tx = L;
  if (!m_w.week) { p.drawPixmap(QPointF(L, 54), ui::flag(m_w.flag(), QSize(22, 15), devicePixelRatioF())); tx = L + 31; }
  p.setFont(titleFont()); p.setPen(theme::map("#fbfbf8"));
  QString title = m_w.title();
  if (m_w.week) title += "  ·  " + i18n::locale().toString(m_w.monday, QCoreApplication::translate("wire", "d MMM")).toUpper() + " – " +
                          i18n::locale().toString(m_w.monday.addDays(4), QCoreApplication::translate("wire", "d MMM")).toUpper();
  p.drawText(QPointF(tx, 75), QFontMetricsF(p.font()).elidedText(title, Qt::ElideRight, viewport()->width() - L - tx));
  // meta line
  const QFont fm = theme::mono(12);
  QFontMetricsF m(fm);
  qreal x = L;
  auto put = [&](const QString& s, const QColor& c) { p.setFont(fm); p.setPen(c); p.drawText(QPointF(x, 100), s); x += m.horizontalAdvance(s); };
  const QLocale en = i18n::locale();
  QString when;
  if (m_w.week) when = en.toString(m_w.start.toLocalTime(), QCoreApplication::translate("wire", "ddd d MMM HH:mm")) + " → " + en.toString(m_w.end.toLocalTime(), QCoreApplication::translate("wire", "ddd d MMM HH:mm"));
  else when = en.toString(m_w.session.date, QCoreApplication::translate("wire", "ddd d MMM")) + " · " + m_w.start.toLocalTime().toString("HH:mm") + "-" +
              m_w.end.toLocalTime().toString("HH:mm");
  put(when + " " + wire::utcLabel(m_w.start.toLocalTime()), kMid());
  int up = 0, dn = 0;
  for (const auto& s : m_st) if (s.ok()) (s.chg >= 0 ? up : dn)++;
  put("   " + QCoreApplication::translate("wire", "Breadth") + " ", kMid());
  put(QString("%1↑").arg(up), kPosText()); put(" ", kMid()); put(QString("%1↓").arg(dn), kNegText());
  put("   " + (m_connected ? (m_rel.size() == 1 ? QCoreApplication::translate("wire", "%1 release") : QCoreApplication::translate("wire", "%1 releases")).arg(m_rel.size()) : QCoreApplication::translate("wire", "— releases")), kMid());
  put("   " + (m_wire.size() == 1 ? QCoreApplication::translate("wire", "%1 headline") : QCoreApplication::translate("wire", "%1 headlines")).arg(m_wire.size()), kMid());
  // status pill
  const bool live = m_w.live();
  const QString st = live ? QCoreApplication::translate("wire", "LIVE") : QCoreApplication::translate("wire", "CLOSED");
  const QFont fp = theme::mono(9, QFont::Normal, 1.6);
  QRectF pill(x + 24, 86, QFontMetricsF(fp).horizontalAdvance(st) + 16, 18);
  if (pill.right() > viewport()->width() - L) pill.moveTopRight(QPointF(viewport()->width() - L, 20));   // narrow: beside SESSION FILE
  p.setPen(QPen(live ? theme::map("#968612") : theme::map("#3d3d3a"), 1)); p.setBrush(live ? theme::map("#342a03") : theme::map("#181818"));
  p.drawRoundedRect(pill.adjusted(.5, .5, -.5, -.5), 2, 2);
  p.setFont(fp); p.setPen(live ? theme::map("#f8e169") : theme::map("#5f605b"));
  p.drawText(pill, Qt::AlignCenter, st);
  // gold rule
  QLinearGradient g(L, 0, L + (viewport()->width() - 2 * L) * 0.82, 0);
  g.setColorAt(0, kGold()); g.setColorAt(0.55, theme::alpha(kGold(), 110)); g.setColorAt(1, theme::alpha(kGold(), 0));
  p.fillRect(QRectF(L, 119, viewport()->width() - 2 * L, 2), g);
}

void CentreView::paintRead(QPainter& p) {
  const auto& g = m_geo;
  const QRect c = g.card;
  {   // reference: warm olive at the top fading to near-black (t000: #181808 → #0b0b0b)
    QLinearGradient bg(0, c.top(), 0, c.bottom());
    bg.setColorAt(0, theme::map("#171708")); bg.setColorAt(0.35, theme::map("#0c0c0a")); bg.setColorAt(1, theme::map("#0b0d0b"));
    p.setPen(Qt::NoPen); p.setBrush(bg); p.drawRect(c);
  }
  p.setPen(QPen(theme::map("#4d4a39"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(c).adjusted(.5, .5, -.5, -.5));
  const int x0 = c.left() + 18;
  // ✣ AUTOMATED READ
  p.setPen(QPen(theme::map("#a07a1c"), 1.2));
  const QPointF ic(x0 + 5, c.top() + 28);
  for (int k = 0; k < 4; ++k) { const qreal a = k * M_PI / 2 + M_PI / 4; p.drawLine(ic, ic + QPointF(std::cos(a) * 5, std::sin(a) * 5)); }
  p.drawLine(ic + QPointF(-3, 0), ic + QPointF(3, 0)); p.drawLine(ic + QPointF(0, -3), ic + QPointF(0, 3));
  p.setFont(theme::mono(10, QFont::Normal, 3.4)); p.setPen(theme::map("#4c493c"));
  p.drawText(QPointF(x0 + 17, c.top() + 32), QCoreApplication::translate("wire", "AUTOMATED READ"));
  p.setFont(theme::mono(9, QFont::Normal, 2.6)); p.setPen(theme::map("#45433b"));
  const QString right = m_w.live() ? QCoreApplication::translate("wire", "UPDATED LIVE · RULES ON THE DATA BELOW")
                                   : QCoreApplication::translate("wire", "COMPUTED BY DEEPWATER · RULES ON THE DATA BELOW");
  {
    const qreal room = g.en.left() - 12 - (x0 + 17 + QFontMetricsF(theme::mono(10, QFont::Normal, 3.4)).horizontalAdvance(QCoreApplication::translate("wire", "AUTOMATED READ")) + 16);
    QString r = right;
    if (QFontMetricsF(p.font()).horizontalAdvance(r) > room) r = r.mid(r.indexOf("·") + 2);
    if (QFontMetricsF(p.font()).horizontalAdvance(r) <= room)
      p.drawText(QRectF(g.en.left() - 12 - room, c.top() + 19, room, 20), Qt::AlignRight | Qt::AlignVCenter, r);
  }
  const bool en = m_lang != "FR";
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#fed41c")); p.drawRect(en ? g.en : g.fr);
  p.setFont(theme::mono(11, QFont::Bold, 1.5));
  p.setPen(en ? (theme::isDefault() ? QColor("#1a1400") : theme::onAccent()) : theme::map("#79776b")); p.drawText(g.en, Qt::AlignCenter, "EN");
  p.setPen(!en ? (theme::isDefault() ? QColor("#1a1400") : theme::onAccent()) : theme::map("#79776b")); p.drawText(g.fr, Qt::AlignCenter, "FR");
  // headline + lead
  p.setFont(theme::condensed(26, QFont::DemiBold)); p.setPen(theme::map("#f8f6ee"));
  p.drawText(QPointF(x0, c.top() + 73), QFontMetricsF(p.font()).elidedText(m_read.headline, Qt::ElideRight, c.width() - 40));
  p.setPen(kBody());
  paragraph(&p, m_read.lead, leadFont(), x0, c.top() + 107, qMin<qreal>(c.width() >= 1500 ? 900 : 660, c.width() - 36), 26);
  // sections
  for (int k = 0; k < m_read.sections.size(); ++k) {
    const auto& s = m_read.sections[k];
    const int top = g.secTop[k];
    const int x0 = g.secX[k];
    const int textX = x0 + 186, textW = qMin(900, g.secRight[k] - 7 - textX);
    if (top > g.secTop.value(0) || (k > 0 && g.secX[k] == g.secX[k - 1]))
      p.fillRect(QRect(x0, top - 22, g.secRight[k] - x0, 1), theme::map("#18170f"));
    p.setFont(theme::mono(11, QFont::Bold, 3)); p.setPen(theme::map("#d8b445"));
    p.drawText(QPointF(x0, top), s.label);
    int ly = top + 22;
    for (int a : s.assets) {
      p.setFont(theme::mono(12)); p.setPen(theme::map("#828278"));
      p.drawText(QPointF(x0, ly), assetName(a));
      const double ch = m_st.value(a).chg;
      p.setPen(std::isfinite(ch) ? signText(ch) : theme::map("#5f5d57"));
      p.drawText(QRectF(x0, ly - 13, textX - 16 - x0, 16), Qt::AlignRight | Qt::AlignVCenter, pctText(ch));
      ly += 19;
    }
    p.setPen(kSecText());
    paragraph(&p, s.text, secFont(), textX, top + 5, textW, 23);
  }
}

void CentreView::paintTimeline(QPainter& p) {
  const auto& g = m_geo;
  const QRect T = g.timeline;
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#0d0802")); p.drawRect(T);
  p.setPen(QPen(theme::map("#3a3522"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(T).adjusted(.5, .5, -.5, -.5));
  ui::drawBrackets(p, QRectF(T).adjusted(3, 3, -3, -3), theme::map("#918a73"), 10, true, false);
  ui::drawBrackets(p, QRectF(T).adjusted(3, 3, -3, -3), theme::map("#5a4f2c"), 10, false, true);
  p.setFont(theme::mono(10, QFont::Normal, 2.2)); p.setPen(theme::map("#5b5750"));
  p.drawText(QPointF(T.left() + 13, T.top() + 29), QCoreApplication::translate("wire", "WINDOW TIMELINE"));
  // lane rules (reference: under the plot, between the lanes, under the wire lane)
  for (int y : {325, 353, 383}) p.fillRect(QRect(T.left() + 1, T.top() + y + g.extra, T.width() - 2, 1), theme::map("#211c10"));
  // chips
  const auto ch = chipAssets();
  for (int k = 0; k < g.chips.size(); ++k) {
    const int a = ch[k];
    const QRectF r = g.chips[k];
    const bool on = a == m_traced;
    p.setPen(QPen(on ? theme::map("#4a4413") : theme::map("#2a2a22"), 1)); p.setBrush(on ? theme::map("#39340b") : theme::map("#0d0a04"));
    p.drawRect(r.adjusted(.5, .5, -.5, -.5));
    const QFont f = theme::mono(11, on ? QFont::Bold : QFont::Normal);
    p.setFont(f); p.setPen(on ? theme::map("#e3d8a5") : theme::map("#807d76"));
    const QString n = assetName(a) + " ";
    p.drawText(QPointF(r.left() + 6, r.center().y() + 4), n);
    p.setFont(theme::mono(11)); p.setPen(signText(m_st[a].chg));
    p.drawText(QPointF(r.left() + 6 + QFontMetricsF(f).horizontalAdvance(n), r.center().y() + 4), pctText(m_st[a].chg));
  }
  // legend
  {
    const QFont f = theme::mono(10);
    const QFontMetricsF fm(f);
    const QString tz = wire::utcLabel(m_w.start.toLocalTime());
    qreal x = T.right() - 15 - fm.horizontalAdvance(tz);
    const qreal by = T.top() + 54;
    p.setFont(f); p.setPen(theme::map("#535048")); p.drawText(QPointF(x, by), tz);
    auto item = [&](const QString& t, int kind) {
      x -= fm.horizontalAdvance(t) + 14;
      p.setPen(theme::map("#535048")); p.drawText(QPointF(x + 9, by), t);
      const QPointF c(x + 3, by - 4);
      if (kind == 2) { p.fillRect(QRectF(c.x() - 1.5, c.y() - 4.5, 3, 9), theme::map("#dfdcda")); return; }
      QPainterPath d; d.moveTo(c + QPointF(0, -4)); d.lineTo(c + QPointF(4, 0)); d.lineTo(c + QPointF(0, 4)); d.lineTo(c + QPointF(-4, 0)); d.closeSubpath();
      p.fillPath(d, kind == 0 ? theme::map("#e8da9b") : theme::map("#585451"));
    };
    item(QCoreApplication::translate("wire", "headline"), 2); item(QCoreApplication::translate("wire", "medium"), 1); item(QCoreApplication::translate("wire", "high impact"), 0);
  }
  const QRectF plot = g.plot;
  // vertical gridlines + time labels
  const QDateTime ls = m_w.start.toLocalTime();
  QDateTime t = QDateTime(ls.date(), QTime(ls.time().hour(), 0), ls.timeZone());
  const int stepH = m_w.week ? 24 : 1;
  if (m_w.week) t = QDateTime(ls.date(), QTime(0, 0), ls.timeZone());
  p.setFont(theme::mono(10));
  // an invalid window (no data yet) gives invalid dates, which addSecs keeps invalid and which compare below every
  // valid one: the loop never ended. Capped too (a week is 168 hourly steps).
  int guard = 0;
  for (; m_w.start.isValid() && m_w.end.isValid() && t.isValid() && t <= m_w.end && ++guard < 2000; t = t.addSecs(stepH * 3600)) {
    if (t <= m_w.start) continue;
    const qreal x = xOf(t.toSecsSinceEpoch());
    p.fillRect(QRectF(x, plot.top(), 1, plot.height()), theme::map("#1e1912"));
    const QString lab = m_w.week ? i18n::locale().toString(t, "ddd d").toUpper() : t.toString("HH:mm");
    const qreal tw = QFontMetricsF(p.font()).horizontalAdvance(lab);
    p.setPen(theme::map("#504c45"));
    p.drawText(QPointF(qBound<qreal>(T.left() + 6, x - tw / 2, plot.right() - tw), T.top() + 398 + g.extra), lab);
  }
  // traced series
  if (m_traced >= 0 && m_st.value(m_traced).ok()) {
    const auto& s = m_st[m_traced];
    const auto& as = assets()[m_traced];
    double lo = qMin(s.lo, s.open), hi = qMax(s.hi, s.open);
    if (hi - lo < 1e-9) { hi += 1; lo -= 1; }
    const QRectF r(plot.left(), plot.top() + 16, plot.width(), 213 + g.extra);
    const auto pts = points(m_traced, r, lo, hi);
    const bool upS = s.chg >= 0;
    const QColor line = upS ? kPosLine() : kNegLine();
    QPainterPath path; path.moveTo(pts.first());
    for (int i = 1; i < pts.size(); ++i) path.lineTo(pts[i]);
    QPainterPath area = path; area.lineTo(pts.last().x(), plot.bottom()); area.lineTo(pts.first().x(), plot.bottom()); area.closeSubpath();
    QLinearGradient fill(0, plot.top(), 0, plot.bottom());
    fill.setColorAt(0, upS ? theme::map(15, 72, 74, 200) : theme::map(84, 24, 28, 200));
    fill.setColorAt(1, upS ? theme::map(15, 49, 49, 30) : theme::map(50, 14, 16, 30));
    p.fillPath(area, fill);
    // open line
    const qreal yo = r.bottom() - (s.open - lo) / (hi - lo) * r.height();
    p.setPen(QPen(theme::map("#414441"), 1, Qt::DashLine)); p.drawLine(QPointF(plot.left(), yo), QPointF(plot.right(), yo));
    p.setPen(QPen(line, 1.5)); p.setBrush(Qt::NoBrush); p.drawPath(path);
    p.setPen(Qt::NoPen); p.setBrush(line); p.drawEllipse(pts.last(), 4.5, 4.5);
    {   // OPEN label after the line, on a dark backing: the line starts at the open price right under it
      const QFont f = theme::mono(10, QFont::Normal, 1.5);
      const QString t = QCoreApplication::translate("wire", "OPEN %1").arg(fmt::num(s.open, as.decimals));
      const QFontMetricsF fm(f);
      const QRectF box(plot.left() + 6, yo - 6 - fm.ascent() - 1, fm.horizontalAdvance(t) + 6, fm.height() + 1);
      p.fillRect(box, theme::map(10, 9, 7, 215));
      p.setFont(f); p.setPen(theme::map("#6a736b"));
      p.drawText(QPointF(box.left() + 3, yo - 6), t);
    }
    // axis: window max / min
    p.setFont(theme::mono(11)); p.setPen(theme::map("#7b7972"));
    p.drawText(QPointF(plot.right() + 7, r.top() + 4), fmt::num(hi, as.decimals));
    p.drawText(QPointF(plot.right() + 7, r.bottom() + 4), fmt::num(lo, as.decimals));
    // hover
    const int hi_ = hoverIndex();
    if (hi_ >= 0 && hi_ < pts.size()) {
      const QPointF hp = pts[hi_];
      p.setPen(QPen(theme::map(220, 218, 210, 150), 1)); p.drawLine(QPointF(m_hoverX, plot.top()), QPointF(m_hoverX, T.top() + 383 + m_geo.extra));
      p.setPen(Qt::NoPen); p.setBrush(line); p.drawEllipse(hp, 3.5, 3.5);
      const double v = hi_ == 0 ? s.open : s.c[hi_ - 1];
      const qint64 tt = hi_ == 0 ? s.t.first() : s.t[hi_ - 1] + 900;
      const QString tlab = QDateTime::fromSecsSinceEpoch(tt, QTimeZone::UTC).toLocalTime().toString(m_w.week ? QStringLiteral("ddd HH:mm") : QStringLiteral("HH:mm")) + " " +
                           wire::utcLabel(m_w.start.toLocalTime());
      const QString vlab = fmt::num(v, as.decimals), clab = fmt::pct((v / s.open - 1) * 100);
      const QFont f1 = theme::mono(11), f2 = theme::mono(11, QFont::Bold);
      const qreal w = QFontMetricsF(f1).horizontalAdvance(tlab + "  ") + QFontMetricsF(f2).horizontalAdvance(vlab + "  ") +
                      QFontMetricsF(f1).horizontalAdvance(clab) + 18;
      QRectF box(m_hoverX + 8, plot.top() + 5, w, 23);
      if (box.right() > T.right() - 4) box.moveRight(m_hoverX - 8);
      p.setPen(QPen(theme::map("#262626"), 1)); p.setBrush(theme::map(12, 12, 12, 235)); p.drawRect(box.adjusted(.5, .5, -.5, -.5));
      qreal x = box.left() + 9; const qreal by = box.center().y() + 4;
      p.setFont(f1); p.setPen(theme::map("#7b7972")); p.drawText(QPointF(x, by), tlab); x += QFontMetricsF(f1).horizontalAdvance(tlab + "  ");
      p.setFont(f2); p.setPen(theme::map("#fefdf8")); p.drawText(QPointF(x, by), vlab); x += QFontMetricsF(f2).horizontalAdvance(vlab + "  ");
      p.setFont(f1); p.setPen(signText(v - s.open)); p.drawText(QPointF(x, by), clab);
    }
  } else {
    p.setFont(theme::mono(11, QFont::Normal, 1)); p.setPen(theme::map("#5b5750"));
    p.drawText(plot, Qt::AlignCenter | Qt::TextWordWrap,
               m_loading ? QCoreApplication::translate("wire", "Loading 15-minute bars…") : QCoreApplication::translate("wire", "No 15-minute bars for this window (market holiday, or older than Yahoo's 60 days)."));
  }
  // releases lane
  const qreal ry = T.top() + 339 + g.extra;
  p.setFont(theme::mono(9, QFont::Normal, 2)); p.setPen(theme::map("#4e4a42"));
  p.drawText(QRectF(T.right() - 120, ry - 8, 107, 14), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "RELEASES"));
  QMap<qint64, QPair<int, int>> byTime;   // time → (count, max impact)
  for (const auto& r : m_rel) { auto& v = byTime[r.time.toSecsSinceEpoch()]; v.first++; v.second = qMax(v.second, r.impact); }
  for (auto it = byTime.cbegin(); it != byTime.cend(); ++it) {
    const QPointF c(xOf(it.key()), ry);
    QPainterPath d; d.moveTo(c + QPointF(0, -6)); d.lineTo(c + QPointF(6, 0)); d.lineTo(c + QPointF(0, 6)); d.lineTo(c + QPointF(-6, 0)); d.closeSubpath();
    p.fillPath(d, it.value().second >= 3 ? theme::map("#e8da9b") : theme::map("#52565d"));
    if (it.value().first > 1) {
      p.setFont(theme::mono(9)); p.setPen(theme::map("#726f68"));
      p.drawText(QPointF(c.x() + 7, c.y() - 5), QString::number(it.value().first));
    }
  }
  if (!m_connected) {
    p.setFont(theme::mono(9, QFont::Normal, 1)); p.setPen(theme::map("#45423b"));
    p.drawText(QPointF(plot.left() + 4, ry + 4), QCoreApplication::translate("wire", "calendar connecting…"));
  }
  // wire lane
  const qreal wb = T.top() + 381 + g.extra;
  p.setFont(theme::mono(9, QFont::Normal, 2)); p.setPen(theme::map("#4e4a42"));
  p.drawText(QRectF(T.right() - 120, wb - 16, 107, 14), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "WIRE"));
  QMap<qint64, int> buckets;
  const qint64 bs = m_w.bucketMinutes() * 60;
  for (const auto& it : m_wire) buckets[it.time.toSecsSinceEpoch() / bs]++;
  int maxN = 1;
  for (int n : buckets) maxN = qMax(maxN, n);
  p.setFont(theme::mono(9));
  for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) {
    const qreal x = xOf(it.key() * bs + bs / 2);
    const qreal h = 9 + 9 * std::sqrt(it.value() / double(maxN));
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#a2a1a1"));
    p.drawRoundedRect(QRectF(x - 1.5, wb - h, 3, h), 1.5, 1.5);
    if (it.value() >= 2) {   // single headlines carry no number in the reference
      p.setPen(theme::map("#726f68"));
      p.drawText(QPointF(x + 2, wb - h - 2), QString::number(it.value()));
    }
  }
  if (m_wire.isEmpty()) {
    p.setFont(theme::mono(9, QFont::Normal, 1)); p.setPen(theme::map("#45423b"));
    p.drawText(QPointF(plot.left() + 4, wb - 4), QCoreApplication::translate("wire", "no headline archived for this window"));
  }
}

void CentreView::paintGrid(QPainter& p) {
  const auto& g = m_geo;
  p.setFont(theme::mono(10, QFont::Normal, 2.3)); p.setPen(theme::map("#595953"));
  const qreal ty = g.timeline.bottom() + 27;
  const QString movesT = QCoreApplication::translate("wire", "MOVES OF THE WINDOW");
  p.drawText(QPointF(g.grid.left(), ty), movesT);
  const qreal tw = QFontMetricsF(p.font()).horizontalAdvance(movesT + " ");
  p.setFont(theme::mono(10)); p.setPen(theme::map("#51514e"));
  p.drawText(QPointF(g.grid.left() + tw + 2, ty), m_w.week ? QCoreApplication::translate("wire", "15-min bars, Mon–Fri · open ⟶ close · click to trace")
                                                        : QCoreApplication::translate("wire", "15-min bars · open ⟶ close · click to trace"));
  for (int i = 0; i < g.cards.size() && i < assets().size(); ++i) {
    const QRect c = g.cards[i];
    const auto& a = assets()[i];
    const Stat s = m_st.value(i);
    const bool sel = i == m_traced;
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#0d0802")); p.drawRect(c);
    p.setPen(QPen(sel ? theme::map("#d2c38d") : theme::map("#3f392a"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(c).adjusted(.5, .5, -.5, -.5));
    QFont fn = theme::condensed(14, QFont::Bold); fn.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    p.setFont(fn); p.setPen(theme::map("#fefdf8"));
    p.drawText(QPointF(c.left() + 10, c.top() + 26), assetName(i).toUpper());
    p.setFont(theme::mono(14, QFont::Bold));
    p.setPen(s.ok() ? (s.chg >= 0 ? kPosCard() : kNegCard()) : theme::map("#5f5d57"));
    p.drawText(QRectF(c.left(), c.top() + 10, c.width() - 10, 20), Qt::AlignRight | Qt::AlignVCenter, pctText(s.chg));
    const QRectF sr(c.left() + 10, c.top() + 38, c.width() - 20, c.height() - 73);
    if (s.ok()) {
      const double lo = qMin(s.lo, s.open), hi = qMax(s.hi, s.open) + (qMax(s.hi, s.open) - lo < 1e-12 ? 1 : 0);
      const auto pts = points(i, sr, lo, hi);
      const bool up = s.chg >= 0;
      const QColor lc = up ? kPosSpark() : kNegSpark();
      QPainterPath path; path.moveTo(pts.first());
      for (int k = 1; k < pts.size(); ++k) path.lineTo(pts[k]);
      QPainterPath area = path; area.lineTo(pts.last().x(), sr.bottom() + 4); area.lineTo(pts.first().x(), sr.bottom() + 4); area.closeSubpath();
      QLinearGradient f(0, sr.top(), 0, sr.bottom() + 4);
      f.setColorAt(0, up ? theme::map(18, 70, 74, 170) : theme::map(80, 26, 28, 170)); f.setColorAt(1, theme::map(13, 8, 2, 0));
      p.fillPath(area, f);
      const qreal yo = sr.bottom() - (s.open - lo) / (hi - lo) * sr.height();
      p.setPen(QPen(theme::map("#3a3a3a"), 1, Qt::DotLine)); p.drawLine(QPointF(sr.left(), yo), QPointF(sr.right(), yo));
      p.setPen(QPen(lc, 1.3)); p.setBrush(Qt::NoBrush); p.drawPath(path);
      p.setPen(Qt::NoPen); p.setBrush(up ? lc : theme::map("#fe8d8f")); p.drawEllipse(pts.last(), 3, 3);
    } else {
      p.setFont(theme::mono(9, QFont::Normal, 1)); p.setPen(theme::map("#45423b"));
      p.drawText(sr, Qt::AlignCenter, m_loading ? QCoreApplication::translate("wire", "loading…") : QCoreApplication::translate("wire", "no bars"));
    }
    p.setFont(theme::mono(11));
    p.setPen(theme::map("#545049")); p.drawText(QPointF(c.left() + 10, c.bottom() - 11), QCoreApplication::translate("wire", "O", "open") + " " + (s.ok() ? fmt::num(s.open, a.decimals) : fmt::dash()));
    p.setPen(theme::map("#7d7a74"));
    p.drawText(QRectF(c.left(), c.bottom() - 23, c.width() - 10, 16), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "C", "close") + " " + (s.ok() ? fmt::num(s.close, a.decimals) : fmt::dash()));
  }
}

// ---------------------------------------------------------------------------------------------- input
void CentreView::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pt = e->position() + QPointF(0, offset());
  const QRectF zone = QRectF(m_geo.plot).adjusted(-4, -4, 4, 64);
  const int hx = zone.contains(pt) ? int(pt.x()) : -1;
  if (hx != m_hoverX) { m_hoverX = hx; updateTimeline(); }
  bool hand = m_geo.en.contains(pt) || m_geo.fr.contains(pt);
  for (const auto& r : m_geo.chips) hand |= r.contains(pt);
  for (const auto& r : m_geo.cards) hand |= r.contains(pt.toPoint());
  viewport()->setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void CentreView::leaveEvent(QEvent*) { if (m_hoverX >= 0) { m_hoverX = -1; updateTimeline(); } }
void CentreView::mousePressEvent(QMouseEvent* e) {
  const QPointF pt = e->position() + QPointF(0, offset());
  if (m_geo.en.contains(pt) && m_lang != "EN") return setLang("EN");   // session only
  if (m_geo.fr.contains(pt) && m_lang != "FR") return setLang("FR");
  const auto ch = chipAssets();
  for (int k = 0; k < m_geo.chips.size(); ++k) if (m_geo.chips[k].contains(pt)) return setTraced(ch[k]);
  for (int k = 0; k < m_geo.cards.size(); ++k) if (m_geo.cards[k].contains(pt.toPoint()) && m_st.value(k).ok()) return setTraced(k);
}
}
