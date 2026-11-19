#pragma once
#include <QString>

// Claude Code keeps its own transcript of every session under <config>/projects/<slug>/<session-id>.jsonl, where
// <config> is $CLAUDE_CONFIG_DIR or ~/.claude and <slug> is the working directory with every character that is not a
// letter or a digit turned into "-" (installed CLI 2.1.325; slugs over 200 characters get a hash we cannot rebuild).
namespace ai::claudecode {
// CLEAR of an environment: removes the transcripts Claude Code wrote for that environment's folder, and nothing else.
// Only the one folder named after the environment's own path, directly under <config>/projects; only the *.jsonl files
// in it whose recorded "cwd" is that environment folder (and their <session-id>/ side folder); the folder itself when
// it is then empty. Symbolic links are never followed nor removed. Returns the number of transcripts removed.
int clearTranscripts(const QString& envFolder, const QString& configDir = {});
}  // namespace ai::claudecode
