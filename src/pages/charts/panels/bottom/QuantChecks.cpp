// Charts tab — quant checks box. Port of quant-terminal public/js/quantui.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/QuantChecks.h"
#include "charts/core/PineQuant.h"
#include "charts/data/CustomFiles.h"
#include "charts/core/Quant.h"
#include "charts/core/Indicators.h"
#include "charts/data/Store.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/bottom/Common.h"
#include "core/I18n.h"
#include <atomic>
#include <QDateTime>
#include <QFrame>
#include <QFutureWatcher>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>

namespace charts::ui::bottom {

static QString n2s(double v) { return charts::store::numberToString(v); }

// A tunable's label is built in Quant.cpp as the indicator name without its " (…)" + " " + the input label in lower
// case ("Exponential Moving Average length"): rebuilt here from the translated parts in French.
static QString tunableLabel(const QString& label) {
  if (i18n::current() != i18n::Lang::French) return label;
  static const QRegularExpression paren(QStringLiteral(" \\(.*\\)\\z"));
  for (const ind::Def& def : ind::all()) {
    QString name = def.name;
    name.remove(paren);
    if (!label.startsWith(name + QLatin1Char(' '))) continue;
    for (const ind::Input& inp : def.inputs) {
      if (inp.type != "int" || name + QLatin1Char(' ') + inp.label.toLower() != label) continue;
      QString fr = fmt::tx(def.name);
      fr.remove(paren);
      return fr + QStringLiteral(" · ") + fmt::tx(inp.label).toLower();
    }
  }
  return fmt::tx(label);
}

bool usesCustom(const QJsonObject& strat) {
  const QJsonObject inds = strat.value("indicators").toObject();
  for (auto it = inds.begin(); it != inds.end(); ++it)
    // custom JavaScript indicators run on the GUI thread only; Pine ones run anywhere (better on a worker)
    if (it.value().toObject().value("id").toString().startsWith("custom:")) return true;
  return false;
}

namespace {
// .gold-box: 1.5 px gold border, 8 px radius, panel ground, a 3 px gold-soft ring (box-shadow 0 0 0 3px).
class GoldBox : public QFrame {
public:
  GoldBox() { setContentsMargins(0, 0, 0, 0); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& V = style::v();
    const QRectF r = QRectF(rect()).adjusted(3, 3, -3, -3);
    p.setPen(QPen(V.goldSoft, 3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(-1.5, -1.5, 1.5, 1.5), 9.5, 9.5);
    p.setPen(QPen(V.gold, 1.5));
    p.setBrush(V.panel);
    p.drawRoundedRect(r.adjusted(0.75, 0.75, -0.75, -0.75), 8, 8);
  }
};

QLabel* para(const QString& text, const QString& cls = {}) {
  auto* l = new QLabel(text);
  l->setWordWrap(true);
  l->setTextFormat(Qt::PlainText);
  l->setFont(style::font(13));
  if (!cls.isEmpty()) l->setStyleSheet(QString("color:%1;").arg(style::colorOf(cls).name(QColor::HexArgb)));
  else l->setStyleSheet(QString("color:%1;").arg(style::v().text.name(QColor::HexArgb)));
  return l;
}

QString day(qint64 t) {
  if (!t) return {};
  const QDate d = QDateTime::fromSecsSinceEpoch(t).toLocalTime().date();
  if (i18n::current() == i18n::Lang::French) return i18n::locale().toString(d, "MMM yyyy");
  return QLocale(QLocale::English, QLocale::UnitedStates).toString(d, "MMM yyyy");
}

QColor heat(double n) {
  const double k = std::min(1.0, std::fabs(n) / 100);
  QColor c = n >= 0 ? style::v().up : style::v().down;
  c.setAlphaF(0.12 + 0.55 * k);
  return c;
}

Table* table(bool header, bool center = false) {
  auto* t = new Table;
  TableStyle s;
  s.wrap = true;      // .gold-box table.t td { white-space: normal }
  s.header = header;
  s.center = center;
  t->setStyle(s);
  return t;
}

// render(q) of quantui.js, into `out`.
void renderChecks(QVBoxLayout* out, const quant::Checks& q) {
  Col col{out};
  const int total = int(q.gates.size());
  const QString cls = q.passed == total ? "up" : q.passed >= total / 2.0 ? "warn" : "down";
  const QString verdict =
      q.passed == total ? QCoreApplication::translate("charts", "It holds up on every check here. Paper trade it next before any real money.")
      : q.passed == 0   ? QCoreApplication::translate("charts", "It failed every check. The backtest result is most likely luck or tuned to the past.")
                        : QCoreApplication::translate("charts", "Some checks failed. Part of this result may be luck or tuned to the past.");
  // .ptq-verdict: inline-block, margin 8px 0, padding 4px 10px, radius 4, 600, hover ground
  auto* v = new QLabel(QString(QCoreApplication::translate("charts", "Passed %1 of %2 checks")).arg(q.passed).arg(total));
  v->setFont(style::font(13, QFont::DemiBold));
  v->setStyleSheet(QString("QLabel{color:%1;background:%2;border-radius:4px;padding:4px 10px;}")
                       .arg(style::colorOf(cls).name(QColor::HexArgb), style::v().hover.name(QColor::HexArgb)));
  auto* vrow = new QWidget;
  auto* vl = new QHBoxLayout(vrow);
  vl->setContentsMargins(0, 0, 0, 0);
  vl->addWidget(v);
  vl->addStretch(1);
  col.add(vrow, 8, 0);
  // the inline-block's margin sits in its line box: it adds to the paragraph's margin instead of collapsing
  out->addSpacing(8);
  col.pending = -1;
  col.add(para(verdict), kPMargin, kPMargin);

  auto* gates = table(false);
  QList<Row> gr;
  for (const auto& g : q.gates)
    gr.append(Row{{Cell{fmt::tx(g.label), {}, true, false, true}, Cell{fmt::tx(g.value), "muted", true},
                   Cell{g.pass ? QCoreApplication::translate("charts", "Pass") : QCoreApplication::translate("charts", "Fail"), g.pass ? "up" : "down"}}});
  gates->setRows(gr);
  col.add(gates);

  if (q.walkForward) {
    const auto& wf = *q.walkForward;
    col.add(h4(QCoreApplication::translate("charts", "Walk-forward")), kH4Top, kH4Bottom);
    col.add(hint(QString(QCoreApplication::translate("charts", "For each stretch, the settings that had done best up to then, then how they did on the "
                           "stretch itself. %1 of %2 stretches made money."))
                     .arg(wf.positiveFolds)
                     .arg(wf.folds.size())),
            kPMargin, kPMargin);
    auto* t = table(true);
    t->setHeader({Cell{QCoreApplication::translate("charts", "Stretch"), {}, true}, {QCoreApplication::translate("charts", "Settings picked")}, {QCoreApplication::translate("charts", "Return")}, {QCoreApplication::translate("charts", "Sharpe")}});
    QList<Row> rows;
    for (const auto& f : wf.folds) {
      QStringList chosen;
      for (double c : f.chosen) chosen << n2s(c);
      rows.append(Row{{Cell{QCoreApplication::translate("charts", "%1 to %2").arg(day(f.from), day(f.to)), {}, true},
                       {chosen.isEmpty() ? QCoreApplication::translate("charts", "as set") : chosen.join(" / ")},
                       Cell{fmtPct(f.testReturnPct), f.testReturnPct >= 0 ? "up" : "down"},
                       {jsFixed(f.testSharpe, 2)}}});
    }
    t->setRows(rows);
    col.add(t);
  }

  if (q.tunables.size() == 2) {
    const auto& a = q.tunables[0];
    const auto& b = q.tunables[1];
    col.add(h4(QCoreApplication::translate("charts", "Nearby settings")), kH4Top, kH4Bottom);
    col.add(hint(QCoreApplication::translate("charts", "Net return for each pair of settings. A real edge is a broad patch of green, not one lucky square. "
                   "Outlined: yours.")),
            kPMargin, kPMargin);
    auto* t = table(true, true);
    QList<Cell> head{Cell{tunableLabel(a.label) + " \\ " + tunableLabel(b.label), {}, true}};
    for (double x : b.values) head.append({n2s(x)});
    t->setHeader(head);
    QList<Row> rows;
    for (double va : a.values) {
      Row r;
      r.cells.append(Cell{n2s(va), {}, false, true, true});   // td.l centred by .heat, bold
      for (double vb : b.values) {
        const quant::Variant* v = nullptr;
        for (const auto& x : q.variants)
          if (x.values.size() >= 2 && x.values[0] == va && x.values[1] == vb) { v = &x; break; }
        const double n = v && !std::isnan(v->netPct) ? v->netPct : 0;
        Cell c{fmtPct(n)};
        c.center = true;
        c.bg = heat(n);
        c.outline = v && v->isBase;
        r.cells.append(c);
      }
      rows.append(r);
    }
    t->setRows(rows);
    col.add(t);
  } else if (q.tunables.size() == 1) {
    const auto& a = q.tunables[0];
    col.add(h4(QCoreApplication::translate("charts", "Nearby settings")), kH4Top, kH4Bottom);
    auto* t = table(true);
    QList<Cell> head{Cell{tunableLabel(a.label), {}, true}};
    Row r;
    r.cells.append(Cell{QCoreApplication::translate("charts", "Net return"), {}, true});
    for (const auto& v : q.variants) {
      head.append({n2s(v.values.value(0)) + (v.isBase ? QCoreApplication::translate("charts", " (yours)") : QString())});
      r.cells.append(Cell{fmtPct(v.netPct), v.netPct >= 0 ? "up" : "down"});
    }
    t->setHeader(head);
    t->setRows({r});
    col.add(t);
  }

  if (q.monteCarlo) {
    const auto& mc = *q.monteCarlo;
    col.add(h4(QCoreApplication::translate("charts", "Monte Carlo")), kH4Top, kH4Bottom);
    col.add(para(QString(QCoreApplication::translate("charts", "Reshuffling these %1 trades %2 times: the worst 5% of runs ended at %3, the middle at %4, "
                           "the best 5% at %5. The usual worst drop was %6%, and 1 run in 20 fell %7% or more. %8% of "
                           "runs ended down."))
                     .arg(q.base.trades)
                     .arg(localeInt(mc.runs))
                     .arg(fmtPct(mc.p5), fmtPct(mc.p50), fmtPct(mc.p95), jsFixed(mc.ddP50, 1), jsFixed(mc.ddP95, 1),
                          jsFixed(mc.lossProb, 0))),
            kPMargin, kPMargin);
  }
  if (q.tunables.isEmpty())
    col.add(hint(QCoreApplication::translate("charts", "Walk-forward, overfitting and nearby settings need a whole-number setting to vary, like an "
                   "indicator's length, so they don't run on fixed rules or an imported signal.")),
            kPMargin, kPMargin);
  col.add(hint(QString(QCoreApplication::translate("charts", "Probabilistic Sharpe %1; the best Sharpe %2 random versions would be expected to show is %3. "
                         "These checks use past prices only: passing them doesn't mean a strategy will make money."))
                   .arg(std::isnan(q.psr) ? QCoreApplication::translate("charts", "n/a") : jsFixed(q.psr, 3))
                   .arg(q.trials)
                   .arg(jsFixed(q.sr0, 2))),
          kPMargin, kPMargin);
  col.finish(false);
}

struct Outcome {
  quant::Checks q;
  QString error;
};

// The quant checks of the result shown, kept outside the box: the tester re-renders (collapse, another tab, theme)
// rebuild the box, which used to lose the result and leave the run going with nobody to show it; a click on the
// rebuilt box then started a second full computation. One job: only the latest result is ever shown.
struct Job {
  int runId = -1;                               // TestResult::runId the job belongs to
  bool running = false;
  std::optional<Outcome> outcome;
  std::function<void(const Outcome&)> show;     // the current box's display (replaced when the box is rebuilt)
};
Job& job() {
  static Job j;
  return j;
}
}  // namespace

QWidget* mountQuantChecks(const TestResult& result, ChartsApp* app) {
  auto* box = new GoldBox;
  auto* lay = new QVBoxLayout(box);
  lay->setContentsMargins(3 + 2 + 12, 3 + 2 + 10, 3 + 2 + 12, 3 + 2 + 10);
  lay->setSpacing(0);
  Col col{lay};
  // .gold-head: badge + title
  auto* badge = new QLabel(QCoreApplication::translate("charts", "Bonus"));
  QFont bf = style::font(11, QFont::Bold);
  bf.setLetterSpacing(QFont::AbsoluteSpacing, 0.33);
  badge->setFont(bf);
  badge->setStyleSheet(QString("QLabel{background:%1;color:%2;border-radius:3px;padding:1px 7px;}")
                           .arg(style::v().gold.name(QColor::HexArgb), style::v().onGold.name(QColor::HexArgb)));
  auto* title = new QLabel("<b>" + QCoreApplication::translate("charts", "Quant checks") + "</b>");
  title->setFont(style::font(13));
  textColor(title);
  auto* head = new QWidget;
  auto* hl = new QHBoxLayout(head);
  hl->setContentsMargins(0, 0, 0, 0);
  hl->setSpacing(8);
  hl->addWidget(badge);
  hl->addWidget(title);
  hl->addStretch(1);
  col.add(head, 0, 4);
  col.add(hint(QCoreApplication::translate("charts", "The checks quant desks run before they trust a backtest: walk-forward, overfitting (PBO), deflated "
                 "Sharpe, cost stress, nearby settings, and a Monte Carlo of these trades. It re-runs this strategy "
                 "about 25 times with nearby settings, here on your computer.")),
          kPMargin, kPMargin);
  auto* run = style::btn(QCoreApplication::translate("charts", "Run quant checks"), "gold");
  col.add(rowFlex({run}));
  auto* out = new QWidget;
  auto* outLay = new QVBoxLayout(out);
  outLay->setContentsMargins(0, 0, 0, 0);
  outLay->setSpacing(0);
  col.add(out);
  col.finish(false);

  auto res = std::make_shared<TestResult>(result);
  QPointer<QWidget> pout(out);
  QPointer<QPushButton> pbtn(run);
  auto show = [pout, outLay, pbtn](const Outcome& o) {
    if (!pout) return;
    while (QLayoutItem* it = outLay->takeAt(0)) {
      if (it->widget()) it->widget()->deleteLater();
      delete it;
    }
    if (!o.error.isEmpty()) {
      auto* l = new QLabel(fmt::tx(o.error));
      l->setWordWrap(true);
      l->setStyleSheet(QString("color:%1;").arg(style::v().down.name(QColor::HexArgb)));
      outLay->addSpacing(kPMargin);
      outLay->addWidget(l);
      outLay->addSpacing(kPMargin);
    } else {
      renderChecks(outLay, o.q);
    }
    if (pbtn) {
      pbtn->setEnabled(true);
      pbtn->setText(QCoreApplication::translate("charts", "Run again"));
    }
  };
  // a box rebuilt for the result whose checks run or ran: show where they are
  if (job().runId == result.runId) {
    job().show = show;
    if (job().running) {
      run->setEnabled(false);
      run->setText(QCoreApplication::translate("charts", "Checking…"));
    } else if (job().outcome) {
      show(*job().outcome);
    }
  }
  // the run outlives the box: its timer and watcher hang on the app, not on the box
  QObject* ctx = app ? static_cast<QObject*>(app) : static_cast<QObject*>(box);
  const int runId = result.runId;
  QObject::connect(run, &QPushButton::clicked, box, [ctx, run, res, show, runId] {
    if (job().running && job().runId == runId) return;   // already checking this result
    // a newer run supersedes the previous one: its computation stops at its next backtest instead of running to the end
    static std::shared_ptr<std::atomic<bool>> cancelPrevious;
    if (cancelPrevious) cancelPrevious->store(true);
    cancelPrevious = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = cancelPrevious;   // kept alive by the computation while it runs
    job().runId = runId;
    job().running = true;
    job().outcome.reset();
    job().show = show;
    run->setEnabled(false);
    run->setText(QCoreApplication::translate("charts", "Checking…"));
    const auto compute = [res, cancel]() -> Outcome {
      Outcome o;
      try {
        if (res->strat.contains("pine")) {   // a Pine strategy: its versions run by PineTS (charts/core/PineQuant.h)
          const QString file = res->strat.value("pine").toString();
          const QString source = data::readCustom(file);
          if (source.isEmpty()) { o.error = QCoreApplication::translate("charts", "Couldn't read %1").arg(file); return o; }
          pine::Options po;
          po.symbol = res->symbol;
          po.timeframe = pine::timeframeFromCharts(res->tf);
          const pine::QuantResult pq = pine::quantChecks(source, res->bars, po, 2000, cancel.get());
          if (!pq.ok()) o.error = pq.error;
          else o.q = pq.checks;
          return o;
        }
        quant::Options opt;
        opt.subBars = res->subBars;
        opt.indicatorOrder = res->indicatorOrder;
        o.q = quant::quantChecks(res->bars, res->strat, opt, cancel.get());
      } catch (const std::exception& e) {
        o.error = QString::fromUtf8(e.what());
      }
      return o;
    };
    // kept for the box shown when it ends; dropped when a newer result took the job meanwhile
    const auto done = [runId](const Outcome& o) {
      if (job().runId != runId) return;
      job().running = false;
      job().outcome = o;
      if (job().show) job().show(o);
    };
    QTimer::singleShot(30, ctx, [ctx, res, done, compute] {
      if (usesCustom(res->strat)) {   // custom indicators: GUI thread only
        done(compute());
        return;
      }
      auto* w = new QFutureWatcher<Outcome>(ctx);
      QObject::connect(w, &QFutureWatcherBase::finished, ctx, [w, done] {
        w->deleteLater();
        done(w->result());
      });
      w->setFuture(QtConcurrent::run(compute));
    });
  });
  return box;
}

}  // namespace charts::ui::bottom
