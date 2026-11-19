#pragma once
// Charts tab — quant-terminal's look (public/css/app.css, MIT, Copyright (c) 2026 Lewis Jackson —
// docs/charts/NOTICE.md) mapped onto Deepwater's theme, plus the shared widget styling of the tab.
//
// HOW TO USE (every Charts UI file):
//  - Colours: charts::ui::style::v().<css variable> (the CSS :root variables, same names in camelCase). Never a
//    literal colour, except chart series colours from the JS (#2962ff, #26a69a, …), which stay literal.
//  - Widgets: create them with the helpers below (or call style::cls(w, "btn") on your own widget). They set the
//    `qtc` property ("quant-terminal class") that the page style sheet (qss(), installed on ChartsApp, inherited
//    by every child including modals, panels and combo popups) matches, so they follow theme changes by themselves.
//  - Repaint your own painted widgets on ChartsApp::themeChanged (fired when the Deepwater theme or the tab's
//    light/dark button changes). style::revision() changes with both: key for caches.
//  - Fonts: style::font(px) = the CSS body font (13 px) → Deepwater sans (Inter); style::monoFont(px) where the
//    CSS uses tabular numbers or monospace (textarea, prices, code) → JetBrains Mono.
//
// LIGHT / DARK (the topbar's moon button, settings.theme): "dark" and "light" are two renderings of the CURRENT
// Deepwater theme. The one that matches the theme's own kind (dark for Deepwater, Ember, …; light for Glacier, Dune,
// Sakura) uses the palette as is; the other one is derived from it by swapping the ground and the text
// (bg ↔ text) and re-tuning accent / up / down / warn for contrast on the new ground. So the tab always reads as
// the current Deepwater theme, in a dark or a light version.
#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QFrame>
#include <QList>
#include <QStaticText>

class QAbstractButton;
class QPushButton;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QScrollArea;

namespace charts::ui::style {

// ---- CSS variables (app.css :root / [data-theme=light]) → Deepwater palette roles ----
struct Vars {
  QColor bg;          // --bg        page ground, panes, side/bottom panels   ← theme::pal().bg
  QColor panel;       // --panel     modal card, toasts, cards, buttons       ← bg → panelAlt (75 %)
  QColor panel2;      // --panel-2
  QColor border;      // --border    1 px lines, grid gaps                    ← divider ↔ border
  QColor text;        // --text                                                ← pal().text
  QColor muted;       // --muted     labels, meta, inactive tabs              ← textDim ↔ textMute
  QColor accent;      // --accent    active tf / tool, primary button, focus  ← pal().accent
  QColor accentSoft;  // --accent-soft  active tf / tool background           ← 18 % accent on bg
  QColor onAccent;    // text on a filled accent (the CSS #fff)               ← theme::onAccent()
  QColor up;          // --up                                                 ← pal().up
  QColor down;        // --down                                               ← pal().down
  QColor hover;       // --hover     hover / selected row fill
  QColor grid;        // --grid      chart grid lines                         ← pal().grid
  QColor input;       // --input     input ground (= bg like the CSS)
  QColor shadow;      // --shadow colour (0 8px 32px)
  QColor gold;        // --gold      "Quant (bonus)" features                 ← pal().warn
  QColor goldSoft;    // --gold-soft
  QColor onGold;      // text on gold (CSS #1a1405)
  QColor warn;        // the CSS literal #f7a600 (status warn, alert toasts, notes, impact Medium) ← pal().warn
  QColor scroll;      // --scroll    scroll bar thumb
  QColor backdrop;    // .modal background rgba(0,0,0,.45)
  bool light = false; // the light rendering is on
};
const Vars& v();

bool light();                 // settings.theme == "light"
void setLight(bool on);       // ChartsApp calls it (start + theme button); bumps revision()
int revision();               // changes with theme::revision() and setLight(): key for painted caches

// Fonts: the CSS body is 13px system sans → Deepwater sans; tabular numbers / code → mono.
QFont font(int px = 13, int weight = QFont::Normal);
QFont monoFont(int px = 12, int weight = QFont::Normal);

// The page style sheet: every `qtc` class below. ChartsApp installs it on itself and re-installs it on theme change.
QString qss();

// ---- classes (CSS class → `qtc` property value) ----
//  tb          .tb            topbar button (transparent, 32 px high, hover fill); with no text = .tb.icon
//  tb-select   select.tb      topbar select (transparent, chevron)
//  tb-check    .tb.check      topbar checkbox label ("Sync")
//  tf          .tf-group button  quick timeframe button (active = accent on accent-soft)
//  tf-custom   .tf-custom     custom timeframe input (46x28, centered)
//  drawbar     .drawbar button   34x34 tool button (active = accent on accent-soft)
//  tab         .tabs button   panel tab (muted; hover/active = text on hover)
//  btn         .btn           28 px button on panel; modifiers with setKind(): "primary" "small" "danger" "gold"
//  in          input.in / select.in / textarea.in / .field input,select  (28 px, input ground, border)
//  search-in   .search-in     38 px search box of the modals (15 px font)
//  hint        .hint-text     12 px muted text (QLabel, rich text allowed: <code>, <b>)
//  field       .field         label above a control, 11 px muted (QWidget made by field())
//  h4          h4             12 px 600 muted uppercase section title
//  card        .card          bordered rounded box on panel (6 px radius)
//  stat        .stat          stats box (.k 11 px muted / .v 16 px 600 tabular)
//  t           table.t        QTableView / QTreeView / QTableWidget: 12 px rows, right aligned, muted header
//  modal-card  .modal-card    the modal card (ChartsApp)
//  toast       .toast         a toast (ChartsApp); kind via property "kind": "" | err | alert | ok
//  muted / up / down / gold-text   text colour classes for QLabel
//  pane-bg / side-bg          plain bg ground (QWidget containers)
void cls(QWidget* w, const QString& qtc);                  // set the class and re-polish
// Modifiers: "primary", "small", "danger", "gold" for btn, space separated ("small primary").
void setKind(QWidget* w, const QString& kinds);
// .on (accent text, e.g. Log, Replay, custom tf in use) and .active (hover fill / tf, drawbar, tab selected).
void setOn(QWidget* w, bool on);
void setActive(QWidget* w, bool on);
void repolish(QWidget* w);
// Buttons made by the factories keep their SVG in the "iconSvg" property and redraw it in the colour of their state.
// ChartsApp calls refreshIcons(itself) on theme change; call it on a detached widget tree of yours if needed.
void setIconSvg(QAbstractButton* b, const QString& iconSvg, int px = 18);
void refreshIcons(QWidget* root);

// ---- widget factories (parent may be null; add them to your layout) ----
// Topbar button: text and/or an SVG icon (inner markup of <svg viewBox="0 0 24 24">, see Icons.h). The icon follows
// the button colour (text, accent when on) and the theme. Icon only → .tb.icon padding.
QPushButton* tbButton(const QString& text, const QString& iconSvg = {}, const QString& tip = {},
                      QWidget* parent = nullptr);
QComboBox* tbSelect(const QString& tip = {}, QWidget* parent = nullptr);
QCheckBox* tbCheck(const QString& text, const QString& tip = {}, QWidget* parent = nullptr);
// .btn: kinds "", "primary", "small", "danger", "gold" (space separated).
QPushButton* btn(const QString& text, const QString& kinds = {}, QWidget* parent = nullptr);
// Icon-only small button inside panels/rows (like `.wl-row .x`, `.rule .x`, `.tabs .icon`): muted, text on hover.
QPushButton* iconButton(const QString& iconSvg, const QString& tip = {}, int iconPx = 14, QWidget* parent = nullptr);
QLineEdit* input(const QString& placeholder = {}, QWidget* parent = nullptr);   // .in
QComboBox* select(QWidget* parent = nullptr);                                  // select.in
QLabel* hint(const QString& html, QWidget* parent = nullptr);                  // .hint-text (word wrap on)
QLabel* h4(const QString& text, QWidget* parent = nullptr);                    // h4 (text upper-cased like the CSS)
QLabel* text(const QString& html, const QString& qtc = {}, QWidget* parent = nullptr);   // plain / muted / up / down
// <label class="field">Label<control></label>: label above the control, 3 px gap.
QWidget* field(const QString& label, QWidget* control, QWidget* parent = nullptr);
QWidget* field(const QString& label, const QList<QWidget*>& controls, QWidget* parent = nullptr);
// <div class="form-grid">: two equal columns, 10 px gaps; controls stretch to the column width.
QWidget* formGrid(const QList<QWidget*>& fields, QWidget* parent = nullptr);
QFrame* card(QWidget* parent = nullptr);                                        // .card (layout yours)
// .stat box: k (label) above v (value). Returns the frame; value label has objectName "v" (find it to update).
QFrame* stat(const QString& k, const QString& v, QWidget* parent = nullptr);

// Text colour of a status class (for painted widgets): "up" "down" "muted" "warn" "gold" "" (text).
QColor colorOf(const QString& cls);

// ---- .tabs bar (side panel, bottom panel): 38 px, 1 px bottom border, buttons 30 px, active = text on hover ----
class TabBar : public QWidget {
  Q_OBJECT
public:
  explicit TabBar(QWidget* parent = nullptr);
  QPushButton* addTab(const QString& id, const QString& label);
  void addStretch();                                   // <div class="spacer">
  QPushButton* addIcon(const QString& iconSvg, const QString& tip);   // .tabs .icon (e.g. bottom toggle)
  void setCurrent(const QString& id);                  // no signal
  QString current() const { return m_cur; }
  QPushButton* button(const QString& id) const;
  QHBoxLayout* row() const { return m_row; }
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;   // narrow: what doesn't fit is cut off on the right
signals:
  void tabClicked(const QString& id);                  // the JS onclick: the owner decides (toggle, switch)
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void wheelEvent(QWheelEvent*) override;   // a bar narrower than its tabs scrolls sideways (no visible bar)
  bool eventFilter(QObject* o, QEvent* e) override;
private:
  void placeStrip();
  int m_offset = 0;   // px the strip is scrolled left
  // The buttons live on a strip at least as wide as their natural width. CSS: `.tabs button { white-space: nowrap }`
  // keeps each label whole (a flex item never shrinks below its text), so a narrow bar overflows on the right instead
  // of squeezing every label (QBoxLayout would cut each one on both sides).
  QWidget* m_strip;
  QHBoxLayout* m_row;
  QString m_cur;
  QList<QPair<QString, QPushButton*>> m_tabs;
};

// ---- .list with .item rows and .group headers (modal lists; usable anywhere): painted, fast for long lists ----
struct ListItem {
  QString left;        // rich text: <b>, plain text (escape yourself with toHtmlEscaped)
  QString meta;        // right part (.meta: 12 px muted)
  QString data;        // anything (id, symbol)
  bool group = false;  // .group header (11 px muted uppercase, 0.05em spacing); not clickable
  bool gold = false;   // .gold-text on a group
};
class ItemList : public QWidget {
  Q_OBJECT
public:
  explicit ItemList(QWidget* parent = nullptr);
  void setItems(const QList<ListItem>& items);
  const QList<ListItem>& items() const { return m_items; }
  void setSelected(int i);                 // -1 = none; index into items (groups skipped by moveSelection)
  int selected() const { return m_sel; }
  void moveSelection(int delta);           // arrow keys
  QSize sizeHint() const override;
  int heightForWidth(int) const override;
  bool hasHeightForWidth() const override { return true; }
signals:
  void activated(int index);               // click on an item
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  int rowAt(int y) const;
  int rowTop(int i) const;
  int rowHeight(int i) const;
  void prepare();                          // builds m_prep for the current items, fonts and theme
  QList<ListItem> m_items;
  int m_sel = -1, m_hover = -1;
  // Laid-out text per item, kept across paints (the picker has ~100 rows; a caret blink or a hover repaints them).
  struct Prepared { QStaticText left; int metaW = 0; };
  QList<Prepared> m_prep;
  QString m_prepKey;                       // fonts + muted colour the cache was built with ("" = stale)
};

// ---- status dot (.status: 9 px circle; ok = up, warn = #f7a600, bad = down, none = muted) ----
class StatusDot : public QWidget {
  Q_OBJECT
public:
  explicit StatusDot(QWidget* parent = nullptr);
  void setLevel(const QString& level);     // "ok" | "warn" | "bad" | ""
  QString level() const { return m_level; }
  QSize sizeHint() const override { return {9, 9}; }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QString m_level;
};

// A vertical scroll area with the CSS thin scroll bars, transparent ground, no frame; content resizes to width.
QScrollArea* scrollArea(QWidget* content, QWidget* parent = nullptr);

}  // namespace charts::ui::style
