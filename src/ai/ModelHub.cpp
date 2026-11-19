#include "ai/ModelHub.h"
#include "net/Http.h"
#include "net/Sources.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStorageInfo>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCoreApplication>

namespace ai {

// the CDN hosts a download is redirected to (reached with net::manager() directly; seen 2026-11-16: us.aws.cdn.hf.co)
static const sources::Registrar kHub({"huggingface", "Hugging Face Hub", {"huggingface.co", "us.aws.cdn.hf.co",
  "eu.aws.cdn.hf.co", "cas-bridge.xethub.hf.co", "cdn-lfs.hf.co", "cdn-lfs-us-1.hf.co", "cdn-lfs-eu-1.hf.co",
  "cdn-lfs.huggingface.co", "cdn-lfs-us-1.huggingface.co"}, sources::Access::Keyless, 500,
  "Model search, model cards and files of local models (GGUF); downloads come from Hugging Face's CDN",
  "Public Hub API (robots.txt allows all). Each model has its own licence, shown before download; gated models need the "
  "user's token and their acceptance of the model's terms on huggingface.co",
  {"ai/models"}, "huggingface"});

QString HubFile::quant() const {
  static const QRegularExpression re(R"((?:^|[-_.])((?:IQ|Q)\d+(?:_[A-Z0-9]+)*|F16|F32|BF16)(?=[-_.]|\.gguf$))",
                                     QRegularExpression::CaseInsensitiveOption);
  const auto m = re.match(QFileInfo(path).fileName());
  return m.hasMatch() ? m.captured(1).toUpper() : QString();
}

ModelHub& ModelHub::instance() { static ModelHub* h = new ModelHub; return *h; }
ModelHub::ModelHub() { loadIndex(); }

QString ModelHub::token() const { return settings::apiKey("huggingface"); }
void ModelHub::setToken(const QString& t) {
  if (t.trimmed().isEmpty()) settings::remove("keys.huggingface"); else settings::setValue("keys.huggingface", t.trimmed());
}
QString ModelHub::modelsDir() const { return paths::data("models"); }
qint64 ModelHub::freeSpace() const { return QStorageInfo(modelsDir()).bytesAvailable(); }

static QString licenseOf(const QJsonObject& m) {
  const QString card = m["cardData"].toObject()["license"].toString();
  if (!card.isEmpty()) return card;
  for (const auto& t : m["tags"].toArray()) if (t.toString().startsWith("license:")) return t.toString().mid(8);
  return {};
}
static QString gatedOf(const QJsonValue& v) {
  if (v.isBool()) return v.toBool() ? QStringLiteral("auto") : QString();
  return v.toString();
}

void ModelHub::search(const QString& query, bool ggufOnly, Sort sort, QObject* ctx,
                      std::function<void(const QList<HubModel>&, const QString&)> cb) {
  QUrl u("https://huggingface.co/api/models");
  QUrlQuery q;
  if (!query.trimmed().isEmpty()) q.addQueryItem("search", query.trimmed());
  if (ggufOnly) q.addQueryItem("filter", "gguf");
  q.addQueryItem("sort", sort == Sort::Likes ? "likes" : sort == Sort::Updated ? "lastModified" : "downloads");
  q.addQueryItem("direction", "-1");
  q.addQueryItem("limit", "50");
  u.setQuery(q);
  net::Request r; r.url = u; r.ttlSec = 300;
  if (!token().isEmpty()) r.headers << qMakePair(QByteArray("Authorization"), "Bearer " + token().toUtf8());
  r.cacheKey = "hf search " + u.query();   // the token must not end up in the cache key
  net::get(r, ctx, [cb](const net::Reply& rep) {
    QList<HubModel> out;
    if (!rep.ok()) { cb(out, rep.error); return; }
    for (const auto& v : QJsonDocument::fromJson(rep.body).array()) {
      const auto o = v.toObject();
      HubModel m;
      m.id = o["id"].toString();
      m.downloads = qint64(o["downloads"].toDouble()); m.likes = qint64(o["likes"].toDouble());
      m.pipeline = o["pipeline_tag"].toString();
      m.license = licenseOf(o);
      m.gated = gatedOf(o["gated"]);
      m.lastModified = QDateTime::fromString(o["lastModified"].toString(), Qt::ISODateWithMs);
      if (!m.id.isEmpty()) out << m;
    }
    cb(out, {});
  });
}

void ModelHub::details(const QString& repo, QObject* ctx, std::function<void(const ModelDetails&)> cb) {
  auto result = std::make_shared<ModelDetails>();
  auto pending = std::make_shared<int>(2);
  auto done = [cb, result, pending] { if (--*pending == 0) cb(*result); };
  auto auth = [this](net::Request& r) {
    if (!token().isEmpty()) r.headers << qMakePair(QByteArray("Authorization"), "Bearer " + token().toUtf8());
  };
  net::Request info; info.url = QUrl("https://huggingface.co/api/models/" + repo); info.ttlSec = 600;
  info.cacheKey = "hf model " + repo; auth(info);
  net::get(info, ctx, [result, done, repo](const net::Reply& rep) {
    if (!rep.ok()) result->error = rep.error;
    const auto o = QJsonDocument::fromJson(rep.body).object();
    result->model.id = repo;
    result->model.license = licenseOf(o);
    result->model.gated = gatedOf(o["gated"]);
    result->model.pipeline = o["pipeline_tag"].toString();
    result->model.downloads = qint64(o["downloads"].toDouble()); result->model.likes = qint64(o["likes"].toDouble());
    result->model.lastModified = QDateTime::fromString(o["lastModified"].toString(), Qt::ISODateWithMs);
    done();
  });
  net::Request tree; tree.url = QUrl("https://huggingface.co/api/models/" + repo + "/tree/main?recursive=1"); tree.ttlSec = 600;
  tree.cacheKey = "hf tree " + repo; auth(tree);
  net::get(tree, ctx, [result, done](const net::Reply& rep) {
    if (!rep.ok() && result->error.isEmpty()) result->error = rep.error;
    for (const auto& v : QJsonDocument::fromJson(rep.body).array()) {
      const auto o = v.toObject();
      if (o["type"].toString() != "file") continue;
      HubFile f;
      f.path = o["path"].toString();
      f.size = qint64(o["size"].toDouble());
      const auto lfs = o["lfs"].toObject();
      if (!lfs.isEmpty()) { f.size = qint64(lfs["size"].toDouble()); f.sha256 = lfs["oid"].toString(); }
      result->files << f;
    }
    std::sort(result->files.begin(), result->files.end(), [](const HubFile& a, const HubFile& b) {
      if (a.isGguf() != b.isGguf()) return a.isGguf();
      return a.size < b.size;
    });
    done();
  });
}

// ---------------------------------------------------------------------------------------------- downloads
bool ModelHub::safeRelative(const QString& p) {
  if (p.isEmpty() || p.startsWith('/') || p.contains('\\') || p.contains(':') || p.size() > 512) return false;
  for (const QString& part : p.split('/')) {
    if (part.isEmpty() || part == "." || part == "..") return false;
    for (QChar ch : part) if (ch.category() == QChar::Other_Control) return false;
  }
  return true;
}
// repo and file come from the Hub's JSON: download() and loadIndex() refuse any that would leave modelsDir()
QString ModelHub::targetPath(const QString& repo, const QString& file) const {
  return modelsDir() + "/" + repo + "/" + file;
}
Download* ModelHub::find(const QString& key) {
  for (auto& d : m_queue) if (d.key() == key) return &d;
  return nullptr;
}
QList<Download> ModelHub::downloads() const { return m_queue; }

void ModelHub::download(const QString& repo, const HubFile& file, const QString& license) {
  Download d; d.repo = repo; d.file = file.path; d.total = file.size;
  if (find(d.key())) return;
  if (!safeRelative(repo) || !safeRelative(file.path)) {   // "../../.zshrc" from the API would be written outside the folder
    d.state = Download::State::Failed;
    d.error = QCoreApplication::translate("ai", "Refused: the Hub gave a file name that would be saved outside the models folder.");
    m_queue << d;
    emit downloadsChanged();
    return;
  }
  QFileInfo part(targetPath(repo, file.path) + ".part");
  d.received = part.exists() ? part.size() : 0;
  m_sha.insert(d.key(), file.sha256);
  m_license.insert(d.key(), license);
  m_queue << d;
  emit downloadsChanged();
  startNext();
}

void ModelHub::startNext() {
  if (m_reply) return;
  for (auto& d : m_queue) {
    if (d.state != Download::State::Queued) continue;
    if (sources::isHostDisabled("huggingface.co")) {
      d.state = Download::State::Failed; d.error = QCoreApplication::translate("ai", "Hugging Face is turned off in the Sources tab");
      emit downloadsChanged(); continue;
    }
    QUrl u("https://huggingface.co/" + d.repo + "/resolve/main/" + QString(QUrl::toPercentEncoding(d.file, "/")));
    d.redirects = 0;
    begin(d, u, true);
    return;
  }
}

void ModelHub::begin(Download& d, const QUrl& url, bool withToken) {
  const QString key = d.key();
  QDir().mkpath(QFileInfo(targetPath(d.repo, d.file)).absolutePath());
  QNetworkRequest nr(url);
  nr.setHeader(QNetworkRequest::UserAgentHeader, net::userAgent());
  nr.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  // the token only over https, and only to huggingface.co itself
  if (withToken && url.scheme() == "https" && url.host() == "huggingface.co" && !token().isEmpty())
    nr.setRawHeader("Authorization", "Bearer " + token().toUtf8());
  if (d.received > 0) nr.setRawHeader("Range", "bytes=" + QByteArray::number(d.received) + "-");
  d.state = Download::State::Running; d.error.clear();
  m_activeKey = key;
  m_reply = net::manager()->get(nr);
  m_rateClock.start(); m_rateBytes = 0;
  emit downloadsChanged();

  connect(m_reply, &QNetworkReply::metaDataChanged, this, [this, key] {
    QNetworkReply* r = m_reply;
    if (!r) return;
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400) return;                // handled in finished()
    Download* d = find(key);
    if (!d || m_out) return;
    if (status == 206 || status == 200) {
      if (status == 200) d->received = 0;                      // the server ignored Range: start over
      m_out = new QFile(targetPath(d->repo, d->file) + ".part");
      if (!m_out->open(status == 206 ? QIODevice::Append : QIODevice::WriteOnly)) {
        d->error = m_out->errorString(); delete m_out; m_out = nullptr; r->abort();
        return;
      }
      const qint64 len = r->header(QNetworkRequest::ContentLengthHeader).toLongLong();
      if (len > 0) d->total = d->received + len;
    }
  });
  connect(m_reply, &QNetworkReply::readyRead, this, [this, key] {
    QNetworkReply* r = m_reply;
    if (!r || !m_out) return;
    const QByteArray chunk = r->readAll();
    if (m_out->write(chunk) != chunk.size()) { if (Download* d = find(key)) d->error = m_out->errorString(); r->abort(); return; }
    if (Download* d = find(key)) {
      d->received += chunk.size();
      m_rateBytes += chunk.size();
      if (m_rateClock.elapsed() > 800) {
        const double rate = m_rateBytes * 1000.0 / m_rateClock.restart();
        d->bytesPerSec = d->bytesPerSec <= 0 ? rate : d->bytesPerSec * 0.6 + rate * 0.4;
        m_rateBytes = 0;
        emit downloadsChanged();
      }
    }
  });
  connect(m_reply, &QNetworkReply::finished, this, [this, key] {
    QNetworkReply* r = m_reply;
    if (!r) return;
    r->deleteLater();
    m_reply = nullptr;
    if (m_out) { m_out->close(); delete m_out; m_out = nullptr; }
    Download* d = find(key);
    if (!d) { startNext(); return; }                          // cancelled
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400) {                      // CDN redirect: follow without the token
      const QUrl next = r->url().resolved(r->header(QNetworkRequest::LocationHeader).toUrl());
      // https only (a model file over plain http could be swapped on the way), and a bounded chain
      if (next.scheme() != "https" || ++d->redirects > 5) {
        finish(key, next.scheme() != "https" ? QCoreApplication::translate("ai", "Refused: the download was redirected to an address that is not https.")
                                             : QCoreApplication::translate("ai", "Refused: too many redirects."));
        return;
      }
      sources::recordResult("huggingface.co", true, {});
      begin(*d, next, next.host() == "huggingface.co");
      return;
    }
    if (d->state == Download::State::Paused) { emit downloadsChanged(); startNext(); return; }
    if (status == 416 && d->received > 0) {
      // range not satisfiable: the .part is already whole (the app quit during the check) or longer than the file
      const QString part = targetPath(d->repo, d->file) + ".part";
      if (d->total > 0 && QFileInfo(part).size() == d->total) { verifyAndInstall(key, true); return; }
      restartFromZero(key);
      return;
    }
    if (r->error() != QNetworkReply::NoError || status >= 400) {
      QString err = d->error.isEmpty() ? r->errorString() : d->error;
      if (status == 401 || status == 403)
        err = QCoreApplication::translate("ai", "Access refused (HTTP %1): this model is gated. Accept its terms on huggingface.co and save your token.").arg(status);
      if (r->url().host() == "huggingface.co") sources::recordResult("huggingface.co", false, err);
      finish(key, err);
      return;
    }
    if (r->url().host() == "huggingface.co") sources::recordResult("huggingface.co", true, {});
    verifyAndInstall(key);
  });
}

void ModelHub::restartFromZero(const QString& key) {
  Download* d = find(key);
  if (!d) return;
  QFile::remove(targetPath(d->repo, d->file) + ".part");
  d->received = 0; d->state = Download::State::Queued; d->error.clear();
  if (key == m_activeKey) m_activeKey.clear();   // a verify can end while another download is the active one
  emit downloadsChanged();
  startNext();
}

void ModelHub::verifyAndInstall(const QString& key, bool restartOnMismatch) {
  Download* d = find(key);
  if (!d) return;
  d->state = Download::State::Verifying;
  emit downloadsChanged();
  const QString part = targetPath(d->repo, d->file) + ".part";
  const QString expected = m_sha.value(key);
  auto* w = new QFutureWatcher<QString>(this);
  connect(w, &QFutureWatcher<QString>::finished, this, [this, w, key, part, expected, restartOnMismatch] {
    const QString got = w->result();
    w->deleteLater();
    Download* d = find(key);
    if (!d) { startNext(); return; }   // cancelled while verifying: the queue must not stall
    if (!expected.isEmpty() && got != expected && restartOnMismatch) { restartFromZero(key); return; }   // a stale .part
    if (!expected.isEmpty() && got != expected) {
      QFile::remove(part);
      finish(key, QCoreApplication::translate("ai", "Checksum mismatch: the file was corrupted in transit and was deleted; download it again."));
      return;
    }
    const QString final = targetPath(d->repo, d->file);
    QFile::remove(final);
    if (!QFile::rename(part, final)) { finish(key, QCoreApplication::translate("ai", "Could not move the file into place.")); return; }
    LocalModel m;
    m.repo = d->repo; m.file = d->file; m.path = final; m.size = QFileInfo(final).size();
    m.license = m_license.value(key); m.sha256 = got; m.verified = !expected.isEmpty();
    m.downloadedAt = QDateTime::currentDateTimeUtc();
    m_installed.erase(std::remove_if(m_installed.begin(), m_installed.end(),
                                     [&](const LocalModel& x) { return x.repo == m.repo && x.file == m.file; }), m_installed.end());
    m_installed << m;
    saveIndex();
    finish(key, {});
    emit installedChanged();
  });
  w->setFuture(QtConcurrent::run([part] {
    QFile f(part);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
  }));
}

void ModelHub::finish(const QString& key, const QString& error) {
  for (int i = 0; i < m_queue.size(); ++i) {
    if (m_queue[i].key() != key) continue;
    if (error.isEmpty()) m_queue.removeAt(i);
    else { m_queue[i].state = Download::State::Failed; m_queue[i].error = error; }
    break;
  }
  // only when this was the active download: model A can finish verifying while B is downloading, and clearing
  // B's key would make PAUSE do nothing and CANCEL delete B's .part without aborting its reply
  if (key == m_activeKey) m_activeKey.clear();
  emit downloadsChanged();
  startNext();
}

void ModelHub::pause(const QString& key) {
  Download* d = find(key);
  if (!d) return;
  if (key == m_activeKey && m_reply) { d->state = Download::State::Paused; m_reply->abort(); }
  else if (d->state == Download::State::Queued) { d->state = Download::State::Paused; emit downloadsChanged(); }
}
void ModelHub::resume(const QString& key) {
  Download* d = find(key);
  if (!d || (d->state != Download::State::Paused && d->state != Download::State::Failed)) return;
  QFileInfo part(targetPath(d->repo, d->file) + ".part");
  d->received = part.exists() ? part.size() : 0;
  d->state = Download::State::Queued; d->error.clear();
  emit downloadsChanged();
  startNext();
}
void ModelHub::cancel(const QString& key) {
  for (int i = 0; i < m_queue.size(); ++i) {
    if (m_queue[i].key() != key) continue;
    const QString part = targetPath(m_queue[i].repo, m_queue[i].file) + ".part";
    m_queue.removeAt(i);
    if (key == m_activeKey && m_reply) m_reply->abort();   // finished() finds no entry and moves on
    QFile::remove(part);
    break;
  }
  emit downloadsChanged();
}

// ---------------------------------------------------------------------------------------------- installed
void ModelHub::loadIndex() {
  QFile f(modelsDir() + "/index.json");
  if (!f.open(QIODevice::ReadOnly)) return;
  for (const auto& v : QJsonDocument::fromJson(f.readAll()).array()) {
    const auto o = v.toObject();
    LocalModel m;
    m.repo = o["repo"].toString(); m.file = o["file"].toString(); m.license = o["license"].toString();
    m.sha256 = o["sha256"].toString(); m.size = qint64(o["size"].toDouble()); m.verified = o["verified"].toBool();
    m.downloadedAt = QDateTime::fromString(o["at"].toString(), Qt::ISODate);
    if (!safeRelative(m.repo) || !safeRelative(m.file)) continue;   // an entry pointing outside the folder (edited by hand)
    m.path = targetPath(m.repo, m.file);
    if (QFileInfo::exists(m.path)) m_installed << m;      // a model deleted by hand drops out of the list
  }
}
void ModelHub::saveIndex() {
  QJsonArray a;
  for (const auto& m : m_installed)
    a << QJsonObject{{"repo", m.repo}, {"file", m.file}, {"license", m.license}, {"sha256", m.sha256},
                     {"size", double(m.size)}, {"verified", m.verified}, {"at", m.downloadedAt.toString(Qt::ISODate)}};
  QSaveFile f(modelsDir() + "/index.json");
  if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(a).toJson()); f.commit(); }
}
QList<LocalModel> ModelHub::installed() const { return m_installed; }
bool ModelHub::isInstalled(const QString& repo, const QString& file) const {
  for (const auto& m : m_installed) if (m.repo == repo && m.file == file) return true;
  return false;
}
void ModelHub::remove(const QString& repo, const QString& file) {
  if (!safeRelative(repo) || !safeRelative(file)) return;
  QFile::remove(targetPath(repo, file));
  m_installed.erase(std::remove_if(m_installed.begin(), m_installed.end(),
                                   [&](const LocalModel& x) { return x.repo == repo && x.file == file; }), m_installed.end());
  // drop empty folders left behind (<models>/<owner>/<repo>)
  QDir dir(QFileInfo(targetPath(repo, file)).absolutePath());
  while (dir.absolutePath() != QDir(modelsDir()).absolutePath() && dir.isEmpty()) {
    const QString name = dir.dirName(); dir.cdUp(); dir.rmdir(name);
  }
  saveIndex();
  emit installedChanged();
}
}  // namespace ai
