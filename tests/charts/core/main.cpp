#include <QCoreApplication>
#include "Check.h"
int main(int argc,char**argv){ QCoreApplication app(argc, argv); return check::runAll(argc, argv); }
