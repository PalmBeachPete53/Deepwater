#pragma once
#include "pages/account/AccountKit.h"
#include "pages/account/TrackRecord.h"   // SmoothScroll
#include <QWidget>
#include <QTimer>
#include <QHash>
#include <QImage>

class QLineEdit;

namespace acct {

// Instrument picker: a combo that opens an inline, searchable list (pushes the form down).
class InstrumentPicker : public QWidget {
  Q_OBJECT
public:
  explicit InstrumentPicker(QWidget* parent = nullptr);
  void setSymbol(const QString& appSymbol);
  QString symbol() const { return m_symbol; }
  bool isOpen() const { return m_open; }
  void setOpen(bool on);
  void setListHeight(int h);                 // visible list height when open (default 232)
  int preferredHeight() const;               // 31 closed, 31 + list when open (38 on phones)
  // phones: 38 px combo, 34 px search, 36 px rows (finger targets); a finger drags the list when touch is on
  void setCompact(bool on);
signals:
  void picked(const QString& appSymbol);
  void openChanged(bool open);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
private:
  void refilter();
  int comboH() const { return m_compact ? 38 : 31; }
  int searchH() const { return m_compact ? 34 : 25; }
  double pitch() const { return m_compact ? 36 : 25.5; }
  double maxScroll() const;
  bool m_compact = false;
  struct Item { QString symbol, display, tag; };
  QList<Item> m_all, m_items;
  QString m_symbol;
  bool m_open = false;
  int m_listH = 232;
  // nullptr first: the search field's event filter runs while the constructor still builds the list (UBSan)
  QLineEdit* m_search = nullptr;
  PaintPanel* m_list = nullptr;
  PaintPanel* m_corner = nullptr;
  double m_scroll = 0;
};

class RiskCalculatorPage : public QWidget {
  Q_OBJECT
public:
  explicit RiskCalculatorPage(QWidget* parent = nullptr);
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  struct Calc {
    bool ok = false, longSide = true, hasTp = false, tpWrong = false;
    bool belowMinLot = false;    // the chosen risk buys less than the 0.01 minimum lot: lots = 0.01, over the risk
    QString error;
    // riskAmt / riskPct: what the lot size shown really risks (rounded DOWN to the 0.01 step, A3);
    // targetPct: the risk the user asked for
    double balance = NAN, riskAmt = NAN, riskPct = NAN, targetPct = NAN, stopDist = NAN, units = NAN, lots = NAN, reward = NAN, rr = NAN;
    double rate = NAN, entry = NAN, stop = NAN, tp = NAN;
  };
  void layoutPage();
  void layoutPhone();
  void compute();
  void setInstrument(const QString& sym, bool prefill);
  void prefillLevels();
  void subscribe();
  QString accountCcy() const;
  QString footText() const;

  InstrumentPicker* m_picker = nullptr;
  QLineEdit *m_balance = nullptr, *m_risk = nullptr, *m_entry = nullptr, *m_stop = nullptr, *m_tp = nullptr;
  SmoothScroll* m_scroll = nullptr;
  QWidget* m_content = nullptr;
  PaintPanel* m_foot = nullptr;
  PaintPanel *m_left = nullptr, *m_toggle = nullptr, *m_pos = nullptr, *m_riskCard = nullptr, *m_rewardCard = nullptr,
             *m_map = nullptr, *m_rr = nullptr;
  QString m_ccy;                 // account currency the quotes are subscribed for (A13: follows the journal)
  bool m_cash = false;           // risk per trade in money instead of % of balance
  bool m_levelsTouched = false;  // the user edited entry / stop / target since the instrument was chosen
  bool m_prefilled = false;
  bool m_phone = false;          // one-column layout (ui::compact of the page width)
  Calc m_c;
  QTimer m_anim; int m_tick = 0; int m_cursorX = 373;
  QHash<QString, QImage> m_glow;
};
}
