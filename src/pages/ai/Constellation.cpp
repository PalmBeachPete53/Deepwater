#include "pages/ai/Constellation.h"
#include "ai/Engine.h"
#include "ai/Slots.h"
#include "ml/Catalog.h"
#include "ml/Comparison.h"
#include "ml/Runs.h"
#include "pages/ai/AgentActivity.h"
#include "pages/ai/MlCompareView.h"
#include "ui/Theme.h"
#include "ui/Touch.h"
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtConcurrent>
#include <cmath>
#include <memory>

namespace aipage {
namespace {
QString T(const char* s) { return QCoreApplication::translate("ai", s); }
constexpr int kCaption = 24;   // room above a node for its name

// The tools that bring data in: what COLLECTE lists.
bool isCollect(const QString& name) {
  static const QSet<QString> more{"ml_datasets", "ml_load_dataset", "ml_read_dataset", "golden_series", "bt_indicators"};
  return name.startsWith("get_") || name.startsWith("read_") || more.contains(name);
}
// What a call asked for, from its arguments: the market, the data set, the series…
QString askedFor(const QJsonObject& call) {
  const QJsonObject a = call.value("arguments").toObject();
  QStringList parts;
  for (const char* k : {"symbol", "symbols", "market", "ticker", "dataset", "key", "country", "series", "indicator", "source", "query",
                        "currency", "bank", "timeframe", "interval", "page"}) {
    const QJsonValue v = a.value(k);
    if (v.isString() && !v.toString().isEmpty()) parts << v.toString();
    else if (v.isArray()) {
      QStringList l;
      for (const auto& x : v.toArray()) l << x.toVariant().toString();
      if (!l.isEmpty()) parts << l.mid(0, 4).join(", ") + (l.size() > 4 ? " …" : "");
    } else if (v.isDouble()) parts << QString::number(v.toDouble());
    if (parts.size() >= 3) break;
  }
  return parts.join(" · ");
}
QString agoText(const QDateTime& at) {
  const qint64 s = at.secsTo(QDateTime::currentDateTime());
  if (s < 60) return T("just now");
  if (s < 3600) return T("%1 min ago").arg(s / 60);
  if (s < 86400) return T("%1 h ago").arg(s / 3600);
  return at.toString("dd MMM HH:mm");
}
QString clockText(qint64 secs) { return QString("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0')); }

void drawFrame(QPainter& p, const QRect& r, bool lit) {
  const auto& c = theme::pal();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(lit ? theme::alpha(c.accent, 200) : c.border, 1));
  p.setBrush(c.panel);
  p.drawRoundedRect(QRectF(r).adjusted(.5, .5, -.5, -.5), 5, 5);
}
// wraps `text` in `box` from y; returns the y below it (nothing drawn past box's bottom)
int wrap(QPainter& p, const QRect& box, int y, const QString& text, const QFont& f, const QColor& col, int maxLines = 3) {
  p.setFont(f);
  const QFontMetrics fm(f);
  const QRect need = fm.boundingRect(QRect(box.left(), y, box.width(), 1000), Qt::TextWordWrap, text);
  const int h = std::min(need.height(), fm.lineSpacing() * maxLines);
  if (y + h > box.bottom()) return y;
  p.setPen(col);
  p.drawText(QRect(box.left(), y, box.width(), h), Qt::TextWordWrap, text);
  return y + h;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ tick box
TickBox::TickBox(const QString& text, QWidget* parent) : QAbstractButton(parent) {
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
  setText(text);
}
QSize TickBox::sizeHint() const { return QSize(22 + QFontMetrics(theme::mono(9, QFont::Bold, 1.4)).horizontalAdvance(text()) + 4, 20); }
void TickBox::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const auto& c = theme::pal();
  const bool on = isChecked();
  const QRectF box(0.5, (height() - 14) / 2.0 + 0.5, 13, 13);
  p.setPen(QPen(on ? c.accentText : underMouse() ? c.borderHi : c.textMute, 1));
  p.setBrush(on ? theme::alpha(c.accentText, 50) : Qt::transparent);
  p.drawRoundedRect(box, 2, 2);
  if (on) {
    QPainterPath tick;
    tick.moveTo(box.left() + 3, box.center().y());
    tick.lineTo(box.left() + 5.6, box.bottom() - 3.2);
    tick.lineTo(box.right() - 2.8, box.top() + 3.2);
    p.setPen(QPen(c.accentText, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    p.drawPath(tick);
  }
  p.setFont(theme::mono(9, QFont::Bold, 1.4));
  p.setPen(on ? c.accentText : c.textDim);
  p.drawText(QRectF(20, 0, width() - 20, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}

// ------------------------------------------------------------------------------------------------ COLLECTE
class CollectNode : public QWidget {
public:
  explicit CollectNode(QWidget* p) : QWidget(p) {}
  void setEnvironment(int index) {
    for (const auto& k : m_conns) disconnect(k);
    m_conns.clear();
    m_index = index;
    if (ai::Engine* e = ai::Engines::instance().at(index))
      for (auto sig : {&ai::Engine::messagesChanged, &ai::Engine::toolActivity, &ai::Engine::stateChanged})
        m_conns << connect(e, sig, this, qOverload<>(&QWidget::update));
    update();
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const auto& c = theme::pal();
    ai::Engine* e = ai::Engines::instance().at(m_index);
    const QJsonObject now = e ? e->activeTool() : QJsonObject();
    const bool live = !now.isEmpty() && isCollect(now.value("name").toString());
    drawFrame(p, rect(), live);
    if (!e) return;
    const QRect box = rect().adjusted(12, 10, -12, -10);
    int y = box.top();
    // the call running now
    if (live) {
      const QString t = clockText(e->activeToolSince().secsTo(QDateTime::currentDateTime()));
      p.setFont(theme::mono(10));
      const int tw = p.fontMetrics().horizontalAdvance(t) + 8;
      p.setPen(c.textDim);
      p.drawText(QRect(box.left(), y, box.width(), 18), Qt::AlignRight | Qt::AlignVCenter, t);
      p.setFont(theme::sans(12, QFont::DemiBold));
      p.setPen(c.accentText);
      p.drawText(QRect(box.left(), y, box.width() - tw, 18), Qt::AlignLeft | Qt::AlignVCenter,
                 p.fontMetrics().elidedText("● " + toolLabel(now.value("name").toString()), Qt::ElideRight, box.width() - tw));
      y += 18;
      const QString a = askedFor(now);
      if (!a.isEmpty()) {
        p.setFont(theme::mono(10)); p.setPen(c.textDim);
        p.drawText(QRect(box.left() + 14, y, box.width() - 14, 16), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(a, Qt::ElideRight, box.width() - 14));
        y += 16;
      }
      y += 6;
      p.setPen(c.divider); p.drawLine(box.left(), y, box.right(), y);
      y += 8;
    }
    // what it read, newest first
    const auto& msgs = e->messages();
    int count = 0, failed = 0;
    for (const auto& m : msgs) if (m.role == "tool" && isCollect(m.call.value("name").toString())) { ++count; if (!m.ok) ++failed; }
    if (count == 0 && !live) {
      wrap(p, box, y, T("Nothing collected yet. The data this agent reads (prices, macro series, news, data sets for training) shows here, newest first."),
           theme::sans(11), c.textMute, 6);
      return;
    }
    if (count) {
      p.setFont(theme::mono(9, QFont::Bold, 1.2));
      p.setPen(c.textMute);
      p.drawText(QRect(box.left(), y, box.width(), 14), Qt::AlignLeft | Qt::AlignVCenter,
                 T("%1 READ(S)").arg(count) + (failed ? "  ·  " + T("%1 REFUSED").arg(failed) : QString()));
      y += 20;
    }
    for (int i = int(msgs.size()) - 1; i >= 0 && y + 34 <= box.bottom(); --i) {
      const auto& m = msgs[i];
      if (m.role != "tool" || !isCollect(m.call.value("name").toString())) continue;
      p.setFont(theme::mono(10, QFont::Bold));
      p.setPen(m.ok ? c.up : c.down);
      p.drawText(QRect(box.left(), y, 14, 17), Qt::AlignLeft | Qt::AlignVCenter, m.ok ? "✓" : "✕");
      p.setFont(theme::mono(9));
      const QString time = m.at.toString("HH:mm");
      const int tw = p.fontMetrics().horizontalAdvance(time) + 8;
      p.setPen(c.textMute);
      p.drawText(QRect(box.left(), y, box.width(), 17), Qt::AlignRight | Qt::AlignVCenter, time);
      p.setFont(theme::sans(11));
      p.setPen(m.ok ? c.text : c.down);
      p.drawText(QRect(box.left() + 16, y, box.width() - 16 - tw, 17), Qt::AlignLeft | Qt::AlignVCenter,
                 p.fontMetrics().elidedText(toolLabel(m.call.value("name").toString()), Qt::ElideRight, box.width() - 16 - tw));
      const QString a = askedFor(m.call);
      p.setFont(theme::mono(9));
      p.setPen(c.textDim);
      p.drawText(QRect(box.left() + 16, y + 16, box.width() - 16, 15), Qt::AlignLeft | Qt::AlignVCenter,
                 p.fontMetrics().elidedText(a.isEmpty() ? m.call.value("name").toString() : a, Qt::ElideRight, box.width() - 16));
      y += 36;
    }
  }
private:
  int m_index = -1;
  QList<QMetaObject::Connection> m_conns;
};

// ------------------------------------------------------------------------------------------------ COMPARAISON
class CompareNode : public QWidget {
public:
  explicit CompareNode(QWidget* p) : QWidget(p) {
    connect(&ml::Runs::instance(), &ml::Runs::listChanged, this, [this] { sync(); });
  }
  void setEnvironment(int index) {
    for (const auto& k : m_conns) disconnect(k);
    m_conns.clear();
    if (index != m_index) { m_report.reset(); m_key.clear(); }
    m_index = index;
    if (ai::Engine* e = ai::Engines::instance().at(index)) {
      m_conns << connect(e, &ai::Engine::messagesChanged, this, [this] { sync(); });
      m_conns << connect(e, &ai::Engine::toolActivity, this, qOverload<>(&QWidget::update));
    }
    sync();
  }
protected:
  // its last ml_compare: the comparison done again from the call's arguments when the call or the runs changed
  void sync() {
    ai::Engine* e = ai::Engines::instance().at(m_index);
    m_call = -1;
    if (e) for (int i = int(e->messages().size()) - 1; i >= 0; --i)
      if (e->messages()[i].role == "tool" && e->messages()[i].call.value("name").toString() == "ml_compare") { m_call = i; break; }
    if (m_call < 0 || !e->messages()[m_call].ok) { update(); return; }
    const auto& m = e->messages()[m_call];
    const QJsonObject args = m.call.value("arguments").toObject();
    const auto& runs = ml::Runs::instance().list();
    const QString key = m.at.toString(Qt::ISODateWithMs) + QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)) +
                        QString::number(runs.size()) + (runs.isEmpty() ? QString() : runs.first().id + QString::number(int(runs.first().state)));
    if (key == m_key) { update(); return; }
    m_key = key;
    QString error;
    const ml::comparison::Input in = ml::comparison::gather(ml::comparison::choose(args, m_index, &error));
    if (in.items.size() < 2) { m_report.reset(); m_busy = false; update(); return; }
    m_busy = true;
    const int seq = ++m_seq;
    auto* w = new QFutureWatcher<ml::comparison::Report>(this);
    connect(w, &QFutureWatcher<ml::comparison::Report>::finished, this, [this, w, seq] {
      w->deleteLater();
      if (seq != m_seq) return;
      m_report = std::make_shared<ml::comparison::Report>(w->result());
      m_busy = false;
      update();
    });
    w->setFuture(QtConcurrent::run([in] { return ml::comparison::build(in); }));
    update();
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const auto& c = theme::pal();
    ai::Engine* e = ai::Engines::instance().at(m_index);
    const bool live = e && e->activeTool().value("name").toString() == "ml_compare";
    drawFrame(p, rect(), live);
    if (!e) return;
    const QRect box = rect().adjusted(12, 10, -12, -10);
    int y = box.top();
    if (live) {
      p.setFont(theme::sans(12, QFont::DemiBold)); p.setPen(c.accentText);
      p.drawText(QRect(box.left(), y, box.width(), 18), Qt::AlignLeft | Qt::AlignVCenter, "● " + toolLabel("ml_compare") + "…");
      y += 26;
    }
    if (m_call < 0) {
      wrap(p, box, y, ai::Slots::instance().hasSkill(m_index, "ml")
                          ? T("No comparison yet. When this agent compares its trainings, each group's best and the verdict show here.")
                          : T("Tick “Machine learning” to let this agent train and compare models."),
           theme::sans(11), c.textMute, 6);
      return;
    }
    const auto& m = e->messages()[m_call];
    p.setFont(theme::mono(9, QFont::Bold, 1.2)); p.setPen(c.textMute);
    QString head = agoText(m.at);
    if (m_report) head += "  ·  " + T("%1 TRAININGS").arg(m_report->compared);
    p.drawText(QRect(box.left(), y, box.width(), 14), Qt::AlignLeft | Qt::AlignVCenter, head.toUpper());
    y += 20;
    if (!m.ok) { wrap(p, box, y, m.text.section('\n', 0, 0), theme::sans(11), c.down, 4); return; }
    if (m_busy && !m_report) { wrap(p, box, y, T("Comparing…"), theme::sans(11), c.textDim); return; }
    if (!m_report) { wrap(p, box, y, T("These trainings are no longer all here: nothing left to compare."), theme::sans(11), c.textMute, 3); return; }
    if (m_report->nothing) { wrap(p, box, y, T("Nothing comparable among the trainings it chose."), theme::sans(11), c.textMute, 3); return; }
    using namespace ml::comparison;
    auto algo = [](const QString& id) { const ml::Algo a = ml::algoById(id); return a.id.isEmpty() ? id : a.name; };
    int shown = 0;
    for (const Group& g : m_report->groups) {
      if (g.mode == Group::OneSetupOnly || g.rows.isEmpty()) continue;
      if (y + 50 > box.bottom()) break;
      if (shown++) { p.setPen(c.divider); p.drawLine(box.left(), y + 2, box.right(), y + 2); y += 10; }
      const bool same = g.mode == Group::SameMarket;
      p.setFont(theme::mono(9, QFont::Normal, 1.6)); p.setPen(c.accentText);
      p.drawText(QRect(box.left(), y, box.width(), 14), Qt::AlignLeft | Qt::AlignVCenter,
                 p.fontMetrics().elidedText(same ? T("SAME MARKET · %1").arg(g.market) : T("SEVERAL MARKETS · %1").arg(algo(g.algorithm)),
                                            Qt::ElideRight, box.width()));
      y += 18;
      const Row& b = g.rows.first();
      // what the group predicts: two groups on one market differ only by it
      if (const ml::Run* r0 = ml::Runs::instance().find(b.id); r0 && (g.kind == Kind::Predict || g.kind == Kind::Probe)) {
        const QJsonObject job = r0->job, pr = r0->result.value("probe").toObject();
        const bool probe = g.kind == Kind::Probe;
        const QString tf = probe ? pr.value("transform").toString() : job.value("target_transform").toString("raw");
        static const QHash<QString, const char*> tfs{{"raw", QT_TRANSLATE_NOOP("ai", "value")}, {"change", QT_TRANSLATE_NOOP("ai", "change")},
                                                      {"pct_change", QT_TRANSLATE_NOOP("ai", "% change")}, {"direction", QT_TRANSLATE_NOOP("ai", "direction (up / down)")}};
        const QString q = T("predicts the %1 of %2, %3 row(s) ahead")
                              .arg(tfs.contains(tf) ? T(tfs.value(tf)) : tf, probe ? pr.value("target").toString() : job.value("target").toString())
                              .arg(probe ? pr.value("horizon").toInt() : job.value("horizon").toInt(1));
        p.setFont(theme::mono(9)); p.setPen(c.textDim);
        p.drawText(QRect(box.left(), y, box.width(), 14), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(q, Qt::ElideRight, box.width()));
        y += 17;
      }
      // the best (same market) or the spread (several markets), then what it means
      QString line;
      QColor tone = c.text;
      if (g.kind == Kind::Predict || g.kind == Kind::Probe) {
        const QString edge = g.values ? (std::isfinite(b.edge) ? QString("%1%2%").arg(b.edge >= 0 ? "+" : "−").arg(std::abs(b.edge) * 100, 0, 'f', 1) : "—")
                                      : (std::isfinite(b.edge) ? QString("%1%2 pt").arg(b.edge >= 0 ? "+" : "−").arg(std::abs(b.edge) * 100, 0, 'f', 1) : "—");
        line = same ? T("best: %1, %2 over no skill").arg(algo(b.algorithm), edge)
                    : T("beats no skill on %1 of %2 markets").arg(g.beat).arg(g.known);
      } else if (g.kind == Kind::Trade) {
        line = same ? T("best: %1, %2 over buy & hold").arg(algo(b.algorithm)).arg(std::isfinite(b.edge) ? QString("%1%2%").arg(b.edge >= 0 ? "+" : "−").arg(std::abs(b.edge) * 100, 0, 'f', 1) : "—")
                    : T("above buy & hold on %1 of %2 markets").arg(g.beat).arg(g.known);
        tone = g.beat * 2 > g.known || (same && b.edge > 0) ? c.up : c.textDim;
      } else {
        line = same ? T("best: %1, silhouette %2").arg(algo(b.algorithm)).arg(b.silhouette, 0, 'f', 2) : T("%1 markets, descriptive only").arg(g.rows.size());
        tone = c.textDim;
      }
      y = wrap(p, box, y, line, theme::mono(10), tone, 2) + 3;
      if (same && (g.kind == Kind::Predict || g.kind == Kind::Probe)) y = wrap(p, box, y, verdictLine(g), theme::sans(11, QFont::DemiBold), verdictTone(g), 4) + 4;
      else if (!same && (g.kind == Kind::Predict || g.kind == Kind::Probe))
        y = wrap(p, box, y, g.sig > 0 ? T("significant after the tries on %1").arg(g.sig) : T("significant on none after the tries"), theme::sans(11, QFont::DemiBold),
                 g.sig > 0 && g.sig * 2 >= g.known ? c.up : g.beat == 0 ? c.down : c.warn, 2) + 4;
    }
    if (shown == 0) wrap(p, box, y, T("Only re-runs of one setup: nothing to rank."), theme::sans(11), c.textMute, 2);
  }
private:
  int m_index = -1;
  int m_call = -1;              // index of its last ml_compare message
  QString m_key;
  int m_seq = 0;
  bool m_busy = false;
  std::shared_ptr<ml::comparison::Report> m_report;
  QList<QMetaObject::Connection> m_conns;
};

// ------------------------------------------------------------------------------------------------ the constellation
Constellation::Constellation(QWidget* parent) : QWidget(parent) {
  m_collect = new CollectNode(this);
  m_train = new AgentActivity(this);
  m_train->setTrainingsOnly(true);
  m_train->setMinimumWidth(0);
  m_compare = new CompareNode(this);
  m_tick.setInterval(33);
  // only the lit links are drawn again (a full update would repaint the conversation and the charts 30 times a second)
  connect(&m_tick, &QTimer::timeout, this, [this] {
    for (const Link k : {Collect, Train, Compare})
      if (active(k)) update(linkRect(k));
    if (active(Collect) && m_clock.elapsed() / 1000 != m_lastSecond) { m_lastSecond = m_clock.elapsed() / 1000; m_collect->update(); }   // its clock
  });
  m_clock.start();
  connect(&ml::Runs::instance(), &ml::Runs::listChanged, this, [this] { syncTick(); });
  connect(&ml::Runs::instance(), &ml::Runs::runChanged, this, [this] { syncTick(); });
}

void Constellation::setEnvironment(int index) {
  for (const auto& k : m_conns) disconnect(k);
  m_conns.clear();
  m_index = index;
  m_collect->setEnvironment(index);
  m_train->setEnvironment(index);
  m_compare->setEnvironment(index);
  if (ai::Engine* e = ai::Engines::instance().at(index))
    m_conns << connect(e, &ai::Engine::toolActivity, this, [this] { syncTick(); update(); });
  syncTick();
  update();
}

void Constellation::setChat(QWidget* chat) {
  m_chat = chat;
  chat->setParent(this);
  chat->show();
  place();
}
QWidget* Constellation::takeChat() {
  QWidget* c = m_chat;
  m_chat = nullptr;
  return c;
}

bool Constellation::active(Link k) const {
  ai::Engine* e = ai::Engines::instance().at(m_index);
  if (!e) return false;
  const QString tool = e->activeTool().value("name").toString();
  switch (k) {
    case Collect: return isCollect(tool);
    case Compare: return tool == "ml_compare";
    case Train: {
      if (tool == "ml_train") return true;
      const QString mine = QString("environment %1 · ").arg(m_index + 1);
      for (const auto& r : ml::Runs::instance().list()) if (r.by.startsWith(mine) && r.state == ml::RunState::Running) return true;
      return false;
    }
  }
  return false;
}

QLineF Constellation::linkLine(Link k) const {
  if (ui::compact(width())) {
    // phones: down the left margin from the conversation's lower edge to the node's upper left; the nearer the node,
    // the further in the line starts, so they never cross
    const int x0 = 6 + (k == Collect ? 16 : k == Train ? 10 : 4);
    const QRect& r = k == Collect ? m_collectR : k == Train ? m_trainR : m_compareR;
    return QLineF(QPointF(x0, m_chatR.bottom() + 1), QPointF(r.left(), r.top() + 16));
  }
  switch (k) {
    case Collect:
      return QLineF(QPointF(m_chatR.left(), std::clamp(m_collectR.center().y() + 0.0, m_chatR.top() + 30.0, m_chatR.bottom() - 30.0)),
                    QPointF(m_collectR.right() + 1, m_collectR.center().y()));
    case Train: return QLineF(QPointF(m_chatR.right() + 1, m_chatR.top() + m_chatR.height() * 0.30), QPointF(m_trainR.left(), m_trainR.center().y()));
    case Compare: return QLineF(QPointF(m_chatR.right() + 1, m_chatR.top() + m_chatR.height() * 0.68), QPointF(m_compareR.left(), m_compareR.center().y()));
  }
  return {};
}
QRect Constellation::linkRect(Link k) const {
  const QLineF l = linkLine(k);
  return QRectF(l.p1(), l.p2()).normalized().toAlignedRect().adjusted(-12, -12, 12, 12);
}

void Constellation::syncTick() {
  const bool on = isVisible() && (active(Collect) || active(Train) || active(Compare));
  if (on && !m_tick.isActive()) m_tick.start();
  else if (!on && m_tick.isActive()) { m_tick.stop(); update(); }
}
void Constellation::showEvent(QShowEvent*) { syncTick(); }
void Constellation::hideEvent(QHideEvent*) { m_tick.stop(); }
void Constellation::resizeEvent(QResizeEvent*) { place(); }

// The drawing: the conversation a tall card in the middle; COLLECTE at its left, a little below the middle; ENTRAÎNEMENT
// up on the right, COMPARAISON below it and nearer. The space left over lengthens the links.
// Phones: the nodes' heights, one under the other below the conversation, indented by the links' margin.
namespace {
constexpr int kPhoneNode[3] = {300, 340, 260};   // COLLECTION, TRAINING, COMPARISON
constexpr int kPhoneGutter = 26, kPhoneGap = 12;
}
int Constellation::phoneNodesHeight() { return kPhoneGap + 3 * (kCaption + kPhoneGap) + kPhoneNode[0] + kPhoneNode[1] + kPhoneNode[2]; }

void Constellation::place() {
  const int W = width(), H = height(), m = 6, gapMin = 44;
  if (ui::compact(W)) {
    m_chatR = QRect(m, m, W - 2 * m, qMax(120, H - 2 * m - phoneNodesHeight()));
    int y = m_chatR.bottom() + 1 + kPhoneGap;
    QRect* nodes[3] = {&m_collectR, &m_trainR, &m_compareR};
    for (int k = 0; k < 3; ++k) {
      y += kCaption;
      *nodes[k] = QRect(m + kPhoneGutter, y, W - 2 * m - kPhoneGutter, kPhoneNode[k]);
      y += kPhoneNode[k] + kPhoneGap;
    }
    if (m_chat) m_chat->setGeometry(m_chatR);
    m_collect->setGeometry(m_collectR);
    m_train->setGeometry(m_trainR);
    m_compare->setGeometry(m_compareR);
    m_stars = QPixmap();
    update();
    return;
  }
  int leftW = std::clamp(int(W * 0.20), 180, 320), rightW = std::clamp(int(W * 0.27), 220, 440);
  int chatW = std::clamp(int(W * 0.40), 380, 680);
  int spare = W - 2 * m - leftW - rightW - chatW - 2 * gapMin;
  if (spare < 0) { const int cut = std::min(-spare, chatW - 360); chatW -= cut; spare += cut; }
  if (spare < 0) { const int cut = -spare; leftW = std::max(150, leftW - cut / 2); rightW = std::max(180, rightW - (cut - cut / 2)); spare = 0; }
  const int extra = std::min(spare / 2, 70);   // longer links, up to a point; the rest widens the conversation
  chatW += std::max(0, spare - 2 * extra);
  const int gap = gapMin + extra;
  const int top = m + kCaption, bottom = H - m;
  m_collectR = QRect(m, 0, leftW, std::clamp(int(H * 0.46), 180, 420));
  m_collectR.moveTop(std::clamp(int(H * 0.55) - m_collectR.height() / 2, top, bottom - m_collectR.height()));
  m_chatR = QRect(m_collectR.right() + 1 + gap, m, chatW, H - 2 * m);
  const int rx = m_chatR.right() + 1 + gap, rw = W - m - rx;
  const int avail = bottom - top;
  int th = std::clamp(int(avail * 0.52), 220, 520), ch = std::clamp(int(avail * 0.34), 150, 360);
  const int vgap = kCaption + 22;
  if (th + vgap + ch > avail) { const double k = double(avail - vgap) / (th + ch); th = int(th * k); ch = int(ch * k); }
  m_trainR = QRect(rx + std::min(28, rw / 10), top + int((avail - th - vgap - ch) * 0.15), rw - std::min(28, rw / 10), th);
  m_compareR = QRect(rx, m_trainR.bottom() + 1 + vgap, rw - std::min(28, rw / 10), ch);
  if (m_compareR.bottom() > bottom) m_compareR.moveBottom(bottom);
  if (m_chat) m_chat->setGeometry(m_chatR);
  m_collect->setGeometry(m_collectR);
  m_train->setGeometry(m_trainR);
  m_compare->setGeometry(m_compareR);
  m_stars = QPixmap();
  update();
}

void Constellation::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const auto& c = theme::pal();
  p.fillRect(rect(), c.bg);
  // the star field, faint, drawn once per size
  const qreal dpr = devicePixelRatioF();
  if (m_stars.isNull() || m_starsDpr != dpr) {
    m_stars = QPixmap(size() * dpr);
    m_stars.setDevicePixelRatio(dpr);
    m_stars.fill(Qt::transparent);
    QPainter sp(&m_stars);
    sp.setRenderHint(QPainter::Antialiasing);
    QRandomGenerator rng(20261119);
    const int n = width() * height() / 7000;
    for (int i = 0; i < n; ++i) {
      const QPointF at(rng.bounded(width()), rng.bounded(height()));
      const bool big = rng.bounded(12) == 0;
      sp.setPen(Qt::NoPen);
      sp.setBrush(theme::alpha(big ? c.accentText : c.textMute, big ? 70 : 25 + rng.bounded(45)));
      sp.drawEllipse(at, big ? 1.3 : 0.7, big ? 1.3 : 0.7);
    }
    m_starsDpr = dpr;
  }
  p.drawPixmap(0, 0, m_stars);
  p.setRenderHint(QPainter::Antialiasing);

  // the links: from the conversation's edge to the node's nearest side, a star at each end
  struct L { QPointF a, b; bool on; };
  QList<L> links;
  for (const Link k : {Collect, Train, Compare}) links << L{linkLine(k).p1(), linkLine(k).p2(), active(k)};
  const double t = m_clock.elapsed() / 1000.0;
  for (const L& l : links) {
    p.setPen(QPen(l.on ? theme::alpha(c.accent, 170) : theme::alpha(c.borderHi, 150), l.on ? 1.6 : 1.1));
    p.drawLine(l.a, l.b);
    for (const QPointF& end : {l.a, l.b}) {
      p.setPen(Qt::NoPen);
      p.setBrush(theme::alpha(l.on ? c.accent : c.borderHi, 60));
      p.drawEllipse(end, 5, 5);
      p.setBrush(l.on ? c.accentText : c.textDim);
      p.drawEllipse(end, 2.2, 2.2);
    }
    if (l.on) {   // a point travels from the conversation to the node
      for (int k = 0; k < 2; ++k) {
        const double f = std::fmod(t / 1.3 + k * 0.5, 1.0);
        const QPointF at = l.a + (l.b - l.a) * f;
        QRadialGradient g(at, 9);
        g.setColorAt(0, theme::alpha(c.accentText, 220));
        g.setColorAt(1, theme::alpha(c.accentText, 0));
        p.setBrush(g);
        p.drawEllipse(at, 9, 9);
      }
    }
  }

  // the nodes' names above them, as on the drawing
  auto caption = [&](const QRect& r, const QString& name, bool on) {
    p.setFont(theme::mono(10, QFont::Bold, 2.2));
    p.setPen(on ? c.accentText : c.textDim);
    p.drawText(QRect(r.left() + 2, r.top() - kCaption, r.width(), kCaption - 6), Qt::AlignLeft | Qt::AlignBottom, name);
  };
  // a disambiguation: "TRAINING" is already a column header elsewhere (translated short)
  caption(m_collectR, QCoreApplication::translate("ai", "COLLECTION", "constellation node"), active(Collect));
  caption(m_trainR, QCoreApplication::translate("ai", "TRAINING", "constellation node"), active(Train));
  caption(m_compareR, QCoreApplication::translate("ai", "COMPARISON", "constellation node"), active(Compare));
}

}  // namespace aipage
