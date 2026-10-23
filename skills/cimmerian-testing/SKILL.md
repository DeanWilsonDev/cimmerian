---
description: Write, run, and extend tests against the Cimmerian C++26 testing framework (unit tests, string/inline/hash snapshots, visual regression tests). Use whenever a project depends on Cimmerian and you need to author test files, wire up test-main.cpp, or add assertions.
---

Cimmerian is a header-driven, zero-dependency C++26 unit testing framework
(BDD-style authoring, visual diff on failure, built-in timing), extended with
snapshot testing and visual/UI regression testing. This skill summarizes the
consumer-facing API; the full design docs live in this repo's `docs/`
directory (`docs/snapshot-testing-spec.md`, `docs/visual-regression-spec.md`)
if you're vendoring Cimmerian and need more depth than what's below.

## Requirements

C++26, CMake 3.30+, GCC 14+, Clang 19+, or a recent AppleClang. On Windows,
use clang-cl from Clang 19+; MSVC's cl.exe isn't supported.

## Wiring it into a project

```cmake
add_subdirectory(Cimmerian)   # or wherever it's vendored (submodule, FetchContent, etc.)

add_executable(my_tests
  test/my.test.cpp
  test/test-main.cpp
)
target_link_libraries(my_tests PRIVATE cimmerian)
target_compile_features(my_tests PUBLIC cxx_std_26)
```

```cpp
// test/test-main.cpp — write your own entry point...
#include "cimmerian/test.hpp"

int main(int argc, char* argv[]) {
  Cimmerian::ParseArgs(argc, argv); // --filter, --list, --shard, --forbid-only, ...
  Cimmerian::TestRunner runner;
  auto summary = runner.RunAll(&Cimmerian::TestRegistry::GetInstance());
  return summary.failed > 0 ? 1 : 0;
}
```

```cpp
// ...or use the provided one instead
#include <cimmerian/test-entry-point.hpp>
```

## Core unit test authoring

```cpp
#include "cimmerian/test.hpp"

DESCRIBE("Math", {
  BEFORE_ALL({ /* once, before the group */ });
  AFTER_ALL({ /* once, after the group */ });
  BEFORE_EACH({ /* before every test in the group */ });
  AFTER_EACH({ /* after every test in the group */ });

  IT("adds two numbers", {              // IT / TEST are aliases
    ASSERT_EQUAL(1 + 1, 2);
  });

  DESCRIBE("Nested", {                  // groups nest arbitrarily
    IT("compares vectors", {
      ASSERT_EQUAL(std::vector{1,2,3}, std::vector{1,2,3});
    });
  });
});
```

`IT_FN`/`TEST_FN` register a free function as a test case — useful for
table-driven tests:

```cpp
void MyTest(void*) { ASSERT_TRUE(SomeCondition()); }
IT_FN("my test", MyTest);
```

### Skipping and focusing (`_SKIP` / `_ONLY`)

Every registration macro has `_SKIP` and `_ONLY` variants: `DESCRIBE_SKIP`,
`IT_ONLY`, `TEST_FN_SKIP`, `VISUAL_TEST_ONLY`,
`VISUAL_DESCRIBE_COMPONENT_SKIP`, and so on.

- `_SKIP`: the test (or every test in the group) doesn't run and prints as
  `[SKIP]`. Its hooks don't fire.
- `_ONLY`: once any test is `_ONLY` (directly or via a `DESCRIBE_ONLY`),
  only `_ONLY` tests run. This applies across the whole binary, unit and
  visual alike. Other tests are skipped silently.
- `_SKIP` beats `_ONLY` at any nesting level.
- Don't leave `_ONLY` in committed code, because it silently drops the rest
  of the suite. The run summary prints `Focused run: ...` whenever one is
  active.
- In CI, pass `--forbid-only` and/or `--forbid-skip` to the test binary. If
  any matching test exists, the runner runs nothing. It lists each offending
  test by full group path and exits non-zero. `--forbid-only` also stops
  `_ONLY` from focusing the run. A hand-written `main` must call
  `Cimmerian::ParseArgs(argc, argv)` for the flags to work;
  `test-entry-point.hpp` already does.

### Picking and sharding tests from the command line

To run one test or a few, pass `--filter`, rather than editing in an `_ONLY`
and rebuilding.

| Flag | Effect |
|---|---|
| `--filter <pattern>` | Runs only the tests whose full name contains `pattern` (case-sensitive) |
| `--list` | Prints the full name of each test a run would run, one per line, and runs nothing |
| `--shard <index>/<count>` | Runs the `index`-th of `count` slices, counting from 1 |
| `--save-timings <file>` | After the run, writes each top-level `DESCRIBE`'s time |
| `--timings <file>` | Balances `--shard` by the times in `file` instead of test counts |

- A full name is the group path plus the test name, as `--list` prints it:
  `Parser > Nested > handles commas`.
- To use every core, run `--shard 1/N` to `--shard N/N` as N processes at
  once, e.g. through CTest (see the README), and pass if every one exits 0.
  Each top-level `DESCRIBE` stays whole in one shard, and the shards together
  run each test exactly once.
- `--update-snapshots` can't be combined with `--shard`; update snapshots in
  one unsharded run.
- Shards balance by test count. When groups differ a lot in time per test,
  record each top-level group's time with `--save-timings <file>` in an
  unsharded run, and pass `--timings <file>` to every shard to balance by
  time. Every shard must read the same file, so a shard can't save over the
  file it reads. Save each shard to its own file and concatenate them.
  A missing file falls back to test count; a malformed one stops the run.

### Assertions

Every check comes in two forms. `ASSERT_*` reports the failure and keeps
running; `REQUIRE_*` reports it and `return`s out of the test immediately.
Arguments are actual first, expected second.

| Fails when | Keeps running | Stops the test |
|---|---|---|
| `cond` is false | `ASSERT_TRUE(cond)` | `REQUIRE_TRUE(cond)` |
| `cond` is true | `ASSERT_FALSE(cond)` | `REQUIRE_FALSE(cond)` |
| `actual != expected` | `ASSERT_EQUAL(actual, expected)` | `REQUIRE_EQUAL(actual, expected)` |
| `actual == expected` | `ASSERT_NOT_EQUAL(actual, expected)` | `REQUIRE_NOT_EQUAL(actual, expected)` |
| they differ by more than `epsilon` | `ASSERT_NEAR(actual, expected, epsilon)` | `REQUIRE_NEAR(actual, expected, epsilon)` |
| `ptr` isn't null | `ASSERT_NULL(ptr)` | `REQUIRE_NULL(ptr)` |
| `ptr` is null | `ASSERT_NOT_NULL(ptr)` | `REQUIRE_NOT_NULL(ptr)` |
| `expression` doesn't throw `ExceptionType` | `ASSERT_THROWS(expression, ExceptionType)` | `REQUIRE_THROWS(expression, ExceptionType)` |
| `expression` throws | `ASSERT_NO_THROW(expression)` | `REQUIRE_NO_THROW(expression)` |

Apart from the snapshot assertions below, that's the full set. There's no
`EXPECT_*`, `CHECK_*`, `ASSERT_EQ`/`ASSERT_NE` or bare `REQUIRE(cond)`. A
condition containing a top-level comma, such as
`ASSERT_TRUE(std::is_same_v<A, B>)`, needs an extra pair of parentheses.

`ASSERT_EQUAL` produces a visual diff on failure (character-level for
strings, element-level for containers whose elements support
`std::format`; `∅` marks a missing element, strikethrough marks an extra
one). Prefer `ASSERT_*` for independent checks in one test, `REQUIRE_*` when
a later line would be meaningless/unsafe after an earlier check fails.

An exception that escapes a test body, `BEFORE_EACH` or `AFTER_EACH` is
reported as that test's failure, with `what()` as the detail, and the run
carries on. A throwing `BEFORE_EACH` skips the test's body but still runs
`AFTER_EACH`. A memory fault is a different matter: an out-of-bounds read
still takes the whole run down, which is why a size check that guards an
index should be a `REQUIRE_*`. The run's last line on stderr then names the
crash and what was running, e.g. `[CRASH] SIGSEGV (invalid memory access) in
[Parser > Nested] handles quotes, in BEFORE_EACH`, and the process ends with
that signal.

If `BEFORE_ALL` fails an assertion or throws, every test in its group, nested
groups included, is reported `[FAIL] ... (not run: BEFORE_ALL failed)` without
running, and `AFTER_ALL` still runs. A failed `AFTER_ALL` gets its own
`[FAIL] [Group] AFTER_ALL` line and fails the run, even if every test passed.
It's counted in `summary.failed`, so a `main` that exits on it needs no change.

## Snapshot testing (`cimmerian/snapshot.hpp`)

Three flavors, all `DESCRIBE`/`TEST`-compatible:

```cpp
#include "cimmerian/snapshot.hpp"

TEST("struct-like string snapshot", {
  ASSERT_STRING_SNAPSHOT(value, "frame-dims");       // stored in a per-test .snap file
});

TEST("shorthand via std::format", {
  ASSERT_STRING_SNAPSHOT_FMT(answer, "answer");       // formats `answer` for you
});

TEST("inline snapshot", {
  ASSERT_INLINE_SNAPSHOT(value, R"(Point{x=10, y=20})"); // literal lives in the source itself
});

TEST("hash snapshot for large/binary data", {
  ASSERT_HASH_SNAPSHOT_RANGE(buffer, "small-buffer");     // FNV-1a hash, not the raw bytes
});
```

- **String snapshots** land in `__snapshots__/<file>.snap` next to the test
  file, keyed by group/test/label — good for structured, human-diffable text.
- **Inline snapshots** rewrite the literal in the source file itself when run
  in update mode — good for small values you want visible at the call site.
- **Hash snapshots** store an FNV-1a hash instead of the raw content — good
  for large buffers where the content itself isn't worth diffing.

Run mode (verify vs. update-on-mismatch) follows the same run-mode
integration as the rest of the suite — see `docs/snapshot-testing-spec.md`
§"Run Mode Integration" for the exact CLI/env flag if you're touching that
layer.

## Visual / UI regression testing (`cimmerian/visual.hpp`)

Mirrors `DESCRIBE`/`TEST`, plus a window handle and event/wait primitives:

```cpp
#include "cimmerian/visual.hpp"
using namespace std::chrono_literals;

VISUAL_DESCRIBE("Settings Panel", settingsPanelWindow, {
  VISUAL_TEST("default state looks correct", {
    WAIT(50ms);
    ASSERT_SNAPSHOT("default-state");
  });

  VISUAL_TEST("hover on save button", {
    SEND(MouseMove(200, 340));
    WAIT(16ms);
    ASSERT_SNAPSHOT("save-button-hover");

    SEND(MouseClick(200, 340));           // SEND(MouseClick(x, y, 3)) = right-click
    WAIT(32ms);
    ASSERT_SNAPSHOT("save-button-clicked");
  });
});
```

- **`SEND`** — real, global OS input (`MouseMove`, `MouseClick`,
  `MouseScroll`, `KeyPress`). Screen-absolute, not scoped to the target
  window — see "Platform caveats" below before relying on it in a
  multi-window / non-CI environment.
- **`WAIT(duration)`** — lets a frame/animation settle before the next
  assertion; `16ms` ≈ one frame at 60fps.
- **`ASSERT_SNAPSHOT(label[, windowHandle][, DiffOptions{...}])`** — captures
  and compares a screenshot of the window. `DiffOptions{.maxDifferencePercent
  = 0.5f}` tolerates minor rendering noise (AA, subpixel text).
- **`NAVIGATE(screenKey)`** — jumps straight to a screen via a
  consumer-registered `NavigateFn` instead of clicking through the UI with
  `SEND`. **Throws if no `NavigateFn` is registered** — it's opt-in, not a
  silent no-op, so a forgotten registration fails loudly rather than letting
  a later `ASSERT_SNAPSHOT` pass against the wrong screen.
- **`VISUAL_DESCRIBE_COMPONENT(name, { ... })`** — a `VISUAL_DESCRIBE` with
  no group-level window; each `VISUAL_TEST` mounts and tears down its own
  host window and passes it explicitly to `ASSERT_SNAPSHOT("label",
  host.WindowHandle())`. Use this for component-level tests (a button's
  hover/disabled/selected states) rather than reusing one shared window.

### Registering a `NavigateFn`

```cpp
Cimmerian::Visual::ActiveNavigationDriver::GetInstance().Set(
    [](const std::string& screenKey) { /* route the real app */ });
```

### Wiring up a real, separately-launched app

`WaitForWindowByTitle` / `WaitForWindowByPid`
(`cimmerian/visual/window-lookup.hpp`) poll for a window by title substring
or owning pid and return its handle as `void*` (`nullptr` on timeout) — the
way to get a `VISUAL_DESCRIBE` window handle for an app spawned as its own
subprocess rather than a scratch window built inline in the test.

### Platform caveats (read before writing multi-window visual tests)

- Input injection is **real, global OS input** — not scoped to the target
  window the way a browser automation tool's isolated page is. If the
  target window loses OS focus mid-test (notification, alt-tab, background
  process raising a window), the next `SEND()` goes wherever focus actually
  is. `X11EventInjector` checks focus via `XGetInputFocus` before each
  `SEND()` and warns (doesn't refuse) if the target isn't focused — this
  narrows the race, it doesn't eliminate it. Prefer a dedicated `Xvfb`/CI
  display over a live multi-window desktop when possible.
- On Linux, XTEST-over-XWayland may not work depending on the compositor;
  `AutoLinuxEventInjector` falls back to the kernel `/dev/uinput` layer
  automatically in that case.
- Win32/macOS window-lookup and event injection exist but aren't
  compile-tested in every environment — check `docs/next_steps.md` for the
  current state before assuming full parity across platforms.

Full design detail (event model, screenshot storage layout, image
comparison algorithm, run modes, review-mode HTML report) is in
`docs/visual-regression-spec.md`.

## Project structure reference

```
cimmerian/
├── include/cimmerian/       — public headers (single entry point: test.hpp / snapshot.hpp / visual.hpp)
├── src/                     — implementation
├── test/                    — Cimmerian's own self-tests (good worked examples)
├── docs/                    — design specs and gap/proposal docs (durable record, read before deep changes)
└── CMakeLists.txt
```
