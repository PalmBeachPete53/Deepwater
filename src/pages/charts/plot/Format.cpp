#include "pages/charts/plot/Format.h"
#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QTimeZone>

#include <QHash>
#include <algorithm>

namespace charts::plot {

static QColor parseCssColor(const QString& cssIn);

// Indicators and the volume series give a CSS colour per bar (the same few strings over and over): parsed once each.
QColor cssColor(const QString& css) {
  thread_local QHash<QString, QColor> cache;
  if (const auto it = cache.constFind(css); it != cache.cend()) return *it;
  const QColor c = parseCssColor(css);
  if (cache.size() >= 1024) cache.clear();   // gradients made per value: keep it bounded
  cache.insert(css, c);
  return c;
}

static QColor parseCssColor(const QString& cssIn) {
  const QString css = cssIn.trimmed().toLower();
  if (css.isEmpty()) return {};
  if (css == "transparent") return QColor(0, 0, 0, 0);
  if (css.startsWith("rgb")) {
    const int a = css.indexOf('('), b = css.lastIndexOf(')');
    if (a < 0 || b < a) return {};
    static const QRegularExpression sep("[,\\s/]+");
    const QStringList parts = css.mid(a + 1, b - a - 1).split(sep, Qt::SkipEmptyParts);
    if (parts.size() < 3) return {};
    auto chan = [](const QString& s) {
      return s.endsWith('%') ? qBound(0, int(std::lround(s.chopped(1).toDouble() * 2.55)), 255)
                             : qBound(0, int(std::lround(s.toDouble())), 255);
    };
    double al = 1;
    if (parts.size() >= 4) al = parts[3].endsWith('%') ? parts[3].chopped(1).toDouble() / 100 : parts[3].toDouble();
    QColor c(chan(parts[0]), chan(parts[1]), chan(parts[2]));
    c.setAlphaF(float(qBound(0.0, al, 1.0)));
    return c;
  }
  if (css.startsWith('#') && (css.size() == 5 || css.size() == 9)) {   // #rgba / #rrggbbaa (CSS order)
    QString h = css.mid(1);
    if (h.size() == 4) h = QString("%1%1%2%2%3%3%4%4").arg(h[0]).arg(h[1]).arg(h[2]).arg(h[3]);
    bool ok = false;
    const uint v = h.toUInt(&ok, 16);
    if (!ok) return {};
    return QColor((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
  }
  QColor c = QColor::fromString(css);
  return c;
}

MarkerPosition markerPosition(const QString& s) {
  if (s == "belowBar") return MarkerPosition::BelowBar;
  if (s == "inBar") return MarkerPosition::InBar;
  return MarkerPosition::AboveBar;
}

MarkerShape markerShape(const QString& s) {
  if (s == "arrowUp") return MarkerShape::ArrowUp;
  if (s == "arrowDown") return MarkerShape::ArrowDown;
  if (s == "square") return MarkerShape::Square;
  return MarkerShape::Circle;
}

bool DataPoint::isWhitespace(SeriesType type) const {
  return (type == SeriesType::Candlestick || type == SeriesType::Bar) ? !std::isfinite(close) : !std::isfinite(value);
}

SeriesOptions seriesDefaults(SeriesType type) {
  SeriesOptions o;
  if (type == SeriesType::Histogram) o.color = QColor("#26a69a");
  return o;
}

namespace fmt {

static const QChar kMinus(0x2212);

QString fixed(double v, int precision, double minMove) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  if (minMove > 0) v = std::round(v / minMove) * minMove;
  QString s = QString::number(std::fabs(v), 'f', qMax(0, precision));
  const bool neg = v < 0 && std::any_of(s.cbegin(), s.cend(), [](QChar c) { return c >= u'1' && c <= u'9'; });   // not a "-0.00"
  return neg ? kMinus + s : s;
}

static QString trimZeros(QString s) {
  if (!s.contains('.')) return s;
  while (s.endsWith('0')) s.chop(1);
  if (s.endsWith('.')) s.chop(1);
  return s;
}

QString volume(double v, int precision) {
  if (!std::isfinite(v)) return QStringLiteral("—");
  QString sign = v < 0 ? QString(kMinus) : QString();
  v = std::fabs(v);
  if (v < 995) return sign + trimZeros(QString::number(v, 'f', precision));
  if (v < 999995) return sign + trimZeros(QString::number(v / 1e3, 'f', precision)) + 'K';
  if (v < 999999995) return sign + trimZeros(QString::number(v / 1e6, 'f', precision)) + 'M';
  return sign + trimZeros(QString::number(v / 1e9, 'f', precision)) + 'B';
}

QString percent(double v) { return fixed(v, 2) + '%'; }

// French interface (the app sets the default QLocale to the interface language, core/I18n): French month names.
static bool frenchUi() { return QLocale().language() == QLocale::French; }

QString monthShort(int m) {
  static const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (frenchUi()) return QLocale().monthName(qBound(1, m, 12), QLocale::ShortFormat);   // "janv.", "févr."…
  return QString::fromLatin1(names[qBound(1, m, 12) - 1]);
}

static QString monthLong(int m) {
  static const char* names[] = {"January", "February", "March", "April", "May", "June",
                                "July", "August", "September", "October", "November", "December"};
  if (frenchUi()) return QLocale().monthName(qBound(1, m, 12), QLocale::LongFormat);
  return QString::fromLatin1(names[qBound(1, m, 12) - 1]);
}

QString date(Time t, const QString& f) {
  const QDate d = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date();
  QString out;
  for (int i = 0; i < f.size();) {
    auto run = [&](QChar c) { int n = 0; while (i + n < f.size() && f[i + n] == c) ++n; return n; };
    const QChar c = f[i];
    int n = (c == 'y' || c == 'M' || c == 'd') ? run(c) : 0;
    if (c == 'y' && n >= 4) { out += QString::number(d.year()); i += 4; continue; }
    if (c == 'y' && n >= 2) { out += QString("%1").arg(d.year() % 100, 2, 10, QChar('0')); i += 2; continue; }
    if (c == 'M' && n >= 4) { out += monthLong(d.month()); i += 4; continue; }
    if (c == 'M' && n == 3) { out += monthShort(d.month()); i += 3; continue; }
    if (c == 'M' && n == 2) { out += QString("%1").arg(d.month(), 2, 10, QChar('0')); i += 2; continue; }
    if (c == 'M' && n == 1) { out += QString::number(d.month()); i += 1; continue; }
    if (c == 'd' && n >= 2) { out += QString("%1").arg(d.day(), 2, 10, QChar('0')); i += 2; continue; }
    if (c == 'd' && n == 1) { out += QString::number(d.day()); i += 1; continue; }
    out += c;
    ++i;
  }
  return out;
}

QString clock(Time t, bool seconds) {
  const QTime tm = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).time();
  return tm.toString(seconds ? "HH:mm:ss" : "HH:mm");
}

int timeWeight(Time t, Time prev) {
  const QDateTime a = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC), b = QDateTime::fromSecsSinceEpoch(prev, QTimeZone::UTC);
  if (a.date().year() != b.date().year()) return WYear;
  if (a.date().month() != b.date().month()) return WMonth;
  if (a.date().day() != b.date().day()) return WDay;
  // intraday: the coarsest bucket boundary crossed between the two points
  static const struct { qint64 secs; int w; } buckets[] = {
      {12 * 3600, WHour12}, {6 * 3600, WHour6}, {3 * 3600, WHour3}, {3600, WHour1},
      {1800, WMinute30}, {300, WMinute5}, {60, WMinute1}, {1, WSecond}};
  auto fl = [](qint64 v, qint64 d) { return v >= 0 ? v / d : -((-v + d - 1) / d); };
  for (const auto& bk : buckets)
    if (fl(t, bk.secs) != fl(prev, bk.secs)) return bk.w;
  return WNone;
}

QColor textOn(const QColor& bg) {
  const double lum = 0.199 * bg.red() + 0.687 * bg.green() + 0.114 * bg.blue();
  return lum > 160 ? Qt::black : Qt::white;
}

}  // namespace fmt
}  // namespace charts::plot
