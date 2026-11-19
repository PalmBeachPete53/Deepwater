#include "ui/Theme.h"
#include "core/Settings.h"
#include <QCoreApplication>
#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QtMath>
#include <QHash>

namespace theme {

// ---------------------------------------------------------------------------------------------------- theme table
namespace {
// The Deepwater palette, as designed (identity for map()).
const Palette kDeepwater{
  QColor("#060402"), QColor("#090603"), QColor("#14100a"), QColor("#433d2c"), QColor("#8a7e45"), QColor("#807752"),
  QColor("#fcd700"), QColor("#332b03"), QColor("#f5f2eb"), QColor("#a9a7a0"), QColor("#605c55"), QColor("#4fd1da"),
  QColor("#e0666b"), QColor("#fba800"), QColor("#8c96a8"), QColor("#2a2618"), QColor("#ebd88a"), QColor("#5fbf7f"),
  QColor("#1e1a0f"), QColor("#111111"), QColor("#2b2405"), QColor("#4c4e4e")};
}

const QList<ThemeInfo>& themes() {
  static const QList<ThemeInfo> t{
    {"deepwater", QT_TRANSLATE_NOOP("theme", "Deepwater"),
     QT_TRANSLATE_NOOP("theme", "Default theme: warm black, amber accents, cyan up and coral down."), true,
     Roles{QColor("#060402"), QColor("#2a2618"), QColor("#222630"), QColor("#08090c"), QColor("#46484e"), QColor("#5a5c62"),
           QColor("#3a3e48"), QColor("#c48a1a"), QColor("#090603"), QColor("#433d2c"), QColor("#111111"), QColor("#2b2405"),
           QColor("#f5f2eb"), QColor("#a9a7a0"), QColor("#605c55"), QColor("#4c4e4e"), QColor("#fcd700"), QColor("#4fd1da"),
           QColor("#e0666b"), QColor("#433d2c"), QColor("#8a7e45"), QColor("#15110a"), QColor("#8a7e45"), QColor("#0b0703"),
           QColor(0, 0, 0, 120), QColor("#e07a4f"), QColor("#e8c468"), QColor("#c9a27a"), QColor("#a08cc8"),
           QColor("#8dbfa8"), QColor("#f2eadf")}},
    {"ember", QT_TRANSLATE_NOOP("theme", "Ember"), QT_TRANSLATE_NOOP("theme", "Warm charcoal with gold accents."), true,
     Roles{QColor("#171310"), QColor("#2a231d"), QColor("#241c17"), QColor("#120e0c"), QColor("#332a23"), QColor("#4a3e33"), QColor("#3a3027"), QColor("#5a4530"), QColor("#1c1713"), QColor("#332a22"), QColor("#2a221c"), QColor("#3a2f26"), QColor("#ece3d6"), QColor("#9a8c7c"), QColor("#7d6f60"), QColor("#6f6356"), QColor("#e8c468"), QColor("#8dbfa8"), QColor("#e07a4f"), QColor("#3d3229"), QColor("#5a4c3f"), QColor("#241d18"), QColor("#4a3e33"), QColor("#16120f"), QColor(0, 0, 0, 120), QColor("#e07a4f"), QColor("#e8c468"), QColor("#c9a27a"), QColor("#a08cc8"), QColor("#8dbfa8"), QColor("#f2eadf")}},
    {"abyss", QT_TRANSLATE_NOOP("theme", "Abyss"), QT_TRANSLATE_NOOP("theme", "Deep-sea blue with aqua accents."), true,
     Roles{QColor("#0a1317"), QColor("#13222a"), QColor("#0f2430"), QColor("#061015"), QColor("#1a2e36"), QColor("#2a4651"), QColor("#173039"), QColor("#1f6a7c"), QColor("#0d191e"), QColor("#1b2e36"), QColor("#13252c"), QColor("#1b3540"), QColor("#d9ecef"), QColor("#7f9ba4"), QColor("#5f7f89"), QColor("#4d6871"), QColor("#6fe0d6"), QColor("#6fd3bf"), QColor("#ee8a6a"), QColor("#21404a"), QColor("#35626f"), QColor("#11232a"), QColor("#2a4651"), QColor("#08141a"), QColor(0, 0, 0, 120), QColor("#e07a4f"), QColor("#e8c468"), QColor("#c9a27a"), QColor("#a08cc8"), QColor("#8dbfa8"), QColor("#f2eadf")}},
    {"jade", QT_TRANSLATE_NOOP("theme", "Jade"), QT_TRANSLATE_NOOP("theme", "Dark green with jade accents."), true,
     Roles{QColor("#0d1411"), QColor("#17231d"), QColor("#13241e"), QColor("#07100c"), QColor("#21342b"), QColor("#33503f"), QColor("#1b2e25"), QColor("#2e6b4b"), QColor("#111a16"), QColor("#1f2e27"), QColor("#18261f"), QColor("#213a2e"), QColor("#e2ede6"), QColor("#8aa497"), QColor("#6a8778"), QColor("#557063"), QColor("#5cc98f"), QColor("#7fd6a4"), QColor("#e8866a"), QColor("#24392f"), QColor("#37584a"), QColor("#15241d"), QColor("#33503f"), QColor("#0a120e"), QColor(0, 0, 0, 120), QColor("#e07a4f"), QColor("#e8c468"), QColor("#c9a27a"), QColor("#a08cc8"), QColor("#8dbfa8"), QColor("#f2eadf")}},
    {"oxide", QT_TRANSLATE_NOOP("theme", "Oxide"), QT_TRANSLATE_NOOP("theme", "Rust browns with a red-orange accent."), true,
     Roles{QColor("#160f0d"), QColor("#291b17"), QColor("#26150f"), QColor("#110a08"), QColor("#3a231c"), QColor("#57342a"), QColor("#3a241d"), QColor("#7a3422"), QColor("#1b1210"), QColor("#36231d"), QColor("#2a1a16"), QColor("#3e231c"), QColor("#f0e2dc"), QColor("#a88a80"), QColor("#8a6c62"), QColor("#73574e"), QColor("#d9573a"), QColor("#8dbfa8"), QColor("#ef6a4c"), QColor("#432a23"), QColor("#64392e"), QColor("#26170f"), QColor("#57342a"), QColor("#130b09"), QColor(0, 0, 0, 120), QColor("#e07a4f"), QColor("#e8c468"), QColor("#c9a27a"), QColor("#a08cc8"), QColor("#8dbfa8"), QColor("#f2eadf")}},
    {"nova", QT_TRANSLATE_NOOP("theme", "Nova"), QT_TRANSLATE_NOOP("theme", "Night violet with lavender accents."), true,
     Roles{QColor("#110c19"), QColor("#1f172d"), QColor("#1c1430"), QColor("#0a0711"), QColor("#2b2140"), QColor("#46365f"), QColor("#251c37"), QColor("#6b42a8"), QColor("#161020"), QColor("#2a2039"), QColor("#221a32"), QColor("#2f2447"), QColor("#ece6f6"), QColor("#9d91b6"), QColor("#7e7298"), QColor("#675b80"), QColor("#b98cff"), QColor("#79d6b0"), QColor("#f07a8a"), QColor("#33284a"), QColor("#51406f"), QColor("#1d1529"), QColor("#46365f"), QColor("#0d0914"), QColor(0, 0, 0, 120), QColor("#e07a4f"), QColor("#e8c468"), QColor("#c9a27a"), QColor("#a08cc8"), QColor("#8dbfa8"), QColor("#f2eadf")}},
    {"bloomberg", QT_TRANSLATE_NOOP("theme", "Bloomberg"), QT_TRANSLATE_NOOP("theme", "Pure black, amber text and white accents, like a classic market terminal."), true,
     Roles{QColor("#000000"), QColor("#141414"), QColor("#07101c"), QColor("#000000"), QColor("#1b1b1b"), QColor("#5c3b0c"), QColor("#16161a"), QColor("#4a3006"), QColor("#080808"), QColor("#2b2b2b"), QColor("#161616"), QColor("#0e2a55"), QColor("#ffa028"), QColor("#c27d22"), QColor("#e8e8e8"), QColor("#7d5418"), QColor("#ffffff"), QColor("#3ddc6a"), QColor("#ff433d"), QColor("#333333"), QColor("#8a5a14"), QColor("#101010"), QColor("#5c3b0c"), QColor("#000000"), QColor(0, 0, 0, 150), QColor("#ff6a1a"), QColor("#ffd400"), QColor("#9fb3c8"), QColor("#b48cff"), QColor("#35c2d6"), QColor("#ffffff")}},
    {"glacier", QT_TRANSLATE_NOOP("theme", "Glacier"), QT_TRANSLATE_NOOP("theme", "Light: ice white with blue accents."), false,
     Roles{QColor("#eef3f6"), QColor("#dfe7ed"), QColor("#d9e6ef"), QColor("#bed0de"), QColor("#f9fbfc"), QColor("#b4c6d3"), QColor("#c6d5e0"), QColor("#9cc2de"), QColor("#f7fafc"), QColor("#d8e2ea"), QColor("#e7eff5"), QColor("#d9e8f3"), QColor("#1c2a35"), QColor("#5d7181"), QColor("#788c9a"), QColor("#98a9b5"), QColor("#2b7fc0"), QColor("#18916b"), QColor("#cf4f34"), QColor("#cdd9e2"), QColor("#a6bccb"), QColor("#ffffff"), QColor("#c2d1db"), QColor("#ffffff"), QColor(30, 60, 90, 55), QColor("#d2602e"), QColor("#c4911a"), QColor("#9a6a42"), QColor("#7558b3"), QColor("#2f9677"), QColor("#34475a")}},
    {"dune", QT_TRANSLATE_NOOP("theme", "Dune"), QT_TRANSLATE_NOOP("theme", "Light: sand with burnt-orange accents."), false,
     Roles{QColor("#f4ecdc"), QColor("#e8dcc6"), QColor("#e9dcc2"), QColor("#d6c29f"), QColor("#fcf6ea"), QColor("#cdb48c"), QColor("#dccaa8"), QColor("#e0a868"), QColor("#f9f3e7"), QColor("#e6d8bf"), QColor("#f1e6d1"), QColor("#ecdcbd"), QColor("#3a2a1a"), QColor("#7a6550"), QColor("#957d63"), QColor("#b29d84"), QColor("#c4661f"), QColor("#3d8b5e"), QColor("#b8402c"), QColor("#e0cfb2"), QColor("#c9ae84"), QColor("#fffaf1"), QColor("#dcc7a4"), QColor("#fffaf1"), QColor(110, 70, 30, 55), QColor("#c9452b"), QColor("#b8860b"), QColor("#5b6f80"), QColor("#7a4fa8"), QColor("#2a8a74"), QColor("#3c3a4a")}},
    {"sakura", QT_TRANSLATE_NOOP("theme", "Sakura"), QT_TRANSLATE_NOOP("theme", "Light: blush pink with rose accents."), false,
     Roles{QColor("#f9eef1"), QColor("#efdde3"), QColor("#f1dbe2"), QColor("#dfbfc9"), QColor("#fdf7f8"), QColor("#d3a8b6"), QColor("#e4c6cf"), QColor("#f0a3b8"), QColor("#fcf5f7"), QColor("#ecd6dd"), QColor("#f6e4e9"), QColor("#f1d4dc"), QColor("#3b2430"), QColor("#7a5a67"), QColor("#997583"), QColor("#b899a4"), QColor("#cf3f72"), QColor("#2e9466"), QColor("#c23b2e"), QColor("#e8cfd7"), QColor("#d1a7b5"), QColor("#fffafb"), QColor("#e0c2cc"), QColor("#fffafb"), QColor(120, 50, 80, 55), QColor("#d1602a"), QColor("#c4911a"), QColor("#5f6b86"), QColor("#7a4fa8"), QColor("#2a8a74"), QColor("#4a3a4a")}},
  };
  return t;
}

// ---------------------------------------------------------------------------------------------------- current theme
namespace {
int g_index = -1;
int g_revision = 0;
Palette g_pal = kDeepwater;
QHash<QRgb, QRgb> g_cache;   // GUI thread only

int indexOf(const QString& id) {
  const auto& t = themes();
  for (int i = 0; i < t.size(); ++i) if (t[i].id == id) return i;
  return 0;
}
int idx() {
  if (g_index < 0) g_index = indexOf(settings::value("ui.theme", "deepwater").toString());
  return g_index;
}

double luma(const QColor& c) { return 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF(); }
double smooth(double e0, double e1, double x) { const double t = qBound(0.0, (x - e0) / (e1 - e0), 1.0); return t * t * (3 - 2 * t); }
QColor lerp(const QColor& a, const QColor& b, double t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t);
}
struct Anchor { double y; QColor c; };
QColor ramp(const QList<Anchor>& a, double y) {
  if (y <= a.first().y) return a.first().c;
  for (int i = 1; i < a.size(); ++i)
    if (y <= a[i].y) return lerp(a[i - 1].c, a[i].c, (y - a[i - 1].y) / qMax(1e-6, a[i].y - a[i - 1].y));
  return a.last().c;
}

// Deepwater colour → current theme (alpha ignored here). Neutrals follow a lightness ramp anchored on the Deepwater
// roles (background … text); coloured pixels follow their family: amber → accent, cyan/green → up, coral/red →
// down, blues/purples (chart series) keep their hue with a lightness fitted to the theme.
QRgb mapRgb(QRgb in) {
  const ThemeInfo& th = themes()[idx()];
  const Roles& r = th.roles;
  const QColor c = QColor::fromRgb(in);
  const double y = luma(c);
  const QColor white(255, 255, 255), black(0, 0, 0);
  static const double yBg = luma(QColor("#060402")), yPanel = luma(QColor("#090603")), yHover = luma(QColor("#111111")),
      yGrid = luma(QColor("#2a2618")), yBorder = luma(QColor("#433d2c")), yFaint = luma(QColor("#4c4e4e")),
      ySection = luma(QColor("#605c55")), yDim = luma(QColor("#a9a7a0")), yText = luma(QColor("#f5f2eb")),
      yActive = luma(QColor("#2b2405")), yAccent = luma(QColor("#fcd700")), yUp = luma(QColor("#4fd1da")),
      yDown = luma(QColor("#e0666b"));
  const QColor brightest = th.dark ? lerp(r.text, white, 0.4) : lerp(r.text, black, 0.3);
  const QColor neutral = ramp({{0, r.background}, {yBg, r.background}, {yPanel, r.panel}, {yHover, r.hover},
                               {yGrid, r.gridLine}, {yBorder, r.panelLine}, {yFaint, r.textFaint},
                               {ySection, r.textSection}, {yDim, r.textDim}, {yText, r.text}, {1.0, brightest}}, y);
  const double chroma = (qMax(c.redF(), qMax(c.greenF(), c.blueF())) - qMin(c.redF(), qMin(c.greenF(), c.blueF())));
  const double wf = smooth(0.05, 0.18, chroma);
  if (wf <= 0) return neutral.rgb();
  double h = c.hueF() * 360.0; if (h < 0) h = 0;
  // hue weights: down (red/coral), accent (orange…yellow), up (green…cyan), series (blue…purple)
  double wD = 0, wA = 0, wU = 0, wS = 0;
  if (h < 18 || h >= 335) wD = 1;
  else if (h < 30) { wA = (h - 18) / 12; wD = 1 - wA; }
  else if (h < 80) wA = 1;
  else if (h < 100) { wU = (h - 80) / 20; wA = 1 - wU; }
  else if (h < 200) wU = 1;
  else if (h < 215) { wS = (h - 200) / 15; wU = 1 - wS; }
  else if (h < 315) wS = 1;
  else { wD = (h - 315) / 20; wS = 1 - wD; }
  const QColor brightAcc = lerp(r.accent, brightest, 0.45), brightUp = lerp(r.up, brightest, 0.45), brightDown = lerp(r.down, brightest, 0.45);
  const QColor acc = ramp({{0, r.background}, {yActive, r.active}, {yAccent, r.accent}, {1.0, brightAcc}}, y);
  const QColor up = ramp({{0, r.background}, {yUp, r.up}, {1.0, brightUp}}, y);
  const QColor down = ramp({{0, r.background}, {yDown, r.down}, {1.0, brightDown}}, y);
  QColor series = c;
  if (!th.dark) {
    float hh, ss, ll; c.getHslF(&hh, &ss, &ll);
    if (ll >= 0.35f) ll = qBound(0.32f, 0.62f - (ll - 0.35f) * 0.5f, 0.55f);   // lines and text: darker on light
    else { ll = 1.0f - ll * 0.5f; ss *= 0.6f; }                                // dark tints → light tints
    series = QColor::fromHslF(hh < 0 ? 0 : hh, ss, ll);
  }
  const QColor fam = QColor::fromRgbF(acc.redF() * wA + up.redF() * wU + down.redF() * wD + series.redF() * wS,
                                      acc.greenF() * wA + up.greenF() * wU + down.greenF() * wD + series.greenF() * wS,
                                      acc.blueF() * wA + up.blueF() * wU + down.blueF() * wD + series.blueF() * wS);
  return lerp(neutral, fam, wf).rgb();
}

void rebuild() {
  g_cache.clear();
  ++g_revision;
  const auto& k = kDeepwater;
  g_pal = Palette{map(k.bg), map(k.panel), map(k.panelAlt), map(k.border), map(k.borderHi), map(k.bracket), map(k.accent),
                  map(k.accentDim), map(k.text), map(k.textDim), map(k.textMute), map(k.up), map(k.down), map(k.warn),
                  map(k.neutral), map(k.grid), map(k.accentText), map(k.posTape), map(k.divider), map(k.rowHover),
                  map(k.rowSelected), map(k.textFaint)};
}
}

const Palette& pal() {
  static bool init = [] { idx(); rebuild(); return true; }();
  Q_UNUSED(init);
  return g_pal;
}
const ThemeInfo& current() { return themes()[idx()]; }
QString currentId() { return current().id; }
bool isDefault() { return idx() == 0; }
bool isDark() { return current().dark; }
const Roles& roles() { return current().roles; }
int revision() { pal(); return g_revision; }

QColor map(const QColor& c) {
  if (!c.isValid() || idx() == 0) return c;
  const QRgb key = c.rgb();
  auto it = g_cache.constFind(key);
  QRgb out;
  if (it != g_cache.cend()) out = *it;
  else { out = mapRgb(key); g_cache.insert(key, out); }
  QColor m = QColor::fromRgb(out);
  m.setAlpha(c.alpha());
  return m;
}
QColor onAccent() {
  const QColor a = roles().accent;
  if (isDefault()) return QColor("#0b0800");
  return luma(a) > 0.45 ? (isDark() ? roles().background : QColor("#101010")) : QColor("#ffffff");
}
QImage mapImage(const QImage& img) {
  if (isDefault() || img.isNull()) return img;
  QImage out = img.convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < out.height(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < out.width(); ++x) {
      const QRgb p = line[x];
      if (qAlpha(p) == 0) continue;
      const QColor m = map(QColor::fromRgb(p));
      line[x] = qRgba(m.red(), m.green(), m.blue(), qAlpha(p));
    }
  }
  return out;
}
void setCurrent(const QString& id) {
  const int i = indexOf(id);
  if (i == idx() && settings::value("ui.theme").toString() == id) return;
  settings::setValue("ui.theme", themes()[i].id);
  g_index = i;
  rebuild();
  applyPalette();
  emit Notifier::instance().changed();
}
Notifier& Notifier::instance() { static Notifier* n = new Notifier; return *n; }

static QString g_mono = "JetBrains Mono", g_sans = "Inter", g_display = "Chakra Petch", g_cond = "Rajdhani";

QFont mono(int px, int weight, qreal ls) {
  QFont f(g_mono); f.setPixelSize(px); f.setWeight(QFont::Weight(weight));
  f.setStyleHint(QFont::Monospace);
  if (ls != 0) f.setLetterSpacing(QFont::AbsoluteSpacing, ls);
  f.setHintingPreference(QFont::PreferNoHinting);
  return f;
}
QFont sans(int px, int weight) {
  QFont f(g_sans); f.setPixelSize(px); f.setWeight(QFont::Weight(weight));
  return f;
}
QFont display(int px, int weight, qreal ls) {
  QFont f(g_display); f.setPixelSize(px); f.setWeight(QFont::Weight(weight));
  if (ls != 0) f.setLetterSpacing(QFont::AbsoluteSpacing, ls);
  return f;
}
QFont condensed(int px, int weight) { QFont f(g_cond); f.setPixelSize(px); f.setWeight(QFont::Weight(weight)); return f; }
QFont title() { return mono(13, QFont::Bold, 1.7); }     // measured: "RISK SENTIMENT" = 132 px wide
QFont microLabel() { return mono(9, QFont::Normal, 2.4); }
QColor zoneBand(int z) {
  static const QColor b[5] = {QColor("#3e2015"), QColor("#2b190f"), QColor("#14130a"), QColor("#182215"), QColor("#12312c")};
  return map(b[qBound(0, z, 4)]);
}
QColor currencyColor(const QString& ccy) {
  const QString k = ccy.toUpper();
  if (k == "USD") return map("#5db963"); if (k == "EUR") return map("#387af8");
  if (k == "GBP") return map("#00c6da"); if (k == "JPY") return map("#f7f6f6");
  if (k == "CHF") return map("#b864c6"); if (k == "CAD") return map("#f05154");
  if (k == "AUD") return map("#fba800"); if (k == "NZD") return map("#fceb50");
  return pal().neutral;
}

QColor mix(const QColor& a, const QColor& b, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
QColor alpha(const QColor& c, int a) { QColor x = c; x.setAlpha(a); return x; }
QColor signColor(double v) { return v > 0 ? pal().up : v < 0 ? pal().down : pal().textDim; }
QColor diverging(double t) {
  t = qBound(-1.0, t, 1.0);
  return t >= 0 ? mix(pal().panelAlt, pal().up, t) : mix(pal().panelAlt, pal().down, -t);
}

QString styleSheet() {
  const auto& p = pal();
  return QString(R"(
QWidget { color: %1; }
QToolTip { background: %8; color: %1; border: 1px solid %2; padding: 6px 8px; font-family: "JetBrains Mono"; font-size: 11px; }
QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
QScrollBar::handle:vertical { background: %3; border-radius: 3px; min-height: 24px; }
QScrollBar:horizontal { background: transparent; height: 6px; margin: 0; }
QScrollBar::handle:horizontal { background: %3; border-radius: 3px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollArea, QAbstractScrollArea { background: transparent; border: none; }
QLineEdit { background: %4; border: 1px solid %5; border-radius: 3px; padding: 5px 8px; color: %1;
  font-family: "JetBrains Mono"; font-size: 12px; selection-background-color: %6; }
QLineEdit:focus { border-color: %6; }
QComboBox { background: %4; border: 1px solid %5; border-radius: 3px; padding: 4px 8px; color: %1;
  font-family: "JetBrains Mono"; font-size: 12px; }
QComboBox QAbstractItemView { background: %9; border: 1px solid %5; color: %1; selection-background-color: %7; outline: 0; }
QMenu { background: %9; border: 1px solid %5; color: %1; font-family: "JetBrains Mono"; font-size: 12px; }
QMenu::item { padding: 6px 18px; } QMenu::item:selected { background: %7; color: %6; }
)").arg(p.text.name(), p.borderHi.name(), map("#3a3220").name(), p.panelAlt.name(), p.border.name(), p.accent.name(), p.accentDim.name(),
          map("#15110a").name(), map("#120d06").name());
}

void install() {
  const char* files[] = {":/fonts/JetBrainsMono.ttf", ":/fonts/Inter.ttf", ":/fonts/ChakraPetch-SemiBold.ttf",
                         ":/fonts/ChakraPetch-Bold.ttf", ":/fonts/Rajdhani-Medium.ttf", ":/fonts/Rajdhani-SemiBold.ttf",
                         ":/fonts/Rajdhani-Bold.ttf"};
  for (const char* f : files) {
    const int id = QFontDatabase::addApplicationFont(f);
    if (id < 0) continue;
    const auto fams = QFontDatabase::applicationFontFamilies(id);
    if (fams.isEmpty()) continue;
    const QString n(f);
    if (n.contains("JetBrains")) g_mono = fams.first();
    else if (n.contains("Inter")) g_sans = fams.first();
    else if (n.contains("Chakra")) g_display = fams.first();
    else g_cond = fams.first();
  }
  applyPalette();
}

void applyPalette() {
  const auto& p = pal();
  QPalette qp;
  qp.setColor(QPalette::Window, p.bg); qp.setColor(QPalette::Base, p.panel);
  qp.setColor(QPalette::AlternateBase, p.panelAlt); qp.setColor(QPalette::WindowText, p.text);
  qp.setColor(QPalette::Text, p.text); qp.setColor(QPalette::Button, p.panelAlt);
  qp.setColor(QPalette::ButtonText, p.text); qp.setColor(QPalette::Highlight, p.accentDim);
  qp.setColor(QPalette::HighlightedText, p.accent); qp.setColor(QPalette::ToolTipBase, p.panelAlt);
  qp.setColor(QPalette::ToolTipText, p.text); qp.setColor(QPalette::PlaceholderText, p.textMute);
  qApp->setPalette(qp);
  static bool fontSet = false;
  if (!fontSet) { qApp->setFont(sans(13)); fontSet = true; }
  qApp->setStyleSheet(styleSheet());
}
}
