#include "cimmerian/snapshot/inline-snapshot-rewriter.hpp"
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <tuple>

namespace Cimmerian::Snapshot {

namespace {

std::size_t OffsetOfLine(const std::string& content, int line)
{
  if (line <= 1) {
    return 0;
  }
  int currentLine = 1;
  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      ++currentLine;
      if (currentLine == line) {
        return i + 1;
      }
    }
  }
  return std::string::npos;
}

// Scans a call starting at `openParen` (the '(' of the macro invocation) for
// the matching close paren and the span of the last top-level string literal
// argument found before it (a plain "..." literal or a raw R"delim(...)delim"
// literal). Returns {literalStart, literalEnd, closeParenPos}, with
// literalStart == npos if no string literal argument was found.
std::tuple<std::size_t, std::size_t, std::size_t> FindLastStringLiteralBeforeClose(
    const std::string& content,
    std::size_t openParen
)
{
  std::size_t pos = openParen + 1;
  int depth = 1;
  std::size_t literalStart = std::string::npos;
  std::size_t literalEnd = std::string::npos;

  while (pos < content.size() && depth > 0) {
    const char c = content[pos];

    if (c == '(') {
      ++depth;
      ++pos;
      continue;
    }
    if (c == ')') {
      --depth;
      if (depth == 0) {
        break;
      }
      ++pos;
      continue;
    }
    if (c == '"') {
      const bool isRaw = (pos > 0 && content[pos - 1] == 'R');
      const std::size_t stringStart = isRaw ? pos - 1 : pos;

      if (isRaw) {
        std::size_t d = pos + 1;
        const std::size_t delimStart = d;
        while (d < content.size() && content[d] != '(') {
          ++d;
        }
        const std::string delim = content.substr(delimStart, d - delimStart);
        const std::string closer = ")" + delim + "\"";
        std::size_t closePos = content.find(closer, d + 1);
        std::size_t stringEnd =
            (closePos == std::string::npos) ? content.size() : closePos + closer.size();

        if (depth == 1) {
          literalStart = stringStart;
          literalEnd = stringEnd;
        }
        pos = stringEnd;
        continue;
      }
      else {
        const std::size_t s = pos;
        ++pos;
        while (pos < content.size() && content[pos] != '"') {
          if (content[pos] == '\\') {
            ++pos;
          }
          ++pos;
        }
        if (pos < content.size()) {
          ++pos; // include closing quote
        }
        if (depth == 1) {
          literalStart = s;
          literalEnd = pos;
        }
        continue;
      }
    }
    ++pos;
  }

  return {literalStart, literalEnd, pos};
}

} // namespace

InlineSnapshotRewriter& InlineSnapshotRewriter::GetInstance()
{
  static InlineSnapshotRewriter instance;
  return instance;
}

void InlineSnapshotRewriter::RecordUpdate(
    const std::string& filePath,
    int line,
    const std::string& newValue
)
{
  this->pendingByFile[filePath].push_back({line, newValue});
}

std::string InlineSnapshotRewriter::FindSafeDelimiter(const std::string& value)
{
  auto isSafe = [&](const std::string& delim) {
    const std::string closer = ")" + delim + "\"";
    return value.find(closer) == std::string::npos;
  };

  if (isSafe("")) {
    return "";
  }
  if (isSafe("snap")) {
    return "snap";
  }
  for (int i = 1;; ++i) {
    std::string delim = "s" + std::to_string(i);
    if (isSafe(delim)) {
      return delim;
    }
  }
}

std::string InlineSnapshotRewriter::MakeRawStringLiteral(const std::string& value)
{
  const std::string delim = FindSafeDelimiter(value);
  return "R\"" + delim + "(" + value + ")" + delim + "\"";
}

void InlineSnapshotRewriter::RewriteFile(
    const std::string& filePath,
    std::vector<PendingUpdate>& updates
)
{
  std::ifstream in(filePath, std::ios::binary);
  if (!in) {
    return;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  std::string content = buffer.str();
  in.close();

  // Bottom-up: process the highest line number first so earlier updates'
  // recorded line numbers stay valid even if a later replacement changes the
  // total line count (e.g. inserting a multi-line raw string).
  std::sort(updates.begin(), updates.end(), [](const PendingUpdate& a, const PendingUpdate& b) {
    return a.line > b.line;
  });

  for (const PendingUpdate& update : updates) {
    const std::size_t lineStart = OffsetOfLine(content, update.line);
    if (lineStart == std::string::npos) {
      continue;
    }

    std::size_t macroPos = content.find("ASSERT_INLINE_SNAPSHOT", lineStart);
    if (macroPos == std::string::npos) {
      continue;
    }

    std::size_t afterName = macroPos + std::strlen("ASSERT_INLINE_SNAPSHOT");
    if (content.compare(afterName, 4, "_FMT") == 0) {
      afterName += 4;
    }

    std::size_t openParen = content.find('(', afterName);
    if (openParen == std::string::npos) {
      continue;
    }

    auto [literalStart, literalEnd, closeParenPos] = FindLastStringLiteralBeforeClose(content, openParen);
    (void)closeParenPos;
    if (literalStart == std::string::npos) {
      continue;
    }

    const std::string newLiteral = MakeRawStringLiteral(update.newValue);
    content.replace(literalStart, literalEnd - literalStart, newLiteral);
  }

  std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
  out << content;

  SnapshotSummaryAccumulator::GetInstance().RecordInlineRewrite();
}

void InlineSnapshotRewriter::FlushAll()
{
  for (auto& [filePath, updates] : this->pendingByFile) {
    this->RewriteFile(filePath, updates);
  }
  this->pendingByFile.clear();
}

} // namespace Cimmerian::Snapshot
