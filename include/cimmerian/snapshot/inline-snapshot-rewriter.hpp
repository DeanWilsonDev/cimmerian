#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Cimmerian::Snapshot {

class InlineSnapshotRewriter {
public:
  static InlineSnapshotRewriter& GetInstance();

  InlineSnapshotRewriter(const InlineSnapshotRewriter&) = delete;
  InlineSnapshotRewriter& operator=(const InlineSnapshotRewriter&) = delete;

  void RecordUpdate(const std::string& filePath, int line, const std::string& newValue);

  // Called once at end of run. Rewrites all affected source files.
  void FlushAll();

  static std::string MakeRawStringLiteral(const std::string& value);
  static std::string FindSafeDelimiter(const std::string& value);

private:
  InlineSnapshotRewriter() = default;

  struct PendingUpdate {
    int line;
    std::string newValue;
  };
  std::unordered_map<std::string, std::vector<PendingUpdate>> pendingByFile;

  void RewriteFile(const std::string& filePath, std::vector<PendingUpdate>& updates);
};

} // namespace Cimmerian::Snapshot
