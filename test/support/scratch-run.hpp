#pragma once

#include "cimmerian/ansi-formatter.hpp"
#include "cimmerian/test-registry.hpp"
#include "cimmerian/test-runner.hpp"
#include <cstdio>
#include <regex>
#include <string>

// Runs a tree built in its own registry on a runner of its own, which hands
// failures back to this run's runner when it's destroyed. Its report goes to
// a temp file, so its [FAIL] lines don't land in this run's output.

namespace CimmerianTestSupport {

struct ScratchRun {
  Cimmerian::TestRunSummary summary;
  // The scratch runner's report, without colours, and with timings and line
  // numbers replaced by <time> and <line>.
  std::string output;
};

inline std::string ReadAll(std::FILE* stream)
{
  std::string text;
  std::rewind(stream);
  char buffer[4096];
  std::size_t count = 0;
  while ((count = std::fread(buffer, 1, sizeof(buffer), stream)) > 0) {
    text.append(buffer, count);
  }
  return text;
}

inline ScratchRun RunScratch(Cimmerian::TestRegistry& registry)
{
  ScratchRun run;
  std::FILE* stream = std::tmpfile();
  {
    Cimmerian::TestRunner runner;
    if (stream) {
      runner.SetOutput(stream);
    }
    runner.RunGroup(registry.GetRootGroup(), &run.summary);
  }
  if (stream) {
    run.output = Cimmerian::Ansi::AnsiFormatter::StripCodes(ReadAll(stream));
    std::fclose(stream);
  }
  run.output = std::regex_replace(run.output, std::regex(R"(\d+\.\d+ms)"), "<time>");
  run.output = std::regex_replace(run.output, std::regex(R"((\.cpp):\d+)"), "$1:<line>");
  return run;
}

} // namespace CimmerianTestSupport
