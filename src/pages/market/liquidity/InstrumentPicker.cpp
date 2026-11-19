#include "pages/market/liquidity/InstrumentPicker.h"
#include "pages/market/technical/MarketKit.h"
#include "market/Instruments.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QCoreApplication>
#include <algorithm>

using market::Category;

QString instrumentLabel(const QString& symbol) {
  const auto* i = market::find(symbol);
  if (!i) return symbol;
  if (i->category == Category::Forex) return i->base + "/" + i->quote;
  if (i->category == Category::Crypto && i->symbol.endsWith("USD")) return i->symbol.left(i->symbol.size() - 3) + "/USD";
  return i->name;
}
static QString tagOf(Category c) {
  switch (c) {
    case Category::Forex: return "FX";
    case Category::Index: return QCoreApplication::translate("technical", "INDEX");
    case Category::Commodity: return QCoreApplication::translate("technical", "CMDTY");
    case Category::Stock: return QCoreApplication::translate("technical", "STOCK");
    case Category::Crypto: return QCoreApplication::translate("technical", "CRYPTO");
    case Category::Bond: return QCoreApplication::translate("technical", "YIELD");
  }
  return {};
}

// ------------------------------------------------------------------ button
InstrumentButton::InstrumentButton(QWidget* parent) : QWidget(parent) {
  setCursor(Qt::PointingHandCursor); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
void InstrumentButton::setText(const QString& t) { m_text = t; updateGeometry(); update(); }
void InstrumentButton::setOpen(bool o) { m_open = o; update(); }
QSize InstrumentButton::sizeHint() const { return QSize(int(mkit::textWidth(theme::sans(11, QFont::Medium), m_text) + 32), 27); }
void InstrumentButton::mousePressEvent(QMouseEvent*) { emit clicked(); }
void InstrumentButton::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void InstrumentButton::leaveEvent(QEvent*) { m_hover = false; update(); }
void InstrumentButton::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(m_hover || m_open ? theme::map("#6b6040") : theme::map("#47412d"), 1)); p.setBrush(theme::map("#0f0704"));
  p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  p.setFont(theme::sans(11, QFont::Medium)); p.setPen(theme::map("#e3e0db"));
  p.drawText(QRectF(10, 0, width() - 20, height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
  const double cx = width() - 13, cy = height() / 2.0;
  p.setPen(QPen(theme::map("#c9c5bc"), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
  QPolygonF v; if (m_open) v << QPointF(cx - 2.5, cy + 1.2) << QPointF(cx, cy - 1.3) << QPointF(cx + 2.5, cy + 1.2);
  else v << QPointF(cx - 2.5, cy - 1.2) << QPointF(cx, cy + 1.3) << QPointF(cx + 2.5, cy - 1.2);
  p.drawPolyline(v);
}

// ------------------------------------------------------------------ list
namespace { constexpr double kItem = 25.5; }
InstrumentList::InstrumentList(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true); setCursor(Qt::PointingHandCursor);
  // majors first (the reference list starts EUR/USD, GBP/USD, USD/JPY, USD/CHF, USD/CAD, AUD/USD, NZD/USD, EUR/GBP…)
  const QStringList first{"EURUSD", "GBPUSD", "USDJPY", "USDCHF", "USDCAD", "AUDUSD", "NZDUSD", "EURGBP", "EURJPY", "GBPJPY"};
  for (const auto& s : first) if (const auto* i = market::find(s)) m_all.push_back({i->symbol, instrumentLabel(i->symbol), tagOf(i->category)});
  const Category order[] = {Category::Forex, Category::Index, Category::Commodity, Category::Crypto, Category::Stock, Category::Bond};
  for (auto c : order)
    for (const auto& i : market::byCategory(c))
      if (!first.contains(i.symbol)) m_all.push_back({i.symbol, instrumentLabel(i.symbol), tagOf(i.category)});
  m_items = m_all;
}
void InstrumentList::setFilter(const QString& f) {
  m_items.clear();
  for (const auto& it : m_all)
    if (f.isEmpty() || it.label.contains(f, Qt::CaseInsensitive) || it.symbol.contains(f, Qt::CaseInsensitive) || it.tag.contains(f, Qt::CaseInsensitive))
      m_items.push_back(it);
  m_scroll = 0; m_hover = -1; update();
}
void InstrumentList::setCurrent(const QString& s) { m_current = s; update(); }
void InstrumentList::scrollToCurrent() {
  for (int i = 0; i < m_items.size(); ++i)
    if (m_items[i].symbol == m_current) {
      const double y = i * kItem;
      if (y < m_scroll || y + kItem > m_scroll + height()) m_scroll = std::max(0.0, y - height() / 2.0);
    }
  m_scroll = std::clamp(m_scroll, 0.0, std::max(0.0, m_items.size() * kItem - height()));
  update();
}
void InstrumentList::wheelEvent(QWheelEvent* e) {
  const double d = !e->pixelDelta().isNull() ? e->pixelDelta().y() : e->angleDelta().y() / 120.0 * kItem * 3;
  m_scroll = std::clamp(m_scroll - d, 0.0, std::max(0.0, m_items.size() * kItem - height()));
  update(); e->accept();
}
void InstrumentList::mouseMoveEvent(QMouseEvent* e) {
  const int h = int((e->position().y() + m_scroll) / kItem);
  const int v = h >= 0 && h < m_items.size() ? h : -1;
  if (v != m_hover) { m_hover = v; update(); }
}
void InstrumentList::mousePressEvent(QMouseEvent* e) {
  const int h = int((e->position().y() + m_scroll) / kItem);
  if (h >= 0 && h < m_items.size()) emit picked(m_items[h].symbol);
}
void InstrumentList::leaveEvent(QEvent*) { m_hover = -1; update(); }
void InstrumentList::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const QFont f = theme::sans(11), tf = theme::mono(7, QFont::Normal, 0.5);
  const int a = int(m_scroll / kItem), b = std::min<int>(m_items.size() - 1, int((m_scroll + height()) / kItem));
  for (int i = a; i <= b; ++i) {
    const QRectF r(0, i * kItem - m_scroll, width(), kItem - 1.5);
    const bool cur = m_items[i].symbol == m_current;
    if (cur || i == m_hover) { p.setPen(Qt::NoPen); p.setBrush(cur ? theme::map("#29251d") : theme::map("#18140c")); p.drawRoundedRect(r, 2, 2); }
    p.setFont(f); p.setPen(cur ? theme::map("#d1c78a") : theme::map("#dddad6"));
    p.drawText(r.adjusted(6, 0, -40, 0), Qt::AlignVCenter | Qt::AlignLeft, m_items[i].label);
    p.setFont(tf); p.setPen(theme::map("#454139"));
    p.drawText(r.adjusted(0, 0, -6, 0), Qt::AlignVCenter | Qt::AlignRight, m_items[i].tag);
  }
  if (m_items.isEmpty()) { p.setFont(theme::sans(11)); p.setPen(theme::map("#5f5b54")); p.drawText(rect().adjusted(6, 4, 0, 0), Qt::AlignTop | Qt::AlignLeft, QCoreApplication::translate("technical", "No match")); }
}

// ------------------------------------------------------------------ dropdown
InstrumentDropdown::InstrumentDropdown(QWidget* host) : QWidget(host) {
  hide();
  m_search = new mkit::SearchBox(QCoreApplication::translate("technical", "Search..."), false, this);
  m_list = new InstrumentList(this);
  connect(m_search, &mkit::SearchBox::textChanged, m_list, &InstrumentList::setFilter);
  connect(m_list, &InstrumentList::picked, this, [this](const QString& s) { dismiss(); emit picked(s); });
}
void InstrumentDropdown::popup(const QPoint& at, const QString& current) {
  setGeometry(at.x(), at.y(), 190, 258);
  m_search->setGeometry(7, 7, width() - 12, 25);
  m_list->setGeometry(7, 36, width() - 12, height() - 38);
  m_search->clear();
  m_list->setCurrent(current); m_list->scrollToCurrent();
  raise(); show();
  m_search->setFocusToEdit();
  qApp->installEventFilter(this);
}
void InstrumentDropdown::dismiss() {
  if (!isVisible()) return;
  qApp->removeEventFilter(this);
  hide();
  emit closed();
}
bool InstrumentDropdown::eventFilter(QObject* o, QEvent* e) {
  if (e->type() == QEvent::MouseButtonPress) {
    auto* w = qobject_cast<QWidget*>(o);
    if (w && w->window() == window() && !isAncestorOf(w) && w != this && w != button) dismiss();
  } else if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
    dismiss(); return true;
  }
  return false;
}
void InstrumentDropdown::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape) dismiss(); }
void InstrumentDropdown::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), theme::map("#0d0802"));
  p.setPen(QPen(theme::map("#433f20"), 1)); p.setBrush(Qt::NoBrush); p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
}
