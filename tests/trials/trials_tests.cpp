#include "Check.h"
#include "charts/core/Quant.h"
#include "trials/Registry.h"
#include "trials/Stats.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>

using namespace trials;

namespace {
Trial bt(const QString& config, double sr, const QString& origin = "user", const QString& who = {}) {
  Trial t;
  t.at = 1763600000;
  t.kind = "backtest";
  t.origin = origin;
  t.who = who;
  t.family = backtestFamily("BINANCE:BTCUSDT", 3600);
  t.config = config;
  t.label = "rules: " + config;
  t.market = "BINANCE:BTCUSDT";
  t.timeframe = "1h";
  t.n = 500;
  t.sr = sr;
  return t;
}
}  // namespace

TEST("registry: a try written is read back, and can be neither changed nor removed") {
  QTemporaryDir dir;
  const QString path = dir.path() + "/trials.sqlite";
  Registry reg(path);
  CHECK(reg.ok());
  Trial t = bt("aaa", 0.05);
  t.daily = {0.01f, -0.02f, 0.0f, 0.125f};
  t.day0 = 20400;
  const qint64 id = reg.add(t);
  CHECK(id > 0);
  const auto rows = reg.family(t.family);
  CHECK_EQ(rows.size(), 1);
  CHECK_EQ(rows[0].config, QString("aaa"));
  CHECK_NEAR(rows[0].sr, 0.05, 1e-12);
  CHECK(rows[0].daily.isEmpty());   // family() leaves the daily returns out
  qint64 day0 = 0;
  const QVector<float> d = reg.daily(id, &day0);
  CHECK_EQ(day0, 20400);
  CHECK(d == t.daily);
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "raw");
    db.setDatabaseName(path);
    CHECK(db.open());
    QSqlQuery q(db);
    CHECK(!q.exec("UPDATE trials SET sr = 9 WHERE id = 1"));
    CHECK(!q.exec("DELETE FROM trials"));
    CHECK(q.exec("SELECT COUNT(*), MAX(sr) FROM trials") && q.next());
    CHECK_EQ(q.value(0).toInt(), 1);
    CHECK_NEAR(q.value(1).toDouble(), 0.05, 1e-12);
    db.close();
  }
  QSqlDatabase::removeDatabase("raw");
  // the registry opened again: the same row
  Registry again(path);
  CHECK_EQ(again.count(), 1);
}

TEST("registry: an ML run is one try however often it is offered") {
  QTemporaryDir dir;
  Registry reg(dir.path() + "/trials.sqlite");
  Trial t;
  t.kind = "ml";
  t.origin = "user";
  t.family = "ml|BTC/USDT · 1h\x1fsupervised · classification of direction close, 1 row(s) ahead";
  t.config = "cfg1";
  t.ref = "20261120-101010";
  const qint64 a = reg.add(t), b = reg.add(t);
  CHECK(a > 0);
  CHECK_EQ(a, b);
  CHECK_EQ(reg.count(), 1);
  CHECK(reg.hasRef("20261120-101010"));
  CHECK_EQ(reg.refs("ml"), QStringList{"20261120-101010"});
  const auto by = reg.configsByFamily("ml");
  CHECK_EQ(by.value(t.family).size(), 1);
}

TEST("registry: the latest row of each setup counts, in first-tried order") {
  QTemporaryDir dir;
  Registry reg(dir.path() + "/trials.sqlite");
  reg.add(bt("A", 0.01));
  reg.add(bt("B", 0.02, "agent", "environment 1 · Claude Code"));
  reg.add(bt("A", 0.03));
  const auto rows = reg.family(backtestFamily("BINANCE:BTCUSDT", 3600));
  CHECK_EQ(rows.size(), 3);
  const auto latest = latestPerConfig(rows);
  CHECK_EQ(latest.size(), 2);
  CHECK_EQ(latest[0].config, QString("A"));
  CHECK_NEAR(latest[0].sr, 0.03, 1e-12);
  CHECK_EQ(latest[1].config, QString("B"));
  const auto f = reg.families("backtest");
  CHECK_EQ(f.size(), 1);
  CHECK_EQ(f[0].configs, 2);
  CHECK_EQ(f[0].timeframe, QString("1h"));
}

TEST("registry: tries written from several threads at once are all kept") {
  QTemporaryDir dir;
  Registry reg(dir.path() + "/trials.sqlite");
  QList<QThread*> threads;
  for (int k = 0; k < 4; ++k)
    threads << QThread::create([&reg, k] {
      for (int i = 0; i < 25; ++i) reg.add(bt(QString("t%1-%2").arg(k).arg(i), 0.01 * i));
    });
  for (auto* t : threads) t->start();
  for (auto* t : threads) { t->wait(); delete t; }
  CHECK_EQ(reg.count(), 100);
  CHECK_EQ(latestPerConfig(reg.family(backtestFamily("BINANCE:BTCUSDT", 3600))).size(), 100);
}

TEST("stats: the per-bar Sharpe and the returns compounded per UTC day from an equity curve") {
  // hourly bars over three UTC days, from 2026-11-18 22:00: two bars on the first day, 24 on the next, one on the last
  QVector<charts::bt::EquityPoint> e;
  const qint64 t0 = 1763503200;   // 2026-11-18 22:00 UTC
  double v = 10000;
  QVector<double> rets;
  for (int i = 0; i < 27; ++i) {
    if (i) {
      const double r = 0.001 * ((i % 5) - 2);
      rets << r;
      v *= 1 + r;
    }
    e.push_back({t0 + i * 3600, v});
  }
  Trial t;
  setReturns(t, e);
  CHECK_EQ(t.n, 26);
  CHECK_NEAR(t.sr, charts::quant::sharpe(rets), 1e-12);
  CHECK_NEAR(t.sharpe, t.sr * std::sqrt(365.0 * 24), 1e-9);
  CHECK_EQ(t.from, t0);
  CHECK_EQ(t.to, t0 + 26 * 3600);
  CHECK_EQ(t.day0, t0 / 86400);
  CHECK_EQ(t.daily.size(), 3);
  // day 1: 22:00 → 23:00; day 2: 23:00 → the next day's 23:00; day 3: → 00:00
  CHECK_NEAR(t.daily[0], e[1].value / e[0].value - 1, 1e-6);
  CHECK_NEAR(t.daily[1], e[25].value / e[1].value - 1, 1e-6);
  CHECK_NEAR(t.daily[2], e[26].value / e[25].value - 1, 1e-6);
}

TEST("stats: the deflated Sharpe of the best try grows stricter with the number of tries") {
  // one try: deflated = probabilistic Sharpe against 0
  QList<Trial> one{bt("A", 0.08)};
  auto d1 = deflate(one);
  CHECK(d1.ok);
  CHECK_EQ(d1.tries, 1);
  CHECK_NEAR(d1.sr0, 0, 1e-12);
  CHECK_NEAR(d1.dsr, psrOf(0.08, 500, 0, 3, 0), 1e-12);
  // twenty tries of noise around it: the same best is worth less
  QList<Trial> many = one;
  for (int i = 0; i < 19; ++i) many << bt(QString("N%1").arg(i), 0.004 * (i - 9));
  const auto d20 = deflate(many);
  CHECK_EQ(d20.tries, 20);
  CHECK_EQ(d20.best.config, QString("A"));
  CHECK(d20.sr0 > 0);
  QVector<double> srs;
  for (const auto& t : many) srs << t.sr;
  const auto m = charts::quant::moments(srs);
  const double var = m.sd * m.sd * 20.0 / 19.0;
  CHECK_NEAR(d20.sr0, charts::quant::expectedMaxSharpe(20, var), 1e-12);
  CHECK_NEAR(d20.dsr, psrOf(0.08, 500, 0, 3, d20.sr0), 1e-12);
  CHECK(d20.dsr < d1.dsr);
  CHECK(!deflate({}).ok);
}

TEST("stats: who tried what, an environment matched whatever model it ran") {
  QList<Trial> rows{bt("A", 0.1), bt("B", 0.1, "agent", "environment 2 · Claude Code"), bt("B", 0.1, "agent", "environment 2 · opencode"),
                    bt("C", 0.1, "agent", "environment 3 · Claude Code"), bt("A", 0.1, "agent", "environment 3 · Claude Code")};
  const auto w = tried(rows, "environment 2 · ");
  CHECK_EQ(w.all, 3);
  CHECK_EQ(w.user, 1);
  CHECK_EQ(w.me, 1);
  CHECK_EQ(w.others, 2);
  const auto u = tried(rows, {});   // the user's view: every agent is "others"
  CHECK_EQ(u.others, 3);
}

TEST("stats: a setup's fingerprint ignores its name, not its rules") {
  QJsonObject a{{"entryLong", QJsonArray{QJsonObject{{"left", "close"}, {"op", ">"}, {"right", "e"}}}}, {"name", "mine"}};
  QJsonObject b = a;
  b["name"] = "other";
  CHECK_EQ(strategyConfig(a), strategyConfig(b));
  b["stopLossPct"] = 2;
  CHECK(strategyConfig(a) != strategyConfig(b));
  CHECK_EQ(strategyConfig(a).size(), 12);
}

namespace {
// daily returns: a steady edge plus noise (deterministic)
QVector<float> dailySeries(int days, double edge, quint32 seed) {
  QVector<float> d(days);
  quint32 x = seed;
  for (int i = 0; i < days; ++i) {
    x = x * 1664525u + 1013904223u;
    d[i] = float(edge + ((x >> 8) / double(1 << 24) - 0.5) * 0.02);
  }
  return d;
}
QVector<charts::bt::EquityPoint> equityOf(qint64 day0, const QVector<float>& d) {
  QVector<charts::bt::EquityPoint> e{{day0 * 86400, 10000}};
  double v = 10000;
  for (int i = 0; i < d.size(); ++i) e.push_back({(day0 + i) * 86400 + 86399, v *= 1 + d[i]});
  return e;
}
}  // namespace

TEST("pbo across tries: needs 3 strategies on 40 days, leaves out those tried on too few of them") {
  const qint64 d0 = 20000;
  const auto self = dailySeries(80, 0.0, 1);
  CHECK(!pboAcross(d0, self, {}).has_value());
  CHECK(!pboAcross(d0, dailySeries(30, 0, 1), {{d0, dailySeries(30, 0, 2)}, {d0, dailySeries(30, 0, 3)}}).has_value());
  // one covers only half the days: left out, so two strategies remain → nothing
  CHECK(!pboAcross(d0, self, {{d0, dailySeries(80, 0, 2)}, {d0 + 40, dailySeries(80, 0, 3)}}).has_value());
  const auto p = pboAcross(d0, self, {{d0, dailySeries(80, 0, 2)}, {d0 - 5, dailySeries(90, 0, 3)}, {d0, dailySeries(80, 0, 4)}});
  CHECK(p.has_value());
  CHECK_EQ(p->strategies, 4);
  CHECK_EQ(p->days, 80);
  CHECK_EQ(p->pbo.splits, 70);   // C(8, 4)
  // a strategy that really is better everywhere: picked in-sample, still above the median out of sample
  const auto good = pboAcross(d0, dailySeries(160, 0.004, 1), {{d0, dailySeries(160, 0, 2)}, {d0, dailySeries(160, 0, 3)}, {d0, dailySeries(160, 0, 4)}});
  CHECK(good.has_value());
  CHECK(good->pbo.pbo < 0.1);
}

TEST("quant checks with the registry: the deflated Sharpe counts the other strategies, a PBO gate across them") {
  QTemporaryDir dir;
  Registry reg(dir.path() + "/trials.sqlite");
  const qint64 d0 = 20000;
  const QString fam = backtestFamily("BINANCE:BTCUSDT", 86400);
  for (int k = 0; k < 4; ++k) {
    Trial t = bt(QString("other%1").arg(k), 0.0);
    t.family = fam;
    setReturns(t, equityOf(d0, dailySeries(120, 0.0005 * k, 10 + k)));
    reg.add(t);
  }
  Trial me = bt("me", 0);
  me.family = fam;
  const auto eq = equityOf(d0, dailySeries(120, 0.001, 99));
  setReturns(me, eq);
  reg.add(me);   // its own row is left out of the others
  charts::quant::Checks c;
  c.trialSharpes = {0.05, 0.02, -0.01};
  c.base.sharpe = me.sharpe;
  c.gates.push_back({"dsr", "Deflated Sharpe", "old", false});
  withRegistry(c, eq, fam, "me", reg);
  CHECK_EQ(c.trials, 7);
  CHECK(c.gates[0].value.contains("after 7 tries on this data: 3 nearby settings and 4 other strategies"));
  CHECK_EQ(c.gates.size(), 2);
  CHECK_EQ(c.gates[1].key, QString("pbo_tries"));
  CHECK(c.gates[1].value.contains("of the 5 strategies"));
  int passed = 0;
  for (const auto& g : c.gates) passed += g.pass;
  CHECK_EQ(c.passed, passed);
  // alone on its data: nothing changes
  charts::quant::Checks alone;
  alone.gates.push_back({"dsr", "Deflated Sharpe", "old", false});
  withRegistry(alone, eq, backtestFamily("BINANCE:ETHUSDT", 86400), "me", reg);
  CHECK_EQ(alone.gates.size(), 1);
  CHECK_EQ(alone.gates[0].value, QString("old"));
}

int main(int argc, char** argv) {
  QTemporaryDir profile;   // a throwaway profile: nothing touches the user's data
  qputenv("DW_PROFILE", profile.path().toUtf8());
  QCoreApplication app(argc, argv);
  return check::runAll(argc, argv);
}
