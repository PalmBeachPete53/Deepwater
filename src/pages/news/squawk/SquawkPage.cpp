// News › Squawk — the real-time wire (docs/reference/News/squawk.md).
// Left: price-move ticker, ALL / DATA / MOVERS / RELEASES tabs, the wire list (wire::Wire).
// Right: TERMINAL TV (channels' YouTube live streams) above a compact economic calendar (wire::releaseSource()).
//
// Page hooks (tests): DW_SQUAWK_SCROLL=<list px>[,<calendar px>]  DW_SQUAWK_HOVER=<x>,<y> (page coords)
//                     DW_SQUAWK_TAB=<0..3>  DW_SQUAWK_NO_TV=1 (do not load the player)
#include "app/Pages.h"
#include "pages/news/squawk/CalendarPane.h"
#include "pages/news/squawk/SquawkLook.h"
#include "pages/news/squawk/SquawkWidgets.h"
#include "pages/news/squawk/TvPane.h"
#include "core/Settings.h"
#include "wire/ReleaseSource.h"
#include "wire/Wire.h"
#include <QCoreApplication>
#include <QPainter>
#include "core/I18n.h"
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace wire::squawk {
class SquawkPage : public QWidget {
public:
  explicit SquawkPage(QWidget* parent) : QWidget(parent) {
    m_left = new QWidget(this);
    auto* lv = new QVBoxLayout(m_left);
    lv->setContentsMargins(0, 0, 0, 0); lv->setSpacing(0);
    m_header = new Header(m_left);
    m_ticker = new Ticker(m_left);
    m_tabs = new FilterTabs(m_left);
    m_list = new WireList(m_left);
    m_footer = new Footer(m_left);
    lv->addWidget(m_header); lv->addWidget(m_ticker); lv->addWidget(m_tabs); lv->addWidget(m_list, 1); lv->addWidget(m_footer);
    m_tv = new TvPane(this);
    m_tv->onIdleChanged = [this] { resizeEvent(nullptr); };
    m_cal = new CalendarPane(this);

    m_tabs->setCurrent(qBound(0, qEnvironmentVariableIsSet("DW_SQUAWK_TAB") ? qEnvironmentVariableIntValue("DW_SQUAWK_TAB")
                                                                             : settings::value("news.squawk.filter", 1).toInt(), 3));
    connect(m_tabs, &FilterTabs::changed, this, [this](int i) {
      settings::setValue("news.squawk.filter", i);
      m_list->scrollToTop();
      refresh();
    });
    auto& w = wire::Wire::instance();
    connect(&w, &wire::Wire::changed, this, [this] { if (isVisible()) refresh(); else m_dirty = true; });
    connect(&w, &wire::Wire::moversChanged, this, [this] { if (isVisible()) m_ticker->setMoves(wire::Wire::instance().movers()); });
    connect(&wire::Hooks::instance(), &wire::Hooks::translatorChanged, this, [this] { updateLanguage(); });
    connect(&wire::Hooks::instance(), &wire::Hooks::releaseSourceChanged, this, [this] { refresh(); });
    connect(m_header, &Header::languageToggled, this, [this] {
      if (!wire::translator() || !wire::translator()->available()) return;
      m_want = m_lang == "EN" ? "FR" : "EN";   // session only: the default is the interface language
      updateLanguage();
    });
    m_want = i18n::code() == "fr" ? "FR" : "EN";   // headlines follow the interface language by default
    m_minute.setSingleShot(true);
    connect(&m_minute, &QTimer::timeout, this, [this] { m_list->refreshAges(); armMinute(); });
    refresh();
    updateLanguage();
  }

protected:
  void resizeEvent(QResizeEvent*) override {
    // Size classes (reference 1600x966: 933 | 665). The wire list keeps 58 % of the width but never less than
    // 520 px nor more than 1180 px (headlines wrap at ≤ 950 px); the right column keeps ≥ 460 px. The TV grows
    // with its column up to 42 % of the height, and shrinks (down to 120 px of video) so the calendar always keeps
    // its filters and two rows. On very wide windows the calendar is capped at 1200 px and centred under the TV.
    const int W = width(), H = height();
    int lw = qBound(520, int(std::round(W * 933.0 / 1600.0)), 1180);
    lw = qMin(lw, W - 462);
    m_left->setGeometry(0, 0, lw, H);
    const int rx = lw + 2, rw = W - rx;
    const int calMin = 178 + 28 + 37 + 2 * 46 + 2;   // filters, footer, day header, two compact rows
    // no live stream on the channel: the TV keeps a message band only, the calendar gets the height
    const int tvh = m_tv->idle() ? m_tv->idleHeight()
                                 : qMax(m_tv->minimumHeightFor(), qMin(m_tv->preferredHeight(rw, H), H - 2 - calMin));
    m_tv->setGeometry(rx, 0, rw, tvh);
    const int cw = qMin(rw, 1200);
    m_cal->setGeometry(rx + (rw - cw) / 2, tvh + 2, cw, H - tvh - 2);
    update();
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), look().page);
    p.fillRect(QRect(m_left->width(), 0, 2, height()), look().divider);
    p.fillRect(QRect(m_tv->x(), m_tv->height(), m_tv->width(), 2), theme::map("#100c06"));
  }
  void showEvent(QShowEvent*) override {
    wire::Wire::instance().setActive(this, true);
    if (m_dirty) refresh();
    m_ticker->setMoves(wire::Wire::instance().movers());
    m_ticker->setRunning(true);
    if (!qEnvironmentVariableIsSet("DW_SQUAWK_NO_TV")) m_tv->setPlaying(true);
    m_cal->setRunning(true);
    m_list->refreshAges();
    armMinute();
    if (!m_hooksDone) { m_hooksDone = true; QTimer::singleShot(qEnvironmentVariableIntValue("DW_SQUAWK_HOOK_MS") > 0 ? qEnvironmentVariableIntValue("DW_SQUAWK_HOOK_MS") : 5000, this, [this] { applyHooks(); }); }
  }
  void hideEvent(QHideEvent*) override {
    wire::Wire::instance().setActive(this, false);
    m_ticker->setRunning(false);
    m_tv->setPlaying(false);
    m_cal->setRunning(false);
    m_minute.stop();
  }

private:
  wire::Filter filter() const { return wire::Filter(m_tabs->current()); }
  void refresh() {
    m_dirty = false;
    auto& w = wire::Wire::instance();
    const QVector<int> n = w.counts();
    const int all = n[0];
    m_tabs->setCounts({all, n[1], n[2], w.releasesConnected() ? n[3] : -1});
    m_header->setCount(all);
    m_list->setItems(w.items(filter()));
    QString empty;
    switch (filter()) {
      case wire::Filter::Releases:
        empty = w.releasesConnected() ? QCoreApplication::translate("wire", "No release with a published actual in the last 24 hours.")
                                      : QCoreApplication::translate("wire", "Calendar connecting…\nReleases appear here once the News calendar feeds the wire.");
        break;
      case wire::Filter::Movers:
        empty = QCoreApplication::translate("wire", "No price move beyond its threshold yet.\nIndices 1% · stocks and commodities 2% · crypto 3% · FX majors 0.5% "
                "(change vs previous close, from live quotes while a wire page is open).");
        break;
      default:
        empty = w.feedErrors().isEmpty() ? QCoreApplication::translate("wire", "Connecting to the wire…")
                                         : QCoreApplication::translate("wire", "Wire unreachable: %1").arg(w.feedErrors().join(" · "));
    }
    m_list->setEmptyText(empty);
    const QDateTime stamp = w.lastFetch().isValid() ? w.lastFetch() : w.lastReceived();
    m_footer->setStamp(stamp, w.feedErrors().size() < 5 && stamp.isValid());
    if (m_lang == "FR") translateVisible();
  }
  void updateLanguage() {
    // a registered translator that cannot translate (no French pack) left "FR" lit over English headlines
    const bool avail = wire::translator() && wire::translator()->available();
    m_lang = avail ? m_want : QStringLiteral("EN");
    m_header->setLanguage(m_lang, avail);
    if (m_lang == "FR") translateVisible();
    m_list->setTranslations(m_lang == "FR" ? &m_fr : nullptr);
  }
  void translateVisible() {
    auto* t = wire::translator();
    if (!t || !t->available()) return;
    QStringList ids, texts;
    for (const auto& i : wire::Wire::instance().items(wire::Filter::Data, 300))
      if (!m_fr.contains(i.id) && !m_pending.contains(i.id)) { ids << i.id; texts << i.text; m_pending.insert(i.id); }
    for (int k = 0; k < texts.size(); k += 50) {
      const QStringList idc = ids.mid(k, 50);
      t->translate(texts.mid(k, 50), "fr", this, [this, idc, src = texts.mid(k, 50)](const QStringList& out) {
        // an original handed back (translation failed) is not stored: it would pass for a translation and never be
        // asked again once the translator works
        for (int j = 0; j < idc.size(); ++j) {
          m_pending.remove(idc[j]);
          if (j < out.size() && out[j] != src.value(j)) m_fr.insert(idc[j], out[j]);
        }
        if (m_lang == "FR") m_list->setTranslations(&m_fr);
      });
    }
  }
  void armMinute() {
    const QTime t = QTime::currentTime();
    m_minute.start((60 - t.second()) * 1000 - t.msec() + 30);
  }
  void applyHooks() {
    const QStringList sc = qEnvironmentVariable("DW_SQUAWK_SCROLL").split(',', Qt::SkipEmptyParts);
    if (sc.size() >= 1) m_list->scrollBy(sc[0].toInt());
    if (sc.size() >= 2) m_cal->rows()->scrollBy(sc[1].toInt());
    const QStringList hv = qEnvironmentVariable("DW_SQUAWK_HOVER").split(',', Qt::SkipEmptyParts);
    if (hv.size() == 2) {
      const QPoint pt(hv[0].toInt(), hv[1].toInt());
      QTimer::singleShot(400, this, [this, pt] {
        const QPoint inTabs = m_tabs->mapFrom(this, pt);
        if (m_tabs->rect().contains(inTabs)) m_tabs->setHoverAt(inTabs);
        const QPoint inList = m_list->viewport()->mapFrom(this, pt);
        if (m_list->viewport()->rect().contains(inList)) m_list->setHoverAt(inList);
        const QPoint inCal = m_cal->rows()->viewport()->mapFrom(this, pt);
        if (m_cal->rows()->viewport()->rect().contains(inCal)) m_cal->rows()->setHoverAt(inCal);
      });
    }
  }

  QWidget* m_left;
  Header* m_header;
  Ticker* m_ticker;
  FilterTabs* m_tabs;
  WireList* m_list;
  Footer* m_footer;
  TvPane* m_tv;
  CalendarPane* m_cal;
  QTimer m_minute;
  QString m_lang = "EN", m_want = "EN";
  QHash<QString, QString> m_fr;
  QSet<QString> m_pending;
  bool m_dirty = false, m_hooksDone = false;
};
}

DW_PAGE("news/squawk", wire::squawk::SquawkPage)
