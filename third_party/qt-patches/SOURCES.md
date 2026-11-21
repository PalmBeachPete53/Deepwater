# Qt patches (iOS build only)

Files from Qt's own sources, compiled into the iOS app because Qt is linked statically there, so their definitions
take the place of the library's. Each carries Qt's licence header (LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only;
Deepwater ships under the AGPL-3.0, which GPL-3.0 code may be combined with).

| File | Origin | Why | Remove when |
|---|---|---|---|
| qbackingstoredefaultcompositor.cpp | qtbase v6.12.0 `src/gui/painting/qbackingstoredefaultcompositor.cpp` + upstream commit f9d9f89b50 (QTBUG-151881) | Qt 6.12.0 copies the whole window image on every repaint on iOS (measured 2026-11-21 on an iPhone 15 Plus: 35 % of a core on a static page) | Deepwater builds with Qt 6.12.1 or later |
