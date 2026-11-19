// AI › Agents: the downloaded local models (and the Claude Code integration of AI › LLM › Integration) on the left; four isolated environments drawn as cubes on the right.
// Drag a model onto a cube to load it there: the cube lights up from its core. Drag a loaded cube onto another to
// move its model; EJECT turns the cube off. Assignments persist (ai::Slots).
// Each lit cube runs its model in its own sandboxed process (ai::Engine); CHAT opens the conversation of that environment.
// Test hooks: DW_AI_SLOTS="i:owner/repo/file;…" (load before showing), DW_AI_HOVER=<slot> (drop-target highlight),
// DW_AI_LIGHT=<0..1> (freeze every lit cube at that intensity, for screenshots), DW_AI_CHAT=<slot> (open its chat),
// DW_AI_ASK=<question> (with DW_AI_CHAT: send it once the page is shown), DW_AI_SKILLS="i:skill,…;…" (tick boxes),
// DW_AI_TOOLTEST=<file> (run every data tool once, results written to the file), DW_AI_UNTICK_ON_CALL=1.
#include "app/Pages.h"
#include "ai/ModelHub.h"
#include "ai/Slots.h"
#include "ai/Engine.h"
#include "ai/Tools.h"
#include "ai/ClaudeCode.h"
#include <QFile>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>
#include "pages/ai/ChatView.h"
#include "pages/ai/SkillBox.h"
#include "pages/ai/VoxelCube.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QFileInfo>
#include <QStackedWidget>
#include <functional>
#include <cmath>

namespace aipage {
namespace {

const char* kMime = "application/x-deepwater-model";

QString bytes(qint64 b) {
  if (b <= 0) return QStringLiteral("—");
  const double v = double(b);
  if (v >= 1e9) return QString::number(v / 1e9, 'f', 2) + " GB";
  if (v >= 1e6) return QString::number(v / 1e6, 'f', 1) + " MB";
  return QString::number(v / 1e3, 'f', 0) + " KB";
}
QString quantOf(const QString& file) { ai::HubFile f; f.path = file; return f.quant(); }
qint64 sizeOf(const QString& repo, const QString& file) {
  for (const auto& m : ai::ModelHub::instance().installed()) if (m.repo == repo && m.file == file) return m.size;
  return 0;
}
QString displayName(const QString& repo, const QString& file) { ai::Slot s; s.repo = repo; s.file = file; return s.displayName(); }
QMimeData* mimeFor(const QString& repo, const QString& file, int fromSlot) {
  auto* m = new QMimeData;
  m->setData(kMime, (repo + "\n" + file + "\n" + QString::number(fromSlot)).toUtf8());
  return m;
}
bool readMime(const QMimeData* m, QString* repo, QString* file, int* fromSlot) {
  if (!m->hasFormat(kMime)) return false;
  const QStringList p = QString::fromUtf8(m->data(kMime)).split('\n');
  if (p.size() < 3) return false;
  *repo = p[0]; *file = p[1]; *fromSlot = p[2].toInt();
  return true;
}
QPixmap dragChip(const QString& text, qreal dpr) {
  const auto& c = theme::pal();
  const QFont f = theme::mono(12, QFont::Bold, 0.5);
  const int w = QFontMetrics(f).horizontalAdvance(text) + 34;
  QPixmap pm(QSize(w, 30) * dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent);
  QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(c.accent, 1)); p.setBrush(theme::alpha(c.panelAlt, 235)); p.drawRoundedRect(QRectF(0.5, 0.5, w - 1, 29), 3, 3);
  p.setPen(Qt::NoPen); p.setBrush(c.up); p.drawEllipse(QPointF(14, 15), 4, 4);
  p.setFont(f); p.setPen(c.text); p.drawText(QRectF(24, 0, w - 24, 30), Qt::AlignVCenter, text);
  return pm;
}

// ---------------------------------------------------------------------------------------------- model list
class ModelList : public QWidget {
public:
  explicit ModelList(QWidget* p) : QWidget(p) { setMouseTracking(true); rebuild(); }
  void rebuild() {
    m_models = ai::ModelHub::instance().installed();
    ai::LocalModel cc; cc.repo = ai::claudecode::kRepo; cc.file = ai::claudecode::kFile;   // integration, after the files
    m_models << cc;
    setMinimumHeight(int(m_models.size()) * kRow + 4);
    update();
  }
  QSize sizeHint() const override { return QSize(300, int(m_models.size()) * kRow + 4); }
protected:
  static constexpr int kRow = 60;
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    for (int i = 0; i < m_models.size(); ++i) {
      const auto& m = m_models[i];
      const QRectF r(0, i * kRow + 2, width(), kRow - 6);
      p.setPen(QPen(i == m_hover ? c.borderHi : c.border, 1));
      p.setBrush(i == m_hover ? c.panelAlt : c.panel);
      p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 3, 3);
      // grip dots: "drag me"
      p.setPen(Qt::NoPen); p.setBrush(c.textMute);
      for (int k = 0; k < 3; ++k) for (int j = 0; j < 2; ++j) p.drawEllipse(QPointF(r.left() + 12 + j * 5, r.center().y() - 6 + k * 6), 1.3, 1.3);
      const bool integ = ai::claudecode::isKey(m.repo, m.file);
      p.setFont(theme::mono(12, QFont::Bold)); p.setPen(c.text);
      p.drawText(QRectF(r.left() + 30, r.top() + 7, r.width() - 110, 20), Qt::AlignLeft | Qt::AlignVCenter,
                 QFontMetricsF(p.font()).elidedText(displayName(m.repo, m.file), Qt::ElideMiddle, r.width() - 120));
      p.setFont(theme::mono(10)); p.setPen(c.textMute);
      p.drawText(QRectF(r.left() + 30, r.top() + 28, r.width() - (integ ? 40 : 110), 18), Qt::AlignLeft | Qt::AlignVCenter,
                 QFontMetricsF(p.font()).elidedText(integ ? QCoreApplication::translate("ai", "integration · Anthropic's servers") : m.repo,
                                                    integ ? Qt::ElideRight : Qt::ElideMiddle, r.width() - (integ ? 44 : 120)));
      if (integ) {
        p.setFont(theme::mono(10, QFont::Bold, 1.0)); p.setPen(c.accentText);
        p.drawText(QRectF(r.right() - 110, r.top() + 7, 100, 20), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("ai", "INTEGRATION"));
        continue;
      }
      p.setFont(theme::mono(11, QFont::Medium)); p.setPen(c.textDim);
      p.drawText(QRectF(r.right() - 90, r.top() + 7, 80, 20), Qt::AlignRight | Qt::AlignVCenter, bytes(m.size));
      const QString q = quantOf(m.file);
      if (!q.isEmpty()) { p.setFont(theme::mono(10, QFont::Bold, 1.0)); p.setPen(c.accentText);
        p.drawText(QRectF(r.right() - 90, r.top() + 28, 80, 18), Qt::AlignRight | Qt::AlignVCenter, q); }
    }
  }
  int rowAt(const QPointF& pos) const { const int i = int(pos.y()) / kRow; return i >= 0 && i < m_models.size() ? i : -1; }
  void mouseMoveEvent(QMouseEvent* e) override {
    if ((e->buttons() & Qt::LeftButton) && m_press >= 0 && (e->position() - m_pressPos).manhattanLength() > 6) {
      const auto m = m_models[m_press];
      m_press = -1;
      auto* drag = new QDrag(this);
      drag->setMimeData(mimeFor(m.repo, m.file, -1));
      drag->setPixmap(dragChip(displayName(m.repo, m.file), devicePixelRatioF()));
      drag->setHotSpot(QPoint(14, 15));
      drag->exec(Qt::CopyAction);
      return;
    }
    const int h = rowAt(e->position());
    if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor); update(); }
  }
  void mousePressEvent(QMouseEvent* e) override { m_press = rowAt(e->position()); m_pressPos = e->position(); }
  void mouseReleaseEvent(QMouseEvent*) override { m_press = -1; }
  void leaveEvent(QEvent*) override { m_hover = -1; update(); }
private:
  QList<ai::LocalModel> m_models;
  int m_hover = -1, m_press = -1;
  QPointF m_pressPos;
};

// ---------------------------------------------------------------------------------------------- one environment
class CubeCard : public QWidget {
public:
  std::function<void(int)> onChat;
  explicit CubeCard(int index, QWidget* parent) : QWidget(parent), m_index(index), m_anim(new QVariantAnimation(this)) {
    setAcceptDrops(true);
    setMinimumSize(200, 210);   // the cube shrinks first; title, model line and EJECT always fit
    setMouseTracking(true);
    m_eject = new QPushButton(QCoreApplication::translate("ai", "EJECT"), this);
    m_eject->setCursor(Qt::PointingHandCursor);
    m_eject->setFont(theme::mono(10, QFont::Bold, 1.2));
    const auto& c = theme::pal();
    m_eject->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 3px; padding: 3px 10px; }"
                                   "QPushButton:hover { color: %3; border-color: %4; }")
                               .arg(c.textDim.name(), c.border.name(), c.down.name(), c.down.name()));
    connect(m_eject, &QPushButton::clicked, this, [this] { ai::Slots::instance().eject(m_index); });
    m_chat = new QPushButton(QCoreApplication::translate("ai", "CHAT"), this);
    m_chat->setCursor(Qt::PointingHandCursor);
    m_chat->setFont(theme::mono(10, QFont::Bold, 1.2));
    m_chat->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 3px 12px; }"
                                  "QPushButton:hover { background: %4; }")
                              .arg(theme::alpha(c.up, 40).name(QColor::HexArgb), c.up.name(), theme::alpha(c.up, 150).name(QColor::HexArgb),
                                   theme::alpha(c.up, 80).name(QColor::HexArgb)));
    connect(m_chat, &QPushButton::clicked, this, [this] { if (onChat) onChat(m_index); });
    for (const auto& sk : ai::skills()) m_skillBoxes << new SkillBox(m_index, sk, this);
    ai::Engine* eng = ai::Engines::instance().at(m_index);
    connect(eng, &ai::Engine::stateChanged, this, [this] { updatePulse(); update(); });
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_light = v.toReal(); update(); });
    m_pulse.setInterval(66);   // 15 fps gentle breathing while lit
    connect(&m_pulse, &QTimer::timeout, this, [this] { update(cubeRect().toAlignedRect().adjusted(-20, -20, 20, 20)); });
    m_clock.start();
    const bool on = !ai::Slots::instance().slot(m_index).isEmpty();
    m_light = on ? 1.0 : 0.0;
    if (qEnvironmentVariableIsSet("DW_AI_LIGHT") && on) m_light = qEnvironmentVariable("DW_AI_LIGHT").toDouble();
    if (qEnvironmentVariableIsSet("DW_AI_HOVER") && qEnvironmentVariableIntValue("DW_AI_HOVER") == m_index) m_dropHover = true;
    syncLabels();
  }
  void slotChanged() {
    const bool on = !ai::Slots::instance().slot(m_index).isEmpty();
    m_anim->stop();
    m_anim->setStartValue(m_light);
    m_anim->setEndValue(on ? 1.0 : 0.0);
    m_anim->setDuration(on ? 1400 : 600);
    m_anim->setEasingCurve(on ? QEasingCurve::OutCubic : QEasingCurve::InQuad);
    m_anim->start();
    m_lightStart = m_clock.elapsed();
    syncLabels();
    updatePulse();
  }

protected:
  // Height under the cube: drop hint, model lines or the "empty" note, skill boxes, EJECT / CHAT. An empty card has no
  // buttons (its boxes sit at the bottom) and drops its note when the cube would be small (the hint says the same).
  bool loaded() const { return !ai::Slots::instance().slot(m_index).isEmpty(); }
  static QString emptyNote() { return QCoreApplication::translate("ai", "Isolated environment, empty. Drag a downloaded model onto the cube."); }
  int emptyNoteHeight() const {   // the note's wrapped height at this card's width
    return QFontMetrics(theme::sans(11)).boundingRect(QRect(0, 0, qMax(40, width() - 32), 1000), Qt::TextWordWrap, emptyNote()).height() + 2;
  }
  bool showEmptyNote() const { return loaded() || height() - 62 - emptyNoteHeight() - 24 * m_skillBoxes.size() >= 120; }
  // loaded card too short for a cube above the model lines (1024 x 600 window, 2 x 2): cube on the left, lines beside it
  bool compact() const { return loaded() && height() - 126 - 24 * m_skillBoxes.size() < 90 && width() >= 260; }
  QRectF cubeRect() const {
    if (compact()) {
      const qreal side = qMax<qreal>(0, qMin<qreal>(width() * 0.4, height() - 100 - 22 * (m_skillBoxes.size() - 1)));
      return QRectF(14, 34, side, side);
    }
    const qreal below = loaded() ? 126 : showEmptyNote() ? 62 + emptyNoteHeight() : 60;
    const qreal side = qMax<qreal>(0, qMin<qreal>(width() - 24, height() - below - 24 * m_skillBoxes.size()));
    return QRectF((width() - side) / 2.0, 34, side, side);
  }
  void resizeEvent(QResizeEvent*) override {
    m_eject->adjustSize();
    m_eject->move(width() - m_eject->width() - 12, height() - m_eject->height() - 12);
    m_chat->adjustSize();
    m_chat->move(m_eject->x() - m_chat->width() - 8, m_eject->y());
    // skills: tick boxes just above the buttons (they belong to the environment, empty or not); at the bottom when
    // the card is empty (no buttons), so no empty band is left under them
    const int boxesBottom = loaded() ? m_eject->y() - 6 : height() - 10;
    for (int k = 0; k < m_skillBoxes.size(); ++k)
      m_skillBoxes[k]->setGeometry(14, boxesBottom - 20 - 22 * (int(m_skillBoxes.size()) - 1 - k), width() - 28, 20);
    m_cube = {};
  }
  void showEvent(QShowEvent*) override { updatePulse(); }
  void hideEvent(QHideEvent*) override { m_pulse.stop(); }

  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto& c = theme::pal();
    const auto& s = ai::Slots::instance().slot(m_index);
    const bool on = !s.isEmpty();
    // card
    p.setPen(QPen(m_dropHover ? c.accent : on ? theme::mix(c.border, c.up, 0.35 * m_light) : c.border, m_dropHover ? 2 : 1));
    p.setBrush(c.panel);
    p.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 4, 4);
    ui::drawBrackets(p, QRectF(rect()).adjusted(3, 3, -3, -3), m_dropHover ? c.accent : c.bracket, 10);
    // title row
    p.setFont(theme::mono(11, QFont::Bold, 2.4)); p.setPen(c.accentText);
    p.drawText(QPointF(16, 24), QCoreApplication::translate("ai", "ENVIRONMENT %1").arg(m_index + 1));
    const ai::Engine::State es = ai::Engines::instance().at(m_index)->state();
    const QString state = on ? ai::stateLabel(es) : QCoreApplication::translate("ai", "OFF");
    const QFont sf = theme::mono(9, QFont::Bold, 1.4);
    const qreal sw = QFontMetricsF(sf).horizontalAdvance(state) + 22;
    const QRectF pill(width() - 14 - sw, 12, sw, 18);
    const QColor sc = !on ? c.textMute : es == ai::Engine::State::Failed ? c.down : es == ai::Engine::State::Starting ? c.warn
                    : es == ai::Engine::State::Off ? c.textDim : c.up;
    p.setPen(QPen(theme::alpha(sc, 130), 1)); p.setBrush(theme::alpha(sc, on ? 40 : 18)); p.drawRoundedRect(pill, 3, 3);
    p.setPen(Qt::NoPen); p.setBrush(sc); p.drawEllipse(QPointF(pill.left() + 9, pill.center().y()), 3, 3);
    p.setFont(sf); p.setPen(sc); p.drawText(pill.adjusted(16, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, state);

    // cube
    const QRectF cr = cubeRect();
    const qreal dpr = devicePixelRatioF();
    if (m_cube.size() != cr.size().toSize() || m_cubeDpr != dpr || m_cubeRev != theme::revision()) {
      m_cube.render(cr.size().toSize(), dpr, c.up);
      m_cubeDpr = dpr; m_cubeRev = theme::revision();
    }
    p.drawPixmap(cr.topLeft(), m_cube.metal());
    double light = m_light;
    if (on && light > 0.999 && !qEnvironmentVariableIsSet("DW_AI_LIGHT")) {
      if (es == ai::Engine::State::Busy)            // answering: the core works, quick strong pulse
        light = 0.70 + 0.30 * (0.5 + 0.5 * std::sin(m_clock.elapsed() / 160.0));
      else if (es == ai::Engine::State::Starting)   // loading the weights: slow rise and fall
        light = 0.45 + 0.40 * (0.5 + 0.5 * std::sin(m_clock.elapsed() / 380.0));
      else if (es == ai::Engine::State::Ready)      // in memory: gentle breathing
        light = 0.86 + 0.14 * (0.5 + 0.5 * std::sin(m_clock.elapsed() / 900.0));
      else if (es == ai::Engine::State::Failed)
        light = 0.25;
      // standby (not in memory yet): steady light, no animation
    }
    if (light > 0.001) {
      p.save();
      p.setCompositionMode(QPainter::CompositionMode_Plus);
      p.setOpacity(qBound(0.0, light, 1.0));
      p.drawPixmap(cr.topLeft(), m_cube.glow());
      p.restore();
    }
    if (!on) {   // hint on an empty cube
      p.setFont(theme::mono(10, QFont::Medium, 1.6)); p.setPen(m_dropHover ? c.accent : c.textMute);
      p.drawText(QRectF(0, cr.bottom() - 4, width(), 18), Qt::AlignCenter,
                 m_dropHover ? QCoreApplication::translate("ai", "RELEASE TO LOAD") : QCoreApplication::translate("ai", "DROP A MODEL HERE"));
    }

    // model line(s)
    const bool side = compact();
    const qreal x0 = side ? cr.right() + 12 : 16, lw = width() - x0 - 16;
    const qreal y0 = side ? cr.center().y() - 29 : cr.bottom() + 18;
    if (on) {
      p.setFont(theme::mono(12, QFont::Bold)); p.setPen(c.text);
      p.drawText(QRectF(x0, y0, lw, 20), Qt::AlignLeft | Qt::AlignVCenter,
                 QFontMetricsF(p.font()).elidedText(s.displayName(), Qt::ElideMiddle, lw));
      const qint64 sz = sizeOf(s.repo, s.file);
      QStringList meta;
      if (s.isIntegration()) meta << QCoreApplication::translate("ai", "INTEGRATION") << QCoreApplication::translate("ai", "runs on Anthropic's servers");
      else {
        const QString q = quantOf(s.file);
        if (!q.isEmpty()) meta << q;
        meta << bytes(sz);
        const qint64 ram = ai::Slots::instance().memoryTotal();
        if (ram > 0) {
          const double pct = 100.0 * sz / ram;
          meta << (pct < 1 ? QCoreApplication::translate("ai", "< 1% of RAM") : QCoreApplication::translate("ai", "≈ %1% of RAM").arg(qRound(pct)));
        }
      }
      p.setFont(theme::mono(10)); p.setPen(c.textDim);
      if (side)   // beside the cube: the details may take two lines
        p.drawText(QRectF(x0, y0 + 22, lw, 36), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, meta.join("  ·  "));
      else
        p.drawText(QRectF(16, y0 + 20, width() - 32, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetricsF(p.font()).elidedText(meta.join("  ·  "), Qt::ElideRight, width() - 32));
    } else if (showEmptyNote()) {
      p.setFont(theme::sans(11)); p.setPen(c.textMute);
      const qreal textBottom = m_skillBoxes.isEmpty() ? height() - 10 : m_skillBoxes.first()->y() - 4;
      p.drawText(QRectF(16, y0, width() - 32, qMax<qreal>(20, textBottom - y0)), Qt::AlignLeft | Qt::TextWordWrap, emptyNote());
    }
  }

  void dragEnterEvent(QDragEnterEvent* e) override {
    QString r, f; int from;
    if (readMime(e->mimeData(), &r, &f, &from) && from != m_index) { e->acceptProposedAction(); m_dropHover = true; update(); }
  }
  void dragLeaveEvent(QDragLeaveEvent*) override { m_dropHover = false; update(); }
  void dropEvent(QDropEvent* e) override {
    m_dropHover = false;
    QString r, f; int from;
    if (!readMime(e->mimeData(), &r, &f, &from)) return;
    e->acceptProposedAction();
    if (from >= 0) ai::Slots::instance().eject(from);   // moved from another cube
    ai::Slots::instance().load(m_index, r, f);
  }
  // drag a loaded cube onto another one to move its model
  void mousePressEvent(QMouseEvent* e) override { m_pressPos = e->position(); }
  void mouseDoubleClickEvent(QMouseEvent* e) override {
    if (!ai::Slots::instance().slot(m_index).isEmpty() && cubeRect().contains(e->position()) && onChat) onChat(m_index);
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    const auto& s = ai::Slots::instance().slot(m_index);
    const bool overCube = cubeRect().contains(e->position());
    setCursor(!s.isEmpty() && overCube ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (!(e->buttons() & Qt::LeftButton) || s.isEmpty() || !cubeRect().contains(m_pressPos)) return;
    if ((e->position() - m_pressPos).manhattanLength() < 8) return;
    auto* drag = new QDrag(this);
    drag->setMimeData(mimeFor(s.repo, s.file, m_index));
    drag->setPixmap(dragChip(s.displayName(), devicePixelRatioF()));
    drag->setHotSpot(QPoint(14, 15));
    drag->exec(Qt::MoveAction);
  }

private:
  void syncLabels() {
    const bool on = !ai::Slots::instance().slot(m_index).isEmpty();
    m_eject->setVisible(on);
    m_chat->setVisible(on);
    resizeEvent(nullptr);   // the skill boxes move down when the card empties, up when it loads
    update();
  }
  void updatePulse() {
    const bool on = !ai::Slots::instance().slot(m_index).isEmpty();
    const bool steady = ai::Engines::instance().at(m_index)->state() == ai::Engine::State::Off ||
                        ai::Engines::instance().at(m_index)->state() == ai::Engine::State::Failed;
    if (on && !steady && isVisible() && !qEnvironmentVariableIsSet("DW_AI_LIGHT")) m_pulse.start(); else m_pulse.stop();
    update();
  }
  int m_index;
  VoxelCube m_cube; qreal m_cubeDpr = 0; int m_cubeRev = -1;
  QVariantAnimation* m_anim;
  QTimer m_pulse;
  QElapsedTimer m_clock;
  qint64 m_lightStart = 0;
  double m_light = 0;
  bool m_dropHover = false;
  QPointF m_pressPos;
  QPushButton* m_eject;
  QPushButton* m_chat;
  QList<SkillBox*> m_skillBoxes;
};

// ---------------------------------------------------------------------------------------------- page
class AgentsPage : public QWidget {
public:
  explicit AgentsPage(QWidget* parent) : QWidget(parent) {
    const auto& c = theme::pal();
    applyHookSlots();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12); root->setSpacing(10);

    auto* head = new QHBoxLayout; head->setSpacing(12);
    head->addWidget(ui::label(QCoreApplication::translate("ai", "AGENTS"), theme::title(), c.accentText, this));
    head->addWidget(ui::label(QCoreApplication::translate("ai", "four isolated environments"), theme::mono(11), c.textDim, this));
    head->addStretch(1);
    m_mem = ui::label(QString(), theme::mono(10), c.textDim, this);
    head->addWidget(m_mem);
    root->addLayout(head);

    auto* body = new QHBoxLayout; body->setSpacing(12);
    root->addLayout(body, 1);

    auto* left = new ui::Panel(QCoreApplication::translate("ai", "Models & integrations"), this);
    m_list = new ModelList(left);
    auto* scroll = new QScrollArea(left);
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(m_list);
    left->body()->addWidget(scroll, 1);
    m_empty = ui::label(QCoreApplication::translate("ai", "No model downloaded yet: get one in AI › LLM › Local models, then drag it onto a cube."),
                        theme::sans(12), c.textMute, left);
    m_empty->setWordWrap(true);
    left->body()->addWidget(m_empty);
    auto* hint = ui::label(QCoreApplication::translate("ai", "Drag a model onto a cube to load it. Drag a lit cube onto another to move its model."),
                           theme::sans(11), c.textMute, left);
    hint->setWordWrap(true);
    left->body()->addWidget(hint);
    left->setMinimumWidth(240);   // 1024 px window: 240 + the 2 × 2 cards (stretch 3 : 7 keeps the wide look)
    body->addWidget(left, 3);
    m_left = left;

    auto* gridHost = new QWidget(this);
    m_gridHost = gridHost;
    gridHost->installEventFilter(this);
    m_grid = new QGridLayout(gridHost);
    m_grid->setContentsMargins(0, 0, 0, 0); m_grid->setSpacing(12);
    for (int i = 0; i < ai::Slots::kCount; ++i) {
      m_cards << new CubeCard(i, gridHost);
      m_cards.last()->onChat = [this](int k) { openChat(k); };
    }
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(gridHost);
    m_chatView = new ChatView(m_stack);
    m_chatView->onBack = [this] { closeChat(); };
    m_stack->addWidget(m_chatView);
    body->addWidget(m_stack, 7);
    layoutCards();

    auto* foot = ui::label(QCoreApplication::translate("ai",
        "Each cube is an isolated environment: its own folder and its own sandboxed process, with no internet, no access to "
        "your files, nothing shared with the other cubes. It reads Deepwater's data only when its box is ticked. "
        "A cube holding Claude Code runs it on Anthropic's servers: what it reads through its ticked boxes is sent there."),
        theme::sans(11), c.textMute, this);
    foot->setWordWrap(true);
    root->addWidget(foot);

    connect(&ai::ModelHub::instance(), &ai::ModelHub::installedChanged, this, [this] { refresh(); });
    connect(&ai::Slots::instance(), &ai::Slots::changed, this, [this](int i) { if (i >= 0 && i < m_cards.size()) m_cards[i]->slotChanged(); refresh(); });
    refresh();
    m_foot = foot;
    // DW_AI_TOOLTEST=<out file>: runs every read_data tool with sample arguments and writes the results there
    if (qEnvironmentVariableIsSet("DW_AI_TOOLTEST")) runToolTest(qEnvironmentVariable("DW_AI_TOOLTEST"));
    // DW_AI_MLTEST=<out file>: calls the "ml" skill's tools one after the other (valid and invented arguments) as environment 1
    if (qEnvironmentVariableIsSet("DW_AI_MLTEST")) runMlTest(qEnvironmentVariable("DW_AI_MLTEST"));
    if (qEnvironmentVariableIsSet("DW_AI_CHAT")) {
      const int k = qEnvironmentVariableIntValue("DW_AI_CHAT");
      QTimer::singleShot(0, this, [this, k] {
        openChat(k);
        if (qEnvironmentVariableIsSet("DW_AI_ASK")) ai::Engines::instance().at(k)->ask(qEnvironmentVariable("DW_AI_ASK"));
        // DW_AI_UNTICK_ON_CALL=1: untick every skill of that environment as soon as the model starts writing a tool call
        // (the call then arrives without its skill: it must be refused)
        if (qEnvironmentVariableIsSet("DW_AI_UNTICK_ON_CALL")) {
          ai::Engine* e = ai::Engines::instance().at(k);
          connect(e, &ai::Engine::textArrived, this, [e, k] {
            if (e->messages().isEmpty() || !e->messages().last().text.contains("<tool_call>")) return;
            for (const auto& sk : ai::skills()) ai::Slots::instance().setSkill(k, sk.id, false);
          });
        }
      });
    }
  }
protected:
  void paintEvent(QPaintEvent*) override { QPainter p(this); p.fillRect(rect(), theme::pal().bg); }
  bool eventFilter(QObject* o, QEvent* e) override {
    if (o == m_gridHost && e->type() == QEvent::Resize) layoutCards();
    return QWidget::eventFilter(o, e);
  }
private:
  void openChat(int i) {
    if (i < 0 || i >= ai::Slots::kCount || ai::Slots::instance().slot(i).isEmpty()) return;
    m_chatView->setEnvironment(i);
    m_left->hide(); m_foot->hide();
    m_stack->setCurrentWidget(m_chatView);
    m_chatView->focusInput();
    ai::Engines::instance().at(i)->start();   // load the model while the user types
  }
  void closeChat() {
    m_stack->setCurrentIndex(0);
    m_left->show(); m_foot->show();
  }
  // four in a row or 2 × 2: whichever gives the bigger cube in the space the grid really has (card: cube side = min(w-24, h-120))
  void layoutCards() {
    const qreal w = m_gridHost->width(), h = m_gridHost->height(), gap = m_grid->spacing();
    const qreal rowSide = qMin((w - 3 * gap) / 4 - 24, h - 120);
    const qreal gridSide = qMin((w - gap) / 2 - 24, (h - gap) / 2 - 120);
    const bool row = rowSide > gridSide && (w - 3 * gap) / 4 >= 290;   // a row card must still fit title + state pill
    if (row == m_rowMode && m_grid->count() == m_cards.size()) return;
    m_rowMode = row;
    for (int i = 0; i < m_cards.size(); ++i) { m_grid->removeWidget(m_cards[i]); m_grid->addWidget(m_cards[i], row ? 0 : i / 2, row ? i : i % 2); }
    for (int i = 0; i < 4; ++i) { m_grid->setRowStretch(i, 0); m_grid->setColumnStretch(i, 0); }
    for (int i = 0; i < (row ? 1 : 2); ++i) m_grid->setRowStretch(i, 1);
    for (int i = 0; i < (row ? 4 : 2); ++i) m_grid->setColumnStretch(i, 1);
  }
  void refresh() {
    m_list->rebuild();
    m_empty->setVisible(ai::ModelHub::instance().installed().isEmpty());
    const auto& s = ai::Slots::instance();
    int loaded = 0; for (int i = 0; i < ai::Slots::kCount; ++i) if (!s.slot(i).isEmpty()) ++loaded;
    QString t = QCoreApplication::translate("ai", "%1 / 4 environments loaded").arg(loaded);
    if (s.memoryTotal() > 0)
      t += "  ·  " + QCoreApplication::translate("ai", "models ≈ %1 of %2 RAM").arg(bytes(s.memoryLoaded()), bytes(s.memoryTotal()));
    m_mem->setText(t);
    const bool over = s.memoryTotal() > 0 && s.memoryLoaded() > s.memoryTotal() * 0.75;
    m_mem->setStyleSheet(QString("color:%1;background:transparent;").arg((over ? theme::pal().warn : theme::pal().textDim).name()));
    m_mem->setToolTip(over ? QCoreApplication::translate("ai", "The loaded models together need more than 3/4 of this computer's memory.") : QString());
  }
  static void runToolTest(const QString& outFile) {
    const QList<QPair<QString, QJsonObject>> calls{
        {"get_quotes", QJsonObject{{"symbols", QJsonArray{"EURUSD", "gold", "SPX", "nonsense"}}}},
        {"get_price_history", QJsonObject{{"symbol", "EURUSD"}, {"bars", 5}}},
        {"get_economic_calendar", QJsonObject{{"days_ahead", 3}, {"min_impact", "high"}}},
        {"get_news", QJsonObject{{"max", 3}}},
        {"get_squawk", QJsonObject{{"max", 5}}},
        {"get_macro_data", QJsonObject{{"country", "France"}}},
        {"get_macro_data", QJsonObject{{"country", "USD"}, {"indicator", "inflation"}}},
        {"get_central_banks", QJsonObject{}},
        {"get_central_banks", QJsonObject{{"bank", "ECB"}}},
        {"get_cot", QJsonObject{{"market", "EUR"}, {"weeks", 3}}},
        {"get_retail_sentiment", QJsonObject{{"symbol", "EURUSD"}}},
        {"get_risk_sentiment", QJsonObject{}},
        {"get_bank_forecasts", QJsonObject{{"market", "EURUSD"}}},
        {"get_bank_research", QJsonObject{{"max", 3}}},
        {"get_market_hours", QJsonObject{}},
        {"get_technical_read", QJsonObject{{"symbol", "XAUUSD"}}},
        {"get_liquidity_pools", QJsonObject{{"symbol", "EURUSD"}}},
        {"get_seasonality", QJsonObject{{"symbol", "XAUUSD"}}},
        {"get_fundamental_scores", QJsonObject{}},
        {"get_track_record", QJsonObject{{"trades", 3}}},
        {"get_technical_coverage", QJsonObject{}},
        {"get_technical_data", QJsonObject{}},
        {"get_technical_data", QJsonObject{{"market", "BTCUSDT"}, {"timeframe", "1D"}, {"bars", 5}, {"end", "2020-03-13"}, {"columns", QJsonArray{"rsi14", "sma200"}}}},
        {"get_technical_data", QJsonObject{{"timeframe", "1m"}, {"bars", 200}, {"columns", QJsonArray{"rsi14", "ema20", "ema50", "ema200", "bb_upper", "bb_lower", "macd", "macd_signal", "obv", "vwap"}}}},
        {"get_technical_data", QJsonObject{{"timeframe", "1h"}, {"bars", 3}, {"end", "2018-02-09 12:00"}}},
        {"get_technical_data", QJsonObject{{"timeframe", "3h"}}},
        {"get_technical_data", QJsonObject{{"columns", QJsonArray{"nonsense"}}}},
        {"get_technical_data", QJsonObject{{"market", "ETHUSDT"}}},
        {"get_macro_chains", QJsonObject{}},
        {"get_macro_chains", QJsonObject{{"query", "united states inflation"}}},
        {"get_macro_chains", QJsonObject{{"query", "fed"}, {"kind", "implied"}}},
        {"get_macro_facts", QJsonObject{{"chain", "rate:us"}, {"last", 6}}},
        {"get_macro_facts", QJsonObject{{"chain", "nonsense"}}},
    };
    auto results = std::make_shared<QMap<int, QString>>();
    auto left = std::make_shared<int>(int(calls.size()));
    for (int k = 0; k < calls.size(); ++k) {
      const auto* t = ai::Tools::instance().find(calls[k].first);
      const QString head = "### " + calls[k].first + " " + QString::fromUtf8(QJsonDocument(calls[k].second).toJson(QJsonDocument::Compact));
      auto write = [results, left, k, head, outFile](const ai::ToolResult& r) {
        if (results->contains(k)) return;
        results->insert(k, head + (r.ok ? "  [ok]\n" : "  [FAILED]\n") + r.text + "\n");
        if (--*left > 0) return;
        QFile f(outFile);
        if (f.open(QIODevice::WriteOnly)) for (const auto& s : *results) f.write(s.toUtf8() + "\n");
      };
      if (!t) { write({false, "no such tool"}); continue; }
      QTimer::singleShot(40000, qApp, [write] { write({false, "timeout 40 s"}); });
      t->run(calls[k].second, write);
    }
  }
  static void runMlTest(const QString& outFile) {
    // DW_AI_MLTEST_CALLS=<json file>: [["tool", {args}], …] instead of the list below
    QList<QPair<QString, QJsonObject>> custom;
    if (qEnvironmentVariableIsSet("DW_AI_MLTEST_CALLS")) {
      QFile cf(qEnvironmentVariable("DW_AI_MLTEST_CALLS"));
      if (cf.open(QIODevice::ReadOnly))
        for (const auto& v : QJsonDocument::fromJson(cf.readAll()).array()) custom << qMakePair(v.toArray().at(0).toString(), v.toArray().at(1).toObject());
    }
    const QList<QPair<QString, QJsonObject>> calls = !custom.isEmpty() ? custom : QList<QPair<QString, QJsonObject>>{
        {"ml_status", {}},
        {"ml_methods", {}},
        {"ml_methods", QJsonObject{{"family", "reinforcement"}}},
        {"ml_load_dataset", QJsonObject{{"source", "prices"}, {"key", "EURUSD"}, {"interval", "1d"}}},
        {"ml_datasets", {}},
        {"ml_read_dataset", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"columns", QJsonArray{"close", "rsi_14"}}, {"last_rows", 3}}},
        {"ml_read_dataset", QJsonObject{{"dataset_id", "../../Preferences/x"}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "supervised"}, {"algorithm", "kmeans"}, {"target", "close"}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "supervised"}, {"algorithm", "hist_gb"}, {"target", "closing_price"}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "supervised"}, {"algorithm", "hist_gb"}, {"target", "close"},
                                 {"features", QJsonArray{"rsi_14", "sentiment"}}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "supervised"}, {"algorithm", "hist_gb"}, {"target", "close"},
                                 {"params", QJsonObject{{"n_trees", 50}}}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "supervised"}, {"algorithm", "hist_gb"}, {"target", "close"},
                                 {"target_transform", "direction"}, {"horizon", 5}, {"params", QJsonObject{{"n_estimators", 99999}, {"learning_rate", 0.1}}}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "unsupervised"}, {"algorithm", "kmeans"}}},
        {"ml_run", QJsonObject{{"run_id", "@last"}, {"wait_seconds", 25}}},
        {"ml_run_rows", QJsonObject{{"run_id", "@last"}, {"last_rows", 3}}},
        {"ml_train", QJsonObject{{"dataset_id", "prices-eurusd-1d"}, {"family", "reinforcement"}, {"algorithm", "q_learning"}, {"target", "close"},
                                 {"params", QJsonObject{{"episodes", 20}}}}},
        {"ml_run", QJsonObject{{"run_id", "@last"}, {"wait_seconds", 25}}},
        {"ml_runs", QJsonObject{{"last", 3}}},
    };
    auto out = std::make_shared<QString>();
    auto lastRun = std::make_shared<QString>();
    // DW_AI_MLTEST_SKIP=<n>: starts at call n (11 = straight to a valid ml_train, before anything checked the environment)
    auto idx = std::make_shared<int>(qEnvironmentVariableIntValue("DW_AI_MLTEST_SKIP"));
    auto next = std::make_shared<std::function<void()>>();
    *next = [=] {
      if (*idx >= calls.size()) {
        QFile f(outFile);
        if (f.open(QIODevice::WriteOnly)) f.write(out->toUtf8());
        return;
      }
      auto [name, args] = calls[(*idx)++];
      if (args.value("run_id").toString() == "@last") args["run_id"] = *lastRun;
      const auto* t = ai::Tools::instance().find(name);
      *out += "### " + name + " " + QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)) + "\n";
      if (!t) { *out += "no such tool\n\n"; QTimer::singleShot(0, qApp, *next); return; }
      ai::currentCaller() = ai::ToolCaller{0, "test"};
      t->run(args, [=](const ai::ToolResult& r) {
        *out += (r.ok ? "[ok] " : "[REFUSED/FAILED] ") + r.text + "\n\n";
        const auto m = QRegularExpression("run (\\d{8}-\\d{6}-[0-9a-f]+)").match(r.text);
        if (m.hasMatch() && name == "ml_train") *lastRun = m.captured(1);
        QTimer::singleShot(0, qApp, *next);
      });
    };
    QTimer::singleShot(qEnvironmentVariableIsSet("DW_AI_MLTEST_SKIP") ? 0 : 3000, qApp, *next);
  }
  static void applyHookSlots() {
    // DW_AI_SKILLS="2:read_data;3:" grants (or, with nothing after the colon, removes) skills before showing
    for (const QString& part : qEnvironmentVariable("DW_AI_SKILLS").split(';', Qt::SkipEmptyParts)) {
      const int colon = int(part.indexOf(':'));
      if (colon <= 0) continue;
      const int env = part.left(colon).toInt();
      const QStringList want = part.mid(colon + 1).split(',', Qt::SkipEmptyParts);
      for (const auto& sk : ai::skills()) ai::Slots::instance().setSkill(env, sk.id, want.contains(sk.id));
    }
    if (!qEnvironmentVariableIsSet("DW_AI_SLOTS")) return;
    for (const QString& part : qEnvironmentVariable("DW_AI_SLOTS").split(';', Qt::SkipEmptyParts)) {
      const int colon = int(part.indexOf(':'));
      const QString key = part.mid(colon + 1);
      if (colon > 0 && key == QString(ai::claudecode::kRepo) + "/" + ai::claudecode::kFile) {   // "0:integration/claude-code"
        ai::Slots::instance().load(part.left(colon).toInt(), ai::claudecode::kRepo, ai::claudecode::kFile);
        continue;
      }
      const int cut = int(key.indexOf('/', key.indexOf('/') + 1));
      if (colon > 0 && cut > 0) ai::Slots::instance().load(part.left(colon).toInt(), key.left(cut), key.mid(cut + 1));
    }
  }
  QLabel *m_mem, *m_empty, *m_foot = nullptr;
  QWidget* m_left = nullptr;
  QStackedWidget* m_stack = nullptr;
  ChatView* m_chatView = nullptr;
  ModelList* m_list;
  QGridLayout* m_grid;
  QWidget* m_gridHost = nullptr;
  bool m_rowMode = false;
  QList<CubeCard*> m_cards;
};
}  // namespace
}  // namespace aipage

using AgentsPageImpl = aipage::AgentsPage;
DW_PAGE("ai/agents", AgentsPageImpl)
