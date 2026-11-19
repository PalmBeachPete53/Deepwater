// Charts tab — operand / rule picker. Port of quant-terminal public/js/panels.js (top of the file)
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md). See RuleBuilder.h.
#include "pages/charts/panels/RuleBuilder.h"
#include "pages/charts/Style.h"
#include "pages/charts/Format.h"
#include "core/I18n.h"
#include "charts/core/Backtest.h"
#include "charts/core/Indicators.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

namespace charts::ui::rules {

namespace {



}  // namespace

// ============================== data helpers ==============================

const QStringList& priceFields() {
  static const QStringList p = {"close", "open", "high", "low", "volume", "hl2", "hlc3", "ohlc4"};
  return p;
}

const QStringList& ops() { return bt::OPS; }

QString opLabel(const QString& op) {
  if (op == ">") return QCoreApplication::translate("charts", "is above");
  if (op == "<") return QCoreApplication::translate("charts", "is below");
  if (op == ">=") return QCoreApplication::translate("charts", "is at or above");
  if (op == "<=") return QCoreApplication::translate("charts", "is at or below");
  if (op == "crossesAbove") return QCoreApplication::translate("charts", "crosses above");
  if (op == "crossesBelow") return QCoreApplication::translate("charts", "crosses below");
  if (op == "crosses") return QCoreApplication::translate("charts", "crosses");
  return op;
}

QString fieldLabel(const QString& f) {
  static const char* const kFields[][2] = {
      {"open", QT_TRANSLATE_NOOP("charts", "open")},     {"high", QT_TRANSLATE_NOOP("charts", "high")},
      {"low", QT_TRANSLATE_NOOP("charts", "low")},       {"close", QT_TRANSLATE_NOOP("charts", "close")},
      {"volume", QT_TRANSLATE_NOOP("charts", "volume")},
  };
  for (const auto& kv : kFields)
    if (f == QLatin1String(kv[0])) return QCoreApplication::translate("charts", kv[1]);
  return f;
}

QString outputLabel(const ind::Def& def, const QString& key) {
  if (i18n::current() != i18n::Lang::French) return key;
  for (const auto& o : def.outputs)
    if (o.key == key && !o.label.isEmpty()) return fmt::tx(o.label);
  if (key == "direction") return QCoreApplication::translate("charts", "direction");
  return key;
}

QStringList outputsOf(const ind::Def& def) {
  QStringList out;
  for (const auto& o : def.outputs) out << o.key;
  if (def.id == "supertrend") out << "direction";
  return out;
}

QStringList outputsOf(const QString& id) {
  const ind::DefPtr d = ind::byId(id);
  return d ? outputsOf(*d) : QStringList{};
}

const ind::Input* mainInput(const ind::Def& def) {
  for (const auto& i : def.inputs)
    if (i.type == "int" || i.type == "float") return &i;
  return nullptr;
}

QJsonObject defaultRule() {
  return QJsonObject{{"left", QJsonObject{{"kind", "price"}, {"field", "close"}}},
                     {"op", "crossesAbove"},
                     {"right", QJsonObject{{"kind", "ind"}, {"id", "ema"},
                                           {"params", QJsonObject{{"length", 50}}}, {"output", "value"}}}};
}

QString kindValue(const QJsonObject& o) {
  const QString k = o.value("kind").toString();
  if (k == "line") return "l";
  if (k == "value") return "v";
  if (k == "price") return "p:" + o.value("field").toString();
  return "i:" + o.value("id").toString();
}

QJsonObject operandForKind(const QString& k) {
  if (k == "v") return QJsonObject{{"kind", "value"}, {"v", 0}};
  if (k.startsWith("p:")) return QJsonObject{{"kind", "price"}, {"field", k.mid(2)}};
  const QString id = k.mid(2);
  const QStringList outs = outputsOf(id);
  QJsonObject o{{"kind", "ind"}, {"id", id}, {"params", QJsonObject{}}};
  if (!outs.isEmpty()) o.insert("output", outs.first());   // JS: outputsOf(BY_ID[id])[0]
  return o;
}

// Values of a params object in the JS insertion order. QJsonObject sorts its keys, so the order is taken from the
// indicator's inputs (the order the JS forms and examples write them), then any other key alphabetically.
static QStringList paramValuesInOrder(const QString& id, const QJsonObject& params) {
  QStringList keys;
  if (const ind::DefPtr d = ind::byId(id))
    for (const auto& in : d->inputs)
      if (params.contains(in.key)) keys << in.key;
  for (auto it = params.begin(); it != params.end(); ++it)
    if (!keys.contains(it.key())) keys << it.key();
  QStringList vals;
  for (const auto& k : keys) vals << bt::jsString(params.value(k));
  return vals;
}

static QJsonValue compileOperand(const QJsonObject& o, QJsonObject& indicators) {
  const QString kind = o.value("kind").toString();
  if (kind == "line") return QJsonObject{{"line", o.value("line")}};
  if (kind == "value") return bt::jsNum(o.value("v"));
  if (kind == "price") return o.value("field");
  const QString id = o.value("id").toString();
  const QJsonObject params = o.value("params").toObject();
  QString key = id + "_" + paramValuesInOrder(id, params).join("_");
  key.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
  indicators.insert(key, QJsonObject{{"id", id}, {"params", params}});
  return key + "." + o.value("output").toString();
}

QJsonObject compileRules(const QList<QPair<QString, QJsonArray>>& groups) {
  QJsonObject indicators;
  QJsonObject out;
  for (const auto& [name, rules] : groups) {
    QJsonArray arr;
    for (const auto& rv : rules) {
      const QJsonObject r = rv.toObject();
      QJsonObject c;
      c.insert("left", compileOperand(r.value("left").toObject(), indicators));
      c.insert("op", r.value("op"));
      c.insert("right", compileOperand(r.value("right").toObject(), indicators));
      arr.append(c);
    }
    out.insert(name, arr);
  }
  out.insert("indicators", indicators);
  return out;
}

QJsonObject compileRules(const QJsonObject& groups) {
  QList<QPair<QString, QJsonArray>> g;
  for (auto it = groups.begin(); it != groups.end(); ++it) g.append({it.key(), it.value().toArray()});
  return compileRules(g);
}

QJsonObject toOperand(const QJsonValue& x, const QJsonObject& indicators) {
  if (x.isObject() && bt::jsTruthy(x.toObject().value("line")))
    return QJsonObject{{"kind", "line"}, {"line", x.toObject().value("line")}, {"label", "Drawn line"}};
  if (x.isDouble()) return QJsonObject{{"kind", "value"}, {"v", x.toDouble()}};
  const QString s = bt::jsString(x);
  if (x.isString() && priceFields().contains(s)) return QJsonObject{{"kind", "price"}, {"field", s}};
  const QStringList parts = s.split('.');
  const QString k = parts.value(0), out = parts.value(1);
  const QJsonObject si = indicators.value(k).toObject();
  const ind::DefPtr def = indicators.contains(k) ? ind::byId(si.value("id").toString()) : nullptr;
  if (!def) return QJsonObject{{"kind", "value"}, {"v", 0}};
  QString output = out;
  if (output.isEmpty()) output = si.value("output").toString();
  if (output.isEmpty()) output = outputsOf(*def).value(0);
  return QJsonObject{{"kind", "ind"}, {"id", si.value("id")}, {"params", si.value("params").toObject()},
                     {"output", output}};
}

QJsonObject toRule(const QJsonObject& r, const QJsonObject& indicators) {
  return QJsonObject{{"left", toOperand(r.value("left"), indicators)},
                     {"op", r.value("op")},
                     {"right", toOperand(r.value("right"), indicators)}};
}

QJsonArray toRules(const QJsonArray& rules, const QJsonObject& indicators) {
  QJsonArray a;
  for (const auto& r : rules) a.append(toRule(r.toObject(), indicators));
  return a;
}

// ============================== style ==============================

QString styleSheet() {
  const auto& v = style::v();
  const QString bd = v.border.name(QColor::HexArgb), tx = v.text.name(QColor::HexArgb),
                mu = v.muted.name(QColor::HexArgb), pn = v.panel.name(QColor::HexArgb);
  // .rule { background: panel; border: 1px solid border; border-radius: 6px; padding: 5px }
  // .rule select, .rule input { height: 26px; padding: 0 5px } (the rest is the page's select.in / input.in)
  // .rule .x { border: 0; background: transparent; color: muted }
  return QString(R"(
QFrame[qtc="rule"] { background: %1; border: 1px solid %2; border-radius: 6px; }
QComboBox[rule="1"], QLineEdit[rule="1"] { padding: 0 5px; font-family: "%5"; font-size: 13px; }
QComboBox[rule="1"] { padding-right: 20px; }
QPushButton[qtc="rule-x"] { border: 0; background: transparent; color: %3; padding: 1px 6px; }
QPushButton[qtc="rule-x"]:hover { color: %4; }
)")
      .arg(pn, bd, mu, tx, style::font(13).family());
}

namespace {

// A combo box that paints the CSS select chevron (the JS uses the browser's select arrow).
class RuleCombo : public QComboBox {
public:
  explicit RuleCombo(QWidget* parent = nullptr) : QComboBox(parent) {
    setProperty("qtc", "in");
    setProperty("rule", "1");
    setFixedHeight(26);
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    // .rule select { max-width: 150px }; French operators ("passe au-dessus de") need a little more
    setMaximumWidth(i18n::current() == i18n::Lang::French ? 190 : 150);
    setFocusPolicy(Qt::StrongFocus);
    setFont(style::font(13));
    view()->setFont(style::font(13));
  }
protected:
  void wheelEvent(QWheelEvent* e) override { e->ignore(); }   // a browser select never changes on scroll
};

// <optgroup> rows: bold, not selectable; options under a group are indented in the popup.
class GroupDelegate : public QStyledItemDelegate {
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

QLineEdit* numInput(QWidget* parent) {
  auto* e = new QLineEdit(parent);
  e->setProperty("qtc", "in");
  e->setProperty("rule", "1");
  e->setFixedSize(70, 26);   // .rule input.num { width: 70px }
  e->setFont(style::font(13));
  // Digits, signs, an exponent and a decimal point or comma: a French keyboard types "0,5", which the C-locale
  // QDoubleValidator refused (the comma was lost, or read as NaN: a rule never true). numText() reads ',' as '.'.
  static const QRegularExpression typable(QStringLiteral("[0-9eE+\\-.,]*"));
  e->setValidator(new QRegularExpressionValidator(typable, e));
  return e;
}

// Number(text) of a rule number field, with a decimal comma read as the point ("" = 0, invalid = NaN as in JS).
double numText(const QString& t) { return bt::jsNum(QJsonValue(QString(t).replace(QLatin1Char(','), QLatin1Char('.')))); }

// The .rule flex box: items in a row with 4 px gaps, wrapping (flex-wrap: wrap); the × button (last item) is pushed
// to the right end of its line (margin-left: auto); items centred vertically per line (align-items: center).
class FlowLayout : public QLayout {
public:
  explicit FlowLayout(QWidget* parent) : QLayout(parent) { setContentsMargins(5, 5, 5, 5); setSpacing(4); }
  ~FlowLayout() override { while (auto* i = takeAt(0)) delete i; }
  void addItem(QLayoutItem* i) override { items_.append(i); }
  int count() const override { return int(items_.size()); }
  QLayoutItem* itemAt(int i) const override { return items_.value(i); }
  QLayoutItem* takeAt(int i) override { return i >= 0 && i < items_.size() ? items_.takeAt(i) : nullptr; }
  Qt::Orientations expandingDirections() const override { return {}; }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int w) const override { return doLayout(QRect(0, 0, w, 0), true); }
  void setGeometry(const QRect& r) override { QLayout::setGeometry(r); doLayout(r, false); }
  QSize sizeHint() const override {
    // One line when there is room (the width of everything in a row).
    const auto m = contentsMargins();
    int w = 0, h = 0, n = 0;
    for (auto* i : items_) {
      if (i->isEmpty()) continue;
      const QSize s = i->sizeHint();
      w += s.width() + (n++ ? spacing() : 0);
      h = std::max(h, s.height());
    }
    return {w + m.left() + m.right(), h + m.top() + m.bottom()};
  }
  QSize minimumSize() const override {
    const auto m = contentsMargins();
    QSize s;
    for (auto* i : items_) s = s.expandedTo(i->minimumSize());
    return s + QSize(m.left() + m.right(), m.top() + m.bottom());
  }

private:
  int doLayout(const QRect& rect, bool test) const {
    const auto m = contentsMargins();
    const QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    struct Line { QList<QPair<QLayoutItem*, QSize>> items; int h = 0; };
    QList<Line> lines(1);
    int x = 0;
    for (auto* i : items_) {
      if (i->isEmpty()) continue;
      QSize s = i->sizeHint().boundedTo(i->maximumSize()).expandedTo(i->minimumSize());
      s.setWidth(std::min(s.width(), r.width()));
      if (!lines.last().items.isEmpty() && x + spacing() + s.width() > r.width()) {
        lines.append(Line{});
        x = 0;
      }
      x += (lines.last().items.isEmpty() ? 0 : spacing()) + s.width();
      lines.last().items.append({i, s});
      lines.last().h = std::max(lines.last().h, s.height());
    }
    int y = r.y();
    for (int li = 0; li < lines.size(); ++li) {
      const auto& line = lines[li];
      int lx = r.x();
      for (int k = 0; k < line.items.size(); ++k) {
        auto [item, s] = line.items[k];
        const bool last = li == lines.size() - 1 && k == line.items.size() - 1;
        if (last && pushLast) lx = std::max(lx, r.right() + 1 - s.width());
        if (!test) item->setGeometry(QRect(QPoint(lx, y + (line.h - s.height()) / 2), s));
        lx += s.width() + spacing();
      }
      y += line.h + (li + 1 < lines.size() ? spacing() : 0);
    }
    return y - rect.y() + m.bottom();
  }
  QList<QLayoutItem*> items_;
public:
  bool pushLast = true;
};

}  // namespace

// ============================== OperandEdit ==============================

OperandEdit::OperandEdit(const QJsonObject& operand, QWidget* parent) : QWidget(parent) {
  lay_ = new QHBoxLayout(this);
  lay_->setContentsMargins(0, 0, 0, 0);
  lay_->setSpacing(0);   // the JS puts the select and its inputs side by side with no whitespace between them
  setOperand(operand);
}

void OperandEdit::setOperand(const QJsonObject& o) {
  op_ = o;
  rebuild();
}

void OperandEdit::rebuild() {
  while (auto* it = lay_->takeAt(0)) {
    if (auto* w = it->widget()) w->deleteLater();
    delete it;
  }
  v_ = nullptr;
  out_ = nullptr;
  paramKey_.clear();

  const QJsonObject& o = op_;
  const QString kind = o.value("kind").toString();
  const QString val = kindValue(o);

  kind_ = new RuleCombo(this);
  auto* model = new QStandardItemModel(kind_);
  auto addOpt = [&](const QString& text, const QString& value, bool child) {
    auto* it = new QStandardItem(text);
    it->setData(value, Qt::UserRole);
    it->setData(child, Qt::UserRole + 2);
    model->appendRow(it);
  };
  auto addGroup = [&](const QString& label) {
    auto* it = new QStandardItem(label);
    it->setFlags(Qt::ItemIsEnabled);   // not selectable
    it->setData(true, Qt::UserRole + 1);
    model->appendRow(it);
  };
  if (kind == "line")
    addOpt(o.value("label").toString().isEmpty() ? QCoreApplication::translate("charts", "Drawn line") : fmt::tx(o.value("label").toString()),
           "l", false);
  addGroup(QCoreApplication::translate("charts", "Price"));
  for (const auto& f : priceFields()) addOpt(f == "close" ? QCoreApplication::translate("charts", "Price (close)") : fieldLabel(f), "p:" + f, true);
  addOpt(QCoreApplication::translate("charts", "Value"), "v", false);
  addGroup(QCoreApplication::translate("charts", "Indicators"));
  for (const auto& d : ind::all())
    if (!d.outputs.isEmpty() && !d.extra.value("drawnOnly").toBool()) addOpt(fmt::tx(d.name), "i:" + d.id, true);   // drawnOnly: no values
  kind_->setModel(model);
  kind_->setItemDelegate(new GroupDelegate(kind_));
  // JS: the option whose value matches is selected; none (unknown indicator) = the first option.
  int sel = -1;
  for (int r = 0; r < model->rowCount(); ++r)
    if (model->item(r)->data(Qt::UserRole).toString() == val) { sel = r; break; }
  if (sel < 0)
    for (int r = 0; r < model->rowCount(); ++r)
      if (model->item(r)->flags() & Qt::ItemIsSelectable) { sel = r; break; }
  kind_->setCurrentIndex(sel);
  lay_->addWidget(kind_);
  connect(kind_, &QComboBox::activated, this, [this](int i) {
    const QString k = kind_->itemData(i, Qt::UserRole).toString();
    if (k == kindValue(op_)) return;   // the browser fires no change event for the same option
    if (k == "l") return;
    op_ = operandForKind(k);           // bindRules: span.outerHTML = operandHtml(o)
    QMetaObject::invokeMethod(this, [this] { rebuild(); emit changed(); }, Qt::QueuedConnection);
  });

  if (kind == "value") {
    v_ = numInput(this);
    v_->setText(o.contains("v") && !o.value("v").isNull() ? bt::jsString(o.value("v")) : "0");
    lay_->addWidget(v_);
  }
  if (kind == "ind") {
    const ind::DefPtr def = ind::byId(o.value("id").toString());
    const ind::Input* mi = def ? mainInput(*def) : nullptr;
    if (mi) {
      v_ = numInput(this);
      paramKey_ = mi->key;
      v_->setToolTip(fmt::tx(mi->label));
      const QJsonValue pv = o.value("params").toObject().value(mi->key);
      v_->setText(pv.isUndefined() || pv.isNull() ? bt::jsString(mi->def) : bt::jsString(pv));
      lay_->addWidget(v_);
    }
    const QStringList outs = def ? outputsOf(*def) : QStringList{};
    if (outs.size() > 1) {
      out_ = new RuleCombo(this);
      for (const QString& k : outs) out_->addItem(outputLabel(*def, k), k);   // shown: label (French) or key
      const int i = outs.indexOf(o.value("output").toString());
      out_->setCurrentIndex(i < 0 ? 0 : i);
      lay_->addWidget(out_);
      connect(out_, &QComboBox::activated, this, [this] { emit changed(); });
    }
  }
  if (v_) connect(v_, &QLineEdit::editingFinished, this, [this] { emit changed(); });
  updateGeometry();
}

QJsonObject OperandEdit::operand() const {
  const QString k = kind_ ? kind_->currentData(Qt::UserRole).toString() : kindValue(op_);
  if (k == "l")
    return QJsonObject{{"kind", "line"}, {"line", op_.value("line")},
                       {"label", op_.value("label").toString().isEmpty() ? QString("Drawn line") : op_.value("label").toString()}};
  if (k == "v") {
    const QString t = v_ ? v_->text() : QString();
    return QJsonObject{{"kind", "value"}, {"v", t.isEmpty() ? 0.0 : numText(t)}};
  }
  if (k.startsWith("p:")) return QJsonObject{{"kind", "price"}, {"field", k.mid(2)}};
  const QString id = k.mid(2);
  // Start from the operand's own params: only the main input has a field here, and the other ones (Bollinger mult,
  // MACD slow / signal…) of a strategy loaded from JSON must run and be saved as they were, not at their defaults.
  // A kind change replaced op_ with a fresh operand of the new id (operandForKind), so they never cross indicators.
  QJsonObject params = op_.value("kind").toString() == "ind" && op_.value("id").toString() == id
                           ? op_.value("params").toObject() : QJsonObject{};
  if (v_ && !paramKey_.isEmpty()) params.insert(paramKey_, numText(v_->text()));
  QString output = out_ ? out_->currentData().toString() : QString();
  if (output.isEmpty()) output = outputsOf(id).value(0);
  QJsonObject o{{"kind", "ind"}, {"id", id}, {"params", params}};
  if (!output.isEmpty()) o.insert("output", output);
  return o;
}

// ============================== RuleRow ==============================

RuleRow::RuleRow(const QJsonObject& rule, QWidget* parent) : QFrame(parent) {
  setProperty("qtc", "rule");
  rev_ = style::revision();
  setStyleSheet(styleSheet());
  setAttribute(Qt::WA_StyledBackground);
  auto* flow = new FlowLayout(this);
  left_ = new OperandEdit(rule.value("left").toObject(), this);
  op_ = new RuleCombo(this);
  for (const auto& o : ops()) op_->addItem(opLabel(o), o);
  right_ = new OperandEdit(rule.value("right").toObject(), this);
  auto* x = new QPushButton(QString::fromUtf8("×"), this);
  x->setProperty("qtc", "rule-x");
  x->setToolTip(QCoreApplication::translate("charts", "Remove"));
  x->setCursor(Qt::PointingHandCursor);
  x->setFont(style::font(13));
  flow->addWidget(left_);
  flow->addWidget(op_);
  flow->addWidget(right_);
  flow->addWidget(x);
  // Not setRule(rule): the two OperandEdits were just built from this rule, and setRule() rebuilt each of them (a
  // combo with every indicator) a second time for every row of every strategy shown. Only the operator is left.
  const int i = ops().indexOf(rule.value("op").toString());
  op_->setCurrentIndex(i < 0 ? 0 : i);
  connect(left_, &OperandEdit::changed, this, [this] { updateGeometry(); emit changed(); });
  connect(right_, &OperandEdit::changed, this, [this] { updateGeometry(); emit changed(); });
  connect(op_, &QComboBox::activated, this, &RuleRow::changed);
  connect(x, &QPushButton::clicked, this, &RuleRow::removeRequested);
}

void RuleRow::changeEvent(QEvent* e) {
  QFrame::changeEvent(e);
  if ((e->type() == QEvent::StyleChange || e->type() == QEvent::PaletteChange) && rev_ != style::revision()) {
    rev_ = style::revision();
    setStyleSheet(styleSheet());
  }
}

void RuleRow::setRule(const QJsonObject& r) {
  left_->setOperand(r.value("left").toObject());
  right_->setOperand(r.value("right").toObject());
  const int i = ops().indexOf(r.value("op").toString());
  op_->setCurrentIndex(i < 0 ? 0 : i);
}

QJsonObject RuleRow::rule() const {
  return QJsonObject{{"left", left_->operand()}, {"op", op_->currentData().toString()}, {"right", right_->operand()}};
}

// ============================== RuleList ==============================

RuleList::RuleList(const QJsonArray& rules, QWidget* parent) : QWidget(parent) {
  lay_ = new QVBoxLayout(this);
  lay_->setContentsMargins(0, 4, 0, 10);   // .rules { gap: 6px; margin: 4px 0 10px }
  lay_->setSpacing(6);
  setRules(rules);
}

void RuleList::setOuterMargins(bool on) { lay_->setContentsMargins(0, on ? 4 : 0, 0, on ? 10 : 0); }

void RuleList::clear() {
  for (auto* r : rows_) r->deleteLater();
  rows_.clear();
}

void RuleList::setRules(const QJsonArray& rules) {
  clear();
  for (const auto& r : rules) addRule(r.toObject());
}

RuleRow* RuleList::addRule(const QJsonObject& rule) {
  auto* row = new RuleRow(rule, this);
  rows_.append(row);
  lay_->addWidget(row);
  connect(row, &RuleRow::changed, this, &RuleList::changed);
  connect(row, &RuleRow::removeRequested, this, [this, row] {
    rows_.removeAll(row);
    row->hide();
    row->deleteLater();
    emit changed();
  });
  return row;
}

QJsonArray RuleList::rules() const {
  QJsonArray a;
  for (auto* r : rows_) a.append(r->rule());
  return a;
}

int RuleList::count() const { return int(rows_.size()); }

}  // namespace charts::ui::rules
