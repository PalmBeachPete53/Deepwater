#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QDateTime>
#include <QVector>

// Currency strength of the eight majors (USD EUR GBP JPY CHF CAD AUD NZD), computed by Deepwater from the
// 28 major pairs (Yahoo candles via market::candles, live last bar from market::QuoteHub).
// A currency's value between two bars is the unweighted mean of its % moves against the seven others:
//   strength(c, from, i) = mean_k ( P_ck(i) / P_ck(from) - 1 ) × 100,  P_ck = price of c in units of k.
// The eight values sum to ≈ 0. Charts re-base on the first bar on screen by passing it as `from`.
namespace global {

class FxStrength : public QObject {
  Q_OBJECT
public:
  enum Timeframe { M5, M15, H1, H4, D1, W1 };
  explicit FxStrength(QObject* parent = nullptr);

  static QStringList currencies();          // display order: CAD NZD AUD GBP JPY CHF USD EUR
  static QStringList timeframeLabels();      // 5m 15m 1H 4H D W
  static int timeframeSeconds(Timeframe tf);

  void setTimeframe(Timeframe tf);
  Timeframe timeframe() const { return m_tf; }
  void setActive(bool on);                   // live quotes + periodic refresh only while the chart is visible

  int size() const { return int(m_times.size()); }
  qint64 time(int i) const { return m_times.value(i); }   // UTC seconds, bar open
  double value(int ccy, int from, int i) const;            // % since bar `from`; NaN without data

  bool loading() const { return m_pending > 0 && m_times.isEmpty(); }
  int loaded() const { return m_done; }
  int total() const { return 28; }
  QString error() const { return m_error; }
  QString sourceNote() const;                // "Yahoo Finance · 28 pairs · 1H bars"

signals:
  void changed();       // new data (timeframe switch or refresh): views keep their zoom when possible
  void progress();      // loading progress (loaded()/total())
  void ticked();        // last bar updated from live quotes

private:
  struct Pair { QString symbol; int base, quote; };
  void load(bool refresh);
  void rebuild();
  void onQuotes(const QStringList& symbols);
  QVector<Pair> m_pairs;
  Timeframe m_tf = H1;
  int m_generation = 0, m_pending = 0, m_done = 0;
  QString m_error;
  QVector<QVector<QPair<qint64, double>>> m_raw;   // per pair: (time, close)
  QVector<qint64> m_times;
  QVector<QVector<double>> m_close;                // [pair][bar], forward-filled
  QTimer m_refresh;
  QDateTime m_loadedAt;                            // when the candles last loaded (UTC)
  bool m_active = false;
};
}
