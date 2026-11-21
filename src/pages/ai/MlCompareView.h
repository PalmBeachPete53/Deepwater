#pragma once
#include "ml/Comparison.h"
#include <QColor>
#include <QWidget>
#include <functional>

class QVBoxLayout;
namespace ml { struct Run; }

// AI › ML › Runs, COMPARE (user's request 2026-11-19: "pouvoir lancer les comparaisons dans l'app"): the report of
// ml::comparison drawn as tables in the interface language, the same figures the agents' ml_compare quotes. One block per
// group (SAME MARKET: ranked setups and the verdict; SEVERAL MARKETS: one setup, one row per market), then what was not
// compared and why. A click on a table row opens that training.
namespace aipage {

struct CompareViewHooks {
  std::function<QString(const ml::Run&)> question;     // what the run predicts / trades / groups, in words
  std::function<QString(const QString&)> algorithm;    // an algorithm id's name
  std::function<void(const QString&)> open;            // a row clicked: show that run
  bool compact = false;                                // phones: a table too narrow for one line puts each row on two
};

// A ranked prediction group's verdict, in words, and its colour (green beats, red worse, amber in between).
QString verdictLine(const ml::comparison::Group& g);
QColor verdictTone(const ml::comparison::Group& g);

// Adds the report's widgets to `into` (the result column's layout), children of `host`.
void addComparison(QVBoxLayout* into, QWidget* host, const ml::comparison::Report& rep, const CompareViewHooks& hooks);

}  // namespace aipage
