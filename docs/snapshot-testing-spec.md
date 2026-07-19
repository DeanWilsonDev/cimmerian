# Snapshot Testing Extension — Spec

## Overview

This document specifies two complementary snapshot testing features for Cimmerian: **string snapshots** and **inline snapshots**. These are data-oriented and sit alongside the visual (screenshot) snapshot system, but are implemented independently — they have no dependency on platform capture or event injection.

The mental model mirrors Jest's snapshot system: serialize a value to a string, store that string as the golden, and fail the test if the serialized form changes. The difference between the two variants is where the golden lives.

| Variant | Golden lives | Best for |
|---|---|---|
| String snapshot | `.snap` file on disk, keyed by test + label | Large outputs, generated data, shared goldens |
| Inline snapshot | String literal in the test source file | Small values, self-documenting tests |
| Hash snapshot | `.snap` file on disk, just a hex hash | Large binary buffers where you don't need to read the diff |

---

## Serialization

Neither variant mandates a serialization format. The user is responsible for producing the string that gets stored. This keeps the API simple and avoids coupling Cimmerian to any particular reflection or JSON library.

```cpp
// User produces the string however they like:
ASSERT_STRING_SNAPSHOT(std::format("{}", myValue), "my-label");
ASSERT_INLINE_SNAPSHOT(myStruct.DebugString(), "");
ASSERT_HASH_SNAPSHOT(buffer.data(), buffer.size(), "buffer-label");
```

If `std::format` support exists for a type (via the existing `Formattable` concept), the helpers accept the value directly as a convenience:

```cpp
// Shorthand when the type is Formattable:
ASSERT_STRING_SNAPSHOT_FMT(myInt, "int-label");
ASSERT_INLINE_SNAPSHOT_FMT(myFloat, "");
```

These are thin wrappers that call `std::format("{}", value)` before passing to the core macro.

---

## String Snapshots (File-Based)

### How They Work

On first run (or in Update mode), the serialized string is written to a `.snap` file and the test passes. In Verify mode, the serialized string is compared against what's stored. Any difference fails the test and prints a character-level diff using the existing `OutputStringDiff` infrastructure.

### File Layout

Snapshots for a test file are stored in a `__snapshots__` directory adjacent to the test file:

```
test/
  my-feature.test.cpp
  __snapshots__/
    my-feature.test.snap
```

A single `.snap` file holds all snapshots for that test file, one per labelled entry. The format is human-readable and diffable:

```
# Snapshot: Rendering > default state
<snapshot content goes here, potentially multi-line>

# Snapshot: Rendering > with disabled flag
<snapshot content goes here>
```

The key is `<group path> > <label>`, built from the enclosing `DESCRIBE` names and the label passed to `ASSERT_STRING_SNAPSHOT`. This mirrors how Jest scopes snapshots to their test hierarchy.

### SnapshotKey

```cpp
struct SnapshotKey {
    std::string filePath;   // absolute path to the .test.cpp file
    std::string scopePath;  // "Group > Subgroup > ..."
    std::string label;      // the name passed to ASSERT_STRING_SNAPSHOT
};

std::string SnapshotKeyToString(const SnapshotKey& key);
// → "Group > Subgroup > label"
```

### StringSnapshotStore

```cpp
class StringSnapshotStore {
public:
    explicit StringSnapshotStore(const std::string& snapshotRootDir = "");
    // if snapshotRootDir is empty, derives path from the test file's __FILE__

    std::optional<std::string> Load(const SnapshotKey& key) const;
    void Save(const SnapshotKey& key, const std::string& value);
    void Flush();  // write all pending saves to disk (called at end of run)

private:
    // In-memory map of all loaded .snap files, keyed by file path.
    // Only reads each file once per run; batches all writes at Flush().
    std::unordered_map<std::string, SnapFile> loadedFiles;
};
```

Batching writes at `Flush()` means the `.snap` file is rewritten once per run rather than once per assertion, preserving the order of entries and avoiding partial-write corruption.

### Macros

```cpp
// Core: user provides the serialized string.
ASSERT_STRING_SNAPSHOT(serializedValue, label)

// Shorthand for Formattable types.
ASSERT_STRING_SNAPSHOT_FMT(value, label)

// Example usage:
DESCRIBE("Rendering", {
    TEST("default state", {
        auto result = renderer.Render(scene);
        ASSERT_STRING_SNAPSHOT(result.ToDebugString(), "full-output");
    });
});
```

On failure the output is:

```
test/my-feature.test.cpp:42: SNAPSHOT MISMATCH: "Rendering > full-output"
  Expected: "Frame{width=800, height=600, objects=3}"
  Received: "Frame{width=800, height=600, objects=4}"
                                                    ^
Run with --update-snapshots to accept the new value.
```

---

## Inline Snapshots

### How They Work

The snapshot string is stored as a string literal directly in the test source, as the second argument to `ASSERT_INLINE_SNAPSHOT`. An empty string signals "not yet captured". In Update mode the runner rewrites the source file to fill in (or replace) that argument. In Verify mode it compares the runtime value against the literal.

```cpp
// Before first capture:
ASSERT_INLINE_SNAPSHOT(myValue.ToString(), "");

// After running with --update-snapshots:
ASSERT_INLINE_SNAPSHOT(myValue.ToString(), R"(Point{x=10, y=20})");
```

This is the most readable form for small values — the expected output lives right next to the code that produces it, so it's immediately obvious what the test asserts.

### Source Rewriting

The rewriter runs after all tests have executed, in a single pass per file. The mechanism:

1. `ASSERT_INLINE_SNAPSHOT` captures `__FILE__` and `__LINE__` at the call site via the macro expansion.
2. During the test run, if Update mode is active, the runner records a `PendingInlineUpdate { filePath, line, newValue }` for every inline snapshot encountered.
3. After the run, the rewriter processes each file:
   - Read the file into memory.
   - For each pending update on that file (sorted by line, processed bottom-up so line numbers stay valid):
     - Locate the line.
     - Find the second string argument of `ASSERT_INLINE_SNAPSHOT` on that line using a simple token scan (look for the last string literal argument before the closing `)`.
     - Replace it with a raw string literal: `R"(<value>)"`.
   - Write the file back.

Processing bottom-up (last line first) is the key detail that makes multi-update rewrites safe — earlier lines are unaffected by changes to later lines.

**What if the value contains `)"` (which would break a raw string literal)?** Use a raw string delimiter: `R"delim(<value>)delim"`. The rewriter picks a delimiter that doesn't appear in the value (e.g., `snap`, `s1`, `s2`, ... incrementing until clean).

### Macro

```cpp
ASSERT_INLINE_SNAPSHOT(serializedValue, currentSnapshot)

// Shorthand for Formattable types:
ASSERT_INLINE_SNAPSHOT_FMT(value, currentSnapshot)
```

`currentSnapshot` is always a string literal (never a variable) — this is a compile-time constraint enforced by the macro expanding `__FILE__` and `__LINE__` alongside it.

### InlineSnapshotRewriter

```cpp
class InlineSnapshotRewriter {
public:
    void RecordUpdate(
        const std::string& filePath,
        int line,
        const std::string& newValue);

    // Called once at end of run. Rewrites all affected source files.
    void FlushAll();

private:
    struct PendingUpdate { int line; std::string newValue; };
    std::unordered_map<std::string, std::vector<PendingUpdate>> pendingByFile;

    void RewriteFile(
        const std::string& filePath,
        std::vector<PendingUpdate>& updates);

    static std::string MakeRawStringLiteral(const std::string& value);
    static std::string FindSafeDelimiter(const std::string& value);
};
```

### Multi-Line Values

Multi-line strings work naturally with raw string literals:

```cpp
ASSERT_INLINE_SNAPSHOT(tree.Print(), R"(
Node(root)
  Node(left)
  Node(right)
)");
```

The rewriter preserves the newlines. Verify mode compares the full string including newlines.

---

## Hash Snapshots

### How They Work

Rather than storing the serialized form, a hash snapshot stores only a fixed-length hex digest of the data. This is useful for large binary buffers (mesh data, audio, compressed assets) where a diff is meaningless but you still want to catch unintended changes.

On Update: hash the data, write the hex string to a `.snaphash` file. On Verify: hash again, compare hex strings. A mismatch fails the test but the output is minimal — just "hash changed" with no diff, because by definition you can't diff an opaque blob meaningfully.

### Hash Algorithm

FNV-1a (64-bit) is embedded as a header-only implementation — no external dependency. It's fast, deterministic, and sufficient for regression detection. The stored format is a 16-character lowercase hex string.

If the consumer needs a cryptographic hash (e.g., for asset integrity checking beyond testing), they can provide a custom `HashFn`:

```cpp
using HashFn = std::function<std::string(const void* data, std::size_t size)>;
```

### File Layout

Hash snapshots share the `__snapshots__` directory alongside `.snap` files:

```
test/
  __snapshots__/
    my-feature.test.snap       ← string snapshots
    my-feature.test.snaphash   ← hash snapshots
```

The `.snaphash` format is one entry per line:

```
Buffer > mesh-vertices: a3f1c2d4e5b6f7a8
Buffer > index-data: 1122334455667788
```

### Macros

```cpp
// Hash a raw memory region:
ASSERT_HASH_SNAPSHOT(dataPointer, byteSize, label)

// Hash a std::string or std::vector<uint8_t>:
ASSERT_HASH_SNAPSHOT_RANGE(container, label)

// With a custom hash function:
ASSERT_HASH_SNAPSHOT_FN(dataPointer, byteSize, label, hashFn)
```

---

## Run Mode Integration

All three snapshot types respond to the same run mode flags as the visual runner:

```
--update-snapshots    accept current output as new golden for all snapshot types
--snapshot-dir <p>   override the __snapshots__ root (default: adjacent to test file)
```

The `TestRunSummary` gains additional fields:

```cpp
struct SnapshotSummary {
    int snapshotsMatched   = 0;
    int snapshotsFailed    = 0;
    int snapshotsUpdated   = 0;   // in Update mode
    int snapshotsMissing   = 0;   // golden didn't exist, written on first run
    int inlineRewriteCount = 0;   // source files rewritten
};
```

---

## New Files

```
include/cimmerian/
  snapshot/
    snapshot-key.hpp
    string-snapshot-store.hpp
    hash-snapshot-store.hpp
    inline-snapshot-rewriter.hpp
    snapshot-macros.hpp          # ASSERT_STRING_SNAPSHOT, ASSERT_INLINE_SNAPSHOT, ASSERT_HASH_SNAPSHOT
  snapshot.hpp                   # single include

src/
  snapshot/
    snapshot-key.cpp
    string-snapshot-store.cpp
    hash-snapshot-store.cpp
    inline-snapshot-rewriter.cpp
    fnv1a.hpp                    # embedded hash, header-only
```

---

## Implementation Phases

### Phase 1 — String snapshots

- `SnapshotKey`, slug generation, `StringSnapshotStore` (load/save/flush)
- `.snap` file parser and writer
- `ASSERT_STRING_SNAPSHOT` macro wired into the existing `TestRunner` fail path
- Unit tests for the store using synthetic `.snap` files

### Phase 2 — Inline snapshots

- `InlineSnapshotRewriter` with bottom-up multi-file rewriting
- Raw string literal generation and delimiter selection
- `ASSERT_INLINE_SNAPSHOT` macro capturing `__FILE__` / `__LINE__`
- Tests that actually rewrite source files in a temp directory

### Phase 3 — Hash snapshots

- `fnv1a.hpp` embedded implementation
- `HashSnapshotStore` and `.snaphash` file format
- `ASSERT_HASH_SNAPSHOT` family of macros
- Tests with known buffers against precomputed digests
