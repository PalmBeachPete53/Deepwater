#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

// Fills the Facts database from the services Deepwater already has, family by family:
//   rates       — policy rates of every economy (BIS, whole history, one fact per change) + the 8 followed banks'
//                 current rate, decisions (text facts) and market-implied path (forecast chain → rate chain)
//   macro       — every macro::Ind series of every economy, whole history (macro::MacroStore::deep())
//   calendar    — recorded calendar weeks: actuals, ForexFactory consensus, and revisions read from "previous"
//   prices      — daily open / high / low / close of every instrument (Yahoo, whole history once, then 3 months)
//   forecasts   — banks' published forecasts (ING) by quarter → the market's close chain
//   positioning — CFTC COT since 1986 (open interest and net positions by category), retail sentiment recordings,
//                 the risk index
// The first pass of a family imports its history (facts marked "import"); later passes (every 6 h while the app runs,
// and at once for each calendar actual) only add what is new or revised (facts marked "live"). Progress is shown on
// the Macro data page. The first pass starts when the Macro data page is first opened; after that, at launch — but a family whose
// last pass ended less than 6 h ago is left alone (meta "last.<family>"): relaunching does not download it again.
namespace macrodata {

class Importer : public QObject {
  Q_OBJECT
public:
  static Importer& instance();
  void start();                       // no-op while running; passes only the families due (first import, or last pass ≥ 6 h)
  bool running() const { return m_running; }
  QString status() const { return m_status; }
  int step() const { return m_step; }
  int steps() const { return m_steps; }
  static QStringList families();       // rates macro calendar prices forecasts positioning
  static QString familyName(const QString& f);   // translated

signals:
  void progress();
  void finished();

private:
  Importer();
  void next();
  void say(const QString& s);
  void done(const QString& family);
  void rates(std::function<void()> then);
  void macro(std::function<void()> then);
  void calendar(std::function<void()> then);
  void prices(std::function<void()> then);
  void forecasts(std::function<void()> then);
  void positioning(std::function<void()> then);
  bool deepDone(const QString& family) const;
  QDateTime lastPass(const QString& family) const;   // end of the family's last pass (invalid: never)
  QStringList due() const;                           // families to pass now
  void scheduleNext();                               // the timer fires when the next family is due
  static constexpr qint64 kPassMs = 6LL * 3600 * 1000;

  bool m_running = false;
  bool m_incomplete = false;           // the running family's source was not ready: its pass is not marked done
  QStringList m_queue;
  QString m_status;
  int m_step = 0, m_steps = 0;
  QTimer m_timer;
};
}  // namespace macrodata
