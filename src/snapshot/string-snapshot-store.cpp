#include "cimmerian/snapshot/string-snapshot-store.hpp"
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Cimmerian::Snapshot {

namespace fs = std::filesystem;

static constexpr const char* MARKER_PREFIX = "# Snapshot: ";

// Captured failure messages carry ANSI escape codes (color, underline, ...)
// so ASSERT_STRING_SNAPSHOT can pin exact formatting. Those raw control bytes
// are opaque in a text editor, so on-disk we escape them to a readable form
// (e.g. ESC -> "\e") and reverse it on load; in-memory values always carry
// the real bytes, so comparisons still catch formatting regressions.
static std::string EscapeControlChars(const std::string& raw)
{
  std::string out;
  out.reserve(raw.size());
  for (unsigned char c : raw) {
    if (c == '\x1b') {
      out += "\\e";
    }
    else if (c == '\\') {
      out += "\\\\";
    }
    else if (c < 0x20 && c != '\n') {
      char buf[5];
      std::snprintf(buf, sizeof(buf), "\\x%02x", c);
      out += buf;
    }
    else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

static std::string UnescapeControlChars(const std::string& escaped)
{
  std::string out;
  out.reserve(escaped.size());
  for (std::size_t i = 0; i < escaped.size(); ++i) {
    if (escaped[i] == '\\' && i + 1 < escaped.size()) {
      const char next = escaped[i + 1];
      if (next == 'e') {
        out += '\x1b';
        ++i;
        continue;
      }
      if (next == '\\') {
        out += '\\';
        ++i;
        continue;
      }
      if (next == 'x' && i + 3 < escaped.size()) {
        const std::string hex = escaped.substr(i + 2, 2);
        out += static_cast<char>(std::stoi(hex, nullptr, 16));
        i += 3;
        continue;
      }
    }
    out += escaped[i];
  }
  return out;
}

std::optional<std::string> SnapFile::Find(const std::string& key) const
{
  auto it = this->indexByKey.find(key);
  if (it == this->indexByKey.end()) {
    return std::nullopt;
  }
  return this->entries[it->second].second;
}

void SnapFile::Set(const std::string& key, const std::string& value)
{
  auto it = this->indexByKey.find(key);
  if (it != this->indexByKey.end()) {
    this->entries[it->second].second = value;
    return;
  }
  this->indexByKey[key] = this->entries.size();
  this->entries.emplace_back(key, value);
}

SnapFile SnapFile::Parse(const std::string& fileContents)
{
  SnapFile file;

  std::istringstream stream(fileContents);
  std::string line;
  std::string currentKey;
  std::string currentContent;
  bool inEntry = false;

  auto flushEntry = [&]() {
    if (inEntry) {
      // Serialize() always appends two framing newlines after the value: one
      // that terminates its last content line and one blank separator line
      // before the next entry. Strip exactly those two back off.
      for (int i = 0; i < 2 && !currentContent.empty() && currentContent.back() == '\n'; ++i) {
        currentContent.pop_back();
      }
      file.Set(currentKey, UnescapeControlChars(currentContent));
    }
    currentContent.clear();
  };

  while (std::getline(stream, line)) {
    if (line.rfind(MARKER_PREFIX, 0) == 0) {
      flushEntry();
      currentKey = line.substr(std::string(MARKER_PREFIX).size());
      inEntry = true;
    }
    else if (inEntry) {
      currentContent += line;
      currentContent += '\n';
    }
  }
  flushEntry();

  return file;
}

std::string SnapFile::Serialize() const
{
  std::string out;
  for (const auto& [key, value] : this->entries) {
    out += MARKER_PREFIX;
    out += key;
    out += '\n';
    out += EscapeControlChars(value);
    out += '\n';
    out += '\n';
  }
  return out;
}

StringSnapshotStore& StringSnapshotStore::GetInstance()
{
  static StringSnapshotStore instance;
  return instance;
}

StringSnapshotStore::StringSnapshotStore(const std::string& snapshotRootDir)
    : snapshotRootDir(snapshotRootDir)
{
}

std::string StringSnapshotStore::ResolveSnapFilePath(const SnapshotKey& key) const
{
  std::string root = this->snapshotRootDir;
  if (root.empty()) {
    if (const auto& override_ = SnapshotRunModeRegistry::GetInstance().GetSnapshotRootOverride();
        override_.has_value()) {
      root = *override_;
    }
  }
  if (root.empty()) {
    root = SnapshotDirForFile(key.filePath);
  }
  return root + "/" + SnapshotStemForFile(key.filePath) + ".snap";
}

SnapFile& StringSnapshotStore::GetOrLoadFile(const std::string& path)
{
  auto it = this->loadedFiles.find(path);
  if (it != this->loadedFiles.end()) {
    return it->second;
  }

  SnapFile file;
  std::ifstream in(path, std::ios::binary);
  if (in) {
    std::ostringstream buffer;
    buffer << in.rdbuf();
    file = SnapFile::Parse(buffer.str());
  }

  auto [inserted, _] = this->loadedFiles.emplace(path, std::move(file));
  return inserted->second;
}

std::optional<std::string> StringSnapshotStore::Load(const SnapshotKey& key)
{
  const std::string path = this->ResolveSnapFilePath(key);
  SnapFile& file = this->GetOrLoadFile(path);
  return file.Find(SnapshotKeyToString(key));
}

void StringSnapshotStore::Save(const SnapshotKey& key, const std::string& value)
{
  const std::string path = this->ResolveSnapFilePath(key);
  SnapFile& file = this->GetOrLoadFile(path);
  file.Set(SnapshotKeyToString(key), value);
}

void StringSnapshotStore::Flush()
{
  for (const auto& [path, file] : this->loadedFiles) {
    fs::path fsPath(path);
    if (fsPath.has_parent_path()) {
      fs::create_directories(fsPath.parent_path());
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << file.Serialize();
  }
}

} // namespace Cimmerian::Snapshot
