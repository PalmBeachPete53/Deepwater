#include "pages/ai/ChatView.h"
#include "pages/ai/VoxelCube.h"
#include "pages/ai/SkillBox.h"
#include "pages/ai/Fit.h"
#include "ai/Engine.h"
#include "ai/Slots.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <QRegularExpression>

namespace aipage {
namespace {
QPushButton* button(const QString& text, const QColor& fg, const QColor& border, QWidget* parent, bool filled = false) {
  auto* b = new QPushButton(text, parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setFont(theme::mono(10, QFont::Bold, 1.2));
  b->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 4px 12px; }"
                           "QPushButton:hover { background: %4; }"
                           "QPushButton:disabled { color: %5; border-color: %6; background: transparent; }")
                       .arg(filled ? theme::alpha(fg, 40).name(QColor::HexArgb) : QString("transparent"), fg.name(),
                            border.name(QColor::HexArgb), theme::alpha(fg, filled ? 80 : 30).name(QColor::HexArgb),
                            theme::pal().textMute.name(), theme::pal().border.name()));
  return b;
}

// Splits "<think>…</think>answer" (reasoning models) into its two parts; an unclosed block is reasoning still running.
void splitThinking(const QString& t, QString* thinking, QString* answer) {
  const int a = int(t.indexOf("<think>"));
  if (a < 0) { *thinking = QString(); *answer = t; return; }
  const int b = int(t.indexOf("</think>", a));
  *thinking = t.mid(a + 7, b < 0 ? -1 : b - a - 7).trimmed();
  *answer = b < 0 ? QString() : (t.left(a) + t.mid(b + 8)).trimmed();
}

// The answer without its <tool_call> blocks (shown as tool lines instead).
QString withoutCalls(QString t) {
  for (int a; (a = int(t.indexOf("<tool_call>"))) >= 0;) {
    const int b = int(t.indexOf("</tool_call>", a));
    t.remove(a, b < 0 ? t.size() - a : b + 12 - a);
  }
  return t.trimmed();
}
QString argsText(const QJsonObject& args) {
  QStringList l;
  for (auto it = args.begin(); it != args.end(); ++it) {
    QString v = it.value().isString() ? "\"" + it.value().toString() + "\"" : QString::fromUtf8(QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    if (v.size() > 60) v = v.left(57) + "…";
    l << it.key() + "=" + v;
  }
  return l.join(", ");
}

// Mechanical check of an answer written from tool results: every figure of the answer (decimals, or 100 and more,
// dates, times and years aside) must appear in what the tools returned or in the user's question. Signs are ignored
// (a fall of -0.29% may be written "0.29%"), French decimal commas are read, a figure rounded from the data counts.
QList<double> figures(QString t, QStringList* raw = nullptr) {
  static const QRegularExpression dates("\\b\\d{4}-\\d{2}-\\d{2}\\b|\\b\\d{1,2}:\\d{2}\\b|\\b\\d{1,2}/\\d{1,2}/\\d{2,4}\\b");
  t.replace(dates, " ");
  static const QRegularExpression numRe("(?<![\\w.])[-+−]?\\d[\\d,\\x{202f} ]*(?:\\.\\d+)?");
  QList<double> out;
  auto it = numRe.globalMatch(t);
  while (it.hasNext()) {
    QString tok = it.next().captured().trimmed();
    tok.remove(QChar(0x202f)); tok.replace(QChar(0x2212), '-'); tok.remove('+');
    QString clean = tok;
    clean.remove(' ');
    // 78,559 / 1,234,567 → thousands; 1,10280 / 0,29 → decimal comma (French)
    static const QRegularExpression thousands("^-?\\d{1,3}(,\\d{3})+(\\.\\d+)?$");
    if (thousands.match(clean).hasMatch()) clean.remove(',');
    else clean.replace(',', '.');
    bool ok = false;
    const double v = clean.toDouble(&ok);
    if (!ok) continue;
    const bool decimals = clean.contains('.');
    const double a = std::abs(v);
    if (!decimals && (a < 100 || (a >= 1900 && a <= 2100))) continue;   // counts, days, years: not checked
    out << v;
    if (raw) *raw << tok;
  }
  return out;
}
bool foundIn(double v, const QString& rawTok, const QList<double>& pool) {
  QString c = rawTok; c.remove(' '); c.remove(QChar(0x202f));
  const int dot = int(qMax(c.lastIndexOf('.'), c.lastIndexOf(',')));
  const int dec = (dot >= 0 && c.size() - dot - 1 != 3) ? int(c.size() - dot - 1) : 0;
  const double a = std::abs(v), scale = std::pow(10.0, dec);
  for (double p : pool) {
    const double b = std::abs(p);
    if (std::abs(a - b) <= 1e-9 * qMax(1.0, b)) return true;
    if (std::abs(std::round(b * scale) / scale - a) <= 0.5 / scale * 1e-6 + 1e-12) return true;   // rounded from the data
  }
  return false;
}

QString markdownHtml(const QString& md) {
  QTextDocument d;
  d.setDefaultFont(theme::sans(13));
  // no raw HTML from the model: it could imitate the app's own TOOL / "figures checked" lines
  d.setMarkdown(md, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
  // nor images (![](file:///…) would show a local file): each becomes its alt text
  for (QTextBlock b = d.begin(); b.isValid(); b = b.next()) {
    struct Img { int pos, len; QString alt; };
    QList<Img> imgs;
    for (auto it = b.begin(); !it.atEnd(); ++it) {
      const QTextFragment f = it.fragment();
      if (f.charFormat().isImageFormat())
        imgs.append({f.position(), f.length(), f.charFormat().property(QTextFormat::ImageAltText).toString()});
    }
    for (int k = int(imgs.size()) - 1; k >= 0; --k) {   // from the end: earlier positions stay valid
      QTextCursor cur(&d);
      cur.setPosition(imgs[k].pos);
      cur.setPosition(imgs[k].pos + imgs[k].len, QTextCursor::KeepAnchor);
      cur.insertText("[" + (imgs[k].alt.isEmpty() ? QCoreApplication::translate("ai", "image") : imgs[k].alt) + "]", QTextCharFormat());
    }
  }
  const QString h = d.toHtml();
  const int b = int(h.indexOf("<body"));
  const int s = b < 0 ? -1 : int(h.indexOf('>', b));
  const int e = int(h.lastIndexOf("</body>"));
  return s < 0 || e < 0 ? md.toHtmlEscaped() : h.mid(s + 1, e - s - 1);
}
}  // namespace

// The environment's cube, small, lit like the big one: steady in standby, breathing when ready, quick when answering.
class MiniCube : public QWidget {
public:
  explicit MiniCube(QWidget* p) : QWidget(p) {
    setFixedSize(52, 52);
    m_timer.setInterval(66);
    connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_clock.start();
  }
  void setState(bool on, ai::Engine::State s) {
    m_on = on; m_state = s;
    const bool anim = on && (s == ai::Engine::State::Ready || s == ai::Engine::State::Busy || s == ai::Engine::State::Starting);
    if (anim && !qEnvironmentVariableIsSet("DW_AI_LIGHT")) m_timer.start(); else m_timer.stop();
    update();
  }
protected:
  void hideEvent(QHideEvent*) override { m_timer.stop(); }
  void showEvent(QShowEvent*) override { setState(m_on, m_state); }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    const qreal dpr = devicePixelRatioF();
    if (m_cube.size() != size() || m_dpr != dpr || m_rev != theme::revision()) {
      m_cube.render(size(), dpr, theme::pal().up); m_dpr = dpr; m_rev = theme::revision();
    }
    p.drawPixmap(0, 0, m_cube.metal());
    if (!m_on) return;
    const double t = m_clock.elapsed();
    double light = 1.0;
    switch (m_state) {
      case ai::Engine::State::Busy: light = 0.70 + 0.30 * (0.5 + 0.5 * std::sin(t / 160.0)); break;
      case ai::Engine::State::Starting: light = 0.45 + 0.40 * (0.5 + 0.5 * std::sin(t / 380.0)); break;
      case ai::Engine::State::Ready: light = 0.86 + 0.14 * (0.5 + 0.5 * std::sin(t / 900.0)); break;
      case ai::Engine::State::Failed: light = 0.25; break;
      case ai::Engine::State::Off: break;
    }
    if (qEnvironmentVariableIsSet("DW_AI_LIGHT")) light = qEnvironmentVariable("DW_AI_LIGHT").toDouble();
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setOpacity(qBound(0.0, light, 1.0));
    p.drawPixmap(0, 0, m_cube.glow());
  }
private:
  VoxelCube m_cube; qreal m_dpr = 0; int m_rev = -1;
  QTimer m_timer; QElapsedTimer m_clock;
  bool m_on = false;
  ai::Engine::State m_state = ai::Engine::State::Off;
};

ChatView::ChatView(QWidget* parent) : QWidget(parent) {
  const auto& c = theme::pal();
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(14, 10, 14, 12); root->setSpacing(10);

  // header
  auto* head = new QHBoxLayout; head->setSpacing(12);
  m_back = button(QCoreApplication::translate("ai", "← ENVIRONMENTS"), c.textDim, c.border, this);
  connect(m_back, &QPushButton::clicked, this, [this] { if (onBack) onBack(); });
  head->addWidget(m_back, 0, Qt::AlignVCenter);
  m_cube = new MiniCube(this);
  head->addWidget(m_cube);
  auto* titles = new QVBoxLayout; titles->setSpacing(2);
  // eliding (full text in the tooltip): a long model name or repo must not hold AI › Agents wider than the window
  m_title = fit::label(QString(), theme::mono(11, QFont::Bold, 2.4), c.accentText, this);
  m_model = fit::label(QString(), theme::mono(12, QFont::Bold), c.text, this);
  m_info = fit::label(QString(), theme::mono(10), c.textDim, this);
  titles->addWidget(m_title); titles->addWidget(m_model); titles->addWidget(m_info);
  auto* skillRow = new QHBoxLayout; skillRow->setSpacing(14);
  for (const auto& sk : ai::skills()) { m_skillBoxes << new SkillBox(0, sk, this); skillRow->addWidget(m_skillBoxes.last()); }
  skillRow->addStretch(1);
  titles->addLayout(skillRow);
  head->addLayout(titles, 1);
  m_state = ui::label(QString(), theme::mono(9, QFont::Bold, 1.4), c.up, this);
  head->addWidget(m_state, 0, Qt::AlignVCenter);
  for (int i = 0; i < ai::Slots::kCount; ++i) {
    auto* b = button(QString::number(i + 1), c.textDim, c.border, this);
    b->setToolTip(QCoreApplication::translate("ai", "Environment %1").arg(i + 1));
    b->setFixedWidth(30);
    connect(b, &QPushButton::clicked, this, [this, i] { setEnvironment(i); focusInput(); ai::Engines::instance().at(i)->start(); });
    m_envButtons << b;
    head->addWidget(b, 0, Qt::AlignVCenter);
  }
  m_clear = button(QCoreApplication::translate("ai", "CLEAR"), c.textDim, c.border, this);
  m_clear->setToolTip(QCoreApplication::translate("ai", "Empty this environment's conversation"));
  connect(m_clear, &QPushButton::clicked, this, [this] { if (auto* e = ai::Engines::instance().at(m_index)) e->clear(); });
  head->addWidget(m_clear, 0, Qt::AlignVCenter);
  root->addLayout(head);

  // failure banner
  m_errorBox = new QWidget(this);
  auto* eb = new QHBoxLayout(m_errorBox); eb->setContentsMargins(10, 8, 10, 8); eb->setSpacing(10);
  m_errorBox->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:3px;")
                                .arg(theme::alpha(c.down, 25).name(QColor::HexArgb), theme::alpha(c.down, 120).name(QColor::HexArgb)));
  m_error = ui::label(QString(), theme::sans(12), c.down, m_errorBox);
  m_error->setWordWrap(true);
  m_error->setStyleSheet(QString("color:%1;background:transparent;border:none;").arg(c.down.name()));
  m_error->setTextInteractionFlags(Qt::TextSelectableByMouse);
  eb->addWidget(m_error, 1);
  m_retry = button(QCoreApplication::translate("ai", "RETRY"), c.down, theme::alpha(c.down, 150), m_errorBox);
  connect(m_retry, &QPushButton::clicked, this, [this] { if (auto* e = ai::Engines::instance().at(m_index)) e->start(); });
  eb->addWidget(m_retry, 0, Qt::AlignTop);
  m_errorBox->hide();
  root->addWidget(m_errorBox);

  // transcript
  m_view = new QTextBrowser(this);
  m_view->setOpenLinks(false);          // a model's links are text, never opened from here
  connect(m_view, &QTextBrowser::anchorClicked, this, [this](const QUrl& u) {
    if (u.scheme() != "think") return;
    const int i = u.path().toInt();
    const int keep = m_view->verticalScrollBar()->value();
    if (m_openThinking.contains(i)) m_openThinking.remove(i); else m_openThinking.insert(i);
    render();
    m_view->verticalScrollBar()->setValue(keep);
  });
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setFont(theme::sans(13));
  m_view->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:10px 14px; }")
                            .arg(c.panel.name(), c.text.name(), c.border.name()));
  m_view->document()->setDocumentMargin(6);
  root->addWidget(m_view, 1);

  // input
  auto* in = new QHBoxLayout; in->setSpacing(10);
  m_input = new QPlainTextEdit(this);
  m_input->setFont(theme::sans(13));
  m_input->setFixedHeight(64);
  m_input->setStyleSheet(QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:6px 8px; }"
                                 "QPlainTextEdit:focus { border-color:%4; }")
                             .arg(c.panelAlt.name(), c.text.name(), c.border.name(), c.borderHi.name()));
  m_input->installEventFilter(this);
  in->addWidget(m_input, 1);
  m_send = button(QCoreApplication::translate("ai", "SEND"), c.up, theme::alpha(c.up, 150), this, true);
  m_send->setFixedHeight(64);
  m_send->setMinimumWidth(96);
  connect(m_send, &QPushButton::clicked, this, [this] {
    auto* e = ai::Engines::instance().at(m_index);
    // a question waiting for the model to load can be cancelled too (ask() refuses a second one meanwhile, so SEND
    // did nothing): cancel() forgets the pending answer, the question stays in the conversation
    if (e && (e->state() == ai::Engine::State::Busy || e->pending())) { e->cancel(); syncHeader(); } else send();
  });
  in->addWidget(m_send);
  root->addLayout(in);
m_hint = ui::label(QString(), theme::sans(11), c.textMute, this);
  m_hint->setWordWrap(true);
  root->addWidget(m_hint);

  m_renderTimer.setSingleShot(true);
  m_renderTimer.setInterval(50);
  connect(&m_renderTimer, &QTimer::timeout, this, &ChatView::render);
  connect(&ai::Slots::instance(), &ai::Slots::skillsChanged, this, [this](int i) { if (i == m_index) { syncHeader(); render(); } });
  connect(&ai::Slots::instance(), &ai::Slots::changed, this, [this](int i) {
    syncHeader();
    if (i == m_index && ai::Slots::instance().slot(i).isEmpty() && isVisible() && onBack) onBack();   // ejected meanwhile
  });
}

void ChatView::paintEvent(QPaintEvent*) { QPainter p(this); p.fillRect(rect(), theme::pal().bg); }

void ChatView::setEnvironment(int index) {
  for (const auto& k : m_conns) disconnect(k);
  m_conns.clear();
  if (index != m_index) { m_cache.clear(); m_figCache.clear(); m_openThinking.clear(); m_input->clear(); }
  m_toBottom = true;
  m_index = index;
  for (auto* b : m_skillBoxes) b->setEnvironment(index);
  ai::Engine* e = ai::Engines::instance().at(index);
  if (!e) return;
  m_conns << connect(e, &ai::Engine::stateChanged, this, [this] { syncHeader(); m_renderTimer.start(); });
  m_conns << connect(e, &ai::Engine::messagesChanged, this, [this, e] {
    m_cache.clear(); m_figCache.clear();
    // unfolded reasoning is remembered by message index: after CLEAR those indices name new messages
    for (auto it = m_openThinking.begin(); it != m_openThinking.end();)
      it = *it >= e->messages().size() ? m_openThinking.erase(it) : std::next(it);
    render();
  });
  m_conns << connect(e, &ai::Engine::textArrived, this, [this] { if (!m_renderTimer.isActive()) m_renderTimer.start(); });
  syncHeader();
  render();
}

void ChatView::focusInput() { m_input->setFocus(); }

void ChatView::syncHeader() {
  if (m_index < 0) return;
  const auto& c = theme::pal();
  ai::Engine* e = ai::Engines::instance().at(m_index);
  const auto& s = ai::Slots::instance().slot(m_index);
  const auto st = e->state();
  m_title->setText(QCoreApplication::translate("ai", "ENVIRONMENT %1").arg(m_index + 1));
  m_model->setText(s.displayName() + (s.repo.isEmpty() || s.isIntegration() ? QString() : "   ·   " + s.repo));
  m_cube->setState(!s.isEmpty(), st);
  const QColor sc = st == ai::Engine::State::Failed ? c.down : st == ai::Engine::State::Starting ? c.warn
                  : st == ai::Engine::State::Off ? c.textDim : c.up;
  // render() calls this 20 times a second while an answer streams: style sheets (a re-polish each) only on change
  m_state->setText("● " + ai::stateLabel(st));
  const QString stateQss = QString("color:%1;background:%2;border:1px solid %3;border-radius:3px;padding:2px 8px;")
                               .arg(sc.name(), theme::alpha(sc, 30).name(QColor::HexArgb), theme::alpha(sc, 130).name(QColor::HexArgb));
  if (m_state->styleSheet() != stateQss) m_state->setStyleSheet(stateQss);
  QStringList info;
  if (e->isClaude()) { if (!e->device().isEmpty()) info << e->device(); }
  else if (!e->device().isEmpty()) info << (e->device().startsWith("CPU") ? QCoreApplication::translate("ai", "CPU") : QCoreApplication::translate("ai", "GPU %1").arg(e->device().section(" · ", 1)));
  if (e->context() > 0) info << QCoreApplication::translate("ai", "context %1 tokens").arg(QLocale(QLocale::English).toString(e->context()));
  if (e->lastTokensPerSecond() > 0) info << QCoreApplication::translate("ai", "%1 tokens/s").arg(e->lastTokensPerSecond(), 0, 'f', 1);
  m_info->setText(info.join("  ·  "));
  for (int i = 0; i < m_envButtons.size(); ++i) {
    const bool on = !ai::Slots::instance().slot(i).isEmpty();
    if (m_envButtons[i]->isEnabled() != (on && i != m_index)) {
      m_envButtons[i]->setEnabled(on && i != m_index);
      m_envButtons[i]->setStyleSheet(m_envButtons[i]->styleSheet());   // refresh :disabled look
    }
    m_envButtons[i]->setText(QString::number(i + 1));
    QFont f = m_envButtons[i]->font();
    if (f.underline() != (i == m_index)) { f.setUnderline(i == m_index); m_envButtons[i]->setFont(f); }
  }
  m_errorBox->setVisible(st == ai::Engine::State::Failed);
  m_error->setText(e->error());
  const bool reads = ai::Slots::instance().hasSkill(m_index, "read_data");
  if (e->isClaude())
    m_hint->setText(reads ? QCoreApplication::translate("ai", "Enter sends · Shift+Enter for a new line · Claude Code, on Anthropic's servers: reads "
                                                         "Deepwater's data through its tools (each call shown here and journaled and sent to "
                                                         "Anthropic), its own tools are off: no shell, files or web.")
                          : QCoreApplication::translate("ai", "Enter sends · Shift+Enter for a new line · Claude Code, on Anthropic's servers: no "
                                                         "access to Deepwater's data, its own tools are off: no shell, files or web."));
#ifndef __APPLE__
  // the runner is sandboxed on macOS only (sandbox-exec): elsewhere the hint must not promise what is not enforced
  else m_hint->setText(reads ? QCoreApplication::translate("ai", "Enter sends · Shift+Enter for a new line · runs on this computer: reads "
                                                       "Deepwater's data through its tools (each call shown here and journaled). Not "
                                                       "sandboxed on this system: use models you trust.")
                        : QCoreApplication::translate("ai", "Enter sends · Shift+Enter for a new line · runs on this computer, no access to "
                                                       "Deepwater's data. Not sandboxed on this system: use models you trust."));
#else
  else m_hint->setText(reads ? QCoreApplication::translate("ai", "Enter sends · Shift+Enter for a new line · runs on this computer, offline: "
                                                       "reads Deepwater's data through its tools (each call shown here and journaled), "
                                                       "no internet, no access to your files.")
                        : QCoreApplication::translate("ai", "Enter sends · Shift+Enter for a new line · runs on this computer, offline: no "
                                                       "internet, no access to Deepwater's data or your files."));
#endif
  m_send->setText(st == ai::Engine::State::Busy || e->pending() ? QCoreApplication::translate("ai", "STOP") : QCoreApplication::translate("ai", "SEND"));
  m_clear->setEnabled(st != ai::Engine::State::Busy && !e->messages().isEmpty());
}

bool ChatView::eventFilter(QObject* o, QEvent* ev) {
  if (o == m_input && ev->type() == QEvent::KeyPress) {
    auto* k = static_cast<QKeyEvent*>(ev);
    if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && !(k->modifiers() & Qt::ShiftModifier)) { send(); return true; }
  }
  return QWidget::eventFilter(o, ev);
}

void ChatView::send() {
  ai::Engine* e = ai::Engines::instance().at(m_index);
  if (!e) return;
  if (e->ask(m_input->toPlainText())) m_input->clear();
}

QString ChatView::fragment(int i) {
  const auto& msgs = ai::Engines::instance().at(m_index)->messages();
  const auto& m = msgs[i];
  const size_t h = qHash(m.text) ^ (m.stats.isEmpty() ? 0 : 1) ^ (m_openThinking.contains(i) ? 2 : 0);
  auto it = m_cache.constFind(i);
  if (it != m_cache.constEnd() && it->first == h) return it->second;
  const auto& c = theme::pal();
  QString out;
  if (m.role == "assistant") {
    QString thinking, answer;
    splitThinking(m.text, &thinking, &answer);
    answer = withoutCalls(answer);
    if (!thinking.isEmpty()) {
      // finished reasoning folds to its first lines (a link unfolds it); while the model writes, it shows whole
      const bool done = !m.stats.isEmpty();
      const bool fold = done && !m_openThinking.contains(i) && thinking.size() > 260;
      QString body = (fold ? thinking.left(240).trimmed() + "…" : thinking).toHtmlEscaped().replace("\n", "<br>");
      QString link;
      if (done && thinking.size() > 260)
        link = QString(" &nbsp;<a href='think:%1' style='color:%2;font-style:normal;text-decoration:none;'>%3</a>")
                   .arg(i).arg(c.accentText.name(), fold ? QCoreApplication::translate("ai", "show all") : QCoreApplication::translate("ai", "fold"));
      out += QString("<p style='color:%1;font-size:11px;margin:4px 0 2px 0;'><b>%2</b></p><p style='color:%1;font-style:italic;margin:0 0 8px 0;'>%3%4</p>")
                 .arg(c.textMute.name(), QCoreApplication::translate("ai", "REASONING"), body, link);
    }
    out += markdownHtml(answer);
  } else {
    out += QString("<p style='margin:2px 0;'>%1</p>").arg(m.text.toHtmlEscaped().replace("\n", "<br>"));
  }
  m_cache.insert(i, {h, out});
  return out;
}

void ChatView::showEvent(QShowEvent* e) {
  QWidget::showEvent(e);
  if (m_stale) render();
}

void ChatView::render() {
  if (m_index < 0) return;
  // hidden (another page, the cubes, the side panel closed): a streaming answer re-rendered the whole transcript up
  // to 20 times a second for nobody; once when shown again instead
  if (!isVisible()) { m_stale = true; return; }
  m_stale = false;
  QElapsedTimer cost; cost.start();
  syncHeader();
  const auto& c = theme::pal();
  ai::Engine* e = ai::Engines::instance().at(m_index);
  const auto& msgs = e->messages();
  QString html = QString("<html><body style='color:%1;'>").arg(c.text.name());
  if (msgs.isEmpty()) {
    const bool reads = ai::Slots::instance().hasSkill(m_index, "read_data");
    if (e->isClaude())
      html += QString("<p style='color:%1;margin-top:18px;'>%2</p>")
                  .arg(c.textMute.name(), (reads ? QCoreApplication::translate("ai", "Ask Claude Code in environment %1 a question. It runs on "
                                                     "Anthropic's servers and reads Deepwater's data through its tools only.")
                                                 : QCoreApplication::translate("ai", "Ask Claude Code in environment %1 a question. It runs on "
                                                     "Anthropic's servers, without access to Deepwater's data: tick its box to let it read.")).arg(m_index + 1).toHtmlEscaped());
    else html += QString("<p style='color:%1;margin-top:18px;'>%2</p>")
                .arg(c.textMute.name(), (reads
                     ? QCoreApplication::translate("ai", "Ask the model of environment %1 a question. It runs on this computer, offline, and "
                                                         "reads Deepwater's data through its tools.")
                     : QCoreApplication::translate("ai", "Ask the model of environment %1 a question. It runs on this computer, offline, "
                                                         "without access to the internet, Deepwater's data or your files.")).arg(m_index + 1).toHtmlEscaped());
  }
  for (int i = 0; i < msgs.size(); ++i) {
    const auto& m = msgs[i];
    if (m.role == "tool") {   // one line per tool call: what the model read, and how it went
      const QString name = m.call.value("name").toString();
      QString line;
      if (name == "—") line = m.text;
      else {
        line = name + "(" + argsText(m.call.value("arguments").toObject()) + ")  →  ";
        line += m.ok ? QCoreApplication::translate("ai", "%1 characters read").arg(QLocale(QLocale::English).toString(m.text.size()))
                     : m.text.left(200);
      }
      html += QString("<p style='margin:3px 0 3px 0;'><span style='color:%1;font-family:\"%2\";font-size:10px;font-weight:700;'>%3</span>"
                      "<span style='color:%4;font-family:\"%2\";font-size:10px;'>&nbsp;&nbsp;%5</span></p>")
                  .arg((m.ok ? c.accentText : c.down).name(), theme::mono(10).family(),
                       (m.ok ? QCoreApplication::translate("ai", "TOOL") : QCoreApplication::translate("ai", "TOOL ✕")).toHtmlEscaped(),
                       (m.ok ? c.textDim : c.down).name(), line.toHtmlEscaped());
      continue;
    }
    const bool me = m.role == "user";
    if (!me) {   // an answer that only called tools shows nothing but its tool lines
      QString th, an;
      splitThinking(m.text, &th, &an);
      const bool streamingNow = i == msgs.size() - 1 && e->state() == ai::Engine::State::Busy;
      if (th.isEmpty() && withoutCalls(an).isEmpty() && !streamingNow && (m.text.contains("<tool_call>") || m.text.trimmed().isEmpty())) continue;
    }
    QString who = me ? QCoreApplication::translate("ai", "YOU") : QFileInfo(m.model).completeBaseName().toUpper();
    QString meta = m.at.toString("HH:mm");
    if (!me && !m.stats.isEmpty() && !m.stats.value("partial").toBool()) {
      const int n = m.stats.value("gen_tokens").toInt();
      const double ms = m.stats.value("gen_ms").toDouble();
      meta += "  ·  " + QCoreApplication::translate("ai", "%1 tokens").arg(n);
      if (ms > 0 && n > 0) meta += "  ·  " + QCoreApplication::translate("ai", "%1 tokens/s").arg(n * 1000.0 / ms, 0, 'f', 1);
      if (m.stats.value("duration_ms").toDouble() > 0)   // Claude Code: the whole answer, tool calls included
        meta += "  ·  " + QCoreApplication::translate("ai", "%1 s").arg(m.stats.value("duration_ms").toDouble() / 1000.0, 0, 'f', 1);
      const QString r = m.stats.value("reason").toString();
      if (r == "error") meta += "  ·  " + QCoreApplication::translate("ai", "error");
      else if (r == "stopped") meta += "  ·  " + QCoreApplication::translate("ai", "stopped");
      else if (r == "length") meta += "  ·  " + QCoreApplication::translate("ai", "cut: answer length limit");
      else if (r == "context") meta += "  ·  " + QCoreApplication::translate("ai", "cut: context full");
      if (m.stats.value("dropped_messages").toInt() > 0)
        meta += "  ·  " + QCoreApplication::translate("ai", "%1 oldest messages left out (context)").arg(m.stats.value("dropped_messages").toInt());
    }
    html += QString("<p style='margin-top:%1px;margin-bottom:2px;'><span style='color:%2;font-family:\"%3\";font-size:11px;font-weight:700;'>%4</span>"
                    "<span style='color:%5;font-family:\"%3\";font-size:10px;'>&nbsp;&nbsp;%6</span></p>")
                .arg(i == 0 ? 4 : 18).arg((me ? c.accentText : c.up).name(), theme::mono(10).family(), who.toHtmlEscaped(),
                     c.textMute.name(), meta.toHtmlEscaped());
    const bool streaming = !me && i == msgs.size() - 1 && e->state() == ai::Engine::State::Busy;
    if (!me && m.text.isEmpty() && streaming) html += QString("<p style='color:%1;'>…</p>").arg(c.textMute.name());
    else html += fragment(i);
    // figures check, for a finished answer written after tool results of the same question
    if (!me && !streaming && !m.stats.isEmpty() && !m.text.contains("<tool_call>")) {
      QString pool; bool usedTools = false;
      for (int k = i - 1; k >= 0; --k) {
        if (msgs[k].role == "tool") { if (msgs[k].ok) { pool += msgs[k].text + "\n"; usedTools = true; } continue; }
        if (msgs[k].role == "user") { pool += msgs[k].text; break; }
      }
      // the check of a finished answer depends on its text and the tool data only: computed once, not 20 times a
      // second while a later answer streams
      const size_t fh = qHash(m.text) ^ (qHash(pool) * 31);
      if (const auto fit = m_figCache.constFind(i); usedTools && fit != m_figCache.cend() && fit->first == fh) html += fit->second;
      else if (usedTools) {
        QString figHtml;
        QString th, an; splitThinking(m.text, &th, &an);
        QStringList raw;
        const QList<double> got = figures(withoutCalls(an), &raw);
        const QList<double> have = figures(pool);
        QStringList missing;
        for (int k = 0; k < got.size(); ++k) if (!foundIn(got[k], raw[k], have) && !missing.contains(raw[k])) missing << raw[k];
        if (!got.isEmpty()) {
          const bool clean = missing.isEmpty();
          const QString msg = clean ? QCoreApplication::translate("ai", "✓ %1 figures checked: all appear in the data the tools returned.").arg(got.size())
                                    : QCoreApplication::translate("ai", "⚠ figures not found in the data the tools returned: %1").arg(missing.join(", "));
          figHtml = QString("<p style='color:%1;font-family:\"%2\";font-size:10px;margin-top:6px;'>%3</p>")
                        .arg((clean ? c.textMute : c.warn).name(), theme::mono(10).family(), msg.toHtmlEscaped());
        }
        m_figCache.insert(i, {fh, figHtml});
        html += figHtml;
      }
    }
  }
  if (!msgs.isEmpty() && msgs.last().role == "user" && e->state() == ai::Engine::State::Starting)
    html += QString("<p style='color:%1;margin-top:14px;'>%2</p>").arg(c.textMute.name(), QCoreApplication::translate("ai", "Loading the model in memory…"));
  html += "</body></html>";

  QScrollBar* sb = m_view->verticalScrollBar();
  const bool atBottom = m_toBottom || sb->value() >= sb->maximum() - 8;
  if (m_toBottom) QTimer::singleShot(0, this, [this] { m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum()); });
  m_toBottom = false;
  const int keep = sb->value();
  m_view->setHtml(html);
  sb->setValue(atBottom ? sb->maximum() : keep);
  // Each render lays the whole conversation out again (10.8 ms for 60 messages, more as it grows): while an answer
  // streams, refreshes stay 20 a second as long as one costs under 10 ms, then come 8 × its cost apart (≤ 200 ms), so
  // a long conversation's refreshes take at most ~1/8 of the time.
  const int ms = int(cost.elapsed());
  m_renderTimer.setInterval(ms < 10 ? 50 : qMin(200, ms * 8));
}
}  // namespace aipage
