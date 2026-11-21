#pragma once
// Data › Technical data, footprint history (user's request 2026-11-20: "les afficher dans un tableau différent
// concernant les valeurs du footprint fait sens considérant la nature des données"):
//   FootprintCollectionView  under the pair's timeframes: one row per footprint market (spot, perpetual), the days
//                            stored of those wanted, the period, the days the archives lack, the size, the state;
//   FootprintPanel           its own table: the minutes of one stored day (newest first) with the values a footprint
//                            gives (buy / sell volume, delta, POC, value area, imbalances, stacks), and the levels of
//                            the minute picked (bid × ask per basis-point level, highest price on top).
// Reads technical-data.sqlite (fp_days) and ArchiveCollector's progress; nothing here downloads.
#include "technicaldata/ArchiveCollector.h"
#include "ui/Kit.h"
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTableView;
class QTimer;

namespace technicaldatapage {

class FootprintCollectionView : public QWidget {
public:
  explicit FootprintCollectionView(QWidget* p);
  void setRows(const QVector<technicaldata::ArchiveCollector::Progress>& rows, bool collecting);
  const QVector<technicaldata::ArchiveCollector::Progress>& rows() const { return m_rows; }
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QVector<technicaldata::ArchiveCollector::Progress> m_rows;
  bool m_collecting = false;
};
// a row's state in words and its colour (the steps hook writes it down too)
std::pair<QString, QColor> footprintState(const technicaldata::ArchiveCollector::Progress& p, bool collecting);

class MinutesModel;
class LevelsModel;
class BandsModel;
class OiModel;

// The values of what the pair records, one tab each (only those it records): Footprint (a stored day's minutes and
// the levels of the minute picked), Book depth (a stored day's snapshots: the bands and the bids' share), Open interest
// (a stored day's 5-minute rows), Live book (the bands of the book kept in memory, refreshed every second).
class FootprintPanel : public ui::Panel {
public:
  explicit FootprintPanel(QWidget* parent);
  void setMarket(const QString& market);   // a pair; hidden when it records none of these
  void refresh();                          // the store changed
  QString dumpText() const;                // test hook: what the panel shows, in words
  bool setMode(const QString& id);         // "fp", "depth", "oi", "live" (test hook); false if the pair has no such tab
  // phones: the tabs (scrolling sideways) and the venue / day row under the title, the levels under the minutes
  void setPhone(bool on);
protected:
  void showEvent(QShowEvent* e) override;
  void hideEvent(QHideEvent* e) override;
private:
  void load();                             // the tab shown, from the store (or the live book)
  void loadFootprint();
  void loadDepth();
  void loadOi();
  void loadLive();
  void syncHeader();                       // venue pills and day navigation for the tab shown
  void pickMinute(int row);
  QString mode() const;
  QString venue() const;
  QString m_market;
  QStringList m_modes, m_venues, m_fpVenues;
  QVector<qint64> m_days;                  // stored days of what is shown, ascending
  qint64 m_day = 0;                        // shown (0 = the newest)
  bool m_follow = true;                    // showing the newest day: a newer one replaces it
  ui::PillBar *m_modeBar, *m_venueBar;
  QPushButton *m_prev, *m_next;
  QLabel *m_dayLabel, *m_info, *m_levelsHead;
  QStackedWidget* m_stack;
  QTableView *m_minutes, *m_levels, *m_depthTable, *m_oiTable, *m_liveTable;
  MinutesModel* m_minModel;
  LevelsModel* m_lvModel;
  BandsModel *m_depthModel, *m_liveModel;
  OiModel* m_oiModel;
  QTimer* m_liveTimer;
  bool m_dirty = true;
  bool m_phone = false;
  QScrollArea* m_modeScroll = nullptr;   // phones only: holds the tabs
  QWidget* m_navRow = nullptr;                 // phones: venue pills, day navigation (hidden otherwise)
  QHBoxLayout *m_navRowL = nullptr, *m_fpRow = nullptr;
};

}  // namespace technicaldatapage
