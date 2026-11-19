// AI › ML: build and train machine-learning models on Deepwater's data or a CSV file of the user's.
// Top: the Python environment (ml::Environment: install / state) and the five families of learning. Step 1 of the user's
// plan (2026-11-16): SUPERVISED works; the four other families say which step brings them.
// Supervised: DATA (source, target, features) | MODEL (algorithm, parameters, validation, TRAIN) | RUNS (list, results).
// Test hooks: DW_ML_INSTALL=1 or =torch (installs without asking), DW_ML_FAMILY=<0..4>, DW_ML_LOAD="prices:EURUSD:1d" | "macro:us" | "cot:EUR" | "csv:/path.csv" (loads at once),
// DW_ML_TRAIN="<algorithm>:<target>:<transform>:<horizon>" (after DW_ML_LOAD: trains at once), DW_ML_RUN=<row> (selects a run),
// DW_ML_PICK=<symbol> (switches market through the list after 3 s, tries TRAIN at once, then once loaded; logs ml-pick lines).
#include "app/Pages.h"
#include "core/Settings.h"
#include "macro/Registry.h"
#include "market/Instruments.h"
#include "ml/Catalog.h"
#include "ml/Datasets.h"
#include "technicaldata/Collector.h"
#include "technicaldata/TechnicalDataStore.h"
#include "ml/Environment.h"
#include "ml/Runs.h"
#include "positioning/Cot.h"
#include "pages/ai/Fit.h"
#include "ui/Kit.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>

namespace aipage {
namespace {

// ------------------------------------------------------------------------------------------------ small helpers
QPushButton* button(const QString& text, bool primary, QWidget* parent) {
  auto* b = new QPushButton(text, parent);
  const auto& c = theme::pal();
  b->setCursor(Qt::PointingHandCursor);
  b->setFont(theme::mono(10, QFont::Bold, 1.2));
  b->setStyleSheet(primary
    ? QString("QPushButton { background: %1; color: %2; border: 0; border-radius: 3px; padding: 6px 14px; }"
              "QPushButton:hover { background: %3; } QPushButton:disabled { background: %4; color: %5; }")
          .arg(c.accent.name(), theme::onAccent().name(), theme::mix(c.accent, c.text, 0.2).name(), c.panelAlt.name(), c.textMute.name())
    : QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 3px; padding: 4px 10px; }"
              "QPushButton:hover { color: %3; border-color: %4; } QPushButton:disabled { color: %5; border-color: %2; }")
          .arg(c.textDim.name(), c.border.name(), c.text.name(), c.borderHi.name(), c.textMute.name()));
  return b;
}
QLabel* text(const QString& s, const QFont& f, const QColor& c, QWidget* p, bool wrap = false) {
  auto* l = ui::label(s, f, c, p);
  l->setWordWrap(wrap);
  return l;
}
QLabel* micro(const QString& s, QWidget* p) { return text(s, theme::microLabel(), theme::pal().textMute, p, true); }
QString formQss() {
  const auto& c = theme::pal();
  return QString("QSpinBox, QDoubleSpinBox { background: %1; border: 1px solid %2; border-radius: 3px; padding: 3px 6px; color: %3;"
                 " font-family: 'JetBrains Mono'; font-size: 12px; }"
                 "QSpinBox:focus, QDoubleSpinBox:focus { border-color: %4; }"
                 "QListWidget { background: %1; border: 1px solid %2; border-radius: 3px; color: %3; font-family: 'JetBrains Mono';"
                 " font-size: 11px; outline: 0; }"
                 "QListWidget::item { padding: 3px 4px; } QListWidget::item:selected { background: %5; color: %3; }"
                 "QListWidget::item:disabled { color: %6; }"
                 "QListWidget::indicator { width: 12px; height: 12px; border: 1px solid %6; border-radius: 2px; background: transparent; }"
                 "QListWidget::indicator:checked { border-color: %7; background: %8; }"
                 "QProgressBar { background: %1; border: 1px solid %2; border-radius: 3px; height: 8px; text-align: center; color: transparent; }"
                 "QProgressBar::chunk { background: %4; border-radius: 2px; }")
      .arg(c.panelAlt.name(), c.border.name(), c.text.name(), c.accent.name(), theme::alpha(c.accent, 60).name(QColor::HexArgb), c.textMute.name(),
           c.up.name(), theme::alpha(c.up, 150).name(QColor::HexArgb));
}
QString pct(double v, int dec = 1) { return std::isfinite(v) ? QString::number(v * 100, 'f', dec) + "%" : QStringLiteral("—"); }
QString fixed(double v, int dec = 4) { return std::isfinite(v) ? QString::number(v, 'f', dec) : QStringLiteral("—"); }
double jnum(const QJsonValue& v) { return v.isDouble() ? v.toDouble() : NAN; }
// a return; above +1,000 % as a multiple ("×98.4M"), which stays readable when an agent learned its training rows by heart
QString ret(double v) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  if (v >= 10) return "×" + fmt::compact(1 + v, 1);
  return (v >= 0 ? "+" : "") + QString::number(v * 100, 'f', 1) + "%";
}

// ------------------------------------------------------------------------------------------------ algorithms
// the algorithm catalogue is shared with the "ML" skill of the cubes (ai/MlTools.cpp)
using Param = ml::Param;
using Algo = ml::Algo;
QList<Algo> algosFor(int family) { return ml::algosFor(family); }
Algo algoById(const QString& id) { return ml::algoById(id); }

struct Transform { QString id, label; };
QList<Transform> transforms() {
  return {{"raw", QCoreApplication::translate("ai", "value")}, {"change", QCoreApplication::translate("ai", "change")}, {"pct_change", QCoreApplication::translate("ai", "% change")}, {"direction", QCoreApplication::translate("ai", "direction (up / down)")}};
}

// Group colours: a fixed set of hues (theme colours collapse on light themes); noise / normal rows grey.
QColor groupColor(int label) {
  static const char* hues[] = {"#f2c94c", "#2bd1c4", "#e46a6a", "#7a8cff", "#b37bff", "#6cc96c", "#ff9f43", "#5ec8ff", "#e98ad8", "#c9b37a"};
  if (label < 0) return theme::pal().textMute;
  return QColor(hues[label % 10]);
}
QString groupName(int label, const QString& kind) {
  if (kind == "anomaly") return label ? QCoreApplication::translate("ai", "UNUSUAL") : QCoreApplication::translate("ai", "NORMAL");
  return label < 0 ? QCoreApplication::translate("ai", "NOISE") : QCoreApplication::translate("ai", "GROUP %1").arg(label + 1);
}
struct Assign { QStringList dates; QVector<int> label; QVector<double> score, x, y; };
Assign readAssign(const QString& path) {
  Assign a;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return a;
  QTextStream in(&f);
  in.readLine();
  while (!in.atEnd()) {
    const QStringList c = in.readLine().split(',');
    if (c.size() < 5) continue;
    a.dates << c[0].left(10); a.label << c[1].toInt(); a.score << c[2].toDouble(); a.x << c[3].toDouble(); a.y << c[4].toDouble();
  }
  return a;
}

QString transformLabel(const QString& id) { for (const auto& t : transforms()) if (t.id == id) return t.label; return id; }

// ------------------------------------------------------------------------------------------------ run list
class RunList : public QWidget {
public:
  std::function<void(const QString&)> picked;
  explicit RunList(QWidget* p) : QWidget(p) { setMouseTracking(true); }
  void rebuild() {
    m_ids.clear();
    for (const auto& r : ml::Runs::instance().list()) m_ids << r.id;
    setMinimumHeight(int(m_ids.size()) * kRow + 2);
    updateGeometry(); update();
  }
  void select(const QString& id) { m_sel = id; update(); }
  QString selected() const { return m_sel; }
  QString idAt(int row) const { return m_ids.value(row); }
  QSize sizeHint() const override { return QSize(300, int(m_ids.size()) * kRow + 2); }
protected:
  static constexpr int kRow = 50;
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    if (m_ids.isEmpty()) {
      p.setFont(theme::sans(12)); p.setPen(c.textMute);
      p.drawText(rect().adjusted(4, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, QCoreApplication::translate("ai", "No training yet."));
      return;
    }
    for (int i = 0; i < m_ids.size(); ++i) {
      const ml::Run* r = ml::Runs::instance().find(m_ids[i]);
      if (!r) continue;
      const QRectF row(0, i * kRow + 1, width(), kRow - 4);
      const bool sel = r->id == m_sel, hov = i == m_hover;
      p.setPen(QPen(sel ? c.accent : hov ? c.borderHi : c.border, 1));
      p.setBrush(sel ? theme::alpha(c.accent, 22) : hov ? c.panelAlt : c.panel);
      p.drawRoundedRect(row.adjusted(.5, .5, -.5, -.5), 3, 3);
      // state
      const QColor sc = r->state == ml::RunState::Done ? c.up : r->state == ml::RunState::Running ? c.warn
                      : r->state == ml::RunState::Stopped ? c.textDim : c.down;
      const QString st = r->state == ml::RunState::Running ? QString::number(fmt::roundInt(r->pct)) + "%" : ml::stateName(r->state);
      const QFont sf = theme::mono(9, QFont::Bold, 1.2);
      const qreal sw = QFontMetricsF(sf).horizontalAdvance(st) + 14;
      const QRectF pill(row.right() - 10 - sw, row.top() + 7, sw, 16);
      p.setPen(QPen(theme::alpha(sc, 140), 1)); p.setBrush(theme::alpha(sc, 30)); p.drawRoundedRect(pill, 3, 3);
      p.setFont(sf); p.setPen(sc); p.drawText(pill, Qt::AlignCenter, st);
      p.setFont(theme::mono(11, QFont::Bold)); p.setPen(c.text);
      p.drawText(QRectF(row.left() + 10, row.top() + 5, row.width() - sw - 30, 20), Qt::AlignLeft | Qt::AlignVCenter,
                 QFontMetricsF(p.font()).elidedText(r->name, Qt::ElideRight, row.width() - sw - 30));
      // second line: the main score against the no-skill baseline
      QString line = r->created.toString("dd MMM HH:mm");
      const QJsonObject res = r->result;
      if (!res.isEmpty() && res.value("family").toString() == "reinforcement") {
        const QJsonObject b = res.value("benchmarks").toObject();
        line += "  ·  " + QCoreApplication::translate("ai", "test %1 · buy & hold %2 · beats %3 of random agents")
                              .arg(ret(jnum(res.value("test").toObject().value("total_return"))), ret(jnum(b.value("buy_hold").toObject().value("total_return"))),
                                   pct(jnum(b.value("random").toObject().value("beaten")), 0));
      } else if (!res.isEmpty() && res.value("family").toString() == "self_supervised") {
        const QJsonArray h = res.value("history").toArray();
        if (!h.isEmpty()) line += "  ·  " + QCoreApplication::translate("ai", "loss %1 → %2")
                                              .arg(h.first().toObject().value("train").toDouble(), 0, 'f', 3).arg(h.last().toObject().value("train").toDouble(), 0, 'f', 3);
        const QJsonObject pr = res.value("probe").toObject();
        if (pr.value("task").toString() == "classification")
          line += "  ·  " + QCoreApplication::translate("ai", "probe %1 (raw %2)").arg(pct(jnum(pr.value("embeddings").toObject().value("balanced_accuracy"))),
                                                                              pct(jnum(pr.value("raw_now").toObject().value("balanced_accuracy"))));
      } else if (!res.isEmpty() && res.value("family").toString() == "unsupervised") {
        const QString kind = res.value("kind").toString();
        if (kind == "clustering") {
          int groups = 0; for (const auto& g : res.value("groups").toArray()) if (g.toObject().value("label").toInt() >= 0) ++groups;
          line += "  ·  " + QCoreApplication::translate("ai", "%1 groups").arg(groups);
          const double sil = jnum(res.value("quality").toObject().value("silhouette"));
          if (std::isfinite(sil)) line += "  ·  " + QCoreApplication::translate("ai", "silhouette %1").arg(sil, 0, 'f', 2);
        } else if (kind == "anomaly") line += "  ·  " + QCoreApplication::translate("ai", "%1 unusual rows").arg(pct(jnum(res.value("anomaly_share"))));
        else if (!res.value("components").toArray().isEmpty())
          line += "  ·  " + QCoreApplication::translate("ai", "first axis explains %1").arg(pct(jnum(res.value("components").toArray().first().toObject().value("explained"))));
        else line += "  ·  " + QCoreApplication::translate("ai", "map of %1 rows").arg(res.value("rows").toInt());
      } else if (!res.isEmpty()) {
        const QJsonObject te = res.value("test").toObject(), base = res.value("baseline").toObject();
        if (res.value("task").toString() == "classification")
          line += "  ·  " + QCoreApplication::translate("ai", "balanced acc. %1 (no skill %2)").arg(pct(jnum(te.value("balanced_accuracy"))), pct(jnum(base.value("balanced_accuracy"))));
        else
          line += "  ·  " + QCoreApplication::translate("ai", "MAE %1 (no skill %2)").arg(fixed(jnum(te.value("mae"))), fixed(jnum(base.value("mae"))));
      } else if (!r->error.isEmpty()) line += "  ·  " + r->error.section('\n', 0, 0);
      else if (r->state == ml::RunState::Running) line += "  ·  " + r->stage;
      p.setFont(theme::mono(10)); p.setPen(c.textDim);
      p.drawText(QRectF(row.left() + 10, row.top() + 25, row.width() - 20, 18), Qt::AlignLeft | Qt::AlignVCenter,
                 QFontMetricsF(p.font()).elidedText(line, Qt::ElideRight, row.width() - 20));
    }
  }
  int rowAt(const QPointF& pos) const { const int i = int(pos.y()) / kRow; return i >= 0 && i < m_ids.size() ? i : -1; }
  void mouseMoveEvent(QMouseEvent* e) override { const int h = rowAt(e->position()); if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); } }
  void leaveEvent(QEvent*) override { m_hover = -1; update(); }
  void mousePressEvent(QMouseEvent* e) override {
    const int i = rowAt(e->position());
    if (i < 0) return;
    m_sel = m_ids[i]; update();
    if (picked) picked(m_sel);
  }
private:
  QStringList m_ids;
  QString m_sel;
  int m_hover = -1;
};

// ------------------------------------------------------------------------------------------------ result charts
class ImportanceBars : public QWidget {
public:
  explicit ImportanceBars(QWidget* p) : QWidget(p) {}
  void setItems(const QList<QPair<QString, double>>& items) { m_items = items.mid(0, 12); setFixedHeight(int(m_items.size()) * 18 + 4); update(); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    double mx = 0; for (const auto& it : m_items) mx = qMax(mx, it.second);
    if (mx <= 0) return;
    const int nameW = qMin(170, width() / 3);
    p.setFont(theme::mono(10));
    for (int i = 0; i < m_items.size(); ++i) {
      const qreal y = i * 18 + 2;
      p.setPen(c.textDim);
      p.drawText(QRectF(0, y, nameW - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                 QFontMetricsF(p.font()).elidedText(m_items[i].first, Qt::ElideMiddle, nameW - 6));
      const qreal w = (width() - nameW - 54) * m_items[i].second / mx;
      p.setPen(Qt::NoPen); p.setBrush(theme::alpha(c.accent, 170));
      p.drawRoundedRect(QRectF(nameW, y + 3, qMax<qreal>(1, w), 10), 2, 2);
      p.setPen(c.textMute);
      p.drawText(QRectF(nameW + w + 6, y, 50, 16), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_items[i].second * 100, 'f', 1) + "%");
    }
  }
private:
  QList<QPair<QString, double>> m_items;
};

// Test period: regression → actual vs predicted; classification → rolling hit rate (30 rows) against the no-skill line.
class PredChart : public QWidget {
public:
  explicit PredChart(QWidget* p) : QWidget(p) { setFixedHeight(184); }
  void setData(const QString& csv, bool classification, double baseline) {
    m_a.clear(); m_p.clear(); m_dates.clear(); m_cls = classification; m_base = baseline;
    QFile f(csv);
    if (f.open(QIODevice::ReadOnly)) {
      QTextStream in(&f);
      in.readLine();
      while (!in.atEnd()) {
        const QStringList c = in.readLine().split(',');
        if (c.size() < 3) continue;
        m_dates << c[0].left(10); m_a << c[1].toDouble(); m_p << c[2].toDouble();
      }
    }
    // the lines and their range depend on the data only: once here, not at every paint (the rolling hit rate is 30
    // operations a test row, for up to hundreds of thousands of rows)
    const int n = int(m_a.size());
    m_s1.clear(); m_s2.clear();
    if (m_cls) {
      const int w = qMin(30, n);
      for (int i = 0; i < n; ++i) {
        int hit = 0, cnt = 0;
        for (int k = qMax(0, i - w + 1); k <= i; ++k) { ++cnt; if (fmt::roundInt(m_a[k]) == fmt::roundInt(m_p[k])) ++hit; }
        m_s1 << double(hit) / cnt;
      }
    } else { m_s1 = m_a; m_s2 = m_p; }
    m_lo = INFINITY; m_hi = -INFINITY;
    for (double v : m_s1) { m_lo = qMin(m_lo, v); m_hi = qMax(m_hi, v); }
    for (double v : m_s2) { m_lo = qMin(m_lo, v); m_hi = qMax(m_hi, v); }
    if (m_cls) { m_lo = 0; m_hi = 1; }
    if (m_hi - m_lo < 1e-12) { m_hi += 1; m_lo -= 1; }
    update();
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(44, 20, -8, -20);
    p.setPen(QPen(c.border, 1)); p.drawRect(r);
    const int n = int(m_a.size());
    if (n < 2) return;
    const QVector<double>& s1 = m_s1;
    const QVector<double>& s2 = m_s2;
    const double lo = m_lo, hi = m_hi;
    auto X = [&](int i) { return r.left() + r.width() * i / (n - 1); };
    auto Y = [&](double v) { return r.bottom() - r.height() * (v - lo) / (hi - lo); };
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    for (int k = 0; k <= 2; ++k) {
      const double v = lo + (hi - lo) * k / 2;
      p.drawText(QRectF(0, Y(v) - 8, 40, 16), Qt::AlignRight | Qt::AlignVCenter, m_cls ? pct(v, 0) : QString::number(v, 'g', 4));
    }
    p.drawText(QRectF(r.left(), r.bottom() + 3, 120, 14), Qt::AlignLeft, m_dates.first());
    p.drawText(QRectF(r.right() - 120, r.bottom() + 3, 120, 14), Qt::AlignRight, m_dates.last());
    auto line = [&](const QVector<double>& s, const QColor& col, qreal w) {
      QPainterPath path; path.moveTo(X(0), Y(s[0]));
      for (int i = 1; i < s.size(); ++i) path.lineTo(X(i), Y(s[i]));
      p.setPen(QPen(col, w)); p.setBrush(Qt::NoBrush); p.drawPath(path);
    };
    if (m_cls) {
      if (std::isfinite(m_base)) { p.setPen(QPen(c.textMute, 1, Qt::DashLine)); p.drawLine(QPointF(r.left(), Y(m_base)), QPointF(r.right(), Y(m_base))); }
      line(s1, c.accent, 1.5);
      p.setPen(c.textDim); p.setFont(theme::mono(9));
      p.drawText(QRectF(r.left(), 2, r.width(), 14), Qt::AlignRight,
                 QCoreApplication::translate("ai", "hit rate over the last 30 test rows  ·  dashed: no-skill rate"));
    } else {
      line(s1, c.textDim, 1.2);
      line(s2, c.accent, 1.4);
      p.setFont(theme::mono(9)); p.setPen(c.textDim);
      p.drawText(QRectF(r.left(), 2, r.width(), 14), Qt::AlignRight, QCoreApplication::translate("ai", "grey: actual  ·  amber: predicted"));
    }
  }
private:
  QVector<double> m_a, m_p;
  QVector<double> m_s1, m_s2;   // the lines drawn (setData)
  double m_lo = 0, m_hi = 1;
  QStringList m_dates;
  bool m_cls = false;
  double m_base = NAN;
};

// Each row's group along time: one thin slice per row (regimes read left to right). Anomaly runs: unusual rows in red.
class RegimeStrip : public QWidget {
public:
  explicit RegimeStrip(const Assign& a, bool anomaly, QWidget* p) : QWidget(p), m_a(a), m_anomaly(anomaly) { setFixedHeight(46); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const auto& c = theme::pal();
    const int n = int(m_a.label.size());
    if (n < 2) return;
    const QRectF r(0, 2, width(), 26);
    p.fillRect(r, c.panelAlt);
    for (int i = 0; i < n; ++i) {
      const int lab = m_a.label[i];
      if (m_anomaly && lab == 0) continue;
      const qreal x0 = r.left() + r.width() * i / n, x1 = r.left() + r.width() * (i + 1) / n;
      p.fillRect(QRectF(x0, r.top(), qMax<qreal>(1, x1 - x0), r.height()), m_anomaly ? c.down : groupColor(lab));
    }
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    p.drawText(QRectF(0, r.bottom() + 3, 140, 14), Qt::AlignLeft, m_a.dates.first());
    p.drawText(QRectF(width() - 140, r.bottom() + 3, 140, 14), Qt::AlignRight, m_a.dates.last());
  }
private:
  Assign m_a;
  bool m_anomaly;
};

// Rows on a plane (the first two principal axes, or the t-SNE map): coloured by group, or by date when there is none.
class ScatterMap : public QWidget {
public:
  enum Mode { Groups, Anomaly, Time };
  ScatterMap(const Assign& a, Mode m, QWidget* p) : QWidget(p), m_a(a), m_mode(m) {
    setFixedHeight(280);
    // bounds from the 0.5–99.5 % quantiles: a few extreme rows sit on the edge instead of squeezing all the others
    // finite values only: NaN (the trainer's file can hold some) breaks std::sort's ordering (undefined behaviour).
    // Once here: two sorts of up to a million values at every paint before.
    auto bounds = [](QVector<double> v, double* lo, double* hi) {
      v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return !std::isfinite(x); }), v.end());
      if (v.isEmpty()) { *lo = 0; *hi = 1; return; }
      std::sort(v.begin(), v.end());
      const int m = int(v.size());
      *lo = v[int(m * 0.005)]; *hi = v[qMin(m - 1, int(m * 0.995))];
    };
    bounds(m_a.x, &m_x0, &m_x1); bounds(m_a.y, &m_y0, &m_y1);
    if (m_x1 - m_x0 < 1e-12) m_x1 = m_x0 + 1;
    if (m_y1 - m_y0 < 1e-12) m_y1 = m_y0 + 1;
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(4, 4, -4, -18);
    p.setPen(QPen(c.border, 1)); p.setBrush(Qt::NoBrush); p.drawRect(r);
    const int n = int(m_a.x.size());
    if (n < 2) return;
    const double x0 = m_x0, x1 = m_x1, y0 = m_y0, y1 = m_y1;
    const QRectF in = r.adjusted(8, 8, -8, -8);
    auto P = [&](int i) { return QPointF(in.left() + in.width() * qBound(0.0, (m_a.x[i] - x0) / (x1 - x0), 1.0),
                                         in.bottom() - in.height() * qBound(0.0, (m_a.y[i] - y0) / (y1 - y0), 1.0)); };
    p.setPen(Qt::NoPen);
    const int step = qMax(1, n / 8000);
    // normal rows first, unusual ones on top
    for (int pass = 0; pass < 2; ++pass)
      for (int i = 0; i < n; i += step) {
        const bool top = m_mode == Anomaly ? m_a.label[i] == 1 : m_a.label[i] < 0;
        if ((pass == 1) != top) continue;
        QColor col = m_mode == Time ? theme::mix(c.textMute, c.accent, double(i) / (n - 1))
                   : m_mode == Anomaly ? (m_a.label[i] ? c.down : theme::alpha(c.textDim, 120)) : groupColor(m_a.label[i]);
        if (m_mode == Groups) col.setAlpha(170);
        p.setBrush(col);
        const qreal rad = m_mode == Anomaly && m_a.label[i] ? 2.6 : 1.6;
        p.drawEllipse(P(i), rad, rad);
      }
    // the latest row, ringed
    p.setBrush(Qt::NoBrush); p.setPen(QPen(c.text, 1.4)); p.drawEllipse(P(n - 1), 5, 5);
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    p.drawText(QRectF(r.left(), r.bottom() + 3, r.width(), 14), Qt::AlignLeft,
               m_mode == Time ? QCoreApplication::translate("ai", "colour: from the oldest rows (grey) to the latest (amber)  ·  ring: latest row")
                              : QCoreApplication::translate("ai", "each dot is a row  ·  ring: latest row"));
  }
private:
  Assign m_a;
  Mode m_mode;
  double m_x0 = 0, m_x1 = 1, m_y0 = 0, m_y1 = 1;   // the plane's bounds (constructor)
};

// Unusualness score along time, unusual rows marked.
class ScoreLine : public QWidget {
public:
  ScoreLine(const Assign& a, QWidget* p) : QWidget(p), m_a(a) { setFixedHeight(130); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(2, 4, -2, -18);
    p.setPen(QPen(c.border, 1)); p.drawRect(r);
    const int n = int(m_a.score.size());
    if (n < 2) return;
    double lo = INFINITY, hi = -INFINITY;
    for (double v : m_a.score) { lo = qMin(lo, v); hi = qMax(hi, v); }
    if (hi - lo < 1e-12) hi = lo + 1;
    auto X = [&](int i) { return r.left() + r.width() * i / (n - 1); };
    auto Y = [&](double v) { return r.bottom() - (r.height() - 4) * (v - lo) / (hi - lo); };
    QPainterPath path; path.moveTo(X(0), Y(m_a.score[0]));
    for (int i = 1; i < n; ++i) path.lineTo(X(i), Y(m_a.score[i]));
    p.setPen(QPen(theme::alpha(c.textDim, 160), 1)); p.drawPath(path);
    p.setPen(Qt::NoPen); p.setBrush(c.down);
    for (int i = 0; i < n; ++i) if (m_a.label[i]) p.drawEllipse(QPointF(X(i), Y(m_a.score[i])), 2.2, 2.2);
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    p.drawText(QRectF(r.left(), r.bottom() + 3, 140, 14), Qt::AlignLeft, m_a.dates.first());
    p.drawText(QRectF(r.right() - 140, r.bottom() + 3, 140, 14), Qt::AlignRight, m_a.dates.last());
  }
private:
  Assign m_a;
};

// Self-supervised training: pretext loss per epoch, on the training windows (amber) and the later held-out ones (grey).
class LossChart : public QWidget {
public:
  LossChart(const QJsonArray& h, QWidget* p) : QWidget(p) {
    for (const auto& v : h) { m_t << v.toObject().value("train").toDouble(); m_v << (v.toObject().value("valid").isDouble() ? v.toObject().value("valid").toDouble() : NAN); }
    setFixedHeight(130);
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(44, 18, -8, -18);
    p.setPen(QPen(c.border, 1)); p.drawRect(r);
    const int n = int(m_t.size());
    if (n < 2) return;
    double lo = INFINITY, hi = -INFINITY;
    for (double v : m_t) { lo = qMin(lo, v); hi = qMax(hi, v); }
    for (double v : m_v) if (std::isfinite(v)) { lo = qMin(lo, v); hi = qMax(hi, v); }
    if (hi - lo < 1e-12) hi = lo + 1;
    auto X = [&](int i) { return r.left() + r.width() * i / (n - 1); };
    auto Y = [&](double v) { return r.bottom() - r.height() * (v - lo) / (hi - lo); };
    auto line = [&](const QVector<double>& s, const QColor& col) {
      QPainterPath path; bool started = false;
      for (int i = 0; i < s.size(); ++i) {
        if (!std::isfinite(s[i])) continue;
        if (!started) { path.moveTo(X(i), Y(s[i])); started = true; } else path.lineTo(X(i), Y(s[i]));
      }
      p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush); p.drawPath(path);
    };
    line(m_v, c.textDim);
    line(m_t, c.accent);
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    p.drawText(QRectF(0, r.top() - 6, 40, 14), Qt::AlignRight, QString::number(hi, 'g', 3));
    p.drawText(QRectF(0, r.bottom() - 8, 40, 14), Qt::AlignRight, QString::number(lo, 'g', 3));
    p.drawText(QRectF(r.left(), r.bottom() + 3, 60, 14), Qt::AlignLeft, "1");
    p.drawText(QRectF(r.right() - 60, r.bottom() + 3, 60, 14), Qt::AlignRight, QString::number(n));
    p.drawText(QRectF(r.left(), 1, r.width(), 14), Qt::AlignRight,
               QCoreApplication::translate("ai", "amber: training windows  ·  grey: later windows, never trained on  ·  per epoch"));
  }
private:
  QVector<double> m_t, m_v;
};

// Reinforcement, test period: the agent's equity (amber) against buy & hold (grey), positions in a strip below.
class EquityChart : public QWidget {
public:
  EquityChart(const QString& csv, QWidget* p) : QWidget(p) {
    QFile f(csv);
    if (f.open(QIODevice::ReadOnly)) {
      QTextStream in(&f); in.readLine();
      while (!in.atEnd()) {
        const QStringList c = in.readLine().split(',');
        if (c.size() < 5) continue;
        m_d << c[0].left(10); m_pos << c[2].toDouble(); m_a << c[3].toDouble(); m_b << c[4].toDouble();
      }
    }
    setFixedHeight(200);
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(46, 18, -8, -40);
    p.setPen(QPen(c.border, 1)); p.drawRect(r);
    const int n = int(m_a.size());
    if (n < 2) return;
    double lo = 1, hi = 1;
    for (int i = 0; i < n; ++i) { lo = qMin(lo, qMin(m_a[i], m_b[i])); hi = qMax(hi, qMax(m_a[i], m_b[i])); }
    if (hi - lo < 1e-9) { hi += 0.01; lo -= 0.01; }
    auto X = [&](int i) { return r.left() + r.width() * i / (n - 1); };
    auto Y = [&](double v) { return r.bottom() - r.height() * (v - lo) / (hi - lo); };
    p.setPen(QPen(c.textMute, 1, Qt::DashLine)); p.drawLine(QPointF(r.left(), Y(1)), QPointF(r.right(), Y(1)));
    auto line = [&](const QVector<double>& s, const QColor& col, qreal w) {
      QPainterPath path; path.moveTo(X(0), Y(s[0]));
      for (int i = 1; i < n; ++i) path.lineTo(X(i), Y(s[i]));
      p.setPen(QPen(col, w)); p.setBrush(Qt::NoBrush); p.drawPath(path);
    };
    line(m_b, c.textDim, 1.2);
    line(m_a, c.accent, 1.6);
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    for (double v : {lo, 1.0, hi}) p.drawText(QRectF(0, Y(v) - 8, 42, 16), Qt::AlignRight | Qt::AlignVCenter, QString("%1%2%").arg(v >= 1 ? "+" : "").arg((v - 1) * 100, 0, 'f', 0));
    // positions: long up (green), short down (red), flat empty
    const QRectF strip(r.left(), r.bottom() + 6, r.width(), 12);
    p.fillRect(strip, c.panelAlt);
    for (int i = 0; i < n; ++i) {
      if (m_pos[i] == 0) continue;
      const qreal x0 = strip.left() + strip.width() * i / n, x1 = strip.left() + strip.width() * (i + 1) / n;
      p.fillRect(QRectF(x0, strip.top(), qMax<qreal>(1, x1 - x0), strip.height()), m_pos[i] > 0 ? c.up : c.down);
    }
    p.drawText(QRectF(r.left(), strip.bottom() + 3, 120, 14), Qt::AlignLeft, m_d.first());
    p.drawText(QRectF(r.right() - 120, strip.bottom() + 3, 120, 14), Qt::AlignRight, m_d.last());
    p.drawText(QRectF(r.left(), 1, r.width(), 14), Qt::AlignRight,
               QCoreApplication::translate("ai", "amber: agent  ·  grey: buy & hold  ·  strip: long (green), short (red), flat (empty)"));
  }
private:
  QStringList m_d;
  QVector<double> m_pos, m_a, m_b;
};

// Reinforcement learning curve: greedy result per episode on the training rows (amber) and on the test rows (grey).
class RlCurve : public QWidget {
public:
  RlCurve(const QJsonArray& h, QWidget* p) : QWidget(p) {
    for (const auto& v : h) { m_tr << v.toObject().value("train_return").toDouble(); m_te << v.toObject().value("test_return").toDouble(); }
    setFixedHeight(130);
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(52, 18, -8, -18);
    p.setPen(QPen(c.border, 1)); p.drawRect(r);
    const int n = int(m_tr.size());
    if (n < 1) return;
    // returns are drawn as log(1 + x) so a huge training gain does not flatten the test line
    auto lg = [](double v) { return std::log(qMax(1e-6, 1 + v)); };
    double lo = 0, hi = 0;
    for (int i = 0; i < n; ++i) { lo = qMin(lo, qMin(lg(m_tr[i]), lg(m_te[i]))); hi = qMax(hi, qMax(lg(m_tr[i]), lg(m_te[i]))); }
    if (hi - lo < 1e-9) { hi += 0.01; lo -= 0.01; }
    auto X = [&](int i) { return n == 1 ? r.center().x() : r.left() + r.width() * i / (n - 1); };
    auto Y = [&](double v) { return r.bottom() - r.height() * (lg(v) - lo) / (hi - lo); };
    p.setPen(QPen(c.textMute, 1, Qt::DashLine)); p.drawLine(QPointF(r.left(), Y(0)), QPointF(r.right(), Y(0)));
    auto line = [&](const QVector<double>& s, const QColor& col) {
      QPainterPath path; path.moveTo(X(0), Y(s[0]));
      for (int i = 1; i < n; ++i) path.lineTo(X(i), Y(s[i]));
      p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush); p.drawPath(path);
      p.setPen(Qt::NoPen); p.setBrush(col); p.drawEllipse(QPointF(X(n - 1), Y(s[n - 1])), 2.5, 2.5);
    };
    line(m_te, c.textDim);
    line(m_tr, c.accent);
    p.setFont(theme::mono(9)); p.setPen(c.textMute);
    auto lbl = [&](double l) { return ret(std::exp(l) - 1); };
    p.drawText(QRectF(0, r.top() - 6, 48, 14), Qt::AlignRight, lbl(hi));
    p.drawText(QRectF(0, r.bottom() - 8, 48, 14), Qt::AlignRight, lbl(lo));
    p.drawText(QRectF(r.left(), r.bottom() + 3, 60, 14), Qt::AlignLeft, "1");
    p.drawText(QRectF(r.right() - 60, r.bottom() + 3, 60, 14), Qt::AlignRight, QString::number(n));
    p.drawText(QRectF(r.left(), 1, r.width(), 14), Qt::AlignRight,
               QCoreApplication::translate("ai", "result per episode  ·  amber: training rows  ·  grey: test rows (never trained on)  ·  log scale"));
  }
private:
  QVector<double> m_tr, m_te;
};

// k-means scan: silhouette for each k (higher = better separated groups), the chosen k marked.
class KScan : public QWidget {
public:
  KScan(const QJsonArray& scan, int chosen, QWidget* p) : QWidget(p), m_chosen(chosen) {
    for (const auto& v : scan) { m_k << v.toObject().value("k").toInt(); m_s << v.toObject().value("silhouette").toDouble(); }
    setFixedHeight(110);
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const QRectF r = QRectF(rect()).adjusted(36, 6, -10, -18);
    p.setPen(QPen(c.border, 1)); p.drawRect(r);
    const int n = int(m_k.size());
    if (n < 2) return;
    double lo = INFINITY, hi = -INFINITY;
    for (double v : m_s) { lo = qMin(lo, v); hi = qMax(hi, v); }
    if (hi - lo < 1e-9) { hi += 0.05; lo -= 0.05; }
    auto X = [&](int i) { return r.left() + r.width() * i / (n - 1); };
    auto Y = [&](double v) { return r.bottom() - r.height() * (v - lo) / (hi - lo); };
    QPainterPath path; path.moveTo(X(0), Y(m_s[0]));
    for (int i = 1; i < n; ++i) path.lineTo(X(i), Y(m_s[i]));
    p.setPen(QPen(c.accent, 1.5)); p.drawPath(path);
    p.setFont(theme::mono(9));
    for (int i = 0; i < n; ++i) {
      const bool sel = m_k[i] == m_chosen;
      p.setPen(Qt::NoPen); p.setBrush(sel ? c.up : c.accent); p.drawEllipse(QPointF(X(i), Y(m_s[i])), sel ? 4 : 2.5, sel ? 4 : 2.5);
      p.setPen(sel ? c.up : c.textMute);
      p.drawText(QRectF(X(i) - 12, r.bottom() + 3, 24, 14), Qt::AlignCenter, QString::number(m_k[i]));
    }
    p.setPen(c.textMute);
    p.drawText(QRectF(0, r.top() - 6, 32, 14), Qt::AlignRight, QString::number(hi, 'f', 2));
    p.drawText(QRectF(0, r.bottom() - 8, 32, 14), Qt::AlignRight, QString::number(lo, 'f', 2));
  }
private:
  QVector<int> m_k;
  QVector<double> m_s;
  int m_chosen;
};

// ------------------------------------------------------------------------------------------------ page
class MlPage : public QWidget {
public:
  explicit MlPage(QWidget* parent) : QWidget(parent) {
    const auto& c = theme::pal();
    setStyleSheet(formQss());
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12); root->setSpacing(10);

    // header + environment
    auto* head = new QHBoxLayout; head->setSpacing(12);
    head->addWidget(text("ML", theme::title(), c.accentText, this));
    head->addWidget(fit::label(QCoreApplication::translate("ai", "machine learning on Deepwater's data or your own files"), theme::mono(11), c.textDim, this));
    head->addStretch(1);
    m_env = text(QString(), theme::mono(10), c.textDim, this);
    m_env->setTextInteractionFlags(Qt::TextSelectableByMouse);
    head->addWidget(m_env);
    m_install = button(QCoreApplication::translate("ai", "INSTALL"), true, this);
    connect(m_install, &QPushButton::clicked, this, [this] {
      auto& env = ml::Environment::instance();
      if (env.installing()) { env.cancelInstall(); return; }
      if (env.status().ready()) {   // the only thing left to install from here is PyTorch
        if (ui::askPlain(this, QCoreApplication::translate("ai", "Install PyTorch"),
                QCoreApplication::translate("ai", "Deepwater will add PyTorch %1 to its Python environment, from the Python Package Index, as a ready-built package "
                  "(about 700 MB on disk). It runs on the processor, inside the same sandbox as the other trainings.\n\nGo on?")
                    .arg(ml::Environment::torchRequirements().value(0).section("==", 1)))
            != QMessageBox::Yes) return;
        env.install(true);
        syncEnv();
        return;
      }
      if (ui::askPlain(this, QCoreApplication::translate("ai", "Install the Python environment"),
              QCoreApplication::translate("ai", "Deepwater will create its own Python environment in its data folder and download scikit-learn, pandas, NumPy, "
                "SciPy, LightGBM and XGBoost from the Python Package Index (about 300 MB), as ready-built packages only.\n\nGo on?"))
          != QMessageBox::Yes) return;
      env.install();
      syncEnv();
    });
    head->addWidget(m_install);
    root->addLayout(head);
    m_installLine = text(QString(), theme::mono(10), c.textMute, this);
    m_installLine->setVisible(false);
    root->addWidget(m_installLine);

    // families
    auto* famRow = new QHBoxLayout; famRow->setSpacing(14);
    m_family = new ui::PillBar({QCoreApplication::translate("ai", "SUPERVISED"), QCoreApplication::translate("ai", "UNSUPERVISED"), QCoreApplication::translate("ai", "SEMI-SUPERVISED"), QCoreApplication::translate("ai", "SELF-SUPERVISED"), QCoreApplication::translate("ai", "REINFORCEMENT")},
                               this, ui::PillBar::Boxed);
    famRow->addWidget(m_family);
    m_familyDesc = text(QString(), theme::sans(12), c.textDim, this, true);
    famRow->addWidget(m_familyDesc, 1);
    root->addLayout(famRow);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);
    m_stack->addWidget(buildSupervised());
    auto* later = new ui::Panel(QString(), this);
    m_later = text(QString(), theme::sans(13), c.textDim, later, true);
    m_later->setAlignment(Qt::AlignCenter);
    later->body()->addStretch(1); later->body()->addWidget(m_later); later->body()->addStretch(1);
    m_stack->addWidget(later);

    const int fam = qBound(0, qEnvironmentVariableIsSet("DW_ML_FAMILY") ? qEnvironmentVariableIntValue("DW_ML_FAMILY")
                                                                        : settings::value("ml.family", 0).toInt(), 4);
    connect(m_family, &ui::PillBar::changed, this, [this](int i) { settings::setValue("ml.family", i); syncFamily(); });
    m_family->setCurrent(fam);
    syncFamily();

    auto& env = ml::Environment::instance();
    connect(&env, &ml::Environment::changed, this, [this] { syncEnv(); });
    connect(&env, &ml::Environment::installLine, this, [this](const QString& l) { m_installLine->setText(l.left(220)); });
    connect(&env, &ml::Environment::installFinished, this, [this](bool ok, const QString& err) {
      m_installLine->setText(ok ? QCoreApplication::translate("ai", "Environment ready.") : err);
      m_installLine->setStyleSheet(QString("color:%1;background:transparent;").arg((ok ? theme::pal().up : theme::pal().down).name()));
      syncEnv();
    });
    auto& runs = ml::Runs::instance();
    // the result panel is rebuilt (its CSV files read again: up to a million rows) only when what it shows changed: any
    // run starting or ending, or a progress line of another run, used to rebuild the selected run's finished result
    connect(&runs, &ml::Runs::listChanged, this, [this] {
      rebuildRuns(); syncTrain();
      if (runSignature(m_runList->selected()) != m_shownRun) showRun(m_runList->selected());
    });
    connect(&runs, &ml::Runs::runChanged, this, [this](const QString& id) {
      m_runList->update();
      if (id == m_runList->selected() && runSignature(id) != m_shownRun) showRun(id);
      syncTrain();
    });
    syncEnv();
    if (qEnvironmentVariableIsSet("DW_ML_INSTALL") && !env.installing()) env.install(qEnvironmentVariable("DW_ML_INSTALL") == "torch");   // test hook: install without the dialog
    rebuildRuns();
    if (!runs.list().isEmpty()) { m_runList->select(runs.list().first().id); showRun(runs.list().first().id); }
    applyHooks();
  }

protected:
  void paintEvent(QPaintEvent*) override { QPainter p(this); p.fillRect(rect(), theme::pal().bg); }
  // Python is probed once per session (a probe launches it a dozen times and imports torch: seconds of CPU); INSTALL
  // probes again by itself. While there is no environment yet, a show looks again at most once a minute (a Python the
  // user has just installed).
  void showEvent(QShowEvent*) override {
    auto& env = ml::Environment::instance();
    if (env.installing()) return;
    const auto& s = env.status();
    if (s.probed && s.venv) return;
    if (m_lastProbe.isValid() && m_lastProbe.elapsed() < 60000) return;
    m_lastProbe.start();
    env.refresh();
  }
  QElapsedTimer m_lastProbe;

private:
  // A panel's content in a vertical scroll area filling its body: the panel keeps its look when there is room and
  // scrolls (instead of overlapping) when the window is short. Returns the layout to fill.
  static QVBoxLayout* scrollBody(ui::Panel* panel) {
    auto* host = new QWidget;
    host->setAutoFillBackground(false);
    auto* l = new QVBoxLayout(host);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(panel->body()->spacing());
    auto* sa = new fit::VScrollArea(panel);
    sa->viewport()->setAutoFillBackground(false);
    sa->setStyleSheet("QScrollArea { background: transparent; }");
    sa->setWidget(host);
    panel->body()->addWidget(sa, 1);
    return l;
  }

  // ---------------------------------------------------------------- supervised layout
  QWidget* buildSupervised() {
    const auto& c = theme::pal();
    auto* host = new QWidget(this);
    auto* body = new QHBoxLayout(host); body->setContentsMargins(0, 0, 0, 0); body->setSpacing(12);

    // DATA
    auto* data = new ui::Panel(QCoreApplication::translate("ai", "Data"), host);
    // its content scrolls when the window is short (1024x600); the variables list takes the spare height otherwise
    auto* dl = scrollBody(data);
    m_source = new ui::PillBar({QCoreApplication::translate("ai", "PRICES"), "MACRO", "COT", "CSV", QCoreApplication::translate("ai", "TECHNICAL DATA")}, data,
                               ui::PillBar::Underline);
    dl->addWidget(m_source);
    auto* pick = new QHBoxLayout; pick->setSpacing(8);
    m_key = new fit::ElideCombo(data, 100);
    m_key->setMaxVisibleItems(24);
    pick->addWidget(m_key, 1);
    m_interval = new ui::PillBar({"1D", "1W", "1H"}, data, ui::PillBar::Boxed);
    pick->addWidget(m_interval);
    // TECHNICAL DATA (Data › Technical data): the timeframe and how many of the newest bars
    QStringList tfs;
    for (const auto& t : technicaldata::timeframes()) tfs << t.name;
    m_marketTf = new ui::PillBar(tfs, data, ui::PillBar::Boxed);
    m_marketTf->setCurrent(qBound(0, settings::value("ml.marketTf", 3).toInt(), int(tfs.size()) - 1));
    auto* pick2 = new QHBoxLayout; pick2->setSpacing(8);   // a second row: the timeframes and the rows do not fit beside the market
    pick2->addWidget(m_marketTf);
    m_marketRows = new QComboBox(data);
    for (int n : {50000, 200000, 500000, 1000000})
      m_marketRows->addItem(QCoreApplication::translate("ai", "last %1 bars").arg(QLocale(QLocale::English).toString(n)), n);
    m_marketRows->setCurrentIndex(qMax(0, m_marketRows->findData(settings::value("ml.marketRows", ml::TECHNICAL_DATA_ROWS_DEFAULT).toInt())));
    pick2->addWidget(m_marketRows);
    pick2->addStretch(1);
    m_choose = button(QCoreApplication::translate("ai", "CHOOSE FILE…"), false, data);
    pick->addWidget(m_choose);
    m_load = button(QCoreApplication::translate("ai", "LOAD"), false, data);
    pick->addWidget(m_load);
    dl->addLayout(pick);
    dl->addLayout(pick2);
    m_dataInfo = text(QCoreApplication::translate("ai", "Pick a source and LOAD it."), theme::mono(10), c.textMute, data, true);
    dl->addWidget(m_dataInfo);

    dl->addSpacing(6);
    m_targetLabel = micro(QCoreApplication::translate("ai", "TARGET: WHAT TO PREDICT"), data);
    dl->addWidget(m_targetLabel);
    auto* tg = new QGridLayout; tg->setHorizontalSpacing(8); tg->setVerticalSpacing(6);
    m_target = new fit::ElideCombo(data, 80);
    m_transform = new fit::ElideCombo(data, 110);
    for (const auto& t : transforms()) m_transform->addItem(t.label, t.id);
    m_transform->setCurrentIndex(3);   // direction (up / down): the usual first question on a market
    m_horizon = new QSpinBox(data); m_horizon->setRange(0, 250); m_horizon->setValue(1);
    m_horizon->setSuffix(" " + QCoreApplication::translate("ai", "row(s) ahead"));
    m_ofLabel = text(QCoreApplication::translate("ai", "of"), theme::mono(11), c.textDim, data);
    tg->addWidget(m_transform, 0, 0); tg->addWidget(m_ofLabel, 0, 1);
    tg->addWidget(m_target, 0, 2); tg->addWidget(m_horizon, 1, 0, 1, 3);
    tg->setColumnStretch(2, 1);
    dl->addLayout(tg);
    m_targetHint = text(QString(), theme::sans(11), c.textMute, data, true);
    dl->addWidget(m_targetHint);

    dl->addSpacing(6);
    auto* fh = new QHBoxLayout; fh->setSpacing(6);
    fh->addWidget(micro(QCoreApplication::translate("ai", "FEATURES: WHAT IT LEARNS FROM"), data), 1);
    auto* all = button(QCoreApplication::translate("ai", "ALL"), false, data), *none = button(QCoreApplication::translate("ai", "NONE"), false, data);
    fh->addWidget(all); fh->addWidget(none);
    dl->addLayout(fh);
    m_features = new QListWidget(data);
    m_features->setMinimumHeight(96);
    connect(m_features, &QListWidget::itemChanged, this, [this](QListWidgetItem* it) {
      m_featureChoice.insert(it->text(), it->checkState() == Qt::Checked);
      syncTrain();
    });
    dl->addWidget(m_features, 1);
    auto* lagRow = new QHBoxLayout;
    auto* lagText = new fit::LineLabel(QCoreApplication::translate("ai", "Past values of each feature too:"), data);
    lagText->setFont(theme::sans(11));
    lagText->setStyleSheet(QString("color:%1;background:transparent;").arg(c.textDim.name(QColor::HexArgb)));
    lagRow->addWidget(lagText);
    m_lags = new QSpinBox(data); m_lags->setRange(0, 10); m_lags->setValue(0); m_lags->setSuffix(" " + QCoreApplication::translate("ai", "lag(s)"));
    lagRow->addWidget(m_lags); lagRow->addStretch(1);
    dl->addLayout(lagRow);
    body->addWidget(data, 4);

    // MODEL
    auto* model = new ui::Panel(QCoreApplication::translate("ai", "Model"), host);
    auto* mb = scrollBody(model);   // everything above the TRAIN button scrolls when the window is short
    mb->addWidget(micro(QCoreApplication::translate("ai", "ALGORITHM"), model));
    m_algo = new QListWidget(model);
    fillAlgos(0);
    m_algo->setFixedHeight(int(qMax(algosFor(0).size(), algosFor(1).size())) * qMax(22, m_algo->sizeHintForRow(0)) + 6);
    m_algo->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mb->addWidget(m_algo);
    m_algoDesc = text(QString(), theme::sans(11), c.textDim, model, true);
    mb->addWidget(m_algoDesc);
    mb->addSpacing(4);
    mb->addWidget(micro(QCoreApplication::translate("ai", "PARAMETERS"), model));
    m_paramsHost = new QWidget(model);
    m_params = new QGridLayout(m_paramsHost); m_params->setContentsMargins(0, 0, 0, 0); m_params->setHorizontalSpacing(10); m_params->setVerticalSpacing(6);
    mb->addWidget(m_paramsHost);
    mb->addSpacing(4);
    m_supOnly << micro(QCoreApplication::translate("ai", "VALIDATION"), model);
    mb->addWidget(m_supOnly.last());
    auto* vg = new QGridLayout; vg->setHorizontalSpacing(10); vg->setVerticalSpacing(6);
    m_test = new QSpinBox(model); m_test->setRange(5, 50); m_test->setValue(20); m_test->setSuffix(" %");
    m_folds = new QSpinBox(model); m_folds->setRange(0, 10); m_folds->setValue(3);
    m_task = new QComboBox(model);
    m_task->addItem(QCoreApplication::translate("ai", "automatic"), "auto"); m_task->addItem(QCoreApplication::translate("ai", "classification"), "classification"); m_task->addItem(QCoreApplication::translate("ai", "regression"), "regression");
    m_supOnly << text(QCoreApplication::translate("ai", "Test part (latest rows)"), theme::sans(11), c.textDim, model, true) << m_test
              << text(QCoreApplication::translate("ai", "Time-series CV folds (0 = none)"), theme::sans(11), c.textDim, model, true) << m_folds
              << text(QCoreApplication::translate("ai", "Task"), theme::sans(11), c.textDim, model) << m_task;
    for (int k = 0; k < 6; ++k) vg->addWidget(m_supOnly[1 + k], k / 2, k % 2);
    vg->setColumnStretch(0, 1);
    mb->addLayout(vg);
    mb->addSpacing(4);
    mb->addWidget(micro(QCoreApplication::translate("ai", "NAME"), model));
    m_name = new QLineEdit(model);
    m_name->setPlaceholderText(QCoreApplication::translate("ai", "named after the algorithm and the data if empty"));
    mb->addWidget(m_name);
    mb->addStretch(1);
    mb = model->body();   // progress, state and TRAIN stay under the scrolling part
    m_progress = new QProgressBar(model); m_progress->setRange(0, 100); m_progress->setFixedHeight(8); m_progress->setVisible(false);
    mb->addWidget(m_progress);
    m_trainInfo = text(QString(), theme::mono(10), c.textMute, model, true);
    mb->addWidget(m_trainInfo);
    auto* tr = new QHBoxLayout;
    m_train = button(QCoreApplication::translate("ai", "TRAIN"), true, model);
    tr->addStretch(1); tr->addWidget(m_train);
    mb->addLayout(tr);
    body->addWidget(model, 4);

    // RUNS
    auto* runs = new ui::Panel(QCoreApplication::translate("ai", "Runs"), host);
    m_delete = button(QCoreApplication::translate("ai", "DELETE"), false, runs);
    m_folder = button(QCoreApplication::translate("ai", "FOLDER"), false, runs);
    runs->headerRight()->addWidget(m_folder);
    runs->headerRight()->addWidget(m_delete);
    m_runList = new RunList(runs);
    auto* rs = new QScrollArea(runs); rs->setWidgetResizable(true); rs->setFrameShape(QFrame::NoFrame); rs->setWidget(m_runList);
    m_runScroll = rs;
    runs->body()->addWidget(rs);
    auto* resHost = new QWidget(runs);
    m_result = new QVBoxLayout(resHost); m_result->setContentsMargins(0, 6, 6, 0); m_result->setSpacing(6);
    auto* res = new QScrollArea(runs); res->setWidgetResizable(true); res->setFrameShape(QFrame::NoFrame); res->setWidget(resHost);
    res->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // everything wraps to the column's width
    runs->body()->addWidget(res, 1);
    body->addWidget(runs, 6);

    // wiring
    // the data set always follows the selection: a new market, interval or source loads at once
    // (2026-11-16: the user picked AUD/CHF without LOAD and trained on the EUR/USD still loaded)
    connect(m_source, &ui::PillBar::changed, this, [this] { fillKeys(); if (m_source->current() != 3) loadData(); else setDataset(ml::Dataset{}); });
    connect(m_marketTf, &ui::PillBar::changed, this, [this](int i) { settings::setValue("ml.marketTf", i); loadData(); });
    connect(m_marketRows, &QComboBox::currentIndexChanged, this, [this] { settings::setValue("ml.marketRows", m_marketRows->currentData().toInt()); loadData(); });
    connect(m_key, &QComboBox::currentIndexChanged, this, [this] { loadData(); });
    connect(m_interval, &ui::PillBar::changed, this, [this] { loadData(); });
    connect(m_choose, &QPushButton::clicked, this, [this] {
      const QString f = QFileDialog::getOpenFileName(this, QCoreApplication::translate("ai", "A CSV file with a header row"), QString(), "CSV (*.csv *.txt)");
      if (f.isEmpty()) return;
      m_csv = f;
      {
        // addItem fires currentIndexChanged → loadData(): with the call below the file was read and parsed twice
        const QSignalBlocker block(m_key);
        m_key->clear(); m_key->addItem(QFileInfo(f).fileName(), f);
      }
      loadData();
    });
    connect(m_load, &QPushButton::clicked, this, [this] { loadData(); });
    connect(all, &QPushButton::clicked, this, [this] { setAllFeatures(true); });
    connect(none, &QPushButton::clicked, this, [this] { setAllFeatures(false); });
    connect(m_algo, &QListWidget::currentRowChanged, this, [this] { buildParams(); if (m_install) syncEnv(); });
    connect(m_transform, &QComboBox::currentIndexChanged, this, [this] { syncTargetHint(); });
    connect(m_target, &QComboBox::currentIndexChanged, this, [this] { syncTargetHint(); });
    connect(m_horizon, &QSpinBox::valueChanged, this, [this] { syncTargetHint(); });
    connect(m_test, &QSpinBox::valueChanged, this, [this] { syncTargetHint(); });   // the reinforcement note quotes it
    connect(m_train, &QPushButton::clicked, this, [this] {
      if (ml::Runs::instance().running()) ml::Runs::instance().stop(); else train();
    });
    m_runList->picked = [this](const QString& id) { showRun(id); };
    connect(m_delete, &QPushButton::clicked, this, [this] {
      const QString id = m_runList->selected();
      const ml::Run* r = ml::Runs::instance().find(id);
      if (!r || r->state == ml::RunState::Running) return;
      if (ui::askPlain(this, QCoreApplication::translate("ai", "Delete the run"), QCoreApplication::translate("ai", "Delete \"%1\" and its model?").arg(r->name)) != QMessageBox::Yes) return;
      ml::Runs::instance().remove(id);
      const auto& l = ml::Runs::instance().list();
      m_runList->select(l.isEmpty() ? QString() : l.first().id);
      showRun(m_runList->selected());
    });
    connect(m_folder, &QPushButton::clicked, this, [this] {
      const ml::Run* r = ml::Runs::instance().find(m_runList->selected());
      QDesktopServices::openUrl(QUrl::fromLocalFile(r ? r->dir : ml::runsDir()));
    });
    m_source->setCurrent(qBound(0, settings::value("ml.source", 0).toInt(), 4));
    fillKeys();
    QTimer::singleShot(0, this, [this] { if (!qEnvironmentVariableIsSet("DW_ML_LOAD") && m_source->current() != 3) loadData(); });
    return host;
  }

  void rebuildRuns() {
    m_runList->rebuild();
    m_runScroll->setFixedHeight(qBound(52, int(ml::Runs::instance().list().size()) * 50 + 4, 4 * 50 + 4));
  }

  // ---------------------------------------------------------------- environment
  void syncEnv() {
    const auto& c = theme::pal();
    auto& env = ml::Environment::instance();
    const auto& s = env.status();
    QString t; QColor col = c.textDim;
    if (env.installing()) { t = QCoreApplication::translate("ai", "installing (%1/%2)…").arg(env.installStep() + 1).arg(ml::Environment::installSteps()); col = c.warn; }
    else if (!s.probed) t = QCoreApplication::translate("ai", "checking Python…");
    else if (s.ready()) {
      QStringList parts{"Python " + s.baseVersion, "scikit-learn " + s.packages.value("sklearn")};
      if (s.packages.contains("torch")) parts << "PyTorch " + s.packages.value("torch");
      for (const char* k : {"lightgbm", "xgboost"}) if (s.packages.contains(k)) parts << QString(k == QString("lightgbm") ? "LightGBM " : "XGBoost ") + s.packages.value(k);
      t = parts.join("  ·  "); col = c.up;
    } else if (s.basePython.isEmpty()) { t = QCoreApplication::translate("ai", "no Python 3.11+ on this computer: install it from python.org"); col = c.down; }
    else { t = QCoreApplication::translate("ai", "Python %1 found · environment not installed").arg(s.baseVersion); col = c.warn; }
    m_env->setText(t);
    m_env->setStyleSheet(QString("color:%1;background:transparent;").arg(col.name()));
    QStringList tip;
    for (auto it = s.unavailable.begin(); it != s.unavailable.end(); ++it) tip << it.key() + ": " + it.value();
    m_env->setToolTip(tip.join("\n"));
    const bool needTorch = s.ready() && !env.hasTorch() && (family() == 3 || (family() == 4 && m_algo && currentAlgo().needs == "torch"));
    m_install->setVisible(env.installing() || (s.probed && !s.ready() && !s.basePython.isEmpty()) || needTorch);
    m_install->setText(env.installing() ? QCoreApplication::translate("ai", "CANCEL") : needTorch ? QCoreApplication::translate("ai", "INSTALL PYTORCH") : QCoreApplication::translate("ai", "INSTALL"));
    m_installLine->setVisible(env.installing() || !m_installLine->text().isEmpty());
    if (env.installing()) m_installLine->setStyleSheet(QString("color:%1;background:transparent;").arg(c.textMute.name()));
    // algorithms whose library does not load
    for (int i = 0; i < m_algo->count(); ++i) {
      auto* it = m_algo->item(i);
      const Algo a = algoById(it->data(Qt::UserRole).toString());
      const bool off = !a.needs.isEmpty() && s.probed && s.venv && !s.packages.contains(a.needs);
      // PyTorch is installed on request: its methods stay selectable (TRAIN says what is missing)
      const bool torch = a.needs == "torch";
      it->setFlags(off && !torch ? Qt::NoItemFlags : Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      it->setText(a.name + (off ? "  —  " + (torch ? QCoreApplication::translate("ai", "needs PyTorch") : QCoreApplication::translate("ai", "unavailable")) : QString()));
      it->setToolTip(off ? s.unavailable.value(a.needs, QCoreApplication::translate("ai", "not installed")) : a.desc);
    }
    syncTrain();
  }

  void syncFamily() {
    const int f = m_family->current();
    static const char* desc[] = {
        QT_TRANSLATE_NOOP("ai", "Learn to predict a column from the others, from labelled history: a class (up / down) or a value."),
        QT_TRANSLATE_NOOP("ai", "Find structure without a target: groups of similar periods, market regimes, unusual days, fewer dimensions."),
        QT_TRANSLATE_NOOP("ai", "Learn from a few labelled rows and many unlabelled ones."),
        QT_TRANSLATE_NOOP("ai", "Learn from the data itself (predict hidden or future parts), then reuse what was learned."),
        QT_TRANSLATE_NOOP("ai", "An agent learns to buy, sell or stay flat on a market's history, rewarded by its simulated profit and loss.")};
    m_familyDesc->setText(QCoreApplication::translate("ai", desc[f]));
    if (f <= 4) { applyFamily(f); m_stack->setCurrentIndex(0); syncEnv(); return; }
    const int step = 4;
    m_later->setText(QCoreApplication::translate("ai", "This family comes with step %1 of the plan.\nDone: step 1, supervised; step 2, unsupervised; step 3, semi- and self-supervised.\n"
                       "Step 4: reinforcement on simulated trading.").arg(step));
    m_stack->setCurrentIndex(1);
  }

  int family() const { return m_family ? m_family->current() : 0; }
  bool unsup() const { return family() == 1; }
  bool targetOptional() const { return family() == 1 || family() == 3; }
  Algo currentAlgo() const { return m_algo->currentItem() ? algoById(m_algo->currentItem()->data(Qt::UserRole).toString()) : Algo{}; }
  void fillAlgos(int fam) {
    m_algo->blockSignals(true);
    m_algo->clear();
    for (const auto& a : algosFor(fam)) { auto* it = new QListWidgetItem(a.name, m_algo); it->setData(Qt::UserRole, a.id); }
    m_algo->blockSignals(false);
    m_algo->setCurrentRow(fam == 0 ? 2 : 0);   // gradient boosting / k-means
    m_algoFamily = fam;
  }
  void applyFamily(int fam) {
    if (fam != m_algoFamily) { fillAlgos(fam); syncEnv(); }
    // validation: the test part for every family that predicts; CV folds and task for supervised only
    for (int k = 0; k < m_supOnly.size(); ++k) m_supOnly[k]->setVisible(k <= 2 ? fam != 1 : fam == 0);
    m_targetLabel->setText(fam == 0 ? QCoreApplication::translate("ai", "TARGET: WHAT TO PREDICT")
                         : fam == 1 ? QCoreApplication::translate("ai", "OUTCOME (OPTIONAL): WHAT FOLLOWED EACH GROUP")
                         : fam == 2 ? QCoreApplication::translate("ai", "TARGET: THE CLASSES TO LEARN, FROM FEW LABELS")
                         : fam == 4 ? QCoreApplication::translate("ai", "PRICE: WHAT THE AGENT TRADES")
                                    : QCoreApplication::translate("ai", "PROBE (OPTIONAL): A TARGET TO TEST WHAT IT LEARNED"));
    // the agent trades the price itself: no change / direction / rows ahead
    m_transform->setVisible(fam != 4); m_ofLabel->setVisible(fam != 4); m_horizon->setVisible(fam != 4);
    fillTargets();
    syncTargetHint();
  }
  void fillTargets() {
    const QString keep = m_target->currentData().toString();
    m_target->blockSignals(true);
    m_target->clear();
    if (targetOptional()) m_target->addItem(QCoreApplication::translate("ai", "— none —"), QString());
    for (const auto& col : m_data.numeric) m_target->addItem(col, col);
    int k = m_target->findData(keep);
    if (k < 0 || (keep.isEmpty() && !targetOptional())) {
      const int ci = m_target->findData("close"), fi = m_target->findData("futures_close");
      // a file of the user's: the last column, the usual place of what a table is about (a class, a result)
      k = ci >= 0 ? ci : fi >= 0 ? fi : m_source->current() == 3 ? m_target->count() - 1 : 0;
    }
    m_target->setCurrentIndex(qMax(0, k));
    m_target->blockSignals(false);
  }

  // ---------------------------------------------------------------- data
  void fillKeys() {
    const int s = m_source->current();
    settings::setValue("ml.source", s);
    m_key->blockSignals(true);
    m_key->clear();
    if (s == 0) {
      for (const auto& i : market::instruments())
        m_key->addItem(market::display(i.symbol) + (i.name == market::display(i.symbol) ? QString() : "  —  " + i.name), i.symbol);
      const int k = m_key->findData(settings::value("ml.prices", "EURUSD").toString());
      m_key->setCurrentIndex(qMax(0, k));
    } else if (s == 1) {
      for (const auto& e : macro::economies()) m_key->addItem(macro::displayName(e), e.iso2);
      const int k = m_key->findData(settings::value("ml.macro", "us").toString());
      m_key->setCurrentIndex(qMax(0, k));
    } else if (s == 2) {
      for (const auto& m : positioning::Cot::markets()) m_key->addItem(m.shortName + "  —  " + m.contract, m.id);
      const int k = m_key->findData(settings::value("ml.cot", "EUR").toString());
      m_key->setCurrentIndex(qMax(0, k));
    } else if (s == 4) {
      const QStringList markets = technicaldata::Store::instance().pairs();
      for (const auto& m : markets)
        m_key->addItem(technicaldata::binanceSymbol(m) + (m.startsWith("DUKASCOPY:") ? QStringLiteral("  —  Dukascopy") : QStringLiteral("  —  Binance")), m);
      const int k = m_key->findData(settings::value("ml.market", "BINANCE:BTCUSDT").toString());
      m_key->setCurrentIndex(qMax(0, k));
    } else if (!m_csv.isEmpty()) m_key->addItem(QFileInfo(m_csv).fileName(), m_csv);
    m_key->blockSignals(false);
    m_key->setVisible(s != 3 || !m_csv.isEmpty());
    m_interval->setVisible(s == 0);
    m_marketTf->setVisible(s == 4);
    m_marketRows->setVisible(s == 4);
    m_choose->setVisible(s == 3);
    m_load->setVisible(s != 3);
  }

  void loadData() {
    const int s = m_source->current();
    ml::DatasetSpec spec;
    spec.kind = s == 0 ? ml::SourceKind::Prices : s == 1 ? ml::SourceKind::Macro : s == 2 ? ml::SourceKind::Cot : s == 4 ? ml::SourceKind::TechnicalData : ml::SourceKind::Csv;
    spec.key = m_key->currentData().toString();
    spec.interval = s == 4 ? technicaldata::timeframes().value(m_marketTf->current()).name : QStringList{"1d", "1wk", "1h"}.value(m_interval->current(), "1d");
    if (s == 4) spec.rows = m_marketRows->currentData().toInt();
    if (spec.key.isEmpty()) return;
    const QString want = selectionKey();
    setDataset(ml::Dataset{});        // the previous data set is gone: TRAIN waits for this one
    m_loading = true;
    syncTrain();
    if (s == 0) settings::setValue("ml.prices", spec.key);
    else if (s == 1) settings::setValue("ml.macro", spec.key);
    else if (s == 2) settings::setValue("ml.cot", spec.key);
    else if (s == 4) settings::setValue("ml.market", spec.key);
    m_dataInfo->setText(s == 1 ? QCoreApplication::translate("ai", "Loading… (macro data sets can take up to a minute the first time)") : QCoreApplication::translate("ai", "Loading…"));
    m_dataInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(theme::pal().textMute.name()));
    m_load->setEnabled(false);
    const int ticket = ++m_loadTicket;
    ml::build(spec, this, [this, ticket, want](const ml::Dataset& d) {
      if (ticket != m_loadTicket) return;   // a newer selection superseded it
      m_load->setEnabled(true);
      m_loading = false;
      m_loadedKey = d.ok() ? want : QString();
      setDataset(d);
    });
  }
  // what the DATA panel selects now: source, market / economy / file, interval
  QString selectionKey() const {
    const int s = m_source->current();
    return QString::number(s) + "|" + m_key->currentData().toString() + (s == 0 ? "|" + QStringList{"1d", "1wk", "1h"}.value(m_interval->current()) : QString()) +
           (s == 4 ? "|" + QString::number(m_marketTf->current()) + "|" + m_marketRows->currentData().toString() : QString());
  }

  void setDataset(const ml::Dataset& d) {
    const auto& c = theme::pal();
    m_data = d;
    if (!d.ok() && d.error.isEmpty()) {   // cleared: waiting for a load, or no file chosen yet
      if (!m_loading) m_loadedKey.clear();
      m_target->clear(); m_features->clear();
      if (!m_loading) m_dataInfo->setText(m_source->current() == 3 ? QCoreApplication::translate("ai", "Choose a CSV file.") : QString());
      syncTrain();
      return;
    }
    if (!d.ok()) {
      m_dataInfo->setText(d.error);
      m_dataInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(c.down.name()));
      m_target->clear(); m_features->clear();
      syncTrain();
      return;
    }
    m_dataInfo->setText(d.first.isEmpty()   // a table without dates
        ? QCoreApplication::translate("ai", "%1  ·  %2 rows  ·  %3").arg(d.title).arg(d.rows).arg(d.source)
        : QCoreApplication::translate("ai", "%1  ·  %2 rows  ·  %3 → %4  ·  %5").arg(d.title).arg(d.rows).arg(d.first.left(10), d.last.left(10), d.source));
    m_dataInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(c.textDim.name()));
    m_dataInfo->setToolTip(QCoreApplication::translate("ai", "Data sets are kept under %1 GB in total: past that, the oldest ones are deleted when a new one is written.")
                               .arg(ml::DATASETS_MAX_BYTES >> 30));
    if (!d.pruned.isEmpty()) {   // the deletion was silent: the user sees which data sets went
      m_dataInfo->setText(m_dataInfo->text() + "\n" +
                          QCoreApplication::translate("ai", "Over %1 GB of data sets: the oldest were deleted (%2).")
                              .arg(ml::DATASETS_MAX_BYTES >> 30).arg(d.pruned.join(", ")));
      m_dataInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(c.warn.name()));
    }
    fillTargets();
    QSignalBlocker quiet(m_features);   // filling the list is not the user's choice: itemChanged must not record it
    m_features->clear();
      for (const auto& col : d.numeric) {
      auto* it = new QListWidgetItem(col, m_features);
      it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      // a column the user has already seen keeps the box as they left it (switching market keeps the choice);
      // otherwise raw price levels are left out: a model that sees the level mostly learns the period
      it->setCheckState(m_featureChoice.contains(col) ? (m_featureChoice.value(col) ? Qt::Checked : Qt::Unchecked)
                        : ml::isLevelColumn(col) ? Qt::Unchecked : Qt::Checked);
    }
    if (d.dateColumn.isEmpty()) { m_transform->setCurrentIndex(0); m_horizon->setValue(0); }
    else if (m_transform->currentIndex() == 0 && m_horizon->value() == 0) { m_transform->setCurrentIndex(3); m_horizon->setValue(1); }
    syncTargetHint();
    syncTrain();
  }
  void setAllFeatures(bool on) { for (int i = 0; i < m_features->count(); ++i) m_features->item(i)->setCheckState(on ? Qt::Checked : Qt::Unchecked); }

  // the target's own column cannot be a feature when the model predicts its value at the same row (it would read the answer)
  void syncTargetFeature() {
    const QString col = m_target->currentData().toString();
    const bool sameRow = (family() == 0 || family() == 2) && m_transform->currentData().toString() == "raw" && m_horizon->value() == 0;
    QSignalBlocker quiet(m_features);
    for (int i = 0; i < m_features->count(); ++i) {
      auto* it = m_features->item(i);
      const bool answer = sameRow && it->text() == col;
      const bool wasOff = !(it->flags() & Qt::ItemIsEnabled);
      it->setFlags(answer ? Qt::ItemIsUserCheckable : Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      // shown unticked while set aside; the user's own choice comes back when it is a feature again
      if (answer) it->setCheckState(Qt::Unchecked);
      else if (wasOff) it->setCheckState(m_featureChoice.value(it->text(), true) ? Qt::Checked : Qt::Unchecked);
      it->setToolTip(answer ? QCoreApplication::translate("ai", "The target itself: left out, the model would read the answer.") : QString());
    }
  }
  void syncTargetHint() {
    syncTargetFeature();
    const QString tr = m_transform->currentData().toString(), col = m_target->currentData().toString();
    const int hz = m_horizon->value();
    m_transform->setEnabled(!col.isEmpty()); m_horizon->setEnabled(!col.isEmpty());
    if (unsup()) {
      m_targetHint->setText(col.isEmpty() ? QCoreApplication::translate("ai", "The groups are formed from the features only.")
          : tr != "raw" && hz == 0 ? QCoreApplication::translate("ai", "A change or a direction needs at least 1 row ahead.")
          : QCoreApplication::translate("ai", "Not used to form the groups: the %1 of %2 over the next %3 row(s) is shown for each group, to see what followed it.")
                .arg(m_transform->currentText(), col).arg(hz));
      syncTrain();
      return;
    }
    if (family() == 4) {
      m_targetHint->setText(col.isEmpty() ? QString()
          : QCoreApplication::translate("ai", "At each row the agent picks a position held until the next row and earns the change of %1, minus the costs. "
                    "It learns on the older rows and is judged on the latest %2% it never saw. A simulation only: no order is sent.").arg(col).arg(m_test->value()));
      syncTrain();
      return;
    }
    if (family() == 3) {
      m_targetHint->setText(col.isEmpty() ? QCoreApplication::translate("ai", "It learns from the features alone; without a probe it shows the map and the unusual windows.")
          : tr != "raw" && hz == 0 ? QCoreApplication::translate("ai", "A change or a direction needs at least 1 row ahead.")
          : QCoreApplication::translate("ai", "Not used to learn: after training, a simple linear model predicts the %1 of %2 over the next %3 row(s) from what it learned, compared with the raw features.")
                .arg(m_transform->currentText(), col).arg(hz));
      syncTrain();
      return;
    }
    const int h = m_horizon->value();
    QString t;
    if (col.isEmpty()) t.clear();
    else if (tr == "raw" && h == 0) t = QCoreApplication::translate("ai", "Predicts %1 of the same row from the other columns (no look ahead: for tables that are not time series).").arg(col);
    else if (tr == "raw") t = QCoreApplication::translate("ai", "Predicts the value of %1, %2 row(s) later.").arg(col).arg(h);
    else if (tr == "direction") t = QCoreApplication::translate("ai", "Predicts whether %1 is higher %2 row(s) later (1) or not (0): a classification.").arg(col).arg(h);
    else t = QCoreApplication::translate("ai", "Predicts how much %1 moves over the next %2 row(s).").arg(col).arg(h);
    if (tr != "raw" && h == 0) t = QCoreApplication::translate("ai", "A change or a direction needs at least 1 row ahead.");
    if (family() == 2 && !col.isEmpty() && tr != "direction")
      t += " " + QCoreApplication::translate("ai", "Semi-supervised learning works on classes: pick the direction, or a column of classes.");
    m_targetHint->setText(t);
    syncTrain();
  }

  // ---------------------------------------------------------------- model
  void buildParams() {
    while (auto* it = m_params->takeAt(0)) { if (it->widget()) it->widget()->deleteLater(); delete it; }
    m_paramEdits.clear();
    const Algo a = currentAlgo();
    if (a.id.isEmpty()) return;
    m_algoDesc->setText(a.desc);
    int r = 0;
    for (const auto& p : a.params) {
      m_params->addWidget(text(p.label, theme::sans(11), theme::pal().textDim, m_paramsHost, true), r, 0);
      QWidget* w = nullptr;
      if (p.type == 'i') { auto* s = new QSpinBox(m_paramsHost); s->setRange(int(p.min), int(p.max)); s->setSingleStep(int(p.step)); s->setValue(int(p.def)); w = s; }
      else if (p.type == 'd') { auto* s = new QDoubleSpinBox(m_paramsHost); s->setDecimals(3); s->setRange(p.min, p.max); s->setSingleStep(p.step); s->setValue(p.def); w = s; }
      else if (p.type == 'c') { auto* cb = new QComboBox(m_paramsHost); cb->addItems(p.options); cb->setCurrentText(p.text); w = cb; }
      else { auto* e = new QLineEdit(p.text, m_paramsHost); w = e; }
      w->setMinimumWidth(110);
      m_params->addWidget(w, r, 1);
      m_paramEdits.insert(p.key, w);
      ++r;
    }
    m_params->setColumnStretch(0, 1);
  }
  QJsonObject params() const {
    QJsonObject o;
    for (auto it = m_paramEdits.begin(); it != m_paramEdits.end(); ++it) {
      if (auto* s = qobject_cast<QSpinBox*>(it.value())) o[it.key()] = s->value();
      else if (auto* d = qobject_cast<QDoubleSpinBox*>(it.value())) o[it.key()] = d->value();
      else if (auto* e = qobject_cast<QLineEdit*>(it.value())) o[it.key()] = e->text();
      else if (auto* cb = qobject_cast<QComboBox*>(it.value())) o[it.key()] = cb->currentText();
    }
    return o;
  }
  QStringList checkedFeatures() const {
    QStringList f;
    for (int i = 0; i < m_features->count(); ++i)
      if (m_features->item(i)->checkState() == Qt::Checked && (m_features->item(i)->flags() & Qt::ItemIsEnabled)) f << m_features->item(i)->text();
    return f;
  }

  void syncTrain() {
    if (!m_train) return;
    const auto& c = theme::pal();
    auto& runs = ml::Runs::instance();
    const bool running = runs.running();
    m_train->setText(running ? QCoreApplication::translate("ai", "STOP") : QCoreApplication::translate("ai", "TRAIN"));
    QString why;
    if (!running) {
      if (!ml::Environment::instance().status().ready()) why = QCoreApplication::translate("ai", "Install the Python environment first (INSTALL, top right).");
      else if (m_loading) why = QCoreApplication::translate("ai", "Loading the data set…");
      else if (!m_data.ok()) why = QCoreApplication::translate("ai", "Load a data set first.");
      else if (m_loadedKey != selectionKey()) why = QCoreApplication::translate("ai", "The data loaded is not the one selected: LOAD it.");
      else if (!targetOptional() && m_target->currentData().toString().isEmpty()) why = QCoreApplication::translate("ai", "Pick a target.");
      else if (checkedFeatures().isEmpty()) why = QCoreApplication::translate("ai", "Tick at least one feature.");
      else if (!m_target->currentData().toString().isEmpty() && m_transform->currentData().toString() != "raw" && m_horizon->value() == 0) why = QCoreApplication::translate("ai", "A change or a direction needs at least 1 row ahead.");
      else if (currentAlgo().needs == "torch" && !ml::Environment::instance().hasTorch())
        why = QCoreApplication::translate("ai", "Install PyTorch first (INSTALL PYTORCH, top right).");
      else if (m_algo->currentItem() && !(m_algo->currentItem()->flags() & Qt::ItemIsEnabled)) why = QCoreApplication::translate("ai", "This algorithm is unavailable.");
    }
    m_train->setEnabled(running || why.isEmpty());
    const ml::Run* cur = nullptr;
    for (const auto& r : runs.list()) if (r.state == ml::RunState::Running) { cur = &r; break; }
    m_progress->setVisible(cur != nullptr);
    if (cur) {
      m_progress->setValue(fmt::roundInt(cur->pct));
      m_trainInfo->setText(QCoreApplication::translate("ai", "Training \"%1\": %2").arg(cur->name, cur->stage));
      m_trainInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(c.warn.name()));
    } else {
      m_trainInfo->setText(why.isEmpty() ? QCoreApplication::translate("ai", "Runs in a sandbox: no network, no access to your files or keys.") : why);
      m_trainInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(c.textMute.name()));
    }
  }

  void train() {
    const Algo a = currentAlgo();
    if (a.id.isEmpty() || !m_data.ok() || m_loading || m_loadedKey != selectionKey()) return;   // only the selected data
    syncTrain();
    if (!m_train->isEnabled()) return;   // the same checks as the button (features ticked, target, PyTorch…)
    const QString target = m_target->currentData().toString();
    static const char* fams[] = {"supervised", "unsupervised", "semi_supervised", "self_supervised", "reinforcement"};
    QJsonObject job{{"family", fams[qBound(0, family(), 4)]}, {"target", target}, {"target_transform", m_transform->currentData().toString()},
                    {"horizon", m_horizon->value()}, {"features", QJsonArray::fromStringList(checkedFeatures())}, {"lags", m_lags->value()},
                    {"algorithm", a.id}, {"params", params()}, {"test_size", m_test->value() / 100.0}, {"cv_folds", m_folds->value()},
                    {"task", m_task->currentData().toString()}, {"seed", 42}};
    if (!m_data.dateColumn.isEmpty()) job["date_column"] = m_data.dateColumn;
    QString name = m_name->text().trimmed();
    if (name.isEmpty())
      name = a.name + " · " + m_data.title + (target.isEmpty() ? QString()
             : family() == 4 ? " · " + QCoreApplication::translate("ai", "trades %1").arg(target) : " · " + m_transform->currentText() + " " + target);
    QString err;
    const QString id = ml::Runs::instance().start(job, m_data.path, m_data.title, name, &err);
    if (id.isEmpty()) { m_trainInfo->setText(err); m_trainInfo->setStyleSheet(QString("color:%1;background:transparent;").arg(theme::pal().down.name())); return; }
    m_runList->select(id);
    showRun(id);
    syncTrain();
  }

  // ---------------------------------------------------------------- results
  // what showRun() draws of a run: the same signature, the same panel
  static QString runSignature(const QString& id) {
    const ml::Run* r = ml::Runs::instance().find(id);
    if (!r) return id + "|-";
    return QStringList{id, QString::number(int(r->state)), r->stage, QString::number(r->pct), r->error, r->name,
                       QString::number(r->result.size())}.join('|');
  }
  void showRun(const QString& id) {
    m_shownRun = runSignature(id);
    while (auto* it = m_result->takeAt(0)) {
      if (it->widget()) it->widget()->deleteLater();
      else if (it->layout()) { while (auto* s = it->layout()->takeAt(0)) { if (s->widget()) s->widget()->deleteLater(); delete s; } }
      delete it;
    }
    const auto& c = theme::pal();
    QWidget* host = m_result->parentWidget();
    m_delete->setEnabled(false);
    const ml::Run* r = ml::Runs::instance().find(id);
    if (!r) { m_result->addStretch(1); return; }
    m_delete->setEnabled(r->state != ml::RunState::Running);
    m_result->addWidget(text(r->name, theme::mono(12, QFont::Bold), c.text, host, true));
    const QJsonObject job = r->job;
    if (job.value("family").toString() == "unsupervised") { showUnsupervised(*r, host); return; }
    if (job.value("family").toString() == "self_supervised") { showSelf(*r, host); return; }
    if (job.value("family").toString() == "reinforcement") { showRl(*r, host); return; }
    const Algo a = algoById(job.value("algorithm").toString());
    QStringList feats; for (const auto& f : job.value("features").toArray()) if (f.toString() != job.value("target").toString() || job.value("horizon").toInt() > 0) feats << f.toString();
    m_result->addWidget(text(QCoreApplication::translate("ai", "%1 · target: %2 of %3, %4 row(s) ahead · %5 feature(s)%6")
                                 .arg(a.id.isEmpty() ? job.value("algorithm").toString() : a.name,
                                      transformLabel(job.value("target_transform").toString()), job.value("target").toString())
                                 .arg(job.value("horizon").toInt()).arg(feats.size())
                                 .arg(job.value("lags").toInt() ? " + " + QCoreApplication::translate("ai", "%1 lag(s)").arg(job.value("lags").toInt()) : QString()),
                             theme::mono(10), c.textDim, host, true));
    if (r->state == ml::RunState::Running) {
      m_result->addWidget(text(QCoreApplication::translate("ai", "Training: %1 (%2%)").arg(r->stage).arg(fmt::roundInt(r->pct)), theme::mono(11), c.warn, host));
      m_result->addStretch(1);
      return;
    }
    if (r->state != ml::RunState::Done) {
      m_result->addWidget(text(r->error.isEmpty() ? ml::stateName(r->state) : r->error, theme::mono(11), r->state == ml::RunState::Failed ? c.down : c.textDim, host, true));
      m_result->addStretch(1);
      return;
    }
    const QJsonObject res = r->result;
    const bool cls = res.value("task").toString() == "classification";
    const QJsonArray tp = res.value("test_period").toArray(), trp = res.value("train_period").toArray();
    m_result->addWidget(text(QCoreApplication::translate("ai", "%1 · trained on %2 rows (%3 → %4), tested on the next %5 rows (%6 → %7) · %8 s")
                                 .arg(cls ? QCoreApplication::translate("ai", "classification") : QCoreApplication::translate("ai", "regression")).arg(res.value("train_rows").toInt())
                                 .arg(trp.at(0).toString().left(10), trp.at(1).toString().left(10)).arg(res.value("test_rows").toInt())
                                 .arg(tp.at(0).toString().left(10), tp.at(1).toString().left(10))
                                 .arg(res.value("seconds").toDouble(), 0, 'f', 1),
                             theme::mono(10), c.textMute, host, true));
    if (res.value("family").toString() == "semi_supervised") showSemiBlock(res, host);

    // metrics: test | train | no skill
    const QJsonObject te = res.value("test").toObject(), trn = res.value("train").toObject(), base = res.value("baseline").toObject();
    auto* g = new QGridLayout; g->setHorizontalSpacing(16); g->setVerticalSpacing(3);
    const QStringList heads{QCoreApplication::translate("ai", "METRIC"), QCoreApplication::translate("ai", "TEST"), QCoreApplication::translate("ai", "TRAINING"), QCoreApplication::translate("ai", "NO SKILL")};
    for (int k = 0; k < heads.size(); ++k) g->addWidget(micro(heads[k], host), 0, k, k ? Qt::AlignRight : Qt::AlignLeft);
    QList<QPair<QString, QString>> rows;
    if (cls) rows = {{"balanced_accuracy", QCoreApplication::translate("ai", "Balanced accuracy")}, {"accuracy", QCoreApplication::translate("ai", "Accuracy")}, {"roc_auc", "ROC AUC"},
                     {"precision", QCoreApplication::translate("ai", "Precision")}, {"recall", QCoreApplication::translate("ai", "Recall")}, {"f1", "F1"}};
    else rows = {{"mae", QCoreApplication::translate("ai", "Mean absolute error")}, {"rmse", "RMSE"}, {"r2", "R²"}, {"direction_accuracy", QCoreApplication::translate("ai", "Right direction")}};
    int gr = 1;
    for (const auto& [key, label] : rows) {
      if (!te.contains(key)) continue;
      const bool asPct = cls || key == "direction_accuracy";
      auto fmt = [&](const QJsonObject& o) { return o.contains(key) ? (asPct ? pct(jnum(o.value(key))) : fixed(jnum(o.value(key)))) : QStringLiteral("—"); };
      g->addWidget(text(label, theme::sans(11), c.textDim, host), gr, 0);
      g->addWidget(text(fmt(te), theme::mono(11, QFont::Bold), c.text, host), gr, 1, Qt::AlignRight);
      g->addWidget(text(fmt(trn), theme::mono(11), c.textDim, host), gr, 2, Qt::AlignRight);
      g->addWidget(text(fmt(base), theme::mono(11), c.textMute, host), gr, 3, Qt::AlignRight);
      ++gr;
    }
    g->setColumnStretch(0, 1);
    m_result->addLayout(g);
    const QString rule = base.value("rule").toString();
    const QString ruleText = rule == "majority" ? QCoreApplication::translate("ai", "always class %1, the most frequent in training").arg(base.value("class").toInt())
                           : rule == "mean" ? QCoreApplication::translate("ai", "always the training mean")
                           : rule == "mean_direction" ? QCoreApplication::translate("ai", "always the training mean (for the direction: always up when that mean is above 0)")
                           : rule;
    m_result->addWidget(text(QCoreApplication::translate("ai", "No skill: %1. A model is only useful where TEST beats it; TRAINING far above TEST means it learned the past by heart.")
                                 .arg(ruleText), theme::sans(11), c.textMute, host, true));
    const QJsonObject cv = res.value("cv").toObject();
    if (!cv.isEmpty())
      m_result->addWidget(text(QCoreApplication::translate("ai", "Cross-validation on the training rows (%1 folds in time order): %2 = %3 ± %4")
                                   .arg(cv.value("folds").toInt()).arg(cv.value("metric").toString())
                                   .arg(cls ? pct(jnum(cv.value("mean"))) : fixed(jnum(cv.value("mean"))),
                                        cls ? pct(jnum(cv.value("std"))) : fixed(jnum(cv.value("std")))),
                               theme::mono(10), c.textDim, host, true));
    const QJsonObject latest = res.value("latest").toObject();
    if (!latest.isEmpty()) {
      const double pv = jnum(latest.value("prediction"));
      QString what;
      if (cls && job.value("target_transform").toString() == "direction")
        what = (pv > 0.5 ? QCoreApplication::translate("ai", "UP") : QCoreApplication::translate("ai", "NOT UP")) + (latest.contains("probability") ? "  " + QCoreApplication::translate("ai", "(probability of up %1)").arg(pct(jnum(latest.value("probability")))) : QString());
      else if (cls) what = QCoreApplication::translate("ai", "class %1").arg(fmt::roundInt(pv));
      else what = fixed(pv);
      m_result->addWidget(text(QCoreApplication::translate("ai", "Latest row (%1), %2 row(s) ahead: %3").arg(latest.value("date").toString().left(16)).arg(job.value("horizon").toInt()).arg(what),
                               theme::mono(11, QFont::Bold), c.accentText, host, true));
      m_result->addWidget(text(QCoreApplication::translate("ai", "A model's output, not advice: check how it did on the test rows above before giving it any weight."),
                               theme::sans(10), c.textMute, host, true));
    }

    m_result->addSpacing(6);
    m_result->addWidget(micro(cls ? QCoreApplication::translate("ai", "TEST PERIOD: HIT RATE") : QCoreApplication::translate("ai", "TEST PERIOD: ACTUAL AND PREDICTED"), host));
    auto* chart = new PredChart(host);
    chart->setData(ml::runFile(*r, "predictions.csv"), cls, cls ? jnum(base.value("accuracy")) : NAN);
    m_result->addWidget(chart);

    const QJsonObject imp = res.value("importances").toObject();
    QList<QPair<QString, double>> items;
    for (const auto& v : imp.value("items").toArray()) items << qMakePair(v.toObject().value("feature").toString(), v.toObject().value("share").toDouble());
    if (!items.isEmpty()) {
      m_result->addSpacing(6);
      const QString how = imp.value("method").toString();
      const QString howText = how == "model" ? QCoreApplication::translate("ai", "the model's own measure")
                            : how.startsWith("coefficients") ? QCoreApplication::translate("ai", "size of the coefficients, features scaled")
                            : how == "permutation" ? QCoreApplication::translate("ai", "loss when the feature is shuffled") : how;
      m_result->addWidget(micro(QCoreApplication::translate("ai", "WHAT IT RELIES ON (%1)").arg(howText.toUpper()), host));
      auto* bars = new ImportanceBars(host);
      bars->setItems(items);
      m_result->addWidget(bars);
    }
    const QJsonObject cm = res.value("confusion").toObject();
    if (!cm.isEmpty()) {
      m_result->addSpacing(6);
      m_result->addWidget(micro(QCoreApplication::translate("ai", "CONFUSION ON THE TEST ROWS (ROWS: ACTUAL, COLUMNS: PREDICTED)"), host));
      auto* cg = new QGridLayout; cg->setHorizontalSpacing(18); cg->setVerticalSpacing(2);
      const QJsonArray labels = cm.value("labels").toArray(), mx = cm.value("matrix").toArray();
      for (int k = 0; k < labels.size(); ++k) {
        cg->addWidget(text(QString::number(labels[k].toInt()), theme::mono(10, QFont::Bold), c.textMute, host), 0, k + 1, Qt::AlignRight);
        cg->addWidget(text(QString::number(labels[k].toInt()), theme::mono(10, QFont::Bold), c.textMute, host), k + 1, 0);
        const QJsonArray row = mx.at(k).toArray();
        for (int j = 0; j < row.size(); ++j)
          cg->addWidget(text(QString::number(row[j].toInt()), theme::mono(11, j == k ? QFont::Bold : QFont::Normal), j == k ? c.up : c.textDim, host), k + 1, j + 1, Qt::AlignRight);
      }
      cg->setColumnStretch(labels.size() + 1, 1);
      m_result->addLayout(cg);
    }
    m_result->addStretch(1);
  }

  void showUnsupervised(const ml::Run& r, QWidget* host) {
    const auto& c = theme::pal();
    const QJsonObject job = r.job, res = r.result;
    const Algo a = algoById(job.value("algorithm").toString());
    QStringList feats; for (const auto& f : job.value("features").toArray()) feats << f.toString();
    QString line = QCoreApplication::translate("ai", "%1 · %2 feature(s)").arg(a.id.isEmpty() ? job.value("algorithm").toString() : a.name).arg(feats.size());
    if (job.value("lags").toInt()) line += " + " + QCoreApplication::translate("ai", "%1 lag(s)").arg(job.value("lags").toInt());
    const QString outcome = job.value("target").toString();
    if (!outcome.isEmpty())
      line += " · " + QCoreApplication::translate("ai", "outcome: %1 of %2 over the next %3 row(s)")
                          .arg(transformLabel(job.value("target_transform").toString()), outcome).arg(job.value("horizon").toInt());
    m_result->addWidget(text(line, theme::mono(10), c.textDim, host, true));
    if (r.state == ml::RunState::Running) {
      m_result->addWidget(text(QCoreApplication::translate("ai", "Training: %1 (%2%)").arg(r.stage).arg(fmt::roundInt(r.pct)), theme::mono(11), c.warn, host));
      m_result->addStretch(1);
      return;
    }
    if (r.state != ml::RunState::Done) {
      m_result->addWidget(text(r.error.isEmpty() ? ml::stateName(r.state) : r.error, theme::mono(11), r.state == ml::RunState::Failed ? c.down : c.textDim, host, true));
      m_result->addStretch(1);
      return;
    }
    const QString kind = res.value("kind").toString();
    const QJsonArray per = res.value("period").toArray();
    m_result->addWidget(text(QCoreApplication::translate("ai", "%1 rows (%2 → %3), all of them used: nothing is held out, nothing is predicted · %4 s")
                                 .arg(res.value("rows").toInt()).arg(per.at(0).toString().left(10), per.at(1).toString().left(10))
                                 .arg(res.value("seconds").toDouble(), 0, 'f', 1),
                             theme::mono(10), c.textMute, host, true));
    if (res.value("clipped").toInt() > 0)
      m_result->addWidget(text(QCoreApplication::translate("ai", "%1 extreme values (beyond ±%2σ: bad ticks, crashes) were brought back to ±%2σ so they do not form a group of their own.")
                                   .arg(res.value("clipped").toInt()).arg(res.value("clip_sigma").toDouble(), 0, 'f', 0),
                               theme::mono(10), c.textMute, host, true));
    const Assign as = readAssign(ml::runFile(r, "assignments.csv"));
    const QString tf = job.value("target_transform").toString();
    const bool pctOutcome = tf == "pct_change";
    auto outcomeText = [&](const QJsonObject& o) {
      if (o.isEmpty()) return QStringLiteral("—");
      QString t = tf == "direction" ? QCoreApplication::translate("ai", "up %1 of the time").arg(pct(jnum(o.value("mean"))))
                                    : (jnum(o.value("mean")) >= 0 ? "+" : "") + QString::number(jnum(o.value("mean")), 'f', pctOutcome ? 3 : 4) + (pctOutcome ? "%" : "");
      if (tf != "direction" && o.value("up_share").isDouble()) t += "  ·  " + QCoreApplication::translate("ai", "up %1").arg(pct(jnum(o.value("up_share")), 0));
      return t;
    };

    if (kind == "clustering" || kind == "anomaly") {
      const QJsonObject q = res.value("quality").toObject();
      if (!q.isEmpty())
        m_result->addWidget(text(QCoreApplication::translate("ai", "Silhouette %1 (from −1 to 1: above 0.5 well apart, near 0 groups that overlap)  ·  Davies-Bouldin %2 (lower is better)")
                                     .arg(jnum(q.value("silhouette")), 0, 'f', 2).arg(jnum(q.value("davies_bouldin")), 0, 'f', 2),
                                 theme::mono(10), c.textDim, host, true));
      if (res.contains("bic"))
        m_result->addWidget(text(QCoreApplication::translate("ai", "BIC %1 · AIC %2 (lower is better, to compare runs with another number of groups)")
                                     .arg(jnum(res.value("bic")), 0, 'f', 0).arg(jnum(res.value("aic")), 0, 'f', 0), theme::mono(10), c.textDim, host, true));
      if (jnum(res.value("noise_share")) > 0)
        m_result->addWidget(text(QCoreApplication::translate("ai", "%1 of the rows fit no group (noise).").arg(pct(jnum(res.value("noise_share")))), theme::mono(10), c.textDim, host, true));
      // latest row
      const QJsonObject lt = res.value("latest").toObject();
      if (!lt.isEmpty()) {
        const int lab = lt.value("label").toInt();
        QString t = kind == "anomaly"
            ? QCoreApplication::translate("ai", "Latest row (%1): %2 · more unusual than %3 of the rows").arg(lt.value("date").toString().left(10), groupName(lab, kind), pct(jnum(lt.value("rank")), 0))
            : QCoreApplication::translate("ai", "Latest row (%1) is in %2").arg(lt.value("date").toString().left(10), groupName(lab, kind));
        auto* l = text(t, theme::mono(11, QFont::Bold), kind == "anomaly" ? (lab ? c.down : c.accentText) : groupColor(lab), host, true);
        m_result->addWidget(l);
      }
      m_result->addSpacing(4);
      m_result->addWidget(micro(kind == "anomaly" ? QCoreApplication::translate("ai", "UNUSUAL ROWS ALONG TIME") : QCoreApplication::translate("ai", "GROUPS ALONG TIME"), host));
      m_result->addWidget(new RegimeStrip(as, kind == "anomaly", host));
      if (kind == "anomaly") m_result->addWidget(new ScoreLine(as, host));

      // groups table
      m_result->addSpacing(4);
      m_result->addWidget(micro(QCoreApplication::translate("ai", "THE GROUPS"), host));
      auto* g = new QGridLayout; g->setHorizontalSpacing(14); g->setVerticalSpacing(4);
      QStringList heads{QString(), QCoreApplication::translate("ai", "ROWS"), QCoreApplication::translate("ai", "LASTS"),
                        outcome.isEmpty() ? QString() : QCoreApplication::translate("ai", "WHAT FOLLOWED"), QCoreApplication::translate("ai", "WHAT SETS IT APART")};
      for (int k = 0; k < heads.size(); ++k) g->addWidget(micro(heads[k], host), 0, k);
      int gr = 1;
      for (const auto& gv : res.value("groups").toArray()) {
        const QJsonObject go = gv.toObject();
        const int lab = go.value("label").toInt();
        const QColor col = kind == "anomaly" ? (lab ? c.down : c.textDim) : groupColor(lab);
        g->addWidget(text("■ " + groupName(lab, kind), theme::mono(10, QFont::Bold), col, host), gr, 0, Qt::AlignTop);
        g->addWidget(text(QString("%1  (%2)").arg(go.value("size").toInt()).arg(pct(jnum(go.value("share")), 0)), theme::mono(10), c.text, host), gr, 1, Qt::AlignTop);
        g->addWidget(text(QCoreApplication::translate("ai", "≈ %1 rows").arg(jnum(go.value("avg_stretch")), 0, 'f', 1), theme::mono(10), c.textDim, host), gr, 2, Qt::AlignTop);
        if (!outcome.isEmpty()) g->addWidget(text(outcomeText(go.value("outcome").toObject()), theme::mono(10), c.text, host), gr, 3, Qt::AlignTop);
        QStringList why;
        for (const auto& d : go.value("defining").toArray()) {
          const QJsonObject o = d.toObject();
          if (why.size() >= 3) break;
          const double z = o.value("z").toDouble();
          why << QString("%1 %2%3σ").arg(o.value("feature").toString(), z >= 0 ? "+" : "−").arg(std::abs(z), 0, 'f', 1);
        }
        g->addWidget(text(why.join("  "), theme::mono(10), c.textDim, host, true), gr, 4, Qt::AlignTop);
        ++gr;
      }
      g->setColumnStretch(4, 1);
      m_result->addLayout(g);
      m_result->addWidget(text(QCoreApplication::translate("ai",
          "LASTS: how many rows in a row the group holds on average. WHAT SETS IT APART: the features furthest from their usual level in "
          "this group, in standard deviations (σ).%1").arg(outcome.isEmpty() ? QString() : " " + QCoreApplication::translate("ai",
          "WHAT FOLLOWED: the outcome's average after the group's rows, in the past; groups that differ here are worth a closer look, "
          "not a rule.")), theme::sans(11), c.textMute, host, true));

      const QJsonArray scan = res.value("k_scan").toArray();
      if (!scan.isEmpty()) {
        m_result->addSpacing(4);
        m_result->addWidget(micro(QCoreApplication::translate("ai", "HOW MANY GROUPS? SILHOUETTE FOR EACH k (HIGHER = BETTER SEPARATED)"), host));
        m_result->addWidget(new KScan(scan, job.value("params").toObject().value("n_clusters").toInt(), host));
      }
      if (kind == "anomaly") {
        m_result->addSpacing(4);
        m_result->addWidget(micro(QCoreApplication::translate("ai", "THE MOST UNUSUAL ROWS, AND WHY"), host));
        int shown = 0;
        for (const auto& av : res.value("anomalies").toArray()) {
          if (++shown > 12) break;
          const QJsonObject o = av.toObject();
          QStringList why;
          for (const auto& w : o.value("why").toArray()) {
            const QJsonObject x = w.toObject();
            const double z = x.value("z").toDouble();
            why << QString("%1 %2 (%3%4σ)").arg(x.value("feature").toString()).arg(x.value("value").toDouble(), 0, 'g', 5).arg(z >= 0 ? "+" : "−").arg(std::abs(z), 0, 'f', 1);
          }
          m_result->addWidget(text(o.value("date").toString().left(10) + "   " + why.join("  ·  "), theme::mono(10), c.textDim, host, true));
        }
      }
      m_result->addSpacing(4);
      m_result->addWidget(micro(QCoreApplication::translate("ai", "MAP OF THE ROWS (FIRST TWO PRINCIPAL AXES)"), host));
      m_result->addWidget(new ScatterMap(as, kind == "anomaly" ? ScatterMap::Anomaly : ScatterMap::Groups, host));
    } else {
      // reduction
      const QJsonArray comps = res.value("components").toArray();
      if (!comps.isEmpty()) {
        QList<QPair<QString, double>> ex;
        double sum = 0;
        for (int i = 0; i < comps.size(); ++i) { const double e = comps[i].toObject().value("explained").toDouble(); ex << qMakePair(QString("PC%1").arg(i + 1), e); sum += e; }
        m_result->addWidget(text(QCoreApplication::translate("ai", "%1 axes keep %2 of the information (variance) of %3 features.")
                                     .arg(comps.size()).arg(pct(sum)).arg(res.value("features").toArray().size()), theme::mono(11, QFont::Bold), c.accentText, host, true));
        m_result->addWidget(micro(QCoreApplication::translate("ai", "SHARE OF THE VARIANCE PER AXIS"), host));
        auto* bars = new ImportanceBars(host); bars->setItems(ex);
        m_result->addWidget(bars);
        m_result->addWidget(micro(QCoreApplication::translate("ai", "WHAT EACH AXIS IS MADE OF (LOADINGS)"), host));
        for (int i = 0; i < comps.size(); ++i) {
          QStringList parts;
          for (const auto& t : comps[i].toObject().value("top").toArray())
            parts << QString("%1 %2%3").arg(t.toObject().value("feature").toString(), t.toObject().value("loading").toDouble() >= 0 ? "+" : "−")
                         .arg(std::abs(t.toObject().value("loading").toDouble()), 0, 'f', 2);
          m_result->addWidget(text(QString("PC%1   ").arg(i + 1) + parts.join("  "), theme::mono(10), c.textDim, host, true));
        }
      }
      m_result->addSpacing(4);
      m_result->addWidget(micro(job.value("algorithm").toString() == "tsne" ? QCoreApplication::translate("ai", "t-SNE MAP: NEARBY DOTS ARE SIMILAR ROWS (DISTANCES BETWEEN FAR GROUPS MEAN LITTLE)")
                                                                          : QCoreApplication::translate("ai", "MAP OF THE ROWS (FIRST TWO AXES)"), host));
      m_result->addWidget(new ScatterMap(as, ScatterMap::Time, host));
    }
    m_result->addStretch(1);
  }

  // semi-supervised: how many labels were kept, and the same model with fewer / all labels on the same test rows
  void showRl(const ml::Run& r, QWidget* host) {
    const auto& c = theme::pal();
    const QJsonObject job = r.job, res = r.result, prm = job.value("params").toObject();
    const Algo a = algoById(job.value("algorithm").toString());
    QStringList feats; for (const auto& f : job.value("features").toArray()) feats << f.toString();
    const bool lo = prm.value("positions").toString() == "long_only";
    m_result->addWidget(text(QCoreApplication::translate("ai", "%1 · trades %2 · %3 · costs %4 bp · %5 feature(s) · %6 episodes")
                                 .arg(a.id.isEmpty() ? job.value("algorithm").toString() : a.name, job.value("target").toString(),
                                      lo ? QCoreApplication::translate("ai", "long or flat") : QCoreApplication::translate("ai", "long, short or flat"))
                                 .arg(prm.value("cost_bp").toDouble(), 0, 'f', 1).arg(feats.size()).arg(prm.value("episodes").toInt()),
                             theme::mono(10), c.textDim, host, true));
    if (r.state == ml::RunState::Running) {
      m_result->addWidget(text(QCoreApplication::translate("ai", "Training: %1 (%2%)").arg(r.stage).arg(fmt::roundInt(r.pct)), theme::mono(11), c.warn, host, true));
      m_result->addStretch(1);
      return;
    }
    if (r.state != ml::RunState::Done) {
      m_result->addWidget(text(r.error.isEmpty() ? ml::stateName(r.state) : r.error, theme::mono(11), r.state == ml::RunState::Failed ? c.down : c.textDim, host, true));
      m_result->addStretch(1);
      return;
    }
    const QJsonArray trp = res.value("train_period").toArray(), tep = res.value("test_period").toArray();
    m_result->addWidget(text(QCoreApplication::translate("ai", "Learned on %1 → %2, judged on %3 → %4 (never seen while learning) · simulation, no order sent · %5 s")
                                 .arg(trp.at(0).toString().left(10), trp.at(1).toString().left(10), tep.at(0).toString().left(10), tep.at(1).toString().left(10))
                                 .arg(res.value("seconds").toDouble(), 0, 'f', 1),
                             theme::mono(10), c.textMute, host, true));
    const QJsonObject te = res.value("test").toObject(), tr = res.value("train").toObject(), b = res.value("benchmarks").toObject();
    const QJsonObject rnd = b.value("random").toObject();
    const double beaten = jnum(rnd.value("beaten"));
    const bool beatsHold = jnum(te.value("total_return")) > jnum(b.value("buy_hold").toObject().value("total_return"));
    // green only when it beats both luck and simply holding: in a trend, random agents (half of them short) are an easy bar
    QString verdict;
    if (beaten < 0.95)
      verdict = QCoreApplication::translate("ai", "On the test period it beats %1 of %2 random agents that trade as often: within what luck gives (below 95%).");
    else if (!beatsHold)
      verdict = QCoreApplication::translate("ai", "On the test period it beats %1 of %2 random agents that trade as often, but not buy & hold: it mostly rode the trend.");
    else
      verdict = QCoreApplication::translate("ai", "On the test period it beats %1 of %2 random agents that trade as often, and buy & hold: more than luck or the trend would usually give.");
    m_result->addWidget(text(verdict.arg(pct(beaten, 0)).arg(rnd.value("runs").toInt()), theme::mono(11, QFont::Bold),
                             beaten >= 0.95 && beatsHold ? c.up : c.warn, host, true));

    auto* g = new QGridLayout; g->setHorizontalSpacing(12); g->setVerticalSpacing(3);
    const QStringList heads{QString(), QCoreApplication::translate("ai", "RETURN"), QCoreApplication::translate("ai", "PER YEAR"), "SHARPE",
                            QCoreApplication::translate("ai", "MAX DRAWDOWN"), QCoreApplication::translate("ai", "TRADES"), QCoreApplication::translate("ai", "IN MARKET")};
    for (int k = 0; k < heads.size(); ++k) g->addWidget(micro(heads[k], host), 0, k, k ? Qt::AlignRight : Qt::AlignLeft);
    const QList<QPair<QString, QJsonObject>> rows{
        {QCoreApplication::translate("ai", "Agent, test period"), te},
        {QCoreApplication::translate("ai", "Buy & hold, test period"), b.value("buy_hold").toObject()},
        {QCoreApplication::translate("ai", "Flat, test period"), b.value("flat").toObject()},
        {QCoreApplication::translate("ai", "Agent, training period"), tr}};
    int gr = 1;
    for (const auto& [label, o] : rows) {
      const bool main = gr == 1;
      auto cell = [&](const QString& v, int col) { g->addWidget(text(v, theme::mono(11, main ? QFont::Bold : QFont::Normal), main ? c.text : c.textDim, host), gr, col, Qt::AlignRight); };
      g->addWidget(text(label, theme::sans(11), main ? c.text : c.textDim, host), gr, 0);
      cell(ret(jnum(o.value("total_return"))), 1);
      cell(o.value("annual_return").isDouble() ? pct(jnum(o.value("annual_return"))) : QStringLiteral("—"), 2);
      cell(o.value("sharpe").isDouble() ? QString::number(jnum(o.value("sharpe")), 'f', 2) : QStringLiteral("—"), 3);
      cell(pct(jnum(o.value("max_drawdown"))), 4);
      cell(QString::number(o.value("trades").toInt()), 5);
      cell(pct(jnum(o.value("exposure")), 0), 6);
      ++gr;
    }
    g->addWidget(text(QCoreApplication::translate("ai", "Random agents, test period"), theme::sans(11), c.textDim, host), gr, 0);
    g->addWidget(text(QCoreApplication::translate("ai", "mean %1 · 5–95%: %2 to %3").arg(pct(jnum(rnd.value("mean"))), pct(jnum(rnd.value("p05"))), pct(jnum(rnd.value("p95")))),
                      theme::mono(10), c.textDim, host), gr, 1, 1, 6, Qt::AlignLeft);
    g->setColumnStretch(0, 1);
    m_result->addLayout(g);
    m_result->addWidget(text(QCoreApplication::translate("ai",
        "A training result far above the test result means the agent learned the past by heart. Costs are paid on every position change; "
        "no slippage, no financing, no gaps: real trading would do worse."), theme::sans(11), c.textMute, host, true));
    const QJsonObject lt = res.value("latest").toObject();
    if (!lt.isEmpty()) {
      const QString act = lt.value("action").toString();
      const QString name = act == "long" ? QCoreApplication::translate("ai", "LONG") : act == "short" ? QCoreApplication::translate("ai", "SHORT") : QCoreApplication::translate("ai", "FLAT");
      m_result->addWidget(text(QCoreApplication::translate("ai", "Its choice on the latest row (%1): %2").arg(lt.value("date").toString().left(10), name),
                               theme::mono(11, QFont::Bold), c.accentText, host, true));
      m_result->addWidget(text(QCoreApplication::translate("ai", "A simulated agent's output, not advice: check the test period above first."), theme::sans(10), c.textMute, host, true));
    }
    m_result->addSpacing(4);
    m_result->addWidget(micro(QCoreApplication::translate("ai", "TEST PERIOD: SIMULATED EQUITY"), host));
    m_result->addWidget(new EquityChart(ml::runFile(r, "equity.csv"), host));
    m_result->addSpacing(4);
    m_result->addWidget(micro(QCoreApplication::translate("ai", "LEARNING: RESULT PER EPISODE"), host));
    m_result->addWidget(new RlCurve(res.value("history").toArray(), host));
    m_result->addStretch(1);
  }

  void showSemiBlock(const QJsonObject& res, QWidget* host) {
    const auto& c = theme::pal();
    const QJsonObject sm = res.value("semi").toObject();
    QString t = QCoreApplication::translate("ai", "Labels kept: %1 training rows (%2); %3 labels hidden").arg(sm.value("labelled").toInt())
                    .arg(pct(jnum(sm.value("share")), 1)).arg(sm.value("hidden").toInt());
    if (sm.value("blank").toInt() > 0) t += " · " + QCoreApplication::translate("ai", "%1 rows without a label in the file").arg(sm.value("blank").toInt());
    if (sm.contains("pseudo_labelled"))
      t += " · " + QCoreApplication::translate("ai", "it labelled %1 rows itself in %2 round(s)").arg(sm.value("pseudo_labelled").toInt()).arg(sm.value("iterations").toInt());
    m_result->addWidget(text(t, theme::mono(10), c.textDim, host, true));
    auto* g = new QGridLayout; g->setHorizontalSpacing(16); g->setVerticalSpacing(3);
    const QStringList heads{QCoreApplication::translate("ai", "ON THE SAME TEST ROWS"), QCoreApplication::translate("ai", "BALANCED ACC."), "ROC AUC"};
    for (int k = 0; k < heads.size(); ++k) g->addWidget(micro(heads[k], host), 0, k, k ? Qt::AlignRight : Qt::AlignLeft);
    const QList<QPair<QString, QJsonObject>> rows{
        {QCoreApplication::translate("ai", "This model: kept labels + unlabelled rows"), res.value("test").toObject()},
        {QCoreApplication::translate("ai", "Same model, kept labels only"), sm.value("labels_only").toObject()},
        {QCoreApplication::translate("ai", "Same model, every label (upper bound)"), sm.value("all_labels").toObject()},
        {QCoreApplication::translate("ai", "No skill"), res.value("baseline").toObject()}};
    for (int i = 0; i < rows.size(); ++i) {
      g->addWidget(text(rows[i].first, theme::sans(11), i == 0 ? c.text : c.textDim, host), i + 1, 0);
      g->addWidget(text(pct(jnum(rows[i].second.value("balanced_accuracy"))), theme::mono(11, i == 0 ? QFont::Bold : QFont::Normal), i == 0 ? c.text : c.textDim, host), i + 1, 1, Qt::AlignRight);
      g->addWidget(text(rows[i].second.contains("roc_auc") ? pct(jnum(rows[i].second.value("roc_auc"))) : QStringLiteral("—"), theme::mono(11), c.textDim, host), i + 1, 2, Qt::AlignRight);
    }
    g->setColumnStretch(0, 1);
    m_result->addLayout(g);
    m_result->addWidget(text(QCoreApplication::translate("ai", "The unlabelled rows help when this model beats \"kept labels only\"; the closer it gets to \"every label\", the more they gave back."),
                             theme::sans(11), c.textMute, host, true));
    m_result->addSpacing(6);
  }

  void showSelf(const ml::Run& r, QWidget* host) {
    const auto& c = theme::pal();
    const QJsonObject job = r.job, res = r.result, prm = job.value("params").toObject();
    const Algo a = algoById(job.value("algorithm").toString());
    QStringList feats; for (const auto& f : job.value("features").toArray()) feats << f.toString();
    m_result->addWidget(text(QCoreApplication::translate("ai", "%1 · %2 feature(s) · windows of %3 rows · embedding of %4 · %5 epochs")
                                 .arg(a.id.isEmpty() ? job.value("algorithm").toString() : a.name).arg(feats.size())
                                 .arg(prm.value("window").toInt()).arg(prm.value("embedding").toInt()).arg(prm.value("epochs").toInt()),
                             theme::mono(10), c.textDim, host, true));
    if (r.state == ml::RunState::Running) {
      m_result->addWidget(text(QCoreApplication::translate("ai", "Training: %1 (%2%)").arg(r.stage).arg(fmt::roundInt(r.pct)), theme::mono(11), c.warn, host, true));
      m_result->addStretch(1);
      return;
    }
    if (r.state != ml::RunState::Done) {
      m_result->addWidget(text(r.error.isEmpty() ? ml::stateName(r.state) : r.error, theme::mono(11), r.state == ml::RunState::Failed ? c.down : c.textDim, host, true));
      m_result->addStretch(1);
      return;
    }
    const QJsonArray per = res.value("period").toArray(), pre = res.value("pretrain_period").toArray();
    m_result->addWidget(text(QCoreApplication::translate("ai", "%1 windows (%2 → %3); learned on the windows up to %4 only, no target used · %5 s")
                                 .arg(res.value("windows").toInt()).arg(per.at(0).toString().left(10), per.at(1).toString().left(10), pre.at(1).toString().left(10))
                                 .arg(res.value("seconds").toDouble(), 0, 'f', 1),
                             theme::mono(10), c.textMute, host, true));
    m_result->addSpacing(4);
    m_result->addWidget(micro(QCoreApplication::translate("ai", "LEARNING: PRETEXT LOSS PER EPOCH (LOWER = BETTER)"), host));
    m_result->addWidget(new LossChart(res.value("history").toArray(), host));

    const QJsonObject pr = res.value("probe").toObject();
    if (!pr.isEmpty()) {
      const bool cls = pr.value("task").toString() == "classification";
      m_result->addSpacing(4);
      m_result->addWidget(micro(QCoreApplication::translate("ai", "PROBE: %1 OF %2 OVER %3 ROW(S), LINEAR MODEL, %4 LATER WINDOWS")
                                    .arg(transformLabel(pr.value("transform").toString()).toUpper(), pr.value("target").toString().toUpper())
                                    .arg(pr.value("horizon").toInt()).arg(pr.value("test_windows").toInt()), host));
      auto* g = new QGridLayout; g->setHorizontalSpacing(16); g->setVerticalSpacing(3);
      const QStringList heads = cls ? QStringList{QString(), QCoreApplication::translate("ai", "BALANCED ACC."), "ROC AUC"}
                                    : QStringList{QString(), QCoreApplication::translate("ai", "MEAN ABS. ERROR"), QCoreApplication::translate("ai", "RIGHT DIRECTION")};
      for (int k = 0; k < heads.size(); ++k) g->addWidget(micro(heads[k], host), 0, k, k ? Qt::AlignRight : Qt::AlignLeft);
      const QList<QPair<QString, QString>> rows{{QCoreApplication::translate("ai", "On what it learned (embeddings)"), "embeddings"},
                                                {QCoreApplication::translate("ai", "On the raw features of the last row"), "raw_now"},
                                                {QCoreApplication::translate("ai", "On the raw window (every row of it)"), "raw_window"},
                                                {QCoreApplication::translate("ai", "No skill"), "no_skill"}};
      for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject o = pr.value(rows[i].second).toObject();
        const QString v1 = cls ? pct(jnum(o.value("balanced_accuracy"))) : fixed(jnum(o.value("mae")));
        const QString v2 = cls ? (o.contains("roc_auc") ? pct(jnum(o.value("roc_auc"))) : QStringLiteral("—"))
                               : (o.contains("direction_accuracy") ? pct(jnum(o.value("direction_accuracy"))) : QStringLiteral("—"));
        g->addWidget(text(rows[i].first, theme::sans(11), i == 0 ? c.text : c.textDim, host), i + 1, 0);
        g->addWidget(text(v1, theme::mono(11, i == 0 ? QFont::Bold : QFont::Normal), i == 0 ? c.text : c.textDim, host), i + 1, 1, Qt::AlignRight);
        g->addWidget(text(v2, theme::mono(11), c.textDim, host), i + 1, 2, Qt::AlignRight);
      }
      g->setColumnStretch(0, 1);
      m_result->addLayout(g);
      m_result->addWidget(text(QCoreApplication::translate("ai", "What it learned is useful for this target when the first row beats the raw features; all of them must beat \"no skill\" to mean anything."),
                               theme::sans(11), c.textMute, host, true));
    }
    const QJsonArray un = res.value("unusual").toArray();
    if (!un.isEmpty()) {
      m_result->addSpacing(4);
      m_result->addWidget(micro(QCoreApplication::translate("ai", "WINDOWS IT FOUND HARDEST TO REBUILD OR PREDICT (TYPICAL ERROR %1)").arg(jnum(res.value("error_median")), 0, 'f', 3), host));
      QStringList l;
      for (int i = 0; i < un.size() && i < 12; ++i)
        l << un[i].toObject().value("date").toString().left(10) + "  " + QString::number(un[i].toObject().value("error").toDouble(), 'f', 2);
      m_result->addWidget(text(l.join("   ·   "), theme::mono(10), c.textDim, host, true));
    }
    m_result->addSpacing(4);
    m_result->addWidget(micro(QCoreApplication::translate("ai", "MAP OF THE EMBEDDINGS (FIRST TWO AXES)"), host));
    m_result->addWidget(new ScatterMap(readAssign(ml::runFile(r, "assignments.csv")), ScatterMap::Time, host));
    m_result->addWidget(text(QCoreApplication::translate("ai", "Every window's embedding is in assignments.csv (emb_0, emb_1…), the encoder in encoder.pt: FOLDER, top right."),
                             theme::sans(11), c.textMute, host, true));
    m_result->addStretch(1);
  }

  // ---------------------------------------------------------------- test hooks
  void applyHooks() {
    // DW_ML_PICK=<symbol>: 3 s after opening, picks that market in the list as a click would, tries TRAIN at once (must be
    // refused: its data is not there yet), then trains once it has loaded (with DW_ML_TRAIN's algorithm). Logs to stderr.
    if (qEnvironmentVariableIsSet("DW_ML_PICK")) {
      QTimer::singleShot(3000, this, [this] {
        // "<symbol>" or "<prices|macro|cot>:<key>[:<1d|1wk|1h>]": source, then key, then interval, each as a click would
        QStringList pk = qEnvironmentVariable("DW_ML_PICK").split(':');
        if (pk.size() == 1) pk.prepend("prices");
        const int src = int(QStringList{"prices", "macro", "cot", "csv", "technical-data"}.indexOf(pk[0] == "markets" || pk[0] == "charts-data" ? "technical-data" : pk[0]));
        if (src < 0) return;
        qInfo("ml-pick: before: loaded=%s", qPrintable(m_data.title));
        // DW_ML_UNTICK=<column>: untick that feature first, as a click would (it must stay unticked after the switch)
        if (qEnvironmentVariableIsSet("DW_ML_UNTICK"))
          for (int i = 0; i < m_features->count(); ++i)
            if (m_features->item(i)->text() == qEnvironmentVariable("DW_ML_UNTICK")) m_features->item(i)->setCheckState(Qt::Unchecked);
        if (src != m_source->current()) m_source->setCurrent(src, true);
        if (src == 3) {   // what CHOOSE FILE… does with the file picked
          m_csv = pk.mid(1).join(':');
          { const QSignalBlocker block(m_key); m_key->clear(); m_key->addItem(QFileInfo(m_csv).fileName(), m_csv); }
          loadData();
        }
        const int k = m_key->findData(src == 3 ? m_csv : pk.value(1));
        if (k < 0) { qInfo("ml-pick: unknown key %s", qPrintable(pk.value(1))); return; }
        m_key->setCurrentIndex(k);
        if (src == 0 && pk.size() > 2) m_interval->setCurrent(qMax(0, int(QStringList{"1d", "1wk", "1h"}.indexOf(pk[2]))), true);
        qInfo("ml-pick: right after the pick: TRAIN enabled=%d, loaded=%s", int(m_train->isEnabled()), qPrintable(m_data.title));
        train();   // a click on TRAIN at that moment
        qInfo("ml-pick: runs after an early TRAIN: %d", int(ml::Runs::instance().list().size()));
        auto* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this, t] {
          if (m_loading || !m_data.ok()) return;
          t->deleteLater();
          const QStringList tr = qEnvironmentVariable("DW_ML_TRAIN").split(':');
          for (int i = 0; i < m_algo->count(); ++i) if (m_algo->item(i)->data(Qt::UserRole).toString() == tr.value(0)) m_algo->setCurrentRow(i);
          qInfo("ml-pick: loaded=%s, TRAIN enabled=%d (%s)", qPrintable(m_data.title), int(m_train->isEnabled()), qPrintable(m_trainInfo->text()));
          qInfo("ml-pick: features ticked after the switch: %s", qPrintable(checkedFeatures().join(",")));
          QStringList all, targets;
          for (int i = 0; i < m_features->count(); ++i) all << m_features->item(i)->text();
          for (int i = 0; i < m_target->count(); ++i) targets << m_target->itemData(i).toString();
          qInfo("ml-pick: feature list: %s", qPrintable(all.join(",")));
          qInfo("ml-pick: target list: %s | chosen: %s", qPrintable(targets.join(",")), qPrintable(m_target->currentData().toString()));
          train();
        });
        t->start(300);
      });
      return;
    }
    if (!qEnvironmentVariableIsSet("DW_ML_LOAD")) {
      if (qEnvironmentVariableIsSet("DW_ML_RUN")) {
        const QString id = m_runList->idAt(qEnvironmentVariableIntValue("DW_ML_RUN"));
        if (!id.isEmpty()) { m_runList->select(id); showRun(id); }
      }
      return;
    }
    const QStringList p = qEnvironmentVariable("DW_ML_LOAD").split(':');
    const int src = QStringList{"prices", "macro", "cot", "csv", "technical-data"}.indexOf(p.value(0) == "markets" || p.value(0) == "charts-data" ? "technical-data" : p.value(0));
    if (src < 0) return;
    m_source->setCurrent(src); fillKeys();
    if (src == 3) { m_csv = p.mid(1).join(':'); m_key->clear(); m_key->addItem(QFileInfo(m_csv).fileName(), m_csv); }
    else { QSignalBlocker b(m_key); const int k = m_key->findData(p.value(1)); if (k >= 0) m_key->setCurrentIndex(k); }
    if (src == 0) m_interval->setCurrent(qMax(0, int(QStringList{"1d", "1wk", "1h"}.indexOf(p.value(2, "1d")))));
    ml::DatasetSpec spec{src == 0 ? ml::SourceKind::Prices : src == 1 ? ml::SourceKind::Macro : src == 2 ? ml::SourceKind::Cot : src == 4 ? ml::SourceKind::TechnicalData : ml::SourceKind::Csv,
                         m_key->currentData().toString(), p.value(2, "1d")};
    if (src == 4) {   // DW_ML_LOAD=markets:BINANCE:BTCUSDT … the key has a colon: markets:<exchange>:<pair>:<tf>[:<rows>]
      spec.key = p.value(1) + ":" + p.value(2);
      spec.interval = p.value(3, "1h");
      spec.rows = p.value(4, "0").toInt();
      { QSignalBlocker b(m_key); const int k = m_key->findData(spec.key); if (k >= 0) m_key->setCurrentIndex(k); }
      int ti = 0;
      for (int i = 0; i < technicaldata::timeframes().size(); ++i) if (technicaldata::timeframes()[i].name == spec.interval) ti = i;
      { QSignalBlocker b(m_marketTf); m_marketTf->setCurrent(ti); }
      if (spec.rows > 0) { QSignalBlocker b(m_marketRows); const int k = m_marketRows->findData(spec.rows); if (k >= 0) m_marketRows->setCurrentIndex(k); }
    }
    m_dataInfo->setText(QCoreApplication::translate("ai", "Loading…"));
    const int ticket = ++m_loadTicket;
    const QString want = selectionKey();
    ml::build(spec, this, [this, ticket, want](const ml::Dataset& d) {
      if (ticket != m_loadTicket) return;
      m_loadedKey = d.ok() ? want : QString();
      setDataset(d);
      if (!qEnvironmentVariableIsSet("DW_ML_TRAIN")) return;
      const QStringList t = qEnvironmentVariable("DW_ML_TRAIN").split(':');
      for (int i = 0; i < m_algo->count(); ++i) if (m_algo->item(i)->data(Qt::UserRole).toString() == t.value(0)) m_algo->setCurrentRow(i);
      if (t.size() > 1) m_target->setCurrentIndex(qMax(0, m_target->findData(t[1] == "-" ? QString() : t[1])));
      if (t.size() > 2) m_transform->setCurrentIndex(qMax(0, m_transform->findData(t[2])));
      if (t.size() > 3) m_horizon->setValue(t[3].toInt());
      // wait for the environment check, then train once
      auto* timer = new QTimer(this);
      connect(timer, &QTimer::timeout, this, [this, timer] {
        if (!ml::Environment::instance().status().probed) return;
        timer->deleteLater();
        if (m_train->isEnabled()) train();
      });
      timer->start(300);
    });
  }

  QLabel *m_env, *m_installLine, *m_familyDesc, *m_later;
  QPushButton* m_install;
  ui::PillBar* m_family;
  QStackedWidget* m_stack;
  // data
  ui::PillBar *m_source = nullptr, *m_interval = nullptr, *m_marketTf = nullptr;
  QComboBox* m_marketRows = nullptr;
  QComboBox *m_key = nullptr, *m_target = nullptr, *m_transform = nullptr, *m_task = nullptr;
  QPushButton *m_choose = nullptr, *m_load = nullptr, *m_train = nullptr, *m_delete = nullptr, *m_folder = nullptr;
  QLabel *m_dataInfo = nullptr, *m_targetHint = nullptr, *m_algoDesc = nullptr, *m_trainInfo = nullptr;
  QSpinBox *m_horizon = nullptr, *m_lags = nullptr, *m_test = nullptr, *m_folds = nullptr;
  QListWidget *m_features = nullptr, *m_algo = nullptr;
  QWidget* m_paramsHost = nullptr;
  QGridLayout* m_params = nullptr;
  QHash<QString, QWidget*> m_paramEdits;
  QLineEdit* m_name = nullptr;
  QProgressBar* m_progress = nullptr;
  RunList* m_runList = nullptr;
  QString m_shownRun;   // runSignature() of what the result panel shows
  QLabel* m_targetLabel = nullptr;
  QLabel* m_ofLabel = nullptr;
  QList<QWidget*> m_supOnly;
  int m_algoFamily = -1;
  QScrollArea* m_runScroll = nullptr;
  QVBoxLayout* m_result = nullptr;
  ml::Dataset m_data;
  QString m_loadedKey;   // selectionKey() of m_data
  QHash<QString, bool> m_featureChoice;   // feature column → ticked, as the user left it (kept across loads)
  bool m_loading = false;
  QString m_csv;
  int m_loadTicket = 0;
};
}  // namespace
}  // namespace aipage

using MlPageImpl = aipage::MlPage;
DW_PAGE("ai/ml", MlPageImpl)
