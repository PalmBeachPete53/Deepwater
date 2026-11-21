#include "pages/ai/MlCompareView.h"
#include "ml/Runs.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QLocale>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <cmath>

namespace aipage {
namespace {
using namespace ml::comparison;
QString T(const char* s) { return QCoreApplication::translate("ai", s); }

QString num(double v, int dec = 4) { return std::isfinite(v) ? QString::number(v, 'f', dec) : QStringLiteral("—"); }
QString pct(double v, int dec = 1) { return std::isfinite(v) ? QString::number(v * 100, 'f', dec) + "%" : QStringLiteral("—"); }
QString signedPct(double v, int dec = 1) { return std::isfinite(v) ? (v >= 0 ? "+" : "−") + QString::number(std::abs(v) * 100, 'f', dec) + "%" : QStringLiteral("—"); }
// balanced accuracy points: +1.5 pt
QString points(double v) { return std::isfinite(v) ? (v >= 0 ? "+" : "−") + QString::number(std::abs(v) * 100, 'f', 1) + " pt" : QStringLiteral("—"); }
QString pval(double p) { return !std::isfinite(p) ? QStringLiteral("—") : p < 0.001 ? QStringLiteral("< 0.001") : QString::number(p, 'f', 3); }

QLabel* label(const QString& s, const QFont& f, const QColor& c, QWidget* p, bool wrap = true) {
  auto* l = new QLabel(s, p);
  l->setFont(f); l->setWordWrap(wrap);
  l->setStyleSheet(QString("color:%1;background:transparent;").arg(c.name(QColor::HexArgb)));
  l->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return l;
}

// A painted table: the run (or market) column takes the spare width and is elided; the others fit their widest cell.
// Cells carry their colour; a row with an id opens that run when clicked.
class Table : public QWidget {
public:
  struct Cell { QString text; QColor color; bool bold = false; };
  struct Line { QString id, tip; QList<Cell> cells; };
  Table(const QStringList& heads, int stretchCol, QWidget* p) : QWidget(p), m_heads(heads), m_stretch(stretchCol) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }
  std::function<void(const QString&)> open;
  // Phones (set by the ML page): when the figures and a readable run column do not fit on one line, each row takes two:
  // the run (with its rank) across the full width, the figures under it, their headers in a smaller font.
  void setCompact(bool on) { compact = on; fitHeight(); }
  void add(const Line& l) { m_lines << l; fitHeight(); }
  QSize sizeHint() const override { return {400, height()}; }
  QSize minimumSizeHint() const override { return {200, height()}; }

protected:
  static constexpr int kRow = 22, kPad = 8;
  // headers wrap on two lines or more rather than widen a column past its figures
  static QFont headFont() { return theme::mono(9, QFont::Normal, 0.8); }
  int headHeight(const QVector<qreal>& w) const {
    const QFontMetricsF fh(headFont());
    qreal h = 0;
    for (int k = 0; k < m_heads.size(); ++k)
      h = std::max(h, fh.boundingRect(QRectF(0, 0, w[k] + 1, 1000), Qt::TextWordWrap, m_heads[k]).height());
    return int(std::ceil(h)) + 8;
  }
  void fitHeight() {
    m_head = twoLine() ? headHeight2(widths2()) : headHeight(widths());
    setFixedHeight(m_head + int(m_lines.size()) * rowH() + 2);
  }
  // two-line rows: the columns other than the run's (and its rank "#", which goes in front of the run)
  static constexpr int kPad2 = 4;
  static QFont headFont2() { return theme::mono(8); }
  bool skipped(int k) const { return k == m_stretch || (m_stretch == 1 && k == 0); }
  bool twoLine() const {
    if (!compact) return false;
    const QVector<qreal> w = widths();
    qreal used = 0;
    for (int k = 0; k < w.size(); ++k) if (k != m_stretch) used += w[k] + 2 * kPad;
    return used + 140 + 2 * kPad > width();   // the run column would get under 140 px
  }
  int rowH() const { return twoLine() ? 2 * kRow : kRow; }
  QVector<qreal> widths2() const {
    const QFontMetricsF fh(headFont2()), fc(theme::mono(10)), fb(theme::mono(10, QFont::Bold));
    QVector<qreal> w(m_heads.size(), 0);
    for (int k = 0; k < m_heads.size(); ++k) {
      if (skipped(k)) continue;
      for (const QString& word : m_heads[k].split(' ', Qt::SkipEmptyParts)) w[k] = std::max(w[k], fh.horizontalAdvance(word));
      for (const auto& l : m_lines)
        if (k < l.cells.size()) w[k] = std::max(w[k], (l.cells[k].bold ? fb : fc).horizontalAdvance(l.cells[k].text));
    }
    // the spare width shared out between the columns
    qreal used = 0; int n = 0;
    for (int k = 0; k < w.size(); ++k) if (!skipped(k)) { used += w[k] + 2 * kPad2; ++n; }
    const qreal extra = n ? std::max<qreal>(0, width() - used) / n : 0;
    for (int k = 0; k < w.size(); ++k) if (!skipped(k)) w[k] += extra;
    return w;
  }
  int headHeight2(const QVector<qreal>& w) const {
    const QFontMetricsF fh(headFont2());
    qreal h = 0;
    for (int k = 0; k < m_heads.size(); ++k)
      if (!skipped(k)) h = std::max(h, fh.boundingRect(QRectF(0, 0, w[k] + 1, 1000), Qt::TextWordWrap, m_heads[k]).height());
    return int(std::ceil(h)) + 8;
  }
  void paintTwoLine(QPainter& p) {
    const auto& c = theme::pal();
    const QVector<qreal> w = widths2();
    const int kHead = m_head;
    p.setPen(c.border); p.drawLine(QPointF(0, kHead - .5), QPointF(width(), kHead - .5));
    p.setFont(headFont2()); p.setPen(c.textMute);
    qreal x = 0;
    for (int k = 0; k < m_heads.size(); ++k) {
      if (skipped(k)) continue;
      p.drawText(QRectF(x + kPad2, 0, w[k] + 1, kHead - 4), Qt::AlignRight | Qt::AlignBottom | Qt::TextWordWrap, m_heads[k]);
      x += w[k] + 2 * kPad2;
    }
    for (int i = 0; i < m_lines.size(); ++i) {
      const auto& l = m_lines[i];
      const QRectF row(0, kHead + i * 2 * kRow, width(), 2 * kRow);
      if (i == m_hover && !l.id.isEmpty()) p.fillRect(row, c.panelAlt);
      p.setPen(theme::alpha(c.border, 120)); p.drawLine(row.bottomLeft() - QPointF(0, .5), row.bottomRight() - QPointF(0, .5));
      // first line: "#  run"
      if (m_stretch < l.cells.size()) {
        const Cell& run = l.cells[m_stretch];
        qreal rx = kPad2;
        if (m_stretch == 1 && !l.cells.isEmpty()) {
          p.setFont(theme::mono(10)); p.setPen(l.cells[0].color.isValid() ? l.cells[0].color : c.text);
          p.drawText(QRectF(rx, row.top(), 30, kRow), Qt::AlignLeft | Qt::AlignVCenter, l.cells[0].text);
          rx += QFontMetricsF(p.font()).horizontalAdvance(l.cells[0].text) + 8;
        }
        p.setFont(theme::mono(10, run.bold ? QFont::Bold : QFont::Normal));
        p.setPen(run.color.isValid() ? run.color : c.text);
        const qreal rw = width() - rx - kPad2;
        p.drawText(QRectF(rx, row.top(), rw, kRow), Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF(p.font()).elidedText(run.text, Qt::ElideRight, rw));
      }
      // second line: the figures
      x = 0;
      for (int k = 0; k < l.cells.size() && k < w.size(); ++k) {
        if (skipped(k)) continue;
        const Cell& cell = l.cells[k];
        p.setFont(theme::mono(10, cell.bold ? QFont::Bold : QFont::Normal));
        p.setPen(cell.color.isValid() ? cell.color : c.text);
        p.drawText(QRectF(x + kPad2, row.top() + kRow - 2, w[k], kRow), Qt::AlignRight | Qt::AlignVCenter,
                   QFontMetricsF(p.font()).elidedText(cell.text, Qt::ElideRight, w[k]));
        x += w[k] + 2 * kPad2;
      }
    }
  }
  void resizeEvent(QResizeEvent*) override { const int h = m_head; fitHeight(); if (h != m_head) update(); }
  QVector<qreal> widths() const {
    const QFontMetricsF fh(headFont()), fc(theme::mono(10)), fb(theme::mono(10, QFont::Bold));
    QVector<qreal> w(m_heads.size(), 0);
    for (int k = 0; k < m_heads.size(); ++k)
      for (const QString& word : m_heads[k].split(' ', Qt::SkipEmptyParts)) w[k] = std::max(w[k], fh.horizontalAdvance(word));
    for (const auto& l : m_lines)
      for (int k = 0; k < l.cells.size() && k < w.size(); ++k)
        if (k != m_stretch) w[k] = std::max(w[k], (l.cells[k].bold ? fb : fc).horizontalAdvance(l.cells[k].text));
    qreal used = 0;
    for (int k = 0; k < w.size(); ++k) if (k != m_stretch) used += w[k] + 2 * kPad;
    w[m_stretch] = std::max<qreal>(60, width() - used - 2 * kPad);
    return w;
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    if (twoLine()) { paintTwoLine(p); return; }
    const auto& c = theme::pal();
    const QVector<qreal> w = widths();
    const int kHead = m_head;
    p.setPen(c.border); p.drawLine(QPointF(0, kHead - .5), QPointF(width(), kHead - .5));
    qreal x = 0;
    p.setFont(headFont()); p.setPen(c.textMute);
    for (int k = 0; k < m_heads.size(); ++k) {
      const QRectF r(x + kPad, 0, w[k] + 1, kHead - 4);
      p.drawText(r, (k == m_stretch || k == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignBottom | Qt::TextWordWrap, m_heads[k]);
      x += w[k] + 2 * kPad;
    }
    for (int i = 0; i < m_lines.size(); ++i) {
      const auto& l = m_lines[i];
      const QRectF row(0, kHead + i * kRow, width(), kRow);
      if (i == m_hover && !l.id.isEmpty()) p.fillRect(row, c.panelAlt);
      p.setPen(theme::alpha(c.border, 120)); p.drawLine(row.bottomLeft() - QPointF(0, .5), row.bottomRight() - QPointF(0, .5));
      x = 0;
      for (int k = 0; k < l.cells.size() && k < w.size(); ++k) {
        const Cell& cell = l.cells[k];
        p.setFont(theme::mono(10, cell.bold ? QFont::Bold : QFont::Normal));
        p.setPen(cell.color.isValid() ? cell.color : c.text);
        const QRectF r(x + kPad, row.top(), w[k], kRow);
        p.drawText(r, (k == m_stretch || k == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                   QFontMetricsF(p.font()).elidedText(cell.text, Qt::ElideRight, w[k]));
        x += w[k] + 2 * kPad;
      }
    }
  }
  int lineAt(const QPointF& pos) const {
    const int i = int(std::floor((pos.y() - m_head) / rowH()));
    return pos.y() >= m_head && i >= 0 && i < m_lines.size() ? i : -1;
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    const int h = lineAt(e->position());
    if (h == m_hover) return;
    m_hover = h;
    const bool link = h >= 0 && !m_lines[h].id.isEmpty();
    setCursor(link ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setToolTip(h >= 0 ? m_lines[h].tip : QString());
    update();
  }
  void leaveEvent(QEvent*) override { m_hover = -1; update(); }
  void mousePressEvent(QMouseEvent* e) override {
    const int i = lineAt(e->position());
    if (i >= 0 && !m_lines[i].id.isEmpty() && open) open(m_lines[i].id);
  }

private:
  bool compact = false;
  QStringList m_heads;
  int m_stretch;
  int m_head = 24;
  QList<Line> m_lines;
  int m_hover = -1;
};

QString scoreHead(const QString& key) {
  if (key == "balanced_accuracy") return T("BALANCED ACC.");
  if (key == "mae") return T("MAE");
  if (key == "probe_balanced_accuracy") return T("PROBE BAL. ACC.");
  if (key == "probe_mae") return T("PROBE MAE");
  return key.toUpper();
}

// colours: a figure that beats its reference after the tries in green, one significantly worse in red
QColor edgeColor(double edge, double adjP) {
  const auto& c = theme::pal();
  if (!std::isfinite(edge)) return c.textMute;
  if (std::isfinite(adjP) && adjP < 0.05) return edge > 0 ? c.up : c.down;
  return c.text;
}
QColor pColor(double p) { return std::isfinite(p) && p < 0.05 ? theme::pal().accentText : theme::pal().textDim; }

QString verdictText(const Group& g) {
  const QString tries = T("counting the %1 setup(s) tried").arg(g.tries);
  switch (g.verdict) {
    case Verdict::BeatsBoth: return T("The best beats both no skill and persistence, %1.").arg(tries);
    case Verdict::BeatsNoSkillOnly:
      return (g.persBetter ? T("The best beats no skill (%1) but not persistence: the naive rule \"what was last seen happens again\" does better. Nothing a simple rule does not already give.")
                           : T("The best beats no skill (%1) but not persistence: the naive rule \"what was last seen happens again\" does as well. Nothing a simple rule does not already give."))
          .arg(tries);
    case Verdict::BeatsNoSkill: return T("The best beats no skill, %1 (no persistence on record for this run: made by an earlier version).").arg(tries);
    case Verdict::Worse:
      return g.beatsPersistence ? T("Even the best does worse than no skill, and not by chance, though it beats persistence here: none of these setups shows skill.")
                                : T("Even the best does worse than no skill, and not by chance: none of these setups shows skill.");
    case Verdict::NotBetter:
      return g.beatsPersistence ? T("Even the best does no better than no skill, though it beats persistence here: none of these setups shows skill.")
                                : T("Even the best does no better than no skill: none of these setups shows skill.");
    case Verdict::Luck:
      return (g.beatsPersistence ? T("After %1 setup(s) tried, the best is not distinguishable from no skill (adj. p %2): its edge can be luck, even though it beats persistence.")
                                 : T("After %1 setup(s) tried, the best is not distinguishable from no skill (adj. p %2): its edge can be luck."))
          .arg(g.tries).arg(pval(g.verdictAdjP));
    case Verdict::Unknown: return T("No test against no skill for these runs: their no-skill prediction was not recorded (made by an earlier version).");
    case Verdict::None: break;
  }
  return {};
}
QColor verdictColor(Verdict v) {
  const auto& c = theme::pal();
  return v == Verdict::BeatsBoth || v == Verdict::BeatsNoSkill ? c.up : v == Verdict::Worse || v == Verdict::NotBetter ? c.down : c.warn;
}

// "alpha=0.1 · gamma=0.95": the keys given (all when empty)
QJsonObject paramsOf(const QString& json) { return QJsonDocument::fromJson(json.toUtf8()).object(); }
QString paramText(const QString& json, const QStringList& only, bool all) {
  const QJsonObject o = paramsOf(json);
  QStringList parts;
  for (auto it = o.begin(); it != o.end(); ++it) {
    if (!all && !only.contains(it.key())) continue;
    const QJsonValue v = it.value();
    const QString val = v.isDouble() ? QString::number(v.toDouble(), 'g', 6) : v.isBool() ? (v.toBool() ? "true" : "false")
                      : v.isString() ? v.toString() : QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    parts << it.key() + "=" + val;
  }
  return parts.join(" · ");
}
// the parameters whose values differ between the group's rows: what tells its setups apart
QStringList differing(const QList<Row>& rows) {
  QSet<QString> keys;
  for (const Row& r : rows) for (const QString& k : paramsOf(r.params).keys()) keys.insert(k);
  QStringList out;
  for (const QString& k : keys) {
    QSet<QString> vals;
    for (const Row& r : rows) vals.insert(QString::fromUtf8(QJsonDocument(QJsonArray{paramsOf(r.params).value(k)}).toJson(QJsonDocument::Compact)));
    if (vals.size() > 1) out << k;
  }
  out.sort();
  return out;
}

// refs.json's reason (train.py --refs, Runs::completeReferences: English), in the interface language
QString whyText(const QString& w) {
  static const char* fixed[] = {
      QT_TRANSLATE_NOOP("ai", "the test rows' dates differ from the run's: the data set was rebuilt since"),
      QT_TRANSLATE_NOOP("ai", "the test rows' actual values differ from the run's: the data set was rebuilt since"),
      QT_TRANSLATE_NOOP("ai", "the rebuilt no-skill class differs from the run's"),
      QT_TRANSLATE_NOOP("ai", "its data set is no longer on disk"),
      QT_TRANSLATE_NOOP("ai", "the trainer stopped while adding them")};
  for (const char* f : fixed) if (w == QLatin1String(f)) return T(f);
  static const QRegularExpression rows(R"(^the data set now gives (\d+) test rows, the run had (\d+): it was rebuilt since$)"),
      ns(R"(^the rebuilt no-skill (\S+) \((.*)\) differs from the run's \((.*)\)$)");
  if (const auto m = rows.match(w); m.hasMatch())
    return T("the data set now gives %1 test rows, the run had %2: it was rebuilt since").arg(m.captured(1), m.captured(2));
  if (const auto m = ns.match(w); m.hasMatch())
    return T("the rebuilt no-skill %1 (%2) differs from the run's (%3)").arg(m.captured(1), m.captured(2), m.captured(3));
  return w;
}

QString runName(const QString& id) { const ml::Run* r = ml::Runs::instance().find(id); return r ? r->name : id; }
QString questionOf(const QString& id, const CompareViewHooks& h) {
  const ml::Run* r = ml::Runs::instance().find(id);
  return r && h.question ? h.question(*r) : QString();
}
}  // namespace

QString verdictLine(const Group& g) { return verdictText(g); }
QColor verdictTone(const Group& g) { return verdictColor(g.verdict); }

void addComparison(QVBoxLayout* into, QWidget* host, const Report& rep, const CompareViewHooks& hooks) {
  const auto& c = theme::pal();
  into->addWidget(label(T("COMPARISON · %1 TRAININGS").arg(rep.compared), theme::mono(12, QFont::Bold), c.text, host));
  into->addWidget(label(T("Every figure is computed by Deepwater from the trainings' own files, the same the agents quote. p: paired block bootstrap on the "
                          "same test rows; adj. p: p × the setups tried on that market and question (re-runs of one setup count once). Under 0.05: "
                          "unlikely to be chance. Click a row to open the training."),
                        theme::sans(11), c.textMute, host));
  if (!rep.leftOut.isEmpty()) {
    QStringList names;
    for (const QString& s : rep.leftOut.mid(0, 6)) names << runName(s.section(' ', 0, 0));
    into->addWidget(label(T("Left out, not finished: %1").arg(names.join(" · ")) + (rep.leftOut.size() > 6 ? " …" : QString()), theme::mono(10), c.textDim, host));
  }
  auto algo = [&](const QString& id) { return hooks.algorithm ? hooks.algorithm(id) : id; };
  auto params = [](const QString& p) { const QString t = paramText(p, {}, true); return t.isEmpty() ? QString() : "  " + t; };

  for (const Group& g : rep.groups) {
    into->addSpacing(10);
    if (g.mode == Group::OneSetupOnly) {
      into->addWidget(label(T("SAME MARKET · %1").arg(g.market), theme::microLabel(), c.accentText, host));
      into->addWidget(label(questionOf(g.onlyRun, hooks), theme::mono(10), c.textDim, host));
      into->addWidget(label(T("%1 runs of one setup only: the same result each time, nothing to rank.").arg(g.copiesOnly), theme::sans(11), c.textDim, host));
      continue;
    }
    const bool same = g.mode == Group::SameMarket;
    const QString first = g.rows.isEmpty() ? QString() : g.rows.first().id;
    if (same) {
      into->addWidget(label(T("SAME MARKET · %1").arg(g.market), theme::microLabel(), c.accentText, host));
      QString sub = questionOf(first, hooks);
      if (!g.testPeriod.isEmpty()) sub += " · " + T("test %1").arg(g.testPeriod);
      sub += " · " + (g.dataHash.isEmpty() ? T("data version not recorded (older runs)") : T("data %1").arg(g.dataHash));
      into->addWidget(label(sub, theme::mono(10), c.textDim, host));
      into->addWidget(label(T("%1 setup(s) here; %2 different setup(s) tried on this market and question in all.").arg(g.setups).arg(g.tries),
                            theme::mono(10), c.textMute, host));
    } else {
      into->addWidget(label(T("SEVERAL MARKETS · %1").arg(algo(g.algorithm) + params(g.params)), theme::microLabel(), c.accentText, host));
      into->addWidget(label(questionOf(first, hooks), theme::mono(10), c.textDim, host));
      into->addWidget(label(T("Each market against its own reference: raw scores do not compare across markets."), theme::mono(10), c.textMute, host));
    }

    Table* t = nullptr;
    const QStringList diff = differing(g.rows);
    auto who = [&](const Row& r) {
      const QString d = paramText(r.params, diff, false);
      QString s = same ? algo(r.algorithm) + (d.isEmpty() ? QString() : "  " + d) : r.market;
      if (same && r.copies > 1) s += QString(" ×%1").arg(r.copies);
      return s;
    };
    auto tip = [&](const Row& r) { return runName(r.id) + "\n" + algo(r.algorithm) + params(r.params) + "\n" + r.id; };
    if (g.kind == Kind::Predict || g.kind == Kind::Probe) {
      const QString sc = scoreHead(g.scoreKey);
      QStringList heads;
      if (same) heads << "#";
      heads << (same ? T("SETUP") : T("MARKET")) << sc << T("NO SKILL") << (g.values ? T("ERROR REMOVED") : T("EDGE")) << T("ADJ. P")
            << T("VS PERSISTENCE") << T("ADJ. P");
      t = new Table(heads, same ? 1 : 0, host);
      int k = 0;
      for (const Row& r : g.rows) {
        Table::Line l{r.id, tip(r), {}};
        if (same) l.cells << Table::Cell{QString::number(++k), c.textMute};
        l.cells << Table::Cell{who(r), c.text, k == 1 && same};
        l.cells << Table::Cell{g.values ? num(r.value) : pct(r.value), c.text, true};
        l.cells << Table::Cell{g.values ? num(r.reference) : pct(r.reference), c.textDim};
        l.cells << Table::Cell{g.values ? signedPct(r.edge) : points(r.edge), edgeColor(r.edge, r.adjP), true};
        l.cells << Table::Cell{pval(r.adjP) + (same ? QString() : QString(" (%1)").arg(r.tries)), pColor(r.adjP)};
        l.cells << Table::Cell{g.values ? signedPct(r.persEdge) : points(r.persEdge), edgeColor(r.persEdge, r.persAdjP)};
        l.cells << Table::Cell{pval(r.persAdjP), pColor(r.persAdjP)};
        t->add(l);
      }
    } else if (g.kind == Kind::Trade) {
      QStringList heads;
      if (same) heads << "#";
      heads << (same ? T("SETUP") : T("MARKET")) << T("TEST RETURN") << T("BUY & HOLD") << T("EXCESS") << T("RANDOM BEATEN");
      if (same) heads << T("SHARPE") << T("MAX DRAWDOWN");
      t = new Table(heads, same ? 1 : 0, host);
      int k = 0;
      for (const Row& r : g.rows) {
        Table::Line l{r.id, tip(r), {}};
        if (same) l.cells << Table::Cell{QString::number(++k), c.textMute};
        l.cells << Table::Cell{who(r), c.text, k == 1 && same};
        l.cells << Table::Cell{signedPct(r.value), theme::signColor(r.value), true};
        l.cells << Table::Cell{signedPct(r.reference), c.textDim};
        l.cells << Table::Cell{signedPct(r.edge), theme::signColor(r.edge), true};
        l.cells << Table::Cell{pct(r.beaten, 0), std::isfinite(r.beaten) && r.beaten >= 0.95 ? c.up : c.textDim};
        if (same) { l.cells << Table::Cell{num(r.sharpe, 2), c.textDim}; l.cells << Table::Cell{signedPct(r.maxDrawdown), c.textDim}; }
        t->add(l);
      }
    } else {
      QStringList heads;
      if (same) heads << "#";
      heads << (same ? T("SETUP") : T("MARKET")) << T("GROUPS") << T("SILHOUETTE");
      if (same) heads << T("DAVIES-BOULDIN") << T("BIC"); else heads << T("LONGEST GROUP (ROWS)");
      t = new Table(heads, same ? 1 : 0, host);
      int k = 0;
      for (const Row& r : g.rows) {
        Table::Line l{r.id, tip(r), {}};
        if (same) l.cells << Table::Cell{QString::number(++k), c.textMute};
        l.cells << Table::Cell{who(r), c.text, k == 1 && same};
        l.cells << Table::Cell{QString::number(r.groups), c.text};
        l.cells << Table::Cell{num(r.silhouette, 3), c.text, true};
        if (same) { l.cells << Table::Cell{num(r.daviesBouldin, 3), c.textDim}; l.cells << Table::Cell{num(r.bic, 0), c.textDim}; }
        else l.cells << Table::Cell{num(r.longest, 1), c.textDim};
        t->add(l);
      }
    }
    t->open = hooks.open;
    t->setCompact(hooks.compact);
    into->addWidget(t);

    // what the table says
    if (same && (g.kind == Kind::Predict || g.kind == Kind::Probe)) {
      if (!g.topA.isEmpty()) {
        const QString diff = g.values ? T("MAE lower by %1").arg(num(g.topDiff)) : T("balanced accuracy %1").arg(points(g.topDiff));
        into->addWidget(label(T("Best against second on their %1 shared test rows: %2, p %3. %4").arg(g.topRows).arg(diff, pval(g.topP),
                                g.topP < 0.05 ? T("The best is really better on these rows.")
                                              : T("Not significantly different: choose on other grounds (simpler model, steadier folds).")),
                              theme::sans(11), c.textDim, host));
      }
      into->addWidget(label(verdictText(g), theme::sans(12, QFont::DemiBold), verdictColor(g.verdict), host));
      {   // volatility targets: each run against HAR and EWMA (the trainer's paired test)
        QStringList v;
        auto word = [](double p, double pw) { return p < 0.05 ? T("better") : pw < 0.05 ? T("worse") : T("no difference"); };
        for (const auto& row : g.rows)
          if (std::isfinite(row.harP)) v << row.algorithm + " : HAR " + word(row.harP, row.harPWorse) + ", EWMA " + word(row.ewmaP, row.ewmaPWorse);
        if (!v.isEmpty())
          into->addWidget(label(T("Against the volatility references: %1.").arg(v.join(" · ")), theme::sans(11), c.textDim, host));
      }
      {   // in money: the test predictions traded after costs (runs since 2026-11-20)
        QStringList money;
        double bh = NAN, bp = NAN;
        for (const auto& row : g.rows) {
          if (!std::isfinite(row.tradeNet)) continue;
          money << row.algorithm + " " + signedPct(row.tradeNet);
          bh = row.tradeBuyHold; bp = row.tradeCostBp;
        }
        if (!money.isEmpty())
          into->addWidget(label(T("Traded after costs (%1 bp a side, long when the model says up): %2. Buy & hold on the same rows: %3.")
                                    .arg(QLocale().toString(bp, 'f', 1), money.join(" · "), signedPct(bh)),
                                theme::sans(11), c.textDim, host));
      }
      if (std::isfinite(g.pbo))
        into->addWidget(label(T("Overfitting across these %1 setups (PBO, on their %2 shared test rows): %3 % chance that the setup best on half the rows "
                                "is at or below the median on the other half. %4")
                                  .arg(g.setups).arg(g.pboRows).arg(QLocale().toString(g.pbo * 100, 'f', 0),
                                       g.pbo > 0.5 ? T("Above 50 %: picking the best here is mostly picking luck.")
                                                   : T("Under 50 %: the ranking holds from one half of the rows to the other.")),
                              theme::sans(11), c.textDim, host));
    } else if (g.kind == Kind::Trade) {
      if (same)
        into->addWidget(label(T("Noise: random agents trading as often returned %1 to %2 (5th to 95th percentile); differences inside that band are chance. "
                                "No paired test for simulated trading.").arg(signedPct(g.noiseLo), signedPct(g.noiseHi)),
                              theme::sans(11), c.textDim, host));
      else
        into->addWidget(label(T("Above buy & hold on %1 of %2 markets, beats 95 % of random agents on %3; mean excess %4.")
                                  .arg(g.beat).arg(g.known).arg(g.sig).arg(signedPct(g.meanEdge)),
                              theme::sans(12, QFont::DemiBold), g.beat * 2 > g.known ? c.up : g.beat == 0 ? c.down : c.warn, host));
    } else if (g.kind == Kind::Groups) {
      into->addWidget(label(same ? T("Silhouette: higher means groups further apart (above 0.5 well apart, near 0 overlapping). Davies-Bouldin: lower is better. "
                                     "BIC: lower is better, between Gaussian-mixture runs only. No test of significance for groupings.")
                                 : T("Descriptive only: groupings on different markets have no common score; compare how many groups and how long they last."),
                            theme::sans(11), c.textDim, host));
    }
    if (!same && (g.kind == Kind::Predict || g.kind == Kind::Probe))
      into->addWidget(label(T("Beats its own no skill on %1 of %2 markets, significantly (after tries) on %3; mean edge %4.")
                                .arg(g.beat).arg(g.known).arg(g.sig).arg(g.values ? signedPct(g.meanEdge) : points(g.meanEdge)) +
                                (g.sig == 1 && g.known > 1 ? " " + T("Significant on one market only: a setup that works on one market only is likely fitted to it.")
                                 : g.beat == 0 ? " " + T("It shows skill on none of them.") : QString()),
                            theme::sans(12, QFont::DemiBold), g.sig > 0 && g.sig * 2 >= g.known ? c.up : g.beat == 0 ? c.down : c.warn, host));
  }

  if (!rep.notCompared.isEmpty()) {
    into->addSpacing(10);
    into->addWidget(label(T("NOT COMPARED"), theme::microLabel(), c.textMute, host));
    QStringList lines;
    for (const auto& e : rep.notCompared.mid(0, 15))
      lines << "• " + runName(e.id) + " — " +
                   (e.why == Excluded::OtherPeriod ? T("same market and question as another training, but another test period: not the same rows.")
                    : e.why == Excluded::OtherData ? T("same market, question and test period as another training, but another version of the data set (rebuilt since, or not recorded for older runs): not the same rows.")
                                                   : T("no other training of this question on this market, and its setup was not run on another market."));
    if (rep.notCompared.size() > 15) lines << T("… and %1 more").arg(rep.notCompared.size() - 15);
    into->addWidget(label(lines.join("\n"), theme::sans(11), c.textDim, host));
  }
  if (!rep.withoutRefs.isEmpty()) {
    into->addSpacing(10);
    into->addWidget(label(T("WITHOUT REFERENCES"), theme::microLabel(), c.textMute, host));
    QStringList lines;
    for (const auto& w : rep.withoutRefs.mid(0, 10)) lines << "• " + runName(w.first) + " — " + whyText(w.second);
    into->addWidget(label(T("These older trainings could not get their no-skill and persistence columns afterwards (their — above). Re-running the same setup on today's data set gives a training with them.")
                              + "\n" + lines.join("\n"),
                          theme::sans(11), c.textDim, host));
  }
  if (rep.nothing)
    into->addWidget(label(T("Nothing to compare among these: tick trainings of the same question on one market, or one setup run on several markets."),
                          theme::sans(12), c.warn, host));
}

}  // namespace aipage
