#pragma once
// Charts tab — bottom panel helpers shared by its tabs: the formats of quant-terminal public/js/api.js (fmtPrice,
// fmtNum, fmtPct, ticker, exchange), the browser's toLocale*String, and the HTML building blocks the tabs' markup
// uses (table.t, .row-flex, .stats, .tester two-column grid) as Qt widgets. (MIT, Copyright (c) 2026 Lewis Jackson —
// docs/charts/NOTICE.md.) Colours and fonts from charts::ui::style (Style.h).
#include <QColor>
#include <QLayout>
#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>
#include <optional>

class QScrollArea;
class QVBoxLayout;
class QLabel;
class QComboBox;
class QLineEdit;

namespace charts::ui { class ChartPane; }

namespace charts::ui::bottom {

// ---- the market a chart's bars belong to ----
// ChartsApp sets cfg["symbol"] / cfg["tf"] as soon as the user switches market, while `bars` stay the previous
// market's until the new load ends. The panels attribute a result or price bars by this key instead: it is the cfg
// as it was when the pane last emitted loaded(). watchPane() starts following a pane (BottomPanel::onActiveChange
// calls it for every pane); a pane first seen with bars is assumed to hold its cfg's market.
struct BarsKey {
  QString symbol, tf;
  bool operator==(const BarsKey&) const = default;
};
void watchPane(ChartPane* p);
BarsKey barsKey(ChartPane* p);
// true while the pane's bars are still another market's or timeframe's than its cfg (a load is in flight)
bool barsStale(ChartPane* p);

// ---- api.js formats ----
QString fmtPrice(double v);
QString fmtNum(double v, int d = 2);
QString fmtPct(double v);
QString ticker(const QString& id);
QString exchange(const QString& id);
QString jsFixed(double v, int d);              // v.toFixed(d)
QString jsNum(double v);                        // String(v)
QString jsPrecision(double v, int p);           // String(Number(v.toPrecision(p)))
QString localeInt(double v);                    // n.toLocaleString() for whole numbers: 12,345
// new Date(ms).toLocaleDateString() / toLocaleString() as the reference browser (en-US, local time zone).
QString localeDate(qint64 ms);
QString localeDateTime(qint64 ms);
QString esc(const QString& s);                  // HTML escape for rich-text labels

// ---- <select> with <option value> and <optgroup> (bold, not selectable; children indented in the popup) ----
struct Opt {
  QString text, value;
  bool group = false;     // <optgroup label="text">
  bool child = false;     // option inside a group
};
QComboBox* optSelect(const QList<Opt>& opts, const QString& selected = {}, bool classIn = true);
QString optValue(const QComboBox* c);
void setOptValue(QComboBox* c, const QString& value);
// <input type="number">: a .in line edit holding String(value), `width` px wide (0 = the browser default 140).
QLineEdit* numInput(const QString& text, int width = 0);
// The value of an <input type="number">: nullopt when the browser would report "" (empty, or text that isn't a valid
// floating-point number such as "abc" or a lone "-"). A decimal comma reads as a point ("1,5" = 1.5). Never NaN.
std::optional<double> inputValue(const QLineEdit* e);
// Number(el.value) of an <input type="number">: "" (empty or invalid text) -> 0.
double inputNumber(const QLineEdit* e);
// JS prompt() / confirm() (the browser's own dialogs).
QString prompt(QWidget* parent, const QString& text, bool* ok = nullptr);
bool confirm(QWidget* parent, const QString& text);

// ---- .row-flex: wrapping row, 8 px gaps, items centred on their line (align-self:end via alignEnd()) ----
class FlowLayout : public QLayout {
public:
  explicit FlowLayout(QWidget* parent = nullptr, int hGap = 8, int vGap = 8);
  ~FlowLayout() override;
  void addItem(QLayoutItem* item) override;
  void addSpacer();                             // <span class="spacer"> (flex:1): pushes what follows to the right
  QLayoutItem* itemAt(int i) const override;
  QLayoutItem* takeAt(int i) override;
  int count() const override;
  Qt::Orientations expandingDirections() const override { return {}; }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int w) const override;
  QSize minimumSize() const override;
  QSize sizeHint() const override;
  void setGeometry(const QRect& r) override;
  void setNoWrap(bool on) { m_noWrap = on; }    // style flex-wrap: nowrap
private:
  int doLayout(const QRect& r, bool test) const;
  QList<QLayoutItem*> m_items;
  QList<bool> m_spacer;
  int m_h, m_v;
  bool m_noWrap = false;
};
void alignEnd(QWidget* w);                       // style="align-self:end"
void setFlex(QWidget* w, int grow);              // style="flex:1" inside a FlowLayout line
QWidget* rowFlex(const QList<QWidget*>& items, QWidget* parent = nullptr);   // nullptr item = spacer

// ---- .stats: grid repeat(auto-fill, minmax(min, 1fr)), 8 px gaps ----
class StatsGrid : public QWidget {
public:
  explicit StatsGrid(int minCol = 130, QWidget* parent = nullptr);
  void addStat(const QString& k, const QString& v, const QString& vCls = {});
  void clear();
  QSize sizeHint() const override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int w) const override;
protected:
  void resizeEvent(QResizeEvent*) override;
private:
  void place();
  int m_min;
  QList<QWidget*> m_cells;
};

// ---- table.t: painted HTML table (auto layout, sticky header, hover rows, coloured cells, buttons) ----
struct Cell {
  QString text;
  QString cls;            // "" | "up" | "down" | "muted" | "warn"
  bool left = false;      // .l (text-align left); the first column is always left
  bool center = false;
  bool bold = false;
  QColor bg;              // style="background:…"
  bool outline = false;   // .base in .heat: 2 px gold outline, bold
  bool button = false;    // <button class="btn small">text</button>
  QString buttonKind;     // "danger"…
  QString data;           // data-* of the cell's button
  bool operator==(const Cell&) const = default;   // lets a caller skip setRows() when nothing shown changed
};
struct Row {
  QList<Cell> cells;
  QString data;           // data-s / data-t of the <tr>
  bool span = false;      // one cell spanning every column (colspan)
  bool sum = false;       // .seas-sum: bold with a top border
  bool operator==(const Row&) const = default;
};
struct TableStyle {
  int font = 12, padX = 8, padY = 5, thPadY = 6;
  bool center = false;    // .seas-table / .heat: centred cells
  bool wrap = false;      // white-space: normal (the tester's right column, the gold box)
  bool header = true;
};
class Table : public QWidget {
  Q_OBJECT
public:
  explicit Table(QWidget* parent = nullptr);
  void setStyle(const TableStyle& s);
  void setHeader(const QList<Cell>& cols);
  void setRows(const QList<Row>& rows);
  const QList<Row>& rows() const { return m_rows; }
  // The scroll area whose viewport the header sticks to (position: sticky; top: 0). nullptr = none.
  void setStickyIn(QScrollArea* area);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int w) const override;
signals:
  void headerClicked(int col);
  void rowClicked(int row);
  void buttonClicked(int row, int col, const QString& data);
protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject* o, QEvent* e) override;
private:
  struct Geo { QVector<int> x, w; QVector<int> rowY, rowH; int headH = 0, total = 0, width = -1; };
  const Geo& geo(int width) const;
  QFont cellFont(const Cell& c, bool head) const;
  QRect buttonRect(const QRect& cellRect, const Cell& c) const;
  int rowAt(int y) const;
  int headerTop() const;
  TableStyle m_s;
  QList<Cell> m_head;
  QList<Row> m_rows;
  mutable Geo m_geo;
  int m_hover = -1;
  QScrollArea* m_sticky = nullptr;
};

// ---- .tester: grid minmax(330px, 420px) | 1fr, both columns scroll, 10 px 12 px padding, left border ----
struct TwoCol {
  QWidget* root = nullptr;
  QWidget* left = nullptr;          // content widget of the left column (QVBoxLayout, no spacing)
  QWidget* right = nullptr;         // content of the right column (QVBoxLayout, 10 px gap = flex column gap)
  QVBoxLayout* leftLay = nullptr;
  QVBoxLayout* rightLay = nullptr;
  QScrollArea* leftArea = nullptr;
  QScrollArea* rightArea = nullptr;
};
TwoCol twoCol(QWidget* parent = nullptr, bool rightGap = true);
// A scrolling "pad" body (the tab-body itself scrolls): content QVBoxLayout, no margins.
QWidget* scrollBody(QWidget* content, QScrollArea** area = nullptr);

// Margin above an h4 (10 px) as the CSS: insert before each h4 in a column; margin-top:0 for the first.
QLabel* h4(const QString& text, bool firstInColumn = false);
// A block column with CSS vertical margins that collapse between siblings (max of the two).
struct Col {
  QVBoxLayout* lay = nullptr;
  int pending = -1;            // bottom margin of the last block (-1 = none yet)
  void add(QWidget* w, int marginTop = 0, int marginBottom = 0, int stretch = 0);
  void finish(bool stretch = true);
};
// CSS margins of the blocks the tabs use.
inline constexpr int kH4Top = 10, kH4Bottom = 4, kPMargin = 12, kRulesTop = 4, kRulesBottom = 10;
QLabel* hint(const QString& html);
// Plain text in the --text colour (the page style sheet colours only classed labels).
void textColor(QLabel* l);

}  // namespace charts::ui::bottom
