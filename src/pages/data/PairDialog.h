#pragma once
// Data › Technical data: the settings of one pair (user's design, 2026-11-18) — the Binance spot pair (checked with
// Binance), the first day of its history, its timeframes, and its indicators (any Charts indicator with its settings,
// accepted only once technicaldata::checkIndicator found it can be stored), with the size it will take. Saving writes
// the pair to the store; the collector and the indicators follow on their own. What saving would delete (a timeframe
// removed, the bars before a later first day) is asked first; so is removing the pair.
#include <QString>
#include <functional>

class QDialog;
class QWidget;

namespace technicaldatapage {
// market empty: a new pair. Window-modal, returns at once; done(market) once saved or removed.
QDialog* openPairDialog(QWidget* parent, const QString& market, std::function<void(const QString& market)> done = {});
}
