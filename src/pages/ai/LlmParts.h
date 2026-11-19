#pragma once
class QWidget;

// The two parts of AI › LLM (pages/ai/LlmPage.cpp): Local models (Hugging Face Hub, AiModelsPage.cpp) and Integration
// (Claude Code, IntegrationPage.cpp).
namespace aipage {
QWidget* createLocalModelsPart(QWidget* parent);
QWidget* createIntegrationPart(QWidget* parent);
}
