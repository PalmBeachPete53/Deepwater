#pragma once
#include <QWidget>
#include <QSet>
#include <QDate>
#include <QDateTime>

class QLineEdit;

namespace news::pages {

// Top of News › Calendar: search box, week navigator, COUNTRIES chips (+ More… menu), IMPACT segments.
class CalendarFilters : public QWidget {
  Q_OBJECT
public:
  explicit CalendarFilters(QWidget* parent = nullptr);
  void setWeek(const QDate& sunday);
  void setCounts(int all, int high, int medium, int low);
  void setOtherCurrencies(const QStringList& ccys);    // currencies of the week outside the eight majors
  QString search() const;
  bool accepts(const QString& ccy) const;              // country filter
  int impact() const { return m_impact; }              // 0 all, 1 high, 2 medium, 3 low
  static constexpr int kHeight = 143;
  static constexpr int kCompactHeight = 109;
  // short windows: the IMPACT segments move up beside the week navigator (one row less above the table)
  void setCompact(bool on);
  bool compact() const { return m_compact; }
  // phones (narrow page): country chips wrap over full-width rows, finger-sized targets; the height follows the width
  void setPhone(bool on);
  int phoneHeight(int width) const;
signals:
  void changed();
  void weekStep(int delta);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  struct Hit { QRectF r; int kind; QString id; };   // kind: 0 prev, 1 next, 2 chip, 3 more, 4 impact
  QList<Hit> hits() const;
  QList<Hit> phoneHits(double W) const;
  QLineEdit* m_search;
  QDate m_week;
  QSet<QString> m_off;            // deselected currencies
  QStringList m_others;
  int m_counts[4] = {0, 0, 0, 0};
  int m_impact = 0;
  int m_hover = -1;
  bool m_compact = false;
  bool m_phone = false;
  double impactX() const;         // left of the IMPACT label (10, or after the week navigator when compact)
  double impactY() const { return m_compact ? 49 : 116; }
};

// "Terminal Data · HH:MM:SS" footer with the static LIVE badge (31 px), shared by the News pages.
class NewsFooter : public QWidget {
public:
  explicit NewsFooter(QWidget* parent = nullptr);
  void setTime(const QDateTime& t) { m_t = t; update(); }
  void setLive(bool live) { m_live = live; update(); }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QDateTime m_t;
  bool m_live = true;
};
}  // namespace news::pages
