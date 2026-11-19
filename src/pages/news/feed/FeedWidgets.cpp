#include "pages/news/feed/FeedWidgets.h"
#include "news/Translate.h"
#include "core/Settings.h"
#include "ui/Theme.h"
#include "pages/news/ThemeColor.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include "ui/Kit.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLineEdit>
#include <QTextLayout>
#include <QStaticText>
#include <QElapsedTimer>
#include <QToolTip>
#include <QMenu>
#include <cmath>
#include <QCache>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <climits>
#include <QDateTime>
#include <QApplication>

namespace news::pages {

// ------------------------------------------------------------------------------------------------ helpers
namespace {
const news::pages::Dw kWhite{"#fefefc"}, kTag{"#d3c194"}, kMetaDim{"#4c4b46"};

QStringList wrap(const QString& text, const QFont& f, double width, int maxLines) {
  QStringList out;
  QTextLayout tl(text, f);
  tl.beginLayout();
  for (;;) {
    QTextLine line = tl.createLine();
    if (!line.isValid()) break;
    line.setLineWidth(width);
    out << text.mid(line.textStart(), line.textLength()).trimmed();
  }
  tl.endLayout();
  if (maxLines > 0 && out.size() > maxLines) {
    QString last = out[maxLines - 1];
    const QFontMetricsF fm(f);
    while (!last.isEmpty() && fm.horizontalAdvance(last + "...") > width) last.chop(1);
    out = out.mid(0, maxLines);
    out[maxLines - 1] = last.trimmed() + "...";
  }
  return out;
}
QString initials(const QString& src) {
  if (src.startsWith("INVESTING.")) return "IN";
  if (src.startsWith("INVESTINGLIVE")) return "IL";
  if (src.startsWith("YAHOO")) return "YF";
  if (src.startsWith("MARKET")) return "MW";
  if (src.startsWith("CENTRAL")) return "CB";
  if (src.startsWith("FINANCIAL")) return "FJ";
  return src.left(2);
}
}

QString sourceLabel(const QString& name) { return QCoreApplication::translate("news", name.toUtf8().constData()); }
QString ageCompact(const QDateTime& t) {
  const qint64 s = std::max<qint64>(0, t.secsTo(QDateTime::currentDateTimeUtc()));
  if (s < 3600) return QCoreApplication::translate("news", "%1M").arg(std::max<qint64>(1, s / 60));
  if (s < 86400) return QCoreApplication::translate("news", "%1H").arg(s / 3600);
  return QCoreApplication::translate("news", "%1D").arg(s / 86400);
}
QString ageUpper(const QDateTime& t) {
  const qint64 s = std::max<qint64>(0, t.secsTo(QDateTime::currentDateTimeUtc()));
  if (s < 3600) return QCoreApplication::translate("news", "%1M AGO").arg(std::max<qint64>(1, s / 60));
  if (s < 86400) return QCoreApplication::translate("news", "%1H AGO").arg(s / 3600);
  return QCoreApplication::translate("news", "%1D AGO").arg(s / 86400);
}
QString ageLower(const QDateTime& t) {
  const qint64 s = std::max<qint64>(0, t.secsTo(QDateTime::currentDateTimeUtc()));
  if (s < 3600) return QCoreApplication::translate("news", "%1m ago").arg(std::max<qint64>(1, s / 60));
  if (s < 86400) return QCoreApplication::translate("news", "%1h ago").arg(s / 3600);
  return QCoreApplication::translate("news", "%1d ago").arg(s / 86400);
}

// Thumbnails by image and size: bounded by bytes (64 MB), least recently used out first (it was 300 entries of any
// size, an arbitrary one dropped, sometimes one on screen). The smooth scale of the decoded image runs on a worker.
static QCache<QString, QPixmap>& thumbs() { static QCache<QString, QPixmap> c(64 * 1024 * 1024); return c; }
static void keep(const QString& key, const QPixmap& pm) { thumbs().insert(key, new QPixmap(pm), int(qMin<qint64>(qint64(pm.width()) * pm.height() * 4, INT_MAX))); }
QPixmap storyPixmap(const news::Story& s, QSize size, QWidget* w) {
  // `failed`: image URL → when it failed (ms); asked again after 5 minutes (a host down for a moment kept every
  // tile on the placeholder for the whole session)
  static QHash<QString, qint64> failed;
  static QSet<QString> pending;
  // Image URL → cache key of its latest thumbnail, and the generation of the size asked last. A resize changes the
  // size at every step: each step flashed the generated tile and decoded + smooth-scaled the image again. Now the
  // previous thumbnail is shown scaled meanwhile, and the new size is loaded once the size stops changing.
  static QHash<QString, QString> lastKey;
  static QHash<QString, quint64> wantGen;
  static quint64 gen = 0;
  const qreal dpr = 2.0;
  const QString key = s.image + QString("@%1x%2").arg(size.width()).arg(size.height());
  if (!s.image.isEmpty()) {
    if (const QPixmap* hit = thumbs().object(key)) {
      if (lastKey.size() > 4000) lastKey.clear();   // stories roll over: a few thousand short strings at most
      lastKey.insert(s.image, key);
      return *hit;
    }
    const QPixmap* old = nullptr;
    if (const auto lk = lastKey.constFind(s.image); lk != lastKey.cend()) old = thumbs().object(*lk);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (auto f = failed.constFind(s.image); f != failed.cend() && nowMs - *f > 5 * 60 * 1000) failed.remove(s.image);
    if (!failed.contains(s.image) && !pending.contains(key)) {
      pending.insert(key);
      QPointer<QWidget> guard(w);
      // the callback must run even when the asking widget is gone (theme / language change rebuilds the cards):
      // it is what clears `pending`, so the load is tied to the application, and `guard` only gates the repaint
      auto load = [key, size, dpr, guard, url = s.image] {
        news::loadImage(url, qApp, [key, size, dpr, guard, url](const QImage& img) {
          if (img.isNull()) { pending.remove(key); failed.insert(url, QDateTime::currentMSecsSinceEpoch()); if (guard) guard->update(); return; }
          auto* watcher = new QFutureWatcher<QImage>();
          QObject::connect(watcher, &QFutureWatcher<QImage>::finished, [watcher, key, dpr, guard, url] {
            watcher->deleteLater();
            pending.remove(key);
            QPixmap pm = QPixmap::fromImage(watcher->result()); pm.setDevicePixelRatio(dpr);
            keep(key, pm);
            lastKey.insert(url, key);
            if (guard) guard->update();
          });
          watcher->setFuture(QtConcurrent::run([img, px = size * dpr] {
            QImage sc = img.scaled(px, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            return sc.copy((sc.width() - px.width()) / 2, (sc.height() - px.height()) / 2, px.width(), px.height());
          }));
        });
      };
      if (!old) load();
      else {   // another size is on screen: load this one only if it is still the one wanted 200 ms later
        const quint64 g = ++gen;
        wantGen.insert(s.image, g);
        QTimer::singleShot(200, qApp, [load, key, guard, g, url = s.image] {
          if (wantGen.value(url) != g) { pending.remove(key); return; }
          wantGen.remove(url);
          load();
          if (guard) guard->update();
        });
      }
    }
    if (old) {   // the previous size, scaled fast and cover-cropped, until the smooth one is ready
      const QSize px = size * dpr;
      const QPixmap sc = old->scaled(px, Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);
      QPixmap pm = sc.copy((sc.width() - px.width()) / 2, (sc.height() - px.height()) / 2, px.width(), px.height());
      pm.setDevicePixelRatio(dpr);
      return pm;
    }
  }
  // generated tile (no image in the feed, or still loading)
  const QString gkey = "gen:" + s.source + QString("@%1x%2#%3").arg(size.width()).arg(size.height()).arg(theme::revision());   // tile colours follow the theme
  if (const QPixmap* hit = thumbs().object(gkey)) return *hit;
  QPixmap pm(size * dpr); pm.setDevicePixelRatio(dpr);
  QPainter p(&pm);
  QLinearGradient g(0, 0, size.width(), size.height());
  g.setColorAt(0, theme::map("#221a05")); g.setColorAt(1, theme::map("#0a0703"));
  p.fillRect(QRect(QPoint(0, 0), size), g);
  p.setPen(theme::map(255, 220, 120, 14));
  for (int x = -size.height(); x < size.width(); x += 9) p.drawLine(x, size.height(), x + size.height(), 0);
  p.setPen(theme::map("#5a4c1c")); p.drawRect(QRectF(0.5, 0.5, size.width() - 1, size.height() - 1));
  p.setFont(theme::mono(std::max(10, size.height() / 4), QFont::Bold, 2)); p.setPen(theme::map("#8a7638"));
  p.drawText(QRect(QPoint(0, 0), size), Qt::AlignCenter, initials(s.source));
  p.end();
  keep(gkey, pm);
  return pm;
}

// ------------------------------------------------------------------------------------------------ state
FeedState::FeedState(QObject* parent) : QObject(parent) {
  fr = settings::value("news.feed.lang", i18n::code()).toString() == "fr";   // default: the interface language
  const QStringList off = settings::value("news.feed.sourcesOff", news::Feed::defaultSourcesOff()).toStringList();
  sourcesOff = QSet<QString>(off.cbegin(), off.cend());
  queue = settings::value("news.feed.queue").toStringList();
  // The feed keeps four days of stories (its archive is loaded by now): ids it no longer knows can never be shown
  // again, so drop them instead of letting the preference grow forever. Not when the feed is empty (archive missing or
  // unreadable): then nothing tells which ids are gone.
  if (!news::Feed::instance().stories().isEmpty()) {
    const int before = int(queue.size());
    queue.removeIf([](const QString& id) { return !news::Feed::instance().find(id); });
    if (queue.size() != before) settings::setValue("news.feed.queue", queue);
  }
  connect(&news::Translate::instance(), &news::Translate::progressed, this, [this] { if (fr) emit textChanged(); });
  connect(&news::Translate::instance(), &news::Translate::stateChanged, this, [this] { emit textChanged(); });
}
void FeedState::save() {
  settings::setValue("news.feed.lang", fr ? "fr" : "en");
  settings::setValue("news.feed.sourcesOff", QStringList(sourcesOff.cbegin(), sourcesOff.cend()));
  settings::setValue("news.feed.queue", queue);
}
QString FeedState::tx(const QString& en) const { return fr ? news::Translate::instance().text(en) : en; }
bool FeedState::translating() const { return fr && news::Translate::instance().usable() && news::Translate::instance().pending() > 0; }

QList<news::Story> FeedState::visibleSources() const {
  QList<news::Story> out;
  for (const auto& s : news::Feed::instance().stories()) if (!sourcesOff.contains(s.sourceId) && !s.lifestyle) out << s;
  return out;
}
QList<news::Story> FeedState::filtered() const {
  QList<news::Story> out;
  for (const auto& s : visibleSources()) {
    if (topic == 0 && s.impact == news::Impact3::None) continue;
    if (topic > 0 && !(s.topics & quint32(topic))) continue;
    if (!search.isEmpty() && !s.title.contains(search, Qt::CaseInsensitive) && !tx(s.title).contains(search, Qt::CaseInsensitive)
        && !s.source.contains(search, Qt::CaseInsensitive)) continue;
    out << s;
  }
  if (topic == 0) {   // TOP IMPACT: most market-moving first, within the last 48 h, then by time
    const QDateTime cut = QDateTime::currentDateTimeUtc().addSecs(-48 * 3600);
    std::stable_sort(out.begin(), out.end(), [cut](const news::Story& a, const news::Story& b) {
      const bool ra = a.time >= cut, rb = b.time >= cut;
      if (ra != rb) return ra;
      if (a.score != b.score) return a.score > b.score;
      return a.time > b.time;
    });
  }
  return out;
}

// ------------------------------------------------------------------------------------------------ header
WireHeader::WireHeader(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setFixedHeight(41);
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_clock.setInterval(1000);
  connect(&m_clock, &QTimer::timeout, this, [this] { update(QRect(0, 0, width(), 41)); });
  connect(st, &FeedState::textChanged, this, [this] { update(); });
}
void WireHeader::showEvent(QShowEvent*) { m_clock.start(); }
void WireHeader::hideEvent(QHideEvent*) { m_clock.stop(); }

QList<WireHeader::Hit> WireHeader::hits() const {
  QList<Hit> out;
  const QFont f = theme::mono(11, QFont::Bold, 0.8);
  const QFontMetricsF fm(f);
  // language chip: glyph (18 px) + "FR…" (the longest state) + margins; a fixed 44 px cut "FR…" and the glyph touched the text
  const double langW = 22 + QFontMetricsF(theme::mono(11, QFont::Bold)).horizontalAdvance(QString::fromUtf8("FR…")) + 7;
  const double langX = width() - 14 - QFontMetricsF(theme::mono(10, QFont::Normal, 1.5)).horizontalAdvance(QCoreApplication::translate("news", "%1 STORIES").arg("000")) - 9 - langW;
  const double liveClock = 12 + 52 + 11 + 82;
  const double captionW = 209;
  // fold sources into "+N" (switched-off ones first, then from the end) until chips + live/clock fit
  QList<news::FeedSource> shown = news::Feed::instance().sources();
  m_hidden.clear();
  auto chipsW = [&](const QList<news::FeedSource>& l, bool more) {
    double w = 0; for (const auto& s : l) w += fm.horizontalAdvance(sourceLabel(s.name)) + 18 + 6;
    return w + (more ? 44 : 0);
  };
  m_caption = 236 + chipsW(shown, false) + liveClock + captionW < langX;
  auto fits = [&] { return 236 + chipsW(shown, !m_hidden.isEmpty()) + liveClock + (m_caption ? captionW : 8) < langX; };
  for (int pass = 0; pass < 2 && !fits(); ++pass)
    for (int i = int(shown.size()) - 1; i >= 0 && !fits(); --i)
      if (pass == 1 || m_st->sourcesOff.contains(shown[i].id)) { m_hidden.prepend(shown[i].id); shown.removeAt(i); }
  double x = 236;
  for (const auto& s : shown) {
    const double w = fm.horizontalAdvance(sourceLabel(s.name)) + 18;
    out << Hit{QRectF(x, 9, w, 23), s.id};
    x += w + 6;
  }
  if (!m_hidden.isEmpty()) out << Hit{QRectF(x, 9, 38, 23), "more"};
  out << Hit{QRectF(langX, 10, langW, 21), "lang"};
  return out;
}

void WireHeader::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const double W = width();
  p.fillRect(rect(), theme::map("#040302"));
  {  // condensed display title, fitted to the reference width (142 px)
    QFont tf = theme::condensed(31, QFont::Bold);
    const double w0 = QFontMetricsF(tf).horizontalAdvance(QCoreApplication::translate("news", "MARKET WIRE"));
    if (w0 > 142) tf.setStretch(std::max(60, int(100 * 142 / w0)));
    p.setFont(tf); p.setPen(theme::map("#f6d24a"));
    p.drawText(QRectF(14, 0, 200, 41), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "MARKET WIRE"));
  }
  p.setFont(theme::mono(9, QFont::Normal, 2)); p.setPen(theme::map("#b7b1a8"));
  p.drawText(QRectF(178, 8, 60, 22), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "SOURCES"));
  const auto hs = hits();
  double after = 236;
  for (int i = 0; i < hs.size(); ++i) {
    const Hit& h = hs[i];
    if (h.id == "lang") continue;
    if (h.id == "more") {
      p.fillRect(h.r, m_hover == i ? theme::map("#14100a") : theme::map("#040302"));
      p.setPen(theme::map("#2a2418")); p.drawRect(h.r.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setFont(theme::mono(11, QFont::Bold)); p.setPen(theme::map("#a9a59a"));
      p.drawText(h.r, Qt::AlignCenter, QString("+%1").arg(m_hidden.size()));
      after = h.r.right();
      continue;
    }
    QString name; for (const auto& s : news::Feed::instance().sources()) if (s.id == h.id) name = sourceLabel(s.name);
    const bool on = !m_st->sourcesOff.contains(h.id);
    const bool err = !news::Feed::instance().sourceError(h.id).isEmpty();
    p.fillRect(h.r, on ? (m_hover == i ? theme::map("#352a03") : theme::map("#231c01")) : (m_hover == i ? theme::map("#14100a") : theme::map("#040302")));
    p.setPen(on ? theme::map("#464127") : theme::map("#2a2418")); p.drawRect(h.r.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setFont(theme::mono(11, QFont::Bold, 0.8)); p.setPen(on ? (err ? theme::map("#c9a9a0") : theme::map("#fdfaf0")) : theme::map("#6d6a60"));
    p.drawText(h.r, Qt::AlignCenter, name);
    after = h.r.right();
  }
  // LIVE chip + clock
  const QString liveText = m_live ? QCoreApplication::translate("news", "LIVE") : QCoreApplication::translate("news", "STALE");
  const QRectF live(after + 12, 9, std::max(52.0, QFontMetricsF(theme::mono(10, QFont::Normal, 1)).horizontalAdvance(liveText) + 24), 22);
  p.setPen(theme::map("#2e2a1e")); p.drawRect(live.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen); p.setBrush(m_live ? theme::map("#cfab46") : theme::map("#5b5a54"));
  p.drawEllipse(QPointF(live.x() + 10, live.center().y()), 2.6, 2.6);
  p.setBrush(Qt::NoBrush);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::mono(10, QFont::Normal, 1)); p.setPen(theme::map("#7f7d73"));
  p.drawText(live.adjusted(17, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, m_live ? QCoreApplication::translate("news", "LIVE") : QCoreApplication::translate("news", "STALE"));
  p.setFont(theme::mono(14, QFont::Medium)); p.setPen(theme::map("#d6bb9a"));
  p.drawText(QRectF(live.right() + 11, 0, 100, 41), Qt::AlignLeft | Qt::AlignVCenter, QTime::currentTime().toString("HH:mm:ss"));

  // right: AUTO-TRANSLATED [lang] N STORIES
  const QFont cf = theme::mono(10, QFont::Normal, 1.5);
  p.setFont(cf); p.setPen(theme::map("#4d4b47"));
  p.drawText(QRectF(W - 314, 0, 300, 41), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("news", "%1 STORIES").arg(m_count));
  const QRectF lr = hs.last().r;
  const auto& tr = news::Translate::instance();
  const bool frOk = m_st->fr && tr.usable();
  p.fillRect(lr, m_hover == hs.size() - 1 ? theme::map("#3a2a0c") : theme::map("#241806"));
  p.setPen(theme::map("#5e4b1e")); p.drawRect(lr.adjusted(0.5, 0.5, -0.5, -0.5));
  // translate glyph (文A-like): two small strokes
  p.setPen(QPen(theme::map("#e5c99a"), 1));
  const double gx = lr.x() + 8, gy = lr.center().y();
  p.drawLine(QPointF(gx, gy - 3), QPointF(gx + 6, gy - 3)); p.drawLine(QPointF(gx + 3, gy - 5), QPointF(gx + 3, gy - 3));
  p.drawLine(QPointF(gx + 1, gy + 3), QPointF(gx + 5, gy - 2)); p.drawLine(QPointF(gx + 6, gy + 5), QPointF(gx + 8, gy));
  p.drawLine(QPointF(gx + 8, gy), QPointF(gx + 10, gy + 5));
  p.setFont(theme::mono(11, QFont::Bold)); p.setPen(theme::map("#e5c99a"));
  const QString lang = !m_st->fr ? "EN" : !tr.usable() ? "EN" : m_st->translating() ? "FR…" : "FR";
  p.drawText(lr.adjusted(22, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, lang);
  p.setFont(theme::mono(9, QFont::Normal, 1.2)); p.setPen(frOk ? theme::map("#4a4742") : theme::map("#8a5a52"));
  const QString cap = m_st->fr && !tr.usable() ? QCoreApplication::translate("news", "FR UNAVAILABLE") : frOk ? QCoreApplication::translate("news", "AUTO-TRANSLATED") : QCoreApplication::translate("news", "ORIGINAL");
  if (m_caption) p.drawText(QRectF(lr.x() - 209, 0, 200, 41), Qt::AlignRight | Qt::AlignVCenter, cap);
  p.fillRect(QRectF(0, 40, W, 1), theme::map("#0a0504"));
}

void WireHeader::mouseMoveEvent(QMouseEvent* e) {
  int h = -1; const auto hs = hits();
  for (int i = 0; i < hs.size(); ++i) if (hs[i].r.contains(e->position())) h = i;
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (h >= 0 && hs[h].id == "lang") QToolTip::showText(e->globalPosition().toPoint(),
      QCoreApplication::translate("news", "Headline language: original English or French translation (on this Mac).\n%1").arg(news::Translate::instance().stateText()), this);
  else if (h >= 0 && hs[h].id == "more") QToolTip::showText(e->globalPosition().toPoint(), QCoreApplication::translate("news", "More sources"), this);
  else if (h >= 0) {
    QString tip; for (const auto& s : news::Feed::instance().sources()) if (s.id == hs[h].id) tip = s.site;
    const QString err = news::Feed::instance().sourceError(hs[h].id);
    QToolTip::showText(e->globalPosition().toPoint(), tip + (err.isEmpty() ? QString() : "\n" + QCoreApplication::translate("news", "Last error: %1").arg(err)) + "\n" + QCoreApplication::translate("news", "Click to show / hide"), this);
  } else QToolTip::hideText();
  if (h != m_hover) { m_hover = h; update(); }
}
void WireHeader::mousePressEvent(QMouseEvent* e) {
  for (const auto& h : hits()) {
    if (!h.r.contains(e->position())) continue;
    if (h.id == "lang") { m_st->fr = !m_st->fr; m_st->save(); emit m_st->textChanged(); emit m_st->changed(); }
    else if (h.id == "more") {
      QMenu menu(this);
      for (const auto& s : news::Feed::instance().sources()) {
        if (!m_hidden.contains(s.id)) continue;
        auto* a = menu.addAction(sourceLabel(s.name)); a->setCheckable(true); a->setChecked(!m_st->sourcesOff.contains(s.id));
        connect(a, &QAction::toggled, this, [this, id = s.id](bool on) {
          if (on) m_st->sourcesOff.remove(id); else m_st->sourcesOff.insert(id);
          m_st->save(); emit m_st->changed(); update();
        });
      }
      menu.exec(mapToGlobal(h.r.bottomLeft().toPoint() + QPoint(0, 4)));
    }
    else {
      if (m_st->sourcesOff.contains(h.id)) m_st->sourcesOff.remove(h.id); else m_st->sourcesOff.insert(h.id);
      m_st->save(); emit m_st->changed();
    }
    update(); return;
  }
}
void WireHeader::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------------------------------------ ticker
Ticker::Ticker(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setFixedHeight(37);
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_timer.setInterval(33);
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    static QElapsedTimer clock; if (!clock.isValid()) clock.start();
    const qint64 now = clock.elapsed();
    const qint64 dt = m_lastMs ? std::min<qint64>(100, now - m_lastMs) : 33;
    m_lastMs = now;
    if (m_paused || m_strip.isNull()) return;
    m_offset += 40.0 * dt / 1000.0;      // ≈40 px/s (the reference video runs 6× faster)
    const double sw = m_strip.width() / m_strip.devicePixelRatio();
    if (m_offset >= sw) m_offset -= sw;
    update();
  });
  // translations arrive in bursts (one signal per headline): one rebuild of the strip at most every 1.5 s, none
  // while hidden (rebuilt when shown again)
  m_rebuildSoon.setSingleShot(true);
  connect(&m_rebuildSoon, &QTimer::timeout, this, [this] { if (isVisible()) rebuild(); else m_dirty = true; });
  connect(st, &FeedState::textChanged, this, [this] {
    if (!isVisible()) { m_dirty = true; return; }
    if (!m_lastRebuild.isValid() || m_lastRebuild.elapsed() > 1500) rebuild();
    else if (!m_rebuildSoon.isActive()) m_rebuildSoon.start(int(1500 - m_lastRebuild.elapsed()) + 20);
  });
}
void Ticker::setStories(const QList<news::Story>& s) { m_stories = s.mid(0, 24); rebuild(); }
void Ticker::rebuild() {
  m_dirty = false; m_lastRebuild.restart();
  m_spans.clear();
  if (m_stories.isEmpty()) { m_strip = QPixmap(); update(); return; }
  const QFont fa = theme::condensed(15, QFont::DemiBold), ft = theme::condensed(15, QFont::DemiBold);
  const QFontMetricsF fma(fa), fmt(ft);
  struct It { QString age, ago, title; double w; };
  QList<It> items; double total = 0;
  for (const auto& s : m_stories) {
    const QString a = ageLower(s.time);
    const bool split = a.endsWith(" ago");   // English: "14h" bright + "ago" dim; other languages: one piece
    It it{split ? a.section(' ', 0, 0) : a, split ? QStringLiteral("ago") : QString(), m_st->tx(s.title), 0};
    it.w = fma.horizontalAdvance(it.age + " ") + fma.horizontalAdvance(it.ago) + 9 + fmt.horizontalAdvance(it.title) + 16 + 9 + 16;
    total += it.w; items << it;
  }
  total = std::min(total, 30000.0);
  const qreal dpr = 2.0;
  QPixmap pm(QSize(int(std::ceil(total)), 37) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(theme::map("#060302"));
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  double x = 0;
  for (int i = 0; i < items.size() && x < total; ++i) {
    const It& it = items[i];
    m_spans << qMakePair(x, m_stories[i].id);
    p.setFont(fa); p.setPen(theme::map("#bbaa82"));
    p.drawText(QPointF(x, 24), it.age);
    double xx = x + fma.horizontalAdvance(it.age + " ");
    p.setPen(theme::map("#a08d61")); p.drawText(QPointF(xx, 24), it.ago);
    xx += fma.horizontalAdvance(it.ago) + 9;
    p.setFont(ft); p.setPen(theme::map("#f9f8f7")); p.drawText(QPointF(xx, 24), it.title);
    xx += fmt.horizontalAdvance(it.title) + 16;
    QPainterPath d; d.moveTo(xx + 4.5, 14); d.lineTo(xx + 9, 18.5); d.lineTo(xx + 4.5, 23); d.lineTo(xx, 18.5); d.closeSubpath();
    p.fillPath(d, theme::map("#605c54"));
    x += it.w;
  }
  p.end();
  m_strip = pm;
  update();
}
void Ticker::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), theme::map("#060302"));
  if (!m_strip.isNull()) {
    const double sw = m_strip.width() / m_strip.devicePixelRatio();
    for (double x = -m_offset; x < width(); x += sw) p.drawPixmap(QPointF(x, 0), m_strip);
  } else {
    p.setFont(theme::mono(10, QFont::Normal, 1)); p.setPen(theme::map("#5b5750"));
    p.drawText(rect().adjusted(14, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "Waiting for the first headlines…"));
  }
  p.fillRect(QRectF(0, 36, width(), 1), theme::map("#0f0602"));
}
void Ticker::showEvent(QShowEvent*) { m_lastMs = 0; m_timer.start(); if (m_dirty) rebuild(); }
void Ticker::hideEvent(QHideEvent*) { m_timer.stop(); }
void Ticker::enterEvent(QEnterEvent*) { m_paused = true; }
void Ticker::leaveEvent(QEvent*) { m_paused = false; }
void Ticker::mouseMoveEvent(QMouseEvent*) { setCursor(Qt::PointingHandCursor); }
void Ticker::mousePressEvent(QMouseEvent* e) {
  if (m_strip.isNull()) return;
  const double sw = m_strip.width() / m_strip.devicePixelRatio();
  const double sx = std::fmod(e->position().x() + m_offset, sw);
  QString id;
  for (const auto& [x0, sid] : m_spans) if (x0 <= sx) id = sid;
  if (!id.isEmpty()) emit m_st->openStory(id);
}

// ------------------------------------------------------------------------------------------------ lead
LeadStory::LeadStory(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setCursor(Qt::PointingHandCursor);
  connect(st, &FeedState::textChanged, this, [this] { update(); });
}
void LeadStory::setStory(const news::Story& s, bool tag) {
  m_s = s; m_has = true; m_tag = tag;
  setToolTip(ui::plainTip(m_st->tx(s.title) + "\n" + sourceLabel(s.source)));   // titles come from feeds: never HTML   // the title may be elided to 2 lines
  update();
}
void LeadStory::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const QRectF r = rect();
  p.fillRect(r, theme::map("#040300"));
  QRadialGradient glow(QPointF(r.center().x(), -40), r.width() * 0.55);
  glow.setColorAt(0, theme::map(40, 26, 2, theme::isDark() ? 120 : 40));   // light themes: faint glow
  glow.setColorAt(1, theme::map(0, 0, 0, 0));
  p.fillRect(r, glow);
  p.setPen(theme::map("#302912")); p.drawLine(QPointF(0, 0.5), QPointF(r.width(), 0.5));
  p.setPen(theme::map("#1f1a05")); p.drawLine(QPointF(0.5, 0), QPointF(0.5, r.height()));
  p.drawLine(QPointF(r.width() - 0.5, 0), QPointF(r.width() - 0.5, r.height()));
  p.setPen(theme::map("#2a1806")); p.drawLine(QPointF(0, r.height() - 0.5), QPointF(r.width(), r.height() - 0.5));
  // label
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#d8ba63")); p.drawEllipse(QPointF(14, 22), 2.5, 2.5);
  p.setBrush(Qt::NoBrush);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::mono(13, QFont::Bold, 2.4)); p.setPen(theme::map("#eedfa3"));
  p.drawText(QRectF(21, 12, 300, 20), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "LEAD STORY"));
  if (m_has) {
    double iw = std::clamp(std::round(r.width() * 0.34), 240.0, 560.0), ih = std::round(iw * 260 / 421);
    if (ih > r.height() - 40) { ih = r.height() - 40; iw = std::round(ih * 421 / 260); }   // short viewports
    const QRectF img(1, 39, iw, ih);
    p.drawPixmap(img.topLeft(), storyPixmap(m_s, img.size().toSize(), this));
    const double tx = iw + 15, tw = std::clamp(r.width() - tx - 60, 160.0, 900.0);   // headline lines ≤ 900 px
    const QFont tf = theme::sans(r.width() < 900 ? 28 : 33, QFont::DemiBold);
    const QStringList lines = wrap(m_st->tx(m_s.title), tf, tw, 2);
    const bool tag = m_tag && m_s.primaryTopic();
    const double blockH = (tag ? 24 : 0) + lines.size() * (tf.pixelSize() + 7) + 22;
    double y = img.center().y() - blockH / 2;
    if (tag) {
      p.setFont(theme::mono(10, QFont::Bold, 2.2)); p.setPen(kTag);
      p.drawText(QRectF(tx, y, tw, 16), Qt::AlignLeft | Qt::AlignVCenter, news::topicName(m_s.primaryTopic()));
      y += 24;
    }
    p.setFont(tf); p.setPen(m_hover ? theme::map("#f2df93") : kWhite);
    const double lh = tf.pixelSize() + 7;
    for (const auto& l : lines) { p.drawText(QRectF(tx, y, tw + 40, lh), Qt::AlignLeft | Qt::AlignVCenter, l); y += lh; }
    p.setFont(theme::mono(10, QFont::Bold, 0.8)); p.setPen(kWhite);
    const QFontMetricsF fm(p.font());
    p.drawText(QRectF(tx, y + 2, tw, 18), Qt::AlignLeft | Qt::AlignVCenter, sourceLabel(m_s.source));
    p.setFont(theme::mono(10, QFont::Normal, 0.8)); p.setPen(kMetaDim);
    p.drawText(QRectF(tx + fm.horizontalAdvance(sourceLabel(m_s.source)) + 8, y + 2, tw, 18), Qt::AlignLeft | Qt::AlignVCenter, "·  " + ageUpper(m_s.time));
  } else {
    p.setFont(theme::mono(11, QFont::Normal, 1)); p.setPen(theme::map("#5b5750"));
    p.drawText(r, Qt::AlignCenter, QCoreApplication::translate("news", "No story matches the filters"));
  }
  ui::drawBrackets(p, r.adjusted(1, 1, -1, -1), theme::map("#f3eaaa"), 10, true, true, true, true);
}
int LeadStory::preferredHeight(int w, int viewH) {
  const double iw = std::clamp(std::round(w * 0.34), 240.0, 560.0);
  double ih = std::round(iw * 260 / 421);        // 300 at the reference
  if (viewH > 0 && viewH < 560) ih = std::min(ih, std::max(110.0, std::round(viewH * 0.30)));
  return int(ih) + 40;
}
void LeadStory::mousePressEvent(QMouseEvent*) { if (m_has) emit m_st->openStory(m_s.id); }
void LeadStory::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void LeadStory::leaveEvent(QEvent*) { m_hover = false; update(); }

// ------------------------------------------------------------------------------------------------ filter bar
FilterBar::FilterBar(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setMouseTracking(true);
  m_search = new QLineEdit(this);
  m_search->setPlaceholderText(QCoreApplication::translate("news", "Search headlines..."));
  m_search->setStyleSheet(mapQss("QLineEdit{background:transparent;border:none;padding:0;color:#e8e3d6;font-family:'Rajdhani';"
                          "font-size:15px;font-weight:500;} QLineEdit:focus{border:none;}"));
  QPalette pl = m_search->palette(); pl.setColor(QPalette::PlaceholderText, theme::map("#4a463d")); m_search->setPalette(pl);
  connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) { m_st->search = t.trimmed(); emit m_st->changed(); });
}
void FilterBar::resizeEvent(QResizeEvent*) { m_search->setGeometry(36, 13, 160, 24); }
QFont FilterBar::chipFont(bool tight) const {
  QFont f = theme::condensed(13, QFont::Bold); f.setLetterSpacing(QFont::AbsoluteSpacing, tight ? 0.6 : 1.4);
  return f;
}
QList<FilterBar::Hit> FilterBar::hits(bool tight) const {
  QList<Hit> out;
  double x = 205, y = 10;
  const QFontMetricsF fm(chipFont(tight));
  const double pad = tight ? 16 : 26, gap = tight ? 6 : 8;
  QList<QPair<int, QString>> items{{0, QCoreApplication::translate("news", "TOP IMPACT")}};
  for (auto t : news::allTopics()) items << qMakePair(int(t), news::topicName(t));
  for (const auto& [id, t] : items) {
    const double w = fm.horizontalAdvance(t) + pad + (id == 0 ? 12 : 0);
    if (x + w > width() - 10 && x > 12) { x = 12; y += 39; }   // narrow windows: the chips wrap to a new row
    out << Hit{QRectF(x, y, w, 31), id, t};
    x += w + gap;
  }
  return out;
}
// Tight chips when they need fewer rows than the reference spacing (1280: "RESULTS" wrapped alone to a 2nd row).
bool FilterBar::tight() const {
  const auto a = hits(false), b = hits(true);
  return !a.isEmpty() && b.last().r.bottom() < a.last().r.bottom();
}
QList<FilterBar::Hit> FilterBar::hits() const { return hits(tight()); }
int FilterBar::preferredHeight(int w) const {
  const int old = width();
  const_cast<FilterBar*>(this)->resize(w, height());
  const auto hs = hits();
  const_cast<FilterBar*>(this)->resize(old, height());
  return hs.isEmpty() ? 51 : int(hs.last().r.bottom()) + 10;
}

void FilterBar::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const QRectF r = rect();
  p.fillRect(r, theme::map("#090703"));
  p.setPen(theme::map("#231e14")); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
  const QRectF sr(12, 10, 185, 32);
  p.setPen(theme::map("#252114")); p.drawRect(sr.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(theme::map("#5e5a50"), 1.2)); p.drawEllipse(QPointF(25, 25), 4, 4); p.drawLine(QPointF(28, 28), QPointF(31, 31));
  p.setRenderHint(QPainter::Antialiasing, false);
  const bool tightChips = tight();
  const auto hs = hits(tightChips);
  for (int i = 0; i < hs.size(); ++i) {
    const Hit& h = hs[i];
    const bool on = m_st->topic == h.topic;
    p.fillRect(h.r, on ? theme::map("#39310f") : m_hover == i ? theme::map("#17110a") : theme::map("#0e0705"));
    p.setPen(on ? theme::map("#4e4527") : theme::map("#282317")); p.drawRect(h.r.adjusted(0.5, 0.5, -0.5, -0.5));
    if (on) p.fillRect(QRectF(h.r.x(), h.r.bottom() - 2, h.r.width(), 2), theme::map("#e0d07c"));
    p.setFont(chipFont(tightChips)); p.setPen(on ? theme::map("#f9e494") : theme::map("#c0b9ad"));
    QRectF tr = h.r;
    if (h.topic == 0) {   // lightning glyph
      p.setRenderHint(QPainter::Antialiasing, true);
      QPainterPath b; const double bx = h.r.x() + 12, by = h.r.center().y();
      b.moveTo(bx + 3, by - 6); b.lineTo(bx - 1, by + 1); b.lineTo(bx + 2, by + 1); b.lineTo(bx, by + 6); b.lineTo(bx + 4.5, by - 1);
      b.lineTo(bx + 1.5, by - 1); b.closeSubpath();
      p.setPen(QPen(on ? theme::map("#f9e494") : theme::map("#c0b9ad"), 1)); p.drawPath(b);
      p.setRenderHint(QPainter::Antialiasing, false);
      tr.adjust(12, 0, 0, 0);
    }
    p.drawText(tr, Qt::AlignCenter, h.text);
  }
}
void FilterBar::mouseMoveEvent(QMouseEvent* e) {
  int h = -1; const auto hs = hits();
  for (int i = 0; i < hs.size(); ++i) if (hs[i].r.contains(e->position())) h = i;
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (h != m_hover) { m_hover = h; update(); }
}
void FilterBar::mousePressEvent(QMouseEvent* e) {
  for (const auto& h : hits())
    if (h.r.contains(e->position())) { m_st->setTopic(m_st->topic == h.topic ? -1 : h.topic); update(); return; }
}
void FilterBar::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------------------------------------ cards
CardList::CardList(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setMouseTracking(true);
  // translations arrive in batches, also while the page is hidden: re-wrapping every card then is wasted (showEvent
  // does it once)
  connect(st, &FeedState::textChanged, this, [this] { m_layoutW = -1; if (isVisible()) { relayout(); update(); } });
}
void CardList::showEvent(QShowEvent*) { relayout(); }   // no-op unless texts changed while hidden
void CardList::setStories(const QList<news::Story>& s, bool tags) { m_s = s; m_tags = tags; m_layoutW = -1; relayout(); update(); }
void CardList::relayout() {
  if (m_layoutW == width()) return;
  m_layoutW = width();
  m_cards.clear();
  const QFont tf = theme::sans(20, QFont::Medium);
  const int cols = width() >= 1800 ? 2 : 1;   // very wide windows: two columns (headlines stay ≤ ~900 px)
  const double cw = (width() - (cols - 1) * 9.0) / cols;
  double y = 0;
  for (int i = 0; i < m_s.size(); i += cols) {
    double rowH = 86;
    QList<Card> row;
    for (int k = 0; k < cols && i + k < m_s.size(); ++k) {
      const auto& s = m_s[i + k];
      Card c; c.x = k * (cw + 9); c.w = cw; c.y = y;
      c.lines = wrap(m_st->tx(s.title), tf, cw - 115 - 60, 2);
      const bool tag = m_tags && s.primaryTopic();
      const double block = (tag ? 18 : 0) + c.lines.size() * 27 + 21;
      rowH = std::max(rowH, block + 24);
      row << c;
    }
    for (auto& c : row) { c.h = rowH; m_cards << c; }
    y += rowH + 9;
  }
  setFixedHeight(int(std::max(0.0, y)));
}
int CardList::heightForWidth(int) const { return height(); }
void CardList::resizeEvent(QResizeEvent*) { relayout(); }
void CardList::paintEvent(QPaintEvent* ev) {
  QPainter p(this);
  const QFont tf = theme::sans(20, QFont::Medium);
  for (int i = 0; i < m_cards.size(); ++i) {
    const Card& c = m_cards[i];
    if (c.y > ev->rect().bottom() || c.y + c.h < ev->rect().top()) continue;
    const news::Story& s = m_s[i];
    const QRectF r(c.x, c.y, c.w, c.h);
    const bool hov = i == m_hover;
    p.fillRect(r, hov ? theme::map("#1a1302") : theme::map("#040302"));
    p.setPen(hov ? theme::map("#342e20") : theme::map("#241e1a")); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    const QRectF th(r.x() + 10, r.center().y() - 34, 96, 68);
    {
      QPainterPath clip; clip.addRoundedRect(th, 2, 2);
      p.save(); p.setRenderHint(QPainter::Antialiasing, true); p.setClipPath(clip);
      p.drawPixmap(th.topLeft(), storyPixmap(s, th.size().toSize(), this));
      p.restore();
    }
    const bool tag = m_tags && s.primaryTopic();
    const double block = (tag ? 18 : 0) + c.lines.size() * 27 + 21;
    double y = r.center().y() - block / 2;
    const double x = r.x() + 115;
    if (tag) {
      p.setFont(theme::mono(9, QFont::Bold, 2)); p.setPen(theme::map("#cebb96"));
      p.drawText(QRectF(x, y, 400, 14), Qt::AlignLeft | Qt::AlignVCenter, news::topicName(s.primaryTopic()));
      y += 18;
    }
    p.setFont(tf); p.setPen(hov ? theme::map("#f2df93") : kWhite);
    for (const auto& l : c.lines) { p.drawText(QRectF(x, y, r.right() - x - 50, 27), Qt::AlignLeft | Qt::AlignVCenter, l); y += 27; }
    p.setFont(theme::mono(10, QFont::Bold, 0.8)); p.setPen(kWhite);
    const QFontMetricsF fm(p.font());
    p.drawText(QRectF(x, y + 3, 400, 16), Qt::AlignLeft | Qt::AlignVCenter, sourceLabel(s.source));
    p.setFont(theme::mono(10, QFont::Normal, 0.8)); p.setPen(theme::map("#45443f"));
    p.drawText(QRectF(x + fm.horizontalAdvance(sourceLabel(s.source)) + 8, y + 3, 300, 16), Qt::AlignLeft | Qt::AlignVCenter, "·  " + ageUpper(s.time));
    if (hov) {
      const QRectF pr = plusRect(r);
      const bool queued = m_st->queue.contains(s.id);
      p.fillRect(pr, m_hoverPlus ? theme::map("#3a300c") : theme::map("#1f1805"));
      p.setPen(theme::map("#7f733a")); p.drawRect(pr.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setPen(QPen(theme::map("#dbca8e"), 1.2));
      const QPointF cc = pr.center();
      if (queued) { p.setRenderHint(QPainter::Antialiasing, true); p.drawLine(cc + QPointF(-4, 0), cc + QPointF(-1, 3)); p.drawLine(cc + QPointF(-1, 3), cc + QPointF(4, -3)); p.setRenderHint(QPainter::Antialiasing, false); }
      else { p.drawLine(cc + QPointF(-4, 0), cc + QPointF(4, 0)); p.drawLine(cc + QPointF(0, -4), cc + QPointF(0, 4)); }
    }
  }
  if (m_cards.isEmpty()) {
    p.setFont(theme::mono(11, QFont::Normal, 1)); p.setPen(theme::map("#5b5750"));
    p.drawText(QRectF(0, 0, width(), 80), Qt::AlignCenter, QCoreApplication::translate("news", "No more stories"));
  }
}
void CardList::hoverAt(const QPoint& pos) {
  m_pinHover = property("pin").toBool();
  int h = -1; bool plus = false;
  for (int i = 0; i < m_cards.size(); ++i)
    if (QRectF(m_cards[i].x, m_cards[i].y, m_cards[i].w, m_cards[i].h).contains(pos)) {
      h = i; plus = plusRect(QRectF(m_cards[i].x, m_cards[i].y, m_cards[i].w, m_cards[i].h)).contains(pos);
    }
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (h >= 0 && !plus && m_cards[h].lines.size() == 2 && m_cards[h].lines.last().endsWith("..."))
    QToolTip::showText(mapToGlobal(pos), ui::plainTip(m_st->tx(m_s[h].title)), this);
  if (h >= 0 && plus) QToolTip::showText(mapToGlobal(pos), m_st->queue.contains(m_s[h].id) ? QCoreApplication::translate("news", "In the reading queue") : QCoreApplication::translate("news", "Add to the reading queue"), this);
  if (h != m_hover || plus != m_hoverPlus) { m_hover = h; m_hoverPlus = plus; update(); }
}
void CardList::mouseMoveEvent(QMouseEvent* e) { setProperty("pin", false); hoverAt(e->position().toPoint()); }
void CardList::mousePressEvent(QMouseEvent* e) {
  hoverAt(e->position().toPoint());
  if (m_hover < 0) return;
  const QString id = m_s[m_hover].id;
  if (m_hoverPlus) {
    if (!m_st->queue.contains(id)) { m_st->queue << id; m_st->save(); emit m_st->queueChanged(); }
    update(); return;
  }
  emit m_st->openStory(id);
}
void CardList::leaveEvent(QEvent*) { if (m_pinHover) return; m_hover = -1; m_hoverPlus = false; update(); }

// ------------------------------------------------------------------------------------------------ headlines
HeadlinesPanel::HeadlinesPanel(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_anim.setInterval(16);
  connect(&m_anim, &QTimer::timeout, this, [this] {
    const double d = m_target - m_scroll;
    if (std::abs(d) < 0.6) { m_scroll = m_target; m_anim.stop(); } else m_scroll += d * 0.3;
    update();
  });
  connect(st, &FeedState::textChanged, this, [this] { if (isVisible()) { relayout(); update(); } else m_dirty = true; });
}
void HeadlinesPanel::showEvent(QShowEvent*) { if (m_dirty) { m_dirty = false; relayout(); } }
void HeadlinesPanel::setStories(const QList<news::Story>& in) {
  QList<news::Story> s;
  const QDateTime cut = QDateTime::currentDateTimeUtc().addSecs(-36 * 3600);
  for (const auto& x : in) if (x.impact != news::Impact3::None && x.time >= cut) s << x;
  std::stable_sort(s.begin(), s.end(), [](const news::Story& a, const news::Story& b) {
    if (a.impact != b.impact) return a.impact > b.impact;
    return a.time > b.time;
  });
  m_s = s.mid(0, 60);
  if (m_hover >= m_s.size()) m_hover = -1;   // the list shrank under the mouse
  m_dirty = false;
  relayout(); update();
}
void HeadlinesPanel::relayout() {
  m_rows.clear();
  const QFont f = theme::sans(16, QFont::DemiBold);
  double y = 39;
  for (const auto& s : m_s) {
    Row r; r.lines = wrap(m_st->tx(s.title), f, width() - 26, 3);
    r.y = y; r.h = 15 + 22 + (r.lines.size() - 1) * 21.5 + 19;
    y += r.h; m_rows << r;
  }
  // keep the scroll inside the new content: after the list shrank (or the panel grew) an old offset past the end
  // left the panel blank until the user scrolled back up
  const double maxS = std::max(0.0, y - height() + 10);
  m_target = std::clamp(m_target, 0.0, maxS);
  m_scroll = std::clamp(m_scroll, 0.0, maxS);
}
void HeadlinesPanel::resizeEvent(QResizeEvent*) { relayout(); }
void HeadlinesPanel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const QRectF R = rect();
  p.fillRect(R, theme::map("#040300"));
  p.setPen(theme::map("#392f24"));
  p.drawLine(QPointF(0.5, 0), QPointF(0.5, R.height())); p.drawLine(QPointF(R.width() - 0.5, 0), QPointF(R.width() - 0.5, R.height()));
  p.setPen(theme::map("#2a2316")); p.drawLine(QPointF(0, 0.5), QPointF(R.width(), 0.5));
  p.save();
  p.setClipRect(QRectF(1, 34, R.width() - 2, R.height() - 34));
  const QFont f = theme::sans(16, QFont::DemiBold);
  const QFont mf = theme::mono(10, QFont::Normal, 0.6), mb = theme::mono(10, QFont::Bold, 0.6);
  for (int i = 0; i < m_rows.size(); ++i) {
    const Row& r = m_rows[i];
    const double y = r.y - m_scroll;
    if (y > R.height() || y + r.h < 0) continue;
    const news::Story& s = m_s[i];
    if (i == m_hover) p.fillRect(QRectF(1, y, R.width() - 2, r.h), theme::map("#120d03"));
    const bool high = s.impact == news::Impact3::High;
    double x = 12;
    p.setFont(mf); p.setPen(high ? theme::map("#b06a6a") : theme::map("#c7b691"));
    const QString lbl = high ? QCoreApplication::translate("news", "HIGH IMPACT") : QCoreApplication::translate("news", "IMPORTANT");
    p.drawText(QRectF(x, y + 8, 200, 14), Qt::AlignLeft | Qt::AlignVCenter, lbl);
    x += QFontMetricsF(mf).horizontalAdvance(lbl);
    p.setPen(theme::map("#4f4b43")); p.drawText(QRectF(x, y + 8, 30, 14), Qt::AlignCenter, "·"); x += 30;
    p.setFont(mb); p.setPen(theme::map("#fefdf9")); p.drawText(QRectF(x - 2, y + 8, 200, 14), Qt::AlignLeft | Qt::AlignVCenter, sourceLabel(s.source));
    x += QFontMetricsF(mb).horizontalAdvance(sourceLabel(s.source)) - 2;
    p.setFont(mf); p.setPen(theme::map("#4f4b43"));
    {   // narrow column: the compact age ("6H" / "6 H") when the full one does not fit
      QString age = "  ·  " + ageUpper(s.time);
      if (x + QFontMetricsF(mf).horizontalAdvance(age) > R.width() - 8) age = "  ·  " + ageCompact(s.time);
      p.drawText(QRectF(x, y + 8, 200, 14), Qt::AlignLeft | Qt::AlignVCenter, age);
    }
    p.setFont(f); p.setPen(i == m_hover ? theme::map("#f2df93") : theme::map("#fefdf8"));
    double ly = y + 26;
    for (const auto& l : r.lines) { p.drawText(QRectF(12, ly, R.width() - 20, 22), Qt::AlignLeft | Qt::AlignVCenter, l); ly += 21.5; }
    p.fillRect(QRectF(12, y + r.h - 1, R.width() - 24, 1), theme::map("#1a1508"));
  }
  if (m_rows.isEmpty()) {
    p.setFont(theme::mono(10, QFont::Normal, 1)); p.setPen(theme::map("#5b5750"));
    p.drawText(QRectF(12, 40, R.width() - 24, 80), Qt::AlignLeft | Qt::TextWordWrap, QCoreApplication::translate("news", "No important headline in the last 36 hours"));
  }
  p.restore();
  // header (fixed)
  p.fillRect(QRectF(1, 1, R.width() - 2, 33), theme::map("#040300"));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#e1c35c")); p.drawEllipse(QPointF(14, 19), 2.5, 2.5);
  p.setBrush(Qt::NoBrush);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::mono(14, QFont::Bold, 3.2)); p.setPen(theme::map("#f1e096"));
  p.drawText(QRectF(21, 8, R.width(), 22), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "LATEST HEADLINES"));
  ui::drawBrackets(p, R.adjusted(0, 0, 0, 0), theme::map("#d2b44c"), 10, true, false, true, false);
}
void HeadlinesPanel::wheelEvent(QWheelEvent* e) {
  const double total = m_rows.isEmpty() ? 0 : m_rows.last().y + m_rows.last().h;
  const double maxS = std::max(0.0, total - height() + 10);
  if (!e->pixelDelta().isNull()) { m_scroll = m_target = std::clamp(m_scroll - e->pixelDelta().y(), 0.0, maxS); update(); }
  else { m_target = std::clamp(m_target - e->angleDelta().y() / 120.0 * 110, 0.0, maxS); if (!m_anim.isActive()) m_anim.start(); }
  e->accept();
}
void HeadlinesPanel::mouseMoveEvent(QMouseEvent* e) {
  int h = -1;
  const double y = e->position().y() + m_scroll;
  if (e->position().y() > 34) for (int i = 0; i < m_rows.size(); ++i) if (y >= m_rows[i].y && y < m_rows[i].y + m_rows[i].h) h = i;
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (h != m_hover && h >= 0 && h < m_s.size() && !m_rows[h].lines.isEmpty() && m_rows[h].lines.last().endsWith("..."))   // elided to 3 lines: full title
    QToolTip::showText(e->globalPosition().toPoint(), ui::plainTip(m_st->tx(m_s[h].title)), this);
  if (h != m_hover) { m_hover = h; update(); }
}
void HeadlinesPanel::mousePressEvent(QMouseEvent*) { if (m_hover >= 0 && m_hover < m_s.size()) emit m_st->openStory(m_s[m_hover].id); }
void HeadlinesPanel::leaveEvent(QEvent*) { m_hover = -1; update(); }
}  // namespace news::pages
