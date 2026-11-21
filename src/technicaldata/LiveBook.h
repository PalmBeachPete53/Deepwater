#pragma once
// Data › Technical data, the live order book (user's request 2026-11-20: "il ne faut pas stocker les données en direct,
// ça sera seulement en mémoire cache car ça sera utile en trading live"). For every Binance pair whose settings ask
// for it (PairSpec::liveBook), the order book of its spot market and of its USD-M perpetual is kept live while
// Deepwater runs: Charts' DepthStream (src/charts/data/DepthStream.h, one per market, shared with any heatmap open on
// it), holding in memory the last 10 minutes at 100 ms and 6 hours at 1 s of heatmap columns (±0.5 % around the price)
// and 24 hours of bookDepth bands at 1 s (±0.2 … ±5 %, the archive's form). Nothing is written: this cache asks the
// stream not to record (a heatmap in Charts keeps its own recording setting).
// 2026-11-20 (agents' order flow tools, src/ai/OrderFlowTools.cpp): the executed trades of the same two markets too
// (Charts' TradeStream, memory only, never on disk), folded every 30 s into 1-minute footprints on the basis-point
// levels of the stored history (FootprintArchive.h), the last 48 hours kept: the bars of today that Binance's daily
// archives publish only tomorrow. Only whole minutes the tape covers without a hole count as live footprint.
// Started with the collection (CollectorStartup.cpp: 20 s after launch, never in a test profile on its own); a pair
// paused or no longer asking lets its streams go.
// Charts' heatmap recordings (<data>/charts/depth/*.dwhm, the heatmap's own disk setting) stay Charts' files (user's
// rule 2026-11-20: "l'enregistrement de la heatmap peut perdurer mais il ne doit pas faire partie de la base de données
// technical data"): nothing of them goes into technical-data.sqlite, and the bands this cache keeps come only from the
// live book (the stream's heatmap columns may be preloaded from those files at its start: Charts' view, not ours).
#include "charts/data/DepthStream.h"
#include "charts/data/TradeStream.h"
#include "technicaldata/LiveFootprint.h"
#include <QTimer>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <memory>

namespace technicaldata {

class LiveBook : public QObject {
  Q_OBJECT
public:
  static LiveBook& instance();
  void start();
  void stop();
  bool started() const { return m_started; }
  void setMarkets(const QStringList& m) { m_markets = m; }   // DW_TECHNICALDATA_ONLY
  QStringList pairs() const;                                 // the pairs whose book is kept now
  // The pair's stream ("spot" or "perp"), null when it is not kept.
  std::shared_ptr<charts::data::DepthStream> stream(const QString& pair, const QString& venue) const;
  std::shared_ptr<charts::data::TradeStream> trades(const QString& pair, const QString& venue) const;

  // The live footprint of a pair's market (LiveFootprint.h), its newest trades folded first; null when not kept.
  const LiveFootprint* footprint(const QString& pair, const QString& venue);

signals:
  void changed();   // a stream added or let go

private:
  LiveBook();
  void sync();
  void foldAll();
  bool m_started = false;
  QStringList m_markets;
  QHash<QString, std::shared_ptr<charts::data::DepthStream>> m_streams;   // "<pair>|spot" / "<pair>|perp"
  QHash<QString, std::shared_ptr<charts::data::TradeStream>> m_trades;    // same keys
  QHash<QString, LiveFootprint> m_fp;
  QHash<QString, LiveFold> m_fold;
  QTimer m_foldTimer;
};

}  // namespace technicaldata
