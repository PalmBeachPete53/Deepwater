// lib/store.js: <name>.json in the data folder, JSON.stringify(v, null, 2), atomic write, /api/state merge.
#include "Check.h"
#include "charts/data/Store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

using namespace charts;

namespace {
QString fixtures() { return QFileInfo(QStringLiteral(__FILE__)).absolutePath() + "/fixtures"; }
QByteArray slurp(const QString& path) { QFile f(path); (void)f.open(QIODevice::ReadOnly); return f.readAll(); }
}  // namespace

TEST("store: the data folder is inside DW_PROFILE, never the user's") {
  CHECK(store::dataDir().startsWith(qEnvironmentVariable("DW_PROFILE")));
  CHECK(store::dataDir().endsWith("/data/charts"));
}

TEST("store: write then read gives the same value back; a missing or broken file gives the fallback") {
  const QJsonObject v{{"watchlists", QJsonObject{{"Main", QJsonArray{"BINANCE:BTCUSDT", "US:AAPL"}}}}, {"theme", "dark"}, {"n", 1.5}};
  CHECK(store::write("roundtrip", v));
  CHECK_EQ(store::read("roundtrip", {}), QJsonValue(v));
  CHECK_EQ(store::read("missing", QJsonArray{1}), QJsonValue(QJsonArray{1}));
  QFile f(store::dataDir() + "/broken.json");
  (void)f.open(QIODevice::WriteOnly);
  f.write("{\"a\": ");
  f.close();
  CHECK_EQ(store::read("broken", "fb"), QJsonValue("fb"));
  // JSON.parse takes any value at the top level
  CHECK(store::write("scalar", 42));
  CHECK_EQ(store::read("scalar", {}), QJsonValue(42));
  CHECK(store::write("str", "x"));
  CHECK_EQ(store::read("str", {}), QJsonValue("x"));
  QFile g(store::dataDir() + "/two.json");
  (void)g.open(QIODevice::WriteOnly);
  g.write("1, 2");
  g.close();
  CHECK_EQ(store::read("two", "fb"), QJsonValue("fb"));
  // no temp file left behind
  CHECK(QDir(store::dataDir()).entryList({"*.tmp"}).isEmpty());
}

TEST("store: the file is JSON.stringify(value, null, 2), byte for byte") {
  // expected text written by node: JSON.stringify(sample, null, 2) (tests/charts/data/fixtures)
  const QJsonObject sample{
      {"a", QJsonObject{{"arr", QJsonArray()}, {"empty", QJsonObject()},
                        {"nested", QJsonObject{{"x", QString::fromUtf8("q\"uote\\ \n\t\x01 é 😀  ")}}}}},
      {"b", QJsonArray{1, 2.5, -0.0, 1e21, 1.5e-7, 0.1, 123456789012.0, 1e-7, 123e-20, -42.75}},
      {"c", QJsonValue::Null}, {"d", true}, {"e", ""}};
  CHECK(store::write("sample", sample));
  CHECK_EQ(slurp(store::dataDir() + "/sample.json"), slurp(fixtures() + "/store-sample.expected.json"));
  CHECK_EQ(store::stringify(QJsonArray{1, "a", QJsonObject{{"k", 2}}}, 0), QString("[1,\"a\",{\"k\":2}]"));
  CHECK_EQ(store::numberToString(100), QString("100"));
  CHECK_EQ(store::numberToString(1e20), QString("100000000000000000000"));
  CHECK_EQ(store::numberToString(1e-6), QString("0.000001"));
  CHECK_EQ(store::numberToString(-1234.5678), QString("-1234.5678"));
  CHECK_EQ(store::numberToString(5e-324), QString("5e-324"));
}

TEST("store: /api/state PUT is a shallow merge {...old, ...patch}") {
  CHECK(store::write("state", QJsonObject{{"a", 1}, {"layout", QJsonObject{{"x", 1}, {"y", 2}}}}));
  const QJsonObject next = store::mergeState({{"layout", QJsonObject{{"x", 5}}}, {"b", "two"}});
  const QJsonObject want{{"a", 1}, {"b", "two"}, {"layout", QJsonObject{{"x", 5}}}};   // layout replaced, not merged
  CHECK_EQ(next, want);
  CHECK_EQ(store::read("state", {}), QJsonValue(want));
  QFile::remove(store::dataDir() + "/state.json");
  CHECK_EQ(store::mergeState({{"z", 0}}), (QJsonObject{{"z", 0}}));
}

// Audit D22: a state.json that can't be parsed is never merged into (it was taken as {} and overwritten).
TEST("store: an unparsable state.json is copied aside, not merged into; the next save starts afresh") {
  const QString file = store::dataDir() + "/state.json";
  QDir(store::dataDir()).mkpath(".");
  for (const QString& f : QDir(store::dataDir()).entryList({"state.json.bad-*"})) QFile::remove(store::dataDir() + "/" + f);
  {
    QFile f(file);
    (void)f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write("{\"watchlists\": {\"Main\": [\"US:AAPL\"]}, oops");   // a hand edit with a typo
  }
  const QByteArray broken = slurp(file);
  QString err;
  store::mergeState(QJsonObject{{"theme", "light"}}, &err);
  CHECK(!err.isEmpty());
  const QStringList bad = QDir(store::dataDir()).entryList({"state.json.bad-*"});
  CHECK_EQ(int(bad.size()), 1);
  CHECK(slurp(store::dataDir() + "/" + bad.value(0)) == broken);   // the user's text is kept byte for byte
  CHECK(!QFile::exists(file));                                     // and nothing was written over it
  err.clear();
  const QJsonObject next = store::mergeState(QJsonObject{{"theme", "light"}}, &err);
  CHECK(err.isEmpty());
  CHECK_EQ(next.value("theme").toString(), QString("light"));
  CHECK_EQ(store::read("state", {}).toObject().value("theme").toString(), QString("light"));
  for (const QString& f : bad) QFile::remove(store::dataDir() + "/" + f);
  QFile::remove(file);
}

TEST("store: read tells a missing file from an unparsable one") {
  store::ReadState st = store::ReadState::Ok;
  store::read("never-written", {}, &st);
  CHECK(st == store::ReadState::Missing);
  QFile f(store::dataDir() + "/half.json");
  (void)f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write("[1, 2");
  f.close();
  store::read("half", {}, &st);
  CHECK(st == store::ReadState::Unparsable);
  CHECK(store::write("half", QJsonArray{1, 2}));
  store::read("half", {}, &st);
  CHECK(st == store::ReadState::Ok);
  QFile::remove(store::dataDir() + "/half.json");
}
