// Footprint history in Data › Technical data. See FootprintPanel.h.
#include "pages/data/FootprintPanel.h"
#include "technicaldata/FootprintArchive.h"
#include "technicaldata/LiveBook.h"
#include "technicaldata/PerpArchive.h"
#include "technicaldata/TechnicalDataSpec.h"
#include "technicaldata/TechnicalDataStore.h"
#include "ui/Theme.h"
#include <QAbstractTableModel>
#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>
#include <array>
#include <cmath>
#include <limits>

namespace technicaldatapage {
using namespace technicaldata;
namespace fpa = technicaldata::fp;

namespace {
QString T(const char* s) { return QCoreApplication::translate("data", s); }
QString n(double v, int dec) { return std::isfinite(v) ? QLocale(QLocale::English).toString(v, 'f', dec) : QStringLiteral("—"); }
QString signedN(double v, int dec) { return (v > 0 ? "+" : v < 0 ? "−" : "") + n(std::fabs(v), dec); }
QString dayText(const QDate& d) { return d.isValid() ? d.toString("yyyy-MM-dd") : QStringLiteral("—"); }
QString venueName(const QString& id) { return id == "spot" ? T("Spot") : T("Perpetual USD-M"); }
// a row of the archives' collection: what it holds, in words
QString kindName(const QString& kind) {
  if (kind == "spot") return T("Footprint spot");
  if (kind == "perp") return T("Footprint perp.");
  if (kind == "depth") return T("Depth perp.");
  return T("Open interest");
}
QString mb(qint64 b) { return b >= 1e9 ? QString("%1 GB").arg(n(b / 1e9, 2)) : QString("%1 MB").arg(n(b / 1e6, 1)); }
}  // namespace

// ------------------------------------------------------------------------------------------------ collection rows
std::pair<QString, QColor> footprintState(const ArchiveCollector::Progress& p, bool collecting) {
  const auto& c = theme::pal();
  if (p.paused) return {T("paused"), c.warn};
  if (p.running) return {T("reading %1").arg(dayText(p.current)), c.accent};
  if (!p.error.isEmpty()) return {T("error: %1 (asked again later)").arg(p.error), c.down};
  const int done = p.stored + p.missing;
  if (p.wanted > 0 && done >= p.wanted) return {collecting ? T("complete, a new day each morning") : T("complete"), collecting ? c.up : c.textDim};
  // only the last days are not there: Binance has not published them yet (the morning after, UTC)
  if (p.wanted > 0 && done + p.unpublished >= p.wanted)
    return {T("up to date to %1").arg(dayText(p.last)), collecting ? c.up : c.textDim};
  if (p.queued) return {T("waiting"), c.textDim};
  if (!collecting) return {p.stored ? T("history incomplete") : T("nothing yet"), c.textDim};
  return {T("history incomplete"), c.textDim};
}

namespace {
constexpr int kHead = 26, kRow = 46;
}

FootprintCollectionView::FootprintCollectionView(QWidget* p) : QWidget(p) { setMinimumHeight(kHead + 2 * kRow + 4); }
QSize FootprintCollectionView::sizeHint() const { return QSize(640, kHead + int(m_rows.size()) * kRow + 4); }

void FootprintCollectionView::setRows(const QVector<ArchiveCollector::Progress>& rows, bool collecting) {
  m_rows = rows;
  m_collecting = collecting;
  setMinimumHeight(kHead + int(rows.size()) * kRow + 4);
  updateGeometry();
  update();
}

void FootprintCollectionView::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& c = theme::pal();
  const int w = width();
  // MARKET | DAYS | FROM → TO | MISSING | SIZE | STATE. A phone (this view under 520 px) drops FROM → TO (the state
  // gives the last day) and SIZE; a dropped column has no width below and is skipped.
  const bool phone = w < 520;
  const double xd[] = {0, 128, 214, 334, 398, 478}, xp[] = {0, 116, 166, 166, 232, 232};
  const double* x = phone ? xp : xd;
  const QStringList heads = {T("ARCHIVES"), T("DAYS"), T("FROM → TO"), T("MISSING"), T("SIZE"), T("STATE")};
  p.setFont(theme::microLabel());
  p.setPen(c.textMute);
  for (int i = 0; i < heads.size(); ++i)
    if (!(phone && (i == 2 || i == 4)))
      p.drawText(QRectF(x[i] + 6, 0, (i + 1 < 6 ? x[i + 1] - x[i] : w - x[i]) - 6, kHead), Qt::AlignVCenter | Qt::AlignLeft, heads[i]);
  p.setPen(c.divider);
  p.drawLine(QPointF(0, kHead - 0.5), QPointF(w, kHead - 0.5));
  for (int r = 0; r < m_rows.size(); ++r) {
    const auto& row = m_rows[r];
    const double y = kHead + r * kRow;
    if (row.running) p.fillRect(QRectF(0, y, w, kRow), c.rowSelected);
    p.setFont(theme::mono(11, QFont::Bold));
    p.setPen(c.accentText);
    p.drawText(QRectF(x[0] + 6, y + 4, x[1] - x[0] - 6, 20), Qt::AlignVCenter, kindName(row.kind));
    p.setFont(theme::mono(9));
    p.setPen(c.textMute);
    const QString what = row.kind == "spot" || row.kind == "perp" ? T("%1 M trades").arg(n(row.rows / 1e6, 1))
                       : row.kind == "depth"                    ? T("%1 snapshots").arg(QLocale(QLocale::English).toString(row.rows))
                                                                : T("%1 rows").arg(QLocale(QLocale::English).toString(row.rows));
    p.drawText(QRectF(x[0] + 6, y + 22, x[1] - x[0] - 6, 18), Qt::AlignVCenter,
               phone ? QFontMetrics(p.font()).elidedText(what, Qt::ElideRight, int(x[1] - x[0] - 6)) : what);
    p.setFont(theme::mono(11));
    p.setPen(c.text);
    p.drawText(QRectF(x[1] + 6, y + 4, x[2] - x[1] - 6, 20), Qt::AlignVCenter, QString::number(row.stored));
    p.setFont(theme::mono(9));
    p.setPen(c.textMute);
    p.drawText(QRectF(x[1] + 6, y + 22, x[2] - x[1] - 6, 18), Qt::AlignVCenter, T("of %1").arg(row.wanted));
    p.setFont(theme::mono(10));
    p.setPen(c.textDim);
    if (!phone) {
      p.drawText(QRectF(x[2] + 6, y + 4, x[3] - x[2] - 6, 20), Qt::AlignVCenter, dayText(row.first));
      p.drawText(QRectF(x[2] + 6, y + 22, x[3] - x[2] - 6, 18), Qt::AlignVCenter, "→ " + dayText(row.last));
    }
    p.setFont(theme::mono(11));
    p.setPen(row.missing ? c.warn : c.textDim);
    p.drawText(QRectF(x[3] + 6, y + 4, x[4] - x[3] - 6, 20), Qt::AlignVCenter, row.missing ? QString::number(row.missing) : QStringLiteral("—"));
    if (!phone) {
      p.setPen(c.text);
      p.drawText(QRectF(x[4] + 6, y + 4, x[5] - x[4] - 6, 20), Qt::AlignVCenter, mb(row.bytes));
      p.setFont(theme::mono(9));
      p.setPen(c.textMute);
      p.drawText(QRectF(x[4] + 6, y + 22, x[5] - x[4] - 6, 18), Qt::AlignVCenter,
                 row.stored ? T("%1 KB a day").arg(n(double(row.bytes) / row.stored / 1e3, 0)) : QString());
    }
    const auto [state, sc] = footprintState(row, m_collecting);
    p.setPen(sc);
    p.setFont(theme::mono(10));
    p.drawText(QRectF(x[5] + 6, y + 4, w - x[5] - 6, 20), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(p.font()).elidedText(state, Qt::ElideRight, int(w - x[5] - 8)));
    const double done = row.wanted > 0 ? std::clamp(double(row.stored + row.missing) / row.wanted, 0.0, 1.0) : 0;
    const QRectF bar(x[5] + 6, y + 27, w - x[5] - 14, 5);
    p.setPen(Qt::NoPen);
    p.setBrush(c.panelAlt);
    p.drawRoundedRect(bar, 2, 2);
    p.setBrush(done >= 1 ? c.up : c.accent);
    p.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * done, bar.height()), 2, 2);
    p.setPen(c.divider);
    p.drawLine(QPointF(0, y + kRow - 0.5), QPointF(w, y + kRow - 0.5));
  }
}

// ------------------------------------------------------------------------------------------------ minutes table
// shown column → value column: time, bought, sold, delta, POC, VA low, VA high, imb. buy, imb. sell, stacks, levels,
// trades, volume, open, high, low, close
constexpr int kOrder[17] = {0, 6, 7, 8, 11, 12, 13, 14, 15, 16, 10, 9, 5, 1, 2, 3, 4};
class MinutesModel : public QAbstractTableModel {
public:
  void set(std::vector<fpa::Minute> minutes) {
    beginResetModel();
    m_min = std::move(minutes);
    m_stats.clear();
    m_stats.reserve(m_min.size());
    for (const auto& m : m_min) m_stats.push_back(fpa::statsOf(m));
    endResetModel();
  }
  const fpa::Minute* at(int row) const { return row >= 0 && row < int(m_min.size()) ? &m_min[m_min.size() - 1 - size_t(row)] : nullptr; }
  int rowCount(const QModelIndex& = {}) const override { return int(m_min.size()); }
  int columnCount(const QModelIndex& = {}) const override { return 17; }
  QVariant headerData(int s, Qt::Orientation o, int role) const override {
    static const char* h[] = {QT_TRANSLATE_NOOP("data", "time (UTC)"), QT_TRANSLATE_NOOP("data", "open"), QT_TRANSLATE_NOOP("data", "high"),
                              QT_TRANSLATE_NOOP("data", "low"), QT_TRANSLATE_NOOP("data", "close"), QT_TRANSLATE_NOOP("data", "volume"),
                              QT_TRANSLATE_NOOP("data", "bought"), QT_TRANSLATE_NOOP("data", "sold"), QT_TRANSLATE_NOOP("data", "delta"),
                              QT_TRANSLATE_NOOP("data", "trades"), QT_TRANSLATE_NOOP("data", "levels"), QT_TRANSLATE_NOOP("data", "POC"),
                              QT_TRANSLATE_NOOP("data", "VA low"), QT_TRANSLATE_NOOP("data", "VA high"), QT_TRANSLATE_NOOP("data", "imb. buy"),
                              QT_TRANSLATE_NOOP("data", "imb. sell"), QT_TRANSLATE_NOOP("data", "stacks")};
    s = kOrder[s];   // shown: the footprint's own values first (the bars table has the OHLC)
    if (o != Qt::Horizontal) return {};
    if (role == Qt::DisplayRole) return QCoreApplication::translate("data", h[s]);
    if (role == Qt::ToolTipRole) {
      switch (s) {
        case 6: return T("Volume of the market orders that bought (lifted the ask)");
        case 7: return T("Volume of the market orders that sold (hit the bid)");
        case 8: return T("Bought − sold");
        case 10: return T("Basis-point levels from the lowest traded price to the highest (one level = 0.01 % of the price)");
        case 11: return T("Point of control: the level with the most volume");
        case 12: case 13: return T("Value area: 70 % of the minute's volume around the POC");
        case 14: return T("Levels where buyers took at least 3 × what sellers gave one level below (diagonal)");
        case 15: return T("Levels where sellers gave at least 3 × what buyers took one level above (diagonal)");
        case 16: return T("Runs of 3 or more imbalances of one side: buy / sell");
        default: return {};
      }
    }
    return {};
  }
  QVariant data(const QModelIndex& shownIndex, int role) const override {
    const QModelIndex i = index(shownIndex.row(), kOrder[shownIndex.column()]);   // the column it stands for
    const fpa::Minute* m = at(i.row());
    if (!m) return {};
    const fpa::Stats& s = m_stats[m_min.size() - 1 - size_t(i.row())];
    const auto& c = theme::pal();
    if (role == Qt::TextAlignmentRole) return int(i.column() == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole) {
      if (i.column() == 0) return c.textDim;
      if (i.column() == 4) return m->close >= m->open ? c.up : c.down;
      if (i.column() == 6) return c.up;
      if (i.column() == 7) return c.down;
      if (i.column() == 8) return m->delta() >= 0 ? c.up : c.down;
      if (i.column() == 14 && s.buyImbalances) return c.up;
      if (i.column() == 15 && s.sellImbalances) return c.down;
      return c.text;
    }
    if (role != Qt::DisplayRole) return {};
    switch (i.column()) {
      case 0: return QDateTime::fromSecsSinceEpoch(m->t, QTimeZone::UTC).toString("HH:mm");
      case 1: return n(m->open, 2);
      case 2: return n(m->high, 2);
      case 3: return n(m->low, 2);
      case 4: return n(m->close, 2);
      case 5: return n(m->volume(), 3);
      case 6: return n(m->buy, 3);
      case 7: return n(m->sell, 3);
      case 8: return signedN(m->delta(), 3);
      case 9: return QLocale(QLocale::English).toString(m->trades);
      case 10: return s.levels;
      case 11: return n(s.poc, 2);
      case 12: return n(s.vaLow, 2);
      case 13: return n(s.vaHigh, 2);
      case 14: return s.buyImbalances;
      case 15: return s.sellImbalances;
      case 16: return QString("%1 / %2").arg(s.buyStacks).arg(s.sellStacks);
      default: return {};
    }
  }
private:
  std::vector<fpa::Minute> m_min;   // ascending; shown newest first
  std::vector<fpa::Stats> m_stats;
};

// ------------------------------------------------------------------------------------------------ levels table
class LevelsModel : public QAbstractTableModel {
public:
  void set(const fpa::Minute* m) {
    beginResetModel();
    m_rows.clear();
    if (m) {
      m_base = m->base;
      charts::flow::analyze(fpa::levelCandle(*m), charts::flow::FootprintParams{}, m_view);
      const fpa::Stats s = fpa::statsOf(*m);
      m_vaLo = fpa::levelOf(s.vaLow * (1 + 1e-9));
      m_vaHi = fpa::levelOf(s.vaHigh * (1 - 1e-9));
      for (auto it = m_view.rows.rbegin(); it != m_view.rows.rend(); ++it) m_rows.push_back(*it);   // highest price on top
    }
    endResetModel();
  }
  int rowCount(const QModelIndex& = {}) const override { return int(m_rows.size()); }
  int columnCount(const QModelIndex& = {}) const override { return 5; }
  QVariant headerData(int s, Qt::Orientation o, int role) const override {
    static const char* h[] = {QT_TRANSLATE_NOOP("data", "price"), QT_TRANSLATE_NOOP("data", "sold"), QT_TRANSLATE_NOOP("data", "bought"),
                              QT_TRANSLATE_NOOP("data", "delta"), ""};
    if (o != Qt::Horizontal) return {};
    if (role == Qt::ToolTipRole && s == 1) return QCoreApplication::translate("data", "Volume of the market orders that sold (hit the bid)");
    if (role == Qt::ToolTipRole && s == 2) return QCoreApplication::translate("data", "Volume of the market orders that bought (lifted the ask)");
    if (role != Qt::DisplayRole) return {};
    return QCoreApplication::translate("data", h[s]);
  }
  QVariant data(const QModelIndex& i, int role) const override {
    if (i.row() < 0 || i.row() >= int(m_rows.size())) return {};
    const auto& r = m_rows[size_t(i.row())];
    const auto& c = theme::pal();
    const bool inVa = r.ticks >= m_vaLo && r.ticks <= m_vaHi;
    if (role == Qt::TextAlignmentRole) return int(i.column() == 4 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole) {
      if (i.column() == 0) return inVa ? c.text : c.textMute;
      if (i.column() == 1) return (r.flags & charts::flow::SellImbalance) ? c.down : c.text;
      if (i.column() == 2) return (r.flags & charts::flow::BuyImbalance) ? c.up : c.text;
      if (i.column() == 3) return r.delta() >= 0 ? c.up : c.down;
      return c.textDim;
    }
    if (role == Qt::FontRole && ((i.column() == 1 && (r.flags & charts::flow::SellImbalance)) || (i.column() == 2 && (r.flags & charts::flow::BuyImbalance)))) {
      QFont f = theme::mono(11, QFont::Bold);
      return f;
    }
    if (role != Qt::DisplayRole) return {};
    switch (i.column()) {
      case 0: return n(fpa::levelLow(int32_t(r.ticks)), 2);
      case 1: return r.bid > 0 ? n(r.bid, 3) : QStringLiteral("·");
      case 2: return r.ask > 0 ? n(r.ask, 3) : QStringLiteral("·");
      case 3: return r.volume() > 0 ? signedN(r.delta(), 3) : QString();
      case 4: {
        QStringList marks;
        if (r.flags & charts::flow::Poc) marks << "POC";
        if (r.flags & charts::flow::BuyImbalance) marks << "▲";
        if (r.flags & charts::flow::SellImbalance) marks << "▼";
        return marks.join(' ');
      }
      default: return {};
    }
  }
private:
  std::vector<charts::flow::FootprintRow> m_rows;
  charts::flow::FootprintView m_view;
  qint32 m_base = 0;
  qint64 m_vaLo = 0, m_vaHi = -1;
};

// ------------------------------------------------------------------------------------------------ bands table
// The book depth bands (stored snapshots, or the live book's seconds), newest first: the bids' share of the depth out
// to ±0.2 / ±1 / ±5 %, then each band in pairs (−0.2 | +0.2 … −5 | +5); the live book adds its mid and how far its
// REST snapshot reached.
class BandsModel : public QAbstractTableModel {
public:
  struct Row {
    qint64 tMs = 0;
    std::array<float, technicaldata::perp::kBands> d{};
    double mid = std::numeric_limits<double>::quiet_NaN();
    float coverBid = 0, coverAsk = 0;
  };
  explicit BandsModel(bool live) : m_live(live) {}
  void set(std::vector<Row> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
  }
  int rowCount(const QModelIndex& = {}) const override { return int(m_rows.size()); }
  int columnCount(const QModelIndex& = {}) const override { return 16 + (m_live ? 2 : 0); }
  QVariant headerData(int s, Qt::Orientation o, int role) const override {
    if (o != Qt::Horizontal) return {};
    if (role == Qt::ToolTipRole) {
      if (s >= 1 && s <= 3) return T("Bids' share of the depth on both sides out to that distance from the price (50 % = balanced)");
      if (s >= 4 && s < 16) return T("Cumulative depth from the price out to that distance (base asset): bids below (−), asks above (+)");
      if (s == 16) return T("Mid price");
      if (s == 17) return T("How far from the price the order book snapshot reached (below / above): bands further out count only "
                            "the levels updated since");
      return {};
    }
    if (role != Qt::DisplayRole) return {};
    if (s == 0) return T("time (UTC)");
    if (s <= 3) return T("bids %1").arg(QStringList{"±0.2 %", "±1 %", "±5 %"}[s - 1]);
    if (s < 16) {
      const int k = (s - 4) / 2, side = (s - 4) % 2;   // k: 0.2, 1, 2, 3, 4, 5
      const double pct = technicaldata::perp::bandPercents()[size_t(6 + k)];
      return QString("%1%2 %").arg(side ? "+" : "−").arg(n(pct, pct < 1 ? 1 : 0));
    }
    return s == 16 ? T("mid") : T("snapshot reach");
  }
  QVariant data(const QModelIndex& i, int role) const override {
    if (i.row() < 0 || i.row() >= int(m_rows.size())) return {};
    const Row& r = m_rows[m_rows.size() - 1 - size_t(i.row())];
    const auto& c = theme::pal();
    const int col = i.column();
    auto band = [&](int k, int side) { return r.d[size_t(side ? 6 + k : 5 - k)]; };   // k 0 … 5 = ±0.2 … ±5
    auto share = [&](int k) {
      const double b = band(k, 0), a = band(k, 1);
      return std::isfinite(b) && std::isfinite(a) && a + b > 0 ? b / (a + b) : std::numeric_limits<double>::quiet_NaN();
    };
    const int shareK[3] = {0, 1, 5};
    if (role == Qt::TextAlignmentRole) return int(col == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole) {
      if (col == 0) return c.textDim;
      if (col <= 3) { const double v = share(shareK[col - 1]); return !std::isfinite(v) ? c.textMute : v >= 0.5 ? c.up : c.down; }
      if (col < 16) {
        const int k = (col - 4) / 2;
        const double reach = (col - 4) % 2 ? r.coverAsk : r.coverBid;
        // live: a band past the snapshot's reach may be short of the real depth
        if (m_live && reach > 0 && technicaldata::perp::bandPercents()[size_t(6 + k)] > reach + 1e-9) return c.textMute;
        return (col - 4) % 2 ? c.down : c.up;
      }
      return c.textDim;
    }
    if (role != Qt::DisplayRole) return {};
    if (col == 0) return QDateTime::fromMSecsSinceEpoch(r.tMs, QTimeZone::UTC).toString("HH:mm:ss");
    if (col <= 3) { const double v = share(shareK[col - 1]); return std::isfinite(v) ? n(v * 100, 1) + " %" : QStringLiteral("—"); }
    if (col < 16) { const double v = band((col - 4) / 2, (col - 4) % 2); return std::isfinite(v) ? n(v, 1) : QStringLiteral("—"); }
    if (col == 16) return n(r.mid, 2);
    return QString("−%1 / +%2 %").arg(n(r.coverBid, 2), n(r.coverAsk, 2));
  }
private:
  bool m_live;
  std::vector<Row> m_rows;   // ascending; shown newest first
};

// ------------------------------------------------------------------------------------------------ open interest table
class OiModel : public QAbstractTableModel {
public:
  void set(std::vector<technicaldata::perp::OiRow> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
  }
  int rowCount(const QModelIndex& = {}) const override { return int(m_rows.size()); }
  int columnCount(const QModelIndex& = {}) const override { return 8; }
  QVariant headerData(int s, Qt::Orientation o, int role) const override {
    static const char* h[] = {QT_TRANSLATE_NOOP("data", "time (UTC)"), QT_TRANSLATE_NOOP("data", "open interest"),
                              QT_TRANSLATE_NOOP("data", "value (M$)"), QT_TRANSLATE_NOOP("data", "change"),
                              QT_TRANSLATE_NOOP("data", "top accounts L/S"), QT_TRANSLATE_NOOP("data", "top positions L/S"),
                              QT_TRANSLATE_NOOP("data", "accounts L/S"), QT_TRANSLATE_NOOP("data", "takers buy/sell")};
    if (o != Qt::Horizontal) return {};
    if (role == Qt::ToolTipRole) {
      switch (s) {
        case 1: return T("Contracts open on the perpetual, in the base asset");
        case 3: return T("Open interest added (+) or closed (−) since the row before");
        case 4: return T("Long / short ratio of the top 20 % traders by margin, counted by accounts");
        case 5: return T("Long / short ratio of the top 20 % traders by margin, counted by position size");
        case 6: return T("Long / short ratio of all accounts");
        case 7: return T("Volume bought by market orders / volume sold by market orders");
        default: return {};
      }
    }
    if (role != Qt::DisplayRole) return {};
    return QCoreApplication::translate("data", h[s]);
  }
  QVariant data(const QModelIndex& i, int role) const override {
    if (i.row() < 0 || i.row() >= int(m_rows.size())) return {};
    const size_t k = m_rows.size() - 1 - size_t(i.row());
    const auto& r = m_rows[k];
    const double change = k > 0 ? r.oi - m_rows[k - 1].oi : std::numeric_limits<double>::quiet_NaN();
    const auto& c = theme::pal();
    if (role == Qt::TextAlignmentRole) return int(i.column() == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole) {
      if (i.column() == 0) return c.textDim;
      if (i.column() == 3) return !std::isfinite(change) ? c.textMute : change >= 0 ? c.up : c.down;
      if (i.column() >= 4) {
        const double v = i.column() == 4 ? r.topAccounts : i.column() == 5 ? r.topPositions : i.column() == 6 ? r.accounts : r.takerRatio;
        return !std::isfinite(v) ? c.textMute : v >= 1 ? c.up : c.down;
      }
      return c.text;
    }
    if (role != Qt::DisplayRole) return {};
    switch (i.column()) {
      case 0: return QDateTime::fromSecsSinceEpoch(r.t, QTimeZone::UTC).toString("HH:mm");
      case 1: return n(r.oi, 1);
      case 2: return n(r.oiValue / 1e6, 1);
      case 3: return std::isfinite(change) ? signedN(change, 1) : QStringLiteral("—");
      case 4: return n(r.topAccounts, 3);
      case 5: return n(r.topPositions, 3);
      case 6: return n(r.accounts, 3);
      case 7: return n(r.takerRatio, 3);
      default: return {};
    }
  }
private:
  std::vector<technicaldata::perp::OiRow> m_rows;   // ascending; shown newest first
};

// ------------------------------------------------------------------------------------------------ panel
namespace {
QTableView* table(QWidget* parent) {
  const auto& c = theme::pal();
  auto* t = new QTableView(parent);
  t->verticalHeader()->hide();
  t->setShowGrid(false);
  t->setFocusPolicy(Qt::NoFocus);
  t->setFont(theme::mono(11));
  t->horizontalHeader()->setFont(theme::microLabel());
  t->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  t->verticalHeader()->setDefaultSectionSize(22);
  t->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  t->setStyleSheet(QString("QTableView { background: %1; border: none; color: %2; selection-background-color: %6; selection-color: %2; }"
                           "QHeaderView::section { background: %1; color: %3; border: none; border-bottom: 1px solid %4; padding: 4px 8px; }"
                           "QTableView::item { border-bottom: 1px solid %5; padding: 0 8px; }")
                       .arg(c.panel.name(), c.text.name(), c.textMute.name(), c.divider.name(),
                            theme::alpha(c.divider, 120).name(QColor::HexArgb), c.rowSelected.name(QColor::HexArgb)));
  return t;
}
QPushButton* navButton(const QString& text, QWidget* parent) {
  const auto& c = theme::pal();
  auto* b = new QPushButton(text, parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setFont(theme::mono(10, QFont::Bold));
  b->setFixedWidth(26);
  b->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 3px; padding: 2px 0; }"
                           "QPushButton:hover { color: %3; border-color: %4; } QPushButton:disabled { color: %5; border-color: %2; }")
                       .arg(c.textDim.name(), c.border.name(), c.text.name(), c.borderHi.name(), c.textMute.name()));
  return b;
}
}  // namespace

FootprintPanel::FootprintPanel(QWidget* parent) : ui::Panel(T("Order flow"), parent) {
  const auto& c = theme::pal();
  setInfo(T("What the pair records besides its bars, as technical-data.sqlite keeps it, one tab each. Footprint: buyers and "
            "sellers at every price level, minute by minute, from Binance's daily trade archives, on levels of one basis "
            "point (0.01 % of the price); pick a minute to see its levels (imbalances: ratio 3, as the Charts footprint). "
            "Book depth: the perpetual's order book out to ±0.2 … ±5 % of the price, every ~30 s. Open interest: the "
            "perpetual's open contracts and long/short ratios, every 5 minutes. Live book: the same bands from the order "
            "book kept in memory, every second, never on disk."));
  m_modeBar = new ui::PillBar({}, this);
  m_venueBar = new ui::PillBar({}, this);
  m_prev = navButton("‹", this);
  m_next = navButton("›", this);
  m_dayLabel = ui::label(QString(), theme::mono(11, QFont::Bold), c.text, this);
  m_prev->setToolTip(T("The day before"));
  m_next->setToolTip(T("The day after"));
  headerRight()->addWidget(m_modeBar);
  headerRight()->addSpacing(10);
  headerRight()->addWidget(m_venueBar);
  headerRight()->addSpacing(8);
  headerRight()->addWidget(m_prev);
  headerRight()->addWidget(m_dayLabel);
  headerRight()->addWidget(m_next);
  // phones: the venue pills and the day navigation on a row of their own (hidden, and empty, otherwise)
  m_navRow = new QWidget(this);
  m_navRowL = new QHBoxLayout(m_navRow);
  m_navRowL->setContentsMargins(0, 0, 0, 0);
  m_navRowL->setSpacing(6);
  m_navRowL->addStretch(1);
  m_navRow->hide();
  body()->addWidget(m_navRow);
  m_info = ui::label(QString(), theme::mono(10), c.textDim, this);
  m_info->setWordWrap(true);
  body()->addWidget(m_info);
  m_stack = new QStackedWidget(this);
  // footprint: minutes and the levels of the minute picked
  {
    auto* page = new QWidget(m_stack);
    auto* row = new QHBoxLayout(page);
    m_fpRow = row;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    m_minutes = table(page);
    m_minModel = new MinutesModel;
    m_minModel->setParent(this);
    m_minutes->setModel(m_minModel);
    m_minutes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_minutes->setSelectionMode(QAbstractItemView::SingleSelection);
    row->addWidget(m_minutes, 1);
    auto* lv = new QVBoxLayout;
    lv->setSpacing(4);
    m_levelsHead = ui::label(QString(), theme::mono(10, QFont::Bold), c.textDim, page);
    lv->addWidget(m_levelsHead);
    m_levels = table(page);
    m_lvModel = new LevelsModel;
    m_lvModel->setParent(this);
    m_levels->setModel(m_lvModel);
    m_levels->setSelectionMode(QAbstractItemView::NoSelection);
    m_levels->setFixedWidth(400);
    m_levels->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   // every column in the width
    m_levels->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);   // the price whole
    m_levels->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    lv->addWidget(m_levels, 1);
    row->addLayout(lv);
    m_stack->addWidget(page);
  }
  auto tableTab = [&](QTableView*& t, QAbstractTableModel* m) {
    t = table(m_stack);
    m->setParent(this);
    t->setModel(m);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    m_stack->addWidget(t);
  };
  m_depthModel = new BandsModel(false);
  tableTab(m_depthTable, m_depthModel);
  m_oiModel = new OiModel;
  tableTab(m_oiTable, m_oiModel);
  m_liveModel = new BandsModel(true);
  tableTab(m_liveTable, m_liveModel);
  body()->addWidget(m_stack, 1);
  m_liveTimer = new QTimer(this);
  m_liveTimer->setInterval(1000);   // the live book's bands come once a second
  connect(m_liveTimer, &QTimer::timeout, this, [this] { if (mode() == "live") loadLive(); });

  connect(m_minutes->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& i) { pickMinute(i.row()); });
  connect(m_modeBar, &ui::PillBar::changed, this, [this](int) { m_follow = true; m_day = 0; syncHeader(); load(); });
  connect(m_venueBar, &ui::PillBar::changed, this, [this](int) { m_follow = true; m_day = 0; load(); });
  connect(m_prev, &QPushButton::clicked, this, [this] {
    const int i = int(m_days.indexOf(m_day));
    if (i > 0) { m_day = m_days[i - 1]; m_follow = false; load(); }
  });
  connect(m_next, &QPushButton::clicked, this, [this] {
    const int i = int(m_days.indexOf(m_day));
    if (i >= 0 && i + 1 < m_days.size()) { m_day = m_days[i + 1]; m_follow = i + 2 == m_days.size(); load(); }
  });
  auto dirty = [this](const QString& market) {
    if (market == m_market) { m_dirty = true; if (isVisible()) refresh(); }
  };
  connect(&Store::instance(), &Store::footprintChanged, this, [dirty](const QString& m, const QString&, qint64) { dirty(m); });
  connect(&Store::instance(), &Store::archiveChanged, this, [dirty](const QString& m, const QString&, qint64) { dirty(m); });
  connect(&technicaldata::LiveBook::instance(), &technicaldata::LiveBook::changed, this, [this] { if (mode() == "live") load(); });
}

void FootprintPanel::setPhone(bool on) {
  if (on == m_phone) return;
  m_phone = on;
  for (QWidget* w : {static_cast<QWidget*>(m_venueBar), static_cast<QWidget*>(m_prev), static_cast<QWidget*>(m_dayLabel), static_cast<QWidget*>(m_next)}) {
    headerRight()->removeWidget(w);
    m_navRowL->removeWidget(w);
  }
  if (on) {
    // the tabs scroll sideways under the title when they are wider than the phone
    headerRight()->removeWidget(m_modeBar);
    m_modeScroll = new QScrollArea(this);
    m_modeScroll->setFrameShape(QFrame::NoFrame);
    m_modeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_modeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_modeScroll->setWidgetResizable(false);
    m_modeScroll->viewport()->setAutoFillBackground(false);
    m_modeScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_modeBar->setFontPx(13);
    m_modeScroll->setWidget(m_modeBar);
    m_modeBar->adjustSize();
    m_modeScroll->setFixedHeight(m_modeBar->sizeHint().height());
    body()->insertWidget(0, m_modeScroll);
    m_navRowL->insertWidget(0, m_venueBar);   // venue, stretch, ‹ day ›
    m_navRowL->addWidget(m_prev);
    m_navRowL->addWidget(m_dayLabel);
    m_navRowL->addWidget(m_next);
  } else {
    // back to the title row as built: tabs, 10, venues, 8, ‹ day ›
    m_modeScroll->takeWidget();
    body()->removeWidget(m_modeScroll);
    m_modeScroll->deleteLater();
    m_modeScroll = nullptr;
    m_modeBar->setFontPx(11);
    m_modeBar->setParent(this);
    headerRight()->insertWidget(0, m_modeBar);
    headerRight()->insertWidget(2, m_venueBar);
    headerRight()->addWidget(m_prev);
    headerRight()->addWidget(m_dayLabel);
    headerRight()->addWidget(m_next);
    m_modeBar->show();
  }
  m_navRow->setVisible(on);
  m_venueBar->setFontPx(on ? 13 : 11);
  // day arrows a finger can hit
  for (auto* b : {m_prev, m_next}) { b->setFixedWidth(on ? 36 : 26); b->setMinimumHeight(on ? 34 : 0); }
  // the levels of the minute picked under the minutes, as wide as them
  m_fpRow->setDirection(on ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
  m_fpRow->setStretch(1, on ? 1 : 0);
  if (on) { m_levels->setMinimumWidth(0); m_levels->setMaximumWidth(QWIDGETSIZE_MAX); }
  else m_levels->setFixedWidth(400);
}

QString FootprintPanel::mode() const { return m_modes.value(m_modeBar->current()); }
QString FootprintPanel::venue() const { return m_venues.value(m_venueBar->current()); }

bool FootprintPanel::setMode(const QString& idAndVenue) {   // "live" or "live/perp"
  const int i = int(m_modes.indexOf(idAndVenue.section('/', 0, 0)));
  if (i < 0) return false;
  m_modeBar->setCurrent(i, true);
  const int v = int(m_venues.indexOf(idAndVenue.section('/', 1)));
  if (v >= 0) m_venueBar->setCurrent(v, true);
  return true;
}

void FootprintPanel::setMarket(const QString& market) {
  const PairSpec spec = specFromJson(Store::instance().pairSpec(market));
  const bool binance = market.startsWith("BINANCE:");
  QStringList modes;
  if (binance && !spec.footprint.isEmpty()) modes << "fp";
  if (binance && spec.depth) modes << "depth";
  if (binance && spec.openInterest) modes << "oi";
  if (binance && spec.liveBook) modes << "live";
  const QStringList fpVenues = binance ? spec.footprint : QStringList();
  const bool same = market == m_market && modes == m_modes && fpVenues == m_fpVenues;
  m_market = market;
  setVisible(!modes.isEmpty());
  if (same) return;
  const QString was = mode();
  m_modes = modes;
  m_fpVenues = fpVenues;
  QStringList names;
  for (const QString& m : modes)
    names << (m == "fp" ? T("Footprint") : m == "depth" ? T("Book depth") : m == "oi" ? T("Open interest") : T("Live book"));
  m_modeBar->setItems(names);
  if (m_modeScroll) m_modeBar->adjustSize();   // phones: its sideways scroller does not follow its size
  const int i = int(modes.indexOf(was));
  m_modeBar->setCurrent(i >= 0 ? i : 0);
  m_follow = true;
  m_day = 0;
  m_dirty = true;
  syncHeader();
  if (isVisible()) load();
}

// the venue pills: the footprint's markets, or the live book's two; none for the perpetual's archives
void FootprintPanel::syncHeader() {
  const QString m = mode();
  const QString was = venue();
  m_venues = m == "fp" ? m_fpVenues : m == "live" ? QStringList{"spot", "perp"} : QStringList();
  QStringList names;
  for (const QString& v : m_venues) names << venueName(v);
  m_venueBar->setItems(names);
  m_venueBar->setVisible(!m_venues.isEmpty());
  const int i = int(m_venues.indexOf(was));
  m_venueBar->setCurrent(i >= 0 ? i : 0);
  const bool days = m != "live";
  m_prev->setVisible(days);
  m_next->setVisible(days);
  m_dayLabel->setVisible(days);
  m_stack->setCurrentIndex(m == "depth" ? 1 : m == "oi" ? 2 : m == "live" ? 3 : 0);
  if (m == "live" && isVisible()) m_liveTimer->start(); else m_liveTimer->stop();
}

void FootprintPanel::showEvent(QShowEvent* e) {
  ui::Panel::showEvent(e);
  if (mode() == "live") m_liveTimer->start();
  if (m_dirty) load();
}

void FootprintPanel::hideEvent(QHideEvent* e) {
  m_liveTimer->stop();
  ui::Panel::hideEvent(e);
}

void FootprintPanel::refresh() {
  if (!m_dirty) return;
  load();
}

void FootprintPanel::load() {
  m_dirty = false;
  const QString m = mode();
  if (m == "depth") loadDepth();
  else if (m == "oi") loadOi();
  else if (m == "live") loadLive();
  else loadFootprint();
}

namespace {
// the day navigation over the days stored; false when there is none (the panel says so)
bool pickDay(const QVector<qint64>& days, qint64& day, bool& follow, QPushButton* prev, QPushButton* next, QLabel* label) {
  if (days.isEmpty()) {
    day = 0;
    label->setText(QStringLiteral("—"));
    prev->setEnabled(false);
    next->setEnabled(false);
    return false;
  }
  if (follow || !days.contains(day)) { day = days.last(); follow = true; }
  const int i = int(days.indexOf(day));
  prev->setEnabled(i > 0);
  next->setEnabled(i + 1 < days.size());
  label->setText(dayText(QDateTime::fromSecsSinceEpoch(day, QTimeZone::UTC).date()));
  return true;
}
}  // namespace

void FootprintPanel::loadFootprint() {
  auto& st = Store::instance();
  m_days.clear();
  qint64 bytes = 0;
  for (const FpDay& d : st.footprintDays(m_market, venue())) { m_days << d.day; bytes += d.bytes; }
  if (!pickDay(m_days, m_day, m_follow, m_prev, m_next, m_dayLabel)) {
    m_minModel->set({});
    m_lvModel->set(nullptr);
    m_levelsHead->clear();
    m_info->setText(T("No day stored yet for this market: the collection downloads the newest days first."));
    return;
  }
  std::vector<fpa::Minute> minutes;
  fpa::decodeDay(st.footprintData(m_market, venue(), m_day), m_day, minutes);
  double buy = 0, sell = 0;
  quint64 dayTrades = 0;
  for (const auto& m : minutes) { buy += m.buy; sell += m.sell; dayTrades += m.trades; }
  m_info->setText(T("%1 · %2 minutes · %3 trades · bought %4 · sold %5 · delta %6   —   %7 days stored (%8)")
                      .arg(venueName(venue()))
                      .arg(minutes.size())
                      .arg(QLocale(QLocale::English).toString(qulonglong(dayTrades)))
                      .arg(n(buy, 2), n(sell, 2), signedN(buy - sell, 2))
                      .arg(m_days.size())
                      .arg(mb(bytes)));
  const int keepRow = m_minutes->currentIndex().isValid() && !m_follow ? m_minutes->currentIndex().row() : 0;
  m_minModel->set(std::move(minutes));
  static bool sized = false;
  if (!sized && m_minModel->rowCount() > 0) { m_minutes->resizeColumnsToContents(); sized = true; }
  if (m_minModel->rowCount() > 0) m_minutes->setCurrentIndex(m_minModel->index(std::min(keepRow, m_minModel->rowCount() - 1), 0));
  else pickMinute(-1);
}

void FootprintPanel::loadDepth() {
  auto& st = Store::instance();
  m_days.clear();
  qint64 bytes = 0;
  for (const ArchDay& d : st.archiveDays(m_market, "depth")) { m_days << d.day; bytes += d.bytes; }
  if (!pickDay(m_days, m_day, m_follow, m_prev, m_next, m_dayLabel)) {
    m_depthModel->set({});
    m_info->setText(T("No day stored yet: the collection downloads the newest days first."));
    return;
  }
  std::vector<technicaldata::perp::DepthSample> s;
  technicaldata::perp::decodeDepth(st.depthData(m_market, m_day), m_day, s);
  std::vector<BandsModel::Row> rows;
  rows.reserve(s.size());
  for (const auto& x : s) rows.push_back(BandsModel::Row{x.t * 1000, x.depth});
  m_info->setText(T("Perpetual · %1 snapshots · cumulative depth (base asset) from the price out to each distance   —   %2 days stored (%3)")
                      .arg(s.size())
                      .arg(m_days.size())
                      .arg(mb(bytes)));
  m_depthModel->set(std::move(rows));
  static bool sized = false;
  if (!sized && m_depthModel->rowCount() > 0) { m_depthTable->resizeColumnsToContents(); sized = true; }
}

void FootprintPanel::loadOi() {
  auto& st = Store::instance();
  m_days.clear();
  for (const ArchDay& d : st.archiveDays(m_market, "oi")) m_days << d.day;
  if (!pickDay(m_days, m_day, m_follow, m_prev, m_next, m_dayLabel)) {
    m_oiModel->set({});
    m_info->setText(T("No day stored yet: the collection downloads the newest days first."));
    return;
  }
  // the row before the day too, so its first change is known
  auto rows = st.openInterest(m_market, m_day - 300, m_day + 86400);
  m_info->setText(T("Perpetual · open interest and long/short ratios every 5 minutes   —   %1 days stored").arg(m_days.size()));
  m_oiModel->set(std::move(rows));
  static bool sized = false;
  if (!sized && m_oiModel->rowCount() > 0) { m_oiTable->resizeColumnsToContents(); sized = true; }
}

void FootprintPanel::loadLive() {
  const auto s = technicaldata::LiveBook::instance().stream(m_market, venue());
  if (!s) {
    m_liveModel->set({});
    m_info->setText(technicaldata::LiveBook::instance().started()
                        ? T("The live book of this market is not kept now (the pair is paused).")
                        : T("The live book starts with the collection, 20 s after Deepwater opens (never in a test profile)."));
    return;
  }
  const auto& st = s->status();
  const auto& bands = s->bands();
  // the last 10 minutes on screen (all 24 h stay in memory)
  std::vector<BandsModel::Row> rows;
  const size_t from = bands.size() > 600 ? bands.size() - 600 : 0;
  rows.reserve(bands.size() - from);
  for (size_t k = from; k < bands.size(); ++k) {
    const auto& b = bands[k];
    BandsModel::Row r{b.timeMs, {}, b.mid, b.coverBid, b.coverAsk};
    std::copy(b.depth.begin(), b.depth.end(), r.d.begin());
    rows.push_back(r);
  }
  const auto& c = theme::pal();
  const QString state = st.synced ? T("synced") : st.feed == charts::data::FeedState::Open ? T("syncing") : T("connecting");
  m_info->setText(T("%1 · %2 · %3 levels in the book · bid %4 / ask %5 · %6 seconds of bands in memory (24 h kept), never on disk")
                      .arg(venueName(venue()), state)
                      .arg(QLocale(QLocale::English).toString(st.bookLevels))
                      .arg(st.bestBid && s->ready() ? n(s->ticks().priceOf(st.bestBid), 2) : QStringLiteral("—"))
                      .arg(st.bestAsk && s->ready() ? n(s->ticks().priceOf(st.bestAsk), 2) : QStringLiteral("—"))
                      .arg(QLocale(QLocale::English).toString(qulonglong(bands.size()))));
  m_info->setStyleSheet(QString("color:%1;background:transparent;").arg((st.synced ? c.textDim : c.warn).name()));
  const int scroll = m_liveTable->verticalScrollBar()->value();
  m_liveModel->set(std::move(rows));
  m_liveTable->verticalScrollBar()->setValue(scroll);
  static bool sized = false;
  if (!sized && m_liveModel->rowCount() > 0) { m_liveTable->resizeColumnsToContents(); sized = true; }
}

void FootprintPanel::pickMinute(int row) {
  const fpa::Minute* m = m_minModel->at(row);
  m_lvModel->set(m);
  if (!m) { m_levelsHead->clear(); return; }
  m_levelsHead->setText(T("LEVELS AT %1 · %2 bp from %3 to %4")
                            .arg(QDateTime::fromSecsSinceEpoch(m->t, QTimeZone::UTC).toString("HH:mm"))
                            .arg(m->levels.size())
                            .arg(n(m->low, 2), n(m->high, 2)));
}

QString FootprintPanel::dumpText() const {
  QStringList out;
  out << "order flow panel: visible=" + QString::number(isVisible()) + " tabs=" + m_modes.join(",") + " tab=" + mode() + " venue=" + venue() +
             " day=" + m_dayLabel->text();
  out << "info: " + m_info->text();
  const QAbstractTableModel* model = mode() == "depth" ? static_cast<const QAbstractTableModel*>(m_depthModel)
                                     : mode() == "oi"  ? static_cast<const QAbstractTableModel*>(m_oiModel)
                                     : mode() == "live" ? static_cast<const QAbstractTableModel*>(m_liveModel)
                                                        : static_cast<const QAbstractTableModel*>(m_minModel);
  if (mode() == "fp") out << "levels: " + m_levelsHead->text();
  QStringList head;
  for (int col = 0; col < model->columnCount(); ++col) head << model->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
  out << "columns: " + head.join(" | ");
  out << "rows: " + QString::number(model->rowCount());
  for (int r = 0; r < std::min(3, model->rowCount()); ++r) {
    QStringList cells;
    for (int col = 0; col < model->columnCount(); ++col) cells << model->data(model->index(r, col), Qt::DisplayRole).toString();
    out << "row: " + cells.join(" | ");
  }
  return out.join("\n");
}

}  // namespace technicaldatapage
