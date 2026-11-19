#pragma once
// Order flow: storing heatmap columns in memory and on disk (step D3). QtCore only (QFile), no threads of its own.
//
//   DepthRecord   a column as stored: the column's header and one 16-bit code per row. Code 0 = nothing resting;
//                 code c ≥ 1 = 1e-8 × 2^((c − 1) / 512) (512 steps per doubling, 0.068 % at most from the true
//                 quantity, 128 doublings above 1e-8 — invisible on a heatmap, half the size of floats). 1064 bytes.
//   ColumnRing<T> a ring of preallocated slots with absolute indices (index i stays the same column until it is
//                 overwritten), for the columns in memory.
//   HeatmapWriter hourly files of fixed-size records: <dir>/<yyyy-MM-ddTHH>.dwhm (UTC hour of the column), each with a
//                 versioned header; appends, flushes, starts the next file at the next hour. A crash can leave a cut
//                 last record: readers ignore it.
//   HeatmapReader one file, mapped read-only (QFile::map): the header checked, record i read by its offset.
//   loadRange     the records of a time range across the hourly files of a folder.
//
// File layout (little-endian, explicit field by field):
//   header 64 bytes: "DWHEATM1" | u16 version (1) | u16 rows (512) | u32 recordSize (1064) | u32 intervalMs |
//                    i64 tick mantissa | i8 tick exponent | u8 venue (0 spot, 1 USD-M) | char[16] symbol | zero padding
//   record 1064 bytes: i64 timeMs | i64 baseTicks | i64 bestBid | i64 bestAsk | i32 rowTicks | u8 flags | 3 × u8 0 |
//                      512 × u16 codes
#include "charts/core/Heatmap.h"
#include "charts/core/Ticks.h"
#include <QString>
#include <cstdint>
#include <memory>
#include <vector>

class QFile;
class QLockFile;

namespace charts::flow {

uint16_t encodeQty(double q);
double decodeQty(uint16_t code);

struct DepthRecord {
  static constexpr int kRows = DepthColumn::kRows;
  static constexpr int kBytes = 4 * 8 + 4 + 4 + kRows * 2;   // 1064
  int64_t timeMs = 0, baseTicks = 0, bestBid = 0, bestAsk = 0;
  int32_t rowTicks = 1;
  uint8_t flags = 0;
  uint16_t codes[kRows] = {};
  static DepthRecord from(const DepthColumn& c);
  void toBytes(unsigned char* out) const;          // kBytes
  static DepthRecord fromBytes(const unsigned char* in);
  bool operator==(const DepthRecord& o) const;
};

template <class T>
class ColumnRing {
public:
  explicit ColumnRing(size_t capacity) : slots_(capacity) {}
  size_t capacity() const { return slots_.size(); }
  int64_t begin() const { return end_ - int64_t(size_); }   // oldest absolute index
  int64_t end() const { return end_; }                      // one past the newest
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T& push() {   // the next slot (overwrites the oldest when full)
    T& s = slots_[size_t(end_ % int64_t(slots_.size()))];
    ++end_;
    if (size_ < slots_.size()) ++size_;
    return s;
  }
  const T& at(int64_t i) const { return slots_[size_t(i % int64_t(slots_.size()))]; }   // begin() ≤ i < end()
  const T& back() const { return at(end_ - 1); }
  void clear() { end_ = 0; size_ = 0; }
  // first absolute index whose timeMs ≥ t (end() if none); times ascend
  int64_t lowerBoundTime(int64_t t) const {
    int64_t lo = begin(), hi = end_;
    while (lo < hi) {
      const int64_t mid = lo + (hi - lo) / 2;
      if (at(mid).timeMs < t) lo = mid + 1;
      else hi = mid;
    }
    return lo;
  }

private:
  std::vector<T> slots_;
  int64_t end_ = 0;
  size_t size_ = 0;
};

struct HeatmapFileInfo {
  QString symbol;
  uint8_t venue = 0;
  Decimal tick;
  uint32_t intervalMs = 1000;
};

// One writer per market folder across processes: it holds <dir>/.lock (QLockFile) while it writes, so a second
// Deepwater on the same data folder records nothing instead of interleaving its records with ours (audit D13).
// A write or flush that fails (disk full) closes the file; the next append reopens it and cuts the partial record, so
// the hour stays aligned on whole records. An existing file of the hour whose header is not this writer's (another
// tick size, interval, layout, or not a heatmap file) is moved aside as <hour>-<n>.dwhm, never appended to.
class HeatmapWriter {
public:
  explicit HeatmapWriter(QString dir, HeatmapFileInfo info);
  ~HeatmapWriter();
  bool append(const DepthRecord& r);   // false: the file could not be written (error() says why)
  bool flush();                        // false: the buffered records could not be written (error())
  QString error() const { return error_; }
  QString currentFile() const;
  static QString fileName(int64_t timeMs);   // "2026-11-18T16.dwhm" (UTC hour)

private:
  bool openFor(int64_t hourStartMs);
  bool lock();
  void dropFile();
  QString dir_;
  HeatmapFileInfo info_;
  std::unique_ptr<QFile> file_;
  std::unique_ptr<QLockFile> lock_;
  int64_t hour_ = INT64_MIN;
  QString error_;
};

class HeatmapReader {
public:
  bool open(const QString& path);   // false: missing, not a heatmap file, other version or layout (error())
  const HeatmapFileInfo& info() const { return info_; }
  int64_t count() const { return count_; }   // whole records (a cut last one is left out)
  DepthRecord record(int64_t i) const;
  QString error() const { return error_; }
  ~HeatmapReader();

private:
  std::unique_ptr<QFile> file_;
  const unsigned char* map_ = nullptr;
  int64_t count_ = 0;
  HeatmapFileInfo info_;
  QString error_;
};

// The records with fromMs ≤ time < toMs found in the hourly files of dir, in time order. With a valid tick, files
// written with another tick size are left out: their records are in that file's ticks, and read with today's tick
// they would draw at wrong prices (a tick-size change by the exchange).
std::vector<DepthRecord> loadRange(const QString& dir, int64_t fromMs, int64_t toMs, const Decimal& tick = {});
// Deletes the hourly files older than keepDays (by their name). Returns how many.
int pruneOlderThan(const QString& dir, int keepDays, int64_t nowMs);

}  // namespace charts::flow
