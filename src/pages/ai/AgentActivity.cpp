#include "pages/ai/AgentActivity.h"
#include "ai/Engine.h"
#include "ai/Slots.h"
#include "ml/Runs.h"
#include "ui/Theme.h"
#include "core/Settings.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QRegularExpression>
#include <algorithm>
#include <climits>
#include <cmath>

namespace aipage {
namespace {
constexpr int kPad = 12;
// The "3D" view of the trainings (paint3D: standing cards) is put aside for now (user, 2026-11-19): no box, the stack is
// flat. true brings the box back, with the choice saved in ai.activity3d.
constexpr bool kOffer3D = false;

// QObject::tr would use the context "QObject" inside the widget (no Q_OBJECT): every string here is in "ai"
QString T(const char* s) { return QCoreApplication::translate("ai", s); }

}  // namespace

// What a tool call does, in a few words (the tool's own name for the ones not listed).
QString toolLabel(const QString& name) {
  static const QHash<QString, const char*> labels{
      {"ml_status", QT_TRANSLATE_NOOP("ai", "checks the ML environment")},
      {"ml_methods", QT_TRANSLATE_NOOP("ai", "reads the ML methods")},
      {"ml_datasets", QT_TRANSLATE_NOOP("ai", "lists the data sets")},
      {"ml_load_dataset", QT_TRANSLATE_NOOP("ai", "builds a data set")},
      {"ml_read_dataset", QT_TRANSLATE_NOOP("ai", "reads a data set")},
      {"ml_train", QT_TRANSLATE_NOOP("ai", "starts a training")},
      {"ml_runs", QT_TRANSLATE_NOOP("ai", "lists the trainings")},
      {"ml_run", QT_TRANSLATE_NOOP("ai", "reads a training's result")},
      {"ml_run_rows", QT_TRANSLATE_NOOP("ai", "reads a training's predictions")},
      {"ml_compare", QT_TRANSLATE_NOOP("ai", "compares trainings")},
      {"bt_indicators", QT_TRANSLATE_NOOP("ai", "reads the indicators")},
      {"bt_run", QT_TRANSLATE_NOOP("ai", "backtests a strategy")},
      {"bt_quant_checks", QT_TRANSLATE_NOOP("ai", "runs the quant checks")},
      {"bt_trials", QT_TRANSLATE_NOOP("ai", "reads its past backtests")},
      {"get_quotes", QT_TRANSLATE_NOOP("ai", "reads live prices")},
      {"get_price_history", QT_TRANSLATE_NOOP("ai", "reads past prices")},
      {"get_economic_calendar", QT_TRANSLATE_NOOP("ai", "reads the calendar")},
      {"get_news", QT_TRANSLATE_NOOP("ai", "reads the news")},
      {"get_squawk", QT_TRANSLATE_NOOP("ai", "reads the squawk")},
      {"get_macro_data", QT_TRANSLATE_NOOP("ai", "reads macro data")},
      {"get_central_banks", QT_TRANSLATE_NOOP("ai", "reads the central banks")},
      {"get_cot", QT_TRANSLATE_NOOP("ai", "reads the COT positions")},
      {"get_retail_sentiment", QT_TRANSLATE_NOOP("ai", "reads retail sentiment")},
      {"get_risk_sentiment", QT_TRANSLATE_NOOP("ai", "reads risk sentiment")},
      {"get_bank_forecasts", QT_TRANSLATE_NOOP("ai", "reads bank forecasts")},
      {"get_bank_research", QT_TRANSLATE_NOOP("ai", "reads bank research")},
      {"get_market_hours", QT_TRANSLATE_NOOP("ai", "reads market hours")},
      {"get_technical_read", QT_TRANSLATE_NOOP("ai", "reads the technical picture")},
      {"get_liquidity_pools", QT_TRANSLATE_NOOP("ai", "reads liquidity pools")},
      {"get_seasonality", QT_TRANSLATE_NOOP("ai", "reads seasonality")},
      {"get_fundamental_scores", QT_TRANSLATE_NOOP("ai", "reads fundamental scores")},
      {"get_macro_chains", QT_TRANSLATE_NOOP("ai", "reads macro chains")},
      {"get_macro_facts", QT_TRANSLATE_NOOP("ai", "reads macro facts")},
      {"get_technical_coverage", QT_TRANSLATE_NOOP("ai", "reads what technical data exists")},
      {"get_technical_data", QT_TRANSLATE_NOOP("ai", "reads technical data")},
      {"get_footprint", QT_TRANSLATE_NOOP("ai", "reads the footprint")},
      {"get_order_book", QT_TRANSLATE_NOOP("ai", "reads the order book")},
      {"get_open_interest", QT_TRANSLATE_NOOP("ai", "reads the open interest")},
      {"get_track_record", QT_TRANSLATE_NOOP("ai", "reads the track record")},
  };
  const auto it = labels.constFind(name);
  return it == labels.cend() ? name : QCoreApplication::translate("ai", *it);
}

namespace {

// The trainer's stage ("cv 2/5", "epoch 3/20 · loss 0.0123", "fit") in words.
QString stageLabel(const QString& s) {
  static const QRegularExpression cv("^cv (\\d+)/(\\d+)$");
  static const QRegularExpression epoch("^epoch (\\d+)/(\\d+) · loss (\\S+)$");
  static const QRegularExpression episode("^episode (\\d+)/(\\d+) · train (\\S+)$");
  if (const auto m = cv.match(s); m.hasMatch()) return T("cross-validation %1/%2").arg(m.captured(1), m.captured(2));
  if (const auto m = epoch.match(s); m.hasMatch()) return T("epoch %1/%2 · loss %3").arg(m.captured(1), m.captured(2), m.captured(3));
  if (const auto m = episode.match(s); m.hasMatch()) return T("episode %1/%2 · train %3").arg(m.captured(1), m.captured(2), m.captured(3));
  static const QHash<QString, const char*> words{
      {"load", QT_TRANSLATE_NOOP("ai", "loading the data")}, {"prepare", QT_TRANSLATE_NOOP("ai", "preparing")},
      {"fit", QT_TRANSLATE_NOOP("ai", "fitting")}, {"evaluate", QT_TRANSLATE_NOOP("ai", "evaluating")},
      {"save", QT_TRANSLATE_NOOP("ai", "saving")}, {"done", QT_TRANSLATE_NOOP("ai", "done")},
      {"embed", QT_TRANSLATE_NOOP("ai", "embedding")}, {"probe", QT_TRANSLATE_NOOP("ai", "probing")},
      {"measure", QT_TRANSLATE_NOOP("ai", "measuring")}, {"compare", QT_TRANSLATE_NOOP("ai", "comparing")}};
  const auto it = words.constFind(s);
  return it == words.cend() ? s : QCoreApplication::translate("ai", *it);
}

QString familyLabel(const QString& f) {
  if (f == "unsupervised") return T("unsupervised");
  if (f == "semi_supervised") return T("semi-supervised");
  if (f == "self_supervised") return T("self-supervised");
  if (f == "reinforcement") return T("reinforcement");
  return T("supervised");
}

double num(const QJsonValue& v) { return v.isDouble() ? v.toDouble() : NAN; }
QString pct(double v, int dec = 1) { return std::isfinite(v) ? QLocale().toString(v * 100, 'f', dec) + QChar(0x00a0) + "%" : QStringLiteral("—"); }
QString dec(double v, int d = 4) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  if (std::abs(v) < 0.5 * std::pow(10.0, -d)) v = 0;   // "-0,00" is 0
  return QLocale().toString(v, 'f', d);
}

// The run's outcome in a line or two (what ml_run's headline says to the model, for the user).
QString resultText(const ml::Run& r) {
  const QJsonObject res = r.result;
  const QString fam = res.value("family").toString(r.job.value("family").toString());
  if (fam == "reinforcement") {
    const QJsonObject b = res.value("benchmarks").toObject();
    return T("Test: return %1 · buy & hold %2").arg(pct(num(res.value("test").toObject().value("total_return"))),
                                                     pct(num(b.value("buy_hold").toObject().value("total_return"))));
  }
  if (fam == "unsupervised") {
    const QString k = res.value("kind").toString();
    if (k == "clustering")
      return T("%1 groups · silhouette %2").arg(res.value("groups").toArray().size()).arg(dec(num(res.value("quality").toObject().value("silhouette")), 2));
    if (k == "anomaly") return T("%1 of the rows unusual").arg(pct(num(res.value("anomaly_share"))));
    return T("Reduced to %1 axes").arg(res.value("components").toArray().size());
  }
  if (fam == "self_supervised") {
    const QJsonArray h = res.value("history").toArray();
    if (h.isEmpty()) return T("Done");
    return T("Loss %1 → %2").arg(dec(h.first().toObject().value("train").toDouble(), 3), dec(h.last().toObject().value("train").toDouble(), 3));
  }
  const QJsonObject te = res.value("test").toObject(), base = res.value("baseline").toObject();
  if (res.value("task").toString() == "classification")
    return T("Test: balanced accuracy %1 · no skill %2").arg(pct(num(te.value("balanced_accuracy"))), pct(num(base.value("balanced_accuracy"))));
  return T("Test: error (MAE) %1 · no skill %2").arg(dec(num(te.value("mae"))), dec(num(base.value("mae"))));
}

struct CurveInfo { QString title, axis; bool percent = false; };
CurveInfo curveInfo(const QString& c) {
  if (c == "cv_mae") return {T("Error of each validation fold (MAE)"), T("CV fold"), false};
  if (c == "cv_balanced_accuracy") return {T("Balanced accuracy of each validation fold"), T("CV fold"), true};
  if (c == "loss") return {T("Loss of each epoch"), T("epoch"), false};
  if (c == "return") return {T("Return of each episode"), T("episode"), true};
  return {c, QString(), false};
}
QString lineLabel(const QString& k) {
  if (k == "train") return T("train");
  if (k == "valid") return T("validation");
  if (k == "test") return T("test");
  if (k == "fold") return T("CV fold");
  return k;
}

QString clock(qint64 secs) {
  return secs >= 3600 ? QString("%1:%2:%3").arg(secs / 3600).arg(secs / 60 % 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0'))
                      : QString("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0'));
}

// A section title: small capitals and a rule under them. Returns the y below.
int section(QPainter& p, int y, int w, const QString& title, const QString& right = {}, const QColor& rightColor = {}) {
  const auto& c = theme::pal();
  p.setFont(theme::mono(9, QFont::Bold, 1.4));
  p.setPen(c.textMute);
  p.drawText(QRect(kPad, y, w - 2 * kPad, 14), Qt::AlignLeft | Qt::AlignVCenter, title);
  if (!right.isEmpty()) {
    p.setPen(rightColor.isValid() ? rightColor : c.textDim);
    p.drawText(QRect(kPad, y, w - 2 * kPad, 14), Qt::AlignRight | Qt::AlignVCenter, right);
  }
  p.setPen(c.divider);
  p.drawLine(kPad, y + 18, w - kPad, y + 18);
  return y + 26;
}

// Lines of the trainer's curve in r: each value key a line, x along the run.
void drawCurve(QPainter& p, const QRect& box, const ml::Run& r) {
  const auto& c = theme::pal();
  const CurveInfo info = curveInfo(r.curve);
  QStringList keys;
  double x0 = INFINITY, x1 = -INFINITY, y0 = INFINITY, y1 = -INFINITY;
  for (const auto& pt : r.points) {
    const double x = pt.value("x").toDouble();
    x0 = std::min(x0, x); x1 = std::max(x1, x);
    const QJsonObject v = pt.value("values").toObject();
    for (auto it = v.begin(); it != v.end(); ++it) {
      if (!keys.contains(it.key())) keys << it.key();
      if (!it.value().isDouble()) continue;
      y0 = std::min(y0, it.value().toDouble()); y1 = std::max(y1, it.value().toDouble());
    }
  }
  // lines in a fixed order (JSON objects sort their keys): what it learns on first, then what checks it
  static const QStringList order{"train", "valid", "test", "fold"};
  std::stable_sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {
    const auto ia = order.indexOf(a), ib = order.indexOf(b);
    return (ia < 0 ? 99 : ia) < (ib < 0 ? 99 : ib);
  });
  p.setFont(theme::sans(11));
  p.setPen(c.textDim);
  p.drawText(QRect(box.left(), box.top(), box.width(), 16), Qt::AlignLeft | Qt::AlignVCenter,
             p.fontMetrics().elidedText(info.title, Qt::ElideRight, box.width()));
  const QRect plot = box.adjusted(0, 22, 0, -16);
  p.setPen(c.border);
  p.setBrush(c.panelAlt);
  p.drawRect(plot.adjusted(0, 0, -1, -1));
  if (!std::isfinite(y0)) return;
  if (y1 - y0 < 1e-12) { y0 -= 0.5 * std::max(std::abs(y0), 1e-6); y1 += 0.5 * std::max(std::abs(y1), 1e-6); }
  const double pad = (y1 - y0) * 0.08; y0 -= pad; y1 += pad;
  const QRectF in = QRectF(plot).adjusted(6, 6, -6, -6);
  auto X = [&](double x) { return x1 > x0 ? in.left() + (x - x0) / (x1 - x0) * in.width() : in.center().x(); };
  auto Y = [&](double y) { return in.bottom() - (y - y0) / (y1 - y0) * in.height(); };
  if (info.percent && y0 < 0 && y1 > 0) {   // returns: the zero line
    p.setPen(QPen(c.grid, 1, Qt::DashLine));
    p.drawLine(QPointF(in.left(), Y(0)), QPointF(in.right(), Y(0)));
  }
  const QList<QColor> colors{c.up, c.accentText, c.warn, c.down};
  p.setRenderHint(QPainter::Antialiasing);
  for (int k = 0; k < keys.size(); ++k) {
    // far more points than pixel columns (500 episodes): each column's lowest and highest point, the same picture
    QPainterPath path;
    bool open = false;
    QPointF last;
    int col = INT_MIN;
    double lo = 0, hi = 0;
    const bool reduce = r.points.size() > 2 * in.width();
    auto flush = [&] { if (col != INT_MIN) { path.lineTo(col, lo); path.lineTo(col, hi); path.lineTo(last); } };
    for (const auto& pt : r.points) {
      const QJsonValue v = pt.value("values").toObject().value(keys[k]);
      if (!v.isDouble()) { if (reduce) flush(); col = INT_MIN; open = false; continue; }
      const QPointF q(X(pt.value("x").toDouble()), Y(v.toDouble()));
      if (reduce && open && int(q.x()) == col) { lo = std::min(lo, q.y()); hi = std::max(hi, q.y()); last = q; continue; }
      if (reduce && open) flush();
      last = q;
      if (open) path.lineTo(last); else { path.moveTo(last); open = true; }
      col = int(q.x()); lo = hi = q.y();
    }
    if (reduce) flush();
    p.setPen(QPen(colors[k % colors.size()], 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setBrush(colors[k % colors.size()]);
    p.setPen(Qt::NoPen);
    p.drawEllipse(last, 2.5, 2.5);
  }
  p.setRenderHint(QPainter::Antialiasing, false);
  // scale: top and bottom values, the axis and the last x, the legend
  auto fmt = [&](double v) { return info.percent ? pct(v) : dec(v, std::abs(y1 - y0) < 0.1 ? 4 : 2); };
  p.setFont(theme::mono(9));
  p.setPen(c.textMute);
  p.drawText(QRect(plot.left() + 4, plot.top() + 2, plot.width() - 8, 12), Qt::AlignLeft | Qt::AlignTop, fmt(y1));
  p.drawText(QRect(plot.left() + 4, plot.bottom() - 13, plot.width() - 8, 12), Qt::AlignLeft | Qt::AlignBottom, fmt(y0));
  p.drawText(QRect(box.left(), plot.bottom() + 3, box.width(), 12), Qt::AlignLeft | Qt::AlignVCenter,
             QString("%1 %2").arg(info.axis).arg(x1));
  int lx = box.right();
  for (int k = int(keys.size()) - 1; k >= 0 && keys.size() > 1; --k) {   // a legend only when there are lines to tell apart
    const QString t = lineLabel(keys[k]);
    const int tw = p.fontMetrics().horizontalAdvance(t);
    p.setPen(c.textDim);
    p.drawText(QRect(lx - tw, plot.bottom() + 3, tw, 12), Qt::AlignRight | Qt::AlignVCenter, t);
    p.fillRect(QRect(lx - tw - 12, plot.bottom() + 8, 8, 2), colors[k % colors.size()]);
    lx -= tw + 20;
  }
}
}  // namespace

AgentActivity::AgentActivity(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(kMinWidth);   // as wide as the user drags it (ChatView's splitter); 300 by default
  setFocusPolicy(Qt::ClickFocus);   // a click on the panel, then ← →
  m_3d = kOffer3D && settings::value("ai.activity3d", false).toBool();
  m_tick.setInterval(500);
  connect(&m_tick, &QTimer::timeout, this, qOverload<>(&QWidget::update));
  m_stop = new QPushButton(T("STOP"), this);
  m_stop->setCursor(Qt::PointingHandCursor);
  m_stop->setFont(theme::mono(9, QFont::Bold, 1.2));
  const auto& c = theme::pal();
  m_stop->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 3px; padding: 2px 8px; }"
                                "QPushButton:hover { background: %3; }")
                            .arg(c.down.name(), theme::alpha(c.down, 150).name(QColor::HexArgb), theme::alpha(c.down, 40).name(QColor::HexArgb)));
  m_stop->setToolTip(T("Stop this training"));
  m_stop->hide();
  connect(m_stop, &QPushButton::clicked, this, [] { ml::Runs::instance().stop(); });
  m_settle.setSingleShot(true);
  m_settle.setInterval(220);
  connect(&m_settle, &QTimer::timeout, this, [this] { m_fast = false; sync(); update(); prefetch(); });
  m_anim = new QVariantAnimation(this);
  m_anim->setDuration(240);
  m_anim->setEasingCurve(QEasingCurve::OutCubic);
  m_anim->setStartValue(0.0);
  m_anim->setEndValue(1.0);
  connect(m_anim, &QVariantAnimation::valueChanged, this, qOverload<>(&QWidget::update));

  connect(m_anim, &QVariantAnimation::finished, this, [this] { m_fromPic = m_toPic = QPixmap(); sync(); update(); prefetch(); });
  auto& runs = ml::Runs::instance();
  connect(&runs, &ml::Runs::listChanged, this, [this] { sync(); update(); });
  connect(&runs, &ml::Runs::runChanged, this, [this](const QString& id) { if (runIds().contains(id)) { sync(); update(); } });
}

void AgentActivity::setEnvironment(int index) {
  for (const auto& k : m_conns) disconnect(k);
  m_conns.clear();
  if (index != m_index) { m_pos = 0; m_newest.clear(); m_anim->stop(); }
  m_index = index;
  if (ai::Engine* e = ai::Engines::instance().at(index)) {
    for (auto sig : {&ai::Engine::stateChanged, &ai::Engine::messagesChanged, &ai::Engine::toolActivity})
      m_conns << connect(e, sig, this, [this] { sync(); update(); });
  }
  sync();
  update();
}

QStringList AgentActivity::runIds() const {
  QStringList out;
  if (m_index < 0) return out;
  const QString mine = QString("environment %1 · ").arg(m_index + 1);
  for (const auto& r : ml::Runs::instance().list()) if (r.by.startsWith(mine)) out << r.id;   // newest first
  return out;
}

QString AgentActivity::shownId() const { return runIds().value(m_pos); }

MlMiniChart* AgentActivity::miniFor(const ml::Run& r) {
  const QString key = QString::number(int(r.state)) + "|" + QString::number(r.result.size());
  auto it = m_minis.find(r.id);
  if (it != m_minis.end() && m_miniKeys.value(r.id) == key) return it->widget ? &*it : nullptr;
  if (m_loading.contains(r.id)) return nullptr;
  // the charts of tiles near the one on top are kept; further ones go (built again when shown)
  if (m_minis.size() >= 12) {
    const QStringList ids = runIds();
    for (auto k = m_minis.begin(); k != m_minis.end();) {
      if (std::abs(int(ids.indexOf(k.key())) - m_pos) <= 5) { ++k; continue; }
      if (k->widget) k->widget->deleteLater();
      m_miniKeys.remove(k.key());
      k = m_minis.erase(k);
    }
  }
  // read and thinned on a worker thread, the widgets built here once that is done
  m_loading.insert(r.id);
  const QString id = r.id;
  auto* w = new QFutureWatcher<std::shared_ptr<const MlMiniRows>>(this);
  connect(w, &QFutureWatcher<std::shared_ptr<const MlMiniRows>>::finished, this, [this, w, id, key] {
    w->deleteLater();
    m_loading.remove(id);
    const ml::Run* run = ml::Runs::instance().find(id);
    if (!run || QString::number(int(run->state)) + "|" + QString::number(run->result.size()) != key) return;   // changed meanwhile
    if (auto old = m_minis.find(id); old != m_minis.end() && old->widget) old->widget->deleteLater();
    MlMiniChart m = mlMiniChart(*run, w->result(), this);
    if (m.widget) m.widget->hide();   // placed and shown by paintEvent, painted into the sliding pictures
    m_minis[id] = m;
    m_miniKeys[id] = key;
    m_faces.remove(id);   // its picture showed the empty frame
    update();
    prefetch();
  });
  w->setFuture(QtConcurrent::run([run = r] { return mlMiniRows(run); }));
  return nullptr;
}

void AgentActivity::sync() {
  ai::Engine* e = ai::Engines::instance().at(m_index);
  const QStringList ids = runIds();
  // a new training lands on top, from whatever tile was shown
  const QString newest = ids.value(0);
  if (!newest.isEmpty() && newest != m_newest) {
    const bool first = m_newest.isEmpty();
    // the tile that was on top is one further down now that the new training heads the list
    const QString was = ids.value(m_pos + 1);
    m_newest = newest;
    m_pos = 0;
    if (!first && isVisible() && !was.isEmpty()) slide(was, newest, true);
  }
  if (m_pos >= ids.size()) m_pos = qMax(0, int(ids.size()) - 1);
  const ml::Run* r = ml::Runs::instance().find(shownId());
  const bool running = r && r->state == ml::RunState::Running;
  const bool busy = e && (!e->activeTool().isEmpty() || e->state() == ai::Engine::State::Busy || e->state() == ai::Engine::State::Starting);
  if ((running || busy) && isVisible()) { if (!m_tick.isActive()) m_tick.start(); }
  else m_tick.stop();
  m_stop->setVisible(running && ml::Runs::instance().running() && m_anim->state() != QAbstractAnimation::Running);
}

void AgentActivity::go(int step) {
  const QStringList ids = runIds();
  const int to = qBound(0, m_pos + step, int(ids.size()) - 1);
  if (to == m_pos || ids.isEmpty()) return;
  const QString from = ids.value(m_pos);
  m_pos = to;
  // presses close together (a held key, quick taps): only the place moves, no slide, tiles drawn light until they stop
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const bool quickPress = now - m_lastGoMs < 250;
  m_lastGoMs = now;
  m_settle.start();
  if (quickPress) m_fast = true;
  if (m_fast || m_anim->state() == QAbstractAnimation::Running) {
    m_anim->stop();
    m_fromPic = m_toPic = QPixmap();
    sync();
    update();
    prefetch();
    return;
  }
  slide(from, ids.value(to), step < 0);   // a newer tile comes back on top; going back, the top one leaves
  sync();
}

void AgentActivity::slide(const QString& from, const QString& to, bool push) {
  m_anim->stop();
  const int tw = width() - 2 * kPad;
  if (tw <= 40 || from.isEmpty() || to.isEmpty()) { update(); return; }
  m_push = push;
  if (m_3d) { m_fromPic = m_toPic = QPixmap(); m_stop->hide(); m_anim->start(); return; }   // paint3D moves the slabs
  // pictures kept per run (facePicture), the neighbours drawn ahead (prefetch): a key press only starts the slide
  m_fromPic = facePicture(from, tw, &m_fromH);
  m_toPic = facePicture(to, tw, &m_toH);
  for (auto& m : m_minis) if (m.widget) m.widget->hide();
  m_stop->hide();
  m_anim->start();
}

void AgentActivity::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Left) { go(+1); return; }    // back: an older training
  if (e->key() == Qt::Key_Right) { go(-1); return; }   // forward: a newer one
  QWidget::keyPressEvent(e);
}

void AgentActivity::mousePressEvent(QMouseEvent* e) {
  const QPoint pt = e->position().toPoint();
  if (m_3dRect.contains(pt)) {
    m_3d = !m_3d;
    settings::setValue("ai.activity3d", m_3d);
    m_anim->stop();
    sync();
    update();
    return;
  }
  if (m_olderRect.contains(pt)) { go(+1); return; }
  if (m_newerRect.contains(pt)) { go(-1); return; }
  // 3D: a click on a slab under the top one brings it up
  for (const auto& [id, poly] : m_slabHits)
    if (poly.containsPoint(QPointF(pt), Qt::OddEvenFill)) {
      const int k = int(runIds().indexOf(id));
      if (k > m_pos) go(k - m_pos);
      return;
    }
  QWidget::mousePressEvent(e);
}

void AgentActivity::showEvent(QShowEvent*) { sync(); prefetch(); }

// The tiles next to the one on top (the next older and newer), drawn ahead, one per turn of the event loop and only
// while nothing slides: ← or → then finds its picture ready.
void AgentActivity::prefetch() {
  QTimer::singleShot(60, this, [this] {
    if (!isVisible() || m_3d) return;
    if (m_anim->state() == QAbstractAnimation::Running || m_fast) { prefetch(); return; }   // not while browsing
    const QStringList ids = runIds();
    const int tw = width() - 2 * kPad;
    if (tw <= 40) return;
    // nearest first: the next older and newer, then further out, four each way
    QStringList order{ids.value(m_pos)};
    for (int d = 1; d <= 4; ++d) order << ids.value(m_pos + d) << ids.value(m_pos - d);
    for (const QString& id : order) {
      if (id.isEmpty()) continue;
      const ml::Run* r = ml::Runs::instance().find(id);
      if (!r || r->state == ml::RunState::Running) continue;   // a running one changes every second: drawn when needed
      if (r->state == ml::RunState::Done) {
        miniFor(*r);                              // its chart's files, read on a worker thread
        if (m_loading.contains(id)) continue;     // its picture once they are (the load calls prefetch again)
      }
      if (hasFace(id, tw)) continue;
      int h = 0;
      facePicture(id, tw, &h);
      prefetch();   // the next one on the next turn
      return;
    }
  });
}
void AgentActivity::hideEvent(QHideEvent*) { m_tick.stop(); }
void AgentActivity::resizeEvent(QResizeEvent*) { update(); }

QPixmap AgentActivity::tilePicture(const QString& id, int width, int* height) {
  *height = 0;
  const ml::Run* r = ml::Runs::instance().withPoints(id);
  if (!r) return {};
  // its height first (laid out into nothing), then a picture of exactly that size
  {
    QImage measure(1, 1, QImage::Format_ARGB32_Premultiplied);
    QPainter mp(&measure);
    *height = paintTile(mp, QRect(0, 0, width, 0), *r, Measure);
  }
  const qreal dpr = devicePixelRatioF();
  QImage img(QSize(width, *height + 1) * dpr, QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);
  QPainter p(&img);
  paintTile(p, QRect(0, 0, width, 0), *r, Picture);
  p.end();
  return QPixmap::fromImage(std::move(img));
}

int AgentActivity::paintTile(QPainter& p, const QRect& area, const ml::Run& rr, TileMode mode) {
  const bool live = mode == Live;
  const bool quick = mode == Quick || mode == QuickMeasure;
  const auto& c = theme::pal();
  const ml::Run* r = &rr;
  constexpr int kIn = 10;
  const int L = area.left() + kIn, W = area.width() - 2 * kIn;
  const int top = area.top();
  int y = top + kIn;
  QPainter& q = p;
  q.save();
  // state, top right; name
  QString stateText = ml::stateName(r->state);
  const QColor stateColor = r->state == ml::RunState::Running ? c.up : r->state == ml::RunState::Done ? c.textDim
                          : r->state == ml::RunState::Failed ? c.down : c.warn;
  q.setFont(theme::mono(9, QFont::Bold, 1.2));
  const int sw = q.fontMetrics().horizontalAdvance(stateText) + 8;
  q.setPen(stateColor);
  q.drawText(QRect(L, y, W, 18), Qt::AlignRight | Qt::AlignVCenter, stateText);
  q.setFont(theme::sans(12, QFont::DemiBold));
  q.setPen(c.text);
  q.drawText(QRect(L, y, W - sw, 18), Qt::AlignLeft | Qt::AlignVCenter, q.fontMetrics().elidedText(r->name, Qt::ElideRight, W - sw));
  y += 19;
  q.setFont(theme::mono(9));
  q.setPen(c.textDim);
  const QString sub = familyLabel(r->job.value("family").toString()) + " · " + r->job.value("algorithm").toString() + " · " + r->dataset;
  q.drawText(QRect(L, y, W, 14), Qt::AlignLeft | Qt::AlignVCenter, q.fontMetrics().elidedText(sub, Qt::ElideRight, W));
  y += 22;
  // progress bar, percentage and stage: while it runs, and full once done (a stopped run's last percentage is not kept)
  if (r->state == ml::RunState::Running || r->state == ml::RunState::Done) {
    const QRect bar(L, y, W, 6);
    q.setPen(Qt::NoPen);
    q.setBrush(c.panelAlt);
    q.drawRoundedRect(bar, 3, 3);
    const double f = qBound(0.0, r->pct / 100.0, 1.0);
    q.setBrush(c.up);
    if (f > 0) q.drawRoundedRect(QRect(bar.left(), bar.top(), std::max(6, int(bar.width() * f)), bar.height()), 3, 3);
    y += 12;
    q.setFont(theme::mono(10));
    q.setPen(c.textDim);
    const QString pctText = QString::number(qRound(r->pct)) + " %";
    const int pw = q.fontMetrics().horizontalAdvance(pctText) + 8;
    q.drawText(QRect(L, y, W - pw, 16), Qt::AlignLeft | Qt::AlignVCenter, q.fontMetrics().elidedText(r->state == ml::RunState::Done ? T("done") : stageLabel(r->stage), Qt::ElideRight, W - pw));
    q.setPen(c.text);
    q.drawText(QRect(L, y, W, 16), Qt::AlignRight | Qt::AlignVCenter, pctText);
    y += 24;
  }
  // a finished run: the ML page's result chart, small (a Gaussian mixture's map of groups, the test period's
  // predictions, the agent's equity…); while it runs, or when it has none, the trainer's live curve
  MlMiniChart* mini = r->state == ml::RunState::Done && !quick ? miniFor(*r) : nullptr;
  const bool waiting = r->state == ml::RunState::Done && (quick || (!mini && m_loading.contains(r->id)));
  if (waiting) {
    // the chart's place, empty, while browsing fast
    const int ch = qBound(130, int(W * 0.6), 340);
    q.setPen(QPen(theme::alpha(c.border, 160), 1, Qt::DashLine));
    q.setBrush(Qt::NoBrush);
    q.drawRoundedRect(QRect(L, y + 20, W, ch), 3, 3);
    y += 20 + ch + 8;
  } else if (mini) {
    q.setFont(theme::mono(9, QFont::Bold, 1.0));
    q.setPen(c.textMute);
    const QRect tr(L, y, W, 14);
    q.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, q.fontMetrics().elidedText(mini->title, Qt::ElideRight, W));
    y += 20;
    const int ch = mini->heightFor ? mini->heightFor(W) : 160;
    const QRect geo(L, y, W, ch);
    if (live) {
      if (mini->widget->geometry() != geo) mini->widget->setGeometry(geo);
      if (!mini->widget->isVisible()) mini->widget->show();
    } else if (mode == Picture) {
      mini->widget->resize(geo.size());
      if (mini->widget->layout()) mini->widget->layout()->setGeometry(QRect(QPoint(0, 0), geo.size()));
      // the chart paints with this painter: its pen and brush must start clean (the progress bar's brush filled its plot)
      q.save();
      q.setPen(Qt::NoPen);
      q.setBrush(Qt::NoBrush);
      mini->widget->render(&q, q.transform().map(geo.topLeft()), QRegion(), QWidget::DrawChildren);
      q.restore();
    }
    y += ch + 4;
    if (!mini->caption.isEmpty()) {
      q.setFont(theme::mono(9));
      q.setPen(c.textMute);
      const QRect box(L, y, W, 40);
      const QRect used = q.fontMetrics().boundingRect(box, Qt::TextWordWrap, mini->caption);
      q.drawText(box, Qt::TextWordWrap, mini->caption);
      y += used.height() + 8;
    }
  } else if (!r->points.isEmpty()) {
    drawCurve(q, QRect(L, y, W, 132), *r);
    y += 142;
  }
  // outcome, or why it ended
  QString out; QColor outColor = c.text;
  if (r->state == ml::RunState::Done) out = resultText(*r);
  else if (r->state != ml::RunState::Running) { out = r->error.section('\n', 0, 1); outColor = r->state == ml::RunState::Failed ? c.down : c.warn; }
  if (!out.isEmpty()) {
    q.setFont(theme::sans(12));
    q.setPen(outColor);
    const QRect box(L, y, W, 64);
    const QRect used = q.fontMetrics().boundingRect(box, Qt::TextWordWrap, out);
    q.drawText(box, Qt::TextWordWrap, out);
    y += std::min(64, used.height()) + 8;
  }
  // when, and STOP while it runs
  q.setFont(theme::mono(9));
  q.setPen(c.textMute);
  const QString when = r->state == ml::RunState::Running
      ? T("running for %1").arg(clock(r->created.secsTo(QDateTime::currentDateTime())))
      : T("started at %1").arg(QLocale().toString(r->created, r->created.date() == QDate::currentDate() ? "HH:mm" : "dd/MM HH:mm"));
  q.drawText(QRect(L, y, W, 22), Qt::AlignLeft | Qt::AlignVCenter, when);
  if (live && m_stop->isVisible()) {
    const QSize s = m_stop->sizeHint();
    m_stop->setGeometry(L + W - s.width(), y, s.width(), 22);
  }
  y += 22 + kIn;
  q.restore();
  return y - top;
}

bool AgentActivity::hasFace(const QString& id, int width) const {
  const ml::Run* r = ml::Runs::instance().find(id);
  const auto it = m_faces.constFind(id);
  if (!r || it == m_faces.cend() || r->state == ml::RunState::Running) return false;
  return it->second.width() == int(width * devicePixelRatioF()) && it->first.section('|', 0, 0) == QString::number(int(r->state)) &&
         !it->first.endsWith(QLatin1String("loading"));   // drawn while its chart was still being read
}

QPixmap AgentActivity::facePicture(const QString& id, int width, int* height) {
  const ml::Run* r = ml::Runs::instance().withPoints(id);   // its curve read first: the key must not change once drawn
  *height = 0;
  if (!r) return {};
  // drawn again only when something it shows changed (the clock of a running one: each second)
  const QString key = QStringList{QString::number(int(r->state)), QString::number(qRound(r->pct)), r->stage, QString::number(r->points.size()),
                                  QString::number(r->result.size()), QString::number(width), QString::number(theme::revision()),
                                  r->state == ml::RunState::Running ? QString::number(r->created.secsTo(QDateTime::currentDateTime())) : QString(),
                                  m_loading.contains(id) ? QStringLiteral("loading") : QString()}.join('|');
  auto it = m_faces.constFind(id);
  if (it != m_faces.cend() && it->first == key) { *height = int(it->second.height() / it->second.devicePixelRatio()); return it->second; }
  int h = 0;
  QPixmap pic = tilePicture(id, width, &h);
  // pictures of the tiles near the one on top are kept (~3 MB each); further ones go first
  if (m_faces.size() >= 12) {
    const QStringList ids = runIds();
    for (auto k = m_faces.begin(); k != m_faces.end();) {
      if (std::abs(int(ids.indexOf(k.key())) - m_pos) > 4) k = m_faces.erase(k); else ++k;
    }
  }
  m_faces[id] = {key, pic};
  *height = h;
  return pic;
}

int AgentActivity::paint3D(QPainter& p, int y, const QStringList& ids) {
  const auto& c = theme::pal();
  for (auto& m : m_minis) if (m.widget && m.widget->isVisible()) m.widget->hide();
  const int n = int(ids.size());
  const int w = width();
  const int A = w - 2 * kPad;
  // Standing cards: the selected one in front, straight, as readable as in 2D; the older ones behind it, each one step
  // further back, up and to the right, a little smaller, slightly turned and in perspective.
  constexpr int kBehind = 4;                     // cards shown behind the front one
  constexpr double kStepX = 16, kStepY = 15;     // how far back each one stands (its top shows above the one before)
  constexpr double kShrink = 0.975;              // and how much smaller
  constexpr double kTurn = 0.955, kPersp = 0.035;   // width kept by the turn, and the far edge's shortening (each end)
  constexpr double kThick = 5;                   // a card's thickness, seen on its top and far edges
  const bool anim = m_anim->state() == QAbstractAnimation::Running;
  const double t = anim ? m_anim->currentValue().toDouble() : 1.0;
  const int tw = int(A - kBehind * kStepX - kThick);   // the tile is laid out at the front card's width

  struct Card { QString id; double k; bool front; };
  QList<Card> cards;
  const int depth = qMin(kBehind, n - 1 - m_pos);
  for (int k = 0; k <= depth; ++k) {
    double pos = k;
    if (anim && m_push) pos = k == 0 ? 0 : k - 1 + t;   // a new card comes to the front: the others step back
    if (anim && !m_push) pos = k + 1 - t;               // the front card leaves: the others step forward
    cards.append({ids[m_pos + k], pos, k == 0});
  }
  QString moving;
  if (anim && m_push && !cards.isEmpty()) { moving = cards.first().id; cards.removeFirst(); }
  if (anim && !m_push && m_pos > 0) moving = ids.value(m_pos - 1);

  // each card as tall as its own tile, drawn at the tile's own size in front: as readable as in 2D
  QHash<QString, QPixmap> pics;
  QHash<QString, int> heights;
  auto picOf = [&](const QString& id) {
    if (pics.contains(id)) return;
    int h = 0;
    pics[id] = facePicture(id, tw, &h);
    heights[id] = h;
  };
  for (const auto& cd : cards) picOf(cd.id);
  if (!moving.isEmpty()) picOf(moving);
  if (cards.isEmpty() && moving.isEmpty()) return y;

  const double x0 = kPad, top0 = y + kBehind * kStepY + kThick + 4;   // the front card's top-left
  // the card at depth k (fractions while moving; negative = in front of the stack, on its way in or out), h tall
  auto quad = [&](double k, double h) {
    // the front card stands straight (a plain rectangle); the turn and the perspective come in from one step back
    const double bent = qBound(0.0, k, 1.0);
    const double sc = std::pow(kShrink, k);
    const double cw = tw * sc * (1.0 - (1.0 - kTurn) * bent), ch = h * sc;
    const double left = x0 + k * kStepX, top = top0 - k * kStepY;
    const double d = ch * kPersp * bent;
    return QPolygonF({QPointF(left, top), QPointF(left + cw, top + d), QPointF(left + cw, top + ch - d), QPointF(left, top + ch)});
  };
  double bottomMax = top0;
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  auto drawCard = [&](const QString& id, double k, double alpha, bool selected) {
    const int h = heights.value(id);
    if (h <= 0) return;
    const QPolygonF f = quad(k, h);
    if (k >= 0) bottomMax = qMax(bottomMax, f[3].y());
    const QPointF back(kThick, -kThick * 0.8);
    p.setOpacity(qBound(0.0, alpha, 1.0));
    // its thickness: the top edge and the far (right) edge
    p.setPen(QPen(theme::alpha(c.border, 220), 1));
    p.setBrush(theme::mix(c.panelAlt, c.panel, 0.3));
    p.drawPolygon(QPolygonF({f[0], f[1], f[1] + back, f[0] + back}));
    p.setBrush(theme::mix(c.panel, c.bg, 0.4));
    p.drawPolygon(QPolygonF({f[1], f[2], f[2] + back, f[1] + back}));
    // the face: the tile
    p.setPen(Qt::NoPen);
    p.setBrush(c.bg);
    p.drawPolygon(f);
    QTransform map;
    if (QTransform::quadToQuad(QPolygonF(QRectF(0, 0, tw, h)), f, map)) {
      p.save();
      p.setTransform(map, true);
      p.drawPixmap(QPointF(0, 0), pics[id]);
      p.restore();
    }
    p.setPen(QPen(selected ? c.accentText : theme::alpha(c.border, 230), selected ? 1.4 : 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(f);
    p.setOpacity(1.0);
    m_slabHits.prepend({id, QPolygonF({f[3], f[0], f[0] + back, f[1] + back, f[2] + back, f[2]})});
  };
  // the furthest first, the front card last; one arriving or leaving in front of them all
  for (int i = int(cards.size()) - 1; i >= 0; --i) drawCard(cards[i].id, cards[i].k, 1.0, cards[i].front && !anim);
  if (!moving.isEmpty()) {
    const double out = m_push ? (1.0 - t) : t;            // 0 = in place at the front, 1 = gone towards the viewer
    drawCard(moving, -1.4 * out, 1.0 - out, false);
  }
  p.restore();
  const int bottom = int(bottomMax) + 10;
  if (m_stop->isVisible()) {   // STOP of a running training in front, under the stack
    const QSize sz = m_stop->sizeHint();
    m_stop->setGeometry(w - kPad - sz.width(), bottom, sz.width(), 22);
    return bottom + 34;
  }
  return bottom + 4;
}

void AgentActivity::paintEvent(QPaintEvent*) {
  const auto& c = theme::pal();
  QPainter p(this);
  p.fillRect(rect(), c.bg);
  const QRect frame = rect().adjusted(0, 0, -1, -1);
  p.setPen(hasFocus() ? c.borderHi : c.border);
  p.setBrush(c.panel);
  p.drawRoundedRect(frame, 4, 4);
  const int w = width();
  ai::Engine* e = ai::Engines::instance().at(m_index);
  if (!e) return;
  const QStringList ids = runIds();
  const ml::Run* r = ml::Runs::instance().withPoints(ids.value(m_pos));
  const ml::Run* newestRun = ml::Runs::instance().find(ids.value(0));
  const bool training = newestRun && newestRun->state == ml::RunState::Running;

  // ---- what the agent does now
  QString now, chip;
  QColor nowColor = c.text, chipColor = c.textMute;
  const QJsonObject tool = e->activeTool();
  const qint64 waited = tool.isEmpty() ? 0 : e->activeToolSince().secsTo(QDateTime::currentDateTime());
  if (!tool.isEmpty()) {
    now = toolLabel(tool.value("name").toString());
    chip = T("TOOL"); chipColor = c.accentText; nowColor = c.accentText;
  } else if (e->state() == ai::Engine::State::Busy) {
    const QString t = e->messages().isEmpty() ? QString() : e->messages().last().text;
    const bool thinking = t.lastIndexOf("<think>") > t.lastIndexOf("</think>");
    now = thinking ? T("thinking…") : T("writing its answer…");
    chip = thinking ? T("THINKING") : T("WRITING"); chipColor = c.up;
  } else if (e->state() == ai::Engine::State::Starting) {
    now = T("loading its model…"); chip = T("STARTING"); chipColor = c.warn;
  } else if (e->state() == ai::Engine::State::Failed) {
    now = T("stopped on an error"); chip = T("ERROR"); chipColor = c.down; nowColor = c.down;
  } else {
    now = T("waiting for your question"); chip = training ? T("TRAINS") : T("IDLE");
    chipColor = training ? c.up : c.textMute; nowColor = c.textDim;
  }
  if (training && !tool.isEmpty()) chip = T("TRAINS");

  int y = 10;
  if (!m_trainingsOnly) {
  p.setFont(theme::mono(11, QFont::Bold, 2.0));
  p.setPen(c.accentText);
  p.drawText(QRect(kPad, y, w - 2 * kPad, 18), Qt::AlignLeft | Qt::AlignVCenter, T("ACTIVITY"));
  {
    p.setFont(theme::mono(9, QFont::Bold, 1.2));
    const QString t = "● " + chip;
    const int tw = p.fontMetrics().horizontalAdvance(t) + 14;
    const QRect chipR(w - kPad - tw, y, tw, 18);
    p.setPen(theme::alpha(chipColor, 130));
    p.setBrush(theme::alpha(chipColor, 28));
    p.drawRoundedRect(chipR, 3, 3);
    p.setPen(chipColor);
    p.drawText(chipR, Qt::AlignCenter, t);
  }
  y += 28;

  y = section(p, y, w, T("NOW"));
  p.setFont(theme::sans(12));
  p.setPen(nowColor);
  const QString since = tool.isEmpty() ? QString() : clock(waited);
  const int sinceW = since.isEmpty() ? 0 : p.fontMetrics().horizontalAdvance(since) + 8;
  p.drawText(QRect(kPad, y, w - 2 * kPad - sinceW, 18), Qt::AlignLeft | Qt::AlignVCenter,
             p.fontMetrics().elidedText(now, Qt::ElideRight, w - 2 * kPad - sinceW));
  if (!since.isEmpty()) {
    p.setFont(theme::mono(10));
    p.setPen(c.textDim);
    p.drawText(QRect(kPad, y, w - 2 * kPad, 18), Qt::AlignRight | Qt::AlignVCenter, since);
  }
  y += 30;
  }   // !m_trainingsOnly

  // ---- its trainings, a stack of tiles: "‹ 3 / 5 ›" (oldest = 1), ‹ goes back
  m_olderRect = m_newerRect = {};
  const int n = int(ids.size());
  y = section(p, y, w, m_trainingsOnly ? (training ? T("TRAINING NOW") : T("ITS TRAININGS")) : T("ML TRAININGS"),
              {}, {});
  if (n > 0) {
    p.setFont(theme::mono(10, QFont::Bold));
    const QString count = QString("%1 / %2").arg(n - m_pos).arg(n);
    const int cw = p.fontMetrics().horizontalAdvance(count);
    const int top = y - 26;
    const QRect next(w - kPad - 14, top, 14, 14), cnt(next.left() - 6 - cw, top, cw, 14), prev(cnt.left() - 20, top, 14, 14);
    const bool canOlder = m_pos + 1 < n, canNewer = m_pos > 0;
    p.setPen(canOlder ? c.accentText : theme::alpha(c.textMute, 90));
    p.drawText(prev, Qt::AlignCenter, "‹");
    p.setPen(c.textDim);
    p.drawText(cnt, Qt::AlignCenter, count);
    p.setPen(canNewer ? c.accentText : theme::alpha(c.textMute, 90));
    p.drawText(next, Qt::AlignCenter, "›");
    if (canOlder) m_olderRect = prev.adjusted(-6, -4, 6, 4);
    if (canNewer) m_newerRect = next.adjusted(-6, -4, 6, 4);
  }
  m_3dRect = {};
  if (kOffer3D) {
    // the "3D" box, after the section's title
    p.setFont(theme::mono(9, QFont::Bold, 1.4));
    const int tx = kPad + p.fontMetrics().horizontalAdvance(T("ML TRAININGS")) + 14, top = y - 26;
    const QRect box(tx, top + 2, 10, 10);
    p.setPen(m_3d ? c.accentText : c.textMute);
    p.setBrush(m_3d ? theme::alpha(c.accentText, 60) : Qt::NoBrush);
    p.drawRoundedRect(box, 2, 2);
    if (m_3d) {
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(c.accentText, 1.6));
      p.drawPolyline(QPolygonF({QPointF(box.left() + 2, box.top() + 5), QPointF(box.left() + 4.5, box.bottom() - 2), QPointF(box.right() - 1.5, box.top() + 2)}));
      p.setRenderHint(QPainter::Antialiasing, false);
    }
    p.setPen(m_3d ? c.accentText : c.textDim);
    p.drawText(QRect(box.right() + 5, top, 24, 14), Qt::AlignLeft | Qt::AlignVCenter, "3D");
    m_3dRect = QRect(box.left() - 4, top - 3, 40, 20);
  }
  m_slabHits.clear();
  if (r && m_3d) {
    y = paint3D(p, y, ids);
  } else if (!r) {
    for (auto& m : m_minis) if (m.widget) m.widget->hide();
    p.setFont(theme::sans(11));
    p.setPen(c.textMute);
    const QRect box(kPad, y, w - 2 * kPad, 60);
    const QString t = ai::Slots::instance().hasSkill(m_index, "ml")
        ? T("No training started by this agent yet. Its trainings show here as they run.")
        : T("Tick “Machine learning” to let this agent train models: its trainings show here as they run.");
    const QRect used = p.fontMetrics().boundingRect(box, Qt::TextWordWrap, t);
    p.drawText(box, Qt::TextWordWrap, t);
    y += used.height() + 14;
  } else {
    const int tw = w - 2 * kPad;
    const int under = qMin(2, n - 1 - m_pos);   // tiles below the one on top, as edges
    auto frameAt = [&](const QRect& tile) {
      // the edges of the tiles underneath peek out below, then the tile itself
      for (int k = under; k >= 1; --k) {
        const QRect edge = tile.adjusted(6 * k, 4 * k, -6 * k, 4 * k);
        p.setPen(theme::alpha(c.border, 200 - 50 * k));
        p.setBrush(c.panel);
        p.drawRoundedRect(edge, 5, 5);
      }
      p.setPen(c.border);
      p.setBrush(c.bg);
      p.drawRoundedRect(tile, 5, 5);
    };
    if (m_anim->state() == QAbstractAnimation::Running && !m_fromPic.isNull() && !m_toPic.isNull()) {
      const double t = m_anim->currentValue().toDouble();
      // the stack's height goes from the old tile's to the new one's: what is below it never overlaps a taller tile
      const int h = int(m_fromH + (m_toH - m_fromH) * t);
      // under the slide: the tile that stays (the old one when a new one comes on top, the older one when the top leaves)
      const QPixmap& below = m_push ? m_fromPic : m_toPic;
      const QPixmap& over = m_push ? m_toPic : m_fromPic;
      const int belowH = m_push ? m_fromH : m_toH, overH = m_push ? m_toH : m_fromH;
      p.save();
      p.setClipRect(QRect(0, y - 2, w, h + 4 * under + 4));
      frameAt(QRect(kPad, y, tw, qMin(belowH, h)));
      p.save();
      p.setClipRect(QRect(kPad, y, tw, qMin(belowH, h)), Qt::IntersectClip);   // the tile below ends where the stack does
      p.drawPixmap(kPad, y, below);
      p.restore();
      const int x = m_push ? kPad + int((1.0 - t) * (tw + kPad)) : kPad + int(t * (tw + kPad));
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, 90));
      p.drawRoundedRect(QRect(x - 4, y + 3, tw, overH), 5, 5);   // its shadow on the tile below
      p.setPen(c.borderHi);
      p.setBrush(c.bg);
      p.drawRoundedRect(QRect(x, y, tw, overH), 5, 5);
      p.drawPixmap(x, y, over);
      p.restore();
      y += h + 4 * under + 12;
    } else {
      if (m_fast) {
        // browsing fast: the tile's picture when ready, else the tile without its chart; no chart widget shown
        for (auto& m : m_minis) if (m.widget && m.widget->isVisible()) m.widget->hide();
        if (m_stop->isVisible()) m_stop->hide();
        int fh = 0;
        if (hasFace(r->id, tw)) {
          const QPixmap pic = facePicture(r->id, tw, &fh);
          frameAt(QRect(kPad, y, tw, fh));
          p.drawPixmap(kPad, y, pic);
        } else {
          QImage measure(1, 1, QImage::Format_ARGB32_Premultiplied);
          {
            QPainter mp(&measure);
            fh = paintTile(mp, QRect(kPad, y, tw, 0), *r, QuickMeasure);
          }
          frameAt(QRect(kPad, y, tw, fh));
          paintTile(p, QRect(kPad, y, tw, 0), *r, Quick);
        }
        y += fh + 4 * under + 12;
      } else {
      // the height first (the frame goes under the content): the tile laid out once into nothing
      QImage measure(1, 1, QImage::Format_ARGB32_Premultiplied);
      int th;
      {
        QPainter mp(&measure);
        th = paintTile(mp, QRect(kPad, y, tw, 0), *r, Measure);
      }
      frameAt(QRect(kPad, y, tw, th));
      for (auto it = m_minis.begin(); it != m_minis.end(); ++it)
        if (it.key() != r->id && it->widget && it->widget->isVisible()) it->widget->hide();
      MlMiniChart* mini = r->state == ml::RunState::Done ? miniFor(*r) : nullptr;
      if (!mini) for (auto& m : m_minis) if (m.widget && m.widget->isVisible()) m.widget->hide();
      paintTile(p, QRect(kPad, y, tw, 0), *r, Live);
      y += th + 4 * under + 12;
      }
    }
  }

  if (m_trainingsOnly) return;   // the constellation's node: the stack only
  // ---- its last tool calls, newest first
  y = section(p, y, w, T("LAST ACTIONS"));
  const auto& msgs = e->messages();
  int shown = 0;
  for (int i = int(msgs.size()) - 1; i >= 0 && y + 18 <= height() - 8; --i) {
    const auto& m = msgs[i];
    if (m.role != "tool" || m.call.value("name").toString() == "—") continue;
    p.setFont(theme::mono(10, QFont::Bold));
    p.setPen(m.ok ? c.up : c.down);
    p.drawText(QRect(kPad, y, 14, 18), Qt::AlignLeft | Qt::AlignVCenter, m.ok ? "✓" : "✕");
    p.setFont(theme::mono(9));
    const QString time = m.at.toString("HH:mm:ss");
    const int tw = p.fontMetrics().horizontalAdvance(time) + 8;
    p.setPen(c.textMute);
    p.drawText(QRect(kPad, y, w - 2 * kPad, 18), Qt::AlignRight | Qt::AlignVCenter, time);
    p.setFont(theme::sans(11));
    p.setPen(m.ok ? c.textDim : c.down);
    p.drawText(QRect(kPad + 16, y, w - 2 * kPad - 16 - tw, 18), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(toolLabel(m.call.value("name").toString()), Qt::ElideRight, w - 2 * kPad - 16 - tw));
    y += 20;
    ++shown;
  }
  // "none yet" only when there are none: a tall tile can leave no room for the list
  bool any = false;
  for (const auto& m : msgs) if (m.role == "tool" && m.call.value("name").toString() != "—") { any = true; break; }
  if (shown == 0 && !any) {
    p.setFont(theme::sans(11));
    p.setPen(c.textMute);
    p.drawText(QRect(kPad, y, w - 2 * kPad, 18), Qt::AlignLeft | Qt::AlignVCenter, T("None yet."));
  }
}

}  // namespace aipage
