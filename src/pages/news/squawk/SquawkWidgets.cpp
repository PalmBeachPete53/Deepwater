#include "pages/news/squawk/SquawkWidgets.h"
#include "ui/Kit.h"
#include "pages/news/squawk/SquawkLook.h"
#include "market/Instruments.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>
#include <QVariantAnimation>
#include <QUrl>

namespace wire::squawk {
namespace {
QFont headlineFont() { return theme::condensed(16, QFont::Medium); }
constexpr int kTextX = 178, kTimeX = 15, kAgeX = 88, kCoinX = 162, kRightPad = 46;

QString ageText(const QDateTime& t) {
  const qint64 s = t.secsTo(QDateTime::currentDateTimeUtc());
  if (s < 60) return QCoreApplication::translate("wire", "just now");
  if (s < 3600) return QCoreApplication::translate("wire", "%1m ago").arg(s / 60);
  if (s < 86400) return QCoreApplication::translate("wire", "%1h ago").arg(s / 3600);
  return QCoreApplication::translate("wire", "%1d ago").arg(s / 86400);
}

// Small round source coin with a glyph (bolt = headline, arrow = mover, bars = release).
void coin(QPainter& p, const QPointF& c, const wire::Item& it) {
  const auto& L = look();
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  QColor fill = L.coin, core = L.coinCore;
  if (it.kind == wire::Kind::Mover) { fill = it.changePct >= 0 ? L.up : L.down; core = fill.darker(140); }
  QRadialGradient g(c - QPointF(1.5, 1.5), 8);
  g.setColorAt(0, fill.lighter(125)); g.setColorAt(1, core);
  p.setPen(Qt::NoPen); p.setBrush(g);
  p.drawEllipse(c, 6.5, 6.5);
  p.setBrush(theme::map("#1a1203"));
  if (it.kind == wire::Kind::Headline) {
    QPainterPath b;   // bolt
    b.moveTo(c + QPointF(0.8, -4.2)); b.lineTo(c + QPointF(-2.6, 0.6)); b.lineTo(c + QPointF(-0.2, 0.6));
    b.lineTo(c + QPointF(-1.0, 4.2)); b.lineTo(c + QPointF(2.6, -0.8)); b.lineTo(c + QPointF(0.2, -0.8)); b.closeSubpath();
    p.drawPath(b);
  } else if (it.kind == wire::Kind::Mover) {
    const qreal s = it.changePct >= 0 ? -1 : 1;
    QPainterPath a; a.moveTo(c + QPointF(0, 3.6 * s)); a.lineTo(c + QPointF(-3.4, -1.2 * s)); a.lineTo(c + QPointF(3.4, -1.2 * s)); a.closeSubpath();
    p.drawPath(a);
  } else {
    p.drawRect(QRectF(c.x() - 3.2, c.y() - 0.5, 1.6, 3.5));
    p.drawRect(QRectF(c.x() - 0.8, c.y() - 3.2, 1.6, 6.2));
    p.drawRect(QRectF(c.x() + 1.6, c.y() - 1.8, 1.6, 4.8));
  }
  p.restore();
}
}

// ============================================================================================ Header
Header::Header(QWidget* parent) : QWidget(parent) {
  setFixedHeight(45);   // reference: title band 0–43, 1 px rule at 44
  setMouseTracking(true);
}
void Header::setLanguage(const QString& lang, bool available) { m_lang = lang; m_langAvailable = available; update(); }
QRectF Header::langRect() const {
  const QFontMetricsF fm(theme::mono(11, QFont::Normal, 2));
  const QString items = QCoreApplication::translate("wire", "%1 ITEMS").arg(m_count);
  const qreal right = width() - 13 - fm.horizontalAdvance(items) - 6;
  return QRectF(right - 50, 9, 50, 22);   // "文A" + gap + "FR" (46 px left "文AFR" run together)
}
void Header::paintEvent(QPaintEvent*) {
  const auto& L = look();
  QPainter p(this);
  p.fillRect(rect(), L.head);
  p.fillRect(QRect(0, 44, width(), 1), L.rule);
  p.setRenderHint(QPainter::Antialiasing);
  // broadcast icon ((•))
  const QPointF c(19, 20);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#d7c27c")); p.drawEllipse(c, 1.8, 1.8);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(theme::map("#c9b06a"), 1.3, Qt::SolidLine, Qt::RoundCap));
  for (int k = 0; k < 2; ++k) {
    const qreal r = 3.8 + k * 2.9;
    p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 135 * 16, 90 * 16);
    p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), -45 * 16, 90 * 16);
  }
  // • SQUAWK
  p.setPen(Qt::NoPen); p.setBrush(L.amberText); p.drawEllipse(QPointF(34.5, 20.5), 2.3, 2.3);
  p.setFont(theme::mono(13, QFont::Bold, 3.2)); p.setPen(L.amberText);
  p.drawText(QPointF(44, 25), "SQUAWK");
  // LIVE tag
  const QString liveT = QCoreApplication::translate("wire", "LIVE");
  const QRectF live(119, 12, QFontMetricsF(theme::mono(10, QFont::Bold)).horizontalAdvance(liveT) + 7, 16);
  p.setPen(Qt::NoPen); p.setBrush(L.amberDim); p.drawRoundedRect(live, 2, 2);
  p.setFont(theme::mono(10, QFont::Bold)); p.setPen(theme::map("#d4ac14"));
  p.drawText(live, Qt::AlignCenter, liveT);
  const qreal subX = live.right() + 7;
  // subtitle
  p.setFont(theme::condensed(12, QFont::Normal)); p.setPen(L.subtitle);
  {   // elided when the column is narrow (full text in the tooltip)
    const QString sub = QCoreApplication::translate("wire", "DATA headlines + real-time price moves + fresh data releases");
    const qreal room = langRect().left() - 12 - subX;
    const QString t = QFontMetricsF(p.font()).elidedText(sub, Qt::ElideRight, room);
    if (room > 40) p.drawText(QPointF(subX, 24.5), t);
    setToolTip(t != sub ? sub : QString());
  }
  // EN toggle
  const QRectF lr = langRect();
  p.setPen(QPen(m_hoverLang ? theme::alpha(L.amberText, 140) : L.btnBorder, 1)); p.setBrush(Qt::NoBrush);
  p.drawRect(lr.adjusted(.5, .5, -.5, -.5));
  p.setFont(theme::sans(10, QFont::DemiBold)); p.setPen(m_langAvailable ? L.btnText.lighter(115) : L.btnText);
  p.drawText(QPointF(lr.left() + 7, lr.center().y() + 4), QStringLiteral("文A"));
  p.setFont(theme::mono(11, QFont::Bold, 1)); p.setPen(m_lang == "FR" ? L.amberText : L.btnText);
  p.drawText(QPointF(lr.left() + 29, lr.center().y() + 4), m_lang);
  // N ITEMS
  p.setFont(theme::mono(11, QFont::Normal, 2)); p.setPen(L.items);
  p.drawText(QRectF(0, 0, width() - 13, height()), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("wire", "%1 ITEMS").arg(m_count));
}
void Header::mousePressEvent(QMouseEvent* e) { if (langRect().contains(e->position())) emit languageToggled(); }
void Header::mouseMoveEvent(QMouseEvent* e) {
  const bool h = langRect().contains(e->position());
  if (h != m_hoverLang) {
    m_hoverLang = h; update();
    setCursor(h ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }
  if (h) QToolTip::showText(e->globalPosition().toPoint(), m_langAvailable
      ? QCoreApplication::translate("wire", "Translate headlines (EN ⇄ FR)")
      : QCoreApplication::translate("wire", "Headline translation is not connected yet"), this);
}
void Header::leaveEvent(QEvent*) { m_hoverLang = false; update(); }

// ============================================================================================ Ticker
Ticker::Ticker(QWidget* parent) : QWidget(parent) {
  setFixedHeight(34);   // reference: strip 45–77, rule at 78
  setAttribute(Qt::WA_OpaquePaintEvent, look().ticker.alpha() == 255);   // fills its rect: no repaint of the panel under it at 30 fps
  m_timer.setInterval(33);   // ≤ 30 fps
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    const qint64 now = m_clock.elapsed();
    const qreal dt = (now - m_lastMs) / 1000.0;
    m_lastMs = now;
    if (m_strip.isNull()) return;
    const qreal w = m_strip.width() / m_strip.devicePixelRatio();
    m_offset = std::fmod(m_offset + 48.0 * qMin(dt, 0.25), w);
    update(0, 0, width(), 33);
  });
  m_clock.start();
}
void Ticker::setMoves(const QList<wire::Move>& moves) {
  // keep the scroll position; only rebuild when the visible text changes
  m_moves = moves;
  rebuild();
  update();
}
void Ticker::setRunning(bool on) {
  if (on && !m_strip.isNull()) { m_lastMs = m_clock.elapsed(); m_timer.start(); }
  else m_timer.stop();
}
void Ticker::resizeEvent(QResizeEvent*) { rebuild(); }
void Ticker::rebuild() {
  const auto& L = look();
  if (m_moves.isEmpty()) {
    m_strip = QPixmap();
    m_timer.stop();
    m_empty = QCoreApplication::translate("wire", "No price move beyond its threshold right now · indices 1% · stocks & commodities 2% · crypto 3% · FX majors 0.5%");
    return;
  }
  const QFont fb = theme::mono(13, QFont::Bold), fr = theme::mono(13, QFont::Medium);
  const QFontMetricsF mb(fb), mr(fr);
  struct Seg { QString t; QColor c; QFont f; };
  QList<QList<Seg>> items;
  qreal total = 0;
  for (const auto& m : m_moves) {
    const QColor sc = m.changePct >= 0 ? L.up : L.down;
    QList<Seg> s{{m.since.toLocalTime().toString("HH:mm:ss"), L.tickText, fb}, {"\u2022", sc, fb}, {m.name + " ", L.tickText, fb},
                 {fmt::pct(m.changePct), sc, fr}, {QCoreApplication::translate("wire", " — now %1").arg(fmt::num(m.price, m.decimals)), theme::map("#d2d0d0"), fb}};
    for (const auto& x : s) total += QFontMetricsF(x.f).horizontalAdvance(x.t);
    total += 34 + 16 - QFontMetricsF(fb).horizontalAdvance(QStringLiteral("\u2022"));
    items << s;
  }
  // repeat the cycle so one strip is at least as wide as the widget (seamless wrap)
  const int reps = qMax(1, int(std::ceil(width() / qMax(1.0, total))));
  const qreal W = total * reps;
  const qreal dpr = devicePixelRatioF();
  m_strip = QPixmap(QSize(int(std::ceil(W * dpr)), int(33 * dpr)));
  m_strip.setDevicePixelRatio(dpr);
  m_strip.fill(Qt::transparent);
  QPainter p(&m_strip);
  p.setRenderHint(QPainter::TextAntialiasing);
  qreal x = 0;
  for (int r = 0; r < reps; ++r)
    for (const auto& s : items) {
      for (const auto& seg : s) {
        if (seg.t == QStringLiteral("\u2022")) {   // small sign-coloured dot, tight like the reference
          p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(seg.c);
          p.drawEllipse(QPointF(x + 8, 16.5), 2, 2);
          x += 16; continue;
        }
        p.setFont(seg.f); p.setPen(seg.c);
        p.drawText(QPointF(x, 21), seg.t);
        x += QFontMetricsF(seg.f).horizontalAdvance(seg.t);
      }
      x += 34;
    }
  p.end();
  if (m_offset >= total * reps) m_offset = 0;
  if (isVisible() && !m_timer.isActive()) { m_lastMs = m_clock.elapsed(); m_timer.start(); }
}
void Ticker::paintEvent(QPaintEvent*) {
  const auto& L = look();
  QPainter p(this);
  p.fillRect(rect(), L.ticker);
  p.fillRect(QRect(0, height() - 1, width(), 1), theme::map("#29251b"));
  if (m_strip.isNull()) {
    p.setFont(theme::mono(11)); p.setPen(L.footer);
    p.drawText(rect().adjusted(14, 0, -14, -2), Qt::AlignVCenter | Qt::AlignLeft, m_empty);
    return;
  }
  const qreal w = m_strip.width() / m_strip.devicePixelRatio();
  for (qreal x = -m_offset; x < width(); x += w) p.drawPixmap(QPointF(x, 0), m_strip);
}

// ============================================================================================ FilterTabs
FilterTabs::FilterTabs(QWidget* parent) : QWidget(parent) {
  setFixedHeight(42);   // reference: tabs 79–119 (pills 88–110), rule at 120
  setMouseTracking(true);
}
static const char* const kTabNames[4] = {QT_TRANSLATE_NOOP("wire", "ALL"), QT_TRANSLATE_NOOP("wire", "DATA"),
                                        QT_TRANSLATE_NOOP("wire", "MOVERS"), QT_TRANSLATE_NOOP("wire", "RELEASES")};
static QString tabName(int i) { return QCoreApplication::translate("wire", kTabNames[i]); }
QList<QRectF> FilterTabs::rects() const {
  QList<QRectF> out;
  const QFontMetricsF fl(theme::mono(12, QFont::Medium)), fc(theme::mono(12));
  qreal x = 11;
  for (int i = 0; i < 4; ++i) {
    qreal w = fl.horizontalAdvance(tabName(i)) + 14;
    if (i == 2) w += 11;   // lightning glyph
    if (m_counts.value(i, -1) >= 0) w += fc.horizontalAdvance(QString::number(m_counts[i])) + 5;
    out << QRectF(x, 9, w, 23);
    x += w + 5;
  }
  return out;
}
void FilterTabs::paintEvent(QPaintEvent*) {
  const auto& L = look();
  QPainter p(this);
  p.fillRect(rect(), L.tabs);
  p.fillRect(QRect(0, height() - 1, width(), 1), L.sepStrong);
  p.setRenderHint(QPainter::Antialiasing);
  const auto rs = rects();
  const QFont fl = theme::mono(12, QFont::Medium), fc = theme::mono(12);
  for (int i = 0; i < 4; ++i) {
    const QRectF r = rs[i];
    const bool on = i == m_cur, hov = i == m_hover;
    if (on) { p.setPen(Qt::NoPen); p.setBrush(L.amberPill); p.drawRoundedRect(r, 2, 2); }
    else if (hov) { p.setPen(Qt::NoPen); p.setBrush(L.tabHoverBg); p.drawRoundedRect(r, 2, 2); }
    qreal x = r.left() + 7;
    const qreal base = r.center().y() + 4.5;
    const QColor lc = on ? L.tabOn : hov ? L.tabHover : L.tabOff;
    if (i == 2) {   // ⚡
      QPainterPath b; const QPointF c(x + 3.5, r.center().y());
      b.moveTo(c + QPointF(1, -5)); b.lineTo(c + QPointF(-3, 0.8)); b.lineTo(c + QPointF(-0.3, 0.8));
      b.lineTo(c + QPointF(-1.2, 5)); b.lineTo(c + QPointF(3, -1)); b.lineTo(c + QPointF(0.3, -1)); b.closeSubpath();
      p.setPen(Qt::NoPen); p.setBrush(theme::alpha(lc, 200)); p.drawPath(b);
      x += 11;
    }
    p.setFont(fl); p.setPen(lc);
    p.drawText(QPointF(x, base), tabName(i));
    x += QFontMetricsF(fl).horizontalAdvance(tabName(i)) + 5;
    if (m_counts.value(i, -1) >= 0) {
      p.setFont(fc); p.setPen(on ? L.tabOnCount : L.tabOffCount);
      p.drawText(QPointF(x, base), QString::number(m_counts[i]));
    }
  }
}
void FilterTabs::mousePressEvent(QMouseEvent* e) {
  const auto rs = rects();
  for (int i = 0; i < rs.size(); ++i)
    if (rs[i].contains(e->position()) && i != m_cur) { m_cur = i; update(); emit changed(i); }
}
void FilterTabs::mouseMoveEvent(QMouseEvent* e) {
  const auto rs = rects(); int h = -1;
  for (int i = 0; i < rs.size(); ++i) if (rs[i].contains(e->position())) h = i;
  if (h != m_hover) { m_hover = h; update(); setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); }
}
void FilterTabs::leaveEvent(QEvent*) { m_hover = -1; update(); }
void FilterTabs::setHoverAt(const QPoint& pt) {
  const auto rs = rects(); m_hover = -1;
  for (int i = 0; i < rs.size(); ++i) if (rs[i].contains(pt)) m_hover = i;
  update();
}

// ============================================================================================ WireList
WireList::WireList(QWidget* parent) : QAbstractScrollArea(parent), m_anim(new QVariantAnimation(this)) {
  setFrameShape(QFrame::NoFrame);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  viewport()->setMouseTracking(true);
  viewport()->setAutoFillBackground(false);
  m_anim->setDuration(160);
  m_anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { verticalScrollBar()->setValue(v.toInt()); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(), [this] { viewport()->update(); });
}
QString WireList::textOf(const wire::Item& i) const {
  if (m_tr && i.kind == wire::Kind::Headline) { const auto it = m_tr->constFind(i.id); if (it != m_tr->cend()) return *it; }
  if (i.kind == wire::Kind::Release) {   // rebuilt in the interface language from the stored values
    auto v = [](const QString& x) { return x.isEmpty() ? QStringLiteral("-") : x; };
    const QString title = i.text.section(" Actual ", 0, 0);   // "<REGION> <event>" as stored by wire::releaseText
    return QCoreApplication::translate("wire", "%1 Actual %2 (Forecast %3, Previous %4)").arg(title, v(i.actual), v(i.forecast), v(i.previous));
  }
  return i.text;
}
void WireList::setItems(const QList<wire::Item>& items) {
  // keep the reading position when new items arrive on top
  const int oldTop = verticalScrollBar()->value();
  QString anchorId; int anchorOff = 0;
  if (oldTop > 0 && !m_items.isEmpty()) {
    const int r = rowAt(oldTop);
    if (r >= 0) { anchorId = m_items[r].id; anchorOff = oldTop - m_y[r]; }
  }
  const QString hoverId = m_hover >= 0 && m_hover < m_items.size() ? m_items[m_hover].id : QString();
  m_items = items;
  m_hover = -1;
  relayout();
  if (!anchorId.isEmpty())
    for (int i = 0; i < m_items.size(); ++i)
      if (m_items[i].id == anchorId) { verticalScrollBar()->setValue(m_y[i] + anchorOff); m_target = m_y[i] + anchorOff; break; }
  if (!hoverId.isEmpty())   // the row under the mouse stays highlighted (by position, as the cursor has not moved)
    m_hover = rowAt(viewport()->mapFromGlobal(QCursor::pos()).y() + verticalScrollBar()->value());
  if (!hoverId.isEmpty() && !underMouse()) for (int i = 0; i < m_items.size(); ++i) if (m_items[i].id == hoverId) m_hover = i;
  viewport()->update();
}
void WireList::relayout() {
  const int w = viewport()->width();
  m_layoutWidth = w;
  const int textW = qMax(120, w - kTextX - kRightPad);
  const QFontMetrics fm(headlineFont());
  m_y.resize(m_items.size() + 1);
  // Heights are kept only for the rows listed at the current width: the cache was keyed by every width a resize went
  // through and every item ever shown, and grew for the whole session.
  QHash<QString, int> kept;
  kept.reserve(m_items.size());
  int y = 0;
  for (int i = 0; i < m_items.size(); ++i) {
    m_y[i] = y;
    const QString key = m_items[i].id + "|" + QString::number(textW);
    int h = m_heights.value(key, -1);
    if (h < 0) {
      const QRect br = fm.boundingRect(QRect(0, 0, textW, 10000), Qt::TextWordWrap, textOf(m_items[i]));
      const int lines = qMax(1, int(std::round(br.height() / double(fm.lineSpacing()))));
      h = 19 + lines * 20;
    }
    kept.insert(key, h);
    y += h;
  }
  m_heights.swap(kept);
  m_y[m_items.size()] = y;
  verticalScrollBar()->setRange(0, qMax(0, y - viewport()->height()));
  verticalScrollBar()->setPageStep(viewport()->height());
}
int WireList::rowAt(int y) const {
  if (m_items.isEmpty() || y < 0 || y >= m_y.last()) return -1;
  const auto it = std::upper_bound(m_y.begin(), m_y.end() - 1, y);
  return int(it - m_y.begin()) - 1;
}
void WireList::resizeEvent(QResizeEvent* e) {
  QAbstractScrollArea::resizeEvent(e);
  relayout();
}
void WireList::paintEvent(QPaintEvent* e) {
  const auto& L = look();
  QPainter p(viewport());
  p.fillRect(e->rect(), L.page);
  const int top = verticalScrollBar()->value();
  const int w = viewport()->width(), h = viewport()->height();
  if (m_items.isEmpty()) {
    p.setFont(theme::mono(11, QFont::Normal, 1)); p.setPen(L.footer);
    p.drawText(viewport()->rect().adjusted(30, 0, -30, 0), Qt::AlignCenter | Qt::TextWordWrap, m_empty);
    return;
  }
  const QFont ft = theme::mono(13, QFont::Bold), fa = theme::mono(11), fh = headlineFont();
  const QFontMetrics fmh(fh);
  const int textW = qMax(120, w - kTextX - kRightPad);
  int r = qMax(0, rowAt(top + e->rect().top()));
  for (; r < m_items.size() && m_y[r] - top < e->rect().bottom() + 1; ++r) {
    const auto& it = m_items[r];
    const int y0 = m_y[r] - top, rh = m_y[r + 1] - m_y[r];
    if (r == m_hover) p.fillRect(QRect(0, y0, w, rh - 1), L.hover);
    p.fillRect(QRect(0, y0 + rh - 1, w, 1), L.sep);
    const int base = y0 + 24;   // first text line baseline
    p.setFont(ft); p.setPen(L.time);
    p.drawText(QPointF(kTimeX, base), it.time.toLocalTime().toString("HH:mm:ss"));
    p.setFont(fa); p.setPen(theme::isDark() ? L.age : theme::roles().textDim);   // faint ages wash out on light themes
    p.drawText(QPointF(kAgeX, base - 0.5), ageText(it.time));
    coin(p, QPointF(kCoinX, base - 7), it);
    p.setFont(fh);
    if (it.kind == wire::Kind::Mover && std::isfinite(it.changePct)) {
      const auto* in = market::find(it.symbol);
      const QString name = (in ? in->name : it.symbol) + " ";
      const QString pc = fmt::pct(it.changePct);
      const QString rest = QCoreApplication::translate("wire", " — now %1").arg(fmt::num(it.price, in ? in->decimals : 2));
      qreal x = kTextX;
      p.setPen(L.body); p.drawText(QPointF(x, base), name); x += fmh.horizontalAdvance(name);
      p.setPen(it.changePct >= 0 ? L.up : L.down); p.drawText(QPointF(x, base), pc); x += fmh.horizontalAdvance(pc);
      p.setPen(L.body); p.drawText(QPointF(x, base), rest);
    } else {
      p.setPen(L.body);
      p.drawText(QRect(kTextX, base - fmh.ascent(), textW, rh), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, textOf(it));
    }
  }
  // amber rule along the left edge
  p.fillRect(QRect(1, 0, 1, h), theme::alpha(L.edge, 200));
}
void WireList::scrollTo(int v) {
  v = qBound(0, v, verticalScrollBar()->maximum());
  m_target = v;
  m_anim->stop();
  m_anim->setStartValue(verticalScrollBar()->value());
  m_anim->setEndValue(v);
  m_anim->start();
}
void WireList::scrollBy(int dy) {
  const int base = m_anim->state() == QAbstractAnimation::Running ? m_target : verticalScrollBar()->value();
  scrollTo(base + dy);
}
void WireList::wheelEvent(QWheelEvent* e) {
  if (!e->pixelDelta().isNull()) { m_anim->stop(); verticalScrollBar()->setValue(verticalScrollBar()->value() - e->pixelDelta().y()); m_target = verticalScrollBar()->value(); }
  else scrollBy(-e->angleDelta().y() * 100 / 120);
  e->accept();
}
void WireList::setHoverAt(const QPoint& pos) {
  m_hover = rowAt(pos.y() + verticalScrollBar()->value());
  viewport()->update();
}
void WireList::mouseMoveEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()) + verticalScrollBar()->value());
  if (r != m_hover) {
    m_hover = r;
    viewport()->update();
    viewport()->setCursor(r >= 0 && !m_items[r].url.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }
}
bool WireList::viewportEvent(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(e);
    const int r = rowAt(he->pos().y() + verticalScrollBar()->value());
    if (r >= 0) {
      const auto& it = m_items[r];
      QString tip = QString("%1 · %2").arg(it.source, i18n::locale().toString(it.time.toLocalTime(), "ddd d MMM HH:mm:ss"));
      if (it.kind == wire::Kind::Mover) tip += "\n" + QCoreApplication::translate("wire", "Price move computed by Deepwater from Yahoo quotes (change vs previous close)");
      if (m_tr && m_tr->contains(it.id) && m_tr->value(it.id) != it.text) tip += "\n" + QCoreApplication::translate("wire", "Original: %1").arg(it.text);
      if (!it.url.isEmpty()) tip += "\n" + QCoreApplication::translate("wire", "Click to open the source");
      QToolTip::showText(he->globalPos(), ui::plainTip(tip), viewport());   // headlines from feeds
    } else QToolTip::hideText();
    return true;
  }
  return QAbstractScrollArea::viewportEvent(e);
}
void WireList::mousePressEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()) + verticalScrollBar()->value());
  if (r >= 0 && e->button() == Qt::LeftButton && !m_items[r].url.isEmpty()) ui::openWebLink(m_items[r].url);
}
void WireList::leaveEvent(QEvent*) { if (m_hover >= 0) { m_hover = -1; viewport()->update(); } }

// ============================================================================================ Footer
Footer::Footer(QWidget* parent) : QWidget(parent) { setFixedHeight(28); }
void Footer::setStamp(const QDateTime& t, bool live) { m_t = t; m_live = live; update(); }
void Footer::paintEvent(QPaintEvent*) {
  const auto& L = look();
  QPainter p(this);
  p.fillRect(rect(), L.page);
  p.fillRect(QRect(0, 0, width(), 1), L.sep);
  p.setFont(theme::sans(10)); p.setPen(L.footer);
  p.drawText(QRectF(7, 0, width() - 80, height()), Qt::AlignVCenter | Qt::AlignLeft,
             QCoreApplication::translate("wire", "Terminal Data · %1").arg(m_t.isValid() ? m_t.toLocalTime().toString("HH:mm:ss") : QStringLiteral("—")));
  if (m_live) { const qreal lw = livePillWidth(); livePill(p, QRectF(width() - 8 - lw, 6, lw, 18)); }
}
}
