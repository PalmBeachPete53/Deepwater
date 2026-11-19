#pragma once
#include <QColor>
#include "ui/Theme.h"

// Tones of Market › Markets Watch measured on the reference frames (docs/reference/Market/markets-watch.md and
// Market/_shared-notes.md). Where the app palette has the same token, it is used; the others are the page's own
// measured tones (dark warm fills that theme::pal() does not carry yet).
namespace watch::col {
inline QColor page() { return theme::pal().bg; }
inline QColor panel() { return theme::map(QColor(0x0c, 0x06, 0x01)); }        // list panels, stat cards
inline QColor zebra() { return theme::map(QColor(0x14, 0x0f, 0x05)); }        // even rows / hover
inline QColor hover() { return theme::map(QColor(0x17, 0x11, 0x06)); }        // hovered row
inline QColor panelBorder() { return theme::map(QColor(0x43, 0x3d, 0x22)); }
inline QColor bracket() { return theme::map(QColor(0x87, 0x7c, 0x50)); }
inline QColor gold() { return theme::map(QColor(0xfd, 0xda, 0x11)); }         // active pill / segment
inline QColor goldDark() { return theme::map(QColor(0xc1, 0x9e, 0x00)); }     // count badge on the active pill
inline QColor goldText() { return theme::map(QColor(0xe6, 0xd3, 0x5f)); }     // panel titles
inline QColor subtle() { return theme::map(QColor(0x1e, 0x1a, 0x14)); }       // search box, segmented groups, pills
inline QColor text() { return theme::map(QColor(0xf1, 0xf1, 0xed)); }
inline QColor symbol() { return theme::map(QColor(0xed, 0xea, 0xe3)); }
inline QColor muted() { return theme::map(QColor(0x87, 0x84, 0x7e)); }
inline QColor label() { return theme::map(QColor(0x5a, 0x56, 0x4e)); }
inline QColor dim() { return theme::map(QColor(0x46, 0x41, 0x3a)); }
inline QColor up() { return theme::map(QColor(0x2d, 0xd5, 0xee)); }
inline QColor down() { return theme::map(QColor(0xe8, 0x62, 0x5a)); }
inline QColor sign(double v) { return v > 0 ? up() : v < 0 ? down() : muted(); }

// Instrument sheet
inline QColor sheetStrip() { return theme::map(QColor(0x0f, 0x0a, 0x02)); }
inline QColor sheetRule() { return theme::map(QColor(0x2a, 0x22, 0x1b)); }
inline QColor card() { return theme::map(QColor(0x08, 0x06, 0x03)); }
inline QColor cardBorder() { return theme::map(QColor(0x21, 0x1d, 0x12)); }
inline QColor cardRule() { return theme::map(QColor(0x27, 0x26, 0x18)); }
inline QColor cardTitle() { return theme::map(QColor(0xc8, 0xc4, 0xbc)); }
inline QColor axis() { return theme::map(QColor(0xa1, 0xa1, 0xa1)); }
inline QColor grid() { return theme::map(QColor(0x1a, 0x1a, 0x17)); }
inline QColor tileGrey() { return theme::map(QColor(0x27, 0x25, 0x23)); }
inline QColor tileRed() { return theme::map(QColor(0x47, 0x1b, 0x18)); }
inline QColor tileRedText() { return theme::map(QColor(0xf0, 0x50, 0x40)); }
inline QColor tileCyan() { return theme::map(QColor(0x0f, 0x3c, 0x42)); }
inline QColor tileCyanText() { return theme::map(QColor(0x3f, 0xd8, 0xea)); }
inline QColor tileText() { return theme::map(QColor(0xc8, 0xc4, 0xbc)); }
inline QColor tileLabel() { return theme::map(QColor(0x5a, 0x55, 0x51)); }
// Chart series
inline QColor seriesBlue() { return theme::map(QColor(0x5a, 0x86, 0xd0)); }
// second performance series: Deepwater gold; other themes take their "precious" category colour, because the
// accent (where Deepwater's gold maps) can be blue and would merge with the first (blue) series
inline QColor seriesGold() { return theme::isDefault() ? QColor(0xf0, 0xc8, 0x60) : theme::roles().precious; }
inline QColor seriesTeal() { return theme::map(QColor(0x57, 0xc8, 0xc7)); }
inline QColor barPos() { return theme::map(QColor(0x16, 0x8c, 0x98)); }
inline QColor barNeg() { return theme::map(QColor(0xa2, 0x54, 0x4d)); }
inline QColor barNow() { return theme::map(QColor(0xff, 0x82, 0x7e)); }
inline QColor candleUp() { return theme::map(QColor(0x00, 0xc4, 0xdf)); }
inline QColor candleDown() { return theme::map(QColor(0xff, 0x69, 0x72)); }
}
