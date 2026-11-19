#pragma once
#include <QString>
#include <QList>
#include <QWidget>
#include <functional>

// Navigation table and page registry.
//
// A page registers itself in its own .cpp, no shared file to edit:
//   #include "app/Pages.h"
//   DW_PAGE("market/seasonality", SeasonalityPage)
// The page is created on first visit and kept. Pages should pause timers/animations in hideEvent
// and resume in showEvent (the window shows one page at a time).
namespace app {
struct SubTab { QString id; QString label; QString description; };
struct MainTab { QString id; QString label; QList<SubTab> subs; };
const QList<MainTab>& tabs();
QString pageId(const QString& main, const QString& sub);   // "global/overview"
// Label or description of the navigation table in the current language (context "nav").
QString label(const QString& english);

using Factory = std::function<QWidget*(QWidget* parent)>;
bool registerPage(const QString& id, Factory f);
QWidget* createPage(const QString& id, QWidget* parent);    // placeholder if none registered
}

#define DW_PAGE_CAT2(a, b) a##b
#define DW_PAGE_CAT(a, b) DW_PAGE_CAT2(a, b)
#define DW_PAGE(ID, CLASS) \
  static const bool DW_PAGE_CAT(dw_page_reg_, __LINE__) = app::registerPage(ID, [](QWidget* p) -> QWidget* { return new CLASS(p); });
