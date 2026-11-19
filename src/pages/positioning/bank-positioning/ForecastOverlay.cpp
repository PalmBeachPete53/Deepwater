#include "pages/news/bank-research/Look.h"
#include "pages/positioning/bank-positioning/ForecastOverlay.h"
#include "pages/positioning/bank-positioning/ForecastChart.h"
#include "banks/Registry.h"
#include "market/QuoteHub.h"
#include "market/Candles.h"
#include "ui/Theme.h"
#include "ui/Kit.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QMenu>
#include <cmath>

namespace banks::pos {

QColor quarterColor(int i) {
  static const char* c[] = {"#fcd400", "#20d8f0", "#b4a0f8", "#24c870", "#fc9c24", "#f48cc4"};
  return banks::seriesColour(c[qBound(0, i, 5)], false);
}
QColor quarterTextColor(int i) {
  static const char* c[] = {"#eada9a", "#82c4ca", "#b7add1", "#75b087", "#d1a781", "#cda2b2"};
  return banks::seriesColour(c[qBound(0, i, 5)], true);
}

namespace {
const banks::DwColor kBackdrop{"#040302"}, kPanel{"#0c0601"}, kBorder{"#45412f"}, kBracket{"#8c7f45"}, kSep{"#1e1a0f"};
const banks::DwColor kPillText{"#98958d"}, kPillBorder{"#2b2a1d"}, kYellow{"#fcd808"}, kDark{"#0c0601"};
constexpr qreal kHeadH = 45, kRowPitch = 35, kTableW = 785;
QString pairText(const QString& id) { const banks::Market* m = banks::market(id); return m ? m->display : id; }
int decimalsOf(const QString& id) { const banks::Market* m = banks::market(id); return m ? m->decimals : 4; }
}

ForecastOverlay::ForecastOverlay(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  m_chart = new ForecastChart(this);
  m_chart->setAttribute(Qt::WA_TransparentForMouseEvents);
  hide();
}

QRectF ForecastOverlay::panelRect() const {
  // 496 px at the reference size (page 966 px); taller pages give the chart 1.5 × the extra height (≤ page − 55)
  const qreal grow = std::max<qreal>(0, height() - 966) * 1.5;
  const qreal h = std::max<qreal>(496 + grow, kHeadH + 30 + (m_rows.size() + 1) * kRowPitch + 40);
  return QRectF(14, 41, width() - 28, std::min<qreal>(h, height() - 55));
}
QRectF ForecastOverlay::closeRect() const { return QRectF(width() - 83, 12, 68, 22); }

// Pair pills, right-aligned like the reference. When they do not all fit, the first ones (most covered) are shown
// and a "+N" pill at the right opens a menu with the others (moreRect()).
QList<QRectF> ForecastOverlay::pairRects() const {
  const QFontMetricsF fm(theme::mono(11, QFont::Medium, 0.3));
  const QRectF pr = panelRect();
  // pills start after the title and subtitle (540 px in English, further in French)
  const qreal head = pr.left() + 11 + QFontMetricsF(theme::display(20, QFont::Bold, 0.8)).horizontalAdvance(QCoreApplication::translate("banks", "FX BANK FORECASTS"))
                   + 12 + QFontMetricsF(theme::mono(10, QFont::Normal, 2.2)).horizontalAdvance(QCoreApplication::translate("banks", "INSTITUTIONAL PRICE PROJECTIONS")) + 24;
  const qreal minX = qMax(pr.left() + 540, head), right = pr.right() - 11;
  QList<qreal> ws; qreal total = 0;
  for (const auto& id : m_pairs) { ws << fm.horizontalAdvance(pairText(id)) + 16; total += ws.last() + 5; }
  int n = int(m_pairs.size());
  qreal avail = right - minX;
  if (total > avail) {
    avail -= 46;                          // room for the "+N" pill
    qreal acc = 0; n = 0;
    while (n < ws.size() && acc + ws[n] + 5 <= avail) { acc += ws[n] + 5; ++n; }
  }
  QList<QRectF> out;
  qreal x = right - (n < m_pairs.size() ? 46 : 0);
  for (int i = n - 1; i >= 0; --i) { out.prepend(QRectF(x - ws[i], pr.top() + 10, ws[i], 26)); x -= ws[i] + 5; }
  return out;
}
QRectF ForecastOverlay::moreRect() const {
  if (pairRects().size() >= m_pairs.size()) return {};
  const QRectF pr = panelRect();
  return QRectF(pr.right() - 11 - 41, pr.top() + 10, 41, 26);
}

QList<QRectF> ForecastOverlay::quarterRects() const {
  QList<QRectF> out;
  const QRectF pr = panelRect();
  qreal x = pr.right() - 11;
  const bool compact = pr.width() < 1200;      // small laptops: smaller pills so the pair and price stay readable
  const QFontMetricsF fm(theme::mono(compact ? 10 : 11, QFont::Medium));
  for (int i = int(m_quarters.size()) - 1; i >= 0; --i) {
    const qreal w = fm.horizontalAdvance(m_quarters[i].label()) + (compact ? 8 : 14);
    out.prepend(QRectF(x - w, pr.top() + kHeadH + 8, w, 23));
    x -= w + 5;
  }
  return out;
}

// Table | chart split: half and half as in the reference, but the table stops at 900 px on wide screens
// (it has one row per bank) and the chart takes the rest.
static qreal splitX(const QRectF& pr) { return pr.left() + qMin<qreal>(pr.width() / 2, 900); }

QString ForecastOverlay::footnote() const {
  return m_bank.isEmpty()
      ? QCoreApplication::translate("banks", "LINES = AVERAGE OF THE BANKS' FORECASTS PER QUARTER (CLICK A BANK FOR ITS OWN) · FORECASTS AS WRITTEN IN EACH BANK'S TABLE")
      : QCoreApplication::translate("banks", "LINES = THIS BANK'S OWN FORECASTS (CLICK THE ROW AGAIN FOR THE AVERAGES) · FORECASTS AS WRITTEN IN THE BANK'S TABLE");
}
bool ForecastOverlay::footnoteTwoLines() const {
  const QRectF pr = panelRect();
  return QFontMetricsF(theme::mono(9, QFont::Normal, 0.8)).horizontalAdvance(footnote()) > pr.right() - splitX(pr) - 30;
}

QRectF ForecastOverlay::rowRect(int i) const {
  const QRectF pr = panelRect();
  return QRectF(pr.left() + 1, pr.top() + kHeadH + 31 + i * kRowPitch, splitX(pr) - pr.left() - 1, kRowPitch);
}

void ForecastOverlay::resizeEvent(QResizeEvent*) {
  const QRectF pr = panelRect();
  const qreal cx = splitX(pr);
  const int foot = footnoteTwoLines() ? 42 : 30;      // narrow chart / long text: the footnote wraps on two lines
  m_chart->setGeometry(QRect(int(cx), int(pr.top() + kHeadH + 39), int(pr.right() - cx), int(pr.bottom() - (pr.top() + kHeadH + 39) - foot)));
}

void ForecastOverlay::open(const QString& pair) {
  m_pairs = banks::Forecasts::instance().fxPairs();
  m_quarters = banks::Quarter::upcoming(6);
  m_qOn = QList<bool>(m_quarters.size(), true);
  m_pair = pair.isEmpty() ? m_pairs.value(0) : pair;
  m_bank.clear();
  show(); raise(); setFocus();
  resizeEvent(nullptr);
  loadPair(true);
}

void ForecastOverlay::showEvent(QShowEvent*) { ::market::QuoteHub::instance().setActive(this, true); }
void ForecastOverlay::hideEvent(QHideEvent*) { ::market::QuoteHub::instance().setActive(this, false); }

void ForecastOverlay::refresh() {
  if (!isVisible()) return;
  if (m_pairs.isEmpty()) m_pairs = banks::Forecasts::instance().fxPairs();
  if (m_pair.isEmpty() && !m_pairs.isEmpty()) { m_pair = m_pairs.first(); loadPair(true); }
  const banks::Market* m = banks::market(m_pair);
  if (m) {
    const auto q = ::market::QuoteHub::instance().quote(m->spot);
    if (q.valid()) { m_last = q.price; m_chart->setLast(m_last); }
  }
  // forecasts may have changed: the table rows and the quarter lines are rebuilt from them (not only on a pair change)
  m_rows = banks::Forecasts::instance().banksFor(m_pair);
  if (!m_bank.isEmpty() && !m_rows.contains(m_bank)) m_bank.clear();
  updateLines(false);
  update();
}

void ForecastOverlay::loadPair(bool animate) {
  m_rows = banks::Forecasts::instance().banksFor(m_pair);
  if (!m_bank.isEmpty() && !m_rows.contains(m_bank)) m_bank.clear();
  const banks::Market* m = banks::market(m_pair);
  m_last = qQNaN();
  m_chart->setBars({}, decimalsOf(m_pair));
  m_chart->setLast(qQNaN());
  ::market::QuoteHub::instance().unsubscribe(this);   // subscribe() adds: keep only the shown pair's spot
  if (m) {
    ::market::QuoteHub::instance().subscribe(this, {m->spot}, isVisible());
    const auto q = ::market::QuoteHub::instance().quote(m->spot);
    if (q.valid()) m_last = q.price;
    const QString id = m_pair;
    ::market::candles(m->spot, "1d", "1y", this, [this, id](const ::market::Series& s) {
      if (id != m_pair) return;
      if (!s.ok()) { m_chart->setMessage(QCoreApplication::translate("banks", "PRICE HISTORY UNAVAILABLE · %1").arg(s.error.toUpper())); return; }
      m_chart->setBars(s.bars, decimalsOf(id));
      if (!std::isfinite(m_last)) m_last = s.bars.last().c;
      m_chart->setLast(m_last);
      m_chart->animateIn();
      update();
    });
  }
  resizeEvent(nullptr);
  updateLines(animate);
  update();
}

void ForecastOverlay::updateLines(bool animate) {
  resizeEvent(nullptr);    // the footnote may change length (averages ↔ one bank)
  QList<ForecastChart::Line> lines;
  const QLocale en(QLocale::English);
  const int dec = decimalsOf(m_pair);
  const auto all = banks::Forecasts::instance().forecasts(m_pair);
  for (int i = 0; i < m_quarters.size(); ++i) {
    if (!m_qOn.value(i)) continue;
    double sum = 0; int n = 0;
    for (const auto& f : all) {
      if (!(f.quarter == m_quarters[i])) continue;
      if (!m_bank.isEmpty() && f.bankId != m_bank) continue;
      sum += f.value; ++n;
    }
    if (!n) continue;
    const double avg = sum / n;
    QString label = QString("%1 %2").arg(m_quarters[i].label(), en.toString(avg, 'f', dec));
    if (!m_bank.isEmpty()) { const banks::Bank* b = banks::bank(m_bank); label += QString::fromUtf8(" · ") + (b ? b->shortName : m_bank); }
    else if (n > 1) label += QCoreApplication::translate("banks", " · %1 banks").arg(n);
    lines << ForecastChart::Line{label, avg, quarterColor(i)};
  }
  // quarters with the same value share one line and one label ("Q4 '26 / Q1 '27 1.1300")
  QList<ForecastChart::Line> merged; QList<QStringList> names; QStringList tails;
  for (const auto& l : lines) {
    const QString q = l.label.section(' ', 0, 1), tail = l.label.section(' ', 2);
    int found = -1;
    for (int k = 0; k < merged.size(); ++k) if (std::abs(merged[k].value - l.value) < 1e-9) found = k;
    if (found < 0) { merged << l; names << QStringList{q}; tails << tail; }
    else names[found] << q;
  }
  for (int k = 0; k < merged.size(); ++k) merged[k].label = names[k].join(" / ") + " " + tails[k];
  lines = merged;
  // draw order: highest value first so lower labels are not hidden by the boxes above them
  std::sort(lines.begin(), lines.end(), [](const auto& a, const auto& b) { return a.value > b.value; });
  m_chart->setLines(lines);
  if (std::isfinite(m_last)) m_chart->setLast(m_last);
  if (animate) m_chart->animateIn();
}

void ForecastOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
  p.fillRect(rect(), kBackdrop);
  // close
  const QRectF cr = closeRect();
  p.setPen(QPen(m_hoverClose ? theme::map("#5a5440") : kPillBorder, 1)); p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(cr.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  p.setFont(theme::mono(11, QFont::Normal, 1.2)); p.setPen(m_hoverClose ? theme::map("#d8d4c8") : theme::map("#81817d"));
  p.drawText(cr, Qt::AlignCenter, QCoreApplication::translate("banks", "× CLOSE"));

  const QRectF pr = panelRect();
  p.fillRect(pr, kPanel);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(kBorder); p.drawRect(pr.adjusted(0, 0, -1, -1));
  ui::drawBrackets(p, pr.adjusted(3, 3, -3, -3), kBracket, 8, true, true);
  // header
  QFont tf = theme::display(20, QFont::Bold, 0.8);
  p.setFont(tf); p.setPen(theme::map("#fffdf8"));
  const QString title = QCoreApplication::translate("banks", "FX BANK FORECASTS");
  p.drawText(QPointF(pr.left() + 11, pr.top() + 29), title);
  const qreal tw = QFontMetricsF(tf).horizontalAdvance(title);
  p.setFont(theme::mono(10, QFont::Normal, 2.2)); p.setPen(theme::map("#5b5750"));
  p.drawText(QPointF(pr.left() + 11 + tw + 12, pr.top() + 27), QCoreApplication::translate("banks", "INSTITUTIONAL PRICE PROJECTIONS"));
  p.setRenderHint(QPainter::Antialiasing, true);
  const auto pills = pairRects();
  const QFont pf = theme::mono(11, QFont::Medium, 0.3);
  for (int i = 0; i < pills.size(); ++i) {
    const bool on = m_pairs[i] == m_pair;
    if (on) { p.setPen(Qt::NoPen); p.setBrush(kYellow); p.drawRoundedRect(pills[i], 2, 2); }
    else { p.setPen(QPen(i == m_hoverPill ? theme::map("#4a4630") : kPillBorder, 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(pills[i].adjusted(0.5, 0.5, -0.5, -0.5), 2, 2); }
    p.setFont(on ? theme::mono(11, QFont::Bold, 0.3) : pf); p.setPen(on ? (theme::isDefault() ? kDark.dw() : theme::onAccent()) : QColor(kPillText));
    p.drawText(pills[i], Qt::AlignCenter, pairText(m_pairs[i]));
  }
  if (const QRectF mr = moreRect(); !mr.isNull()) {
    p.setPen(QPen(QColor(kPillBorder), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(mr.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p.setFont(pf); p.setPen(kPillText);
    p.drawText(mr, Qt::AlignCenter, QString("+%1").arg(m_pairs.size() - pills.size()));
  }
  p.setRenderHint(QPainter::Antialiasing, false);
  p.fillRect(QRectF(pr.left() + 1, pr.top() + kHeadH, pr.width() - 2, 1), theme::map("#2d2818"));
  const qreal cx = splitX(pr);
  p.fillRect(QRectF(cx - 1, pr.top() + kHeadH + 1, 1, pr.height() - kHeadH - 2), theme::map("#2a2516"));

  // table
  const qreal tl = pr.left() + 1, f = (cx - 1 - tl) / kTableW;
  // narrow table (< 640 px): LAST REVISED moves under the bank name, quarter columns share the rest evenly
  const bool narrowT = cx - 1 - tl < 640;
  auto colRight = [&](int c) {
    if (!narrowT) return tl + (c == 0 ? 285 : 285 + 82 * c) * f;     // 0 = LAST REVISED, 1..6 quarters
    const qreal w0 = 118, step = (cx - 1 - tl - w0 - 8) / 6.0;
    return tl + w0 + step * c;
  };
  const qreal hy = pr.top() + kHeadH + 16;
  const QFont hf = theme::mono(9, QFont::Normal, 1.8);
  p.setFont(hf);
  p.setPen(theme::map("#5e5a52"));
  p.drawText(QPointF(tl + 10, hy + 4), QCoreApplication::translate("banks", "INSTITUTION"));
  if (!narrowT) p.drawText(QRectF(colRight(0) - 120, hy - 8, 120, 16), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("banks", "LAST REVISED"));
  for (int q = 0; q < m_quarters.size(); ++q) {
    p.setPen(quarterTextColor(q)); p.setFont(theme::mono(10, QFont::Bold, 1.0));
    p.drawText(QRectF(colRight(q + 1) - 80, hy - 8, 80, 16), Qt::AlignRight | Qt::AlignVCenter, m_quarters[q].label().toUpper());
  }
  const auto all = banks::Forecasts::instance().forecasts(m_pair);
  const int dec = decimalsOf(m_pair);
  const QLocale en(QLocale::English);
  for (int r = 0; r < m_rows.size(); ++r) {
    const QRectF rr = rowRect(r);
    const QString id = m_rows[r];
    if (id == m_bank) { p.fillRect(rr, theme::map("#1b1503")); p.fillRect(QRectF(rr.left(), rr.top(), 2, rr.height()), kYellow); }
    p.fillRect(QRectF(rr.left(), rr.bottom(), rr.width(), 1), kSep);
    const banks::Bank* b = banks::bank(id);
    p.drawPixmap(QPointF(tl + 10, rr.center().y() - 6), ui::flag(b ? b->country : "eu", QSize(18, 12), devicePixelRatioF()));
    p.setFont(theme::sans(12)); p.setPen(theme::map("#e0ddd8"));
    QDate rev;
    for (const auto& x : all) if (x.bankId == id && x.revised > rev) rev = x.revised;
    const QString revT = rev.isValid() ? i18n::locale().toString(rev, "MMM dd").toUpper() : QString::fromUtf8("—");
    if (narrowT) {
      p.drawText(QRectF(tl + 35, rr.top() + 3, 90, 16), Qt::AlignVCenter | Qt::AlignLeft, b ? b->legalName.section(' ', 0, 0) : id);
      p.setFont(theme::mono(9)); p.setPen(theme::map("#8c8881"));
      p.drawText(QRectF(tl + 35, rr.top() + 18, 90, 13), Qt::AlignVCenter | Qt::AlignLeft, QCoreApplication::translate("banks", "REV. %1").arg(revT));
    } else {
      p.drawText(QRectF(tl + 35, rr.top(), 200, rr.height()), Qt::AlignVCenter | Qt::AlignLeft, b ? b->legalName.section(' ', 0, 0) : id);
      p.setFont(theme::mono(12)); p.setPen(theme::map("#8c8881"));
      p.drawText(QRectF(colRight(0) - 120, rr.top(), 120, rr.height()), Qt::AlignVCenter | Qt::AlignRight, revT);
    }
    for (int q = 0; q < m_quarters.size(); ++q) {
      double v = qQNaN();
      for (const auto& x : all) if (x.bankId == id && x.quarter == m_quarters[q]) v = x.value;
      const QRectF cell(colRight(q + 1) - 80, rr.top(), 80, rr.height());
      if (std::isfinite(v)) { p.setFont(theme::mono(13)); p.setPen(theme::map("#fffdfa")); p.drawText(cell, Qt::AlignVCenter | Qt::AlignRight, en.toString(v, 'f', dec)); }
      else { p.setFont(theme::mono(12)); p.setPen(theme::map("#5e5b53")); p.drawText(cell, Qt::AlignVCenter | Qt::AlignRight, QString::fromUtf8("—")); }
    }
  }
  if (m_rows.isEmpty()) {
    p.setFont(theme::mono(10, QFont::Normal, 2)); p.setPen(theme::map("#5b5750"));
    p.drawText(rowRect(0).adjusted(10, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
               banks::Forecasts::instance().loaded() ? QCoreApplication::translate("banks", "NO BANK FORECAST FOR THIS PAIR") : QCoreApplication::translate("banks", "LOADING THE BANKS' FORECASTS"));
  }
  // note under the table: who is read, who is not
  p.setFont(theme::mono(9, QFont::Normal, 1.0)); p.setPen(theme::map("#4a473f"));
  const QRectF note(tl + 10, pr.bottom() - 74, cx - tl - 20, 66);
  p.drawText(note, Qt::TextWordWrap | Qt::AlignBottom | Qt::AlignLeft,
             QCoreApplication::translate("banks", "FORECASTS READ FROM ING THINK (© ING BANK N.V., REUSE WITH ATTRIBUTION). TD, MUFG, KBC, "
                               "SCOTIABANK AND COMMERZBANK PUBLISH FX TABLES TOO, BUT THEIR TERMS FORBID REUSE OR AUTOMATED "
                               "ACCESS: NOT INCLUDED. QUARTER-END VALUES (EOP). LINE AVERAGES COMPUTED BY DEEPWATER."));

  // chart header
  p.setRenderHint(QPainter::Antialiasing, true);
  const banks::Market* m = banks::market(m_pair);
  const qreal chy = pr.top() + kHeadH + 20;
  if (m) p.drawPixmap(QPointF(cx + 11, chy - 6), ui::flag(m_pair.left(3), QSize(22, 14), devicePixelRatioF()));
  QFont nf = theme::sans(15, QFont::Bold);
  p.setFont(nf); p.setPen(theme::map("#fffefb"));
  const QString pt = pairText(m_pair);
  p.drawText(QPointF(cx + 41, chy + 6), pt);
  p.setFont(theme::mono(14)); p.setPen(theme::map("#97938d"));
  if (std::isfinite(m_last)) p.drawText(QPointF(cx + 41 + QFontMetricsF(nf).horizontalAdvance(pt) + 8, chy + 6), en.toString(m_last, 'f', dec));
  const auto qr = quarterRects();
  for (int i = 0; i < qr.size(); ++i) {
    const bool on = m_qOn.value(i);
    p.setPen(on ? Qt::NoPen : QPen(theme::alpha(quarterColor(i), 120), 1));
    p.setBrush(on ? quarterColor(i) : Qt::NoBrush);
    p.drawRoundedRect(qr[i].adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p.setFont(theme::mono(panelRect().width() < 1200 ? 10 : 11, QFont::Medium)); p.setPen(on ? banks::inkOn(quarterColor(i), "#141008") : theme::alpha(quarterColor(i), 170));
    p.drawText(qr[i], Qt::AlignCenter, m_quarters[i].label());
  }
  // footnote
  p.setFont(theme::mono(9, QFont::Normal, 0.8)); p.setPen(theme::map("#555149"));
  {
    const QString fn = footnote();
    const bool two = footnoteTwoLines();   // two lines instead of eliding
    p.drawText(QRectF(cx + 10, pr.bottom() - (two ? 38 : 26), pr.right() - cx - 30, two ? 30 : 16),
               Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, fn);
  }
}

void ForecastOverlay::mouseMoveEvent(QMouseEvent* e) {
  const bool hc = closeRect().contains(e->position());
  int hp = -1; const auto pills = pairRects();
  for (int i = 0; i < pills.size(); ++i) if (pills[i].contains(e->position())) hp = i;
  bool hand = hc || hp >= 0 || moreRect().contains(e->position());
  for (const auto& q : quarterRects()) if (q.contains(e->position())) hand = true;
  for (int r = 0; r < m_rows.size(); ++r) if (rowRect(r).contains(e->position())) hand = true;
  setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (hc != m_hoverClose || hp != m_hoverPill) { m_hoverClose = hc; m_hoverPill = hp; update(); }
}

void ForecastOverlay::mousePressEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  if (closeRect().contains(pos)) { hide(); ::market::QuoteHub::instance().unsubscribe(this); if (onClose) onClose(); return; }
  const auto pills = pairRects();
  for (int i = 0; i < pills.size(); ++i) if (pills[i].contains(pos)) { m_pair = m_pairs[i]; m_bank.clear(); loadPair(true); return; }
  if (const QRectF mr = moreRect(); !mr.isNull() && mr.contains(pos)) {
    QMenu menu(this);
    const int shown = int(pills.size());
    for (int i = shown; i < m_pairs.size(); ++i) menu.addAction(pairText(m_pairs[i]))->setData(i);
    if (QAction* a = menu.exec(mapToGlobal(mr.bottomLeft().toPoint() + QPoint(0, 2)))) {
      const int i = a->data().toInt();
      const int slot = std::max(0, shown - 1);   // no pill fits (narrow window): the first slot, not index -1
      m_pairs.move(i, slot);              // the chosen pair takes the last visible slot
      m_pair = m_pairs[slot]; m_bank.clear(); loadPair(true);
    }
    return;
  }
  const auto qr = quarterRects();
  for (int i = 0; i < qr.size(); ++i) if (qr[i].contains(pos)) { m_qOn[i] = !m_qOn[i]; updateLines(false); update(); return; }
  for (int r = 0; r < m_rows.size(); ++r) if (rowRect(r).contains(pos)) {
    m_bank = m_bank == m_rows[r] ? QString() : m_rows[r];
    updateLines(true); update(); return;
  }
}

void ForecastOverlay::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape) { hide(); ::market::QuoteHub::instance().unsubscribe(this); if (onClose) onClose(); return; }
  QWidget::keyPressEvent(e);
}
}  // namespace banks::pos
