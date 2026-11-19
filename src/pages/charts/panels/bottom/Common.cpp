// Bottom panel helpers (see Common.h). Formats ported from quant-terminal public/js/api.js (MIT, Copyright (c) 2026
// Lewis Jackson).
#include "pages/charts/panels/bottom/Common.h"
#include "ui/Kit.h"
#include "pages/charts/Style.h"
#include "pages/charts/Format.h"
#include "charts/data/Store.h"
#include "core/I18n.h"
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QComboBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QWheelEvent>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <cmath>
#include "charts/core/Types.h"
#include "pages/charts/ChartPane.h"
#include <cmath>

namespace charts::ui::bottom {

// ============================== formats ==============================
QString jsFixed(double v, int d) {
  if (!std::isfinite(v)) return charts::store::numberToString(v);
  if (std::fabs(v) >= 1e21) return charts::store::numberToString(v);
  if (v == 0) v = 0;   // -0 prints as "0" in JS
  return QString::number(v, 'f', d);   // (-0.001).toFixed(2) = "-0.00" as in JS
}

QString jsNum(double v) { return charts::store::numberToString(v); }

QString jsPrecision(double v, int p) {
  if (!std::isfinite(v)) return jsNum(v);
  return jsNum(QString::number(v, 'g', p).toDouble());
}

QString localeInt(double v) { return fmt::localeNumber(v, 0, 0); }
QString fmtNum(double v, int d) { return fmt::num(v, d); }
QString fmtPrice(double v) { return fmt::price(v); }
QString fmtPct(double v) { return fmt::pct(v); }
QString ticker(const QString& id) { return fmt::ticker(id); }
QString exchange(const QString& id) { return fmt::exchange(id); }

QString localeDate(qint64 ms) {
  const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms).toLocalTime();
  if (i18n::current() == i18n::Lang::French) return i18n::locale().toString(t.date(), "dd/MM/yyyy");
  return QString("%1/%2/%3").arg(t.date().month()).arg(t.date().day()).arg(t.date().year());
}

QString localeDateTime(qint64 ms) {
  const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms).toLocalTime();
  if (i18n::current() == i18n::Lang::French) return i18n::locale().toString(t, "dd/MM/yyyy HH:mm:ss");
  const int h = t.time().hour();
  const int h12 = h % 12 == 0 ? 12 : h % 12;
  return QString("%1, %2:%3:%4 %5")
      .arg(localeDate(ms))
      .arg(h12)
      .arg(t.time().minute(), 2, 10, QChar('0'))
      .arg(t.time().second(), 2, 10, QChar('0'))
      .arg(h < 12 ? "AM" : "PM");
}

QString esc(const QString& s) { return fmt::esc(s); }

// ============================== form controls ==============================
namespace {
class OptCombo : public QComboBox {
public:
  using QComboBox::QComboBox;
protected:
  void wheelEvent(QWheelEvent* e) override { e->ignore(); }   // a browser select never changes on scroll
};
class OptDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void initStyleOption(QStyleOptionViewItem* o, const QModelIndex& i) const override {
    QStyledItemDelegate::initStyleOption(o, i);
    if (i.data(Qt::UserRole + 1).toBool()) o->font.setBold(true);
  }
  void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& i) const override {
    QStyleOptionViewItem o = opt;
    if (i.data(Qt::UserRole + 2).toBool()) o.rect.adjust(14, 0, 0, 0);
    QStyledItemDelegate::paint(p, o, i);
  }
  QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& i) const override {
    QSize s = QStyledItemDelegate::sizeHint(opt, i);
    if (i.data(Qt::UserRole + 2).toBool()) s.rwidth() += 14;
    return s;
  }
};
}  // namespace

QComboBox* optSelect(const QList<Opt>& opts, const QString& selected, bool classIn) {
  auto* c = new OptCombo;
  if (classIn) {
    c->setProperty("qtc", "in");
    c->setFixedHeight(28);
  }
  c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  auto* model = new QStandardItemModel(c);
  int sel = -1, firstOpt = -1;
  for (const Opt& o : opts) {
    auto* it = new QStandardItem(o.text);
    if (o.group) {
      it->setFlags(Qt::ItemIsEnabled);
      it->setData(true, Qt::UserRole + 1);
    } else {
      it->setData(o.value, Qt::UserRole);
      it->setData(o.child, Qt::UserRole + 2);
      if (firstOpt < 0) firstOpt = model->rowCount();
      if (sel < 0 && o.value == selected) sel = model->rowCount();
    }
    model->appendRow(it);
  }
  c->setModel(model);
  c->setItemDelegate(new OptDelegate(c));
  c->setCurrentIndex(sel >= 0 ? sel : firstOpt);
  return c;
}
QString optValue(const QComboBox* c) { return c ? c->currentData(Qt::UserRole).toString() : QString(); }
void setOptValue(QComboBox* c, const QString& value) {
  for (int i = 0; i < c->count(); ++i)
    if (c->itemData(i, Qt::UserRole).toString() == value && (c->model()->flags(c->model()->index(i, 0)) & Qt::ItemIsSelectable)) {
      c->setCurrentIndex(i);
      return;
    }
}

// ---- BarsKey ----
namespace {
constexpr const char* kBarsWatched = "dwBarsWatched";
constexpr const char* kBarsSymbol = "dwBarsSymbol";
constexpr const char* kBarsTf = "dwBarsTf";
void stampBars(ChartPane* p) {
  p->setProperty(kBarsSymbol, p->cfg.value("symbol").toString());
  p->setProperty(kBarsTf, p->cfg.value("tf").toString());
}
}  // namespace

void watchPane(ChartPane* p) {
  if (!p || p->property(kBarsWatched).toBool()) return;
  p->setProperty(kBarsWatched, true);
  if (!p->bars.isEmpty()) stampBars(p);
  // ChartPane::load sets cfg["symbol"] to the provider's answer before it emits loaded(): the cfg then is the bars'
  QObject::connect(p, &ChartPane::loaded, p, [p] { stampBars(p); });
}

BarsKey barsKey(ChartPane* p) {
  if (!p) return {};
  watchPane(p);
  const QVariant s = p->property(kBarsSymbol);
  if (!s.isValid()) return {p->cfg.value("symbol").toString(), p->cfg.value("tf").toString()};   // nothing loaded yet
  return {s.toString(), p->property(kBarsTf).toString()};
}

bool barsStale(ChartPane* p) {
  return p && barsKey(p) != BarsKey{p->cfg.value("symbol").toString(), p->cfg.value("tf").toString()};
}

QLineEdit* numInput(const QString& text, int width) {
  QLineEdit* e = style::input();
  e->setText(text);
  e->setFixedWidth(width > 0 ? width : 140);
  // the browser only lets digits, signs, '.' and an exponent be typed into a number field; ',' too, because a French
  // keyboard types "0,5" (without it the comma was swallowed and "0,5" became 5). inputValue() reads it as '.'.
  static const QRegularExpression typable(QStringLiteral("[0-9eE+\\-.,]*"));
  e->setValidator(new QRegularExpressionValidator(typable, e));
  return e;
}

std::optional<double> inputValue(const QLineEdit* e) {
  // HTML "valid floating-point number": -?(digits[.digits]|.digits)([eE][+-]?digits)?; anything else reads as ""
  static const QRegularExpression valid(QStringLiteral("^-?(\\d+(\\.\\d+)?|\\.\\d+)([eE][+-]?\\d+)?$"));
  // a decimal comma counts as the point (French keyboards); a second separator still makes the text invalid
  const QString t = e ? e->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')) : QString();
  if (t.isEmpty() || !valid.match(t).hasMatch()) return std::nullopt;
  bool ok = false;
  const double v = QLocale::c().toDouble(t, &ok);
  if (!ok || !std::isfinite(v)) return std::nullopt;   // 1e999: the browser reports "" too
  return v;
}

double inputNumber(const QLineEdit* e) { return inputValue(e).value_or(0); }

QString prompt(QWidget* parent, const QString& text, bool* ok) {
  bool k = false;
  const QString v = QInputDialog::getText(parent, QString(), text, QLineEdit::Normal, {}, &k);
  if (ok) *ok = k;
  return k ? v : QString();
}
bool confirm(QWidget* parent, const QString& text) {
  return ::ui::askPlain(parent, QString(), text, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) ==
         QMessageBox::Ok;
}

// ============================== FlowLayout ==============================
// The item's size from its widget (QWidgetItemV2 caches size hints taken before the style sheet polished it).
static QSize hintOf(QLayoutItem* it) {
  if (QWidget* w = it->widget()) {
    QSize s = w->sizeHint().expandedTo(w->minimumSize()).boundedTo(w->maximumSize());
    if (w->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored) s.setWidth(w->minimumWidth());
    return s;
  }
  return it->sizeHint();
}
FlowLayout::FlowLayout(QWidget* parent, int hGap, int vGap) : QLayout(parent), m_h(hGap), m_v(vGap) {
  setContentsMargins(0, 0, 0, 0);
}
FlowLayout::~FlowLayout() {
  while (QLayoutItem* it = takeAt(0)) delete it;
}
void FlowLayout::addItem(QLayoutItem* item) {
  m_items.append(item);
  m_spacer.append(false);
}
void FlowLayout::addSpacer() {
  m_items.append(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  m_spacer.append(true);
}
QLayoutItem* FlowLayout::itemAt(int i) const { return m_items.value(i); }
QLayoutItem* FlowLayout::takeAt(int i) {
  if (i < 0 || i >= m_items.size()) return nullptr;
  m_spacer.removeAt(i);
  return m_items.takeAt(i);
}
int FlowLayout::count() const { return m_items.size(); }
int FlowLayout::heightForWidth(int w) const { return doLayout(QRect(0, 0, w, 0), true); }
QSize FlowLayout::minimumSize() const {
  // Items are clamped to the line width (a long <select> never widens the column): no minimum width.
  int h = 0;
  for (auto* it : m_items) h = std::max(h, it->minimumSize().height());
  const auto m = contentsMargins();
  return QSize(m.left() + m.right(), h + m.top() + m.bottom());
}
QSize FlowLayout::sizeHint() const {
  int w = 0, h = 0, n = 0;
  for (auto* it : m_items) {
    if (it->isEmpty()) continue;
    const QSize s = hintOf(it);
    w += s.width() + (n++ ? m_h : 0);
    h = std::max(h, s.height());
  }
  return {w, h};
}
void FlowLayout::setGeometry(const QRect& r) {
  QLayout::setGeometry(r);
  doLayout(r, false);
}

static bool isEnd(QLayoutItem* it) { return it->widget() && it->widget()->property("alignEnd").toBool(); }
static int growOf(QLayoutItem* it) { return it->widget() ? it->widget()->property("flexGrow").toInt() : 0; }

int FlowLayout::doLayout(const QRect& rect, bool test) const {
  const auto m = contentsMargins();
  const QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
  struct Line { QList<int> idx; int width = 0, height = 0; };
  QList<Line> lines;
  Line cur;
  for (int i = 0; i < m_items.size(); ++i) {
    QLayoutItem* it = m_items[i];
    if (!m_spacer[i] && it->isEmpty()) continue;
    const int w = m_spacer[i] ? 0 : std::min(hintOf(it).width(), std::max(1, r.width()));
    const int add = cur.idx.isEmpty() ? w : cur.width + m_h + w;
    if (!m_noWrap && !cur.idx.isEmpty() && add > r.width() && !m_spacer[i]) {
      lines.append(cur);
      cur = Line();
    }
    cur.width = cur.idx.isEmpty() ? w : cur.width + m_h + w;
    if (!m_spacer[i]) cur.height = std::max(cur.height, hintOf(it).height());
    cur.idx.append(i);
  }
  if (!cur.idx.isEmpty()) lines.append(cur);
  int y = r.y();
  for (int li = 0; li < lines.size(); ++li) {
    const Line& L = lines[li];
    if (!test) {
      int grow = 0;
      for (int i : L.idx) grow += m_spacer[i] ? 1 : growOf(m_items[i]);
      const int free = std::max(0, r.width() - L.width);
      int x = r.x();
      bool first = true;
      for (int i : L.idx) {
        QLayoutItem* it = m_items[i];
        const int g = m_spacer[i] ? 1 : growOf(it);
        int w = m_spacer[i] ? 0 : std::min(hintOf(it).width(), std::max(1, r.width()));
        if (grow > 0 && g > 0) w += free * g / grow;
        if (!first) x += m_h;
        first = false;
        if (m_spacer[i]) { x += w; continue; }
        const int h = std::min(L.height, hintOf(it).height());
        const int yy = isEnd(it) ? y + L.height - h : y + (L.height - h) / 2;
        it->setGeometry(QRect(x, yy, w, h));
        x += w;
      }
    }
    y += L.height + (li + 1 < lines.size() ? m_v : 0);
  }
  return y - r.y() + m.top() + m.bottom();
}

void alignEnd(QWidget* w) { w->setProperty("alignEnd", true); }
void setFlex(QWidget* w, int grow) { w->setProperty("flexGrow", grow); }

QWidget* rowFlex(const QList<QWidget*>& items, QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* f = new FlowLayout(w);
  for (QWidget* x : items) {
    if (!x) f->addSpacer();
    else f->addWidget(x);
  }
  QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
  sp.setHeightForWidth(true);
  w->setSizePolicy(sp);
  return w;
}

// ============================== StatsGrid ==============================
StatsGrid::StatsGrid(int minCol, QWidget* parent) : QWidget(parent), m_min(minCol) {
  QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Fixed);
  sp.setHeightForWidth(true);
  setSizePolicy(sp);
}
void StatsGrid::addStat(const QString& k, const QString& v, const QString& vCls) {
  QFrame* f = style::stat(k, v, this);
  if (!vCls.isEmpty())
    if (auto* vl = f->findChild<QLabel*>("v"))
      vl->setStyleSheet(QString("color:%1;").arg(style::colorOf(vCls).name(QColor::HexArgb)));
  f->show();
  m_cells.append(f);
  place();
  updateGeometry();
}
void StatsGrid::clear() {
  qDeleteAll(m_cells);
  m_cells.clear();
  updateGeometry();
}
static int cellH(const QList<QWidget*>& cells) {
  int h = 0;
  for (auto* c : cells) h = std::max(h, c->sizeHint().height());
  return h;
}
int StatsGrid::heightForWidth(int w) const {
  if (m_cells.isEmpty()) return 0;
  const int cols = std::max(1, (w + 8) / (m_min + 8));
  const int rows = (int(m_cells.size()) + cols - 1) / cols;
  return rows * cellH(m_cells) + (rows - 1) * 8;
}
QSize StatsGrid::sizeHint() const { return {m_min * 3, heightForWidth(width() > 0 ? width() : m_min * 3)}; }
void StatsGrid::resizeEvent(QResizeEvent*) { place(); }
void StatsGrid::place() {
  const int w = width();
  if (w <= 0) return;
  const int cols = std::max(1, (w + 8) / (m_min + 8));
  const double cw = (w - 8.0 * (cols - 1)) / cols;
  const int h = cellH(m_cells);
  for (int i = 0; i < m_cells.size(); ++i) {
    const int c = i % cols, r = i / cols;
    const int x0 = int(std::round(c * (cw + 8))), x1 = int(std::round(c * (cw + 8) + cw));
    m_cells[i]->setGeometry(x0, r * (h + 8), x1 - x0, h);
  }
  const int want = heightForWidth(w);
  if (height() != want) setFixedHeight(want);
}

// ============================== Table ==============================
Table::Table(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed);
  sp.setHeightForWidth(true);
  setSizePolicy(sp);
}
void Table::setStyle(const TableStyle& s) { m_s = s; m_geo.width = -1; updateGeometry(); update(); }
void Table::setHeader(const QList<Cell>& cols) { m_head = cols; m_geo.width = -1; updateGeometry(); update(); }
void Table::setRows(const QList<Row>& rows) {
  m_rows = rows;
  m_hover = -1;
  m_geo.width = -1;
  setFixedHeight(geo(std::max(1, width())).total);
  updateGeometry();
  update();
}
void Table::setStickyIn(QScrollArea* area) {
  m_sticky = area;
  if (area) connect(area->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { update(); });
}

QFont Table::cellFont(const Cell& c, bool head) const {
  QFont f = style::font(m_s.font, head ? QFont::Medium : (c.bold || c.outline ? QFont::Bold : QFont::Normal));
  return f;
}

static const double kLine = 1.4;

const Table::Geo& Table::geo(int width) const {
  if (m_geo.width == width) return m_geo;
  Geo g;
  g.width = width;
  int ncol = int(m_head.size());
  for (const Row& r : m_rows) if (!r.span) ncol = std::max(ncol, int(r.cells.size()));
  QVector<double> mx(ncol, 0), mn(ncol, 0);
  auto measure = [&](const Cell& c, bool head, int col) {
    const QFontMetricsF fm(cellFont(c, head));
    double full, word;
    if (c.button) {
      full = word = fm.horizontalAdvance(c.text) + 18;
    } else {
      full = fm.horizontalAdvance(c.text);
      word = full;
      if (m_s.wrap && !head) {
        word = 0;
        for (const QString& wd : c.text.split(' ', Qt::SkipEmptyParts)) word = std::max(word, fm.horizontalAdvance(wd));
      }
    }
    mx[col] = std::max(mx[col], full + 2 * m_s.padX);
    mn[col] = std::max(mn[col], word + 2 * m_s.padX);
  };
  if (m_s.header)
    for (int c = 0; c < m_head.size(); ++c) measure(m_head[c], true, c);
  for (const Row& r : m_rows)
    if (!r.span)
      for (int c = 0; c < r.cells.size(); ++c) measure(r.cells[c], false, c);
  double smx = 0, smn = 0;
  for (int c = 0; c < ncol; ++c) { smx += mx[c]; smn += mn[c]; }
  QVector<double> w(ncol, 0);
  for (int c = 0; c < ncol; ++c) {
    if (smx <= width && smx > 0) w[c] = mx[c] + (width - smx) * mx[c] / smx;
    else if (smn <= width && smx > smn) w[c] = mn[c] + (mx[c] - mn[c]) * (width - smn) / (smx - smn);
    else w[c] = mn[c];
  }
  g.x.resize(ncol);
  g.w.resize(ncol);
  double x = 0;
  for (int c = 0; c < ncol; ++c) {
    g.x[c] = int(std::round(x));
    x += w[c];
    g.w[c] = int(std::round(x)) - g.x[c];
  }
  const double lh = m_s.font * kLine;
  double y = 0;
  if (m_s.header && !m_head.isEmpty()) {
    g.headH = int(std::round(lh + 2 * m_s.thPadY + 1));
    y = g.headH;
  }
  for (const Row& r : m_rows) {
    double content = lh;
    for (int c = 0; c < r.cells.size(); ++c) {
      const Cell& cell = r.cells[c];
      if (cell.button) content = std::max(content, 27.0);
      else if (m_s.wrap && c < ncol) {
        const int cw = r.span ? width : g.w[c];
        const QFontMetricsF fm(cellFont(cell, false));
        const QRectF br = fm.boundingRect(QRectF(0, 0, std::max(1, cw - 2 * m_s.padX), 1e6), Qt::TextWordWrap, cell.text);
        const int lines = std::max(1, int(std::ceil(br.height() / fm.lineSpacing() - 0.01)));
        content = std::max(content, lines * lh);
      }
    }
    g.rowY.append(int(std::round(y)));
    y += content + 2 * m_s.padY + 1;
    g.rowH.append(int(std::round(y)) - g.rowY.last());
  }
  g.total = int(std::round(y));
  m_geo = g;
  return m_geo;
}

int Table::heightForWidth(int w) const { return geo(w).total; }
QSize Table::sizeHint() const { return {400, geo(std::max(1, width())).total}; }
QSize Table::minimumSizeHint() const { return {0, geo(std::max(1, width())).total}; }

void Table::resizeEvent(QResizeEvent*) {
  const int h = geo(width()).total;
  if (height() != h) setFixedHeight(h);
}

int Table::headerTop() const {
  if (!m_sticky || !m_sticky->viewport()) return 0;
  const int y = -mapTo(m_sticky->viewport(), QPoint(0, 0)).y();
  const Geo& g = geo(width());
  return std::clamp(y, 0, std::max(0, g.total - g.headH));
}

int Table::rowAt(int y) const {
  const Geo& g = geo(width());
  if (y < headerTop() + g.headH && g.headH && y >= headerTop()) return -2;   // header
  for (int i = 0; i < g.rowY.size(); ++i)
    if (y >= g.rowY[i] && y < g.rowY[i] + g.rowH[i]) return i;
  return -1;
}

QRect Table::buttonRect(const QRect& cr, const Cell& c) const {
  const QFontMetrics fm(style::font(12));
  const int w = fm.horizontalAdvance(c.text) + 18;
  QRect r(0, 0, w, 24);
  const bool left = c.left;
  r.moveCenter(cr.center());
  if (!left) r.moveRight(cr.right() - m_s.padX);
  else r.moveLeft(cr.left() + m_s.padX);
  return r;
}

void Table::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  const auto& V = style::v();
  const Geo& g = geo(width());
  const QRect clip = e->rect();
  const int ncol = int(g.x.size());
  auto drawCell = [&](const Cell& c, const QRect& cr, int col, bool head) {
    if (c.bg.isValid()) p.fillRect(cr.adjusted(0, 0, 0, -1), c.bg);
    if (c.button) {
      const QRect br = buttonRect(cr, c);
      p.setPen(V.border);
      p.setBrush(V.panel);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.drawRoundedRect(QRectF(br).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
      p.setRenderHint(QPainter::Antialiasing, false);
      p.setFont(style::font(12));
      p.setPen(c.buttonKind == "danger" ? V.down : V.text);
      p.drawText(br, Qt::AlignCenter, c.text);
      return;
    }
    Qt::Alignment al = Qt::AlignRight;
    if (m_s.center || c.center) al = Qt::AlignHCenter;
    if (col == 0 && !(c.center && !head)) al = Qt::AlignLeft;
    if (c.left) al = Qt::AlignLeft;
    p.setFont(cellFont(c, head));
    QColor col2 = head ? V.muted : (c.cls.isEmpty() ? V.text : style::colorOf(c.cls));
    p.setPen(col2);
    const QRect tr = cr.adjusted(m_s.padX, head ? m_s.thPadY : m_s.padY, -m_s.padX, -(head ? m_s.thPadY : m_s.padY) - 1);
    const int flags = int(al) | (m_s.wrap && !head ? (Qt::TextWordWrap | Qt::AlignVCenter) : Qt::AlignVCenter);
    p.drawText(tr, flags, c.text);
    if (c.outline) {
      p.setPen(QPen(V.gold, 2));
      p.setBrush(Qt::NoBrush);
      p.drawRect(QRectF(cr).adjusted(1, 1, -1, -2));
    }
  };
  for (int i = 0; i < m_rows.size(); ++i) {
    const QRect rr(0, g.rowY[i], width(), g.rowH[i]);
    if (!rr.intersects(clip)) continue;
    const Row& r = m_rows[i];
    if (i == m_hover) p.fillRect(rr, V.hover);
    if (r.span) {
      if (!r.cells.isEmpty()) drawCell(r.cells[0], rr, 0, false);
    } else {
      for (int c = 0; c < r.cells.size() && c < ncol; ++c) {
        Cell cell = r.cells[c];
        if (r.sum) cell.bold = true;
        drawCell(cell, QRect(g.x[c], rr.y(), g.w[c], rr.height()), c, false);
      }
    }
    p.fillRect(QRect(0, rr.bottom(), width(), 1), V.border);
    if (r.sum) p.fillRect(QRect(0, rr.top(), width(), 1), V.border);
  }
  if (m_s.header && !m_head.isEmpty()) {
    const int top = headerTop();
    const QRect hr(0, top, width(), g.headH);
    p.fillRect(hr, V.bg);
    for (int c = 0; c < m_head.size() && c < ncol; ++c)
      drawCell(m_head[c], QRect(g.x[c], top, g.w[c], g.headH), c, true);
    p.fillRect(QRect(0, hr.bottom(), width(), 1), V.border);
  }
}

void Table::mouseMoveEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()));
  const int h = r >= 0 ? r : -1;
  setCursor(r == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
  if (h != m_hover) {
    m_hover = h;
    update();
  }
}
void Table::leaveEvent(QEvent*) {
  if (m_hover != -1) { m_hover = -1; update(); }
}
void Table::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  const int y = int(e->position().y()), x = int(e->position().x());
  const int r = rowAt(y);
  const Geo& g = geo(width());
  if (r == -2) {
    for (int c = 0; c < g.x.size(); ++c)
      if (x >= g.x[c] && x < g.x[c] + g.w[c]) emit headerClicked(c);
    return;
  }
  if (r < 0) return;
  const Row& row = m_rows[r];
  if (!row.span)
    for (int c = 0; c < row.cells.size() && c < g.x.size(); ++c) {
      const Cell& cell = row.cells[c];
      if (!cell.button) continue;
      const QRect br = buttonRect(QRect(g.x[c], g.rowY[r], g.w[c], g.rowH[r]), cell);
      if (br.contains(x, y)) {
        emit buttonClicked(r, c, cell.data);
        return;
      }
    }
  emit rowClicked(r);
}
bool Table::eventFilter(QObject* o, QEvent* e) { return QWidget::eventFilter(o, e); }

// ============================== columns ==============================
namespace {
class TwoColRoot : public QWidget {
public:
  QWidget* leftBox = nullptr;
  QWidget* rightBox = nullptr;
  using QWidget::QWidget;
protected:
  void resizeEvent(QResizeEvent*) override {
    const int lw = std::clamp(width(), 330, 420);
    leftBox->setGeometry(0, 0, lw, height());
    rightBox->setGeometry(lw + 1, 0, std::max(0, width() - lw - 1), height());
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const int lw = std::clamp(width(), 330, 420);
    p.fillRect(QRect(lw, 0, 1, height()), style::v().border);
  }
};
}  // namespace

TwoCol twoCol(QWidget* parent, bool rightGap) {
  TwoCol t;
  auto* root = new TwoColRoot(parent);
  t.root = root;
  t.left = new QWidget;
  t.leftLay = new QVBoxLayout(t.left);
  t.leftLay->setContentsMargins(12, 10, 12, 10);
  t.leftLay->setSpacing(0);
  t.right = new QWidget;
  t.rightLay = new QVBoxLayout(t.right);
  t.rightLay->setContentsMargins(12, 10, 12, 10);
  t.rightLay->setSpacing(rightGap ? 10 : 0);
  t.leftArea = style::scrollArea(t.left, root);
  t.rightArea = style::scrollArea(t.right, root);
  t.leftArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  t.rightArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  root->leftBox = t.leftArea;
  root->rightBox = t.rightArea;
  return t;
}

QWidget* scrollBody(QWidget* content, QScrollArea** area) {
  QScrollArea* a = style::scrollArea(content);
  a->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  if (area) *area = a;
  return a;
}

QLabel* h4(const QString& text, bool) {
  QLabel* l = style::h4(text);
  l->setContentsMargins(0, 0, 0, 0);   // margins come from Col (they collapse with the neighbours' like CSS)
  return l;
}

void Col::add(QWidget* w, int mt, int mb, int stretch) {
  const int gap = pending < 0 ? mt : std::max(pending, mt);
  if (gap > 0) lay->addSpacing(gap);
  lay->addWidget(w, stretch);
  pending = mb;
}
void Col::finish(bool stretch) {
  if (pending > 0) lay->addSpacing(pending);
  pending = 0;
  if (stretch) lay->addStretch(1);
}
QLabel* hint(const QString& html) { return style::hint(html); }
void textColor(QLabel* l) { l->setStyleSheet(QString("color:%1;").arg(style::v().text.name(QColor::HexArgb))); }

}  // namespace charts::ui::bottom
