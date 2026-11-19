#include <QCoreApplication>
#include <QTemporaryDir>
#include "Check.h"
int main(int argc, char** argv) {
  // a throwaway profile: nothing touches the user's data or preferences
  QTemporaryDir profile;
  qputenv("DW_PROFILE", profile.path().toUtf8());
  QCoreApplication app(argc, argv);
  return check::runAll(argc, argv);
}
