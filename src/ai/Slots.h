#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>

// AI › Agents: four isolated environments ("cubes"), each holding at most one local model or one integration
// (repo "integration", file "claude-code": see ai/ClaudeCode.h).
// An environment owns its own folder <data>/ai/env-<n>/ (settings, future logs and working files) and shares nothing
// with the others; the model file itself stays where ModelHub put it and is only referenced.
// Assignments are saved in settings ai.slots (list of 4 "owner/repo/file" or "").
// Skills (ai::skills(): tick boxes on each cube) belong to the environment, not to the model: moving a model to another
// cube gives it that cube's skills. Saved in settings ai.skills (list of 4 comma-separated skill ids). A new environment
// grants nothing until a box is ticked. The model runs in ai::Engine; tools check the skill again at every call.
namespace ai {

struct Slot {
  int index = 0;            // 0..3
  QString repo, file;       // empty when off
  QDateTime loadedAt;
  bool isEmpty() const { return file.isEmpty(); }
  bool isIntegration() const;   // an integration (AI › LLM › Integration: Claude Code) instead of a local model file
  QString displayName() const;  // "Qwen3-4B-Q4_K_M", "Claude Code"
  QString key() const { return isEmpty() ? QString() : repo + "/" + file; }
};

class Slots : public QObject {
  Q_OBJECT
public:
  static constexpr int kCount = 4;
  static Slots& instance();
  const Slot& slot(int i) const { return m_slots[i]; }
  void load(int i, const QString& repo, const QString& file);
  void eject(int i);
  QString folder(int i) const;          // created on demand
  qint64 memoryTotal() const;           // physical RAM in bytes
  qint64 memoryLoaded() const;          // sum of the loaded models' file sizes (rough memory need)
  QStringList skills(int i) const;
  bool hasSkill(int i, const QString& id) const { return skills(i).contains(id); }
  void setSkill(int i, const QString& id, bool on);
signals:
  void changed(int index);
  void skillsChanged(int index);
private:
  Slots();
  void save();
  QVector<Slot> m_slots;
  QVector<QStringList> m_skills;
};
}  // namespace ai
