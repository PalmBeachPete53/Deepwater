#include "analytics/Scanner.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QDateTime>
#include <QPointer>

namespace analytics {
Scanner::Scanner(QObject* parent) : QObject(parent) {
  for (const auto& i : market::instruments()) m_items.push_back({i, {}, false, false, {}});
  m_computeSeq = QVector<int>(m_items.size(), 0);
}

void Scanner::start() {
  if (m_pending > 0) return;
  const int gen = ++m_generation;
  m_proxyDone.clear();
  // Request order: each instrument's proxies just before the instrument, so rows appear from the first seconds.
  struct Req { bool proxy; QString key; int index; };
  QVector<Req> order; QSet<QString> seen;
  for (int i = 0; i < m_items.size(); ++i) {
    for (const auto& t : volumeSources(m_items[i].ins.symbol))
      if (t != "-" && !seen.contains(t)) { seen.insert(t); order.push_back({true, t, -1}); }
    order.push_back({false, m_items[i].ins.symbol, i});
  }
  m_pending = m_requested = order.size();
  emit progress(0, m_requested);
  for (const auto& r : order) {
    auto done = [this, gen] {
      if (gen != m_generation) return;
      --m_pending;
      emit progress(m_requested - m_pending, m_requested);
      if (m_pending == 0) emit finished();
    };
    if (r.proxy) {
      const QString t = r.key;
      market::candles(t, "1d", "2y", this, [this, t, gen, done](const market::Series& s) {
        if (gen != m_generation) return;
        onProxy(t, s); done();
      });
    } else {
      const int i = r.index;
      market::candles(r.key, "1d", "2y", this, [this, i, gen, done](const market::Series& s) {
        if (gen != m_generation) return;
        onDaily(i, s); done();
      });
    }
  }
}

void Scanner::onProxy(const QString& t, const market::Series& s) {
  m_proxy.insert(t, s.ok() ? s.bars : QVector<Bar>{});
  m_proxyDone.insert(t);
  // recompute instruments already computed without this proxy
  for (int i = 0; i < m_items.size(); ++i)
    if (m_daily.contains(m_items[i].ins.symbol) && volumeSources(m_items[i].ins.symbol).contains(t)) compute(i);
}

void Scanner::onDaily(int i, const market::Series& s) {
  if (!s.ok()) {
    if (!m_items[i].loaded) { m_items[i].failed = true; m_items[i].error = s.error; emit itemUpdated(i); }
    return;
  }
  m_daily.insert(m_items[i].ins.symbol, s.bars);
  compute(i);
}

void Scanner::compute(int i) {
  const auto& ins = m_items[i].ins;
  const auto bars = m_daily.value(ins.symbol);
  QVector<VolumeInput> vol;
  const QStringList src = volumeSources(ins.symbol);
  if (src.isEmpty()) vol.push_back({ins.yahoo, bars});
  else if (src.first() != "-")
    for (const auto& t : src) vol.push_back({t, m_proxy.value(t)});
  const bool crypto = ins.category == market::Category::Crypto;
  const qint64 today = QDateTime::currentSecsSinceEpoch() / 86400;
  const int seq = ++m_computeSeq[i];
  auto* w = new QFutureWatcher<TechRead>(this);
  connect(w, &QFutureWatcher<TechRead>::finished, this, [this, w, i, seq] {
    w->deleteLater();
    if (seq != m_computeSeq[i]) return;   // a newer read (with more proxies) was started: it alone is applied
    const TechRead r = w->result();
    m_items[i].read = r;
    m_items[i].loaded = r.ok;
    m_items[i].failed = !r.ok;
    m_items[i].error = r.error;
    emit itemUpdated(i);
  });
  w->setFuture(QtConcurrent::run([bars, vol, crypto, today] { return technicalRead(bars, vol, crypto, today); }));
}
}
