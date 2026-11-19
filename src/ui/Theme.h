#pragma once
#include <QColor>
#include <QFont>
#include <QString>
#include <QList>
#include <QObject>
#include <QImage>

// Visual language measured on the reference videos (dark, amber accents, cyan up / coral down).
// Pages use these tokens instead of literal colours.
namespace theme {
struct Palette {
  QColor bg;          // window background
  QColor panel;       // panel fill
  QColor panelAlt;    // row hover, inputs, chips
  QColor border;      // panel border
  QColor borderHi;    // focused / selected border
  QColor bracket;     // panel corner brackets
  QColor accent;      // amber: titles, active tab, LIVE
  QColor accentDim;   // dim amber fills (pill backgrounds)
  QColor text;        // primary text
  QColor textDim;     // secondary text
  QColor textMute;    // labels, column headers
  QColor up;          // positive / long / risk-on (cyan)
  QColor down;        // negative / short / risk-off (coral red)
  QColor warn;        // orange
  QColor neutral;     // grey-blue neutral
  QColor grid;        // chart grid lines
  QColor accentText;  // panel titles and yellow labels (softer than accent)
  QColor posTape;     // green used for positive changes in the market tape only
  QColor divider;     // 1 px separators inside panels
  QColor rowHover;    // list row hover
  QColor rowSelected; // highlighted row (with a 2 px accent bar on the left)
  QColor textFaint;   // footers ("Terminal Data · HH:MM:SS")
};
// Zone band fills for 0–100 scores (index 0 = 0–20 … 4 = 80–100), measured on Risk Sentiment.
QColor zoneBand(int zone);
// Currency colours used by every FX-major chart (USD EUR GBP JPY CHF CAD AUD NZD).
QColor currencyColor(const QString& ccy);
const Palette& pal();

// ---- Themes (preview.html at the project root): Deepwater (default) + Ember, Abyss, Jade, Oxide, Nova, Bloomberg
// (dark) and Glacier, Dune, Sakura (light). Each defines the preview's roles; theme::map() projects any colour
// designed for Deepwater onto the current theme, so pages keep their measured Deepwater colours in code and
// call theme::map(c) where they paint (identity for Deepwater: the default look is unchanged to the pixel).
struct Roles {
  QColor background, gridLine, oceanInner, oceanOuter, land, landBorder, graticule, halo, panel, panelLine, hover,
         active, text, textDim, textSection, textFaint, accent, up, down, controlBorder, controlBorderHover, tooltip,
         tooltipBorder, clusterFill, limbShade;
  QColor energy, precious, industrial, agri, softs, hub;   // commodity category colours
};
struct ThemeInfo {
  QString id;
  const char* name;          // translate("theme", name)
  const char* description;   // translate("theme", description)
  bool dark;
  Roles roles;
};
const QList<ThemeInfo>& themes();
const ThemeInfo& current();
QString currentId();                 // settings key "ui.theme"; unknown or missing → "deepwater"
void setCurrent(const QString& id);  // saves, re-installs palette and style sheet, emits Notifier::changed
bool isDefault();                    // Deepwater: map() is the identity
bool isDark();
const Roles& roles();
int revision();                      // changes with the theme: key for caches of painted pixmaps

// Colour designed for the Deepwater theme → the same role in the current theme (alpha kept). Cached, cheap.
QColor map(const QColor& deepwaterColour);
inline QColor map(const char* hex) { return map(QColor(hex)); }
inline QColor map(int r, int g, int b, int a = 255) { return map(QColor(r, g, b, a)); }
QColor onAccent();                   // text / icon drawn on a filled accent area
// Deepwater-designed image (logo, cached layer) recoloured pixel by pixel; returns the input for Deepwater.
QImage mapImage(const QImage& deepwaterImage);

class Notifier : public QObject {
  Q_OBJECT
public:
  static Notifier& instance();
signals:
  void changed();
};

// Fonts bundled in resources/fonts: JetBrains Mono (numbers, labels, titles), Inter (prose),
// Chakra Petch (display: country/city names, uppercase), Rajdhani (condensed: headlines, event/bank names).
// Sizes are in pixels so pages match the reference at 1600x966.
QFont mono(int px, int weight = QFont::Normal, qreal letterSpacingPx = 0);
QFont sans(int px, int weight = QFont::Normal);
QFont display(int px, int weight = QFont::Bold, qreal letterSpacingPx = 0);
QFont condensed(int px, int weight = QFont::DemiBold);
QFont title();            // panel title: JetBrains Mono Bold 13 px, letter spacing 1.7 (Qt px), uppercase text expected
QFont microLabel();       // JetBrains Mono 9 px, letter spacing 2.4, uppercase (column headers, micro-labels)

void install();           // load fonts, set app palette and style sheet
void applyPalette();      // app palette and style sheet for the current theme
QString styleSheet();

QColor mix(const QColor& a, const QColor& b, qreal t);       // t=0 → a, t=1 → b
QColor alpha(const QColor& c, int a);
QColor signColor(double v);                                  // up / down / textDim for 0
// Diverging scale for heat maps: -1 → down, 0 → panelAlt, +1 → up
QColor diverging(double t);
}
