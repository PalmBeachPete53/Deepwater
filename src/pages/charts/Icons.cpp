// Charts tab — stroke icons (see Icons.h).
#include "pages/charts/Icons.h"
#include "ui/Theme.h"
#include <QSvgRenderer>
#include <QPainter>
#include <QHash>

namespace charts::ui::icons {

namespace {
QString wrap(const QString& inner, const QColor& c) {
  QString body = inner;
  body.replace("currentColor", c.name(QColor::HexRgb));
  QString root = QStringLiteral(
      R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round")")
      .arg(c.name(QColor::HexRgb));
  if (c.alpha() < 255) root += QStringLiteral(R"( opacity="%1")").arg(c.alphaF(), 0, 'f', 3);
  return root + ">" + body + "</svg>";
}
QHash<QString, QPixmap>& cache() { static QHash<QString, QPixmap> c; return c; }
int g_rev = -1;
void checkRev() {
  if (g_rev != theme::revision()) { g_rev = theme::revision(); cache().clear(); }
  if (cache().size() > 4000) cache().clear();
}
QPixmap render(const QString& doc, QSize size, qreal dpr) {
  QPixmap pm(size * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QSvgRenderer r(doc.toUtf8());
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  r.render(&p, QRectF(QPointF(0, 0), QSizeF(size)));
  return pm;
}
}  // namespace

QPixmap pixmap(const QString& inner, const QColor& color, int px, qreal dpr) {
  checkRev();
  const QString key = inner + '|' + QString::number(color.rgba(), 16) + '|' + QString::number(px) + '|' +
                      QString::number(dpr);
  auto it = cache().constFind(key);
  if (it != cache().cend()) return *it;
  QPixmap pm = render(wrap(inner, color), QSize(px, px), dpr);
  cache().insert(key, pm);
  return pm;
}

QIcon icon(const QString& inner, const QColor& color, int px) {
  QIcon ic;
  ic.addPixmap(pixmap(inner, color, px, 1.0));
  ic.addPixmap(pixmap(inner, color, px, 2.0));
  return ic;
}

void paint(QPainter& p, const QRectF& r, const QString& inner, const QColor& color) {
  const qreal dpr = p.device() ? p.device()->devicePixelRatioF() : 2.0;
  const int px = qRound(qMin(r.width(), r.height()));
  const QPixmap pm = pixmap(inner, color, px, dpr);
  p.drawPixmap(QPointF(r.center().x() - px / 2.0, r.center().y() - px / 2.0), pm);
}

QPixmap svg(const QString& document, QSize size, qreal dpr) {
  checkRev();
  const QString key = "doc|" + document + '|' + QString::number(size.width()) + 'x' + QString::number(size.height()) +
                      '|' + QString::number(dpr);
  auto it = cache().constFind(key);
  if (it != cache().cend()) return *it;
  QPixmap pm = render(document, size, dpr);
  cache().insert(key, pm);
  return pm;
}

QPixmap brand(int px, const QColor& accent, const QColor& stroke, qreal dpr) {
  // index.html: <svg viewBox="0 0 32 32"><rect width="32" height="32" rx="7" fill="var(--accent)"/>
  //             <path d="M7 21l6-7 5 4 7-9" stroke="#fff" stroke-width="3" fill="none" .../></svg>
  const QString doc = QStringLiteral(
      R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="7" fill="%1"/><path d="M7 21l6-7 5 4 7-9" stroke="%2" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/></svg>)")
      .arg(accent.name(), stroke.name());
  return svg(doc, QSize(px, px), dpr);
}

}  // namespace charts::ui::icons
