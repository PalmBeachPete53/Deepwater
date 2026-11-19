#pragma once
#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QElapsedTimer>
#include <functional>

class QNetworkReply;
class QFile;

// Local models from the Hugging Face Hub (public API, https://huggingface.co/docs/hub/api):
//   search    GET /api/models?search=…&filter=gguf&sort=…&direction=-1&limit=…
//   details   GET /api/models/<repo>  (licence, gated) and /api/models/<repo>/tree/main?recursive=1 (files, sizes, sha256)
//   download  GET /<repo>/resolve/main/<file> → 302 to the CDN. The redirect is followed by hand so the optional token
//             (settings key keys.huggingface, needed for gated models only) is sent to huggingface.co and never to the CDN.
// Downloads stream to <data>/models/<owner>/<repo>/<file>.part (resumable with Range), are checked against the
// LFS sha256 when the Hub gives one, then renamed. One download runs at a time; the others wait in the queue.
// Installed models are listed in <data>/models/index.json.
namespace ai {

struct HubModel {
  QString id;                 // "Qwen/Qwen2.5-0.5B-Instruct-GGUF"
  qint64 downloads = 0, likes = 0;
  QString pipeline;           // "text-generation", …
  QString license;            // "apache-2.0", "llama3.1", … (empty when not stated)
  QString gated;              // "" (open), "auto" or "manual" (the user must accept the terms on huggingface.co)
  QDateTime lastModified;
};

struct HubFile {
  QString path;               // "qwen2.5-0.5b-instruct-q4_k_m.gguf"
  qint64 size = 0;
  QString sha256;             // LFS oid, empty for small non-LFS files
  bool isGguf() const { return path.endsWith(".gguf", Qt::CaseInsensitive); }
  QString quant() const;      // "Q4_K_M", "F16", … parsed from the name, or ""
};

struct ModelDetails {
  HubModel model;
  QList<HubFile> files;
  QString error;
};

struct Download {
  QString repo, file;
  qint64 total = 0, received = 0;
  double bytesPerSec = 0;
  enum class State { Queued, Running, Paused, Verifying, Failed } state = State::Queued;
  int redirects = 0;          // followed so far for this attempt (at most 5)
  QString error;
  QString key() const { return repo + "/" + file; }
};

struct LocalModel {
  QString repo, file, path, license, sha256;
  qint64 size = 0;
  QDateTime downloadedAt;
  bool verified = false;      // sha256 matched the Hub's
};

class ModelHub : public QObject {
  Q_OBJECT
public:
  static ModelHub& instance();
  enum class Sort { Downloads, Likes, Updated };

  void search(const QString& query, bool ggufOnly, Sort sort, QObject* ctx,
              std::function<void(const QList<HubModel>&, const QString& error)> cb);
  void details(const QString& repo, QObject* ctx, std::function<void(const ModelDetails&)> cb);

  void download(const QString& repo, const HubFile& file, const QString& license);
  void pause(const QString& key);
  void resume(const QString& key);
  void cancel(const QString& key);          // stops and deletes the partial file
  QList<Download> downloads() const;        // queued, running, paused, failed

  QList<LocalModel> installed() const;
  bool isInstalled(const QString& repo, const QString& file) const;
  void remove(const QString& repo, const QString& file);
  QString modelsDir() const;
  qint64 freeSpace() const;

  QString token() const;                    // keys.huggingface, never logged
  void setToken(const QString& token);

signals:
  void downloadsChanged();
  void installedChanged();

private:
  ModelHub();
  void startNext();
  void begin(Download& d, const QUrl& url, bool withToken);
  void finish(const QString& key, const QString& error);
  void verifyAndInstall(const QString& key, bool restartOnMismatch = false);   // restart: a stale .part, download it again
  void restartFromZero(const QString& key);
  void loadIndex();
  void saveIndex();
  QString targetPath(const QString& repo, const QString& file) const;
  // A repo id or file path from the Hub's JSON (or index.json) that is safe to join under modelsDir(): relative,
  // '/'-separated, no empty / "." / ".." component, no backslash or drive colon.
  static bool safeRelative(const QString& p);
  Download* find(const QString& key);

  QList<Download> m_queue;
  QHash<QString, QString> m_sha, m_license;   // by download key
  QPointer<QNetworkReply> m_reply;
  QFile* m_out = nullptr;
  QString m_activeKey;
  QElapsedTimer m_rateClock; qint64 m_rateBytes = 0;
  QList<LocalModel> m_installed;
};
}  // namespace ai
