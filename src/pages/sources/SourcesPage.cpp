// Sources › Data Sources: every outside source declared with sources::Registrar, with its live status
// (active / failing / disabled / not contacted yet), what Deepwater takes from it, the pages using it, its terms,
// a switch to turn it off, and the key or account fields of the sources that need one.
// Keys are stored as settings "keys.<name>" and never shown back; the Myfxbook password goes once to Myfxbook
// through positioning::Retail and is never stored.
#include "app/Pages.h"
#include "net/Sources.h"
#include "core/Settings.h"
#include "positioning/Retail.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <algorithm>

namespace sourcespage {
namespace {

QString statusText(sources::Status s) {
  switch (s) {
    case sources::Status::Active: return QCoreApplication::translate("sources", "ACTIVE");
    case sources::Status::Failing: return QCoreApplication::translate("sources", "FAILING");
    case sources::Status::Disabled: return QCoreApplication::translate("sources", "DISABLED");
    case sources::Status::Waiting: return QCoreApplication::translate("sources", "NOT CONTACTED");
  }
  return {};
}
QColor statusColor(sources::Status s) {
  const auto& c = theme::pal();
  switch (s) {
    case sources::Status::Active: return c.up;
    case sources::Status::Failing: return c.down;
    case sources::Status::Disabled: return c.textMute;
    case sources::Status::Waiting: return c.textDim;
  }
  return c.textDim;
}
QString accessText(sources::Access a) {
  switch (a) {
    case sources::Access::Keyless: return QCoreApplication::translate("sources", "FREE");
    case sources::Access::Key: return QCoreApplication::translate("sources", "API KEY");
    case sources::Access::Account: return QCoreApplication::translate("sources", "ACCOUNT");
    case sources::Access::Bundled: return QCoreApplication::translate("sources", "BUNDLED");
    case sources::Access::Forbidden: return QCoreApplication::translate("sources", "LINK ONLY");
  }
  return {};
}
QString ago(const QDateTime& t) {
  if (!t.isValid()) return QStringLiteral("—");
  const qint64 s = t.secsTo(QDateTime::currentDateTimeUtc());
  if (s < 60) return QCoreApplication::translate("sources", "%1 s ago").arg(qMax<qint64>(0, s));
  if (s < 3600) return QCoreApplication::translate("sources", "%1 min ago").arg(s / 60);
  if (s < 86400) return QCoreApplication::translate("sources", "%1 h ago").arg(s / 3600);
  return QCoreApplication::translate("sources", "%1 d ago").arg(s / 86400);
}
QString pageNames(const QStringList& ids) {
  QStringList out;
  for (const auto& id : ids)
    for (const auto& m : app::tabs()) for (const auto& sub : m.subs)
      if (app::pageId(m.id, sub.id) == id) out << app::label(m.label) + " › " + app::label(sub.label);
  return out.join("  ·  ");
}
// Status shown for a source: the network health, except for account sources whose state is the account's
// (Myfxbook's calendar RSS shares the host and answers even when the account is not connected).
sources::Status effectiveStatus(const sources::Source& s) {
  if (sources::isDisabled(s.id)) return sources::Status::Disabled;
  if (s.id == "myfxbook") {
    using S = positioning::Retail::State;
    switch (positioning::Retail::instance().state()) {
      case S::Connected: return sources::Status::Active;
      case S::Error: case S::Expired: return sources::Status::Failing;
      default: return sources::Status::Waiting;
    }
  }
  return sources::status(s.id);
}
QString shortError(const QString& e) { return e.size() > 140 ? e.left(137) + "…" : e; }

QString lineEditQss() {
  const auto& c = theme::pal();
  return QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 3px; padding: 5px 8px; color: %3;"
                 " font-family: 'JetBrains Mono'; font-size: 12px; } QLineEdit:focus { border-color: %4; }")
      .arg(c.panelAlt.name(), c.border.name(), c.text.name(), c.accent.name());
}
QPushButton* button(const QString& text, bool primary, QWidget* parent) {
  auto* b = new QPushButton(text, parent);
  const auto& c = theme::pal();
  b->setCursor(Qt::PointingHandCursor);
  b->setFont(theme::mono(11, QFont::Bold, 1.2));
  b->setStyleSheet(primary
    ? QString("QPushButton { background: %1; color: %2; border: 0; border-radius: 3px; padding: 6px 14px; }"
              "QPushButton:hover { background: %3; } QPushButton:disabled { background: %4; color: %5; }")
          .arg(c.accent.name(), theme::onAccent().name(), theme::mix(c.accent, c.text, 0.2).name(), c.panelAlt.name(), c.textMute.name())
    : QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 3px; padding: 5px 12px; }"
              "QPushButton:hover { color: %3; border-color: %4; }")
          .arg(c.textDim.name(), c.border.name(), c.text.name(), c.borderHi.name()));
  return b;
}

// ---------------------------------------------------------------------------------------------- status pill
class StatusPill : public QWidget {
public:
  explicit StatusPill(QWidget* p) : QWidget(p) { setFixedSize(132, 22); }
  void set(sources::Status s) { m_s = s; update(); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QColor col = statusColor(m_s);
    const QString t = statusText(m_s);
    const QFont f = theme::mono(10, QFont::Bold, 1.4);
    const qreal w = QFontMetricsF(f).horizontalAdvance(t) + 26;
    const QRectF r(0.5, 2.5, w, height() - 5);
    p.setPen(QPen(theme::alpha(col, 120), 1)); p.setBrush(theme::alpha(col, 34));
    p.drawRoundedRect(r, 3, 3);
    p.setPen(Qt::NoPen); p.setBrush(col); p.drawEllipse(QPointF(r.left() + 10, r.center().y()), 3.5, 3.5);
    p.setFont(f); p.setPen(col);
    p.drawText(r.adjusted(18, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, t);
  }
private:
  sources::Status m_s = sources::Status::Waiting;
};

// ---------------------------------------------------------------------------------------------- on/off switch
class Switch : public QWidget {
public:
  std::function<void(bool)> toggled;
  explicit Switch(QWidget* p) : QWidget(p) { setFixedSize(38, 20); setCursor(Qt::PointingHandCursor); }
  void setOn(bool on) { m_on = on; update(); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r(1, 2, width() - 2, height() - 4);
    p.setPen(QPen(m_on ? c.accent : c.border, 1)); p.setBrush(m_on ? theme::alpha(c.accent, 70) : c.panelAlt);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    const qreal d = r.height() - 4;
    const QPointF k(m_on ? r.right() - 2 - d / 2 : r.left() + 2 + d / 2, r.center().y());
    p.setPen(Qt::NoPen); p.setBrush(m_on ? c.accent : c.textMute); p.drawEllipse(k, d / 2, d / 2);
  }
  void mousePressEvent(QMouseEvent*) override { m_on = !m_on; update(); if (toggled) toggled(m_on); }
private:
  bool m_on = true;
};

// ---------------------------------------------------------------------------------------------- one source row
class Row : public QFrame {
public:
  sources::Source src;
  explicit Row(const sources::Source& s, QWidget* parent) : QFrame(parent), src(s) {
    const auto& c = theme::pal();
    setObjectName("row");
    auto* g = new QGridLayout(this);
    g->setContentsMargins(14, 10, 14, 10); g->setHorizontalSpacing(16); g->setVerticalSpacing(4);
    m_pill = new StatusPill(this);
    g->addWidget(m_pill, 0, 0, Qt::AlignTop);

    auto* name = ui::label(s.publisher, theme::condensed(16, QFont::Bold), c.text, this);
    auto* meta = ui::label(s.id + "  ·  " + (s.hosts.isEmpty() ? QString("—") : s.hosts.join(", ")) + "  ·  " +
                               QCoreApplication::translate("sources", "1 request / %1").arg(paceText(s.paceMs)),
                           theme::mono(10), c.textMute, this);
    meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    meta->setWordWrap(true);   // a source with many hosts (news-images) must not hold every card wider than the window
    auto* nameBox = new QVBoxLayout; nameBox->setSpacing(2);
    nameBox->addWidget(name); nameBox->addWidget(meta);
    g->addLayout(nameBox, 0, 1);

    auto* access = ui::label(accessText(s.access), theme::mono(10, QFont::Bold, 1.4),
                             s.access == sources::Access::Keyless ? c.textDim : c.accentText, this);
    access->setAlignment(Qt::AlignRight | Qt::AlignTop);
    g->addWidget(access, 0, 2, Qt::AlignTop | Qt::AlignRight);

    m_switch = new Switch(this);
    m_switch->setToolTip(QCoreApplication::translate("sources", "Turn the source off: Deepwater stops contacting it."));
    m_switch->toggled = [this](bool on) { sources::setDisabled(src.id, !on); };
    g->addWidget(m_switch, 0, 3, Qt::AlignTop | Qt::AlignRight);

    auto* data = ui::label(s.data, theme::sans(12), c.textDim, this);
    data->setWordWrap(true);
    g->addWidget(data, 1, 1, 1, 3);
    if (!s.pages.isEmpty()) {
      auto* pages = ui::label(QCoreApplication::translate("sources", "Used by: %1").arg(pageNames(s.pages)),
                              theme::sans(11), c.textMute, this);
      pages->setWordWrap(true);
      g->addWidget(pages, 2, 1, 1, 3);
    }
    if (!s.terms.isEmpty()) {
      auto* terms = ui::label(QCoreApplication::translate("sources", "Terms: %1").arg(s.terms), theme::sans(11), c.textMute, this);
      terms->setWordWrap(true);
      g->addWidget(terms, 3, 1, 1, 3);
    }
    m_health = ui::label(QString(), theme::mono(10), c.textDim, this);
    m_health->setWordWrap(true);
    g->addWidget(m_health, 4, 1, 1, 3);

    if (!s.keyName.isEmpty()) addKeyField(g);   // keyed sources, and optional tokens (Hugging Face)
    if (s.id == "myfxbook") addAccountFields(g);
    g->setColumnStretch(1, 1);
    refresh();
  }

  void refresh() {
    const auto& c = theme::pal();
    const auto st = effectiveStatus(src);
    m_pill->set(st);
    m_switch->setOn(!sources::isDisabled(src.id));
    const auto h = sources::health(src.id);
    QStringList parts;
    if (h.lastOk.isValid()) parts << QCoreApplication::translate("sources", "Last answer %1").arg(ago(h.lastOk));
    if (h.lastFail.isValid())
      parts << QCoreApplication::translate("sources", "last failure %1 (%2)").arg(ago(h.lastFail), shortError(h.lastError));
    if (h.okCount + h.failCount > 0)
      parts << QCoreApplication::translate("sources", "%1 answers, %2 failures recorded").arg(h.okCount).arg(h.failCount);
    if (parts.isEmpty()) parts << QCoreApplication::translate("sources", "Not contacted yet: it is read when a page that uses it opens.");
    QString text = parts.join("  ·  ");
    const QStringList shared = sources::sharingHosts(src.id);
    if (st == sources::Status::Disabled && !shared.isEmpty())
      text += "\n" + QCoreApplication::translate("sources", "Shares its host with %1, which are stopped too while this one is off.").arg(shared.join(", "));
    if (src.access == sources::Access::Key && settings::apiKey(src.keyName).isEmpty())
      text += "\n" + QCoreApplication::translate("sources", "No key saved: the data it provides stays empty.");
    if (m_health->text() != text) m_health->setText(text);
    m_health->setToolTip(h.lastError.size() > 140 ? h.lastError : QString());
    // a style sheet re-polishes the widget and its children: set only when the colours change
    const QColor hc = st == sources::Status::Failing ? c.down : c.textDim;
    const QString healthQss = QString("color:%1;background:transparent;").arg(hc.name());
    if (healthQss != m_healthQss) { m_healthQss = healthQss; m_health->setStyleSheet(healthQss); }
    const QString rowQss = QString("QFrame#row { background: %1; border: 1px solid %2; border-radius: 3px; }")
                               .arg((st == sources::Status::Disabled ? c.bg : c.panel).name(), c.border.name());
    if (rowQss != m_rowQss) { m_rowQss = rowQss; setStyleSheet(rowQss); }
    if (m_retailState) updateAccount();
  }

private:
  static QString paceText(int ms) {
    if (ms <= 0) return QCoreApplication::translate("sources", "no limit");
    if (ms < 1000) return QStringLiteral("%1 ms").arg(ms);
    if (ms < 60000) return QStringLiteral("%1 s").arg(ms / 1000.0, 0, 'g', 3);
    return QStringLiteral("%1 min").arg(ms / 60000.0, 0, 'g', 3);
  }

  void addKeyField(QGridLayout* g) {
    auto* box = new QHBoxLayout; box->setSpacing(8);
    auto* edit = new QLineEdit(this);
    edit->setEchoMode(QLineEdit::Password);
    edit->setStyleSheet(lineEditQss());
    edit->setMinimumWidth(280);
    auto* save = button(QCoreApplication::translate("sources", "SAVE KEY"), true, this);
    auto* clear = button(QCoreApplication::translate("sources", "REMOVE"), false, this);
    auto* state = ui::label(QString(), theme::mono(10), theme::pal().textDim, this);
    auto update = [this, edit, clear, state] {
      const bool has = !settings::apiKey(src.keyName).isEmpty();
      edit->setPlaceholderText(has ? QCoreApplication::translate("sources", "Key saved (hidden) — type a new one to replace it")
                                   : QCoreApplication::translate("sources", "Paste your API key"));
      clear->setVisible(has);
      state->setText(has ? QCoreApplication::translate("sources", "● KEY SAVED") : QCoreApplication::translate("sources", "○ NO KEY"));
    };
    connect(save, &QPushButton::clicked, this, [this, edit, update] {
      const QString k = edit->text().trimmed();
      if (k.isEmpty()) return;
      settings::setValue("keys." + src.keyName, k);
      edit->clear(); update(); refresh();
    });
    connect(edit, &QLineEdit::returnPressed, save, &QPushButton::click);
    connect(clear, &QPushButton::clicked, this, [this, update] { settings::remove("keys." + src.keyName); update(); refresh(); });
    box->addWidget(edit, 1); box->addWidget(save); box->addWidget(clear); box->addWidget(state);
    auto* note = ui::label(QCoreApplication::translate("sources", "Stored on this computer only (settings key keys.%1), never shown back. "
                                                       "Used from the next refresh of the pages that read it.").arg(src.keyName),
                           theme::sans(11), theme::pal().textMute, this);
    note->setWordWrap(true);
    g->addLayout(box, 5, 1, 1, 3);
    g->addWidget(note, 6, 1, 1, 3);
    update();
  }

  void addAccountFields(QGridLayout* g) {
    auto& r = positioning::Retail::instance();
    auto* box = new QHBoxLayout; box->setSpacing(8);
    m_email = new QLineEdit(this);
    m_email->setPlaceholderText(QCoreApplication::translate("sources", "Myfxbook email"));
    m_email->setText(r.email());
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(QCoreApplication::translate("sources", "Password"));
    for (auto* e : {m_email, m_password}) { e->setStyleSheet(lineEditQss()); e->setMinimumWidth(200); }
    m_connect = button(QCoreApplication::translate("sources", "CONNECT"), true, this);
    m_logout = button(QCoreApplication::translate("sources", "LOG OUT"), false, this);
    m_retailState = ui::label(QString(), theme::mono(10, QFont::Bold, 1.2), theme::pal().textDim, this);
    connect(m_connect, &QPushButton::clicked, this, [this] {
      const QString pw = m_password->text();
      if (m_email->text().trimmed().isEmpty() || pw.isEmpty()) return;
      positioning::Retail::instance().login(m_email->text().trimmed(), pw);
      m_password->clear();   // the password is sent once and forgotten
    });
    connect(m_password, &QLineEdit::returnPressed, m_connect, &QPushButton::click);
    connect(m_logout, &QPushButton::clicked, this, [] { positioning::Retail::instance().logout(); });
    connect(&r, &positioning::Retail::changed, this, [this] { refresh(); });
    box->addWidget(m_email, 1); box->addWidget(m_password, 1); box->addWidget(m_connect); box->addWidget(m_logout);
    auto* note = ui::label(QCoreApplication::translate("sources", "Your password is sent once to Myfxbook (HTTPS) to open a session and is never stored; "
                                                       "only the session token is kept, until you log out or it expires."),
                           theme::sans(11), theme::pal().textMute, this);
    note->setWordWrap(true);
    g->addLayout(box, 5, 1, 1, 3);
    g->addWidget(m_retailState, 6, 1, 1, 3);
    g->addWidget(note, 7, 1, 1, 3);
    updateAccount();
  }
  void updateAccount() {
    using S = positioning::Retail::State;
    const auto& r = positioning::Retail::instance();
    const auto& c = theme::pal();
    QString t; QColor col = c.textDim;
    switch (r.state()) {
      case S::Connected: t = QCoreApplication::translate("sources", "● CONNECTED"); col = c.up; break;
      case S::LoggingIn: t = QCoreApplication::translate("sources", "… CONNECTING"); break;
      case S::Expired: t = QCoreApplication::translate("sources", "○ SESSION EXPIRED — CONNECT AGAIN"); col = c.warn; break;
      case S::Error: t = QCoreApplication::translate("sources", "✕ %1").arg(r.message()); col = c.down; break;
      case S::NotConnected: t = QCoreApplication::translate("sources", "○ NOT CONNECTED"); break;
    }
    m_retailState->setText(t);
    m_retailState->setStyleSheet(QString("color:%1;background:transparent;").arg(col.name()));
    const bool connected = r.state() == S::Connected;
    m_connect->setVisible(!connected); m_password->setVisible(!connected); m_email->setEnabled(!connected);
    m_logout->setVisible(connected);
  }

  StatusPill* m_pill = nullptr;
  Switch* m_switch = nullptr;
  QLabel* m_health = nullptr;
  QString m_healthQss, m_rowQss;   // the style sheets set last
  QLineEdit *m_email = nullptr, *m_password = nullptr;
  QPushButton *m_connect = nullptr, *m_logout = nullptr;
  QLabel* m_retailState = nullptr;
};

// ---------------------------------------------------------------------------------------------- page
class SourcesPage : public QWidget {
public:
  explicit SourcesPage(QWidget* parent) : QWidget(parent) {
    const auto& c = theme::pal();
    setAutoFillBackground(true);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 12, 16, 12); v->setSpacing(10);

    auto* top = new QHBoxLayout; top->setSpacing(12);
    auto* title = ui::label(QCoreApplication::translate("sources", "DATA SOURCES"), theme::title(), c.accentText, this);
    m_summary = ui::label(QString(), theme::mono(11, QFont::Normal, 1.0), c.textDim, this);
    top->addWidget(title); top->addWidget(m_summary); top->addStretch(1);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QCoreApplication::translate("sources", "Search a source, host or page…"));
    m_search->setStyleSheet(lineEditQss());
    m_search->setFixedWidth(280);
    top->addWidget(m_search);
    v->addLayout(top);

    m_filter = new ui::PillBar({QCoreApplication::translate("sources", "ALL"), QCoreApplication::translate("sources", "ACTIVE"),
                                QCoreApplication::translate("sources", "FAILING"), QCoreApplication::translate("sources", "DISABLED"),
                                QCoreApplication::translate("sources", "NOT CONTACTED"),
                                QCoreApplication::translate("sources", "KEY OR ACCOUNT")}, this);
    v->addWidget(m_filter);

    auto* note = ui::label(QCoreApplication::translate("sources",
        "Status comes from Deepwater's own requests (kept across launches): ACTIVE = the source answered; FAILING = it failed "
        "twice in a row or never answered; DISABLED = switched off here, Deepwater no longer contacts it."),
        theme::sans(11), c.textMute, this);
    note->setWordWrap(true);
    v->addWidget(note);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget(scroll);
    m_list = new QVBoxLayout(inner);
    m_list->setContentsMargins(0, 0, 6, 0); m_list->setSpacing(6);
    scroll->setWidget(inner);
    v->addWidget(scroll, 1);

    QList<sources::Source> all = sources::all();
    std::sort(all.begin(), all.end(), [](const sources::Source& a, const sources::Source& b) {
      return a.publisher.compare(b.publisher, Qt::CaseInsensitive) < 0;
    });
    for (const auto& s : all) { auto* r = new Row(s, inner); m_rows << r; m_list->addWidget(r); }
    m_list->addStretch(1);

    connect(m_filter, &ui::PillBar::changed, this, [this] { applyFilter(); });
    connect(m_search, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    // every answer of every source notifies: a hidden page waits for its showEvent, which refreshes it
    connect(&sources::Notifier::instance(), &sources::Notifier::changed, this, [this] { if (isVisible()) refreshAll(); });
    m_tick.setInterval(30000);   // relative times ("3 min ago")
    connect(&m_tick, &QTimer::timeout, this, [this] { refreshAll(); });
    // test hook: DW_SOURCES_FILTER=<0..5> opens the page on that filter
    if (qEnvironmentVariableIsSet("DW_SOURCES_FILTER")) m_filter->setCurrent(qEnvironmentVariableIntValue("DW_SOURCES_FILTER"));
    refreshAll();
  }

protected:
  void showEvent(QShowEvent*) override { refreshAll(); m_tick.start(); }
  void hideEvent(QHideEvent*) override { m_tick.stop(); }
  void paintEvent(QPaintEvent*) override { QPainter p(this); p.fillRect(rect(), theme::pal().bg); }

private:
  void refreshAll() {
    int n[5] = {0, 0, 0, 0, 0};
    int keyed = 0;
    for (auto* r : std::as_const(m_rows)) {
      r->refresh();
      n[int(effectiveStatus(r->src))]++;
      if (r->src.access == sources::Access::Key || r->src.access == sources::Access::Account || !r->src.keyName.isEmpty()) ++keyed;
    }
    const int total = int(m_rows.size());
    // enum order: Waiting, Active, Failing, Disabled
    m_filter->setCounts({total, n[1], n[2], n[3], n[0], keyed});
    m_summary->setText(QCoreApplication::translate("sources", "%1 sources · %2 active · %3 failing · %4 disabled")
                           .arg(total).arg(n[1]).arg(n[2]).arg(n[3]));
    applyFilter();
  }
  void applyFilter() {
    const int f = m_filter->current();
    const QString q = m_search->text().trimmed();
    for (auto* r : std::as_const(m_rows)) {
      const auto st = effectiveStatus(r->src);
      bool ok = f == 0 || (f == 1 && st == sources::Status::Active) || (f == 2 && st == sources::Status::Failing) ||
                (f == 3 && st == sources::Status::Disabled) || (f == 4 && st == sources::Status::Waiting) ||
                (f == 5 && (r->src.access == sources::Access::Key || r->src.access == sources::Access::Account || !r->src.keyName.isEmpty()));
      if (ok && !q.isEmpty())
        ok = r->src.publisher.contains(q, Qt::CaseInsensitive) || r->src.id.contains(q, Qt::CaseInsensitive) ||
             r->src.hosts.join(' ').contains(q, Qt::CaseInsensitive) || r->src.data.contains(q, Qt::CaseInsensitive) ||
             pageNames(r->src.pages).contains(q, Qt::CaseInsensitive);
      r->setVisible(ok);
    }
  }

  QLabel* m_summary;
  QLineEdit* m_search;
  ui::PillBar* m_filter;
  QVBoxLayout* m_list;
  QList<Row*> m_rows;
  QTimer m_tick;
};
}  // namespace
}  // namespace sourcespage

using SourcesPageImpl = sourcespage::SourcesPage;
DW_PAGE("sources/sources", SourcesPageImpl)
