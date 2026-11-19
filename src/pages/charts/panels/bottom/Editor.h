#pragma once
// Charts tab — bottom panel, Indicator editor. Port of quant-terminal public/js/panels.js (TEMPLATE, edListHtml,
// renderEditor, renderEditorList) (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
// Files go through charts::data custom files (src/charts/data/CustomFiles.h), the same code as POST /api/custom.
#include <QObject>
#include <QPointer>

class QPlainTextEdit;
class QLineEdit;
class QLabel;
class QVBoxLayout;

namespace charts::ui {
class BottomPanel;
namespace bottom {

class EditorTab : public QObject {
  Q_OBJECT
public:
  explicit EditorTab(BottomPanel* panel);
  void render();                       // renderEditor
  static const char* TEMPLATE;

private:
  void renderList();                   // renderEditorList
  void save(bool add);

  BottomPanel* P;
  QString edFile;
  // The draft survives a re-render (collapse, tab switch, theme change, PUT /api/state rebuild the body): an empty
  // code means "nothing typed yet", which shows the template.
  QString draftName_, draftCode_;
  QPointer<QWidget> list_;
  QVBoxLayout* listLay_ = nullptr;
  QPointer<QPlainTextEdit> code_;
  QPointer<QLineEdit> name_;
  QPointer<QLabel> err_;
};

}  // namespace bottom
}  // namespace charts::ui
