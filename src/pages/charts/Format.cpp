// Charts tab — api.js formatters (see Format.h).
#include "pages/charts/Format.h"
#include "charts/core/Series.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QFile>
#include <QLocale>
#include <QPointer>
#include <cmath>

namespace charts::ui::fmt {

QString localeNumber(double v, int minFrac, int maxFrac) {
  // Round to maxFrac like Intl (half away from zero on the decimal value), then drop zeros down to minFrac.
  QString s = series::toFixed(std::fabs(v), maxFrac);
  if (maxFrac > minFrac && s.contains('.')) {
    int keep = s.size();
    const int dot = s.indexOf('.');
    while (keep > dot + 1 + minFrac && s[keep - 1] == '0') --keep;
    if (keep == dot + 1) --keep;   // no decimals left
    s.truncate(keep);
  }
  QString intPart = s.section('.', 0, 0), frac = s.contains('.') ? s.section('.', 1) : QString();
  QString grouped;
  for (int i = 0; i < intPart.size(); ++i) {
    if (i && (intPart.size() - i) % 3 == 0) grouped += ',';
    grouped += intPart[i];
  }
  const bool neg = v < 0 && (s.toDouble() != 0);
  return (neg ? "-" : "") + grouped + (frac.isEmpty() ? QString() : "." + frac);
}

QString num(double v, int d) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  const double a = std::fabs(v);
  if (a >= 1e12) return series::toFixed(v / 1e12, 2) + "T";
  if (a >= 1e9) return series::toFixed(v / 1e9, 2) + "B";
  if (a >= 1e6) return series::toFixed(v / 1e6, 2) + "M";
  if (a >= 1e4) return series::toFixed(v / 1e3, 1) + "K";
  return series::toFixed(v, d);
}

QString price(double v) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  const double a = std::fabs(v);
  if (a >= 1e6) return num(v);
  const int d = a >= 1000 ? 2 : a >= 1 ? (a >= 100 ? 2 : 4) : a >= 0.01 ? 6 : 8;
  return localeNumber(v, std::min(d, 2), d);
}

QString pct(double v) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  return (v >= 0 ? "+" : "") + series::toFixed(v, 2) + "%";
}

QString esc(const QString& s) {
  QString o;
  o.reserve(s.size());
  for (QChar c : s) {
    switch (c.unicode()) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      case '\'': o += "&#39;"; break;
      default: o += c;
    }
  }
  return o;
}

QString ticker(const QString& id) {
  if (id.startsWith("SPREAD:")) {
    QString out;
    for (const QString& x : id.mid(7).split(' ')) out += x.contains(':') ? x.section(':', -1) : x;
    return out;
  }
  return id.section(':', -1);
}

QString exchange(const QString& id) { return id.section(':', 0, 0); }

void download(QWidget* parent, const QString& name, const QByteArray& content) {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  auto* dlg = new QFileDialog(parent, QString(), dir + "/" + name);
  dlg->setAcceptMode(QFileDialog::AcceptSave);
  dlg->setFileMode(QFileDialog::AnyFile);
  dlg->selectFile(name);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  QObject::connect(dlg, &QFileDialog::fileSelected, dlg, [content](const QString& file) {
    QFile f(file);
    if (f.open(QIODevice::WriteOnly)) f.write(content);
  });
  dlg->open();
}

}  // namespace charts::ui::fmt
