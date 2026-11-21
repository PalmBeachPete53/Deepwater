#include "pages/account/TrDialogs.h"
#include <QCoreApplication>
#include "pages/account/AccountKit.h"
#include "ui/Theme.h"
#include "ui/Kit.h"
#include "account/Contracts.h"
#include "account/Importers.h"
#include "account/BridgeSync.h"
#include "market/Instruments.h"
#include "market/QuoteHub.h"
#include "core/Settings.h"
#include "ui/Format.h"
#include "ui/Touch.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDateTimeEdit>
#include <QCompleter>
#include <QFileDialog>
#include <QDoubleValidator>
#include <QMessageBox>
#include <QDir>
#include <QSet>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QStyle>
#include <QScrollArea>
#include <QScreen>
#include <QApplication>

namespace acct {

QString dialogStyle() {
  return qss(QString(R"(
QDialog { background:#0c0601; }
QLabel { color:#a29c8c; background:transparent; }
QLabel[role="field"] { color:#5d584c; font-family:'JetBrains Mono'; font-size:9px; letter-spacing:3px; }
QLabel[role="title"] { color:#ece8de; font-family:'JetBrains Mono'; font-size:15px; font-weight:bold; letter-spacing:3px; }
QLabel[role="section"] { color:#e6c200; font-family:'JetBrains Mono'; font-size:10px; font-weight:bold; letter-spacing:3px; }
QLabel[role="note"] { color:#8a8478; font-size:12px; }
QLabel[role="error"] { color:#e0605a; font-size:12px; }
QLabel[role="ok"] { color:#3fc6d8; font-size:12px; }
QLineEdit, QDateTimeEdit, QComboBox { background:#050401; border:1px solid #2a2618; border-radius:3px; color:#fdfcf9;
  font-family:'JetBrains Mono'; font-size:13px; padding:6px 8px; min-height:18px; selection-background-color:#433603; }
QLineEdit:focus, QDateTimeEdit:focus, QComboBox:focus { border-color:#7a6a20; }
QLineEdit:read-only { color:#a29c8c; }
QComboBox::drop-down, QDateTimeEdit::drop-down { border:none; width:18px; }
QComboBox QAbstractItemView { background:#0f0a03; border:1px solid #3a351b; color:#ece8de; selection-background-color:#433603; selection-color:#f0d040; outline:0; }
QDateTimeEdit::up-button, QDateTimeEdit::down-button { width:0; border:none; }
QPushButton { background:transparent; border:1px solid #3a3424; border-radius:3px; color:#a09988; font-family:'JetBrains Mono';
  font-size:11px; font-weight:bold; letter-spacing:2px; padding:7px 14px; }
QPushButton:hover { color:#ece8de; border-color:#6a5f3c; }
QPushButton:disabled { color:#4a463c; border-color:#24200f; }
QPushButton[role="primary"] { background:#fdd51f; color:#0a0800; border:none; }
QPushButton[role="primary"]:hover { background:#ffe04a; }
QPushButton[role="primary"]:disabled { background:#3a3218; color:#6b6248; }
QPushButton:checked { background:#2a2605; color:#fde695; border-color:#e6c200; }
QPushButton[role="long"]:checked { background:#052423; color:#25a2ae; border-color:#1d7f88; }
QPushButton[role="short"]:checked { background:#2f0a0a; color:#e0605a; border-color:#862d26; }
)").replace("#0a0800", "@onAccent@")).replace("@onAccent@", theme::onAccent().name());   // text on the filled accent button
}

namespace {
// Phones (the app window under ui::kCompactWidth): one column of fields, the dialog as wide as the window,
// 36 px fields and 38 px buttons (finger targets), 16 px margins.
bool phoneFor(QWidget* parent) {
  if (!parent) return false;
  QWidget* w = parent->window();
  // a dialog opened from a dialog (Import › new account): the app window decides, not the 640 px import dialog
  while (qobject_cast<QDialog*>(w) && w->parentWidget()) w = w->parentWidget()->window();
  return ui::compact(w->width());
}
QString phoneStyle() {
  return "QLineEdit, QDateTimeEdit, QComboBox { min-height:22px; } QPushButton { min-height:22px; }";
}
void setupPhone(QDialog& d) {
  d.setStyleSheet(dialogStyle() + phoneStyle());
  d.setMinimumWidth(0);
}

// Dialog skeleton that fits small windows (1024x600): the form scrolls, the button row stays visible.
struct Frame { QVBoxLayout* body; QVBoxLayout* footer; QScrollArea* scroll; bool phone = false; };
Frame frame(QDialog& d, bool phone = false) {
  auto* outer = new QVBoxLayout(&d); outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0);
  auto* sa = new QScrollArea(&d);
  sa->setWidgetResizable(true); sa->setFrameShape(QFrame::NoFrame);
  sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* inner = new QWidget; inner->setObjectName("dlgBody");
  sa->setWidget(inner);
  sa->setStyleSheet(qss("QScrollArea, QScrollArea > QWidget > QWidget#dlgBody { background:#0c0601; }"));
  outer->addWidget(sa, 1);
  auto* body = new QVBoxLayout(inner);
  auto* foot = new QVBoxLayout; foot->setContentsMargins(phone ? 16 : 24, 8, phone ? 16 : 24, 16);
  outer->addLayout(foot);
  return {body, foot, sa, phone};
}
// Sizes the dialog to its content, capped by the main window and the screen (scrolls inside beyond that).
void fit(QDialog& d, const Frame& f) {
  QWidget* inner = f.scroll->widget();
  inner->adjustSize();
  const QSize want(inner->sizeHint().width() + 14, inner->sizeHint().height() + f.footer->sizeHint().height() + 4);
  QSize cap(4000, 4000);
  if (QWidget* w = d.parentWidget() ? d.parentWidget()->window() : nullptr) cap = w->size() - QSize(24, 24);
  if (QScreen* sc = d.screen()) cap = cap.boundedTo(sc->availableGeometry().size() - QSize(40, 60));
  if (f.phone) { d.resize(cap.width(), std::min(want.height(), cap.height())); return; }   // phones: the window's width
  d.resize(want.boundedTo(cap).expandedTo(QSize(std::min(cap.width(), d.minimumWidth()), 200)));
  if (d.minimumWidth() > cap.width()) d.setMinimumWidth(cap.width());
}
QLabel* field(const QString& t, QWidget* parent) {
  auto* l = new QLabel(parent); l->setTextFormat(Qt::PlainText); l->setText(t.toUpper()); l->setProperty("role", "field"); return l;
}
QLabel* note(const QString& t, QWidget* parent, const char* role = "note") {
  // plain text: notes show what a statement file says (account, currency, warnings), which QLabel would render as HTML
  auto* l = new QLabel(parent); l->setTextFormat(Qt::PlainText); l->setText(t); l->setProperty("role", role); l->setWordWrap(true); return l;
}
QPushButton* button(const QString& t, QWidget* parent, const char* role = nullptr) {
  auto* b = new QPushButton(t, parent); if (role) b->setProperty("role", role); b->setCursor(Qt::PointingHandCursor); return b;
}
// typed numbers: a single comma is the decimal mark ("1,0852", "157,250"), groups only when clear ("1,234,567",
// "1,234.5"): rule in account/Importers.h (A4)
double num(QLineEdit* e) { return e->text().trimmed().isEmpty() ? NAN : account::parseNumber(e->text(), true); }
QString numText(double v) { return std::isfinite(v) ? QString::number(v, 'g', 10) : QString(); }

// Trade P&L estimated from prices, size and the contract, converted at today's live rate.
double estimatePnl(const QString& sym, bool lng, double entry, double exit, double lots, const QString& ccy) {
  if (!std::isfinite(entry) || !std::isfinite(exit) || !std::isfinite(lots)) return NAN;
  const auto s = account::spec(sym);
  const double move = (lng ? exit - entry : entry - exit) * lots * s.contract * s.priceUnit;
  const double rate = account::fxRate(s.quoteCcy, ccy);
  return std::isfinite(rate) ? move * rate : NAN;
}
}

// ---------------------------------------------------------------- trade form
bool tradeDialog(QWidget* parent, account::Trade& t, const QString& ccy, bool isNew) {
  QDialog d(parent);
  d.setWindowTitle(isNew ? QCoreApplication::translate("account", "Log trade") : QCoreApplication::translate("account", "Edit trade"));
  d.setStyleSheet(dialogStyle());
  d.setMinimumWidth(560);
  const bool phone = phoneFor(parent);
  if (phone) setupPhone(d);
  const Frame fr = frame(d, phone);
  auto* v = fr.body; v->setContentsMargins(phone ? 16 : 24, 20, phone ? 16 : 24, 12); v->setSpacing(12);
  auto* title = new QLabel(isNew ? QCoreApplication::translate("account", "LOG A CLOSED TRADE") : QCoreApplication::translate("account", "EDIT TRADE"), &d); title->setProperty("role", "title");
  v->addWidget(title);
  auto* g = new QGridLayout; g->setHorizontalSpacing(14); g->setVerticalSpacing(phone ? 6 : 4);
  v->addLayout(g);

  QStringList names;
  for (const auto& i : market::instruments()) if (i.category != market::Category::Bond) names << account::displaySymbol(i.symbol);
  auto* sym = new QComboBox(&d); sym->setEditable(true); sym->addItems(names);
  sym->completer()->setCompletionMode(QCompleter::PopupCompletion);
  sym->completer()->setFilterMode(Qt::MatchContains);
  sym->setCurrentText(t.symbol.isEmpty() ? "EUR/USD" : account::displaySymbol(t.symbol));
  auto* lng = button("↗ LONG", &d, "long"); auto* sht = button("↘ SHORT", &d, "short");
  lng->setCheckable(true); sht->setCheckable(true); lng->setAutoExclusive(true); sht->setAutoExclusive(true);
  (t.side == account::Side::Long ? lng : sht)->setChecked(true);
  auto* sides = new QHBoxLayout; sides->setSpacing(6); sides->addWidget(lng); sides->addWidget(sht);

  const QDateTime now = QDateTime::currentDateTime();
  auto* open = new QDateTimeEdit(t.open.isValid() ? t.open.toLocalTime() : now.addSecs(-3600), &d);
  auto* close = new QDateTimeEdit(t.close.isValid() ? t.close.toLocalTime() : now, &d);
  for (auto* e : {open, close}) { e->setDisplayFormat("dd MMM yyyy  HH:mm"); e->setCalendarPopup(true); }

  auto mk = [&](double val) { auto* e = new QLineEdit(numText(val), &d); return e; };
  auto* entry = mk(t.entry); auto* exit = mk(t.exit); auto* lots = mk(t.lots); auto* sl = mk(t.sl); auto* tp = mk(t.tp);
  auto* pnl = mk(isNew ? NAN : t.pnl);
  auto* comm = mk(isNew ? NAN : t.commission); auto* swap = mk(isNew ? NAN : t.swap);
  comm->setPlaceholderText("0"); swap->setPlaceholderText("0");
  auto* strat = new QComboBox(&d); strat->setEditable(true);
  QSet<QString> known{"Breakout", "Mean reversion", "News", "Reversal", "Trend following"};
  for (const auto& a : account::Journal::instance().accounts()) for (const auto& x : a.trades) if (!x.strategy.isEmpty()) known.insert(x.strategy);
  QStringList sl2 = known.values(); std::sort(sl2.begin(), sl2.end());
  strat->addItem(""); strat->addItems(sl2); strat->setCurrentText(t.strategy);
  auto* comment = new QLineEdit(t.comment, &d);

  // two fields per row (label above its input); phones: one column
  int r = 0;
  auto put = [&](QObject* o, int row, int col) {
    if (auto* w = qobject_cast<QWidget*>(o)) g->addWidget(w, row, col); else g->addLayout(qobject_cast<QLayout*>(o), row, col);
  };
  auto pair = [&](const QString& l1, QObject* w1, const QString& l2, QObject* w2) {
    if (phone) { put(field(l1, &d), r++, 0); put(w1, r++, 0); put(field(l2, &d), r++, 0); put(w2, r++, 0); return; }
    put(field(l1, &d), r, 0); put(field(l2, &d), r++, 1);
    put(w1, r, 0); put(w2, r++, 1);
  };
  pair(QCoreApplication::translate("account", "Instrument"), sym, QCoreApplication::translate("account", "Side"), sides);
  pair(QCoreApplication::translate("account", "Opened (local time)"), open, QCoreApplication::translate("account", "Closed (local time)"), close);
  pair(QCoreApplication::translate("account", "Entry price"), entry, QCoreApplication::translate("account", "Exit price"), exit);
  pair(QCoreApplication::translate("account", "Size (lots)"), lots, QCoreApplication::translate("account", "P&L (%1)").arg(ccy), pnl);
  pair(QCoreApplication::translate("account", "Stop loss (optional)"), sl, QCoreApplication::translate("account", "Take profit (optional)"), tp);
  pair(QCoreApplication::translate("account", "Commission"), comm, QCoreApplication::translate("account", "Swap"), swap);
  pair(QCoreApplication::translate("account", "Strategy"), strat, QCoreApplication::translate("account", "Comment"), comment);

  auto* hint = note("", &d);
  auto* err = note("", &d, "error");
  v->addWidget(hint); v->addWidget(err);
  auto symbolOf = [&]() -> QString {
    const QString txt = sym->currentText().trimmed();
    for (const auto& i : market::instruments()) if (account::displaySymbol(i.symbol).compare(txt, Qt::CaseInsensitive) == 0) return i.symbol;
    return txt;
  };
  // subscribe to the conversion rate so the estimate is in the account currency
  auto refresh = [&] {
    const QString s = symbolOf();
    const auto sp = account::spec(s);
    QStringList need = account::fxSymbolsFor(sp.quoteCcy, ccy);
    if (!need.isEmpty()) market::QuoteHub::instance().subscribe(&d, need);
    const double est = estimatePnl(s, lng->isChecked(), num(entry), num(exit), num(lots), ccy);
    pnl->setPlaceholderText(std::isfinite(est) ? fmt::num(est, 2) : QCoreApplication::translate("account", "required"));
    hint->setText(QCoreApplication::translate("account", "1 lot of %1 = %2 %3. Leave P&L empty to use the estimate from prices and size%4.")
                  .arg(sp.display, fmt::num(sp.contract, 0), unitLabel(sp.unit),
                       sp.quoteCcy == ccy ? QString() : QCoreApplication::translate("account", " (converted from %1 at today's rate, not the rate of the trade day)").arg(sp.quoteCcy)));
  };
  for (auto* e : {entry, exit, lots}) QObject::connect(e, &QLineEdit::textChanged, &d, refresh);
  QObject::connect(sym, &QComboBox::currentTextChanged, &d, refresh);
  QObject::connect(lng, &QPushButton::toggled, &d, refresh);
  QObject::connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, &d, refresh);
  refresh();

  auto* row = new QHBoxLayout; row->addStretch(1);
  auto* cancel = button(QCoreApplication::translate("account", "CANCEL"), &d); auto* save = button(isNew ? QCoreApplication::translate("account", "SAVE TRADE") : QCoreApplication::translate("account", "SAVE CHANGES"), &d, "primary");
  row->addWidget(cancel); row->addWidget(save);
  fr.footer->addLayout(row);
  QObject::connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
  QObject::connect(save, &QPushButton::clicked, &d, [&] {
    QStringList bad;
    const QString s = symbolOf();
    if (s.isEmpty()) bad << QCoreApplication::translate("account", "instrument");
    const double en = num(entry), ex = num(exit), lo = num(lots);
    if (!std::isfinite(en) || en <= 0) bad << QCoreApplication::translate("account", "entry price");
    if (!std::isfinite(ex) || ex <= 0) bad << QCoreApplication::translate("account", "exit price");
    if (!std::isfinite(lo) || lo <= 0) bad << QCoreApplication::translate("account", "size");
    if (close->dateTime() < open->dateTime()) bad << QCoreApplication::translate("account", "close time before open time");
    double p = num(pnl);
    if (!std::isfinite(p)) p = estimatePnl(s, lng->isChecked(), en, ex, lo, ccy);
    if (!std::isfinite(p)) bad << QCoreApplication::translate("account", "P&L (no estimate available: enter it)");
    const double stop = num(sl), target = num(tp);
    if (std::isfinite(stop) && std::isfinite(en) && stop == en) bad << QCoreApplication::translate("account", "stop equal to entry");
    if (!bad.isEmpty()) { err->setText(QCoreApplication::translate("account", "Check: %1.").arg(bad.join(", "))); return; }
    t.symbol = s; t.side = lng->isChecked() ? account::Side::Long : account::Side::Short;
    t.open = open->dateTime().toUTC(); t.close = close->dateTime().toUTC();
    t.entry = en; t.exit = ex; t.lots = lo; t.sl = stop; t.tp = target; t.pnl = std::round(p * 100) / 100;
    const double cm = num(comm), sw = num(swap);
    t.commission = std::isfinite(cm) ? cm : 0; t.swap = std::isfinite(sw) ? sw : 0;
    t.strategy = strat->currentText().trimmed(); t.comment = comment->text().trimmed();
    d.accept();
  });
  fit(d, fr);
  return d.exec() == QDialog::Accepted;
}

// ---------------------------------------------------------------- account form
bool accountDialog(QWidget* parent, account::Account& a, bool isNew) {
  QDialog d(parent);
  d.setWindowTitle(isNew ? QCoreApplication::translate("account", "Add account") : QCoreApplication::translate("account", "Edit account"));
  d.setStyleSheet(dialogStyle());
  d.setMinimumWidth(440);
  const bool phone = phoneFor(parent);
  if (phone) setupPhone(d);
  const Frame fr = frame(d, phone);
  auto* v = fr.body; v->setContentsMargins(phone ? 16 : 24, 20, phone ? 16 : 24, 12); v->setSpacing(6);
  auto* title = new QLabel(isNew ? QCoreApplication::translate("account", "ADD ACCOUNT") : QCoreApplication::translate("account", "EDIT ACCOUNT"), &d); title->setProperty("role", "title");
  v->addWidget(title); v->addSpacing(8);
  auto* name = new QLineEdit(a.name, &d);
  auto* ccy = new QComboBox(&d);
  ccy->addItems({"USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD", "SGD", "HKD", "SEK", "NOK", "PLN", "ZAR"});
  ccy->setEditable(true); ccy->setCurrentText(a.currency);
  auto* bal = new QLineEdit(a.balance > 0 ? QString::number(a.balance, 'f', 2) : QString(), &d);
  bal->setPlaceholderText("e.g. 10000");
  auto* plat = new QComboBox(&d);                  // stored values stay English, the labels are translated
  plat->addItem(QCoreApplication::translate("account", "Manual"), "Manual"); plat->addItem("MT4", "MT4"); plat->addItem("MT5", "MT5");
  plat->addItem("cTrader", "cTrader"); plat->addItem(QCoreApplication::translate("account", "Other"), "Other");
  { const int pi = plat->findData(a.platform.isEmpty() ? "Manual" : a.platform); if (pi >= 0) plat->setCurrentIndex(pi); }
  v->addWidget(field(QCoreApplication::translate("account", "Name"), &d)); v->addWidget(name);
  v->addWidget(field(QCoreApplication::translate("account", "Account currency"), &d)); v->addWidget(ccy);
  v->addWidget(field(QCoreApplication::translate("account", "Starting balance"), &d)); v->addWidget(bal);
  v->addWidget(note(QCoreApplication::translate("account", "Deposits minus withdrawals. Percentages (drawdown, % of balance) are computed from it."), &d));
  v->addWidget(field(QCoreApplication::translate("account", "Platform"), &d)); v->addWidget(plat);
  if (a.demo) v->addWidget(note(QCoreApplication::translate("account", "This is the demo account: its trades are invented sample data."), &d, "error"));
  auto* err = note("", &d, "error"); v->addWidget(err);
  auto* row = new QHBoxLayout; row->addStretch(1);
  auto* cancel = button(QCoreApplication::translate("account", "CANCEL"), &d); auto* ok = button(isNew ? QCoreApplication::translate("account", "ADD ACCOUNT") : QCoreApplication::translate("account", "SAVE"), &d, "primary");
  row->addWidget(cancel); row->addWidget(ok); fr.footer->addLayout(row);
  QObject::connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
  QObject::connect(ok, &QPushButton::clicked, &d, [&] {
    const double b = account::parseNumber(bal->text(), true);
    const QString c3 = ccy->currentText().trimmed().toUpper();
    if (name->text().trimmed().isEmpty()) { err->setText(QCoreApplication::translate("account", "Give the account a name.")); return; }
    if (c3.size() != 3) { err->setText(QCoreApplication::translate("account", "Currency: a 3-letter code (USD, EUR…).")); return; }
    if (!bal->text().trimmed().isEmpty() && (!std::isfinite(b) || b < 0)) { err->setText(QCoreApplication::translate("account", "Starting balance: a positive number.")); return; }
    const double nb = std::isfinite(b) ? b : 0;
    if (nb != a.balance) a.balanceByUser = true;       // the live sync keeps the user's balance from now on (A11)
    a.name = name->text().trimmed(); a.currency = c3; a.balance = nb; a.platform = plat->currentData().toString();
    d.accept();
  });
  fit(d, fr);
  return d.exec() == QDialog::Accepted;
}

// ---------------------------------------------------------------- import
void importDialog(QWidget* parent) {
  QDialog d(parent);
  d.setWindowTitle(QCoreApplication::translate("account", "Import trades"));
  d.setStyleSheet(dialogStyle());
  d.setMinimumWidth(640);
  const bool phone = phoneFor(parent);
  if (phone) setupPhone(d);
  auto& j = account::Journal::instance();
  const Frame fr = frame(d, phone);
  auto* v = fr.body; v->setContentsMargins(phone ? 16 : 24, 20, phone ? 16 : 24, 12); v->setSpacing(8);
  auto* title = new QLabel(QCoreApplication::translate("account", "IMPORT TRADES"), &d); title->setProperty("role", "title");
  v->addWidget(title);
  v->addWidget(note(QCoreApplication::translate("account", "No broker password is needed: export the history from your platform, or let the read-only "
                    "bridge write it for you. Everything stays on this computer."), &d));

  // ---- statement file
  auto* s1 = new QLabel(QCoreApplication::translate("account", "FROM A STATEMENT FILE"), &d); s1->setProperty("role", "section");
  v->addSpacing(6); v->addWidget(s1);
  v->addWidget(note(QCoreApplication::translate("account", "MT4: Account History › right click › All History, then Save as Detailed Report (.htm).\n"
                    "MT5: History › right click › Report › HTML.   cTrader: History › export CSV.   "
                    "Any CSV with a header row (symbol, side, open/close time, prices, size, profit) works too."), &d));
  auto* offRow = new QHBoxLayout;
  auto* off = new QComboBox(&d);
  for (int h = -12; h <= 14; ++h) off->addItem(h == 0 ? "UTC" : QString("UTC%1%2").arg(h > 0 ? "+" : "−").arg(std::abs(h)), h * 60);
  off->setCurrentIndex(off->findData(settings::value("account.import.serverOffsetMin", 0).toInt()));
  offRow->addWidget(field(QCoreApplication::translate("account", "Statement times are in"), &d)); offRow->addWidget(off); offRow->addStretch(1);
  v->addLayout(offRow);
  v->addWidget(note(QCoreApplication::translate("account", "MetaTrader statements use the broker's server time (often UTC+2 in winter, UTC+3 in summer). "
                    "cTrader exports say their zone in the column titles."), &d));
  auto* pick = button(QCoreApplication::translate("account", "CHOOSE FILE…"), &d);
  auto* fileLbl = note(QCoreApplication::translate("account", "No file chosen."), &d);
  auto* pickRow = new QHBoxLayout; pickRow->addWidget(pick); pickRow->addWidget(fileLbl, 1);
  v->addLayout(pickRow);
  auto* preview = note("", &d, "ok");
  v->addWidget(preview);
  auto* target = new QComboBox(&d);
  if (phone) {   // phones: long account names elide instead of widening the dialog past the screen
    target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    target->setMinimumContentsLength(8);
  }
  auto* doImport = button(QCoreApplication::translate("account", "IMPORT"), &d, "primary"); doImport->setEnabled(false);
  auto* tRow = new QHBoxLayout; tRow->addWidget(field(QCoreApplication::translate("account", "Into"), &d)); tRow->addWidget(target, 1); tRow->addWidget(doImport);
  v->addLayout(tRow);
  auto fillTargets = [&] {
    target->clear();
    // with its currency: a statement in EUR imported into a USD account would sum EUR profits as dollars
    for (const auto& a : j.accounts()) if (!a.demo) target->addItem(a.name + "  ·  " + a.currency, a.id);
    target->addItem(QCoreApplication::translate("account", "New account (from the file)"), QString());
    const int cur = target->findData(j.currentId());
    target->setCurrentIndex(cur >= 0 ? cur : target->count() - 1);
  };
  fillTargets();

  account::ImportResult res;
  QString path;
  auto showPreview = [&] {
    if (!res.ok()) { preview->setProperty("role", "error"); preview->setText(res.error.isEmpty() ? QCoreApplication::translate("account", "No trades found.") : res.error); }
    else {
      preview->setProperty("role", "ok");
      QString t = (res.trades.size() == 1 ? QCoreApplication::translate("account", "%1 · %2 closed trade · %3 → %4") : QCoreApplication::translate("account", "%1 · %2 closed trades · %3 → %4")).arg(QCoreApplication::translate("account", res.format.toUtf8().constData())).arg(res.trades.size())
                    .arg(longDate(res.trades.first().close), longDate(res.trades.last().close));
      if (std::isfinite(res.deposits)) t += QCoreApplication::translate("account", " · deposits %1 %2").arg(fmt::num(res.deposits, 2), res.currency);
      if (!res.login.isEmpty()) t += QCoreApplication::translate("account", " · account %1").arg(res.login);
      if (res.skipped) t += (res.skipped == 1 ? QCoreApplication::translate("account", " · %1 row not read") : QCoreApplication::translate("account", " · %1 rows not read")).arg(res.skipped);
      if (!res.warnings.isEmpty()) t += "\n" + res.warnings.join(" ");
      preview->setText(t);
    }
    preview->style()->unpolish(preview); preview->style()->polish(preview);
    doImport->setEnabled(res.ok());
  };
  auto parse = [&] {
    if (path.isEmpty()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);   // big statements take a moment
    res = account::importFile(path, off->currentData().toInt());
    QApplication::restoreOverrideCursor();
    showPreview();
  };
  QObject::connect(pick, &QPushButton::clicked, &d, [&] {
    const QString start = settings::value("account.import.lastDir", QDir::homePath()).toString();
    const QString f = QFileDialog::getOpenFileName(&d, QCoreApplication::translate("account", "Statement or history file"), start,
                                                   QCoreApplication::translate("account", "Statements (*.htm *.html *.csv *.txt);;All files (*)"));
    if (f.isEmpty()) return;
    settings::setValue("account.import.lastDir", QFileInfo(f).absolutePath());
    path = f; fileLbl->setText(QFileInfo(f).fileName());
    parse();
  });
  QObject::connect(off, &QComboBox::currentIndexChanged, &d, [&] {
    const int o = off->currentData().toInt();
    settings::setValue("account.import.serverOffsetMin", o);
    if (path.isEmpty() || res.trades.isEmpty()) { parse(); return; }
    // the file is not read again: its times only move by the change of offset (unless the file says its zone)
    if (res.offsetFromUser && o != res.offsetMin) {
      const qint64 shift = qint64(res.offsetMin - o) * 60;
      for (auto& t : res.trades) { t.open = t.open.addSecs(shift); t.close = t.close.addSecs(shift); }
      res.offsetMin = o;
    }
    showPreview();
  });
  QObject::connect(doImport, &QPushButton::clicked, &d, [&] {
    if (!res.ok()) return;
    QString id = target->currentData().toString();
    if (id.isEmpty()) {
      account::Account a;
      a.platform = res.platform.isEmpty() ? "Other" : res.platform;
      a.name = res.login.isEmpty() ? QCoreApplication::translate("account", "%1 account").arg(a.platform) : QString("%1 %2").arg(a.platform, res.login);
      if (!res.broker.isEmpty()) a.name += " · " + res.broker;
      a.currency = res.currency.isEmpty() ? "USD" : res.currency;
      a.balance = std::isfinite(res.deposits) ? res.deposits : 0;
      a.login = res.login; a.broker = res.broker;
      if (!accountDialog(&d, a, true)) return;
      id = j.addAccount(a);
    } else if (const account::Account* a = j.account(id);
               a && !res.currency.isEmpty() && !a->currency.isEmpty() && res.currency.compare(a->currency, Qt::CaseInsensitive) != 0) {
      // the profits are stored as they are, in the account's currency: a mismatch is the user's call, never silent
      if (ui::askPlain(&d, QCoreApplication::translate("account", "Another currency"),
                       QCoreApplication::translate("account", "This statement is in %1 but the account \"%2\" is in %3. Its profits would be "
                                                              "added as %3 without conversion.\n\nImport into it anyway?")
                           .arg(res.currency, a->name, a->currency)) != QMessageBox::Yes)
        return;
    }
    // the same statement imported again with a corrected offset corrects the times of the trades already there
    int fixedTimes = 0;
    const int added = j.addTrades(id, res.trades, true, 0, &fixedTimes);
    j.setCurrent(id);
    QString done = (added == 1 ? QCoreApplication::translate("account", "Imported %1 new trade (%2 already in the journal).") : QCoreApplication::translate("account", "Imported %1 new trades (%2 already in the journal)."))
                     .arg(added).arg(res.trades.size() - added);
    if (fixedTimes) done += " " + (fixedTimes == 1 ? QCoreApplication::translate("account", "Times of %1 trade corrected to this offset.")
                                                   : QCoreApplication::translate("account", "Times of %1 trades corrected to this offset.")).arg(fixedTimes);
    preview->setText(done);
    doImport->setEnabled(false);
    fillTargets();
  });

  // ---- live bridge
  auto* s2 = new QLabel(QCoreApplication::translate("account", "LIVE SYNC · READ-ONLY BRIDGE"), &d); s2->setProperty("role", "section");
  v->addSpacing(10); v->addWidget(s2);
  v->addWidget(note(QCoreApplication::translate("account", "A small Expert Advisor (MT4 / MT5) or cBot (cTrader) writes your closed trades to a file; "
                    "Deepwater watches the folder and adds new trades as they close. The bridge never sends orders. "
                    "Save the bridge files, install them in your platform (instructions in README.txt), then choose "
                    "the folder the files are written to: MetaTrader's Common/Files, or Documents/Deepwater for cTrader."), &d));
  auto& b = account::BridgeSync::instance();
  auto* folder = new QLineEdit(QDir::toNativeSeparators(b.folder()), &d); folder->setReadOnly(true);
  folder->setPlaceholderText(QCoreApplication::translate("account", "No folder: live sync off"));
  auto* choose = button(QCoreApplication::translate("account", "CHOOSE FOLDER…"), &d);
  auto* stop = button(QCoreApplication::translate("account", "STOP", "stop the live sync"), &d); stop->setEnabled(!b.folder().isEmpty());
  auto* fRow = new QHBoxLayout;
  if (phone) {   // phones: the folder on its own row, its buttons under it
    v->addWidget(folder);
    fRow->addWidget(choose, 1); fRow->addWidget(stop);
  } else { fRow->addWidget(folder, 1); fRow->addWidget(choose); fRow->addWidget(stop); }
  v->addLayout(fRow);
  const QStringList cands = account::BridgeSync::candidateFolders();
  if (!cands.isEmpty()) v->addWidget(note(QCoreApplication::translate("account", "Found on this computer: %1").arg(QDir::toNativeSeparators(cands.join("   ·   "))), &d));
  auto* status = note(b.status(), &d, "ok");
  v->addWidget(status);
  auto* save = button(QCoreApplication::translate("account", "SAVE BRIDGE FILES…"), &d);
  auto* sRow = new QHBoxLayout; sRow->addWidget(save); sRow->addStretch(1);
  v->addLayout(sRow);
  QObject::connect(&b, &account::BridgeSync::statusChanged, &d, [&] { status->setText(b.status()); });
  QObject::connect(choose, &QPushButton::clicked, &d, [&] {
    const QString start = b.folder().isEmpty() ? (cands.isEmpty() ? QDir::homePath() : cands.first()) : b.folder();
    const QString dir = QFileDialog::getExistingDirectory(&d, QCoreApplication::translate("account", "Folder with the Deepwater_*.csv files"), start);
    if (dir.isEmpty()) return;
    j.resumeBridgeFiles();                             // files of removed live accounts are synced again
    b.setFolder(dir); folder->setText(QDir::toNativeSeparators(dir)); stop->setEnabled(true);
  });
  QObject::connect(stop, &QPushButton::clicked, &d, [&] { b.setFolder(QString()); folder->clear(); stop->setEnabled(false); });
  QObject::connect(save, &QPushButton::clicked, &d, [&] {
    const QString dir = QFileDialog::getExistingDirectory(&d, QCoreApplication::translate("account", "Save the bridge sources into"), QDir::homePath());
    if (dir.isEmpty()) return;
    const QStringList w = account::BridgeSync::exportBridgeSources(dir + "/Deepwater bridge");
    status->setText(w.isEmpty() ? QCoreApplication::translate("account", "Could not write the files.") : QCoreApplication::translate("account", "Saved %1 files in %2").arg(w.size()).arg(QDir::toNativeSeparators(dir + "/Deepwater bridge")));
    if (!w.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(dir + "/Deepwater bridge"));
  });

  auto* row = new QHBoxLayout; row->addStretch(1);
  auto* close = button(QCoreApplication::translate("account", "CLOSE"), &d);
  row->addWidget(close); fr.footer->addLayout(row);
  QObject::connect(close, &QPushButton::clicked, &d, &QDialog::accept);
  fit(d, fr);
  d.exec();
}
}
