#pragma once
#include <QList>
#include <QString>
#include <QStringList>

// AI › ML: the algorithms the trainer knows (resources/ml/train.py), by family, with their parameters and the ranges the
// page offers. Shared by the ML page and the cubes' "ML" skill (ai/MlTools.cpp), which checks every argument against it.
// Labels and descriptions follow the interface language (context "ai").
namespace ml {

struct Param {
  QString key, label;
  char type;              // 'i' integer, 'd' decimal, 's' text, 'c' choice among options
  double min, max, def, step;   // 's' (hidden_layers): min, max of each number, 0 = unchecked
  QString text;           // default of 's' and 'c'
  QStringList options;    // 'c'
};
struct Algo {
  QString id, name, desc;
  QList<Param> params;
  QString needs;          // Python module that must import ("lightgbm", "xgboost", "torch"), empty if none
  int family = 0;         // 0 supervised, 1 unsupervised, 2 semi-supervised, 3 self-supervised, 4 reinforcement
};
QList<Algo> algos();                  // built at each call: the labels follow the interface language
QList<Algo> algosFor(int family);
Algo algoById(const QString& id);     // empty id when unknown
QString familyId(int family);         // "supervised" … "reinforcement" (the trainer's names)
int familyOf(const QString& id);      // -1 when unknown
}  // namespace ml
