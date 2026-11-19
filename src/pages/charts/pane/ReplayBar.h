#pragma once
// Charts tab — the bar replay toolbar inside a chart pane (chart.js startReplay: <div class="replay-bar">) and the
// follower tag ("Synced replay"). Port of quant-terminal public/js/chart.js + app.css .replay-bar (MIT, Copyright (c)
// 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include <QFrame>
#include <functional>

class QPushButton;
class QComboBox;
class QCheckBox;

namespace charts::ui::pane {

class ReplayBar : public QFrame {
public:
  // follower: only the "Synced replay" hint, transparent for the mouse (.replay-bar.follower).
  // withSync: the "All charts" checkbox (more than one pane in the layout), checked = settings.syncReplay.
  ReplayBar(QWidget* parent, bool follower, bool withSync = false, bool syncChecked = false);
  bool follower() const { return follower_; }
  void setPlaying(bool playing);                 // ▶ / ❚❚
  void restyle();
  void place(int bottomInset = 0);               // left 50%, bottom 36px (+ inset), translateX(-50%)

  std::function<void(const QString&)> onAct;     // "back" | "fwd" | "play" | "pick" | "exit"
  std::function<void(double)> onSpeed;
  std::function<void(bool)> onSync;

private:
  bool follower_;
  QPushButton* play_ = nullptr;
  QComboBox* speed_ = nullptr;
  QCheckBox* sync_ = nullptr;
};

}  // namespace charts::ui::pane
