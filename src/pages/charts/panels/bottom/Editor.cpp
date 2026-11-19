// Charts tab — Indicator editor. Port of quant-terminal public/js/panels.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Editor.h"
#include "charts/core/Indicators.h"
#include "charts/data/CustomFiles.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/bottom/Common.h"
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

namespace charts::ui::bottom {


const char* EditorTab::TEMPLATE = R"(export default {
  name: 'EMA ribbon squeeze',
  overlay: true, // true = drawn on the price chart, false = its own pane
  inputs: [
    { key: 'fast', label: 'Fast EMA', type: 'int', default: 8 },
    { key: 'slow', label: 'Slow EMA', type: 'int', default: 34 },
  ],
  outputs: [
    { key: 'fast', label: 'Fast', kind: 'line', color: '#26a69a' },
    { key: 'slow', label: 'Slow', kind: 'line', color: '#ef5350' },
  ],
  // bars: [{ time, open, high, low, close, volume }], p: your inputs,
  // ta: sma, ema, rma, wma, hma, rsi, atr, stdev, highest, lowest, crossover, crossunder, source, compute(...)
  calc(bars, p, ta) {
    const close = ta.source(bars, 'close');
    const fast = ta.ema(close, p.fast), slow = ta.ema(close, p.slow);
    const up = ta.crossover(fast, slow);
    return {
      fast,
      slow,
      markers: bars.filter((b, i) => up[i]).map((b) => ({ time: b.time, position: 'belowBar', shape: 'arrowUp', color: '#26a69a', text: 'Cross' })),
    };
  },
};
)";

namespace {
// A Pine Script start (saved as a .pine file, run by PineTS: see charts/core/PineIndicators.h)
const char* const PINE_TEMPLATE = R"(//@version=5
indicator("My Pine indicator", overlay=true)
fastLen = input.int(9, "Fast EMA", minval=1)
slowLen = input.int(21, "Slow EMA", minval=1)
fast = ta.ema(close, fastLen)
slow = ta.ema(close, slowLen)
plot(fast, "Fast", color = fast > fast[1] ? color.teal : color.red, linewidth=2)
plot(slow, "Slow", color=color.orange)
plotshape(ta.crossover(fast, slow), "Cross up", shape.triangleup, location.belowbar, color.teal)
plotshape(ta.crossunder(fast, slow), "Cross down", shape.triangledown, location.abovebar, color.red)
)";
// <textarea>: Tab types two spaces (the JS keydown handler); line-height 1.5.
class CodeEdit : public QPlainTextEdit {
public:
  CodeEdit() {
    setProperty("qtc", "in");
    setFont(style::monoFont(12));
    setTabStopDistance(QFontMetricsF(font()).horizontalAdvance(' ') * 2);   // tab-size: 2
    setLineWrapMode(QPlainTextEdit::WidgetWidth);   // a textarea wraps long lines
    setMinimumHeight(220);
  }
  void setCode(const QString& s) {
    setPlainText(s);
    QTextCursor c(document());
    c.select(QTextCursor::Document);
    QTextBlockFormat f;
    f.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
    c.mergeBlockFormat(f);
  }
protected:
  void keyPressEvent(QKeyEvent* e) override {
    if (e->key() == Qt::Key_Tab && e->modifiers() == Qt::NoModifier) {
      textCursor().insertText("  ");
      return;
    }
    QPlainTextEdit::keyPressEvent(e);
  }
};

QString codeTag(const QString& s) {   // .tester code { background: var(--hover); padding: 1px 4px; font-size: 12px }
  return QString("<code style=\"background:%1;font-size:12px\">&nbsp;%2&nbsp;</code>").arg(style::v().hover.name(), s);
}
}  // namespace

EditorTab::EditorTab(BottomPanel* panel) : QObject(panel), P(panel) {}

void EditorTab::renderList() {
  if (!list_) return;
  while (QLayoutItem* it = listLay_->takeAt(0)) {
    if (it->widget()) it->widget()->deleteLater();
    delete it;
  }
  const auto list = data::listCustom();
  for (const auto& c : list) {
    QFrame* card = style::card();
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(10, 8, 10, 8);
    cl->setSpacing(2);
    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    auto* name = new QLabel(c.file);
    name->setFont(style::font(13));
    textColor(name);
    top->addWidget(name, 1, Qt::AlignTop);
    auto* edit = style::btn(QCoreApplication::translate("charts", "Edit"), "small");
    auto* add = style::btn(QCoreApplication::translate("charts", "Add to chart"), "small");
    auto* del = style::btn(QCoreApplication::translate("charts", "Delete"), "small danger");
    auto* acts = new QHBoxLayout;
    acts->setSpacing(8);
    acts->addWidget(edit);
    acts->addWidget(add);
    acts->addWidget(del);
    top->addLayout(acts);
    cl->addLayout(top);
    if (!c.error.isEmpty()) {
      auto* e = new QLabel(fmt::tx(c.error));
      e->setWordWrap(true);
      e->setFont(style::font(11));
      e->setStyleSheet(QString("color:%1;").arg(style::v().down.name(QColor::HexArgb)));
      cl->addWidget(e);
    }
    const QString file = c.file;
    connect(edit, &QPushButton::clicked, this, [this, file] {
      if (!code_) return;
      static_cast<CodeEdit*>(code_.data())->setCode(data::readCustom(file));
      QString n = file;
      if (n.endsWith(".js")) n.chop(3);
      else if (n.endsWith(".pine")) n.chop(5);
      name_->setText(n.replace('-', ' '));
      edFile = file;
    });
    connect(add, &QPushButton::clicked, this, [this, file] {
      if (const ind::DefPtr def = P->app->importCustom(file))
        if (P->app->active) P->app->active->addIndicator(def->id);
    });
    connect(del, &QPushButton::clicked, this, [this, file] {
      if (!confirm(P->window(), QString(QCoreApplication::translate("charts", "Delete %1?")).arg(file))) return;
      data::deleteCustom(file);
      renderList();   // only the list changed: a full render() would replace the code being typed
    });
    auto* wrap = new QWidget;   // .card { margin: 8px }
    auto* wl = new QVBoxLayout(wrap);
    wl->setContentsMargins(8, 8, 8, 0);
    wl->addWidget(card);
    listLay_->addWidget(wrap);
  }
  if (list.isEmpty()) listLay_->addWidget(hint(QCoreApplication::translate("charts", "None yet.")));
  else listLay_->addSpacing(8);
}

void EditorTab::render() {
  QWidget* body = P->newBody();
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  TwoCol tc = twoCol(body);
  bl->addWidget(tc.root);
  Col left{tc.leftLay};
  auto* nw = style::btn(QCoreApplication::translate("charts", "New indicator"), "primary");
  auto* nwPine = style::btn(QCoreApplication::translate("charts", "New Pine Script"));
  left.add(rowFlex({nw, nwPine}));
  left.add(h4(QCoreApplication::translate("charts", "Your indicators")), kH4Top, kH4Bottom);
  auto* list = new QWidget;
  listLay_ = new QVBoxLayout(list);
  listLay_->setContentsMargins(0, 0, 0, 0);
  listLay_->setSpacing(0);
  list_ = list;
  left.add(list);
  // Deepwater's copies of quant-terminal's docs/custom-indicators.md and docs/indicator-template.js
  left.add(hint(QString(QCoreApplication::translate("charts", "Each indicator is a small JavaScript file in the %1 folder of Deepwater's data folder, in the "
                          "format in %2 (start from %3). Each one is checked before it's saved. You can also ask your "
                          "AI agent: \"add an indicator to Deepwater that…\" and it will write the file for you."))
                    .arg(codeTag("charts/custom"), codeTag("docs/charts/custom-indicators.md"),
                         codeTag("docs/charts/indicator-template.js"))),
           kPMargin, kPMargin);
  left.add(hint(QCoreApplication::translate("charts", "Pine Script works too: paste a TradingView script (version 5 or 6) and save it, it becomes a .pine "
                                            "file in the same folder. Its inputs become the indicator's settings. Not drawn yet: "
                                            "bgcolor, fill, labels, lines, boxes and tables.")),
           kPMargin, kPMargin);
  left.finish();

  name_ = style::input(QCoreApplication::translate("charts", "Indicator name"));
  setFlex(name_, 1);
  auto* saveB = style::btn(QCoreApplication::translate("charts", "Save"));
  auto* saveAdd = style::btn(QCoreApplication::translate("charts", "Save and add to chart"), "primary");
  auto* row = rowFlex({name_, saveB, saveAdd});
  tc.rightLay->addWidget(row);
  auto* code = new CodeEdit;
  code_ = code;
  tc.rightLay->addWidget(code, 1);
  err_ = new QLabel;
  err_->setWordWrap(true);
  err_->setStyleSheet(QString("color:%1;").arg(style::v().down.name(QColor::HexArgb)));
  err_->setFont(style::font(13));
  tc.rightLay->addWidget(err_);
  // the right column is a flex column: the textarea (flex: 1) fills it
  tc.right->setMinimumHeight(0);

  // Restore what was being typed before this re-render; the template only on the first visit.
  code->setCode(draftCode_.isEmpty() ? QString(TEMPLATE) : draftCode_);
  name_->setText(draftCode_.isEmpty() ? QStringLiteral("EMA ribbon squeeze") : draftName_);
  draftCode_ = code->toPlainText();
  draftName_ = name_->text();
  // Keep the draft on every change (typing, Edit, New): only the widgets go away on a re-render, not the tab.
  connect(code, &QPlainTextEdit::textChanged, this, [this, code] { draftCode_ = code->toPlainText(); });
  connect(name_, &QLineEdit::textChanged, this, [this](const QString& t) { draftName_ = t; });
  connect(nw, &QPushButton::clicked, this, [this] {
    edFile.clear();
    static_cast<CodeEdit*>(code_.data())->setCode(QString(TEMPLATE).replace("'EMA ribbon squeeze'", "'My indicator'"));
    name_->setText("My indicator");
  });
  connect(nwPine, &QPushButton::clicked, this, [this] {
    edFile.clear();
    static_cast<CodeEdit*>(code_.data())->setCode(PINE_TEMPLATE);
    name_->setText("My Pine indicator");
  });
  connect(saveB, &QPushButton::clicked, this, [this] { save(false); });
  connect(saveAdd, &QPushButton::clicked, this, [this] { save(true); });
  renderList();
}

void EditorTab::save(bool add) {
  if (!err_ || !code_) return;
  err_->clear();
  QString name = name_->text().trimmed();
  if (name.isEmpty()) name = "indicator";
  const data::CustomSave r = data::saveCustom(name, code_->toPlainText());
  if (r.status != 200) {
    err_->setText(fmt::tx(r.error));
    return;
  }
  const ind::DefPtr def = ind::byId(r.id);   // saveCustom already loaded and registered it: no second run
  if (!def) {
    err_->setText(QCoreApplication::translate("charts", "Saved, but the browser could not load it. Check the code."));
    return;
  }
  P->app->toast(QString(QCoreApplication::translate("charts", "Saved %1")).arg(fmt::esc(r.file)));
  if (add) {
    if (P->app->active) P->app->active->addIndicator(def->id);
  } else {
    for (ChartPane* p : P->app->panes) p->render(false);
  }
  edFile = r.file;
  renderList();
}

}  // namespace charts::ui::bottom
