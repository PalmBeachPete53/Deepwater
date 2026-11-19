#pragma once
#include "ai/Slots.h"
#include "ai/Tools.h"
#include "ui/Theme.h"
#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>

// A skill's tick box for one AI › Agents environment (cube card and chat header). Ticking grants the skill to the
// environment at once (ai::Slots::setSkill): the next tool call is checked against it.
namespace aipage {
class SkillBox : public QAbstractButton {
public:
  SkillBox(int env, const ai::Skill& skill, QWidget* parent, int fontPx = 11) : QAbstractButton(parent), m_env(env), m_id(skill.id), m_px(fontPx) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);   // may shrink to its minimum (the label elides) in a row
    setText(skill.label);
    setToolTip(skill.description);
    setChecked(ai::Slots::instance().hasSkill(env, m_id));
    connect(this, &QAbstractButton::toggled, this, [this](bool on) { ai::Slots::instance().setSkill(m_env, m_id, on); });
    connect(&ai::Slots::instance(), &ai::Slots::skillsChanged, this, [this](int i) {
      if (i != m_env) return;
      const bool on = ai::Slots::instance().hasSkill(m_env, m_id);
      if (on != isChecked()) { QSignalBlocker b(this); setChecked(on); }
      update();
    });
  }
  void setEnvironment(int env) {
    m_env = env;
    QSignalBlocker b(this);
    setChecked(ai::Slots::instance().hasSkill(env, m_id));
    update();
  }
  QSize sizeHint() const override {
    return QSize(22 + QFontMetrics(theme::sans(m_px)).horizontalAdvance(text()) + 4, qMax(18, m_px + 9));
  }
  QSize minimumSizeHint() const override { return QSize(40, sizeHint().height()); }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const bool on = isChecked();
    const QRectF box(0.5, (height() - 14) / 2.0 + 0.5, 13, 13);
    p.setPen(QPen(on ? c.up : underMouse() ? c.borderHi : c.textMute, 1));
    p.setBrush(on ? theme::alpha(c.up, 50) : Qt::transparent);
    p.drawRoundedRect(box, 2, 2);
    if (on) {
      QPainterPath tick;
      tick.moveTo(box.left() + 3, box.center().y());
      tick.lineTo(box.left() + 5.6, box.bottom() - 3.2);
      tick.lineTo(box.right() - 2.8, box.top() + 3.2);
      p.setPen(QPen(c.up, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
      p.drawPath(tick);
    }
    p.setFont(theme::sans(m_px));
    p.setPen(on ? c.text : c.textDim);
    p.drawText(QRectF(20, 0, width() - 20, height()), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(p.font()).elidedText(text(), Qt::ElideRight, width() - 20));
  }
  void enterEvent(QEnterEvent*) override { update(); }
  void leaveEvent(QEvent*) override { update(); }
private:
  int m_env;
  QString m_id;
  int m_px;
};
}  // namespace aipage
