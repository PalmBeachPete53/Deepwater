#pragma once
#include <QWidget>
#include "app/Preload.h"

class QStackedWidget;
class QScrollArea;

namespace economy {
class GlobeWidget;
class AssetFile;
namespace eco_macro { class TopBar; class Registry; class Controls; }

// Economy › Macro: the macro globe (rotating Earth + asset registry) and each economy's Asset File.
// Dev hooks (env): DW_MACRO_COUNTRY=us opens an Asset File, DW_MACRO_DETAIL=inflation|rate|gdp|unemployment opens
// a detail view, DW_MACRO_HOVER=x,y simulates the cursor (page coordinates), DW_MACRO_SCROLL=px scrolls the file,
// DW_MACRO_TAB=0..3 dashboard tab, DW_MACRO_GROUP=0..5 registry group.
class MacroPage : public QWidget, public app::Preloadable {
  Q_OBJECT
public:
  void preload() override;   // app::Preloadable: what showEvent loads, started ahead of a first visit
  explicit MacroPage(QWidget* parent = nullptr);
  void openEconomy(const QString& iso2);
  void closeFile();
protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  void applyHooks();
  void updateTint();
  eco_macro::TopBar* m_top;
  QStackedWidget* m_stack;
  QWidget* m_globeView;
  GlobeWidget* m_globe;
  eco_macro::Registry* m_registry;
  eco_macro::Controls* m_controls;
  QScrollArea* m_scroll;
  AssetFile* m_file;
  bool m_hooksDone = false;
};
}  // namespace economy
