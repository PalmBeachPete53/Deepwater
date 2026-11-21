#pragma once
#include <QString>
#include <functional>
#include <memory>

class QWidget;
namespace ml { struct Run; }

namespace aipage {
// The ML page's result chart of a finished run, small, for AI › Agents' activity panel (src/pages/ai/AgentActivity):
// the map of the rows coloured by group with the groups along time (clustering: k-means, Gaussian mixture…; anomalies),
// the map of the rows or embeddings (reduction, self-supervised), the test period's actual and predicted or hit rate
// (supervised, semi-supervised), the simulated equity against buy & hold (reinforcement). Built from the run's files
// once (MlPage.cpp, the same widgets as the page, without their captions); widget == nullptr when the run is not done.
struct MlMiniChart {
  QWidget* widget = nullptr;
  QString title;                          // the page's own heading of that chart
  QString caption;                        // what the colours mean
  std::function<int(int width)> heightFor;
};
MlMiniChart mlMiniChart(const ml::Run& r, QWidget* parent);
// The same in two steps, the slow one off the window's thread: the run's files read and thinned (any thread), then the
// widgets built from them (window's thread, quick).
struct MlMiniRows;
std::shared_ptr<const MlMiniRows> mlMiniRows(const ml::Run& r);
MlMiniChart mlMiniChart(const ml::Run& r, const std::shared_ptr<const MlMiniRows>& rows, QWidget* parent);
}  // namespace aipage
