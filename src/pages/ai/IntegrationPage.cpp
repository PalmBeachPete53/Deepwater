// AI › LLM › Integration: Claude Code (ai::claudecode). Left: is it installed, signed in, which model and effort it uses.
// Right: the four AI › Agents environments, to load Claude Code into one (or eject it), and what it may do there.
// It can also be dragged onto a cube from the list of AI › Agents.
// Phones (pages/ai/PhoneScroll.h): one column (Claude Code, then the environments), the model and effort choices in
// combo boxes, 34 px buttons; the page scrolls. On iOS no program can be started: Claude Code shows "not found".
#include "pages/ai/LlmParts.h"
#include "pages/ai/PhoneScroll.h"
#include "ai/ClaudeCode.h"
#include "ai/Slots.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace aipage {
namespace {

QPushButton* button(const QString& text, bool primary, QWidget* parent) {
  auto* b = new QPushButton(text, parent);
  const auto& c = theme::pal();
  b->setCursor(Qt::PointingHandCursor);
  b->setFont(theme::mono(10, QFont::Bold, 1.2));
  b->setStyleSheet(primary
    ? QString("QPushButton { background: %1; color: %2; border: 0; border-radius: 3px; padding: 5px 12px; }"
              "QPushButton:hover { background: %3; } QPushButton:disabled { background: %4; color: %5; }")
          .arg(c.accent.name(), theme::onAccent().name(), theme::mix(c.accent, c.text, 0.2).name(), c.panelAlt.name(), c.textMute.name())
    : QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 3px; padding: 4px 10px; }"
              "QPushButton:hover { color: %3; border-color: %4; } QPushButton:disabled { color: %5; border-color: %2; }")
          .arg(c.textDim.name(), c.border.name(), c.text.name(), c.borderHi.name(), c.textMute.name()));
  return b;
}
QLabel* text(const QString& s, const QFont& f, const QColor& c, QWidget* p, bool wrap = false) {
  auto* l = ui::label(s, f, c, p);
  l->setWordWrap(wrap);
  return l;
}

class IntegrationPage : public QWidget {
public:
  explicit IntegrationPage(QWidget* parent) : QWidget(parent), m_scroll(this) {
    const auto& c = theme::pal();
    auto* root = new QVBoxLayout(this);
    m_root = root;
    root->setContentsMargins(16, 12, 16, 12); root->setSpacing(10);

    auto* head = new QHBoxLayout; head->setSpacing(12);
    head->addWidget(text(QCoreApplication::translate("ai", "INTEGRATION"), theme::title(), c.accentText, this));
    m_sub = text(QCoreApplication::translate("ai", "AI tools installed on this computer that a cube can hold"), theme::mono(11), c.textDim, this);
    head->addWidget(m_sub);
    head->addStretch(1);
    root->addLayout(head);
    m_head = head;

    auto* body = new QHBoxLayout; body->setSpacing(12);
    m_body = body;
    root->addLayout(body, 1);

    // ---------------------------------------------------------------- left: Claude Code itself
    auto* cc = new ui::Panel(QStringLiteral("Claude Code"), this);
    auto* grid = new QGridLayout; grid->setHorizontalSpacing(14); grid->setVerticalSpacing(8);
    auto row = [&](int r, const QString& name, QLabel** value) {
      grid->addWidget(text(name, theme::microLabel(), c.textMute, cc), r, 0, Qt::AlignLeft | Qt::AlignVCenter);
      *value = text(QString(), theme::mono(11), c.text, cc, true);
      (*value)->setTextInteractionFlags(Qt::TextSelectableByMouse);
      grid->addWidget(*value, r, 1);
    };
    row(0, QCoreApplication::translate("ai", "PROGRAM"), &m_path);
    row(1, QCoreApplication::translate("ai", "VERSION"), &m_version);
    row(2, QCoreApplication::translate("ai", "ACCOUNT"), &m_account);
    grid->setColumnStretch(1, 1);
    cc->body()->addLayout(grid);
    auto* recheckRow = new QHBoxLayout;
    m_recheck = button(QCoreApplication::translate("ai", "RECHECK"), false, cc);
    connect(m_recheck, &QPushButton::clicked, this, [] { ai::claudecode::Probe::instance().refresh(); });
    recheckRow->addWidget(m_recheck);
    recheckRow->addStretch(1);
    cc->body()->addLayout(recheckRow);
    m_install = text(QString(), theme::sans(12), c.textDim, cc, true);
    m_install->setTextFormat(Qt::RichText);
    m_install->setOpenExternalLinks(true);
    cc->body()->addWidget(m_install);

    cc->body()->addSpacing(8);
    cc->body()->addWidget(text(QCoreApplication::translate("ai", "MODEL"), theme::microLabel(), c.textMute, cc));
    m_model = new ui::PillBar({QCoreApplication::translate("ai", "DEFAULT"), "OPUS", "SONNET", "FABLE", "HAIKU"}, cc, ui::PillBar::Boxed);
    m_model->setCurrent(int(qMax<qsizetype>(0, ai::claudecode::modelAliases().indexOf(ai::claudecode::model()))));
    connect(m_model, &ui::PillBar::changed, this, [](int i) { ai::claudecode::setModel(ai::claudecode::modelAliases().value(i)); });
    cc->body()->addWidget(m_model, 0, Qt::AlignLeft);
    m_modelBox = pillCombo(m_model, {QCoreApplication::translate("ai", "DEFAULT"), "OPUS", "SONNET", "FABLE", "HAIKU"}, cc);
    cc->body()->addWidget(m_modelBox);
    cc->body()->addSpacing(4);
    cc->body()->addWidget(text(QCoreApplication::translate("ai", "EFFORT"), theme::microLabel(), c.textMute, cc));
    m_effort = new ui::PillBar({QCoreApplication::translate("ai", "DEFAULT"), QCoreApplication::translate("ai", "LOW"),
                                QCoreApplication::translate("ai", "MEDIUM"), QCoreApplication::translate("ai", "HIGH"),
                                QCoreApplication::translate("ai", "EXTRA HIGH"), QCoreApplication::translate("ai", "MAX")}, cc, ui::PillBar::Boxed);
    m_effort->setCurrent(int(qMax<qsizetype>(0, ai::claudecode::effortLevels().indexOf(ai::claudecode::effort()))));
    connect(m_effort, &ui::PillBar::changed, this, [](int i) { ai::claudecode::setEffort(ai::claudecode::effortLevels().value(i)); });
    cc->body()->addWidget(m_effort, 0, Qt::AlignLeft);
    m_effortBox = pillCombo(m_effort, {QCoreApplication::translate("ai", "DEFAULT"), QCoreApplication::translate("ai", "LOW"),
                                       QCoreApplication::translate("ai", "MEDIUM"), QCoreApplication::translate("ai", "HIGH"),
                                       QCoreApplication::translate("ai", "EXTRA HIGH"), QCoreApplication::translate("ai", "MAX")}, cc);
    cc->body()->addWidget(m_effortBox);
    cc->body()->addWidget(text(QCoreApplication::translate("ai", "DEFAULT keeps Claude Code's own choice. A change applies from the next question."),
                               theme::sans(11), c.textMute, cc, true));
    cc->body()->addStretch(1);
    body->addWidget(cc, 5);

    // ---------------------------------------------------------------- right: the cubes
    auto* cubes = new ui::Panel(QCoreApplication::translate("ai", "Load into an environment"), this);
    auto* cg = new QGridLayout; cg->setHorizontalSpacing(12); cg->setVerticalSpacing(8);
    for (int i = 0; i < ai::Slots::kCount; ++i) {
      cg->addWidget(text(QCoreApplication::translate("ai", "ENVIRONMENT %1").arg(i + 1), theme::mono(11, QFont::Bold, 1.6), c.accentText, cubes), i, 0);
      m_holds << text(QString(), theme::mono(11), c.textDim, cubes);
      cg->addWidget(m_holds.last(), i, 1);
      m_load << button(QString(), false, cubes);
      connect(m_load.last(), &QPushButton::clicked, this, [i] {
        auto& s = ai::Slots::instance();
        const auto& sl = s.slot(i);
        if (ai::claudecode::isKey(sl.repo, sl.file)) s.eject(i);
        else s.load(i, ai::claudecode::kRepo, ai::claudecode::kFile);
      });
      cg->addWidget(m_load.last(), i, 2, Qt::AlignRight);
    }
    cg->setColumnStretch(1, 1);
    cubes->body()->addLayout(cg);
    cubes->body()->addWidget(text(QCoreApplication::translate("ai", "Or drag Claude Code from the list of AI › Agents onto a cube. "
                                                                    "Its skill boxes and its chat are on the cube."),
                                  theme::sans(11), c.textMute, cubes, true));
    cubes->body()->addSpacing(10);
    cubes->body()->addWidget(text(QCoreApplication::translate("ai", "WHAT IT CAN DO IN A CUBE"), theme::microLabel(), c.textMute, cubes));
    cubes->body()->addWidget(text(QCoreApplication::translate("ai",
        "It answers in the cube's chat. Its own tools are switched off: no shell, no access to your files, no web search. "
        "It reads Deepwater's data only when a skill box of the cube is ticked, through Deepwater's tools served on this "
        "computer (127.0.0.1, a key per cube): each call is shown in the chat and written to the cube's journal."),
        theme::sans(12), c.textDim, cubes, true));
    cubes->body()->addSpacing(4);
    cubes->body()->addWidget(text(QCoreApplication::translate("ai",
        "Unlike a local model, it runs on Anthropic's servers with your own Claude sign-in: your questions and what its tools "
        "read are sent to Anthropic, and count against your Claude plan or API usage."),
        theme::sans(12), c.warn, cubes, true));
    cubes->body()->addStretch(1);
    body->addWidget(cubes, 5);

    connect(&ai::claudecode::Probe::instance(), &ai::claudecode::Probe::changed, this, [this] { sync(); m_scroll.relayout(); });
    connect(&ai::Slots::instance(), &ai::Slots::changed, this, [this] { sync(); });
    sync();
    // phones and short windows: the content's own height (the wrapped texts at this width), the page scrolls
    m_scroll.height = [this](int w, int) { return layoutHeight(m_root, w); };
  }

protected:
  void paintEvent(QPaintEvent*) override { QPainter p(this); p.fillRect(rect(), theme::pal().bg); }
  // A page built hidden gets Qt's default 100 x 30 first: no phone arrangement for that (it would leave the desktop
  // layouts measured differently), the real size arrives with the show.
  void resizeEvent(QResizeEvent*) override {
    if (!isVisible()) return;
    setPhone(ui::compact(width()));
    m_scroll.resize();
  }
  void showEvent(QShowEvent*) override {
    resizeEvent(nullptr);
    const auto& st = ai::claudecode::Probe::instance().status();
    if (!st.checking) ai::claudecode::Probe::instance().refresh();   // re-read each visit: it may have been installed meanwhile
  }

private:
  // One column on a phone: the subtitle under the title, Claude Code above the environments, combo boxes for the
  // five / six choices (the boxed bars are wider than the screen), finger-sized buttons.
  void setPhone(bool on) {
    if (on == m_phone) return;
    m_phone = on;
    m_head->setDirection(on ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_sub->setWordWrap(on);
    m_body->setDirection(on ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_model->setVisible(!on); m_modelBox->setVisible(on);
    m_effort->setVisible(!on); m_effortBox->setVisible(on);
    QList<QPushButton*> buttons = m_load;
    buttons << m_recheck;
    for (auto* b : buttons) b->setMinimumHeight(on ? 34 : 0);
    for (auto* l : m_holds) l->setMinimumWidth(on ? 40 : 0);   // a long model name is cut instead of widening the row
  }
  void sync() {
    const auto& c = theme::pal();
    const auto& st = ai::claudecode::Probe::instance().status();
    const bool found = !st.path.isEmpty();
    auto set = [](QLabel* l, const QString& t, const QColor& col) {
      l->setText(t); l->setStyleSheet(QString("color:%1;background:transparent;").arg(col.name()));
    };
    if (st.checking && !st.probed) set(m_path, QCoreApplication::translate("ai", "checking…"), c.textMute);
    else set(m_path, found ? st.path : QCoreApplication::translate("ai", "not found on this computer"), found ? c.text : c.down);
    set(m_version, st.version.isEmpty() ? QStringLiteral("—") : st.version, st.version.isEmpty() ? c.textMute : c.text);
    QString acct;
    QColor ac = c.textMute;
    if (!found || !st.probed) acct = QStringLiteral("—");
    else if (st.loggedIn) {
      acct = st.authMethod == "claude.ai" ? QCoreApplication::translate("ai", "signed in with a Claude account")
           : QCoreApplication::translate("ai", "signed in (%1)").arg(st.authMethod);
      ac = c.up;
    } else { acct = QCoreApplication::translate("ai", "not signed in"); ac = c.warn; }
    set(m_account, acct, ac);
    m_recheck->setEnabled(!st.checking);
    m_recheck->setText(st.checking ? QCoreApplication::translate("ai", "CHECKING…") : QCoreApplication::translate("ai", "RECHECK"));
    if (!found && st.probed)
      m_install->setText(QCoreApplication::translate("ai", "Install Claude Code (<a href='https://claude.com/claude-code' style='color:%1;'>claude.com/claude-code</a>), "
                                                          "sign in once by typing <b>claude</b> in Terminal, then RECHECK.").arg(c.accentText.name()));
    else if (found && st.probed && !st.loggedIn)
      m_install->setText(QCoreApplication::translate("ai", "Sign in once by typing <b>claude auth login</b> in Terminal, then RECHECK."));
    else m_install->clear();
    m_install->setVisible(!m_install->text().isEmpty());

    for (int i = 0; i < ai::Slots::kCount; ++i) {
      const auto& s = ai::Slots::instance().slot(i);
      const bool mine = ai::claudecode::isKey(s.repo, s.file);
      set(m_holds[i], s.isEmpty() ? QCoreApplication::translate("ai", "empty") : s.displayName(), mine ? c.up : s.isEmpty() ? c.textMute : c.textDim);
      m_load[i]->setText(mine ? QCoreApplication::translate("ai", "EJECT")
                              : s.isEmpty() ? QCoreApplication::translate("ai", "LOAD") : QCoreApplication::translate("ai", "REPLACE"));
      m_load[i]->setToolTip(!mine && !s.isEmpty() ? QCoreApplication::translate("ai", "Takes %1 out of this cube and loads Claude Code instead.").arg(s.displayName())
                                                  : QString());
    }
  }

  QLabel *m_path, *m_version, *m_account, *m_install;
  QPushButton* m_recheck;
  ui::PillBar *m_model, *m_effort;
  QList<QLabel*> m_holds;
  QList<QPushButton*> m_load;
  PhoneScroll m_scroll;
  QVBoxLayout* m_root;
  QLabel* m_sub;
  QBoxLayout *m_head, *m_body;
  QComboBox *m_modelBox, *m_effortBox;
  bool m_phone = false;
};
}  // namespace

QWidget* createIntegrationPart(QWidget* parent) { return new IntegrationPage(parent); }
}  // namespace aipage
