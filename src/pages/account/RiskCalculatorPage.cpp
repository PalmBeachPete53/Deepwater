#include "pages/account/RiskCalculator.h"
#include <QCoreApplication>
#include "account/Contracts.h"
#include "account/Journal.h"
#include "account/Importers.h"
#include "market/Instruments.h"
#include "market/QuoteHub.h"
#include "core/Settings.h"
#include "app/Pages.h"
#include "ui/Format.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QLineEdit>
#include <QPalette>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDoubleValidator>
#include <algorithm>

namespace acct {
namespace {
constexpr int kTitleH = 37;

// Soft glow behind big numbers (blurred copy of the text), cached by text/size/colour.
QImage blurAlpha(QImage img, int r) {
  if (r < 1) return img;
  const int w = img.width(), h = img.height();
  QVector<int> tmp(std::max(w, h));
  for (int pass = 0; pass < 3; ++pass) {
    for (int y = 0; y < h; ++y) {                   // horizontal
      QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
      for (int c = 0; c < 4; ++c) {
        int sum = 0; const int sh = c * 8;
        for (int x = -r; x <= r; ++x) sum += (line[std::clamp(x, 0, w - 1)] >> sh) & 0xff;
        for (int x = 0; x < w; ++x) {
          tmp[x] = sum / (2 * r + 1);
          sum += (line[std::min(x + r + 1, w - 1)] >> sh) & 0xff;
          sum -= (line[std::max(x - r, 0)] >> sh) & 0xff;
        }
        for (int x = 0; x < w; ++x) line[x] = (line[x] & ~(0xffu << sh)) | (uint(tmp[x]) << sh);
      }
    }
    for (int x = 0; x < w; ++x) {                   // vertical
      for (int c = 0; c < 4; ++c) {
        const int sh = c * 8; int sum = 0;
        auto px = [&](int y) -> QRgb& { return reinterpret_cast<QRgb*>(img.scanLine(std::clamp(y, 0, h - 1)))[x]; };
        for (int y = -r; y <= r; ++y) sum += (px(y) >> sh) & 0xff;
        for (int y = 0; y < h; ++y) {
          tmp[y] = sum / (2 * r + 1);
          sum += (px(y + r + 1) >> sh) & 0xff;
          sum -= (px(y - r) >> sh) & 0xff;
        }
        for (int y = 0; y < h; ++y) px(y) = (px(y) & ~(0xffu << sh)) | (uint(tmp[y]) << sh);
      }
    }
  }
  return img;
}
void glowText(QPainter& p, QHash<QString, QImage>& cache, const QPointF& base, const QString& text, const QFont& f,
              const QColor& col, int radius, int alpha) {
  if (!theme::isDark()) alpha = alpha / 3;            // a yellow halo looks dirty on light backgrounds
  const QString key = QString("%1|%2|%3|%4|%5").arg(text).arg(f.pixelSize()).arg(col.name()).arg(radius).arg(alpha);
  const qreal dpr = p.device()->devicePixelRatioF();
  QFontMetricsF fm(f);
  const QRectF br = fm.boundingRect(text);
  if (!cache.contains(key)) {
    if (cache.size() > 40) cache.clear();
    const QSize sz(int((br.width() + 4 * radius) * dpr), int((br.height() + 4 * radius) * dpr));
    QImage img(sz, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
      QPainter q(&img); q.setRenderHint(QPainter::Antialiasing); q.scale(dpr, dpr);
      q.setFont(f); q.setPen(theme::alpha(col, alpha));
      q.drawText(QPointF(2 * radius - br.left(), 2 * radius - br.top()), text);
    }
    img = blurAlpha(img, int(radius * dpr / 2));
    img.setDevicePixelRatio(dpr);
    cache.insert(key, img);
  }
  p.drawImage(QPointF(base.x() + br.left() - 2 * radius, base.y() + br.top() - 2 * radius), cache.value(key));
}

QString lineEditStyle() {
  return qss("QLineEdit { background:#050401; border:1px solid #1b170d; border-radius:3px; color:#fdfcf9;"
         " font-family:'JetBrains Mono'; font-size:16px; padding:0 6px; selection-background-color:#433603; }"
         "QLineEdit:focus { border-color:#4a4224; }");
}
QString fmtNum(double v, int dec) { return std::isfinite(v) ? fmt::num(v, dec) : fmt::dash(); }
}

// ================================================================ instrument picker
InstrumentPicker::InstrumentPicker(QWidget* parent) : QWidget(parent) {
  setCursor(Qt::PointingHandCursor);
  // FX majors in the reference's order, then the other pairs, metals, indices, energy, agriculture, crypto, stocks
  const QStringList majors{"EURUSD", "GBPUSD", "USDJPY", "USDCHF", "USDCAD", "AUDUSD", "NZDUSD"};
  for (const auto& s : majors) { const auto sp = account::spec(s); m_all << Item{s, sp.display, "FX"}; }
  QList<Item> fx, rest[7];
  for (const auto& i : market::instruments()) {
    if (i.category == market::Category::Bond || majors.contains(i.symbol)) continue;
    const auto sp = account::spec(i.symbol);
    Item it{i.symbol, sp.display, sp.tag == "FX" ? "FX" : sp.tag};
    if (i.category == market::Category::Forex) fx << it;
    else if (sp.tag == "METAL") rest[0] << it;
    else if (sp.tag == "IDX") rest[1] << it;
    else if (sp.tag == "ENERGY") rest[2] << it;
    else if (sp.tag == "AGRI") rest[3] << it;
    else if (sp.tag == "CRYPTO") rest[4] << it;
    else rest[5] << it;
  }
  // crosses: EUR/GBP first like the reference, then alphabetical
  std::stable_sort(fx.begin(), fx.end(), [](const Item& a, const Item& b) {
    if ((a.symbol == "EURGBP") != (b.symbol == "EURGBP")) return a.symbol == "EURGBP";
    return a.display < b.display;
  });
  m_all << fx;
  for (auto& r : rest) m_all << r;
  for (auto& it : m_all) if (it.tag == "IDX") it.tag = "INDEX";

  m_search = new QLineEdit(this);
  m_search->setPlaceholderText(QCoreApplication::translate("account", "Search…"));
  m_search->setTextMargins(14, 0, 0, 0);
  m_search->setStyleSheet(qss(QString("QLineEdit { background:#040300; border:1px solid #2a2618; border-radius:0; color:#ece8de;"
                                  " placeholder-text-color:#4e4a40; font-family:'%1'; font-size:13px; font-weight:500; padding:0 6px; }")
                          .arg(theme::condensed(13).family())));
  m_search->setFont(theme::condensed(12, QFont::Medium));
  { QPalette pl = m_search->palette(); pl.setColor(QPalette::PlaceholderText, theme::map("#4e4a40")); pl.setColor(QPalette::Text, theme::map("#ece8de"));
    m_search->setPalette(pl); }
  m_search->installEventFilter(this);
  auto* mag = new PaintPanel(m_search);
  mag->setFrame(false); mag->setAttribute(Qt::WA_TransparentForMouseEvents); mag->setGeometry(5, 6, 14, 14);
  mag->setPaint([](QPainter& p, const QRectF&, const QPointF&, double) {
    p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(c().faint, 1.1)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(5.5, 6), 3.3, 3.3); p.drawLine(QPointF(8, 8.6), QPointF(10.2, 10.8));
  });
  connect(m_search, &QLineEdit::textChanged, this, [this] { m_scroll = 0; refilter(); });
  m_list = new PaintPanel(this);
  m_list->setFrame(false);
  m_list->setHoverable(true);
  m_list->installEventFilter(this);
  m_list->setPaint([this](QPainter& p, const QRectF& r, const QPointF& mouse, double) {
    const double pitch = 25.5;
    p.setClipRect(r);
    const int first = std::max(0, int(m_scroll / pitch));
    for (int i = first; i < m_items.size(); ++i) {
      const double top = i * pitch - m_scroll;
      if (top > r.height()) break;
      const QRectF row(0, top, r.width(), pitch - 1.5);
      const bool cur = m_items[i].symbol == m_symbol;
      const bool hov = std::isfinite(mouse.y()) && row.contains(mouse);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(Qt::NoPen);
      if (hov) { p.setBrush(theme::map("#433603")); p.drawRoundedRect(row, 3, 3); }
      else if (cur) { p.setBrush(theme::map("#29251d")); p.drawRoundedRect(row, 3, 3); }
      p.setFont(theme::condensed(13, QFont::Medium)); p.setPen(hov ? theme::map("#f0d040") : cur ? theme::map("#e6c200") : theme::map("#d6d0c0"));
      p.drawText(row.adjusted(7, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, m_items[i].display);
      p.setFont(mono(8)); p.setPen(theme::map("#3a3630"));
      p.drawText(row.adjusted(0, 0, -6, 0), Qt::AlignVCenter | Qt::AlignRight, tagLabel(m_items[i].tag));
    }
  });
  connect(m_list, &PaintPanel::clicked, this, [this](const QPointF& pt) {
    const int i = int((pt.y() + m_scroll) / 25.5);
    if (i < 0 || i >= m_items.size()) return;
    m_symbol = m_items[i].symbol;
    setOpen(false);
    emit picked(m_symbol);
  });
  // the list's top-left bracket sits over the search field's corner
  m_corner = new PaintPanel(this);
  m_corner->setFrame(false); m_corner->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_corner->setGeometry(6, 40, 10, 10);
  m_corner->setPaint([](QPainter& p, const QRectF&, const QPointF&, double) {
    p.fillRect(QRectF(0, 0, 9, 2), theme::map("#857f67")); p.fillRect(QRectF(0, 0, 2, 9), theme::map("#857f67"));
  });
  m_search->hide(); m_list->hide(); m_corner->hide();
  refilter();
}

void InstrumentPicker::refilter() {
  const QString q = m_search->text().trimmed();
  m_items.clear();
  for (const auto& it : m_all)
    if (q.isEmpty() || it.display.contains(q, Qt::CaseInsensitive) || it.symbol.contains(q, Qt::CaseInsensitive) || it.tag.contains(q, Qt::CaseInsensitive))
      m_items << it;
  m_list->update();
}
void InstrumentPicker::setSymbol(const QString& s) { m_symbol = s; update(); m_list->update(); }
void InstrumentPicker::setListHeight(int h) { m_listH = std::max(90, h); }
int InstrumentPicker::preferredHeight() const { return m_open ? 31 + 2 + m_listH : 31; }
void InstrumentPicker::setOpen(bool on) {
  if (on == m_open) return;
  m_open = on;
  m_search->setVisible(on); m_list->setVisible(on); m_corner->setVisible(on); m_corner->raise();
  if (on) {
    m_search->clear(); refilter();
    int idx = 0; for (int i = 0; i < m_items.size(); ++i) if (m_items[i].symbol == m_symbol) idx = i;
    m_scroll = std::max(0.0, idx * 25.5 - 25.5 * 3);
    if (idx < 7) m_scroll = 0;
    m_search->setFocus();
  }
  update();
  emit openChanged(on);
}
void InstrumentPicker::resizeEvent(QResizeEvent*) {
  m_search->setGeometry(6, 31 + 2 + 7, width() - 12, 25);
  m_list->setGeometry(6, 31 + 2 + 35, width() - 12, std::max(0, height() - (31 + 2 + 35) - 6));
}
bool InstrumentPicker::eventFilter(QObject* o, QEvent* e) {
  if (o == m_list && e->type() == QEvent::Wheel) {
    auto* we = static_cast<QWheelEvent*>(e);
    const double maxS = std::max(0.0, m_items.size() * 25.5 - m_list->height());
    const double d = !we->pixelDelta().isNull() ? we->pixelDelta().y() : we->angleDelta().y() / 120.0 * 25.5 * 2;
    m_scroll = std::clamp(m_scroll - d, 0.0, maxS);
    m_list->update();
    return true;
  }
  if (o == m_search && e->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(e);
    if (ke->key() == Qt::Key_Escape) { setOpen(false); return true; }
    if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !m_items.isEmpty()) {
      m_symbol = m_items.first().symbol; setOpen(false); emit picked(m_symbol); return true;
    }
  }
  return QWidget::eventFilter(o, e);
}
void InstrumentPicker::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF combo(0.5, 0.5, width() - 1, 30);
  p.setPen(theme::map("#1b170d")); p.setBrush(theme::map("#060502"));
  p.drawRoundedRect(combo, 2, 2);
  p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(theme::map("#ece8de"));
  p.drawText(combo.adjusted(9, 0, -30, 0), Qt::AlignVCenter | Qt::AlignLeft, account::displaySymbol(m_symbol));
  QPen pen(theme::map("#ece8de"), 1.2); pen.setCapStyle(Qt::RoundCap); p.setPen(pen);
  const QPointF ch(width() - 15, 15.5);
  const qreal d = m_open ? -1 : 1;
  p.drawPolyline(QPolygonF{ch + QPointF(-3, -1.5 * d), ch + QPointF(0, 1.5 * d), ch + QPointF(3, -1.5 * d)});
  if (m_open) {
    const QRectF box(0.5, 33.5, width() - 1, height() - 34);
    p.setPen(theme::map("#4a4224")); p.setBrush(theme::map("#060502"));
    p.drawRect(box);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen bp(theme::map("#857f67"), 2); bp.setCapStyle(Qt::FlatCap); p.setPen(bp);
    const QPointF br(width() - 7, height() - 7);
    p.drawLine(br, br - QPointF(8, 0)); p.drawLine(br + QPointF(0, 1), br - QPointF(0, 8));
  }
}
void InstrumentPicker::mousePressEvent(QMouseEvent* e) { if (e->position().y() < 32) setOpen(!m_open); }

// ================================================================ page
RiskCalculatorPage::RiskCalculatorPage(QWidget* parent) : QWidget(parent) {
  m_scroll = new SmoothScroll(this);
  m_content = new QWidget;
  m_scroll->setWidget(m_content);
  m_left = new PaintPanel(m_content);
  m_left->setTitle(QCoreApplication::translate("account", "TRADE PARAMETERS"), {}, theme::map("#6b665a"), 9, 2.8);
  // field labels are painted by the left panel at the positions of their inputs
  m_picker = new InstrumentPicker(m_left);
  auto mkEdit = [this](const QString& ph) {
    auto* e = new QLineEdit(m_left);
    e->setStyleSheet(lineEditStyle());
    e->setPlaceholderText(ph);
    e->setFixedHeight(38);
    return e;
  };
  m_balance = mkEdit("10000"); m_risk = mkEdit("1");
  m_entry = mkEdit(QCoreApplication::translate("account", "live price…")); m_stop = mkEdit(""); m_tp = mkEdit(QCoreApplication::translate("account", "optional"));
  m_toggle = new PaintPanel(m_left);
  m_toggle->setFrame(false); m_toggle->setCursor(Qt::PointingHandCursor);
  m_toggle->setHoverable(true);
  m_toggle->setFocusPolicy(Qt::ClickFocus);
  m_toggle->setPaint([this](QPainter& p, const QRectF& r, const QPointF& mouse, double) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(theme::map("#2a2618")); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    const QRectF a(3, 2.5, 21, r.height() - 5), b(24, 2.5, 21, r.height() - 5);
    const QRectF on = m_cash ? b : a, off = m_cash ? a : b;
    // after a click the active button glows (focus), as in the reference from t14
    if (m_toggle->hasFocus()) for (int k = 4; k >= 1; --k) {
      p.setPen(Qt::NoPen); p.setBrush(theme::map(254, 215, 28, (theme::isDark() ? 18 + 10 * (4 - k) : 10 + 5 * (4 - k))));
      p.drawRoundedRect(on.adjusted(-k * 0.8, -k * 0.8, k * 0.8, k * 0.8), 3 + k, 3 + k);
    }
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#fed71c")); p.drawRoundedRect(on, 3, 3);
    const bool hovOff = std::isfinite(mouse.x()) && off.adjusted(-1, -2, 1, 2).contains(mouse);
    p.setFont(chart(11, true));
    p.setPen(theme::onAccent()); p.drawText(on, Qt::AlignCenter, m_cash ? ccySymbol(accountCcy()).trimmed() : "%");
    p.setPen(hovOff ? theme::map("#f0d040") : theme::map("#55514b"));
    p.drawText(off, Qt::AlignCenter, m_cash ? "%" : ccySymbol(accountCcy()).trimmed());
  });
  connect(qApp, &QApplication::focusChanged, m_toggle, [this] { m_toggle->update(); });
  connect(m_toggle, &PaintPanel::clicked, this, [this](const QPointF& pt) {
    const bool cash = pt.x() >= 24;
    if (cash == m_cash) return;
    m_cash = cash; settings::setValue("account.risk.unit", cash ? "cash" : "pct");
    m_toggle->update(); compute();
  });
  m_left->setPaint([this](QPainter& p, const QRectF&, const QPointF&, double) {
    p.setFont(mono(9, false, 2.8)); p.setPen(c().faint);
    auto lbl = [&](QWidget* w, const QString& t) { p.drawText(QPointF(13, w->y() - 8), t); };
    lbl(m_picker, QCoreApplication::translate("account", "INSTRUMENT")); lbl(m_balance, QCoreApplication::translate("account", "ACCOUNT BALANCE"));
    lbl(m_risk, QCoreApplication::translate("account", "RISK PER TRADE")); lbl(m_entry, QCoreApplication::translate("account", "ENTRY PRICE"));
    lbl(m_stop, QCoreApplication::translate("account", "STOP LOSS")); lbl(m_tp, QCoreApplication::translate("account", "TAKE PROFIT (OPTIONAL)"));
    if (!m_c.error.isEmpty()) {
      p.setFont(cond(13, false)); p.setPen(c().redText);
      p.drawText(QRectF(13, m_tp->geometry().bottom() + 14, m_left->width() - 26, 60), Qt::TextWordWrap, m_c.error);
    }
  });

  // ---- right column
  m_pos = new PaintPanel(m_content); m_pos->setTitle(QCoreApplication::translate("account", "POSITION"), {}, theme::map("#6b665a"), 9, 2.8);
  m_riskCard = new PaintPanel(m_content); m_riskCard->setTitle(QCoreApplication::translate("account", "RISK"), {}, theme::map("#6b665a"), 9, 2.8);
  m_rewardCard = new PaintPanel(m_content); m_rewardCard->setTitle(QCoreApplication::translate("account", "REWARD"), {}, theme::map("#6b665a"), 9, 2.8);
  m_map = new PaintPanel(m_content); m_map->setTitle(QCoreApplication::translate("account", "TRADE MAP"), {}, theme::map("#6b665a"), 9, 2.8);
  m_rr = new PaintPanel(m_content);
  m_foot = new PaintPanel(m_content);
  m_foot->setFrame(false);
  m_foot->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    p.setFont(cond(10, false)); p.setPen(c().ghost);
    p.drawText(r, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, footText());   // wraps on narrow windows
  });

  m_pos->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    const auto sp = account::spec(m_picker->symbol());
    const qreal step = r.width() * 0.328;
    const QStringList labels{QCoreApplication::translate("account", "LOT SIZE"), QCoreApplication::translate("account", "POSITION SIZE"),
                             QCoreApplication::translate("account", "STOP DISTANCE")};
    p.setFont(mono(9, false, 2.8)); p.setPen(c().faint);
    for (int i = 0; i < 3; ++i) p.drawText(QPointF(14 + i * step, 52), labels[i]);
    p.setRenderHint(QPainter::Antialiasing);
    // lot size, big yellow with glow
    const QString lots = m_c.ok ? fmt::num(m_c.lots, 2) : fmt::dash();
    const QFont fl = mono(44, true);
    const QColor lotCol = m_c.ok && m_c.belowMinLot ? c().redText : theme::map("#f5cf1a");   // the minimum lot is over the risk
    glowText(p, m_glow, QPointF(14, 98), lots, fl, lotCol, 10, 90);
    p.setFont(fl); p.setPen(lotCol); p.drawText(QPointF(14, 98), lots);
    const qreal lw = QFontMetricsF(fl).horizontalAdvance(lots);
    p.setFont(cond(19, false)); p.setPen(c().dim); p.drawText(QPointF(14 + lw + 12, 97), "lots");
    p.setFont(cond(11, false));
    QString foot = QCoreApplication::translate("account", "1 lot = %1 %2").arg(fmt::num(sp.contract, 0), unitLabel(sp.unit));
    p.setPen(c().faint); p.drawText(QPointF(14, 123), foot);
    if (m_c.ok && m_c.belowMinLot) {
      const qreal fw = QFontMetricsF(p.font()).horizontalAdvance(foot);
      p.setPen(c().redText); p.drawText(QPointF(14 + fw, 123), QCoreApplication::translate("account", " · the 0.01 minimum lot is over your risk"));
    }
    // position size and stop distance
    const QFont fv = mono(24, true);
    p.setFont(fv); p.setPen(c().white);
    const QString units = m_c.ok ? fmt::num(m_c.units, m_c.units < 100 ? 2 : 0) : fmt::dash();
    p.drawText(QPointF(14 + step, 86), units);
    p.setFont(cond(15, false)); p.setPen(c().dim);
    p.drawText(QPointF(14 + step + QFontMetricsF(fv).horizontalAdvance(units) + 8, 86), unitLabel(sp.unit));
    p.setFont(fv); p.setPen(c().white);
    p.drawText(QPointF(14 + 2 * step, 86), std::isfinite(m_c.stopDist) ? fmt::num(m_c.stopDist, sp.decimals, false) : fmt::dash());
    // direction badge
    if (std::isfinite(m_c.stopDist) && m_c.stopDist > 0) {
      const QRectF b(r.width() - 15 - 50, 16, 50, 18);
      p.setPen(Qt::NoPen); p.setBrush(m_c.longSide ? theme::map("#052931") : theme::map("#2f0a0a")); p.drawRoundedRect(b, 3, 3);
      p.setFont(mono(9, true, 0.5)); p.setPen(m_c.longSide ? theme::map("#3fd0e8") : c().redText);
      p.drawText(b, Qt::AlignCenter, m_c.longSide ? "↗ LONG" : "↘ SHORT");
    }
  });
  auto card = [this](PaintPanel* w, bool reward) {
    w->setPaint([this, reward](QPainter& p, const QRectF&, const QPointF&, double) {
      const double v = reward ? m_c.reward : m_c.riskAmt;
      p.setFont(mono(28, true)); p.setPen(reward ? theme::map("#41cee9") : theme::map("#f07070"));
      p.drawText(QPointF(14, 64), (reward ? m_c.ok && m_c.hasTp && !m_c.tpWrong : std::isfinite(v)) ? fmtNum(v, 2) : fmt::dash());
      p.setFont(cond(11, false)); p.setPen(c().faint);
      QString sub;
      if (reward) sub = !m_c.hasTp ? QCoreApplication::translate("account", "no take profit set")
                      : m_c.tpWrong ? QCoreApplication::translate("account", "take profit on the stop side")
                                    : QCoreApplication::translate("account", "at take profit");
      else if (!std::isfinite(m_c.riskPct)) sub = QCoreApplication::translate("account", "of balance");
      else if (m_c.ok && m_c.belowMinLot) {
        sub = QCoreApplication::translate("account", "%1% of balance · over your %2%").arg(fmt::num(m_c.riskPct, 2), fmt::num(m_c.targetPct, 2));
        p.setPen(c().redText);
      } else if (m_c.ok && std::isfinite(m_c.targetPct) && m_c.targetPct - m_c.riskPct >= 0.005)
        sub = QCoreApplication::translate("account", "%1% of balance · target %2%, size rounded down").arg(fmt::num(m_c.riskPct, 2), fmt::num(m_c.targetPct, 2));
      else sub = QCoreApplication::translate("account", "%1% of balance").arg(fmt::num(m_c.riskPct, 2));
      p.drawText(QPointF(14, 88), sub);
    });
  };
  card(m_riskCard, false); card(m_rewardCard, true);

  m_map->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    if (!std::isfinite(m_c.entry) || !std::isfinite(m_c.stop) || m_c.stop == m_c.entry) return;
    const auto sp = account::spec(m_picker->symbol());
    QVector<double> lv{m_c.stop, m_c.entry};
    if (m_c.hasTp && !m_c.tpWrong) lv << m_c.tp;
    const double lo = *std::min_element(lv.begin(), lv.end()), hi = *std::max_element(lv.begin(), lv.end());
    const qreal x0 = 38, x1 = r.width() - 39;
    auto X = [&](double v) { return x0 + (v - lo) / (hi - lo) * (x1 - x0); };
    const qreal xs = X(m_c.stop), xe = X(m_c.entry);
    p.fillRect(QRectF(std::min(xs, xe), 75, std::abs(xe - xs), 11), theme::map("#9e5452"));
    if (lv.size() == 3) { const qreal xt = X(m_c.tp); p.fillRect(QRectF(std::min(xe, xt), 75, std::abs(xt - xe), 11), theme::map("#258690")); }
    auto marker = [&](double v, const QString& name, const QColor& lc, const QColor& line) {
      const qreal x = std::round(X(v));
      p.fillRect(QRectF(x - 1, 52, 2, 56), line);
      p.setFont(mono(9, true, 2)); p.setPen(lc);
      p.drawText(QRectF(x - 60, 30, 120, 16), Qt::AlignCenter, name);
      p.setFont(mono(12, true)); p.setPen(c().white);
      p.drawText(QRectF(x - 70, 113, 140, 18), Qt::AlignCenter, fmt::num(v, sp.decimals, false));
    };
    marker(m_c.stop, "STOP", c().redText, theme::map("#f79590"));
    marker(m_c.entry, QCoreApplication::translate("account", "ENTRY"), c().white, theme::map("#e4fbfd"));
    if (lv.size() == 3) marker(m_c.tp, QCoreApplication::translate("account", "TARGET"), theme::map("#3fc6d8"), theme::map("#60d2d9"));
  });

  m_rr->setPaint([this](QPainter& p, const QRectF& r, const QPointF&, double) {
    const qreal cx = r.width() / 2.0;
    const qreal k = std::clamp((r.height() - 24) / 390.0, 0.62, 1.6);   // compress on short windows, grow on tall ones
    const qreal cy = r.height() / 2.0;
    QRadialGradient g(QPointF(cx, cy - 25 * k), std::max(r.width(), r.height()) * 0.42);
    g.setColorAt(0, theme::map(28, 20, 4, 90)); g.setColorAt(0.6, theme::map(14, 10, 2, 30)); g.setColorAt(1, theme::map(0, 0, 0, 0));
    p.fillRect(r.adjusted(1, 1, -1, -1), g);
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(mono(10, false, 3)); p.setPen(c().faint);
    p.drawText(QRectF(0, cy - 77 * k - 25, r.width(), 16), Qt::AlignCenter, QCoreApplication::translate("account", "RISK / REWARD RATIO"));
    const bool has = m_c.ok && m_c.hasTp && !m_c.tpWrong && std::isfinite(m_c.rr);
    const QString ratio = has ? "1 : " + fmt::num(m_c.rr, 2) : "1 : —";
    const QFont fr = mono(int(72 * k), true);
    const qreal tw = QFontMetricsF(fr).horizontalAdvance(ratio);
    const QPointF base(cx - tw / 2, cy - 23 * k);
    glowText(p, m_glow, base, ratio, fr, theme::map("#ffd709"), int(14 * k), 110);
    p.setFont(fr); p.setPen(has ? theme::map("#ffd709") : c().dim); p.drawText(base, ratio);
    QString verdict = QCoreApplication::translate("account", "SET A TAKE PROFIT"); QColor vc = c().dim;
    if (has) {
      if (m_c.rr < 1) { verdict = QCoreApplication::translate("account", "POOR"); vc = c().redText; }
      else if (m_c.rr < 1.5) { verdict = QCoreApplication::translate("account", "MARGINAL"); vc = theme::map("#f0a040"); }
      else if (m_c.rr < 3) { verdict = QCoreApplication::translate("account", "ACCEPTABLE"); vc = theme::map("#e6c200"); }
      else { verdict = QCoreApplication::translate("account", "EXCELLENT"); vc = theme::map("#3fd0de"); }
    }
    p.setFont(cond(12, true)); p.setPen(vc);
    p.drawText(QRectF(0, cy - 8 * k - 2, r.width(), 18), Qt::AlignCenter, verdict);
    // scale 1:0 … 1:4
    const QRectF bar(cx - 237 * std::max(1.0, k), cy + 26 * k, 474 * std::max(1.0, k), 6);
    p.setPen(Qt::NoPen); p.setBrush(theme::map("#151006")); p.drawRoundedRect(bar, 3, 3);
    if (has) { p.setBrush(theme::map("#f8d61a")); p.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * std::min(m_c.rr, 4.0) / 4.0, 6), 3, 3); }
    p.fillRect(QRectF(bar.left() + bar.width() / 4 - 0.5, bar.top(), 1.5, 6), theme::map("#151006"));
    p.setFont(cond(9, false)); p.setPen(theme::map("#3a3630"));
    for (int i = 0; i <= 4; ++i) p.drawText(QRectF(bar.left() + bar.width() * i / 4 - 20, bar.bottom() + 4, 40, 12), Qt::AlignCenter, QString("1:%1").arg(i));
    // three stats
    const QStringList L{QCoreApplication::translate("account", "BREAKEVEN WIN RATE"), QCoreApplication::translate("account", "RISK"),
                        QCoreApplication::translate("account", "REWARD")};
    const double be = has ? 100.0 / (1.0 + m_c.rr) : NAN;
    const QStringList V{std::isfinite(be) ? fmt::num(be, 0) + "%" : fmt::dash(),
                        std::isfinite(m_c.riskAmt) ? trimNum(m_c.riskAmt, 2) : fmt::dash(),
                        has ? trimNum(m_c.reward, 2) : fmt::dash()};
    const QList<QColor> C{c().white, theme::map("#f07070"), theme::map("#41cee9")};
    for (int i = 0; i < 3; ++i) {
      const qreal x = cx + (i - 1) * 161 * std::max(1.0, k);
      p.setFont(mono(9, false, 2.8)); p.setPen(c().faint);
      p.drawText(QRectF(x - 120, cy + 70 * k, 240, 14), Qt::AlignCenter, L[i]);
      p.setFont(mono(15)); p.setPen(C[i]);
      p.drawText(QRectF(x - 120, cy + 85 * k, 240, 22), Qt::AlignCenter, V[i]);
    }
  });

  for (auto* e : {m_balance, m_risk}) connect(e, &QLineEdit::textEdited, this, [this, e] {
    settings::setValue(e == m_balance ? "account.risk.balance" : "account.risk.risk", e->text().trimmed());
    compute();
  });
  for (auto* e : {m_entry, m_stop, m_tp}) connect(e, &QLineEdit::textEdited, this, [this] { m_levelsTouched = true; compute(); });
  connect(m_picker, &InstrumentPicker::picked, this, [this](const QString& s) { setInstrument(s, true); });
  // the account currency comes from the Track Record's selected account: follow its changes (A13)
  connect(&account::Journal::instance(), &account::Journal::changed, this, [this] {
    if (accountCcy() == m_ccy) return;
    subscribe();
    compute();
  });
  connect(m_picker, &InstrumentPicker::openChanged, this, [this] { layoutPage(); });
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, [this](const QStringList&) {
    if (!isVisible()) return;   // every page's quotes come through here; showEvent catches up
    if (!m_prefilled && !m_levelsTouched) prefillLevels();
    compute();
  });

  // state from settings
  m_cash = settings::value("account.risk.unit").toString() == "cash";
  const auto* acc = account::Journal::instance().current();
  m_balance->setText(settings::value("account.risk.balance", acc && acc->balance > 0 ? QString::number(acc->balance, 'f', 0) : "10000").toString());
  m_risk->setText(settings::value("account.risk.risk", "1").toString());
  setInstrument(settings::value("account.risk.instrument", "EURUSD").toString(), true);

  // test hooks: DW_RC_OPEN=1 opens the list, DW_RC_UNIT=cash|pct, DW_RC_LEVELS=entry,stop,tp fixes the levels
  if (qEnvironmentVariableIsSet("DW_RC_UNIT")) m_cash = qEnvironmentVariable("DW_RC_UNIT") == "cash";
  if (qEnvironmentVariableIsSet("DW_RC_LEVELS")) {
    const auto l = qEnvironmentVariable("DW_RC_LEVELS").split(',');
    if (l.size() == 3) { m_entry->setText(l[0]); m_stop->setText(l[1]); m_tp->setText(l[2]); m_levelsTouched = true; m_prefilled = true; }
  }
  if (qEnvironmentVariable("DW_RC_OPEN") == "1") m_picker->setOpen(true);
  installTestHooks(this);
  compute();

  m_anim.setInterval(100);  // title dot pulse and block cursor blink (10 fps, two tiny rects)
  connect(&m_anim, &QTimer::timeout, this, [this] { ++m_tick; update(QRect(30, 8, 20, 20)); update(QRect(m_cursorX - 4, 6, 16, 24)); });
}

QString RiskCalculatorPage::accountCcy() const {
  const auto* a = account::Journal::instance().current();
  return a && !a->currency.isEmpty() ? a->currency : "USD";
}

void RiskCalculatorPage::setInstrument(const QString& sym, bool prefill) {
  const QString s = market::find(sym) ? market::find(sym)->symbol : "EURUSD";
  m_picker->setSymbol(s);
  settings::setValue("account.risk.instrument", s);
  m_levelsTouched = false; m_prefilled = false;
  subscribe();
  if (prefill) {
    m_entry->clear(); m_stop->clear(); m_tp->clear();
    prefillLevels();
  }
  compute();
  update(QRect(0, 0, width(), kTitleH));
}

void RiskCalculatorPage::subscribe() {
  m_ccy = accountCcy();
  const auto sp = account::spec(m_picker->symbol());
  QStringList syms{m_picker->symbol()};
  syms << account::fxSymbolsFor(sp.quoteCcy, accountCcy());
  market::QuoteHub::instance().unsubscribe(this);
  market::QuoteHub::instance().subscribe(this, syms, isVisible());
}

void RiskCalculatorPage::prefillLevels() {
  const QString sym = m_picker->symbol();
  const auto q = market::QuoteHub::instance().quote(sym);
  if (!q.valid()) return;
  const auto sp = account::spec(sym);
  const int dec = sp.decimals;
  double dist;
  if (sp.tag == "FX") dist = sp.pip * 50;
  else {
    const double raw = q.price * 0.005;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    dist = std::round(raw / mag * 2) / 2 * mag;    // 0.5% of the price, rounded to a readable step
  }
  const double entry = q.price;
  m_entry->setText(QString::number(entry, 'f', dec));
  m_stop->setText(QString::number(entry - dist, 'f', dec));
  m_tp->setText(QString::number(entry + 2 * dist, 'f', dec));
  m_prefilled = true;
}

void RiskCalculatorPage::compute() {
  Calc c0;
  // typed numbers: a single comma is the decimal mark ("157,250" = 157.25), groups only when clear (A4)
  auto num = [](QLineEdit* e) { return e->text().trimmed().isEmpty() ? NAN : account::parseNumber(e->text(), true); };
  const auto sp = account::spec(m_picker->symbol());
  c0.balance = num(m_balance);
  const double risk = num(m_risk);
  c0.entry = num(m_entry); c0.stop = num(m_stop); c0.tp = num(m_tp);
  c0.hasTp = std::isfinite(c0.tp);
  QStringList err;
  if (!std::isfinite(c0.balance) || c0.balance <= 0) err << QCoreApplication::translate("account", "Account balance must be a positive number.");
  if (!std::isfinite(risk) || risk <= 0) err << QCoreApplication::translate("account", "Risk per trade must be a positive number.");
  if (std::isfinite(c0.balance) && std::isfinite(risk)) {
    c0.riskAmt = m_cash ? risk : c0.balance * risk / 100.0;
    c0.riskPct = c0.balance > 0 ? c0.riskAmt / c0.balance * 100.0 : NAN;
  }
  if (std::isfinite(c0.entry) && std::isfinite(c0.stop)) {
    c0.stopDist = std::abs(c0.entry - c0.stop);
    c0.longSide = c0.stop < c0.entry;
    if (c0.stopDist == 0) err << QCoreApplication::translate("account", "The stop loss must differ from the entry price.");
    if (c0.hasTp) c0.tpWrong = c0.longSide ? c0.tp <= c0.entry : c0.tp >= c0.entry;
  } else if (!m_entry->text().isEmpty() || !m_stop->text().isEmpty()) err << QCoreApplication::translate("account", "Enter an entry price and a stop loss.");
  c0.rate = account::fxRate(sp.quoteCcy, accountCcy());
  if (!std::isfinite(c0.rate)) err << QCoreApplication::translate("account", "Waiting for the live %1→%2 rate.").arg(sp.quoteCcy, accountCcy());
  if (err.isEmpty() && std::isfinite(c0.riskAmt) && c0.stopDist > 0) {
    // The size is rounded DOWN to the 0.01 lot step (A3): rounding to the nearest step could double the risk
    // (0.005 lot → 0.01). The risk shown is the one of the rounded size; when even the 0.01 minimum lot risks
    // more than asked, the page shows that minimum and its real risk in red.
    constexpr double kLotStep = 0.01, kMinLot = 0.01;
    const double perLot = c0.stopDist * c0.rate * sp.contract * sp.priceUnit;   // account money lost at the stop by 1.00 lot
    const double raw = c0.riskAmt / perLot;
    c0.targetPct = c0.riskPct;
    c0.lots = std::floor(raw / kLotStep + 1e-9) * kLotStep;
    c0.belowMinLot = c0.lots < kMinLot - 1e-12;
    if (c0.belowMinLot) c0.lots = kMinLot;
    c0.units = c0.lots * sp.contract;
    c0.riskAmt = c0.lots * perLot;
    c0.riskPct = c0.balance > 0 ? c0.riskAmt / c0.balance * 100.0 : NAN;
    if (c0.hasTp && !c0.tpWrong) {
      c0.rr = std::abs(c0.tp - c0.entry) / c0.stopDist;
      c0.reward = c0.units * std::abs(c0.tp - c0.entry) * c0.rate * sp.priceUnit;
    }
    c0.ok = true;
  }
  if (c0.hasTp && c0.tpWrong) err << QCoreApplication::translate("account", "The take profit is on the stop's side of the entry.");
  c0.error = err.join("\n");
  m_c = c0;
  for (auto* w : {m_left, m_toggle, m_pos, m_riskCard, m_rewardCard, m_map, m_rr}) w->update();
  m_foot->update();
}

void RiskCalculatorPage::layoutPage() {
  // Title bar fixed (painted by the page); everything below scrolls when the window is shorter than the content.
  const int W = width();
  m_scroll->setGeometry(0, kTitleH, W, height() - kTitleH);
  const int VH = m_scroll->viewport()->height();
  // ultrawide: the desk is capped at 2400 px and centred (stretching the cards over 3000+ px leaves them
  // mostly empty with tiny values far apart)
  const int cw = std::min(W, 2400), ox = (W - cw) / 2;
  // left column: 407 px at the reference size, a little wider on big windows
  const int leftW = std::clamp(int(cw * 0.2545), 407, 520);
  const int fx = 13, fw = leftW - 26;
  int y = 56;
  m_picker->setListHeight(232);
  const int pickerH = m_picker->preferredHeight();
  y += pickerH + 31;
  const int yBalance = y; y += 38 + 31;
  const int yRisk = y; y += 38 + 31;
  const int yEntry = y; y += 38 + 31;
  const int yStop = y; y += 38 + 30;
  const int yTp = y;
  const int leftNeed = 14 + yTp + 39 + 80 + 15;           // + room for the error text
  // right column
  const int rx = ox + 14 + leftW + 11, rw = ox + cw - 14 - rx;
  const QFontMetricsF fm(cond(10, false));
  const int footH = int(fm.boundingRect(QRectF(0, 0, rw, 200), Qt::TextWordWrap, footText()).height()) + 2;
  const int gap = VH < 744 ? 14 : 22;
  const int rightFixed = 14 + 141 + 108 + 140 + 3 * gap;
  const int rrMin = 300;
  const int contentH = std::max({VH, leftNeed, rightFixed + rrMin + gap + footH + 10});
  m_content->setGeometry(0, 0, m_scroll->viewport()->width(), contentH);
  m_left->setGeometry(ox + 14, 14, leftW, contentH - 14 - 15);
  m_picker->setGeometry(fx, 56, fw, pickerH);
  m_balance->setGeometry(fx, yBalance, fw, 38);
  m_risk->setGeometry(fx, yRisk, fw - 53, 38);
  m_toggle->setGeometry(fx + fw - 49, yRisk + 1, 47, 37);
  m_entry->setGeometry(fx, yEntry, fw, 38);
  m_stop->setGeometry(fx, yStop, fw, 38);
  m_tp->setGeometry(fx, yTp, fw, 39);
  int ry = 14;
  m_pos->setGeometry(rx, ry, rw, 141); ry += 141 + gap;
  const int half = (rw - 12) / 2;
  m_riskCard->setGeometry(rx, ry, half, 108);
  m_rewardCard->setGeometry(rx + half + 12, ry, rw - half - 12, 108); ry += 108 + gap;
  m_map->setGeometry(rx, ry, rw, 140); ry += 140 + gap;
  const int rrH = std::max(rrMin, contentH - std::max(footH + 12, 46) - ry + (footH > 14 ? 0 : 0));
  m_rr->setGeometry(rx, ry, rw, rrH);
  m_foot->setGeometry(rx, ry + rrH + 18, rw, footH);
  update();
}

void RiskCalculatorPage::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), c().page);
  // title bar
  QLinearGradient g(0, 0, 0, kTitleH);
  g.setColorAt(0, theme::map("#0f0a02")); g.setColorAt(1, theme::map("#0a0702"));
  p.fillRect(QRect(0, 0, width(), kTitleH - 1), g);
  p.fillRect(QRect(0, kTitleH - 1, width(), 1), theme::map("#463d17"));
  p.setRenderHint(QPainter::Antialiasing);
  // calculator glyph
  p.setPen(QPen(theme::map("#4d4201"), 1)); p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(15.5, 11.5, 9, 12), 1, 1);
  p.fillRect(QRectF(17, 13, 6, 3), theme::map("#4d4201"));
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 3; ++j) p.fillRect(QRectF(17 + i * 4, 17.5 + j * 2, 2, 1.2), theme::map("#4d4201"));
  // pulsing dot (1 s period)
  const double ph = std::fmod(m_tick * 0.1, 1.0);
  const double a = 0.5 + 0.5 * std::cos(ph * 2 * M_PI);
  p.setPen(Qt::NoPen); p.setBrush(theme::mix(theme::map("#7a7819"), theme::map("#efd632"), a));
  p.drawEllipse(QPointF(38, 17), 3, 3);
  const QFont tf = mono(10, false, 2.3);
  p.setFont(tf); p.setPen(c().faint);
  const QString title = QCoreApplication::translate("account", "RISK CALCULATOR — POSITION SIZING DESK");
  p.drawText(QPointF(50, 21), title);
  const qreal tw = QFontMetricsF(tf).horizontalAdvance(title);
  m_cursorX = int(50 + tw + 3);              // the title's width depends on the language
  if ((m_tick * 100 / 530) % 2 == 0) p.fillRect(QRectF(50 + tw + 3, 11, 7, 12), theme::map("#fbd911"));
  // badge
  const QString badge = QCoreApplication::translate("account", "• %1 · DETERMINISTIC MATH").arg(account::displaySymbol(m_picker->symbol()));
  const QFont bf = mono(9, true, 1.0);
  const qreal bw = QFontMetricsF(bf).horizontalAdvance(badge) + 16;
  const QRectF br(width() - 15 - bw, 9.5, bw, 18);
  p.setPen(theme::map("#544b03")); p.setBrush(theme::map("#1a1503")); p.drawRoundedRect(br, 2, 2);
  p.setFont(bf); p.setPen(theme::map("#e8d27a")); p.drawText(br, Qt::AlignCenter, badge);
}

QString RiskCalculatorPage::footText() const {
  const auto sp = account::spec(m_picker->symbol());
  const QString ccy = accountCcy();
  const QString conv = sp.quoteCcy == ccy ? QCoreApplication::translate("account", "converted from %1 into %2 at the live rate (no conversion needed)").arg(sp.quoteCcy, ccy)
                                          : QCoreApplication::translate("account", "converted from %1 into %2 at the live rate (%3)").arg(sp.quoteCcy, ccy,
                                              std::isfinite(m_c.rate) ? QString("1 %1 = %2 %3").arg(sp.quoteCcy, fmt::num(m_c.rate, 5), ccy) : QCoreApplication::translate("account", "waiting for it"));
  return QCoreApplication::translate("account", "Account in %1 · position size = risk ÷ stop distance in %2, %3 · 1 lot = %4 %5 · "
                 "levels prefilled from the live price, yours to change.")
           .arg(ccy, sp.display, conv, fmt::num(sp.contract, 0), unitLabel(sp.unit));
}

void RiskCalculatorPage::resizeEvent(QResizeEvent*) { layoutPage(); }
void RiskCalculatorPage::showEvent(QShowEvent*) {
  layoutPage();
  // quotes that landed while the page was hidden (other pages' polls) were not applied
  if (!m_prefilled && !m_levelsTouched) prefillLevels();
  compute();
  market::QuoteHub::instance().setActive(this, true);
  m_anim.start();
}
void RiskCalculatorPage::hideEvent(QHideEvent*) {
  market::QuoteHub::instance().setActive(this, false);
  m_anim.stop();
}
}

DW_PAGE("account/risk-calculator", acct::RiskCalculatorPage)
