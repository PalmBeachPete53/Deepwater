#pragma once
#include "account/Journal.h"

class QWidget;

// Dialogs of the Track Record page, in the app's style.
namespace acct {
bool tradeDialog(QWidget* parent, account::Trade& t, const QString& accountCcy, bool isNew);   // + LOG TRADE / edit
bool accountDialog(QWidget* parent, account::Account& a, bool isNew);
void importDialog(QWidget* parent);                                                            // files + live bridge
QString dialogStyle();
}
