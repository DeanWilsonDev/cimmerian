#include "cimmerian/crash-report.hpp"
#include "cimmerian/ansi-codes.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iterator>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace Cimmerian {

namespace {

// Everything the signal handler touches is fixed-size and set up before any
// signal can arrive. The handler itself only calls write, sigaction (signal
// on Windows) and raise, which are async-signal-safe.

constexpr std::size_t CONTEXT_CAPACITY = 512;

// Two buffers, so a signal that arrives while SetCrashContext is filling one
// still reads a whole context from the other.
char contextBuffers[2][CONTEXT_CAPACITY] = {};
std::atomic<int> activeContext{0};
static_assert(std::atomic<int>::is_always_lock_free);

struct FatalSignal {
  int number;
  const char* description;
};

constexpr FatalSignal FATAL_SIGNALS[] = {
    {SIGSEGV, "SIGSEGV (invalid memory access)"},
#ifdef SIGBUS
    {SIGBUS, "SIGBUS (invalid memory access)"},
#endif
    {SIGFPE, "SIGFPE (arithmetic error, such as an integer division by zero)"},
    {SIGILL, "SIGILL (illegal instruction)"},
#ifdef SIGTRAP
    {SIGTRAP, "SIGTRAP (trap, such as a failed hardening check)"},
#endif
    {SIGABRT, "SIGABRT (abort, such as a failed assert or an uncaught exception)"},
};
constexpr std::size_t FATAL_SIGNAL_COUNT = std::size(FATAL_SIGNALS);

#ifdef _WIN32
using SignalAction = void (*)(int);
#else
using SignalAction = struct sigaction;
#endif

// What one live scope replaced, and the scope outside it.
struct ScopeState {
  SignalAction replacedActions[FATAL_SIGNAL_COUNT];
  ScopeState* outer = nullptr;
  bool installedSignalStack = false;
};

// Scopes nest only a few deep (RunAll and the visual runner's RunAll in one
// main), so a small fixed stack is enough.
ScopeState scopeStates[8];
int scopeCount = 0;

// The scope a crash goes through next. The handler steps it outward each time
// it hands the signal on, so a crash passes through every scope's handler to
// whatever came before the outermost one.
ScopeState* volatile innermostScope = nullptr;

// Only the first fatal signal is reported. A scope's handler hands the signal
// on to whatever came before it, which may be an outer scope's handler.
volatile std::sig_atomic_t crashReported = 0;

void WriteToStderr(const char* text)
{
  std::size_t remaining = std::strlen(text);
  while (remaining > 0) {
#ifdef _WIN32
    const int written = _write(2, text, static_cast<unsigned>(remaining));
#else
    const ssize_t written = write(STDERR_FILENO, text, remaining);
#endif
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return;
    }
    text += written;
    remaining -= static_cast<std::size_t>(written);
  }
}

extern "C" void ReportFatalSignal(int signalNumber)
{
  const int savedErrno = errno;

  std::size_t index = 0;
  while (index < FATAL_SIGNAL_COUNT && FATAL_SIGNALS[index].number != signalNumber) {
    index++;
  }
  if (index == FATAL_SIGNAL_COUNT) {
    return;
  }

  if (!crashReported) {
    crashReported = 1;
    const char* context = contextBuffers[activeContext.load(std::memory_order_acquire)];
    WriteToStderr("\n");
    WriteToStderr(Ansi::ANSI_COLOR_BRIGHT_RED);
    WriteToStderr("[CRASH] ");
    WriteToStderr(FATAL_SIGNALS[index].description);
    WriteToStderr(context[0] != '\0' ? " in " : " outside any test");
    WriteToStderr(context);
    WriteToStderr(Ansi::ANSI_RESET);
    WriteToStderr("\n");
  }

  // Go on as if this handler weren't here. The signal is blocked until this
  // returns, so the raise takes effect then, through the restored action.
  ScopeState* scope = innermostScope;
  if (scope) {
    innermostScope = scope->outer;
#ifdef _WIN32
    std::signal(signalNumber, scope->replacedActions[index]);
#else
    sigaction(signalNumber, &scope->replacedActions[index], nullptr);
#endif
  }
  else {
    std::signal(signalNumber, SIG_DFL);
  }
  errno = savedErrno;
  std::raise(signalNumber);
}

#ifndef _WIN32
// A stack overflow leaves no stack to run the handler on, so it runs on this
// one. 128 KiB is macOS's SIGSTKSZ, the largest of the platforms'. It's per
// thread, so it covers the thread that made the scope.
alignas(16) char signalStack[128 * 1024];
#endif

} // namespace

CrashReportScope::CrashReportScope()
{
  if (scopeCount >= static_cast<int>(std::size(scopeStates))) {
    return;
  }
  ScopeState& state = scopeStates[scopeCount++];
  state.installedSignalStack = false;
  this->installed = true;

#ifdef _WIN32
  for (std::size_t i = 0; i < FATAL_SIGNAL_COUNT; ++i) {
    state.replacedActions[i] = std::signal(FATAL_SIGNALS[i].number, ReportFatalSignal);
  }
#else
  // Asked each time rather than remembered: a child made by fork has the
  // same memory but, on macOS, no signal stack.
  stack_t currentStack{};
  if (sigaltstack(nullptr, &currentStack) == 0 && (currentStack.ss_flags & SS_DISABLE)) {
    stack_t newStack{};
    newStack.ss_sp = signalStack;
    newStack.ss_size = sizeof(signalStack);
    state.installedSignalStack = sigaltstack(&newStack, nullptr) == 0;
  }

  struct sigaction action{};
  action.sa_handler = ReportFatalSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_ONSTACK;
  for (std::size_t i = 0; i < FATAL_SIGNAL_COUNT; ++i) {
    sigaction(FATAL_SIGNALS[i].number, &action, &state.replacedActions[i]);
  }
#endif

  state.outer = innermostScope;
  innermostScope = &state;
}

CrashReportScope::~CrashReportScope()
{
  if (!this->installed) {
    return;
  }
  ScopeState& state = scopeStates[--scopeCount];

#ifdef _WIN32
  for (std::size_t i = 0; i < FATAL_SIGNAL_COUNT; ++i) {
    std::signal(FATAL_SIGNALS[i].number, state.replacedActions[i]);
  }
#else
  for (std::size_t i = 0; i < FATAL_SIGNAL_COUNT; ++i) {
    sigaction(FATAL_SIGNALS[i].number, &state.replacedActions[i], nullptr);
  }
  if (state.installedSignalStack) {
    stack_t disabled{};
    disabled.ss_flags = SS_DISABLE;
    sigaltstack(&disabled, nullptr);
  }
#endif

  innermostScope = state.outer;
}

void SetCrashContext(std::string_view context)
{
  const int next = 1 - activeContext.load(std::memory_order_relaxed);
  const std::size_t length = std::min(context.size(), CONTEXT_CAPACITY - 1);
  std::memcpy(contextBuffers[next], context.data(), length);
  contextBuffers[next][length] = '\0';
  activeContext.store(next, std::memory_order_release);
}

std::string GetCrashContext()
{
  return contextBuffers[activeContext.load(std::memory_order_acquire)];
}

} // namespace Cimmerian
