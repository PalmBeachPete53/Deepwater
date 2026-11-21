#include "app/NavDrawer.h"
#include "app/Pages.h"
#include "ui/Theme.h"
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QVariantAnimation>
#include <functional>

namespace app {

namespace {
constexpr int kMainH = 46, kSubH = 42;
}

// The rows: each main tab, then its sub-tabs. A row acts on release when the finger did not move (a drag scrolls).
class NavList : public QWidget {
public:
  explicit NavList(QWidget* parent) : QWidget(parent) { setAttribute(Qt::WA_OpaquePaintEvent); }
  int main = 0, sub = 0;
  std::function<void(int, int)> picked;
  int contentHeight() const {
    int h = 8;
    for (const auto& t : tabs()) h += kMainH + int(t.subs.size()) * kSubH + 6;
    return h + 8;
  }
  struct Row { QRect r; int main, sub; };   // sub -1: a main tab's title (not a target)
  QList<Row> rows() const {
    QList<Row> out;
    int y = 8;
    const auto& t = tabs();
    for (int i = 0; i < t.size(); ++i) {
      out << Row{QRect(0, y, width(), kMainH), i, -1};
      y += kMainH;
      for (int j = 0; j < t[i].subs.size(); ++j) { out << Row{QRect(0, y, width(), kSubH), i, j}; y += kSubH; }
      y += 6;
    }
    return out;
  }
protected:
  void paintEvent(QPaintEvent* e) override {
    QPainter p(this);
    p.setClipRegion(e->region());
    const auto& c = theme::pal();
    p.fillRect(rect(), theme::map("#090603"));
    const auto& t = tabs();
    for (const Row& r : rows()) {
      if (r.sub < 0) {
        p.fillRect(QRect(16, r.r.bottom(), r.r.width() - 32, 1), c.border);
        p.setFont(theme::mono(13, QFont::Bold, 2.0));
        p.setPen(r.main == main ? c.accent : c.text);
        p.drawText(r.r.adjusted(18, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, label(t[r.main].label).toUpper());
        continue;
      }
      const bool on = r.main == main && r.sub == sub;
      if (on) {
        p.setPen(QPen(theme::alpha(c.accent, 110), 1));
        p.setBrush(c.accentDim);
        p.drawRoundedRect(QRectF(r.r.adjusted(12, 3, -12, -3)).adjusted(.5, .5, -.5, -.5), 3, 3);
      } else if (r.r.contains(m_press) && m_pressing) {
        p.setPen(Qt::NoPen); p.setBrush(c.panelAlt); p.drawRoundedRect(r.r.adjusted(12, 3, -12, -3), 3, 3);
      }
      p.setFont(theme::mono(12, QFont::Medium, 1.0));
      p.setPen(on ? c.accent : c.textDim);
      p.drawText(r.r.adjusted(32, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, label(t[r.main].subs[r.sub].label).toUpper());
    }
  }
  void mousePressEvent(QMouseEvent* e) override { m_press = e->position().toPoint(); m_pressing = true; m_moved = false; update(); }
  void mouseMoveEvent(QMouseEvent* e) override {
    if (m_pressing && (e->position().toPoint() - m_press).manhattanLength() > 10) { m_moved = true; m_pressing = false; update(); }
  }
  void mouseReleaseEvent(QMouseEvent* e) override {
    const bool tap = m_pressing && !m_moved;
    m_pressing = false;
    update();
    if (!tap) return;
    for (const Row& r : rows())
      if (r.sub >= 0 && r.r.contains(e->position().toPoint())) { if (picked) picked(r.main, r.sub); return; }
  }
private:
  QPoint m_press;
  bool m_pressing = false, m_moved = false;
};

NavDrawer::NavDrawer(QWidget* parent) : QWidget(parent) {
  hide();
  m_panel = new QWidget(this);
  m_scroll = new QScrollArea(m_panel);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setWidgetResizable(false);
  m_list = new NavList(nullptr);
  m_scroll->setWidget(m_list);
  m_list->picked = [this](int m, int s) { emit navigate(m, s); close(); };
  m_anim = new QVariantAnimation(this);
  m_anim->setDuration(180);
  m_anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { place(v.toDouble()); });
  connect(m_anim, &QVariantAnimation::finished, this, [this] { if (m_t <= 0) hide(); });
}

int NavDrawer::panelWidth() const { return qMin(340, qMax(220, width() - 64)); }

void NavDrawer::place(double t) {
  m_t = t;
  const int w = panelWidth();
  m_panel->setGeometry(int(-w + w * t), 0, w, height());
  m_scroll->setGeometry(0, 0, w, height());
  m_list->resize(w, qMax(height(), m_list->contentHeight()));
  update();
}

void NavDrawer::open(int main, int sub) {
  m_list->main = main;
  m_list->sub = sub;
  m_list->update();
  if (QWidget* p = parentWidget()) setGeometry(p->rect());
  raise();
  show();
  // the current page's row in view
  int y = 0;
  for (const auto& r : m_list->rows()) if (r.main == main && r.sub == sub) y = r.r.top();
  place(m_t);
  m_scroll->verticalScrollBar()->setValue(qMax(0, y - height() / 3));
  m_anim->stop();
  m_anim->setStartValue(m_t);
  m_anim->setEndValue(1.0);
  m_anim->start();
}

void NavDrawer::close() {
  if (!isVisible()) return;
  m_anim->stop();
  m_anim->setStartValue(m_t);
  m_anim->setEndValue(0.0);
  m_anim->start();
}

void NavDrawer::resizeEvent(QResizeEvent*) { place(m_t); }

void NavDrawer::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(0, 0, 0, int(150 * m_t)));
  const QRect r = m_panel->geometry();
  p.fillRect(QRect(r.right() + 1, 0, 1, height()), theme::pal().border);
}

void NavDrawer::mousePressEvent(QMouseEvent* e) {
  if (!m_panel->geometry().contains(e->position().toPoint())) close();
}
}  // namespace app
