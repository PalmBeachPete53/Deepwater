#include "global/FxStrength.h"
#include <QCoreApplication>
#include "market/Candles.h"
#include "market/Instruments.h"
#include "market/QuoteHub.h"
#include <QHash>
#include <QMap>
#include <QTimeZone>
#include <cmath>

namespace global {

QStringList FxStrength::currencies() { return {"CAD", "NZD", "AUD", "GBP", "JPY", "CHF", "USD", "EUR"}; }
QStringList FxStrength::timeframeLabels() { return {"5m", "15m", "1H", "4H", "D", "W"}; }
int FxStrength::timeframeSeconds(Timeframe tf) {
  switch (tf) {
    case M5: return 300; case M15: return 900; case H1: return 3600; case H4: return 14400;
    case D1: return 86400; case W1: return 7 * 86400;
  }
  return 3600;
}

struct TfSpec { const char* interval; const char* range; int refreshSec; };
static TfSpec spec(FxStrength::Timeframe tf) {
  switch (tf) {
    case FxStrength::M5: return {"5m", "5d", 120};
    case FxStrength::M15: return {"15m", "1mo", 300};
    case FxStrength::H1: return {"1h", "3mo", 600};
    case FxStrength::H4: return {"1h", "6mo", 1800};    // Yahoo has no 4h bars: 1h aggregated
    case FxStrength::D1: return {"1d", "2y", 1800};
    case FxStrength::W1: return {"1wk", "10y", 3600};
  }
  return {"1h", "3mo", 600};
}

// Bar bucket of a timestamp. Days and weeks follow the London calendar date: Yahoo stamps a finished daily FX bar at
// the London midnight (23:00 UTC in summer) and the still-open bar of today, like a live quote, with the current time
// (a fixed half-day shift put both in tomorrow's bucket from 12:00 UTC). Saturday goes to Friday, Sunday (the evening
// open) to Monday; weeks start on Monday.
static qint64 bucket(qint64 t, FxStrength::Timeframe tf) {
  const int s = FxStrength::timeframeSeconds(tf);
  if (tf == FxStrength::D1 || tf == FxStrength::W1) {
    static const QTimeZone london("Europe/London");
    QDate d = QDateTime::fromSecsSinceEpoch(t, london).date();
    if (d.dayOfWeek() == 6) d = d.addDays(-1); else if (d.dayOfWeek() == 7) d = d.addDays(1);
    const qint64 day = QDate(1970, 1, 1).daysTo(d);  // days since 1970-01-01 (a Thursday)
    if (tf == FxStrength::D1) return day * 86400;
    return (day - ((day + 3) % 7)) * 86400;           // Monday of that week
  }
  return t / s * s;
}

FxStrength::FxStrength(QObject* parent) : QObject(parent) {
  const QStringList majors = currencies();
  for (const auto& in : market::instruments()) {
    if (in.category != market::Category::Forex) continue;
    const int b = majors.indexOf(in.base), q = majors.indexOf(in.quote);
    if (b < 0 || q < 0) continue;
    m_pairs.push_back({in.symbol, b, q});
  }
  m_refresh.setSingleShot(false);
  connect(&m_refresh, &QTimer::timeout, this, [this] { load(true); });
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, &FxStrength::onQuotes);
}

QString FxStrength::sourceNote() const {
  return QCoreApplication::translate("global", "Computed by Deepwater from %1 Yahoo Finance pairs · %2 bars").arg(m_pairs.size()).arg(timeframeLabels()[m_tf]);
}

void FxStrength::setTimeframe(Timeframe tf) {
  if (tf == m_tf && (!m_times.isEmpty() || m_pending > 0)) return;
  m_tf = tf;
  m_times.clear(); m_close.clear();
  emit changed();
  load(false);
  if (m_active) m_refresh.start(spec(m_tf).refreshSec * 1000);
}

void FxStrength::setActive(bool on) {
  if (on == m_active) return;
  m_active = on;
  QStringList syms; for (const auto& p : m_pairs) syms << p.symbol;
  auto& hub = market::QuoteHub::instance();
  if (on) {
    hub.subscribe(this, syms, true);
    // coming back after longer than one refresh period: the lines are stale, reload now (in the background, the old
    // lines stay until the new ones land) instead of showing them as live until the timer fires
    if (m_times.isEmpty() && m_pending == 0) load(false);
    else if (m_pending == 0 && (!m_loadedAt.isValid() || m_loadedAt.secsTo(QDateTime::currentDateTimeUtc()) >= spec(m_tf).refreshSec))
      load(true);
    m_refresh.start(spec(m_tf).refreshSec * 1000);
  } else {
    hub.setActive(this, false);
    m_refresh.stop();
  }
}

void FxStrength::load(bool refresh) {
  const int gen = ++m_generation;
  const TfSpec sp = spec(m_tf);
  m_pending = int(m_pairs.size());
  if (!refresh) { m_done = 0; m_error.clear(); }
  auto raw = std::make_shared<QVector<QVector<QPair<qint64, double>>>>(m_pairs.size());
  auto failures = std::make_shared<int>(0);
  for (int i = 0; i < m_pairs.size(); ++i) {
    market::candles(m_pairs[i].symbol, sp.interval, sp.range, this, [this, gen, i, raw, failures, refresh](const market::Series& s) {
      if (gen != m_generation) return;
      if (s.ok()) {
        auto& v = (*raw)[i];
        v.reserve(s.bars.size());
        for (const auto& b : s.bars) if (std::isfinite(b.c) && b.c > 0) v.push_back({b.t, b.c});
      } else {
        ++*failures;
      }
      --m_pending;
      if (!refresh) { ++m_done; emit progress(); }
      if (m_pending > 0) return;
      if (*failures > 4) {          // too many missing pairs: an honest mean needs (almost) all of them
        if (m_times.isEmpty()) m_error = QCoreApplication::translate("global", "%1 of 28 pairs unavailable").arg(*failures);
        emit changed();
        return;
      }
      m_raw = *raw;
      m_loadedAt = QDateTime::currentDateTimeUtc();
      rebuild();
      emit changed();
    });
  }
}

void FxStrength::rebuild() {
  const int np = int(m_pairs.size());
  // per pair: bucketed closes (last close of each bucket)
  QVector<QMap<qint64, double>> byBucket(np);
  QHash<qint64, int> count;
  for (int p = 0; p < np; ++p) {
    for (const auto& [t, c] : m_raw[p]) byBucket[p][bucket(t, m_tf)] = c;
    for (auto it = byBucket[p].cbegin(); it != byBucket[p].cend(); ++it) count[it.key()]++;
  }
  // keep the bars most pairs traded (drops stray weekend ticks of a single pair)
  QVector<qint64> times;
  for (auto it = count.cbegin(); it != count.cend(); ++it) if (it.value() >= np * 2 / 3) times << it.key();
  std::sort(times.begin(), times.end());
  QVector<QVector<double>> close(np, QVector<double>(times.size(), NAN));
  for (int p = 0; p < np; ++p) {
    double last = NAN;
    for (int i = 0; i < times.size(); ++i) {
      const auto it = byBucket[p].constFind(times[i]);
      if (it != byBucket[p].cend()) last = *it;
      close[p][i] = last;
    }
    // back-fill a late start with the first known close
    double first = NAN;
    for (double v : close[p]) if (std::isfinite(v)) { first = v; break; }
    for (double& v : close[p]) { if (std::isfinite(v)) break; v = first; }
  }
  m_times = times;
  m_close = close;
  m_error.clear();
}

double FxStrength::value(int ccy, int from, int i) const {
  if (from < 0 || i < 0 || from >= m_times.size() || i >= m_times.size()) return NAN;
  double sum = 0; int n = 0;
  for (int p = 0; p < m_pairs.size(); ++p) {
    const auto& pr = m_pairs[p];
    if (pr.base != ccy && pr.quote != ccy) continue;
    const double a = m_close[p][from], b = m_close[p][i];
    if (!(a > 0) || !(b > 0)) continue;
    sum += pr.base == ccy ? b / a - 1.0 : a / b - 1.0;
    ++n;
  }
  return n ? sum / n * 100.0 : NAN;
}

void FxStrength::onQuotes(const QStringList& symbols) {
  if (!m_active || m_times.isEmpty()) return;
  auto& hub = market::QuoteHub::instance();
  bool any = false;
  for (int p = 0; p < m_pairs.size(); ++p) {
    if (!symbols.contains(m_pairs[p].symbol)) continue;
    const market::Quote q = hub.quote(m_pairs[p].symbol);
    if (!q.valid() || !q.time.isValid()) continue;
    const qint64 b = bucket(q.time.toSecsSinceEpoch(), m_tf);
    if (b < m_times.last()) continue;                 // older than the last bar: nothing to do
    if (b > m_times.last()) {                         // a new bar has started: extend every pair
      if (b - m_times.last() > 8 * timeframeSeconds(m_tf) && m_tf < D1) continue;   // a gap (weekend): wait for candles
      m_times << b;
      for (auto& c : m_close) c << c.last();
    }
    m_close[p].last() = q.price;
    any = true;
  }
  if (any) emit ticked();
}
}
