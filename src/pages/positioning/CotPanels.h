#pragma once
#include <QWidget>
#include <QPixmap>
#include "positioning/Cot.h"
#include "market/Candles.h"

namespace positioning::pk { class Segmented; }
namespace positioning::pos { class CandleChart; }

// Panels of the COT page (private to src/pages/positioning). All are painted with QPainter; each takes the merged
// CFTC history (positioning::CotHistory) and the selected range in weeks.
namespace positioning::cotui {

// Base: dark COT panel with a 28 px title bar ("TITLE  dim subtitle"), no corner brackets.
class Panel : public QWidget {
public:
  Panel(const QString& title, QWidget* parent);
  void setTitle(const QString& t) { m_title = t; update(); }
  void setSubtitle(const QString& s) { m_sub = s; update(); }
  void setRightText(const QString& html) { m_right = html; update(); }   // simple "dim|bright|dim" runs, see paint
protected:
  void paintFrame(QPainter& p);
  bool event(QEvent* e) override;                 // header tooltip: full title + subtitle (they may be elided)
  virtual QString headerTip() const { return m_sub.isEmpty() ? QString() : m_title.trimmed() + "  " + m_sub; }
  static constexpr int kHead = 28;
  QString m_title, m_sub, m_right;
};

class PricePanel : public Panel {
  Q_OBJECT
public:
  explicit PricePanel(QWidget* parent);
  void setMarket(const positioning::CotMarket& m);
  void setHistory(const positioning::CotHistory& h);
  void setBars(const QVector<market::Bar>& bars, int decimals, const QString& note, const QString& timezone = {});
  void setLoading();
  void setHover(const QPoint& p);   // test hook (panel coordinates)
  QString headerTip() const override { return m_note; }
  bool weighted() const;
signals:
  void modeChanged(int mode);       // 0 unweighted index, 1 futures
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  pos::CandleChart* m_chart; pk::Segmented* m_mode;
  int m_fxMode = 0;                 // the user's choice for FX markets (others always show the future)
  positioning::CotHistory m_h; QVector<market::Bar> m_bars; int m_dec = 2; QString m_note;
};

class SplitPanel : public Panel {
  Q_OBJECT
public:
  explicit SplitPanel(QWidget* parent);
  void setData(const positioning::CotHistory& h, int weeks, int category);
  void setHover(const QPointF& p) { m_hover = p; m_has = true; update(); }
signals:
  void categoryChanged(int idx);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QRectF plot() const;
  pk::Segmented* m_cats;
  positioning::CotHistory m_h; int m_weeks = 52, m_cat = 2; QPointF m_hover; bool m_has = false;
};

class DonutPanel : public Panel {
public:
  explicit DonutPanel(QWidget* parent);
  void setData(const positioning::CotHistory& h, int category);
protected:
  void paintEvent(QPaintEvent*) override;
private:
  double m_long = NAN;
};

class NetChart : public Panel {
public:
  NetChart(const QString& title, QWidget* parent);
  // which: 0 legacy (Commercials, Large Speculators, Small Traders), 1 second report (4 categories), 2 nonreportable
  void setData(const positioning::CotHistory& h, int weeks, int which);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override { m_cache = {}; }
private:
  positioning::CotHistory m_h; int m_weeks = 52, m_which = 0; QPixmap m_cache;
  void render(QPainter& p);
};

class ReportTable : public Panel {
public:
  ReportTable(QWidget* parent, bool second);
  void setData(const positioning::CotHistory& h);
protected:
  void paintEvent(QPaintEvent*) override;
private:
  bool m_second; positioning::CotHistory m_h;
};

class SectorPanel : public Panel {
public:
  SectorPanel(QWidget* parent, bool second);
  void setData(const positioning::CotHistory& h);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override { m_cache = {}; }
private:
  bool m_second; positioning::CotHistory m_h; QPixmap m_cache;
};

class StatsPanel : public Panel {
public:
  StatsPanel(QWidget* parent, bool second);
  void setData(const positioning::CotHistory& h);
protected:
  void paintEvent(QPaintEvent*) override;
private:
  bool m_second; positioning::CotHistory m_h;
};

// "AUTOMATED READ": rule-based headline, paragraph and three bullets written from the CFTC figures (EN / FR), and
// each category's net position within its 52-week range.
class ReadPanel : public Panel {
  Q_OBJECT
public:
  explicit ReadPanel(QWidget* parent);
  void setData(const positioning::CotHistory& h);
  void setLanguage(int lang);   // 0 EN, 1 FR
  int language() const { return m_lang; }
  int neededHeight() const { return m_needed; }   // height the texts need (known after the first paint)
signals:
  void languageChanged(int lang);
  void neededHeightChanged();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  pk::Segmented* m_langSel; positioning::CotHistory m_h; int m_lang = 0, m_needed = 0;
};

// Texts of the automated read (exposed for tests and for other pages).
struct CotRead { QString headline, paragraph; QStringList bullets; };
CotRead writeRead(const positioning::CotHistory& h, int lang);
}
