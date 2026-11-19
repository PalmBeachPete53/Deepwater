// Global › Session Recap — a post-mortem of each Tokyo / London / New York session
// (docs/reference/Global/session-recap.md). Left: registry of sessions (or weeks) with their top mover.
// Centre: the session file (automated read, window timeline, moves of the window). Right: releases and wire
// headlines inside the window.
//
// Page hooks (tests): DW_RECAP_SCROLL=<px>  DW_RECAP_HOVER=<x in the centre view>  DW_RECAP_TRACE=<asset 0..11>
//                     DW_RECAP_LANG=FR  DW_RECAP_TAB=1 (weeks)  DW_RECAP_PICK=<row n of the registry>
#include "app/Preload.h"
#include <QPointer>
#include "app/Pages.h"
#include "pages/global/session-recap/CentreView.h"
#include "pages/global/session-recap/RecapWidgets.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include <QDir>
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QPainter>
#include "core/I18n.h"
#include <QTimer>

namespace wire::recap {

class Strip : public QWidget {
public:
  explicit Strip(QWidget* parent) : QWidget(parent) { setFixedHeight(37); }
  void setBlink(bool on) { m_on = on; update(); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    QLinearGradient g(0, 0, width(), 0); g.setColorAt(0, theme::map("#100a03")); g.setColorAt(1, theme::map("#161005"));
    p.fillRect(rect(), g);
    p.fillRect(QRect(0, height() - 2, width(), 2), theme::map("#3e381d"));
    p.setRenderHint(QPainter::Antialiasing);
    // ✣
    p.setPen(QPen(theme::map("#a07a1c"), 1.3));
    const QPointF c(19, 17);
    for (int k = 0; k < 4; ++k) { const qreal a = k * M_PI / 2 + M_PI / 4; p.drawLine(c, c + QPointF(std::cos(a) * 5.5, std::sin(a) * 5.5)); }
    p.drawLine(c + QPointF(-3.5, 0), c + QPointF(3.5, 0)); p.drawLine(c + QPointF(0, -3.5), c + QPointF(0, 3.5));
    p.setPen(Qt::NoPen); p.setBrush(m_on ? theme::map("#d9c25a") : theme::map("#5e5523")); p.drawEllipse(QPointF(38, 17.5), 3, 3);
    p.setFont(theme::mono(11, QFont::Normal, 3.1)); p.setPen(theme::map("#5e5a50"));
    // badge first (the title is elided before it on narrow windows)
    const QString b = QCoreApplication::translate("wire", "• TOKYO · LONDON · NEW YORK · %1").arg(wire::utcLabel());
    const QFont f = theme::mono(10, QFont::Bold, 1.6);
    const qreal w = QFontMetricsF(f).horizontalAdvance(b) + 18;
    const QRectF r(width() - 15 - w, 9, w, 19);
    const QString full = QCoreApplication::translate("wire", "SESSION RECAP — AUTOMATED READ OF EACH SESSION");
    const QString t = QFontMetricsF(p.font()).elidedText(full, Qt::ElideRight, r.left() - 70);
    setToolTip(t != full ? full : QString());
    p.drawText(QPointF(50, 22), t);
    if (m_on) p.fillRect(QRectF(50 + QFontMetricsF(p.font()).horizontalAdvance(t) + 2, 10, 7, 14), theme::map("#f5e392"));
    QLinearGradient bg(0, r.top(), 0, r.bottom()); bg.setColorAt(0, theme::map("#554600")); bg.setColorAt(1, theme::map("#342100"));
    p.setPen(QPen(theme::map("#736935"), 1)); p.setBrush(bg); p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 2, 2);
    p.setFont(f); p.setPen(theme::map("#efdd99")); p.drawText(r, Qt::AlignCenter, b);
  }
private:
  bool m_on = true;
};

class RecapPage : public QWidget, public app::Preloadable {
public:
  // app::Preloadable: the sessions' bars, ahead of a first visit (the list is built when the model changes)
  void preload() override { if (!m_loaded) { m_loaded = true; load(false); } }
  explicit RecapPage(QWidget* parent) : QWidget(parent) {
    m_strip = new Strip(this);
    m_head = new RegistryHead(this);
    m_list = new RegistryList(this);
    m_centre = new CentreView(this);
    m_rel = new ReleasesBox(this);
    m_wire = new WireBox(this);
    m_model = new Model(this);
    m_tab = qEnvironmentVariableIsSet("DW_RECAP_TAB") ? qEnvironmentVariableIntValue("DW_RECAP_TAB") : 0;
    m_head->setTab(m_tab);
    // the read is written in the interface language; the in-card EN/FR toggle switches it for this session only
    m_centre->setLang(qEnvironmentVariableIsSet("DW_RECAP_LANG") ? qEnvironmentVariable("DW_RECAP_LANG")
                                                                 : QString(i18n::code() == "fr" ? "FR" : "EN"));
    connect(m_head, &RegistryHead::tabChanged, this, [this](int t) { m_tab = t; m_selKey.clear(); rebuildList(); reloadIfStale(); });
    connect(m_list, &RegistryList::picked, this, [this](const Window& w) { select(w); });
    connect(m_model, &Model::changed, this, [this] { rebuildList(); refreshWindow(); });
    connect(&wire::Wire::instance(), &wire::Wire::changed, this, [this] { if (isVisible() && m_sel.live()) refreshWindow(); });
    connect(&wire::Hooks::instance(), &wire::Hooks::releaseSourceChanged, this, [this] { hookReleases(); refreshWindow(); });
    hookReleases();
    m_blink.setInterval(530);
    connect(&m_blink, &QTimer::timeout, this, [this] { m_on = !m_on; m_strip->setBlink(m_on); m_head->setBlink(m_on); });
    m_reload.setInterval(120000);
    // sessions that weren't over at the last load (live, or begun since): fetch the bars again; the list then gains
    // the sessions that have started since (it is rebuilt when the model changes)
    connect(&m_reload, &QTimer::timeout, this, [this] { reloadIfStale(); });
  }

protected:
  // Column widths per size class (reference 1600: 317 | centre | 373).
  //  < 1366 wide: two columns — the releases and wire boxes stack under the registry in the left column.
  //  1920+: side columns widen a little (to 380 / 460) so the centre does not swallow all the extra width.
  int leftW() const { return width() < 1366 ? 300 : qBound(317, 317 + (width() - 1600) / 12, 380); }
  int rightW() const { return width() < 1366 ? 0 : qBound(373, 373 + (width() - 1600) / 8, 460); }
  void resizeEvent(QResizeEvent*) override {
    const int W = width(), H = height();
    const int lw = leftW(), rw = rightW();
    m_strip->setGeometry(0, 0, W, 37);
    m_head->setGeometry(0, 37, lw, 71);
    if (rw == 0) {   // stacked: registry ≈ 40 %, releases, wire the rest
      // whole rows at the top of the list (day band 22 + session rows 51, RecapWidgets.cpp), so the last visible
      // session isn't sliced by the RELEASES header on short windows (the list still scrolls)
      const int avail = H - 108;
      const int listH = 22 + qMax(2, (int(avail * 0.40) - 22) / 51) * 51;
      m_list->setGeometry(0, 108, lw, listH);
    } else {
      m_list->setGeometry(0, 108, lw, H - 108);
    }
    m_centre->setGeometry(lw + 1, 37, W - lw - rw - (rw ? 2 : 1), H - 37);
    layoutRight();
    update();
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), theme::map("#050402"));
    p.fillRect(QRect(leftW(), 37, 1, height()), theme::map("#110f0b"));
    if (rightW()) p.fillRect(QRect(width() - rightW() - 1, 37, 1, height()), theme::map("#181714"));
    else p.fillRect(QRect(0, m_rel->y() - 2, leftW(), 1), theme::map("#3a3214"));
  }
  void showEvent(QShowEvent*) override {
    wire::Wire::instance().setActive(this, true);
    m_blink.start(); m_reload.start();
    if (!m_loaded) { m_loaded = true; load(false); rebuildList(); }
    else { rebuildList(); reloadIfStale(); }
    if (!m_hooks) { m_hooks = true; QTimer::singleShot(qEnvironmentVariableIsSet("DW_RECAP_HOOK_MS") ? qEnvironmentVariableIntValue("DW_RECAP_HOOK_MS") : 5000, this, [this] { applyHooks(); }); }
  }
  void hideEvent(QHideEvent*) override {
    wire::Wire::instance().setActive(this, false);
    m_blink.stop(); m_reload.stop();
  }

private:
  void layoutRight() {
    const int H = height();
    if (rightW() == 0) {   // stacked under the registry list
      const int top = m_list->geometry().bottom() + 4, w = leftW();
      const int relH = qMin(m_rel->preferredHeight(), wholeReleases(int((H - top) * 0.55)));
      m_rel->setGeometry(0, top, w, relH);
      m_wire->setGeometry(0, top + relH + 2, w, H - top - relH - 2);
      return;
    }
    const int rw = rightW(), x = width() - rw;
    const int relH = qMin(m_rel->preferredHeight(), wholeReleases(int((H - 37) * 0.45)));
    m_rel->setGeometry(x, 40, rw, relH);
    m_wire->setGeometry(x, 40 + relH + 4, rw, H - 40 - relH - 4);
  }
  // Height of the releases box for at most `h` px, in whole items (header 33 + 52 per item + 4, as
  // ReleasesBox::preferredHeight), at least one: a capped box no longer cuts an item through its text.
  static int wholeReleases(int h) { return 33 + qMax(1, (h - 37) / 52) * 52 + 4; }
  void hookReleases() {
    // Qt::UniqueConnection does not work with lambdas (the connect fails): keep the source we are hooked to instead.
    auto* s = wire::releaseSource();
    if (!s || s == m_hookedSource) return;
    m_hookedSource = s;
    connect(s, &wire::ReleaseSource::changed, this, [this] { if (isVisible()) refreshWindow(); });
  }
  QPointer<wire::ReleaseSource> m_hookedSource;
  void load(bool live) { m_lastLoad = QDateTime::currentDateTimeUtc(); m_model->load(live); }
  void reloadIfStale() {
    if (!m_loaded) return;
    for (const auto& w : windows())
      // until 20 min after a close: Yahoo's last bars of a session land late, a load made just after the close (or
      // before it) kept the pre-close bars for hours
      if (!m_lastLoad.isValid() || w.end.addSecs(20 * 60) > m_lastLoad) { load(true); return; }
  }
  QList<Window> windows() const { return m_tab == 0 ? sessionWindows(62) : weekWindows(9); }
  void rebuildList() {
    QList<RegistryList::Row> rows;
    const auto ws = windows();
    QDate day;
    QString monthKey;
    const QLocale en = i18n::locale();
    for (const auto& w : ws) {
      if (m_tab == 0 && w.session.date != day) {
        day = w.session.date;
        RegistryList::Row h; h.header = true; h.label = en.toString(day, QCoreApplication::translate("wire", "ddd dd MMM")).toUpper(); rows << h;
      } else if (m_tab == 1 && en.toString(w.monday, "MMMM yyyy") != monthKey) {
        monthKey = en.toString(w.monday, "MMMM yyyy");
        RegistryList::Row h; h.header = true; h.label = monthKey.toUpper(); rows << h;
      }
      RegistryList::Row r; r.w = w;
      const auto st = m_model->stats(w);
      const int top = m_model->topMover(st);
      if (m_tab == 0) {
        r.label = w.session.name();
        r.line2 = w.start.toLocalTime().toString("HH:mm") + "-" + w.end.toLocalTime().toString("HH:mm") + " " + wire::utcLabel(w.start.toLocalTime());
      } else {
        r.label = QCoreApplication::translate("wire", "WEEK %1").arg(w.monday.weekNumber());
        r.line2 = en.toString(w.monday, QCoreApplication::translate("wire", "d MMM")) + " – " + en.toString(w.monday.addDays(4), QCoreApplication::translate("wire", "d MMM"));
      }
      if (top >= 0) { r.line2 += " · " + assetName(top); r.chg = st[top].chg; }
      else if (!m_model->loaded()) r.line2 += " · …";
      rows << r;
    }
    m_list->setRows(rows);
    if (m_selKey.isEmpty() && !ws.isEmpty()) select(ws.first());
    else m_list->setSelected(m_selKey);
  }
  void select(const Window& w) {
    m_sel = w; m_selKey = w.key();
    m_list->setSelected(m_selKey);
    if (auto* s = wire::releaseSource()) s->ensure(w.start, w.end);
    refreshWindow();
  }
  void refreshWindow() {
    if (m_selKey.isEmpty()) return;
    const auto rel = windowReleases(m_sel);
    const auto heads = windowHeadlines(m_sel);
    m_centre->setWindow(m_sel, m_model->stats(m_sel), rel, heads, wire::releaseSource() != nullptr, !m_model->loaded(), {});
    m_rel->setData(rel, wire::releaseSource() != nullptr);
    QString empty = QCoreApplication::translate("wire", "No headline archived for this window.");
    // The note says the archive does not reach back to this window. Reading (and sorting) 80 days of archive at every
    // refresh only to decide it was the cost; the day files' names say it (wire/YYYY-MM-DD.jsonl, one per UTC day).
    // Only needed when the window has no headline.
    if (heads.isEmpty()) {
      const QStringList days = QDir(paths::data("wire")).entryList({"\?\?\?\?-\?\?-\?\?.jsonl"}, QDir::Files, QDir::Name);
      const QDate first = days.isEmpty() ? QDate() : QDate::fromString(days.first().left(10), "yyyy-MM-dd");
      if (!first.isValid() || first > m_sel.start.toUTC().date())
        empty += "\n" + QCoreApplication::translate("wire", "The wire archive starts when Deepwater first ran with the wire open; it keeps 70 days.");
    }
    m_wire->setData(heads, empty);
    layoutRight();
  }
  void applyHooks() {
    if (qEnvironmentVariableIsSet("DW_RECAP_PICK")) {
      const auto ws = windows();
      const int n = qEnvironmentVariableIntValue("DW_RECAP_PICK");
      if (n >= 0 && n < ws.size()) select(ws[n]);
    }
    if (qEnvironmentVariableIsSet("DW_RECAP_TRACE")) m_centre->setTraced(qEnvironmentVariableIntValue("DW_RECAP_TRACE"));
    if (qEnvironmentVariableIsSet("DW_RECAP_SCROLL")) m_centre->scrollBy(qEnvironmentVariableIntValue("DW_RECAP_SCROLL"));
    if (qEnvironmentVariableIsSet("DW_RECAP_HOVER")) {
      const int x = qEnvironmentVariableIntValue("DW_RECAP_HOVER");
      QTimer::singleShot(400, this, [this, x] { m_centre->setHoverX(x); });
    }
  }

  Strip* m_strip;
  RegistryHead* m_head;
  RegistryList* m_list;
  CentreView* m_centre;
  ReleasesBox* m_rel;
  WireBox* m_wire;
  Model* m_model;
  Window m_sel;
  QString m_selKey;
  int m_tab = 0;
  bool m_loaded = false, m_hooks = false, m_on = true;
  QDateTime m_lastLoad;   // UTC, last fetch of the bars
  QTimer m_blink, m_reload;
};
}

DW_PAGE("global/session-recap", wire::recap::RecapPage)
