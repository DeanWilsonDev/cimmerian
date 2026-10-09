#pragma once

#include <format>
#include <iterator>
#include <string>
#include <vector>
#include "cimmerian/test-assertions.hpp"
#include "cimmerian/test-runner.hpp"
#include <cstddef>
#include <functional>
#include "fnv1a.hpp"
#include "hash-snapshot-store.hpp"
#include "inline-snapshot-rewriter.hpp"
#include "snapshot-key.hpp"
#include "snapshot-run-mode.hpp"
#include "string-snapshot-store.hpp"

namespace Cimmerian::Snapshot {

inline std::string CurrentScopePath()
{
  Cimmerian::TestRunner* active = Cimmerian::TestRunner::GetActive();
  return active ? active->GetCurrentGroupPath() : std::string();
}

// A mismatched snapshot's diff, followed by how to accept the new value. A
// multi-line snapshot's diff lines carry its newlines, so they're split up.
inline std::vector<std::string>
SnapshotMismatchDetails(const std::string& storedValue, const std::string& serializedValue)
{
  std::vector<std::string> details;
  for (const std::string& diffLine :
       Cimmerian::Assertions::FormatStringDiff(storedValue, serializedValue)) {
    std::vector<std::string> lines = Cimmerian::SplitLines(diffLine);
    details.insert(details.end(), lines.begin(), lines.end());
  }
  details.emplace_back();
  details.emplace_back("Run with --update-snapshots to accept the new value.");
  return details;
}

inline void AssertStringSnapshotImpl(
    const std::string& serializedValue,
    const std::string& label,
    const char* file,
    int line
)
{
  SnapshotKey key {file, CurrentScopePath(), label};
  StringSnapshotStore& store = StringSnapshotStore::GetInstance();
  SnapshotRunModeRegistry& runMode = SnapshotRunModeRegistry::GetInstance();
  SnapshotSummaryAccumulator& accumulator = SnapshotSummaryAccumulator::GetInstance();

  if (runMode.IsUpdateMode()) {
    store.Save(key, serializedValue);
    accumulator.RecordUpdated();
    return;
  }

  std::optional<std::string> existing = store.Load(key);
  if (!existing.has_value()) {
    store.Save(key, serializedValue);
    accumulator.RecordMissing();
    return;
  }

  if (*existing == serializedValue) {
    accumulator.RecordMatched();
    return;
  }

  accumulator.RecordFailed();
  const std::string keyString = SnapshotKeyToString(key);
  Cimmerian::Assertions::fail(
      file, line, "SNAPSHOT MISMATCH: \"" + keyString + "\"",
      SnapshotMismatchDetails(*existing, serializedValue)
  );
}

inline void AssertInlineSnapshotImpl(
    const std::string& serializedValue,
    const std::string& currentSnapshot,
    const char* file,
    int line
)
{
  SnapshotRunModeRegistry& runMode = SnapshotRunModeRegistry::GetInstance();
  SnapshotSummaryAccumulator& accumulator = SnapshotSummaryAccumulator::GetInstance();

  if (runMode.IsUpdateMode()) {
    InlineSnapshotRewriter::GetInstance().RecordUpdate(file, line, serializedValue);
    accumulator.RecordUpdated();
    return;
  }

  if (currentSnapshot.empty()) {
    accumulator.RecordMissing();
    Cimmerian::Assertions::fail(
        file, line, "INLINE SNAPSHOT NOT CAPTURED: run with --update-snapshots to capture it."
    );
    return;
  }

  if (currentSnapshot == serializedValue) {
    accumulator.RecordMatched();
    return;
  }

  accumulator.RecordFailed();
  Cimmerian::Assertions::fail(
      file, line,
      "INLINE SNAPSHOT MISMATCH:", SnapshotMismatchDetails(currentSnapshot, serializedValue)
  );
}

using HashFn = std::function<std::string(const void* data, std::size_t size)>;

inline void AssertHashSnapshotImpl(
    const void* data,
    std::size_t size,
    const std::string& label,
    const char* file,
    int line,
    const HashFn& hashFn = Fnv1a64Hex
)
{
  const std::string hexHash = hashFn(data, size);
  SnapshotKey key {file, CurrentScopePath(), label};
  HashSnapshotStore& store = HashSnapshotStore::GetInstance();
  SnapshotRunModeRegistry& runMode = SnapshotRunModeRegistry::GetInstance();
  SnapshotSummaryAccumulator& accumulator = SnapshotSummaryAccumulator::GetInstance();

  if (runMode.IsUpdateMode()) {
    store.Save(key, hexHash);
    accumulator.RecordUpdated();
    return;
  }

  std::optional<std::string> existing = store.Load(key);
  if (!existing.has_value()) {
    store.Save(key, hexHash);
    accumulator.RecordMissing();
    return;
  }

  if (*existing == hexHash) {
    accumulator.RecordMatched();
    return;
  }

  accumulator.RecordFailed();
  const std::string keyString = SnapshotKeyToString(key);
  Cimmerian::Assertions::fail(
      file, line, "HASH SNAPSHOT MISMATCH: \"" + keyString + "\" (hash changed)"
  );
}

} // namespace Cimmerian::Snapshot

#define ASSERT_STRING_SNAPSHOT(serializedValue, label)                                            \
  ::Cimmerian::Snapshot::AssertStringSnapshotImpl((serializedValue), (label), __FILE__, __LINE__)

#define ASSERT_STRING_SNAPSHOT_FMT(value, label)                                                  \
  ASSERT_STRING_SNAPSHOT(std::format("{}", (value)), (label))

#define ASSERT_INLINE_SNAPSHOT(serializedValue, currentSnapshot)                                   \
  ::Cimmerian::Snapshot::AssertInlineSnapshotImpl(                                                 \
      (serializedValue), (currentSnapshot), __FILE__, __LINE__                                     \
  )

#define ASSERT_INLINE_SNAPSHOT_FMT(value, currentSnapshot)                                         \
  ASSERT_INLINE_SNAPSHOT(std::format("{}", (value)), (currentSnapshot))

#define ASSERT_HASH_SNAPSHOT(dataPointer, byteSize, label)                                         \
  ::Cimmerian::Snapshot::AssertHashSnapshotImpl(                                                   \
      (dataPointer), (byteSize), (label), __FILE__, __LINE__                                       \
  )

#define ASSERT_HASH_SNAPSHOT_RANGE(container, label)                                               \
  ASSERT_HASH_SNAPSHOT(                                                                            \
      std::data(container), std::size(container) * sizeof(*std::data(container)), (label)          \
  )

#define ASSERT_HASH_SNAPSHOT_FN(dataPointer, byteSize, label, hashFn)                              \
  ::Cimmerian::Snapshot::AssertHashSnapshotImpl(                                                   \
      (dataPointer), (byteSize), (label), __FILE__, __LINE__, (hashFn)                             \
  )
