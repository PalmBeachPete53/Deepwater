// Charts tab — Seasonals. Port of quant-terminal public/js/research.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/panels/bottom/Seasonals.h"
#include "charts/core/Seasonals.h"
#include "charts/data/Providers.h"
#include "pages/charts/ChartPane.h"
#include "pages/charts/ChartsApp.h"
#include "pages/charts/Format.h"
#include "pages/charts/Style.h"
#include "pages/charts/panels/BottomPanel.h"
#include "pages/charts/panels/bottom/Common.h"
#include "core/I18n.h"
#include "pages/charts/plot/Chart.h"
#include "pages/charts/plot/PriceScale.h"
#include "pages/charts/plot/Series.h"
#include "pages/charts/plot/TimeScale.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>
#include <cmath>

namespace charts::ui::bottom {


static const char* PALETTE[] = {"#2962ff", "#ff9800", "#e91e63", "#00bcd4", "#8bc34a",
                                "#9c27b0", "#795548", "#607d8b", "#cddc39", "#3f51b5"};
static const char* MONTHS[] = {QT_TRANSLATE_NOOP("charts", "Jan"), QT_TRANSLATE_NOOP("charts", "Feb"),
                               QT_TRANSLATE_NOOP("charts", "Mar"), QT_TRANSLATE_NOOP("charts", "Apr"),
                               QT_TRANSLATE_NOOP("charts", "May"), QT_TRANSLATE_NOOP("charts", "Jun"),
                               QT_TRANSLATE_NOOP("charts", "Jul"), QT_TRANSLATE_NOOP("charts", "Aug"),
                               QT_TRANSLATE_NOOP("charts", "Sep"), QT_TRANSLATE_NOOP("charts", "Oct"),
                               QT_TRANSLATE_NOOP("charts", "Nov"), QT_TRANSLATE_NOOP("charts", "Dec")};

namespace {
// .seas grid: minmax(0, 1.3fr) | minmax(0, 1fr); the left column has a right border.
class SeasGrid : public QWidget {
public:
  QWidget* left = nullptr;
  QWidget* right = nullptr;
  using QWidget::QWidget;
protected:
  void resizeEvent(QResizeEvent*) override {
    const int lw = int(std::round(width() * 1.3 / 2.3));
    left->setGeometry(0, 0, lw, height());
    right->setGeometry(lw, 0, width() - lw, height());
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const int lw = int(std::round(width() * 1.3 / 2.3));
    p.fillRect(QRect(lw - 1, 0, 1, height()), style::v().border);
  }
  QSize minimumSizeHint() const override { return {0, left ? left->minimumSizeHint().height() : 0}; }
  QSize sizeHint() const override { return minimumSizeHint(); }
};

// .seas-legend span: <i> 10x3 swatch, 5 px gap, then the text
QWidget* legendItem(const QColor& c, const QString& html) {
  auto* w = new QWidget;
  auto* l = new QHBoxLayout(w);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(5);
  auto* sw = new QFrame;
  sw->setFixedSize(10, 3);
  sw->setStyleSheet(QString("background:%1;border:none;").arg(c.name(QColor::HexArgb)));
  auto* t = new QLabel(html);
  t->setFont(style::font(12));
  textColor(t);
  l->addWidget(sw, 0, Qt::AlignVCenter);
  l->addWidget(t);
  return w;
}

QString md(qint64 t) {   // toLocaleDateString(undefined, { month: "short", day: "numeric", timeZone: "UTC" })
  const QDate d = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date();
  if (i18n::current() == i18n::Lang::French)   // "14 janv."
    return QString("%1 %2").arg(d.day()).arg(QCoreApplication::translate("charts", MONTHS[d.month() - 1]));
  return QString("%1 %2").arg(MONTHS[d.month() - 1]).arg(d.day());
}

Cell heatCell(double v) {
  if (std::isnan(v)) return Cell{QStringLiteral("·"), "muted"};
  const double a = std::min(1.0, std::fabs(v) / 15);
  QColor bg = v >= 0 ? style::v().up : style::v().down;
  bg.setAlphaF(0.12 + a * 0.55);
  Cell c{(v >= 0 ? "+" : "") + jsFixed(v, 1)};
  c.bg = bg;
  return c;
}
}  // namespace

SeasonalsTab::SeasonalsTab(BottomPanel* panel) : QObject(panel), P(panel) {}

void SeasonalsTab::render(int showYears) {
  years_ = 6;   // opts.years || 6: a later render without options starts at 6 again
  const QString sym = P->app->active ? P->app->active->cfg.value("symbol").toString() : QString();
  QWidget* body = P->newBody();
  auto* bl = new QVBoxLayout(body);
  bl->setContentsMargins(0, 0, 0, 0);
  auto* grid = new SeasGrid;
  bl->addWidget(scrollBody(grid));

  auto* left = new QWidget(grid);
  grid->left = left;
  auto* ll = new QVBoxLayout(left);
  ll->setContentsMargins(0, 0, 1, 0);
  ll->setSpacing(0);
  auto* head = new QWidget;
  auto* hl = new QVBoxLayout(head);
  hl->setContentsMargins(12, 10, 12, 4);
  auto* title = new QLabel("<b>" + fmt::esc(ticker(sym)) + " " + QCoreApplication::translate("charts", "by calendar year") + "</b>");
  title->setFont(style::font(13));
  textColor(title);
  // <label class="field" style="flex-direction:row; align-items:center; gap:6px">Years<select>
  auto* yearsBox = new QWidget;
  auto* yl = new QHBoxLayout(yearsBox);
  yl->setContentsMargins(0, 0, 0, 0);
  yl->setSpacing(6);
  auto* ylab = new QLabel(QCoreApplication::translate("charts", "Years"));
  ylab->setProperty("qtc", "field-label");
  QList<Opt> yo;
  for (int n : {3, 6, 10, 20}) yo.append(Opt{QString::number(n), QString::number(n)});
  QComboBox* ys = optSelect(yo, QString::number(showYears));
  yl->addWidget(ylab);
  yl->addWidget(ys);
  auto* note = new QLabel(QCoreApplication::translate("charts", "Loading daily history…"));
  note->setProperty("qtc", "hint");
  hl->addWidget(rowFlex({title, yearsBox, note}));
  ll->addWidget(head);
  auto* chartBox = new QWidget;
  chartBox->setMinimumHeight(160);
  auto* cbl = new QVBoxLayout(chartBox);
  cbl->setContentsMargins(0, 0, 0, 0);
  ll->addWidget(chartBox, 1);
  auto* legend = new QWidget;
  auto* lgl = new FlowLayout(legend, 12, 4);
  lgl->setContentsMargins(12, 6, 12, 6);
  QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
  sp.setHeightForWidth(true);
  legend->setSizePolicy(sp);
  ll->addWidget(legend);

  auto* rightContent = new QWidget;
  auto* rl = new QVBoxLayout(rightContent);
  rl->setContentsMargins(0, 0, 0, 0);
  rl->setSpacing(0);
  QScrollArea* rightArea = style::scrollArea(rightContent, grid);
  rightArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  rightArea->setWidgetResizable(true);
  grid->right = rightArea;

  connect(ys, &QComboBox::activated, this, [this, ys](int) { render(optValue(ys).toInt()); });

  const int seq = ++seq_;
  QPointer<QWidget> pbody(body);
  data::getCandles(sym, "1D", 8000, this, [=, this](const data::Result<data::CandlesResult>& r) {
    if (seq != seq_ || !pbody || P->body() != pbody) return;   // !el.isConnected
    if (!r.ok()) {
      note->setText(fmt::tx(r.error->message));
      return;
    }
    if ((P->app->active ? P->app->active->cfg.value("symbol").toString() : QString()) != sym) return;
    const seasonals::Result s = seasonals::seasonals(r.value.candles);
    note->setText(QString(QCoreApplication::translate("charts", "%1 years of daily bars · average of %2 complete years"))
                      .arg(s.years.size())
                      .arg(s.completeYears));
    const auto& V = style::v();
    auto* chart = new plot::Chart(chartBox);
    chart->applyOptions([&](plot::ChartOptions& o) {
      o.layout.background = V.bg;
      o.layout.textColor = V.text;
      o.layout.fontSize = 10;
      o.grid.vertLines.color = V.grid;
      o.grid.horzLines.color = V.grid;
      o.rightPriceScale.borderColor = V.border;
      o.timeScale.borderColor = V.border;
      o.timeScale.tickMarkFormatter = [](plot::Time t, plot::TickMarkType) { return md(t); };
      o.localization.timeFormatter = [](plot::Time t) { return md(t); };
      o.localization.priceFormatter = [](double p) { return (p >= 0 ? "+" : "") + jsFixed(p, 1) + "%"; };
    });
    cbl->addWidget(chart);
    const qint64 T0 = QDateTime(QDate(2000, 1, 1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();   // leap year
    const int thisYear = QDateTime::currentDateTimeUtc().date().year();
    const int from = std::max(0, int(s.years.size()) - showYears);
    for (int k = 0; from + k < s.years.size(); ++k) {
      const auto& y = s.years[from + k];
      const bool current = y.year == thisYear;
      const QColor color = current ? V.text : QColor(PALETTE[k % 10]);
      auto* line = chart->addSeries(plot::LineSeries, [&](plot::SeriesOptions& o) {
        o.color = color;
        o.lineWidth = current ? 3 : 1;
        o.priceLineVisible = false;
        o.lastValueVisible = false;
        o.crosshairMarkerVisible = false;
      });
      QSet<int> seen;
      QVector<plot::DataPoint> pts;
      for (const auto& p : y.points) {
        if (seen.contains(p.doy)) continue;
        seen.insert(p.doy);
        pts.append(plot::DataPoint::val(T0 + qint64(p.doy - 1) * 86400, p.pct));
      }
      line->setData(pts);
      const double tot = std::isnan(y.total) ? 0 : y.total;
      lgl->addWidget(legendItem(color, QString("%1 <b style=\"color:%2\">%3</b>")
                                           .arg(y.year)
                                           .arg(style::colorOf(tot >= 0 ? "up" : "down").name(), fmtPct(y.total))));
    }
    if (!s.average.isEmpty()) {
      auto* avg = chart->addSeries(plot::LineSeries, [](plot::SeriesOptions& o) {
        o.color = QColor("#9598a1");
        o.lineWidth = 2;
        o.lineStyle = plot::LineStyle::Dashed;
        o.priceLineVisible = false;
        o.lastValueVisible = false;
      });
      QVector<plot::DataPoint> pts;
      for (const auto& p : s.average) pts.append(plot::DataPoint::val(T0 + qint64(p.doy - 1) * 86400, p.pct));
      avg->setData(pts);
      lgl->addWidget(legendItem(QColor("#9598a1"), QString(QCoreApplication::translate("charts", "Average of %1 years")).arg(s.completeYears)));
    }
    chart->timeScale()->fitContent();
    legend->updateGeometry();

    // the monthly table
    auto* table = new Table;
    TableStyle ts;
    ts.font = 11;
    ts.padX = 5;
    ts.padY = 4;
    ts.thPadY = 4;
    ts.center = true;
    table->setStyle(ts);
    QList<Cell> head{{QCoreApplication::translate("charts", "Year")}};
    for (const char* m : MONTHS) head << Cell{QCoreApplication::translate("charts", m)};
    head << Cell{QCoreApplication::translate("charts", "Year")};
    table->setHeader(head);
    QList<Row> rows;
    for (int i = int(s.monthly.size()) - 1; i >= 0; --i) {
      const auto& m = s.monthly[i];
      Row row;
      row.cells << Cell{QString::number(m.year), {}, true, false, true};
      for (double v : m.months) row.cells << heatCell(v);
      row.cells << heatCell(m.total);
      rows << row;
    }
    Row avgRow, upRow;
    avgRow.sum = upRow.sum = true;
    avgRow.cells << Cell{QCoreApplication::translate("charts", "Average"), {}, true};
    upRow.cells << Cell{QCoreApplication::translate("charts", "% of years up"), {}, true};
    for (const auto& a : s.monthAvg) {
      avgRow.cells << heatCell(a.valid ? a.avg : NaN);
      upRow.cells << Cell{a.valid ? QString::number(std::floor(a.upPct + 0.5)) + "%" : QStringLiteral("·")};
    }
    avgRow.cells << Cell{};
    upRow.cells << Cell{};
    rows << avgRow << upRow;
    table->setRows(rows);
    table->setStickyIn(rightArea);
    rl->addWidget(table);
    auto* h = hint(QCoreApplication::translate("charts", "Monthly moves in %. Past seasonality doesn't make next year's move."));
    h->setContentsMargins(12, 10 + 12, 12, 10 + 12);
    rl->addWidget(h);
    rl->addStretch(1);
  });
}

}  // namespace charts::ui::bottom
