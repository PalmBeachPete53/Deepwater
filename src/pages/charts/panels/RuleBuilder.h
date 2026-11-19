#pragma once
// Charts tab — the operand / rule picker shared by the strategy tester, the alert dialog and the screener's
// technical filter. Port of the top of quant-terminal public/js/panels.js (PRICE, OP_LABEL, outputsOf, mainInput,
// operandHtml, readOperand, ruleHtml, bindRules, readRules, compileRules, toOperand, defaultRule)
// (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
//
// API (stable, used by SidePanel and BottomPanel):
//
// UI operands — the JSON the JS keeps in the form (readOperand / operandHtml):
//   { kind: "price", field: "close" }                          field in PRICE
//   { kind: "value", v: 123.4 }
//   { kind: "ind",   id: "ema", params: { length: 50 }, output: "value" }   params: the main input + any others it came with
//   { kind: "line",  line: { t1, p1, t2, p2, extend }, label: "Drawn line" } a drawing (app->lineOperandFor)
// UI rule: { left: <operand>, op: <one of charts::bt::OPS>, right: <operand> }
//
// Compiled (what backtest / alerts read, JS compileRules):
//   compileRules({ rules: [uiRule...] }) -> { indicators: { "ema_50": { id, params } }, rules: [{ left, op, right }] }
//   operands become: number (value), "close" (price), "ema_50.value" (indicator output), { line } (drawn line).
//   Any group names work: { entryLong: [...], exitLong: [...], ... } -> { indicators, entryLong: [...], ... }.
// toOperand(compiledOperand, indicators) goes back (JS strategyToUi / toOperand).
//
// Widgets (JS markup + bindRules behaviour):
//   OperandEdit  <span class="operand">: kind select (+ number input for a value, main-input number and an output
//                select for an indicator). Changing the kind rebuilds the operand like bindRules.
//   RuleRow      <div class="rule">: operand, op select, operand, × (Remove).
//   RuleList     <div class="rules">: a column of RuleRows; × removes the row; changed() on any edit.
#include <QFrame>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QHBoxLayout;
class QVBoxLayout;
class QToolButton;

namespace charts::ind { struct Def; struct Input; }

namespace charts::ui::rules {

// ---- data helpers (pure; same results as the JS) ----
const QStringList& priceFields();                    // JS PRICE: close, open, high, low, volume, hl2, hlc3, ohlc4
const QStringList& ops();                            // charts::bt::OPS in the JS order
QString opLabel(const QString& op);                  // JS OP_LABEL ("crossesAbove" -> "crosses above"); op if unknown
// Display only (English returns the JS text): a price field ("open" -> "ouverture" in French; hl2, hlc3, ohlc4 as
// they are) and an indicator output (French: its translated label; English: the key, as the JS shows it).
QString fieldLabel(const QString& field);
QString outputLabel(const ind::Def& def, const QString& key);
QStringList outputsOf(const ind::Def& def);          // output keys (+ "direction" for supertrend)
QStringList outputsOf(const QString& id);            // empty for an unknown id
const ind::Input* mainInput(const ind::Def& def);    // first "int" / "float" input, nullptr if none
QJsonObject defaultRule();                           // close crossesAbove EMA 50 (tester "+ Condition")
QString kindValue(const QJsonObject& operand);       // the kind select's value: "l", "v", "p:<field>", "i:<id>"
QJsonObject operandForKind(const QString& kindValue);// a fresh operand for a kind select change (bindRules)
QJsonObject compileRules(const QJsonObject& groups); // groups: { name: [uiRule...] } (see above)
QJsonObject compileRules(const QList<QPair<QString, QJsonArray>>& groups);
QJsonObject toOperand(const QJsonValue& x, const QJsonObject& indicators);
// A compiled rule {left, op, right} back to a UI rule (the g() of JS strategyToUi).
QJsonObject toRule(const QJsonObject& compiledRule, const QJsonObject& indicators);
QJsonArray toRules(const QJsonArray& compiledRules, const QJsonObject& indicators);

// ---- widgets ----
class OperandEdit : public QWidget {
  Q_OBJECT
public:
  explicit OperandEdit(const QJsonObject& operand, QWidget* parent = nullptr);
  QJsonObject operand() const;                       // JS readOperand
  void setOperand(const QJsonObject& operand);       // JS operandHtml (rebuilds the extra inputs)
signals:
  void changed();
private:
  void rebuild();
  QJsonObject op_;                                   // the operand the controls were built from
  QHBoxLayout* lay_ = nullptr;
  QComboBox* kind_ = nullptr;
  QLineEdit* v_ = nullptr;      // value: v / indicator: main input
  QComboBox* out_ = nullptr;    // indicator output (when more than one)
  QString paramKey_;
};

class RuleRow : public QFrame {
  Q_OBJECT
public:
  explicit RuleRow(const QJsonObject& rule, QWidget* parent = nullptr);
  QJsonObject rule() const;                          // { left, op, right } (UI operands)
  void setRule(const QJsonObject& rule);
signals:
  void changed();
  void removeRequested();
protected:
  void changeEvent(QEvent* e) override;   // re-applies the style sheet after a theme / light-dark change
private:
  int rev_ = -1;
  OperandEdit* left_ = nullptr;
  OperandEdit* right_ = nullptr;
  QComboBox* op_ = nullptr;
};

class RuleList : public QWidget {
  Q_OBJECT
public:
  explicit RuleList(const QJsonArray& rules = {}, QWidget* parent = nullptr);
  void setRules(const QJsonArray& rules);            // UI rules
  QJsonArray rules() const;                          // JS readRules
  RuleRow* addRule(const QJsonObject& rule);         // insertAdjacentHTML("beforeend", ruleHtml(rule))
  int count() const;
  void clear();
  // false: the rows have no margin below (the screener's inline filter uses style="margin:0").
  void setOuterMargins(bool on);
signals:
  void changed();                                    // bindRules onChange: any edit, a kind change, a removal
private:
  QVBoxLayout* lay_ = nullptr;
  QList<RuleRow*> rows_;
};

// Style sheet of the rule widgets (JS .rules/.rule/.operand CSS on the current theme). Applied by the widgets
// themselves; exposed for hosts that restyle on a theme change.
QString styleSheet();

}  // namespace charts::ui::rules
