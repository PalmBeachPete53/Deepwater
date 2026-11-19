#pragma once
// Order flow: the footprint's user settings, saved in the chart pane's cfg as "footprint": {...} (state.json).
//   { "mode": "bidask" | "delta" | "profile", "rows": 0 (= Auto) | ticks per row, "ratio": 3, "minVolume": 0,
//     "stack": 3, "poc": true, "totals": true, "buyColor": "", "sellColor": "", "pocColor": "" }
// Colours are "#rrggbb" or "" (= the theme's up / down / warn colours, so a theme change follows).
// fromJson never fails: missing or bad values take the defaults, numbers are clamped to sane ranges.
#include "charts/core/Footprint.h"
#include <QJsonObject>
#include <QString>

namespace charts::flow {

struct FootprintSettings {
  enum class Mode { BidAsk, Delta, Profile };
  Mode mode = Mode::BidAsk;
  int64_t rows = 0;             // ticks per row; 0 = Auto (follows the zoom)
  double ratio = 3.0;           // diagonal imbalance ratio, 1 … 100
  double minVolume = 0;         // ≥ 0
  int stackMin = 3;             // 2 … 20
  bool showPoc = true, showTotals = true;
  QString buyColor, sellColor, pocColor;

  static FootprintSettings fromJson(const QJsonObject& o);
  QJsonObject toJson() const;   // only what differs from the defaults (empty object = all defaults)
  FootprintParams params() const;   // groupTicks = rows (1 when Auto: the layer sets it per frame)
  static QString modeKey(Mode m);
};

}  // namespace charts::flow
