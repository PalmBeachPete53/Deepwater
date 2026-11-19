// Port of quant-terminal public/js/chart.js newUid (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/pane/Util.h"
#include <QDateTime>

namespace charts::ui::pane {

namespace {
QString base36(quint64 v) {
  if (!v) return "0";
  QString s;
  while (v) { s.prepend(QChar("0123456789abcdefghijklmnopqrstuvwxyz"[v % 36])); v /= 36; }
  return s;
}
}  // namespace

QString newUid() {
  static quint64 seq = 0;
  return "i" + base36(quint64(QDateTime::currentMSecsSinceEpoch())) + base36(seq++);
}

}  // namespace charts::ui::pane
