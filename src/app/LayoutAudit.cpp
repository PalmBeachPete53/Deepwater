#include "app/LayoutAudit.h"
#include "app/MainWindow.h"
#include "app/Pages.h"
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QEventLoop>
#include <cmath>
#include <QTextStream>
#include <QTimer>
#include <memory>

namespace layoutaudit {
namespace {

QString nameOf(QWidget* w) {
  QString s = QString::fromLatin1(w->metaObject()->className());
  if (!w->objectName().isEmpty()) s += "#" + w->objectName();
  QString text;
  if (auto* l = qobject_cast<QLabel*>(w)) text = l->text();
  else if (auto* b = qobject_cast<QAbstractButton*>(w)) text = b->text();
  else if (auto* c = qobject_cast<QComboBox*>(w)) text = c->currentText();
  else if (auto* e = qobject_cast<QLineEdit*>(w)) text = e->text().isEmpty() ? e->placeholderText() : e->text();
  text = QTextDocumentFragment::fromHtml(text).toPlainText().simplified();
  if (!text.isEmpty()) s += " \"" + text.left(40) + "\"";
  // where it sits: the two nearest named ancestors
  int n = 0;
  for (QWidget* a = w->parentWidget(); a && n < 2; a = a->parentWidget())
    if (!a->objectName().isEmpty() && !a->objectName().startsWith("qt_")) { s += " in " + a->objectName(); ++n; }
  return s;
}

bool inScrollArea(QWidget* w, QWidget* stop) {
  for (QWidget* a = w->parentWidget(); a && a != stop; a = a->parentWidget())
    if (qobject_cast<QAbstractScrollArea*>(a)) return true;
  return false;
}

bool isLeaf(QWidget* w) {
  if (qobject_cast<QLabel*>(w) || qobject_cast<QAbstractButton*>(w) || qobject_cast<QLineEdit*>(w) ||
      qobject_cast<QComboBox*>(w) || qobject_cast<QAbstractSpinBox*>(w))
    return true;
  for (QObject* o : w->children())
    if (o->isWidgetType() && static_cast<QWidget*>(o)->isVisible()) return false;
  return true;   // a widget that paints itself
}

// Width the label's text needs on one line (rich text through a QTextDocument, as QLabel lays it out).
int textNeed(QLabel* l) {
  const QString t = l->text();
  if (Qt::mightBeRichText(t) || l->textFormat() == Qt::RichText) {
    QTextDocument d;
    d.setDefaultFont(l->font());
    d.setDocumentMargin(0);
    d.setHtml(t);
    return int(std::ceil(d.idealWidth()));
  }
  int w = 0;
  for (const QString& line : t.split('\n')) w = qMax(w, l->fontMetrics().horizontalAdvance(line));
  return w;
}

void scan(QWidget* page, QWidget* win, const QString& tag, QTextStream& out, int& count) {
  const QRect wr = win->rect();
  for (QWidget* c : page->findChildren<QWidget*>()) {
    if (!c->isVisible() || c->width() <= 0 || c->height() <= 0) continue;
    if (c->objectName().startsWith("qt_")) continue;   // scroll bars, viewports
    const QRect r(c->mapTo(win, QPoint(0, 0)), c->size());
    QRect vis = r & wr;
    for (QWidget* a = c->parentWidget(); a && a != win; a = a->parentWidget()) vis &= QRect(a->mapTo(win, QPoint(0, 0)), a->size());
    if (vis.isEmpty()) continue;   // hidden behind a scroll position or a stack
    const bool scroll = inScrollArea(c, win);
    if (!scroll && isLeaf(c) && (vis.width() < r.width() - 1 || vis.height() < r.height() - 1)) {
      out << tag << " CUT " << nameOf(c) << " shows " << vis.width() << "x" << vis.height() << " of " << r.width() << "x"
          << r.height() << "\n";
      ++count;
    }
    // A label with a tooltip is elided on purpose (the full text is in the tooltip): not a clipped text.
    if (auto* l = qobject_cast<QLabel*>(c); l && !l->text().isEmpty() && l->pixmap().isNull() && l->toolTip().isEmpty()) {
      const QRect cr = l->contentsRect();
      if (!l->wordWrap()) {
        const int need = textNeed(l) + 2 * l->margin();
        if (need > cr.width() + 1) {
          out << tag << " TEXT " << nameOf(l) << " needs " << need << " px wide, has " << cr.width() << "\n";
          ++count;
        }
      } else if (l->heightForWidth(cr.width()) > cr.height() + 1) {
        out << tag << " TEXT " << nameOf(l) << " needs " << l->heightForWidth(cr.width()) << " px high, has " << cr.height()
            << "\n";
        ++count;
      }
    } else if (auto* b = qobject_cast<QAbstractButton*>(c); b && !b->text().isEmpty()) {
      // the text (and icon) itself, not the style's preferred padding: a fixed 30 px "1" button is fine
      const QString t = QString(b->text()).remove('&');
      int need = b->fontMetrics().horizontalAdvance(t) + 4;
      if (!b->icon().isNull()) need += b->iconSize().width() + 4;
      if (need > b->width()) {
        out << tag << " TEXT " << nameOf(b) << " needs " << need << " px wide, has " << b->width() << "\n";
        ++count;
      }
    }
    if (auto* sa = qobject_cast<QAbstractScrollArea*>(c)) {
      if (sa->horizontalScrollBar()->maximum() > 0 && !inScrollArea(sa, win)) {
        out << tag << " HSCROLL " << nameOf(sa) << " scrolls " << sa->horizontalScrollBar()->maximum() << " px sideways\n";
        ++count;
      }
    }
  }
}

// Which widgets hold a page wider (or taller) than `limit`: walks down through the widest child each time, so the
// last lines name the widget that really needs the room (a fixed width, a long label, a row of buttons...).
void whyWide(QWidget* w, int limitW, int limitH, const QString& tag, QTextStream& out, int depth = 0) {
  if (depth > 14) return;
  QWidget* widest = nullptr;
  QWidget* tallest = nullptr;
  for (QObject* o : w->children()) {
    if (!o->isWidgetType()) continue;
    auto* c = static_cast<QWidget*>(o);
    if (c->isWindow() || c->isHidden()) continue;
    const QSize m = c->minimumSizeHint().expandedTo(c->minimumSize());
    if (m.width() > limitW && (!widest || m.width() > widest->minimumSizeHint().expandedTo(widest->minimumSize()).width())) widest = c;
    if (m.height() > limitH && (!tallest || m.height() > tallest->minimumSizeHint().expandedTo(tallest->minimumSize()).height())) tallest = c;
  }
  if (!widest && !tallest) {   // a row (or column) of children that add up: list them
    for (QObject* o : w->children()) {
      if (!o->isWidgetType()) continue;
      auto* c = static_cast<QWidget*>(o);
      if (c->isWindow() || c->isHidden()) continue;
      const QSize m = c->minimumSizeHint().expandedTo(c->minimumSize());
      if (m.width() < 40 && m.height() < 40) continue;
      out << tag << " " << QString(depth * 2, ' ') << "PART " << nameOf(c) << " min " << m.width() << "x" << m.height()
          << (c->minimumSize().width() > 0 || c->minimumSize().height() > 0
                  ? QString(" (set %1x%2)").arg(c->minimumSize().width()).arg(c->minimumSize().height())
                  : QString()) << "\n";
    }
    return;
  }
  for (QWidget* c : {widest, tallest}) {
    if (!c) continue;
    const QSize m = c->minimumSizeHint().expandedTo(c->minimumSize());
    out << tag << " " << QString(depth * 2, ' ') << (c == widest ? "WIDE " : "TALL ") << nameOf(c) << " min " << m.width()
        << "x" << m.height() << (c->minimumSize().width() > 0 || c->minimumSize().height() > 0
                                     ? QString(" (set %1x%2)").arg(c->minimumSize().width()).arg(c->minimumSize().height())
                                     : QString()) << "\n";
    whyWide(c, c == widest ? limitW : 1 << 20, c == tallest ? limitH : 1 << 20, tag, out, depth + 1);
    if (widest == tallest) break;
  }
}

void settle(int ms) {
  QEventLoop l;
  QTimer::singleShot(ms, &l, &QEventLoop::quit);
  l.exec();
}

}  // namespace

void run(MainWindow* w, const QString& dir, const QList<QSize>& sizes, const QStringList& onlyPages, int loadMs) {
  QDir().mkpath(dir);
  QStringList ids;
  for (const auto& t : app::tabs())
    for (const auto& s : t.subs) ids << app::pageId(t.id, s.id);
  if (!onlyPages.isEmpty()) ids = onlyPages;
  QTimer::singleShot(500, w, [w, dir, sizes, ids, loadMs] {
    QFile f(dir + "/report.txt");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { qWarning("layout audit: cannot write %s", qPrintable(f.fileName())); qApp->quit(); return; }
    QTextStream out(&f);
    int total = 0;
    for (const QString& id : ids) {
      if (!w->openPage(id)) { out << id << " UNKNOWN\n"; continue; }
      w->dropPagesExcept(id);   // a page's minimum alone, not the largest of every page opened before
      QWidget* page = w->currentPage();
      w->resize(sizes.value(0, QSize(1600, 1042)));
      settle(loadMs);   // data, first layouts
      const QSize min = w->minimumSizeHint().expandedTo(w->minimumSize());
      out << id << " page min " << page->minimumSizeHint().width() << "x" << page->minimumSizeHint().height()
          << ", window min " << min.width() << "x" << min.height() << "\n";
      {
        // budget for the page: the smallest size minus the header (window height - page height)
        const QSize first = sizes.value(0, QSize(1024, 600));
        const int chrome = w->height() - page->height();
        const QSize pm = page->minimumSizeHint().expandedTo(page->minimumSize());
        if (pm.width() > first.width() || pm.height() > first.height() - chrome)
          whyWide(page, first.width(), first.height() - chrome, id, out);
      }
      QString safe = id;
      safe.replace('/', '_');
      for (const QSize& sz : sizes) {
        w->resize(sz);
        settle(600);
        const QString tag = QString("%1 @%2x%3").arg(id).arg(sz.width()).arg(sz.height());
        int count = 0;
        if (w->width() > sz.width() || w->height() > sz.height()) {
          out << tag << " MIN window stays " << w->width() << "x" << w->height() << "\n";
          ++count;
        }
        scan(page, w, tag, out, count);
        total += count;
        w->grab().save(QString("%1/%2@%3x%4.png").arg(dir, safe).arg(sz.width()).arg(sz.height()));
      }
      out.flush();
    }
    out << "total " << total << "\n";
    f.close();
    qApp->quit();
  });
}

}  // namespace layoutaudit
