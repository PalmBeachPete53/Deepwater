// Charts tab — quant-terminal's app.css mapped onto Deepwater's theme (see Style.h).
#include "pages/charts/Style.h"
#include "pages/charts/Icons.h"
#include "ui/Theme.h"
#include "core/Paths.h"
#include <QAbstractButton>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QStaticText>
#include <QStyle>
#include <QFile>
#include <QDir>
#include <QHash>
#include <QPointer>
#include <cmath>

namespace charts::ui::style {

// ------------------------------------------------------------------------------------------------ palette mapping
namespace {
bool g_light = false;
int g_lightRev = 0;
int g_builtFor = -1;
Vars g_vars;

double lum(const QColor& c) {   // WCAG relative luminance
  auto ch = [](double v) { v /= 255.0; return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * ch(c.red()) + 0.7152 * ch(c.green()) + 0.0722 * ch(c.blue());
}
double contrast(const QColor& a, const QColor& b) {
  const double la = lum(a), lb = lum(b);
  return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
// Darken (on a light ground) or lighten (on a dark one) until the colour reads on the ground (>= min contrast).
QColor tune(QColor c, const QColor& ground, double min = 3.0) {
  const bool lightGround = lum(ground) > 0.4;
  for (int i = 0; i < 40 && contrast(c, ground) < min; ++i) {
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);
    l = lightGround ? std::max(0.f, l - 0.025f) : std::min(1.f, l + 0.025f);
    c = QColor::fromHslF(h < 0 ? 0 : h, s, l, a);
  }
  return c;
}
QColor onFill(const QColor& fill, const QColor& darkInk) {
  return lum(fill) > 0.42 ? darkInk : QColor(255, 255, 255);
}

void build() {
  const auto& p = theme::pal();
  const bool native = g_light != theme::isDark();   // light rendering of a light theme, dark of a dark one
  Vars v;
  v.light = g_light;
  if (native) {
    v.bg = p.bg;
    v.text = p.text;
    v.muted = theme::mix(p.textDim, p.textMute, 0.45);
    v.panel = theme::mix(p.bg, p.panelAlt, 0.75);
    v.panel2 = theme::mix(p.panelAlt, p.border, 0.32);
    v.border = theme::mix(p.divider, p.border, 0.45);
    v.accent = p.accent;
    v.up = p.up;
    v.down = p.down;
    v.warn = p.warn;
    v.grid = p.grid;
  } else {
    // The other kind, derived: the theme's text becomes the ground, its ground the text.
    const QColor ground = theme::isDark() ? theme::mix(p.text, QColor(255, 255, 255), 0.55)
                                          : theme::mix(p.text, QColor(0, 0, 0), 0.45);
    v.bg = ground;
    v.text = theme::isDark() ? p.bg : p.bg;
    v.muted = theme::mix(v.bg, v.text, 0.56);
    v.panel = theme::mix(v.bg, v.text, 0.025);
    v.panel2 = theme::mix(v.bg, v.text, 0.07);
    v.border = theme::mix(v.bg, v.text, 0.12);
    v.accent = tune(p.accent, v.bg, 3.2);
    v.up = tune(p.up, v.bg, 3.0);
    v.down = tune(p.down, v.bg, 3.0);
    v.warn = tune(p.warn, v.bg, 2.6);
    v.grid = theme::mix(v.bg, v.text, 0.07);
  }
  v.hover = v.panel2;
  v.input = v.bg;
  v.accentSoft = theme::mix(v.bg, v.accent, v.light ? 0.12 : 0.18);
  const QColor darkInk = lum(v.bg) < lum(v.text) ? v.bg : v.text;
  v.onAccent = onFill(v.accent, darkInk);
  v.gold = v.warn;
  v.goldSoft = theme::mix(v.bg, v.gold, 0.16);
  v.onGold = onFill(v.gold, darkInk);
  v.scroll = theme::mix(v.panel2, v.muted, 0.35);
  v.shadow = QColor(0, 0, 0, v.light ? 38 : 115);
  v.backdrop = QColor(0, 0, 0, 115);
  g_vars = v;
}
int buildKey() { return theme::revision() * 4 + (g_light ? 1 : 0) + g_lightRev * 1000003; }
}  // namespace

const Vars& v() {
  if (g_builtFor != buildKey()) { build(); g_builtFor = buildKey(); }
  return g_vars;
}
bool light() { return g_light; }
void setLight(bool on) { if (on != g_light) { g_light = on; ++g_lightRev; } }
int revision() { return buildKey(); }

QFont font(int px, int weight) { return theme::sans(px, weight); }
QFont monoFont(int px, int weight) { return theme::mono(px, weight); }

QColor colorOf(const QString& c) {
  const auto& s = v();
  if (c == "up") return s.up;
  if (c == "down") return s.down;
  if (c == "muted") return s.muted;
  if (c == "warn") return s.warn;
  if (c == "gold" || c == "gold-text") return s.gold;
  if (c == "accent") return s.accent;
  return s.text;
}

// ------------------------------------------------------------------------------------------------ style sheet
namespace {
QString asset(const QString& name, const QString& svg) {
  const QString dir = paths::cache("charts/ui");
  const QString f = dir + "/" + name + ".svg";
  if (!QFile::exists(f)) {
    QFile out(f);
    if (out.open(QIODevice::WriteOnly)) out.write(svg.toUtf8());
  }
  return f;
}
QString chevron(const QColor& c) {
  // select.tb: two 4x4 gradient halves = a small down chevron, 8 x 4 px.
  return asset("chev-" + c.name().mid(1),
               QStringLiteral(R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="5" viewBox="0 0 8 5"><path d="M0 0.5L4 4.5L8 0.5Z" fill="%1"/></svg>)").arg(c.name()));
}
QString tick(const QColor& c) {
  return asset("tick-" + c.name().mid(1),
               QStringLiteral(R"(<svg xmlns="http://www.w3.org/2000/svg" width="11" height="11" viewBox="0 0 24 24"><path d="M5 12.5l4.5 4.5L19 7" fill="none" stroke="%1" stroke-width="3.2" stroke-linecap="round" stroke-linejoin="round"/></svg>)").arg(c.name()));
}
QString rgba(const QColor& c) {
  return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}
}  // namespace

QString qss() {
  const auto& s = v();
  QString t = QStringLiteral(R"(
QLineEdit, QComboBox, QDateTimeEdit, QAbstractSpinBox, QAbstractItemView, QMenu, QToolTip { font-family: "@sans"; font-size: 13px; }
QWidget[qtc="pane-bg"], QWidget[qtc="side-bg"] { background: @bg; }
QLabel { background: transparent; }
QLabel[qtc="muted"] { color: @muted; }
QLabel[qtc="up"] { color: @up; }
QLabel[qtc="down"] { color: @down; }
QLabel[qtc="gold-text"] { color: @gold; }
QLabel[qtc="warn"] { color: @warn; }

QPushButton[qtc="tb"] { background: transparent; border: 0; border-radius: 4px; padding: 0 9px; color: @text; text-align: left; }
QPushButton[qtc="tb"][iconOnly="1"] { padding: 0 7px; }
QPushButton[qtc="tb"]:hover, QPushButton[qtc="tb"][active="1"] { background: @hover; }
QPushButton[qtc="tb"][on="1"] { color: @accent; }
QPushButton[qtc="tb"][symbol="1"] { font-weight: 600; font-size: 14px; }

QComboBox[qtc="tb-select"] { background: transparent; border: 0; border-radius: 4px; padding: 0 22px 0 9px; color: @text; }
QComboBox[qtc="tb-select"]:hover { background: @hover; }
QComboBox[qtc="tb-select"]::drop-down { border: 0; width: 22px; subcontrol-origin: padding; subcontrol-position: center right; }
QComboBox[qtc="tb-select"]::down-arrow { image: url(@chevmuted); width: 8px; height: 5px; }
QComboBox[qtc="tb-select"]:on { background: @hover; }

QCheckBox { spacing: 5px; color: @text; background: transparent; }
QCheckBox[qtc="tb-check"] { color: @muted; padding: 0 9px; border-radius: 4px; }
QCheckBox[qtc="tb-check"]:hover { background: @hover; }
QCheckBox::indicator { width: 12px; height: 12px; border: 1px solid @muted; border-radius: 3px; background: @input; }
QCheckBox::indicator:hover { border-color: @text; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(@tick); }

QPushButton[qtc="tf"] { background: transparent; border: 0; border-radius: 4px; padding: 0 6px; color: @text; }
QPushButton[qtc="tf"]:hover { background: @hover; }
QPushButton[qtc="tf"][active="1"] { color: @accent; background: @accentSoft; }
QLineEdit[qtc="tf-custom"] { border: 1px solid @border; border-radius: 4px; background: @input; padding: 0 6px; color: @text; font-size: 13px; }
QLineEdit[qtc="tf-custom"]:focus { border-color: @accent; }

QPushButton[qtc="drawbar"] { background: transparent; border: 0; border-radius: 4px; padding: 0; min-width: 34px; max-width: 34px; min-height: 34px; max-height: 34px; }
QPushButton[qtc="drawbar"]:hover { background: @hover; }
QPushButton[qtc="drawbar"][active="1"] { background: @accentSoft; }

QPushButton[qtc="tab"] { background: transparent; border: 0; border-radius: 4px; padding: 0 10px; color: @muted; }
QPushButton[qtc="tab"]:hover { background: @hover; color: @text; }
QPushButton[qtc="tab"][active="1"] { background: @hover; color: @text; }
QPushButton[qtc="tab-icon"] { background: transparent; border: 0; border-radius: 4px; padding: 0 6px; }
QPushButton[qtc="tab-icon"]:hover { background: @hover; }

QPushButton[qtc="icon-x"] { background: transparent; border: 0; padding: 0 2px; }

QPushButton[qtc="btn"] { background: @panel; border: 1px solid @border; border-radius: 4px; padding: 0 12px; color: @text; }
QPushButton[qtc="btn"]:hover { background: @hover; }
QPushButton[qtc="btn"][primary="1"] { background: @accent; border-color: @accent; color: @onAccent; }
QPushButton[qtc="btn"][primary="1"]:hover { background: @accentHi; border-color: @accentHi; }
QPushButton[qtc="btn"][small="1"] { padding: 0 8px; font-size: 12px; }
QPushButton[qtc="btn"][danger="1"] { color: @down; }
QPushButton[qtc="btn"][gold="1"] { background: @gold; border-color: @gold; color: @onGold; font-weight: 600; }
QPushButton[qtc="btn"]:disabled { color: @muted; }

QLineEdit[qtc="in"], QComboBox[qtc="in"], QDateTimeEdit[qtc="in"], QSpinBox[qtc="in"], QDoubleSpinBox[qtc="in"] {
  border: 1px solid @border; border-radius: 4px; background: @input; padding: 0 7px; color: @text; font-size: 13px;
  selection-background-color: @accentSoft; selection-color: @text; }
QLineEdit[qtc="in"]:focus, QComboBox[qtc="in"]:focus, QDateTimeEdit[qtc="in"]:focus { border-color: @accent; }
QComboBox[qtc="in"]::drop-down { border: 0; width: 20px; }
QComboBox[qtc="in"]::down-arrow { image: url(@chevtext); width: 8px; height: 5px; }
QDateTimeEdit[qtc="in"]::up-button, QDateTimeEdit[qtc="in"]::down-button { width: 0; border: 0; }
QPlainTextEdit[qtc="in"], QTextEdit[qtc="in"] { border: 1px solid @border; border-radius: 4px; background: @input; padding: 8px;
  color: @text; font-family: "@mono"; font-size: 12px; selection-background-color: @accentSoft; }
QLineEdit[qtc="search-in"] { border: 1px solid @border; border-radius: 6px; background: @input; padding: 0 12px; color: @text;
  font-size: 15px; selection-background-color: @accentSoft; selection-color: @text; }
QLineEdit[qtc="search-in"]:focus { border-color: @accent; }
QComboBox QAbstractItemView { background: @panel; border: 1px solid @border; color: @text; outline: 0;
  selection-background-color: @hover; selection-color: @text; padding: 2px; }

QLabel[qtc="hint"] { color: @muted; font-size: 12px; }
QLabel[qtc="field-label"] { color: @muted; font-size: 11px; }
QLabel[qtc="h4"] { color: @muted; font-size: 12px; font-weight: 600; }

QFrame[qtc="card"] { background: @panel; border: 1px solid @border; border-radius: 6px; }
QFrame[qtc="stat"] { background: @panel; border: 1px solid @border; border-radius: 6px; }
QFrame[qtc="stat"] QLabel#k { color: @muted; font-size: 11px; }
QFrame[qtc="stat"] QLabel#v { color: @text; font-size: 16px; font-weight: 600; font-family: "@mono"; }

QTableView[qtc="t"], QTreeView[qtc="t"] { background: transparent; border: 0; font-size: 12px; gridline-color: transparent;
  color: @text; outline: 0; selection-background-color: @hover; selection-color: @text; }
QTableView[qtc="t"]::item, QTreeView[qtc="t"]::item { padding: 5px 8px; border: 0; border-bottom: 1px solid @border; }
QTableView[qtc="t"]::item:hover, QTreeView[qtc="t"]::item:hover { background: @hover; }
QTableView[qtc="t"] QHeaderView::section, QTreeView[qtc="t"] QHeaderView::section { background: @bg; color: @muted;
  font-weight: 500; padding: 6px 8px; border: 0; border-bottom: 1px solid @border; font-size: 12px; }

QFrame[qtc="modal-card"] { background: @panel; border: 1px solid @border; border-radius: 8px; }
QWidget[qtc="modal-head"] { border-bottom: 1px solid @border; }
QLabel[qtc="modal-title"] { font-weight: 600; font-size: 15px; color: @text; }
QWidget[qtc="modal-foot"] { border-top: 1px solid @border; }

QLabel[qtc="toast"] { background: @panel; border: 1px solid @border; border-left: 3px solid @accent; border-radius: 6px;
  padding: 10px 12px; color: @text; }
QLabel[qtc="toast"][kind="err"] { border-left-color: @down; }
QLabel[qtc="toast"][kind="alert"] { border-left-color: @warn; }

QScrollArea[qtc="scroll"] { background: transparent; border: 0; }
QScrollArea[qtc="scroll"] > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: @scroll; border-radius: 4px; min-height: 24px; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
QScrollBar::handle:horizontal { background: @scroll; border-radius: 4px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QToolTip { background: @panel; color: @text; border: 1px solid @border; padding: 4px 6px; }
QMenu { background: @panel; border: 1px solid @border; color: @text; }
QMenu::item { padding: 6px 18px; } QMenu::item:selected { background: @hover; }
)");
  const QColor accentHi = s.light ? s.accent.darker(108) : s.accent.lighter(110);
  const QList<QPair<QString, QString>> rep{
      {"@accentSoft", s.accentSoft.name()}, {"@accentHi", accentHi.name()}, {"@onAccent", s.onAccent.name()},
      {"@onGold", s.onGold.name()}, {"@chevmuted", chevron(s.muted)}, {"@chevtext", chevron(s.muted)},
      {"@tick", tick(s.onAccent)}, {"@accent", s.accent.name()}, {"@panel", s.panel.name()},
      {"@border", s.border.name()}, {"@hover", s.hover.name()}, {"@input", s.input.name()},
      {"@muted", s.muted.name()}, {"@scroll", s.scroll.name()}, {"@text", s.text.name()}, {"@gold", s.gold.name()},
      {"@warn", s.warn.name()}, {"@down", s.down.name()}, {"@up", s.up.name()}, {"@bg", s.bg.name()},
      {"@mono", theme::mono(12).family()}, {"@sans", theme::sans(13).family()}};
  for (const auto& [k, val] : rep) t.replace(k, val);
  Q_UNUSED(rgba);
  return t;
}

// ------------------------------------------------------------------------------------------------ classes
void repolish(QWidget* w) {
  if (!w) return;
  w->style()->unpolish(w);
  w->style()->polish(w);
  w->update();
}
void cls(QWidget* w, const QString& qtc) {
  w->setProperty("qtc", qtc);
  repolish(w);
}
void setKind(QWidget* w, const QString& kinds) {
  const QStringList ks = kinds.split(' ', Qt::SkipEmptyParts);
  for (const char* k : {"primary", "small", "danger", "gold"}) w->setProperty(k, ks.contains(k) ? "1" : "0");
  if (ks.contains("small")) w->setFixedHeight(24);
  repolish(w);
}

namespace {
QColor iconColorFor(QAbstractButton* b, bool hovered) {
  const auto& s = v();
  const QString c = b->property("qtc").toString();
  const bool on = b->property("on").toString() == "1", active = b->property("active").toString() == "1";
  if (c == "tf" || c == "drawbar") return active ? s.accent : s.text;
  if (c == "tb") return on ? s.accent : s.text;
  if (c == "tab") return (active || hovered) ? s.text : s.muted;
  if (c == "icon-x") return hovered ? s.text : s.muted;
  if (c == "tab-icon") return s.text;
  if (c == "modal-x") return hovered ? s.text : s.muted;
  return b->isEnabled() ? s.text : s.muted;
}
void applyIcon(QAbstractButton* b, bool hovered) {
  const QString svg = b->property("iconSvg").toString();
  if (svg.isEmpty()) return;
  const int px = b->property("iconPx").toInt() > 0 ? b->property("iconPx").toInt() : 18;
  const QColor c = iconColorFor(b, hovered);
  const qreal dpr = b->devicePixelRatioF() > 0 ? b->devicePixelRatioF() : 2.0;
  QIcon ic;
  ic.addPixmap(icons::pixmap(svg, c, px, dpr));
  b->setIcon(ic);
  b->setIconSize(QSize(px, px));
}
class HoverIcons : public QObject {
public:
  using QObject::QObject;
  bool eventFilter(QObject* o, QEvent* e) override {
    if (e->type() == QEvent::Enter || e->type() == QEvent::Leave) {
      if (auto* b = qobject_cast<QAbstractButton*>(o)) applyIcon(b, e->type() == QEvent::Enter);
    }
    return false;
  }
};
HoverIcons* hoverFilter() {
  static QPointer<HoverIcons> f;
  if (!f) f = new HoverIcons(qApp);
  return f;
}
}  // namespace

void setIconSvg(QAbstractButton* b, const QString& svg, int px) {
  b->setProperty("iconSvg", svg);
  b->setProperty("iconPx", px);
  b->removeEventFilter(hoverFilter());
  b->installEventFilter(hoverFilter());
  applyIcon(b, b->underMouse());
}
void refreshIcons(QWidget* root) {
  if (!root) return;
  for (auto* b : root->findChildren<QAbstractButton*>())
    if (b->property("iconSvg").isValid()) applyIcon(b, b->underMouse());
}
void setOn(QWidget* w, bool on) {
  w->setProperty("on", on ? "1" : "0");
  repolish(w);
  if (auto* b = qobject_cast<QAbstractButton*>(w)) applyIcon(b, b->underMouse());
}
void setActive(QWidget* w, bool on) {
  w->setProperty("active", on ? "1" : "0");
  repolish(w);
  if (auto* b = qobject_cast<QAbstractButton*>(w)) applyIcon(b, b->underMouse());
}

// ------------------------------------------------------------------------------------------------ factories
QPushButton* tbButton(const QString& text, const QString& iconSvg, const QString& tip, QWidget* parent) {
  auto* b = new QPushButton(text, parent);
  b->setProperty("qtc", "tb");
  b->setProperty("iconOnly", text.isEmpty() ? "1" : "0");
  b->setFixedHeight(32);
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::NoFocus);
  if (!tip.isEmpty()) b->setToolTip(tip);
  if (!iconSvg.isEmpty()) setIconSvg(b, iconSvg, 18);
  return b;
}
QComboBox* tbSelect(const QString& tip, QWidget* parent) {
  auto* c = new QComboBox(parent);
  c->setProperty("qtc", "tb-select");
  c->setFixedHeight(32);
  c->setCursor(Qt::PointingHandCursor);
  c->setFocusPolicy(Qt::NoFocus);
  c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  if (!tip.isEmpty()) c->setToolTip(tip);
  return c;
}
QCheckBox* tbCheck(const QString& text, const QString& tip, QWidget* parent) {
  auto* c = new QCheckBox(text, parent);
  c->setProperty("qtc", "tb-check");
  c->setFixedHeight(32);
  c->setCursor(Qt::PointingHandCursor);
  c->setFocusPolicy(Qt::NoFocus);
  if (!tip.isEmpty()) c->setToolTip(tip);
  return c;
}
QPushButton* btn(const QString& text, const QString& kinds, QWidget* parent) {
  auto* b = new QPushButton(text, parent);
  b->setProperty("qtc", "btn");
  b->setFixedHeight(28);
  b->setCursor(Qt::PointingHandCursor);
  b->setAutoDefault(false);
  setKind(b, kinds);
  return b;
}
QPushButton* iconButton(const QString& iconSvg, const QString& tip, int iconPx, QWidget* parent) {
  auto* b = new QPushButton(parent);
  b->setProperty("qtc", "icon-x");
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::NoFocus);
  b->setFixedSize(iconPx + 4, std::max(18, iconPx + 4));
  if (!tip.isEmpty()) b->setToolTip(tip);
  setIconSvg(b, iconSvg, iconPx);
  return b;
}
QLineEdit* input(const QString& placeholder, QWidget* parent) {
  auto* e = new QLineEdit(parent);
  e->setProperty("qtc", "in");
  e->setFixedHeight(28);
  e->setPlaceholderText(placeholder);
  return e;
}
QComboBox* select(QWidget* parent) {
  auto* c = new QComboBox(parent);
  c->setProperty("qtc", "in");
  c->setFixedHeight(28);
  c->setMinimumWidth(70);
  return c;
}
QLabel* hint(const QString& html, QWidget* parent) {
  auto* l = new QLabel(html, parent);
  l->setProperty("qtc", "hint");
  l->setWordWrap(true);
  l->setTextFormat(Qt::RichText);
  l->setOpenExternalLinks(true);
  return l;
}
QLabel* h4(const QString& text, QWidget* parent) {
  auto* l = new QLabel(text.toUpper(), parent);
  l->setProperty("qtc", "h4");
  QFont f = font(12, QFont::DemiBold);
  f.setLetterSpacing(QFont::PercentageSpacing, 104);
  l->setFont(f);
  l->setContentsMargins(0, 10, 0, 4);
  return l;
}
QLabel* text(const QString& html, const QString& qtc, QWidget* parent) {
  auto* l = new QLabel(html, parent);
  if (!qtc.isEmpty()) l->setProperty("qtc", qtc);
  return l;
}
QWidget* field(const QString& label, const QList<QWidget*>& controls, QWidget* parent) {
  auto* w = new QWidget(parent);
  w->setProperty("qtc", "field");
  auto* v = new QVBoxLayout(w);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(3);
  auto* l = new QLabel(label, w);
  l->setProperty("qtc", "field-label");
  v->addWidget(l);
  for (auto* c : controls) {
    c->setParent(w);
    v->addWidget(c);
  }
  return w;
}
QWidget* field(const QString& label, QWidget* control, QWidget* parent) {
  return field(label, QList<QWidget*>{control}, parent);
}
QWidget* formGrid(const QList<QWidget*>& fields, QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* g = new QGridLayout(w);
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(10);
  g->setColumnStretch(0, 1);
  g->setColumnStretch(1, 1);
  for (int i = 0; i < fields.size(); ++i) {
    fields[i]->setParent(w);
    g->addWidget(fields[i], i / 2, i % 2, Qt::AlignTop);
  }
  return w;
}
QFrame* card(QWidget* parent) {
  auto* f = new QFrame(parent);
  f->setProperty("qtc", "card");
  return f;
}
QFrame* stat(const QString& k, const QString& val, QWidget* parent) {
  auto* f = new QFrame(parent);
  f->setProperty("qtc", "stat");
  auto* l = new QVBoxLayout(f);
  l->setContentsMargins(10, 8, 10, 8);
  l->setSpacing(2);
  auto* kl = new QLabel(k, f);
  kl->setObjectName("k");
  auto* vl = new QLabel(val, f);
  vl->setObjectName("v");
  l->addWidget(kl);
  l->addWidget(vl);
  return f;
}
QScrollArea* scrollArea(QWidget* content, QWidget* parent) {
  auto* s = new QScrollArea(parent);
  s->setProperty("qtc", "scroll");
  s->setFrameShape(QFrame::NoFrame);
  s->setWidgetResizable(true);
  s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  s->setWidget(content);
  s->viewport()->setAutoFillBackground(false);
  content->setAutoFillBackground(false);
  return s;
}

// ------------------------------------------------------------------------------------------------ TabBar
TabBar::TabBar(QWidget* parent) : QWidget(parent) {
  setFixedHeight(38);
  m_strip = new QWidget(this);
  m_strip->installEventFilter(this);
  m_row = new QHBoxLayout(m_strip);
  m_row->setContentsMargins(6, 0, 6, 1);
  m_row->setSpacing(2);
}
QSize TabBar::sizeHint() const { return QSize(m_strip->sizeHint().width(), 38); }
QSize TabBar::minimumSizeHint() const { return QSize(0, 38); }
void TabBar::placeStrip() {
  const int w = qMax(width(), m_strip->sizeHint().width());
  m_offset = qBound(0, m_offset, w - width());
  m_strip->setGeometry(-m_offset, 0, w, height());
}
void TabBar::wheelEvent(QWheelEvent* e) {
  const QPoint d = e->angleDelta();
  const int step = std::abs(d.x()) > std::abs(d.y()) ? d.x() : d.y();   // a mouse wheel scrolls it too
  if (step == 0 || m_strip->width() <= width()) { e->ignore(); return; }
  m_offset -= step / 2;
  placeStrip();
  e->accept();
}
void TabBar::resizeEvent(QResizeEvent* e) {
  QWidget::resizeEvent(e);
  placeStrip();
}
bool TabBar::eventFilter(QObject* o, QEvent* e) {
  if (o == m_strip && e->type() == QEvent::LayoutRequest) {   // a tab added, a font or label change
    placeStrip();
    updateGeometry();
  }
  return QWidget::eventFilter(o, e);
}
QPushButton* TabBar::addTab(const QString& id, const QString& label) {
  auto* b = new QPushButton(label, m_strip);
  b->setProperty("qtc", "tab");
  b->setFixedHeight(30);
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::NoFocus);
  m_row->addWidget(b);
  m_tabs << qMakePair(id, b);
  connect(b, &QPushButton::clicked, this, [this, id] { emit tabClicked(id); });
  return b;
}
void TabBar::addStretch() { m_row->addStretch(1); }
QPushButton* TabBar::addIcon(const QString& iconSvg, const QString& tip) {
  auto* b = new QPushButton(m_strip);
  b->setProperty("qtc", "tab-icon");
  b->setFixedHeight(30);
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::NoFocus);
  b->setToolTip(tip);
  setIconSvg(b, iconSvg, 18);
  m_row->addWidget(b);
  return b;
}
void TabBar::setCurrent(const QString& id) {
  m_cur = id;
  for (const auto& [i, b] : m_tabs) setActive(b, i == id);
}
QPushButton* TabBar::button(const QString& id) const {
  for (const auto& [i, b] : m_tabs) if (i == id) return b;
  return nullptr;
}
void TabBar::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(QRect(0, height() - 1, width(), 1), v().border);
}

// ------------------------------------------------------------------------------------------------ ItemList
namespace {
constexpr int kItemH = 34;    // padding 8 + line 18 + 8
constexpr int kGroupH = 30;   // padding 10 + 15 + 4 (+1)
QString colourMuted(QString html) {
  html.replace(QStringLiteral("<span class=\"muted\">"), QStringLiteral("<span style=\"color:%1\">").arg(v().muted.name()));
  return html;
}
}  // namespace
ItemList::ItemList(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}
void ItemList::setItems(const QList<ListItem>& items) {
  m_items = items;
  m_prepKey.clear();
  m_hover = -1;
  if (m_sel >= m_items.size()) m_sel = -1;
  updateGeometry();
  setMinimumHeight(heightForWidth(width()));
  update();
}
void ItemList::setSelected(int i) { m_sel = i; update(); }
void ItemList::moveSelection(int delta) {
  int i = m_sel;
  for (int n = 0; n < m_items.size(); ++n) {
    i = std::clamp(i + delta, 0, int(m_items.size()) - 1);
    if (!m_items[i].group) break;
  }
  if (i >= 0 && i < m_items.size() && !m_items[i].group) setSelected(i);
}
int ItemList::rowHeight(int i) const { return m_items[i].group ? kGroupH : kItemH; }
int ItemList::rowTop(int i) const {
  int y = 0;
  for (int k = 0; k < i; ++k) y += rowHeight(k);
  return y;
}
int ItemList::rowAt(int y) const {
  int top = 0;
  for (int i = 0; i < m_items.size(); ++i) {
    const int h = rowHeight(i);
    if (y >= top && y < top + h) return i;
    top += h;
  }
  return -1;
}
int ItemList::heightForWidth(int) const { return m_items.isEmpty() ? 0 : rowTop(m_items.size()); }
QSize ItemList::sizeHint() const { return {300, heightForWidth(0)}; }
void ItemList::prepare() {
  const QFont body = style::font(13), meta = style::font(12);
  const QString key = body.key() + '|' + meta.key() + '|' + v().muted.name();
  if (key == m_prepKey && m_prep.size() == m_items.size()) return;
  m_prepKey = key;
  m_prep.clear();
  m_prep.reserve(m_items.size());
  const QFontMetrics mfm(meta);
  QTextOption opt;
  opt.setWrapMode(QTextOption::NoWrap);
  for (const auto& it : m_items) {
    Prepared pr;
    if (!it.group) {
      if (!it.meta.isEmpty()) pr.metaW = mfm.horizontalAdvance(it.meta);
      pr.left = QStaticText(colourMuted(it.left));
      pr.left.setTextFormat(Qt::RichText);
      pr.left.setTextWidth(-1);
      pr.left.setTextOption(opt);
      pr.left.prepare(QTransform(), body);
    }
    m_prep << pr;
  }
}
void ItemList::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& s = v();
  prepare();
  int y = 0;
  const QFont body = style::font(13), meta = style::font(12);
  QFont grp = style::font(11);
  grp.setLetterSpacing(QFont::AbsoluteSpacing, 11 * 0.05);
  for (int i = 0; i < m_items.size(); ++i) {
    const auto& it = m_items[i];
    const int h = rowHeight(i);
    const QRect r(0, y, width(), h);
    y += h;
    if (!r.intersects(e->rect())) continue;
    if (it.group) {
      p.setFont(grp);
      p.setPen(it.gold ? s.gold : s.muted);
      p.drawText(QRect(10, r.top() + 10, r.width() - 20, 16), Qt::AlignLeft | Qt::AlignVCenter, it.left.toUpper());
      continue;
    }
    if (i == m_sel || i == m_hover) {
      p.setPen(Qt::NoPen);
      p.setBrush(s.hover);
      p.drawRoundedRect(QRectF(r), 4, 4);
    }
    const Prepared& pr = m_prep[i];
    const int metaW = pr.metaW;
    if (!it.meta.isEmpty()) {
      p.setFont(meta);
      p.setPen(s.muted);
      p.drawText(QRect(r.right() - 10 - metaW, r.top(), metaW + 1, h), Qt::AlignVCenter | Qt::AlignLeft, it.meta);
    }
    const QStaticText& st = pr.left;
    p.setFont(body);
    p.setPen(s.text);
    const int avail = r.width() - 20 - (metaW ? metaW + 10 : 0);
    p.save();
    p.setClipRect(QRect(10, r.top(), avail, h));
    p.drawStaticText(QPointF(10, r.top() + (h - st.size().height()) / 2.0), st);
    p.restore();
  }
}
void ItemList::mouseMoveEvent(QMouseEvent* e) {
  int i = rowAt(int(e->position().y()));
  if (i >= 0 && m_items[i].group) i = -1;
  if (i != m_hover) { m_hover = i; update(); }
  setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
void ItemList::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  const int i = rowAt(int(e->position().y()));
  if (i >= 0 && !m_items[i].group) emit activated(i);
}
void ItemList::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------------------------------------ StatusDot
StatusDot::StatusDot(QWidget* parent) : QWidget(parent) { setFixedSize(9, 9); }
void StatusDot::setLevel(const QString& level) { m_level = level; update(); }
void StatusDot::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& s = v();
  const QColor c = m_level == "ok" ? s.up : m_level == "warn" ? s.warn : m_level == "bad" ? s.down : s.muted;
  p.setPen(Qt::NoPen);
  p.setBrush(c);
  p.drawEllipse(QRectF(rect()));
}

}  // namespace charts::ui::style
