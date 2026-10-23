#include "cimmerian/ansi-formatter.hpp"
#include "cimmerian/crash-report.hpp"
#include "cimmerian/test.hpp"
#include "support/scratch-run.hpp"
#include <csignal>
#include <cstring>
#include <functional>
#include <regex>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

using Cimmerian::TestRegistry;

#ifndef _WIN32

// Each crash runs in a child process. There, this stands in for the default
// action, so the child ends with an exit code instead of the signal. Dying by
// the signal would leave a crash report or core file behind on every run.
extern "C" void ExitWithSignalNumber(int signalNumber)
{
  _exit(100 + signalNumber);
}

struct ChildResult {
  std::string output; // stdout and stderr, without colours or timings
  int exitCode = -1;  // 100 + the signal number if a crash reached ExitWithSignalNumber
};

ChildResult RunInChild(const std::function<void()>& body)
{
  ChildResult result;
  std::fflush(nullptr);
  int pipeEnds[2];
  if (pipe(pipeEnds) != 0) {
    return result;
  }

  const pid_t child = fork();
  if (child == 0) {
    close(pipeEnds[0]);
    dup2(pipeEnds[1], STDOUT_FILENO);
    dup2(pipeEnds[1], STDERR_FILENO);
    close(pipeEnds[1]);

    struct sigaction exitAction{};
    exitAction.sa_handler = ExitWithSignalNumber;
    sigemptyset(&exitAction.sa_mask);
    exitAction.sa_flags = SA_ONSTACK;
    for (const int signalNumber : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGTRAP, SIGABRT}) {
      sigaction(signalNumber, &exitAction, nullptr);
    }

    body();
    std::fflush(nullptr);
    _exit(0);
  }

  close(pipeEnds[1]);
  char buffer[4096];
  ssize_t count = 0;
  while ((count = read(pipeEnds[0], buffer, sizeof(buffer))) > 0) {
    result.output.append(buffer, static_cast<std::size_t>(count));
  }
  close(pipeEnds[0]);

  int status = 0;
  waitpid(child, &status, 0);
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1000 + WTERMSIG(status);

  result.output = Cimmerian::Ansi::AnsiFormatter::StripCodes(result.output);
  result.output = std::regex_replace(result.output, std::regex(R"(\d+\.\d+ms)"), "<time>");
  return result;
}

int* volatile nowhere = nullptr;

void WriteThroughNull()
{
  *nowhere = 1;
}

int Recurse(int depth)
{
  if (depth < 0) { // never, but it keeps the compiler from calling this endless
    return 0;
  }
  volatile char frame[256];
  std::memset(const_cast<char*>(frame), depth, sizeof(frame));
  return Recurse(depth + 1) + frame[depth % 256];
}

enum class CrashStage { BeforeAll, BeforeEach, Body, AfterEach, AfterAll };

// Runs, in a child, a scratch tree whose second test crashes in stage:
//
//   Crashes                 1 passing test
//   Crashes > Nested        hooks, and the test that crashes
ChildResult CrashIn(CrashStage stage, void (*crash)() = WriteThroughNull)
{
  return RunInChild([stage, crash] {
    TestRegistry registry;
    TestGroup* crashes = registry.GetChildGroup(registry.GetRootGroup(), "Crashes");
    registry.RegisterTest(crashes, "passes first", [](void*) {});
    TestGroup* nested = registry.GetChildGroup(crashes, "Nested");
    const auto crashIn = [stage, crash](CrashStage hookStage) {
      return [stage, crash, hookStage](void*) {
        if (stage == hookStage) {
          crash();
        }
      };
    };
    registry.SetBeforeAll(nested, crashIn(CrashStage::BeforeAll));
    registry.SetBeforeEach(nested, crashIn(CrashStage::BeforeEach));
    registry.SetAfterEach(nested, crashIn(CrashStage::AfterEach));
    registry.SetAfterAll(nested, crashIn(CrashStage::AfterAll));
    registry.RegisterTest(nested, "crashes", crashIn(CrashStage::Body));

    Cimmerian::TestRunner runner;
    const Cimmerian::CrashReportScope crashReport;
    Cimmerian::TestRunSummary summary;
    runner.RunGroup(registry.GetRootGroup(), &summary);
  });
}

// The [CRASH] line, or "" if there isn't one.
std::string CrashLine(const std::string& output)
{
  const std::size_t start = output.find("[CRASH]");
  return (start == std::string::npos) ? "" : output.substr(start, output.find('\n', start) - start);
}

#endif

} // namespace

DESCRIBE("Crash reports", {
  TEST("the crash context names the running test", {
    ASSERT_EQUAL(
        Cimmerian::GetCrashContext(),
        std::string("[Crash reports] the crash context names the running test")
    );
  });

  TEST("a runner made inside a test leaves the crash context as it found it", {
    const std::string before = Cimmerian::GetCrashContext();
    TestRegistry registry;
    registry.RegisterTest(
        registry.GetChildGroup(registry.GetRootGroup(), "Scratch"), "runs", [](void*) {}
    );
    CimmerianTestSupport::RunScratch(registry);
    ASSERT_EQUAL(Cimmerian::GetCrashContext(), before);
  });

  TEST("cuts a very long context short instead of overrunning its buffer", {
    const std::string before = Cimmerian::GetCrashContext();
    Cimmerian::SetCrashContext(std::string(2000, 'x'));
    const std::size_t length = Cimmerian::GetCrashContext().size();
    Cimmerian::SetCrashContext(before);
    ASSERT_EQUAL(length, std::size_t{511});
  });
});

#ifndef _WIN32
DESCRIBE("Crash reports in a child process", {
  TEST("puts back the signal handlers it replaced when it ends", {
    struct sigaction before{};
    sigaction(SIGSEGV, nullptr, &before);
    {
      const Cimmerian::CrashReportScope crashReport;
    }
    struct sigaction after{};
    sigaction(SIGSEGV, nullptr, &after);
    ASSERT_TRUE(after.sa_handler == before.sa_handler);
  });

  TEST("a crash in a test body names the test, after everything printed before it", {
    const ChildResult result = CrashIn(CrashStage::Body);
    ASSERT_EQUAL(
        result.output, std::string(
                           "[Crashes]\n"
                           "[PASS] [Crashes] passes first  (<time>)\n"
                           "[Nested]\n"
                           "\n"
                           "[CRASH] SIGSEGV (invalid memory access) in [Crashes > Nested] crashes\n"
                       )
    );
    ASSERT_EQUAL(result.exitCode, 100 + SIGSEGV);
  });

  TEST("a crash in a hook names the hook", {
    ASSERT_EQUAL(
        CrashLine(CrashIn(CrashStage::BeforeAll).output),
        std::string("[CRASH] SIGSEGV (invalid memory access) in [Crashes > Nested] BEFORE_ALL")
    );
    ASSERT_EQUAL(
        CrashLine(CrashIn(CrashStage::BeforeEach).output),
        std::string(
            "[CRASH] SIGSEGV (invalid memory access) in [Crashes > Nested] crashes, in BEFORE_EACH"
        )
    );
    ASSERT_EQUAL(
        CrashLine(CrashIn(CrashStage::AfterEach).output),
        std::string(
            "[CRASH] SIGSEGV (invalid memory access) in [Crashes > Nested] crashes, in AFTER_EACH"
        )
    );
    ASSERT_EQUAL(
        CrashLine(CrashIn(CrashStage::AfterAll).output),
        std::string("[CRASH] SIGSEGV (invalid memory access) in [Crashes > Nested] AFTER_ALL")
    );
  });

  TEST("reports a stack overflow", {
    const ChildResult result = CrashIn(CrashStage::Body, [] { Recurse(0); });
    ASSERT_EQUAL(
        CrashLine(result.output),
        std::string("[CRASH] SIGSEGV (invalid memory access) in [Crashes > Nested] crashes")
    );
    ASSERT_EQUAL(result.exitCode, 100 + SIGSEGV);
  });

  TEST("reports an abort, as from a failed assert", {
    const ChildResult result = CrashIn(CrashStage::Body, [] { std::abort(); });
    ASSERT_EQUAL(
        CrashLine(result.output),
        std::string(
            "[CRASH] SIGABRT (abort, such as a failed assert or an uncaught exception) in "
            "[Crashes > Nested] crashes"
        )
    );
    ASSERT_EQUAL(result.exitCode, 100 + SIGABRT);
  });

  TEST("reports an arithmetic error", {
    const ChildResult result = CrashIn(CrashStage::Body, [] { std::raise(SIGFPE); });
    ASSERT_EQUAL(
        CrashLine(result.output),
        std::string(
            "[CRASH] SIGFPE (arithmetic error, such as an integer division by zero) in "
            "[Crashes > Nested] crashes"
        )
    );
    ASSERT_EQUAL(result.exitCode, 100 + SIGFPE);
  });

  TEST("a crash between stages is reported as outside any test", {
    const ChildResult result = RunInChild([] {
      const Cimmerian::CrashReportScope crashReport;
      Cimmerian::SetCrashContext("");
      WriteThroughNull();
    });
    ASSERT_EQUAL(
        CrashLine(result.output),
        std::string("[CRASH] SIGSEGV (invalid memory access) outside any test")
    );
  });

  TEST("nested scopes report a crash once, then hand it to what came before them", {
    const ChildResult result = RunInChild([] {
      const Cimmerian::CrashReportScope outer;
      const Cimmerian::CrashReportScope inner;
      Cimmerian::SetCrashContext("[Group] test");
      WriteThroughNull();
    });
    ASSERT_EQUAL(
        result.output, std::string("\n[CRASH] SIGSEGV (invalid memory access) in [Group] test\n")
    );
    ASSERT_EQUAL(result.exitCode, 100 + SIGSEGV);
  });
});
#endif
