// Port of the replay bar of quant-terminal public/js/chart.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/pane/ReplayBar.h"
#include "pages/charts/Style.h"
#include "pages/charts/Drawings.h"
#include <QPainter>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace charts::ui::pane {

namespace {

// <select> with the browser's chevron after the text (QSS hides Qt's own arrow).
class SpeedSelect : public QComboBox {
public:
  using QComboBox::QComboBox;
protected:
  void paintEvent(QPaintEvent* e) override {
    QComboBox::paintEvent(e);
    QPainter p(this);
    p.drawPixmap(QPointF(width() - 17, (height() - 12) / 2.0),
                 renderIcon(R"(<path d="M6 9l6 6 6-6"/>)", style::v().text, 12, devicePixelRatioF(), 2));
  }
};
}

ReplayBar::ReplayBar(QWidget* parent, bool follower, bool withSync, bool syncChecked)
    : QFrame(parent), follower_(follower) {
  setObjectName("replayBar");
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(6, 4, 6, 4);   // padding: 4px 6px (+ 1 px border drawn by the style sheet)
  row->setSpacing(4);
  auto hint = [&](const QString& t) {
    auto* l = new QLabel(t, this);
    l->setObjectName("hint");
    l->setProperty("hint", true);
    return l;
  };
  if (follower) {
    row->addWidget(hint(QCoreApplication::translate("charts", "Synced replay")));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* op = new QGraphicsOpacityEffect(this);
    op->setOpacity(0.8);
    setGraphicsEffect(op);
  } else {
    row->addWidget(hint(QCoreApplication::translate("charts", "Replay")));
    auto button = [&](const QString& text, const QString& tip, const QString& act) {
      auto* b = new QPushButton(text, this);
      b->setToolTip(tip);
      b->setCursor(Qt::PointingHandCursor);
      b->setFocusPolicy(Qt::NoFocus);
      connect(b, &QPushButton::clicked, this, [this, act] { if (onAct) onAct(act); });
      row->addWidget(b);
      return b;
    };
    button(QString::fromUtf8("◀◀"), QCoreApplication::translate("charts", "Step back"), "back");
    play_ = button(QString::fromUtf8("▶"), QCoreApplication::translate("charts", "Play / pause"), "play");
    button(QString::fromUtf8("▶▶"), QCoreApplication::translate("charts", "Step forward"), "fwd");
    speed_ = new SpeedSelect(this);
    speed_->setToolTip(QCoreApplication::translate("charts", "Speed"));
    speed_->setCursor(Qt::PointingHandCursor);
    speed_->setFocusPolicy(Qt::NoFocus);
    for (const auto& [label, v] : QList<QPair<QString, double>>{{"0.5x", 0.5}, {"1x", 1}, {"2x", 2}, {"5x", 5}, {"10x", 10}})
      speed_->addItem(label, v);
    speed_->setCurrentIndex(2);
    connect(speed_, &QComboBox::currentIndexChanged, this, [this](int i) { if (onSpeed) onSpeed(speed_->itemData(i).toDouble()); });
    row->addWidget(speed_);
    button(QCoreApplication::translate("charts", "Jump to…"), QCoreApplication::translate("charts", "Pick a new start bar"), "pick");
    if (withSync) {
      sync_ = new QCheckBox(QCoreApplication::translate("charts", "All charts"), this);
      sync_->setObjectName("hint");
      sync_->setToolTip(QCoreApplication::translate("charts", "Replay every chart in the layout to the same moment"));
      sync_->setChecked(syncChecked);
      sync_->setFocusPolicy(Qt::NoFocus);
      connect(sync_, &QCheckBox::toggled, this, [this](bool on) { if (onSync) onSync(on); });
      row->addWidget(sync_);
    }
    button(QCoreApplication::translate("charts", "Exit"), QCoreApplication::translate("charts", "Back to live"), "exit");
    auto* sh = new QGraphicsDropShadowEffect(this);
    sh->setBlurRadius(32);
    sh->setOffset(0, 8);
    sh->setColor(style::v().shadow);
    setGraphicsEffect(sh);
  }
  restyle();
}

void ReplayBar::setPlaying(bool playing) {
  if (play_) play_->setText(QString::fromUtf8(playing ? "❚❚" : "▶"));
}

void ReplayBar::restyle() {
  const auto& v = style::v();
  const QFont f = style::font(13);
  setFont(f);
  for (QWidget* w : findChildren<QWidget*>()) w->setFont(f);
  setStyleSheet(QString(R"(
    QFrame#replayBar { background: %1; border: 1px solid %2; border-radius: 6px; }
    QFrame#replayBar QPushButton { height: 28px; min-height: 28px; max-height: 28px; border: 0; background: transparent;
      border-radius: 4px; padding: 0 8px; color: %3; }
    QFrame#replayBar QPushButton:hover { background: %4; }
    QFrame#replayBar QComboBox { height: 28px; min-height: 28px; border: 0; background: transparent; border-radius: 4px;
      padding: 0 18px 0 8px; color: %3; }
    QFrame#replayBar QComboBox::down-arrow { image: none; width: 0; }
    QFrame#replayBar QComboBox:hover { background: %4; }
    QFrame#replayBar QComboBox::drop-down { border: 0; width: 12px; }
    QFrame#replayBar QComboBox QAbstractItemView { background: %1; color: %3; border: 1px solid %2; selection-background-color: %4; }
    QFrame#replayBar QLabel#hint, QFrame#replayBar QCheckBox#hint { color: %5; padding: 0 6px; background: transparent; }
  )")
                    .arg(v.panel.name(QColor::HexArgb), v.border.name(QColor::HexArgb), v.text.name(QColor::HexArgb),
                         v.hover.name(QColor::HexArgb), v.muted.name(QColor::HexArgb)));
  if (auto* sh = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) sh->setColor(v.shadow);
  adjustSize();
}

void ReplayBar::place(int bottomInset) {
  if (!parentWidget()) return;
  adjustSize();
  const QWidget* p = parentWidget();
  move((p->width() - width()) / 2, p->height() - 36 - bottomInset - height());
}

}  // namespace charts::ui::pane
