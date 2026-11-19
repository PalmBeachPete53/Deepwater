// AI › LLM: a bar LOCAL MODELS | INTEGRATION above the part it shows. The part is created on first view and kept;
// the choice is saved (settings ai.llm.part). Test hook DW_AI_LLM_PART=<0|1>.
#include "app/Pages.h"
#include "core/Settings.h"
#include "pages/ai/LlmParts.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace aipage {
namespace {
class LlmPage : public QWidget {
public:
  explicit LlmPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto* barRow = new QHBoxLayout; barRow->setContentsMargins(16, 10, 16, 0);
    m_bar = new ui::PillBar({QCoreApplication::translate("ai", "LOCAL MODELS"), QCoreApplication::translate("ai", "INTEGRATION")},
                            this, ui::PillBar::Underline);
    m_bar->setFontPx(12);
    barRow->addWidget(m_bar);
    barRow->addStretch(1);
    root->addLayout(barRow);
    m_stack = new QStackedWidget(this);
    m_parts = {nullptr, nullptr};
    root->addWidget(m_stack, 1);
    const int part = qBound(0, qEnvironmentVariableIsSet("DW_AI_LLM_PART") ? qEnvironmentVariableIntValue("DW_AI_LLM_PART")
                                                                            : settings::value("ai.llm.part", 0).toInt(), 1);
    connect(m_bar, &ui::PillBar::changed, this, [this](int i) { settings::setValue("ai.llm.part", i); showPart(i); });
    m_bar->setCurrent(part);
    showPart(part);
  }
protected:
  void paintEvent(QPaintEvent*) override { QPainter p(this); p.fillRect(rect(), theme::pal().bg); }
private:
  void showPart(int i) {
    if (!m_parts[i]) {
      m_parts[i] = i == 0 ? createLocalModelsPart(m_stack) : createIntegrationPart(m_stack);
      m_stack->addWidget(m_parts[i]);
    }
    m_stack->setCurrentWidget(m_parts[i]);
  }
  ui::PillBar* m_bar;
  QStackedWidget* m_stack;
  QList<QWidget*> m_parts;
};
}  // namespace
}  // namespace aipage

using LlmPageImpl = aipage::LlmPage;
DW_PAGE("ai/models", LlmPageImpl)
