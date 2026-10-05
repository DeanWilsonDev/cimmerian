# Cimmerian

<p align="center">
<img src="assets/docs/cimmerian-logo.svg" width="500"/>
</p>

A modern C++20 unit testing framework. BDD-style test authoring, visual diff output on failure, and built-in performance timing — with zero external dependencies.

```
[Example Tests]
[Math]
[PASS] [Math] Addition  (0.0005ms)
[Math] group total: 0.0021ms

[Strings]
[FAIL] [Strings] Compare greetings  (0.0003ms)
  + "hello world"
  - "hello wurld"

────────────────────────────────────────
Summary: 4 total, 3 passed, 1 failed
Slowest: [Math] Fibonacci  (0.0034ms)
────────────────────────────────────────
```

---

## Requirements

- C++20 or later
- CMake 3.20+
- A compiler with `std::format` support (GCC 13+, Clang 16+, MSVC 19.29+)

---

## Installation

Clone the repository and add it as a subdirectory in your CMake project:

```bash
git clone https://github.com/DeanWilsonDev/Cimmerian.git
```

```cmake
# CMakeLists.txt
add_subdirectory(Cimmerian)

add_executable(my_tests
  test/my.test.cpp
  test/test-main.cpp
)

target_link_libraries(my_tests PRIVATE cimmerian)
target_compile_features(my_tests PUBLIC cxx_std_20)
```

Install locally as library:

```
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local -DCIMMERIAN_BUILD_TESTS=OFF
cmake --build build
cmake --install build
```

### Optional features

Snapshot testing (`cimmerian/snapshot.hpp`) and visual regression testing
(`cimmerian/visual.hpp`) are off by default — they aren't compiled into the
library unless you ask for them, so most consumers pay no extra compile time
for either. Enable whichever you need at configure time, independently:

```
cmake -B build -DCIMMERIAN_ENABLE_SNAPSHOT_TESTING=ON -DCIMMERIAN_ENABLE_VISUAL_TESTING=ON
```

Including `cimmerian/snapshot.hpp` or `cimmerian/visual.hpp` without enabling
the matching option fails fast at compile time with a `#error` pointing at
the flag to set, rather than an obscure link error.

---

## Quick Start

**1.Either:**

- A. Write a test entry point:

```cpp
// test/test-main.cpp
#include "cimmerian/test.hpp"

int main(int argc, char* argv[]) {
  Cimmerian::TestModeRegistry::GetInstance().ParseArgs(argc, argv); // --forbid-only / --forbid-skip
  Cimmerian::TestRunner runner;
  auto summary = runner.RunAll(&Cimmerian::TestRegistry::GetInstance());
  return summary.failed > 0 ? 1 : 0;
}
```

- B. Use the provided entry point:

```cpp
// test/test-main.cpp
#include <cimmerian/test-entry-point.hpp>
```


**2. Write your tests**

```cpp
// test/math.test.cpp
#include "cimmerian/test.hpp"
#include <vector>

DESCRIBE("Math", {
  IT("adds two numbers", {
    ASSERT_EQUAL(1 + 1, 2);
  });

  IT("compares vectors", {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b = {1, 2, 3};
    ASSERT_EQUAL(a, b);
  });
});
```

**3. Build and run**

```bash
cmake -B build
cmake --build build
./build/my_tests
```

---

## Authoring Tests

### `DESCRIBE` — group your tests

Groups can be nested arbitrarily. Each `DESCRIBE` inside another creates a child group.

```cpp
DESCRIBE("User", {
  DESCRIBE("Authentication", {
    IT("accepts a valid token", { ... });
    IT("rejects an expired token", { ... });
  });

  DESCRIBE("Profile", {
    IT("returns the correct username", { ... });
  });
});
```

### `IT` / `TEST` — define a test case

`IT` and `TEST` are aliases — use whichever reads better.

```cpp
IT("returns zero for empty input", {
  ASSERT_EQUAL(compute(""), 0);
});
```

### `IT_FN` / `TEST_FN` — register a function as a test

Useful for table-driven or shared test logic.

```cpp
void myTest(void*) {
  ASSERT_TRUE(someCondition());
}

IT_FN("my test", myTest);
```

### Lifecycle hooks

```cpp
DESCRIBE("Database", {
  BEFORE_ALL({ db_connect(); });
  AFTER_ALL({ db_disconnect(); });
  BEFORE_EACH({ db_clear(); });
  AFTER_EACH({ db_reset(); });

  IT("inserts a record", { ... });
  IT("finds a record", { ... });
});
```

| Hook | Runs |
|---|---|
| `BEFORE_ALL` | Once before all tests in the group |
| `AFTER_ALL` | Once after all tests in the group |
| `BEFORE_EACH` | Before every test in the group |
| `AFTER_EACH` | After every test in the group |

### Skipping and focusing tests

Append `_SKIP` or `_ONLY` to any test or group macro.

```cpp
DESCRIBE("Parser", {
  IT_SKIP("handles unicode", { ... });   // never runs, printed as [SKIP]
  IT_ONLY("handles commas", { ... });    // focus: only _ONLY tests run
});

DESCRIBE_SKIP("Legacy", { ... });        // skips every test inside, nested groups included
DESCRIBE_ONLY("Tokenizer", { ... });     // focuses every test inside
```

| Base | Variants |
|---|---|
| `DESCRIBE` | `DESCRIBE_SKIP`, `DESCRIBE_ONLY` |
| `TEST` / `IT` | `TEST_SKIP`, `TEST_ONLY`, `IT_SKIP`, `IT_ONLY` |
| `TEST_FN` / `IT_FN` | `TEST_FN_SKIP`, `TEST_FN_ONLY`, `IT_FN_SKIP`, `IT_FN_ONLY` |
| `VISUAL_DESCRIBE` | `VISUAL_DESCRIBE_SKIP`, `VISUAL_DESCRIBE_ONLY` |
| `VISUAL_DESCRIBE_COMPONENT` | `VISUAL_DESCRIBE_COMPONENT_SKIP`, `VISUAL_DESCRIBE_COMPONENT_ONLY` |
| `VISUAL_TEST` | `VISUAL_TEST_SKIP`, `VISUAL_TEST_ONLY` |

- A single `_ONLY` anywhere in the test binary, unit or visual, focuses the
  whole run. Every other test is skipped without being printed, and the
  summary notes that the run was focused.
- `_SKIP` always wins: a test that is both skipped and focused (e.g. an
  `IT_ONLY` inside a `DESCRIBE_SKIP`) is skipped and does not focus the run.
- Hooks don't run for skipped tests. A group's `BEFORE_ALL`/`AFTER_ALL` run
  only if at least one test inside it runs.
- Skipped tests count toward the total and are reported as skipped:
  `Summary: 10 total, 7 passed, 0 failed, 3 skipped`.

#### Guarding against committed markers in CI

```bash
./build/my_tests --forbid-only --forbid-skip
```

| Flag | Effect |
|---|---|
| `--forbid-only` | Fails the run if any test is marked `_ONLY` (directly or via `DESCRIBE_ONLY`). `_ONLY` also stops focusing the run. |
| `--forbid-skip` | Fails the run if any test is marked `_SKIP` (directly or via `DESCRIBE_SKIP`). |

When a forbidden test is found, no tests in that runner (unit or visual)
are run. The runner lists every offending test with its full group path and
counts each one as a failure, so the process exits non-zero:

```
[ERROR] 2 forbidden test(s) found, not running any tests:
  Parser > handles commas  (_ONLY is forbidden by --forbid-only)
  Legacy > old path  (_SKIP is forbidden by --forbid-skip)
```

The provided entry point (`cimmerian/test-entry-point.hpp`) parses both
flags. A hand-written `main` needs to call
`Cimmerian::TestModeRegistry::GetInstance().ParseArgs(argc, argv)`, as in
the Quick Start example.

---

## Assertions

### Continuing assertions

These report failure and continue running the rest of the test.

| Macro | Fails when |
|---|---|
| `ASSERT_TRUE(cond)` | `cond` is false |
| `ASSERT_FALSE(cond)` | `cond` is true |
| `ASSERT_EQUAL(a, b)` | `a != b` |
| `ASSERT_NOT_EQUAL(a, b)` | `a == b` |

### Halting assertions

These report failure and immediately stop the current test via `return`.

| Macro | Fails when |
|---|---|
| `REQUIRE_TRUE(cond)` | `cond` is false |
| `REQUIRE_EQUAL(a, b)` | `a != b` |

---

## Diff Output

`ASSERT_EQUAL` produces a visual diff on failure. Differing elements are highlighted with bright colour and underline. Missing elements appear as `∅`. Extra elements appear with strikethrough.

**Scalars**
```
  + 42
  - 43
```

**Strings — character level**
```
  + "hello world"
  - "hello wurld"
```

**Containers — element level**
```
  + [1, 2, 3, 4]
  - [1, 2, 9, ∅]
```

Diff is supported for scalars, `std::string`, `const char*`, C-style arrays, and any iterable container whose elements implement `std::format`.

---

## Performance Timing

Every test is timed automatically. No configuration required.

```
[PASS] [Math] Addition  (0.0005ms)
[PASS] [Math] Fibonacci  (0.0034ms)
[Math] group total: 0.0039ms

────────────────────────────────────────
Summary: 2 total, 2 passed, 0 failed
Slowest: [Math] Fibonacci (0.0034ms)
────────────────────────────────────────
```

Timing is reported per test, per group, for the total suite, and highlights the slowest test in the summary.

---

## Project Structure

```
Cimmerian/
├── include/cimmerian/       — public headers
│   ├── test.hpp             — single include entry point
│   ├── test-registry.hpp    — test tree registration
│   ├── test-runner.hpp      — execution and timing
│   ├── test-assertions.hpp  — typed assertions and diff output
│   ├── test-log.hpp         — coloured terminal logging
│   └── ansi-formatter.hpp   — ANSI colour helpers
├── src/                     — implementation files
├── test/                    — Cimmerian's own self-tests
└── CMakeLists.txt
```

---

## Contributing

Cimmerian will not be accepting contributions at this time. Feel free to raise an issue if you have a problem and I will try to resolve it quickly. Thank you.

---

## License

MIT — see [LICENSE](LICENSE) for details.
