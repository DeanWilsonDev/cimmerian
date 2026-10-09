#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Cimmerian {

// One assertion failure, as handed to an ITestFailHandler. The handler owns
// layout: message is the one-line reason ("Containers differ:") and each
// entry in details is one line to lay out under it (an expected/received
// diff, a list of violations), carrying no indentation of the handler's.
// file is empty when the failure has no source location, as for an exception
// escaping a test.
struct TestFailRecord {
  std::string file;
  int line = 0;
  std::string message;
  std::vector<std::string> details;
};

// Splits text on '\n', dropping trailing empty lines, so a preformatted block
// can travel as TestFailRecord::details.
inline std::vector<std::string> SplitLines(std::string_view text)
{
  std::vector<std::string> lines;
  std::size_t lineStart = 0;
  while (true) {
    const std::size_t lineEnd = text.find('\n', lineStart);
    if (lineEnd == std::string_view::npos) {
      lines.emplace_back(text.substr(lineStart));
      break;
    }
    lines.emplace_back(text.substr(lineStart, lineEnd - lineStart));
    lineStart = lineEnd + 1;
  }
  while (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  return lines;
}

} // namespace Cimmerian
