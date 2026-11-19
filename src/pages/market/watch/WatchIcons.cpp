#include "pages/market/watch/WatchIcons.h"
#include "market/Instruments.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QHash>
#include <QFile>
#include <QCoreApplication>

namespace watch {
QString countryOf(const QString& s) {
  static const QHash<QString, QString> m{
    {"SPX", "us"}, {"NDX", "us"}, {"NASDAQ", "us"}, {"DOW", "us"}, {"RUT", "us"}, {"VIX", "us"}, {"DXY", "us"},
    {"TSX", "ca"}, {"BOVESPA", "br"}, {"DAX", "de"}, {"FTSE", "gb"}, {"CAC", "fr"}, {"STOXX50", "eu"}, {"IBEX", "es"},
    {"FTSEMIB", "it"}, {"SMI", "ch"}, {"AEX", "nl"}, {"NIKKEI", "jp"}, {"HSI", "hk"}, {"SSEC", "cn"}, {"ASX200", "au"},
    {"KOSPI", "kr"}, {"SENSEX", "in"}, {"US13W", "us"}, {"US5Y", "us"}, {"US10Y", "us"}, {"US30Y", "us"}};
  return m.value(s.toUpper());
}
QString currencyOf(const QString& s) {
  const auto* i = market::find(s);
  if (!i) return "USD";
  if (i->category == market::Category::Forex) return i->base;
  if (i->category == market::Category::Index) {
    static const QHash<QString, QString> c{{"ca", "CAD"}, {"br", "BRL"}, {"de", "EUR"}, {"fr", "EUR"}, {"eu", "EUR"},
      {"es", "EUR"}, {"it", "EUR"}, {"nl", "EUR"}, {"gb", "GBP"}, {"ch", "CHF"}, {"jp", "JPY"}, {"hk", "HKD"},
      {"cn", "CNY"}, {"au", "AUD"}, {"kr", "KRW"}, {"in", "INR"}};
    return c.value(countryOf(s), "USD");
  }
  return "USD";
}
QString currencyName(const QString& c) {
  static const QHash<QString, const char*> m{
    {"USD", QT_TRANSLATE_NOOP("watch", "U.S. Dollar")}, {"EUR", QT_TRANSLATE_NOOP("watch", "Euro")}, {"GBP", QT_TRANSLATE_NOOP("watch", "British Pound")}, {"JPY", QT_TRANSLATE_NOOP("watch", "Japanese Yen")}, {"CHF", QT_TRANSLATE_NOOP("watch", "Swiss Franc")},
    {"CAD", QT_TRANSLATE_NOOP("watch", "Canadian Dollar")}, {"AUD", QT_TRANSLATE_NOOP("watch", "Australian Dollar")}, {"NZD", QT_TRANSLATE_NOOP("watch", "New Zealand Dollar")}, {"CNH", QT_TRANSLATE_NOOP("watch", "Chinese Yuan (offshore)")},
    {"MXN", QT_TRANSLATE_NOOP("watch", "Mexican Peso")}, {"ZAR", QT_TRANSLATE_NOOP("watch", "South African Rand")}, {"TRY", QT_TRANSLATE_NOOP("watch", "Turkish Lira")}, {"SEK", QT_TRANSLATE_NOOP("watch", "Swedish Krona")},
    {"NOK", QT_TRANSLATE_NOOP("watch", "Norwegian Krone")}, {"SGD", QT_TRANSLATE_NOOP("watch", "Singapore Dollar")}, {"HKD", QT_TRANSLATE_NOOP("watch", "Hong Kong Dollar")}, {"PLN", QT_TRANSLATE_NOOP("watch", "Polish Zloty")},
    {"INR", QT_TRANSLATE_NOOP("watch", "Indian Rupee")}, {"BRL", QT_TRANSLATE_NOOP("watch", "Brazilian Real")}};
  const char* n = m.value(c, nullptr);
  return n ? QCoreApplication::translate("watch", n) : c;   // translated at display time
}

QString marketLabel(const QString& s) {
  const auto* i = market::find(s);
  if (!i) return s;
  switch (i->category) {
    case market::Category::Forex: return i->base + "/" + i->quote;
    case market::Category::Crypto: return i->symbol.left(i->symbol.size() - 3) + "/USD";
    case market::Category::Stock: return i->yahoo.contains('-') ? i->yahoo : i->symbol;
    default: {
      // short names of the reference's list
      static const QHash<QString, QString> shortNames{{"DOW", "Dow Jones"}, {"VIX", "VIX"}, {"DAX", "DAX"}, {"GASOLINE", "Gasoline (RBOB)"}};
      return shortNames.value(i->symbol, i->name);
    }
  }
}

static void badge(QPainter& p, const QRectF& r, const QString& t) {
  p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0x2a, 0x27, 0x22))); p.drawEllipse(r);
  QFont f = theme::mono(6, QFont::Bold); p.setFont(f); p.setPen(theme::map(QColor(0x8a, 0x86, 0x7e)));
  p.drawText(r, Qt::AlignCenter, t.left(2).toUpper());
}
static void disc(QPainter& p, const QRectF& r, const QColor& c) {
  QRadialGradient g(r.center() - QPointF(r.width() * .2, r.height() * .25), r.width() * .75);
  g.setColorAt(0, c.lighter(150)); g.setColorAt(1, c.darker(115));
  p.setPen(QPen(c.darker(160), 0.8)); p.setBrush(g); p.drawEllipse(r.adjusted(.4, .4, -.4, -.4));
}
static void barrel(QPainter& p, const QRectF& r) {
  const QRectF b(r.center().x() - r.width() * .3, r.top() + .5, r.width() * .6, r.height() - 1);
  const bool dw = theme::isDefault();   // energy category colour of the theme (Deepwater keeps its blue barrel)
  const QColor body = dw ? QColor(0x3d, 0x7f, 0xd6) : theme::roles().energy;
  p.setPen(Qt::NoPen); p.setBrush(body); p.drawRoundedRect(b, 1.2, 1.2);
  p.setPen(QPen(dw ? QColor(0x9c, 0xc4, 0xf5) : body.lighter(160), 0.9));
  p.drawLine(QPointF(b.left(), b.top() + b.height() * .33), QPointF(b.right(), b.top() + b.height() * .33));
  p.drawLine(QPointF(b.left(), b.top() + b.height() * .66), QPointF(b.right(), b.top() + b.height() * .66));
}
static void flame(QPainter& p, const QRectF& r) {
  QPainterPath f;
  const qreal cx = r.center().x(), w = r.width() * .36, top = r.top() + .5, bot = r.bottom() - .5;
  f.moveTo(cx, top);
  f.cubicTo(cx + w * 1.4, top + r.height() * .35, cx + w * 1.3, bot, cx, bot);
  f.cubicTo(cx - w * 1.3, bot, cx - w * 1.4, top + r.height() * .45, cx - w * .2, top + r.height() * .3);
  f.closeSubpath();
  QLinearGradient g(r.topLeft(), r.bottomLeft()); 
  if (theme::isDefault()) { g.setColorAt(0, QColor(0xff, 0xb3, 0x3a)); g.setColorAt(1, QColor(0xf0, 0x5a, 0x1e)); }
  else { g.setColorAt(0, theme::roles().energy.lighter(140)); g.setColorAt(1, theme::roles().energy); }
  p.setPen(Qt::NoPen); p.setBrush(g); p.drawPath(f);
}

QPixmap instrumentIcon(const QString& symbol, int h, qreal dpr) {
  static QHash<QString, QPixmap> cache;
  // glyph colours follow the theme: the icons of an older theme revision are never asked again, so the cache is
  // emptied when the revision changes instead of keeping ~400 more pixmaps per theme change
  static auto cacheRevision = theme::revision();
  if (cacheRevision != theme::revision()) { cache.clear(); cacheRevision = theme::revision(); }
  const QString key = QString("%1_%2_%3").arg(symbol).arg(h).arg(dpr);
  if (auto it = cache.constFind(key); it != cache.cend()) return *it;
  const auto* ins = market::find(symbol);
  QPixmap pm;
  auto canvas = [&](int w) { pm = QPixmap(QSize(w, h) * dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent); };
  if (!ins) { canvas(h); }
  else if (ins->category == market::Category::Forex) {
    const int fh = h - 1, fw = int(fh * 1.42);
    canvas(fw + 7);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.drawPixmap(0, 0, ui::flag(ins->base, QSize(fw, fh - 1), dpr));
    // quote flag on top, with a 1 px dark gap so the two read as separate flags
    p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0x0c, 0x06, 0x01))); p.drawRect(QRectF(6, 0.5, fw + 1, fh + 0.5));
    p.drawPixmap(7, 1, ui::flag(ins->quote, QSize(fw, fh), dpr));
  } else if (ins->category == market::Category::Index || ins->category == market::Category::Bond) {
    const QString c = countryOf(ins->symbol);
    const int fw = int((h - 1) * 1.42);
    canvas(fw + 1);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    if (!c.isEmpty() && QFile::exists(QString(":/flags/%1.svg").arg(c))) p.drawPixmap(0, 1, ui::flag(c, QSize(fw, h - 2), dpr));
    else badge(p, QRectF((fw - h) / 2.0 + 1, 0, h, h), ins->symbol);
  } else if (ins->category == market::Category::Crypto) {
    canvas(h + 2);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    const QString coin = ins->symbol.left(ins->symbol.size() - 3).toLower();
    QSvgRenderer svg(QString(":/watch/crypto/%1.svg").arg(coin));
    if (svg.isValid()) svg.render(&p, QRectF(1, 0, h, h)); else badge(p, QRectF(1, 0, h, h), coin);
  } else if (ins->category == market::Category::Commodity) {
    canvas(h + 2);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1, 0, h, h);
    const QString s = ins->symbol;
    const bool dw = theme::isDefault();
    if (s == "XAUUSD") disc(p, r, dw ? QColor(0xe0, 0xa8, 0x2a) : theme::roles().precious);
    else if (s == "XAGUSD") disc(p, r, dw ? QColor(0xb8, 0xc0, 0xcc) : theme::mix(theme::roles().precious, theme::roles().text, 0.6));
    else if (s == "COPPER") disc(p, r, dw ? QColor(0xe0, 0x78, 0x48) : theme::roles().industrial);
    else if (s == "WTI" || s == "BRENT") barrel(p, r);
    else if (s == "NATGAS") flame(p, r);
    else badge(p, r, ins->name);
  } else {   // stock: monogram disc
    canvas(h + 2);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1, 0, h, h);
    p.setPen(Qt::NoPen); p.setBrush(theme::map(QColor(0xe8, 0xe4, 0xdc))); p.drawEllipse(r);
    p.setFont(theme::sans(int(h * 0.62), QFont::Black)); p.setPen(theme::map(QColor(0x14, 0x10, 0x0a)));
    p.drawText(r.adjusted(0, 0, 0, -0.5), Qt::AlignCenter, ins->symbol.left(1));
  }
  cache.insert(key, pm);
  return pm;
}
}
