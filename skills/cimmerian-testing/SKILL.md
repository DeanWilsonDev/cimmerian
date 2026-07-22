---
description: Write, run, and extend tests against the Cimmerian C++20 testing framework (unit tests, string/inline/hash snapshots, visual regression tests). Use whenever a project depends on Cimmerian and you need to author test files, wire up test-main.cpp, or add assertions.
---

Cimmerian is a header-driven, zero-dependency C++20 unit testing framework
(BDD-style authoring, visual diff on failure, built-in timing), extended with
snapshot testing and visual/UI regression testing. This skill summarizes the
consumer-facing API; the full design docs live in this repo's `docs/`
directory (`docs/snapshot-testing-spec.md`, `docs/visual-regression-spec.md`)
if you're vendoring Cimmerian and need more depth than what's below.

## Requirements

C++20, CMake 3.20+, a compiler with `std::format` (GCC 13+, Clang 16+, MSVC
19.29+).

## Wiring it into a project

```cmake
add_subdirectory(Cimmerian)   # or wherever it's vendored (submodule, FetchContent, etc.)

add_executable(my_tests
  test/my.test.cpp
  test/test-main.cpp
)
target_link_libraries(my_tests PRIVATE cimmerian)
target_compile_features(my_tests PUBLIC cxx_std_20)
```

```cpp
// test/test-main.cpp — write your own entry point...
#include "cimmerian/test.hpp"

int main() {
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

### Assertions

| Macro | Behavior on failure |
|---|---|
| `ASSERT_TRUE(cond)` / `ASSERT_FALSE(cond)` | reports, keeps running |
| `ASSERT_EQUAL(a, b)` / `ASSERT_NOT_EQUAL(a, b)` | reports, keeps running |
| `REQUIRE_TRUE(cond)` / `REQUIRE_EQUAL(a, b)` | reports, `return`s out of the test immediately |

`ASSERT_EQUAL` produces a visual diff on failure (character-level for
strings, element-level for containers whose elements support
`std::format`; `∅` marks a missing element, strikethrough marks an extra
one). Prefer `ASSERT_*` for independent checks in one test, `REQUIRE_*` when
a later line would be meaningless/unsafe after an earlier check fails.

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
