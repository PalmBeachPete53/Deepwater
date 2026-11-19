#include "pages/charts/flow/FootprintFeed.h"
#include "charts/core/Series.h"
#include <algorithm>

namespace charts::ui::flow {

namespace {
// Start (ms) of the chart bar holding a time, with the chart's own bucket rule (weeks start on Monday, months on the 1st).
::charts::flow::FootprintBook::BucketFn bucketsFor(const QString& tf) {
  series::Tf t;
  try {
    t = series::parseTf(tf);
  } catch (...) {
    t = series::parseTf("1m");
  }
  if (t.unit == "s" || t.unit == "m" || t.unit == "h" || t.unit == "D")
    return ::charts::flow::FootprintBook::fixedBuckets(int64_t(t.seconds) * 1000);
  return [t](int64_t ms) {
    const int64_t sec = ms >= 0 ? ms / 1000 : -((-ms + 999) / 1000);
    return int64_t(series::bucketStart(sec, t)) * 1000;
  };
}
}  // namespace

FootprintFeed::FootprintFeed(const data::FlowMarket& market, const QString& tf, QObject* parent)
    : QObject(parent), market_(market), tf_(tf), stream_(data::TradeStream::acquire(market)), book_(bucketsFor(tf)) {
  rebuildTimer_.setSingleShot(true);
  notifyTimer_.setSingleShot(true);
  connect(&rebuildTimer_, &QTimer::timeout, this, &FootprintFeed::rebuild);
  connect(&notifyTimer_, &QTimer::timeout, this, [this] {
    const bool r = pendingRebuilt_;
    pendingRebuilt_ = false;
    emit changed(r);
  });
  connect(stream_.get(), &data::TradeStream::appended, this, &FootprintFeed::addNew);
  connect(stream_.get(), &data::TradeStream::historyChanged, this, [this] {
    if (!rebuildTimer_.isActive()) rebuildTimer_.start(250);
  });
  connect(stream_.get(), &data::TradeStream::becameReady, this, &FootprintFeed::rebuild);
  connect(stream_.get(), &data::TradeStream::statusChanged, this, [this] { notify(false); });
  if (stream_->ready()) rebuild();
}

FootprintFeed::~FootprintFeed() = default;

void FootprintFeed::addNew() {
  if (!stream_->ready()) return;
  const auto& tape = stream_->tape();
  if (tape.empty()) return;
  if (lastId_ < 0 || rebuildTimer_.isActive()) {   // a rebuild is due anyway
    if (!rebuildTimer_.isActive()) rebuildTimer_.start(0);
    return;
  }
  for (size_t i = tape.lowerBound(lastId_ + 1); i < tape.size(); ++i) book_.add(tape[i]);
  lastId_ = tape.lastId();
  // The tape keeps a budget of trades and drops the oldest as live ones come; the book only ever grew, so a 1m chart
  // left open piled up candles (tens of MB a day). Candles wholly before the tape's first trade are those a rebuild
  // would not make: once they are an eighth of the book (at least one), rebuild it from the tape (~5 ms for 300 000
  // trades; on a 1m chart about every few minutes to hours). The book has no way to drop its front (charts core).
  const auto& candles = book_.candles();
  if (candles.size() > 1) {
    const int64_t from = tape.front().timeMs;
    // first candle starting after the tape's first trade; every candle two places before it ended before that trade
    const auto after = std::partition_point(candles.begin() + 1, candles.end(),
                                            [from](const ::charts::flow::FootprintCandle& c) { return c.startMs <= from; });
    const size_t stale = size_t(after - candles.begin()) - 1;
    if (stale > 0 && stale * 8 >= candles.size() && !rebuildTimer_.isActive()) rebuildTimer_.start(0);
  }
  notify(false);
}

void FootprintFeed::rebuild() {
  if (!stream_->ready()) return;
  const auto& tape = stream_->tape();
  book_.clear();
  for (size_t i = 0; i < tape.size(); ++i) book_.add(tape[i]);
  lastId_ = tape.lastId();
  if (!tape.empty()) book_.setCoveredFrom(tape.front().timeMs);
  notify(true);
}

void FootprintFeed::notify(bool rebuilt) {
  pendingRebuilt_ = pendingRebuilt_ || rebuilt;
  if (!notifyTimer_.isActive()) notifyTimer_.start(100);
}

const ::charts::flow::FootprintCandle* FootprintFeed::complete(qint64 barTime) const {
  const auto* c = book_.find(int64_t(barTime) * 1000);
  return c && !book_.partial(*c) ? c : nullptr;
}

qint64 FootprintFeed::coveredFromMs() const {
  const auto& tape = stream_->tape();
  return stream_->ready() && !tape.empty() ? tape.front().timeMs : 0;
}

}  // namespace charts::ui::flow
