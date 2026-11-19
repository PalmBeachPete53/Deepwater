#include "charts/core/HeatmapStore.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace charts::flow {

namespace {
constexpr double kQMin = 1e-8;
constexpr int kStepsPerDoubling = 512;
constexpr char kMagic[8] = {'D', 'W', 'H', 'E', 'A', 'T', 'M', '1'};
constexpr uint16_t kVersion = 1;
constexpr int kHeader = 64;
constexpr int64_t kHourMs = 3600000;

void put64(unsigned char* p, int64_t v) { for (int k = 0; k < 8; ++k) p[k] = (unsigned char)(uint64_t(v) >> (8 * k)); }
void put32(unsigned char* p, uint32_t v) { for (int k = 0; k < 4; ++k) p[k] = (unsigned char)(v >> (8 * k)); }
void put16(unsigned char* p, uint16_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
int64_t get64(const unsigned char* p) { uint64_t v = 0; for (int k = 7; k >= 0; --k) v = (v << 8) | p[k]; return int64_t(v); }
uint32_t get32(const unsigned char* p) { uint32_t v = 0; for (int k = 3; k >= 0; --k) v = (v << 8) | p[k]; return v; }
uint16_t get16(const unsigned char* p) { return uint16_t(p[0] | (p[1] << 8)); }

int64_t hourOf(int64_t ms) { return (ms >= 0 ? ms / kHourMs : -((-ms + kHourMs - 1) / kHourMs)) * kHourMs; }

void header(unsigned char* h, const HeatmapFileInfo& info) {
  std::memset(h, 0, kHeader);
  std::memcpy(h, kMagic, 8);
  put16(h + 8, kVersion);
  put16(h + 10, uint16_t(DepthRecord::kRows));
  put32(h + 12, uint32_t(DepthRecord::kBytes));
  put32(h + 16, info.intervalMs);
  put64(h + 20, info.tick.mant);
  h[28] = (unsigned char)(int8_t(info.tick.exp));
  h[29] = info.venue;
  const QByteArray s = info.symbol.toLatin1().left(16);
  std::memcpy(h + 30, s.constData(), size_t(s.size()));
}
}  // namespace

// ------------------------------------------------------------------------------------------------ codes

uint16_t encodeQty(double q) {
  if (!(q > 0)) return 0;
  const double c = 1 + std::round(kStepsPerDoubling * std::log2(q / kQMin));
  return uint16_t(std::clamp(c, 1.0, 65535.0));
}

double decodeQty(uint16_t code) { return code ? kQMin * std::exp2(double(code - 1) / kStepsPerDoubling) : 0.0; }

DepthRecord DepthRecord::from(const DepthColumn& c) {
  DepthRecord r;
  r.timeMs = c.timeMs;
  r.baseTicks = c.baseTicks;
  r.bestBid = c.bestBid;
  r.bestAsk = c.bestAsk;
  r.rowTicks = c.rowTicks;
  r.flags = c.flags;
  for (int i = 0; i < kRows; ++i) r.codes[i] = encodeQty(c.qty[i]);
  return r;
}

void DepthRecord::toBytes(unsigned char* o) const {
  put64(o, timeMs);
  put64(o + 8, baseTicks);
  put64(o + 16, bestBid);
  put64(o + 24, bestAsk);
  put32(o + 32, uint32_t(rowTicks));
  o[36] = flags;
  o[37] = o[38] = o[39] = 0;
  for (int i = 0; i < kRows; ++i) put16(o + 40 + 2 * i, codes[i]);
}

DepthRecord DepthRecord::fromBytes(const unsigned char* in) {
  DepthRecord r;
  r.timeMs = get64(in);
  r.baseTicks = get64(in + 8);
  r.bestBid = get64(in + 16);
  r.bestAsk = get64(in + 24);
  r.rowTicks = int32_t(get32(in + 32));
  r.flags = in[36];
  for (int i = 0; i < kRows; ++i) r.codes[i] = get16(in + 40 + 2 * i);
  return r;
}

bool DepthRecord::operator==(const DepthRecord& o) const {
  return timeMs == o.timeMs && baseTicks == o.baseTicks && bestBid == o.bestBid && bestAsk == o.bestAsk && rowTicks == o.rowTicks &&
         flags == o.flags && std::memcmp(codes, o.codes, sizeof(codes)) == 0;
}

// ------------------------------------------------------------------------------------------------ writer

HeatmapWriter::HeatmapWriter(QString dir, HeatmapFileInfo info) : dir_(std::move(dir)), info_(std::move(info)) {}

HeatmapWriter::~HeatmapWriter() { flush(); }

bool HeatmapWriter::lock() {
  if (lock_) return true;
  QDir().mkpath(dir_);
  auto l = std::make_unique<QLockFile>(dir_ + "/.lock");
  l->setStaleLockTime(0);   // stale only when its process is gone, not after 30 s of a long session
  if (!l->tryLock(0)) {
    error_ = l->error() == QLockFile::LockFailedError ? QStringLiteral("recorded by another Deepwater on this data folder")
                                                       : QStringLiteral("cannot lock the recording folder");
    return false;
  }
  lock_ = std::move(l);
  return true;
}

void HeatmapWriter::dropFile() {
  file_.reset();   // QFile's close tries the buffer once more; the next open cuts what is left of a partial record
  hour_ = INT64_MIN;
}

QString HeatmapWriter::fileName(int64_t timeMs) {
  return QDateTime::fromMSecsSinceEpoch(hourOf(timeMs), QTimeZone::UTC).toString("yyyy-MM-ddTHH") + ".dwhm";
}

QString HeatmapWriter::currentFile() const { return file_ ? file_->fileName() : QString(); }

bool HeatmapWriter::openFor(int64_t hourStart) {
  if (file_) flush();
  dropFile();
  if (!lock()) return false;
  QDir().mkpath(dir_);
  const QString path = dir_ + "/" + fileName(hourStart);
  if (QFile::exists(path) && QFileInfo(path).size() >= kHeader) {
    // appending only to a file of the same layout, tick, interval and market: else its records and ours would be read
    // with one header (wrong prices after a tick-size change); it is kept, under another name of the same hour
    QFile old(path);
    unsigned char have[kHeader] = {}, want[kHeader];
    header(want, info_);
    const bool same = old.open(QIODevice::ReadOnly) && old.read(reinterpret_cast<char*>(have), kHeader) == kHeader &&
                      std::memcmp(have, want, kHeader) == 0;
    old.close();
    if (!same) {
      const QString stem = path.left(path.size() - 5);   // without ".dwhm"
      int n = 1;
      while (QFile::exists(stem + "-" + QString::number(n) + ".dwhm")) ++n;
      if (!QFile::rename(path, stem + "-" + QString::number(n) + ".dwhm")) {
        error_ = "cannot move aside " + path;
        return false;
      }
    }
  }
  auto f = std::make_unique<QFile>(path);
  const bool fresh = !f->exists() || f->size() < kHeader;
  if (!f->open(QIODevice::ReadWrite)) {
    error_ = f->errorString();
    return false;
  }
  if (fresh) {
    unsigned char h[kHeader];
    header(h, info_);
    f->resize(0);
    if (f->write(reinterpret_cast<const char*>(h), kHeader) != kHeader || !f->flush()) { error_ = f->errorString(); return false; }
  } else {
    // an existing file of this hour (a restart, or after a failed write): cut a partial last record, then append
    const qint64 whole = (f->size() - kHeader) / DepthRecord::kBytes;
    if (!f->resize(kHeader + whole * DepthRecord::kBytes)) { error_ = f->errorString(); return false; }
  }
  if (!f->seek(f->size())) { error_ = f->errorString(); return false; }
  file_ = std::move(f);
  hour_ = hourStart;
  return true;
}

bool HeatmapWriter::append(const DepthRecord& r) {
  const int64_t h = hourOf(r.timeMs);
  if (!file_ || h != hour_)
    if (!openFor(h)) return false;
  unsigned char buf[DepthRecord::kBytes];
  r.toBytes(buf);
  if (file_->write(reinterpret_cast<const char*>(buf), DepthRecord::kBytes) != DepthRecord::kBytes) {
    error_ = file_->errorString();
    dropFile();   // what was half written is cut when the file is opened again
    return false;
  }
  return true;
}

bool HeatmapWriter::flush() {
  if (!file_) return true;
  if (file_->flush()) return true;
  error_ = file_->errorString();   // disk full: part of the buffer may be on disk, cut at the next open
  dropFile();
  return false;
}

// ------------------------------------------------------------------------------------------------ reader

HeatmapReader::~HeatmapReader() {
  if (file_ && map_) file_->unmap(const_cast<unsigned char*>(map_));
}

bool HeatmapReader::open(const QString& path) {
  file_ = std::make_unique<QFile>(path);
  if (!file_->open(QIODevice::ReadOnly)) { error_ = file_->errorString(); return false; }
  const qint64 size = file_->size();
  if (size < kHeader) { error_ = "not a heatmap file (too short)"; return false; }
  map_ = file_->map(0, size);
  if (!map_) { error_ = "cannot map the file"; return false; }
  if (std::memcmp(map_, kMagic, 8) != 0) { error_ = "not a heatmap file"; return false; }
  if (get16(map_ + 8) != kVersion) { error_ = QString("heatmap file version %1 (this Deepwater reads %2)").arg(get16(map_ + 8)).arg(kVersion); return false; }
  if (get16(map_ + 10) != DepthRecord::kRows || get32(map_ + 12) != uint32_t(DepthRecord::kBytes)) { error_ = "heatmap file of another layout"; return false; }
  info_.intervalMs = get32(map_ + 16);
  info_.tick = Decimal{get64(map_ + 20), int8_t(map_[28]), true};
  info_.venue = map_[29];
  info_.symbol = QString::fromLatin1(reinterpret_cast<const char*>(map_ + 30), int(strnlen(reinterpret_cast<const char*>(map_ + 30), 16)));
  count_ = (size - kHeader) / DepthRecord::kBytes;
  return true;
}

DepthRecord HeatmapReader::record(int64_t i) const { return DepthRecord::fromBytes(map_ + kHeader + i * DepthRecord::kBytes); }

// ------------------------------------------------------------------------------------------------ folders

std::vector<DepthRecord> loadRange(const QString& dir, int64_t fromMs, int64_t toMs, const Decimal& tick) {
  std::vector<DepthRecord> out;
  // the files that exist, in time order (their names sort by time), whose hour overlaps the range — never a walk over
  // every hour of the range (a wide range would take forever)
  const QString first = HeatmapWriter::fileName(std::max<int64_t>(fromMs, 0));
  for (const QString& f : QDir(dir).entryList({"*.dwhm"}, QDir::Files, QDir::Name)) {
    if (f.left(13) < first.left(13)) continue;   // by the hour stamp: "<hour>-1.dwhm" (moved aside) sorts before "<hour>.dwhm"
    const QDateTime t = QDateTime::fromString(f.left(13), "yyyy-MM-ddTHH");
    if (!t.isValid()) continue;
    if (QDateTime(t.date(), t.time(), QTimeZone::UTC).toMSecsSinceEpoch() >= toMs) break;
    const QString path = dir + "/" + f;
    HeatmapReader rd;
    if (!rd.open(path)) continue;
    if (tick.ok && (rd.info().tick.mant != tick.mant || rd.info().tick.exp != tick.exp)) continue;   // another tick's records
    for (int64_t i = 0; i < rd.count(); ++i) {
      DepthRecord r = rd.record(i);
      if (r.timeMs >= fromMs && r.timeMs < toMs && (out.empty() || r.timeMs > out.back().timeMs)) out.push_back(r);
    }
  }
  return out;
}

int pruneOlderThan(const QString& dir, int keepDays, int64_t nowMs) {
  int n = 0;
  const int64_t cut = nowMs - int64_t(keepDays) * 24 * kHourMs;
  for (const QString& f : QDir(dir).entryList({"*.dwhm"}, QDir::Files)) {
    const QDateTime t = QDateTime::fromString(f.left(13), "yyyy-MM-ddTHH");
    if (!t.isValid()) continue;
    QDateTime utc(t.date(), t.time(), QTimeZone::UTC);
    if (utc.toMSecsSinceEpoch() + kHourMs <= cut && QFile::remove(dir + "/" + f)) ++n;
  }
  return n;
}

}  // namespace charts::flow
