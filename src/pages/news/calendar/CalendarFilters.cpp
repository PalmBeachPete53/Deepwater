#include "pages/news/calendar/CalendarFilters.h"
#include "ui/Theme.h"
#include "pages/news/ThemeColor.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include "ui/Kit.h"
#include "core/Settings.h"
#include <QPainter>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMenu>
#include <QLocale>

namespace news::pages {
// "Nov 8 - Nov 14, 2026" / "8 nov. - 14 nov. 2026"
static QString weekText(const QDate& w) {
  const QLocale l = i18n::locale();
  if (i18n::current() == i18n::Lang::French)
    return l.toString(w, "d MMM") + " - " + l.toString(w.addDays(6), "d MMM yyyy");
  return l.toString(w, "MMM d") + " - " + l.toString(w.addDays(6), "MMM d, yyyy");
}

namespace {
const news::pages::Dw kPageBg{"#0f0a02"}, kSep{"#2d281b"}, kSearchBg{"#150d06"}, kSearchBorder{"#342e22"};
const news::pages::Dw kWeek{"#b8b3aa"}, kChevron{"#76766e"}, kLabel{"#534f47"}, kChipBg{"#120b00"}, kChipBorder{"#413609"};
const news::pages::Dw kChipText{"#e2decb"}, kChipOffText{"#6d6a60"}, kChipOffBorder{"#2a2418"};
const news::pages::Dw kSegOn{"#343027"}, kSegOnText{"#e1e0d5"}, kSegOnCount{"#6a675e"}, kSegOff{"#6b685f"}, kSegOffCount{"#4a473f"};
const QStringList kMajors{"USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD"};
}

CalendarFilters::CalendarFilters(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setFixedHeight(kHeight);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_search = new QLineEdit(this);
  m_search->setPlaceholderText(QCoreApplication::translate("news", "Search event, country, or currency..."));
  m_search->setFrame(false);
  m_search->setFont(theme::sans(13));
  m_search->setStyleSheet(mapQss("QLineEdit{background:transparent;border:none;padding:0;color:#d3d2cb;font-family:'Inter';font-size:13px;"
                          "selection-background-color:#4a3f10;} QLineEdit:focus{border:none;}"));
  QPalette pl = m_search->palette(); pl.setColor(QPalette::PlaceholderText, theme::map("#4a453b")); m_search->setPalette(pl);
  connect(m_search, &QLineEdit::textChanged, this, [this] { emit changed(); });
  for (const auto& c : settings::value("news.calendar.countriesOff").toStringList()) m_off.insert(c);
  m_impact = settings::value("news.calendar.impact", 0).toInt();
}

QString CalendarFilters::search() const { return m_search->text().trimmed(); }
bool CalendarFilters::accepts(const QString& ccy) const {
  if (kMajors.contains(ccy)) return !m_off.contains(ccy);
  return !m_off.contains("OTHER:" + ccy);
}
void CalendarFilters::setWeek(const QDate& w) { m_week = w; update(); }
void CalendarFilters::setCounts(int a, int h, int m, int l) { m_counts[0] = a; m_counts[1] = h; m_counts[2] = m; m_counts[3] = l; update(); }
void CalendarFilters::setOtherCurrencies(const QStringList& c) { m_others = c; }

void CalendarFilters::setCompact(bool on) {
  if (on == m_compact) return;
  m_compact = on;
  setFixedHeight(on ? kCompactHeight : kHeight);
  m_hover = -1;
  update();
}
double CalendarFilters::impactX() const {
  if (!m_compact) return 10;
  const double tw = QFontMetricsF(theme::mono(13, QFont::DemiBold)).horizontalAdvance(weekText(m_week));
  return 36 + tw + 4 + 18 + 34;
}

void CalendarFilters::resizeEvent(QResizeEvent*) { m_search->setGeometry(36, 9, width() - 36 - 20, 25); }

QList<CalendarFilters::Hit> CalendarFilters::hits() const {
  QList<Hit> out;
  const QString wt = weekText(m_week);
  const double tw = QFontMetricsF(theme::mono(13, QFont::DemiBold)).horizontalAdvance(wt);
  out << Hit{QRectF(12, 49, 18, 20), 0, {}} << Hit{QRectF(36 + tw + 4, 49, 18, 20), 1, {}};
  double x = 65;
  const QFont cf = theme::condensed(12, QFont::Bold);
  const QFontMetricsF fm(cf);
  for (const auto& c : kMajors) {
    const double w = 5 + 21 + 4 + fm.horizontalAdvance(c) + 7;
    out << Hit{QRectF(x, 81, w, 22), 2, c};
    x += w + 6;
  }
  out << Hit{QRectF(x, 81, 80, 22), 3, {}};
  x = impactX() + 38;
  const QStringList names{QCoreApplication::translate("news", "All"), QCoreApplication::translate("news", "High"), QCoreApplication::translate("news", "Medium"), QCoreApplication::translate("news", "Low")};
  const QFont sf = theme::condensed(13, QFont::Medium);
  for (int i = 0; i < 4; ++i) {
    const double w = QFontMetricsF(sf).horizontalAdvance(names[i]) + QFontMetricsF(sf).horizontalAdvance(QString::number(m_counts[i])) + 4 + 14;
    out << Hit{QRectF(x, impactY(), w, 20), 4, QString::number(i)};
    x += w + 2;
  }
  return out;
}

void CalendarFilters::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const double W = width();
  p.fillRect(rect(), kPageBg);
  // search box
  p.fillRect(QRectF(9, 7, W - 19, 29), kSearchBg);
  p.setPen(kSearchBorder); p.drawRect(QRectF(9.5, 7.5, W - 20, 28));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(theme::map("#5e5a50"), 1.3)); p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(23, 20.5), 4.2, 4.2); p.drawLine(QPointF(26, 23.5), QPointF(29, 26.5));
  p.setRenderHint(QPainter::Antialiasing, false);
  for (double y : {43.0, 75.0, 109.0}) if (y < height()) p.fillRect(QRectF(0, y, W, 1), kSep);

  const auto hs = hits();
  // week navigator
  p.setFont(theme::mono(13, QFont::DemiBold)); p.setPen(kWeek);
  p.drawText(QRectF(36, 49, 400, 20), Qt::AlignLeft | Qt::AlignVCenter, weekText(m_week));
  p.setRenderHint(QPainter::Antialiasing, true);
  for (int i = 0; i < 2; ++i) {
    const QPointF c = hs[i].r.center();
    p.setPen(QPen(m_hover == i ? theme::map("#e8d491") : kChevron, 1.3));
    const double d = i == 0 ? 1 : -1;
    p.drawLine(c + QPointF(d * 2, -4), c + QPointF(-d * 2, 0)); p.drawLine(c + QPointF(-d * 2, 0), c + QPointF(d * 2, 4));
  }
  p.setRenderHint(QPainter::Antialiasing, false);

  // countries
  p.setFont(theme::mono(8, QFont::Normal, 0.4)); p.setPen(kLabel);
  p.drawText(QRectF(10, 81, 60, 22), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "COUNTRIES"));
  const QFont cf = theme::condensed(12, QFont::Bold);
  for (int i = 0; i < hs.size(); ++i) {
    const Hit& h = hs[i];
    if (h.kind == 2) {
      const bool on = !m_off.contains(h.id);
      p.fillRect(h.r, on ? (m_hover == i ? theme::map("#271d06") : kChipBg) : (m_hover == i ? theme::map("#14100a") : kPageBg));
      p.setPen(on ? kChipBorder : kChipOffBorder); p.drawRect(h.r.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setOpacity(on ? 1.0 : 0.35);
      p.drawPixmap(QPointF(h.r.x() + 5, h.r.center().y() - 7), ui::flag(h.id, QSize(21, 14)));
      p.setOpacity(1.0);
      p.setFont(cf); p.setPen(on ? kChipText : kChipOffText);
      p.drawText(QRectF(h.r.x() + 30, h.r.y(), h.r.width(), h.r.height()), Qt::AlignLeft | Qt::AlignVCenter, h.id);
    } else if (h.kind == 3) {
      int offOthers = 0; for (const auto& o : m_others) if (m_off.contains("OTHER:" + o)) ++offOthers;
      p.fillRect(h.r, m_hover == i ? theme::map("#14100a") : kPageBg);
      p.setPen(kChipOffBorder); p.drawRect(h.r.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setPen(QPen(kChipOffText.c(), 1)); p.drawEllipse(QPointF(h.r.x() + 11, h.r.center().y()), 4, 4);
      p.drawLine(QPointF(h.r.x() + 8.5, h.r.center().y() + 2.5), QPointF(h.r.x() + 13.5, h.r.center().y() - 2.5));
      const QPointF c(h.r.right() - 11, h.r.center().y());
      p.drawLine(c + QPointF(-3, -1.5), c + QPointF(0, 1.5)); p.drawLine(c + QPointF(0, 1.5), c + QPointF(3, -1.5));
      p.setRenderHint(QPainter::Antialiasing, false);
      p.setFont(theme::condensed(13, QFont::Medium)); p.setPen(kChipOffText);
      p.drawText(QRectF(h.r.x() + 20, h.r.y(), 50, h.r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                 offOthers ? QCoreApplication::translate("news", "More (%1)").arg(m_others.size() - offOthers) : QCoreApplication::translate("news", "More..."));
    } else if (h.kind == 4) {
      const int k = h.id.toInt();
      const bool on = k == m_impact;
      if (on) p.fillRect(h.r, kSegOn);
      else if (m_hover == i) p.fillRect(h.r, theme::map("#1a160c"));
      const QFont sf = theme::condensed(13, on ? QFont::DemiBold : QFont::Medium);
      p.setFont(sf);
      const QStringList names{QCoreApplication::translate("news", "All"), QCoreApplication::translate("news", "High"), QCoreApplication::translate("news", "Medium"), QCoreApplication::translate("news", "Low")};
      p.setPen(on ? kSegOnText : kSegOff);
      p.drawText(QRectF(h.r.x() + 7, h.r.y(), h.r.width(), h.r.height()), Qt::AlignLeft | Qt::AlignVCenter, names[k]);
      p.setPen(on ? kSegOnCount : kSegOffCount);
      p.drawText(QRectF(h.r.x() + 7 + QFontMetricsF(sf).horizontalAdvance(names[k]) + 4, h.r.y(), 60, h.r.height()),
                 Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_counts[k]));
    }
  }
  p.setFont(theme::mono(8, QFont::Normal, 0.4)); p.setPen(kLabel);
  p.drawText(QRectF(impactX(), impactY(), 40, 21), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "IMPACT"));
}

void CalendarFilters::mouseMoveEvent(QMouseEvent* e) {
  int h = -1; const auto hs = hits();
  for (int i = 0; i < hs.size(); ++i) if (hs[i].r.contains(e->position())) h = i;
  setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (h != m_hover) { m_hover = h; update(); }
}

void CalendarFilters::mousePressEvent(QMouseEvent* e) {
  const auto hs = hits();
  for (const auto& h : hs) {
    if (!h.r.contains(e->position())) continue;
    if (h.kind == 0) emit weekStep(-1);
    else if (h.kind == 1) emit weekStep(1);
    else if (h.kind == 2) {
      if (m_off.contains(h.id)) m_off.remove(h.id); else m_off.insert(h.id);
      settings::setValue("news.calendar.countriesOff", QStringList(m_off.cbegin(), m_off.cend()));
      update(); emit changed();
    } else if (h.kind == 3) {
      QMenu menu(this);
      if (m_others.isEmpty()) menu.addAction(QCoreApplication::translate("news", "No other currency this week"))->setEnabled(false);
      for (const auto& o : m_others) {
        auto* a = menu.addAction(o == "ALL" ? QCoreApplication::translate("news", "ALL — global events") : o);
        a->setCheckable(true); a->setChecked(!m_off.contains("OTHER:" + o));
        connect(a, &QAction::toggled, this, [this, o](bool on) {
          if (on) m_off.remove("OTHER:" + o); else m_off.insert("OTHER:" + o);
          settings::setValue("news.calendar.countriesOff", QStringList(m_off.cbegin(), m_off.cend()));
          update(); emit changed();
        });
      }
      menu.exec(mapToGlobal(h.r.bottomLeft().toPoint() + QPoint(0, 4)));
    } else if (h.kind == 4) {
      m_impact = h.id.toInt();
      settings::setValue("news.calendar.impact", m_impact);
      update(); emit changed();
    }
    return;
  }
  QWidget::mousePressEvent(e);
}
void CalendarFilters::leaveEvent(QEvent*) { m_hover = -1; update(); }

// ------------------------------------------------------------------------------------------------ footer
NewsFooter::NewsFooter(QWidget* parent) : QWidget(parent) { setFixedHeight(31); setAttribute(Qt::WA_OpaquePaintEvent); }
void NewsFooter::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), theme::map("#060402"));
  p.fillRect(QRectF(0, 0, width(), 1), theme::map("#1a1806"));
  p.setFont(theme::mono(10)); p.setPen(theme::map("#51504a"));
  p.drawText(QRectF(7, 1, 400, 30), Qt::AlignLeft | Qt::AlignVCenter,
             QCoreApplication::translate("news", "Terminal Data · %1").arg(m_t.isValid() ? m_t.toLocalTime().toString("HH:mm:ss") : QString("--:--:--")));
  const QString liveText = m_live ? QCoreApplication::translate("news", "LIVE") : QCoreApplication::translate("news", "STALE");
  const double bw = std::max(50.0, QFontMetricsF(theme::mono(10, QFont::Bold, 0.5)).horizontalAdvance(liveText) + 22);
  const QRectF b(width() - 6 - bw, 7, bw, 20);
  p.fillRect(b, m_live ? theme::map("#382e08") : theme::map("#1c1a14"));
  p.setPen(m_live ? theme::map("#5c4d12") : theme::map("#2c2a22")); p.drawRect(b.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen); p.setBrush(m_live ? theme::map("#a79711") : theme::map("#55534c"));
  p.drawEllipse(QPointF(b.x() + 8, b.center().y()), 2.5, 2.5);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::mono(10, QFont::Bold, 0.5)); p.setPen(m_live ? theme::map("#e8d491") : theme::map("#6b685f"));
  p.drawText(b.adjusted(14, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, m_live ? QCoreApplication::translate("news", "LIVE") : QCoreApplication::translate("news", "STALE"));
}
}  // namespace news::pages
