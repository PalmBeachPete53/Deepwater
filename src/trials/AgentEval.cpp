#include "trials/AgentEval.h"
#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <random>

namespace trials::agenteval {

std::optional<int> parseDecision(const QString& answer) {
  static const QRegularExpression re(R"(DECISION\s*[:：]\s*\**\s*(LONG|SHORT|FLAT)\b)", QRegularExpression::CaseInsensitiveOption);
  std::optional<int> out;
  auto it = re.globalMatch(answer);
  while (it.hasNext()) {   // the last one: a model may quote the format before deciding
    const QString w = it.next().captured(1).toUpper();
    out = w == "LONG" ? 1 : w == "SHORT" ? -1 : 0;
  }
  return out;
}

namespace {
Line lineOf(const QVector<double>& r, const QVector<int>& pos) {
  Line l;
  l.n = int(r.size());
  if (r.isEmpty()) return l;
  double s = 0, s2 = 0;
  int inMarket = 0, wins = 0;
  for (int i = 0; i < r.size(); ++i) {
    s += r[i];
    s2 += r[i] * r[i];
    if (pos[i] != 0) { ++inMarket; if (r[i] > 0) ++wins; }
  }
  l.total = s;
  l.mean = s / r.size();
  l.sd = r.size() > 1 ? std::sqrt(std::max(0.0, (s2 - s * s / r.size()) / (r.size() - 1))) : NAN;
  l.exposure = double(inMarket) / r.size();
  l.hit = inMarket ? double(wins) / inMarket : NAN;
  return l;
}
// decisions are spaced at least a horizon apart: their returns do not overlap, an iid bootstrap of the paired difference
Versus versus(const QVector<double>& a, const QVector<double>& b) {
  Versus v;
  v.n = int(a.size());
  if (a.size() < 2 || a.size() != b.size()) return v;
  QVector<double> d(a.size());
  double s = 0;
  for (int i = 0; i < a.size(); ++i) { d[i] = a[i] - b[i]; s += d[i]; }
  v.meanDiff = s / d.size();
  if (d.size() < kMinTested) return v;   // a bootstrap of a handful of values says nothing: no p
  std::mt19937 g(17);
  std::uniform_int_distribution<int> pick(0, int(d.size()) - 1);
  int below = 0, above = 0;
  constexpr int kResamples = 4000;
  for (int k = 0; k < kResamples; ++k) {
    double m = 0;
    for (int i = 0; i < d.size(); ++i) m += d[pick(g)];
    m /= d.size();
    if (m <= 0) ++below;
    if (m >= 0) ++above;
  }
  v.p = double(below) / kResamples;
  v.pWorse = double(above) / kResamples;
  return v;
}
struct Rows { QVector<double> agent, bh, flat, trend, ml, agentMl; QVector<int> pa, pb, pf, pt, pm; int unparsed = 0; };
Tier tierOf(const QString& name, const Rows& r) {
  Tier t;
  t.name = name;
  t.agent = lineOf(r.agent, r.pa);
  t.buyHold = lineOf(r.bh, r.pb);
  t.flat = lineOf(r.flat, r.pf);
  t.trend = lineOf(r.trend, r.pt);
  t.ml = lineOf(r.ml, r.pm);
  t.vsBuyHold = versus(r.agent, r.bh);
  t.vsTrend = versus(r.agent, r.trend);
  t.vsMl = versus(r.agentMl, r.ml);
  t.unparsed = r.unparsed;
  return t;
}
}  // namespace

Summary score(const QList<Decision>& decisions, const Prices& prices, qint64 horizonSec, double cost, qint64 frontier) {
  Summary s;
  Rows all, a, b;
  QHash<int, double> perRepeat;
  for (const Decision& d : decisions) {
    const auto p0 = prices.closeAt(d.t), p1 = prices.closeAt(d.t + horizonSec);
    if (!p0 || !p1 || !(*p0 > 0)) { ++s.skipped; continue; }
    const double r = *p1 / *p0 - 1;
    auto ret = [&](int pos) { return pos * r - (pos != 0 ? 2 * cost : 0.0); };   // in at D, out at D + horizon
    const auto sma = prices.smaAt(d.t);
    const int trendPos = sma && *p0 > *sma ? 1 : 0;
    for (Rows* rows : {&all, frontier > 0 && d.t >= frontier ? &a : &b}) {
      rows->agent << ret(d.pos); rows->pa << d.pos;
      rows->bh << ret(1); rows->pb << 1;
      rows->flat << 0.0; rows->pf << 0;
      rows->trend << ret(trendPos); rows->pt << trendPos;
      if (d.mlKnown) { rows->ml << ret(d.ml); rows->pm << d.ml; rows->agentMl << ret(d.pos); }
      if (!d.parsed) ++rows->unparsed;
    }
    perRepeat[d.repeat] += ret(d.pos);
    ++s.decisions;
  }
  s.all = tierOf("all", all);
  s.a = tierOf("A", a);
  s.b = tierOf("B", b);
  QList<int> keys = perRepeat.keys();
  std::sort(keys.begin(), keys.end());
  for (int k : keys) s.repeatTotals << perRepeat.value(k);
  s.repeats = int(keys.size());
  if (s.repeatTotals.size() > 1) {
    double m = 0, v = 0;
    for (double x : s.repeatTotals) m += x;
    m /= s.repeatTotals.size();
    for (double x : s.repeatTotals) v += (x - m) * (x - m);
    s.repeatSd = std::sqrt(v / (s.repeatTotals.size() - 1));
  }
  return s;
}

QHash<QString, double> parseMonthly(const QString& answer) {
  QHash<QString, double> out;
  // "2025-03: 82350", "2025-03: ~82 350", "2025-03 = ≈ $82,350 (unsure)": a tilde, "about", narrow spaces are a model's ways
  static const QRegularExpression re(QString::fromUtf8(R"((20\d\d)-(\d\d)\s*[:=|]\s*(?:~|≈|about|approx\.?|ca\.?|environ)?\s*\$?\s*([0-9][0-9 ,.\x{00a0}\x{202f}]*))"),
                                     QRegularExpression::CaseInsensitiveOption);
  auto it = re.globalMatch(answer);
  while (it.hasNext()) {
    const auto m = it.next();
    QString num = m.captured(3).trimmed();
    num.remove(' ');
    num.remove(QChar(0x00a0));
    num.remove(QChar(0x202f));
    num.remove(',');   // thousands separators; prices are written without decimals or with a dot
    bool ok = false;
    const double v = num.toDouble(&ok);
    if (ok && v > 0) out.insert(m.captured(1) + "-" + m.captured(2), v);
  }
  return out;
}

Probe probe(const QHash<QString, double>& said, const QHash<QString, double>& truth, double tol) {
  Probe p;
  QStringList months = truth.keys();
  std::sort(months.begin(), months.end());
  for (const QString& m : months) {
    ProbeMonth pm;
    pm.month = m;
    pm.truth = truth.value(m);
    if (said.contains(m)) {
      pm.said = said.value(m);
      pm.errPct = (pm.said / pm.truth - 1) * 100;
      pm.known = std::fabs(pm.errPct) <= tol;
    }
    p.months << pm;
    // the last month known, and with at least 2 of the 3 months ending there known: a lucky guess on its own is no memory
    const int k = int(p.months.size());
    const int recent = (k >= 1 && p.months[k - 1].known) + (k >= 2 && p.months[k - 2].known) + (k >= 3 && p.months[k - 3].known);
    if (pm.known && recent >= 2) p.lastKnown = m;
  }
  if (!p.lastKnown.isEmpty()) {
    const QDate first = QDate::fromString(p.lastKnown + "-01", "yyyy-MM-dd");
    p.frontier = QDateTime(first.addMonths(1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
  }
  return p;
}

}  // namespace trials::agenteval
