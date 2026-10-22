#include "cimmerian/snapshot.hpp"
#include "cimmerian/snapshot/atomic-file-write.hpp"
#include "cimmerian/test.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Cimmerian::Snapshot::HashSnapshotStore;
using Cimmerian::Snapshot::SnapshotKey;
using Cimmerian::Snapshot::StringSnapshotStore;

// A new, empty directory in the system temp directory, removed with
// everything in it when it goes out of scope.
class ScratchDir {
public:
  ScratchDir()
  {
    std::random_device random;
    const fs::path temp = fs::temp_directory_path();
    do {
      this->path = temp / ("cimmerian-snapshot-files-" + std::to_string(random()));
    } while (!fs::create_directory(this->path));
  }

  ~ScratchDir()
  {
    std::error_code ignored;
    fs::remove_all(this->path, ignored);
  }

  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;

  fs::path path;
};

std::string ReadFile(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& contents)
{
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << contents;
}

// The names of every file in dir, sorted, so a test can check that nothing
// but the snapshot file was left behind.
std::vector<std::string> FileNames(const fs::path& dir)
{
  std::vector<std::string> names;
  for (const fs::directory_entry& entry : fs::directory_iterator(dir)) {
    names.push_back(entry.path().filename().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

// Stands in for another process's copy of the file. If Flush writes the file,
// this text is replaced, so finding it afterwards shows Flush didn't write.
const std::string NOT_WRITTEN = "not written by Flush\n";

// The store files these keys land in are "<root>/files.test.snap" and
// "<root>/files.test.snaphash".
const SnapshotKey FIRST_KEY{"suite/files.test.cpp", "Group", "first"};
const SnapshotKey SECOND_KEY{"suite/files.test.cpp", "Group", "second"};

} // namespace

DESCRIBE("Snapshots", {
  DESCRIBE("String", {
    TEST("captures and matches a struct-like string", {
      std::string value = std::format("Frame{{width={}, height={}}}", 800, 600);
      ASSERT_STRING_SNAPSHOT(value, "frame-dims");
    });

    TEST("captures and matches via FMT shorthand", {
      int answer = 42;
      ASSERT_STRING_SNAPSHOT_FMT(answer, "answer");
    });
  });

  DESCRIBE("Inline", {
    TEST("captures and matches a small value", {
      std::string value = "Point{x=10, y=20}";
      ASSERT_INLINE_SNAPSHOT(value, R"(Point{x=10, y=20})");
    });
  });

  DESCRIBE("Hash", {
    TEST("captures and matches a buffer hash", {
      std::vector<uint8_t> buffer = {1, 2, 3, 4, 5, 6, 7, 8};
      ASSERT_HASH_SNAPSHOT_RANGE(buffer, "small-buffer");
    });
  });

  DESCRIBE("String snapshot files", {
    TEST("a run that only loads doesn't write the file", {
      ScratchDir scratch;
      {
        StringSnapshotStore setup(scratch.path.string());
        setup.Save(FIRST_KEY, "stored");
        setup.Flush();
      }

      StringSnapshotStore store(scratch.path.string());
      ASSERT_EQUAL(store.Load(FIRST_KEY).value_or("<missing>"), std::string("stored"));
      WriteFile(scratch.path / "files.test.snap", NOT_WRITTEN);
      store.Flush();

      ASSERT_EQUAL(ReadFile(scratch.path / "files.test.snap"), NOT_WRITTEN);
    });

    TEST("loading from a missing file doesn't create it", {
      ScratchDir scratch;
      StringSnapshotStore store(scratch.path.string());
      ASSERT_FALSE(store.Load(FIRST_KEY).has_value());
      store.Flush();

      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{});
    });

    TEST("saving a new entry writes the file, leaving no temporary file", {
      ScratchDir scratch;
      StringSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "new value");
      store.Flush();

      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{"files.test.snap"});
      StringSnapshotStore reloaded(scratch.path.string());
      ASSERT_EQUAL(reloaded.Load(FIRST_KEY).value_or("<missing>"), std::string("new value"));
    });

    TEST("saving the value already stored doesn't write the file", {
      ScratchDir scratch;
      {
        StringSnapshotStore setup(scratch.path.string());
        setup.Save(FIRST_KEY, "stored");
        setup.Flush();
      }

      StringSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "stored");
      WriteFile(scratch.path / "files.test.snap", NOT_WRITTEN);
      store.Flush();

      ASSERT_EQUAL(ReadFile(scratch.path / "files.test.snap"), NOT_WRITTEN);
    });

    TEST("saving a changed value rewrites the file and keeps its other entries", {
      ScratchDir scratch;
      {
        StringSnapshotStore setup(scratch.path.string());
        setup.Save(FIRST_KEY, "old");
        setup.Save(SECOND_KEY, "untouched");
        setup.Flush();
      }

      StringSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "new");
      store.Flush();

      StringSnapshotStore reloaded(scratch.path.string());
      ASSERT_EQUAL(reloaded.Load(FIRST_KEY).value_or("<missing>"), std::string("new"));
      ASSERT_EQUAL(reloaded.Load(SECOND_KEY).value_or("<missing>"), std::string("untouched"));
    });

    TEST("a second Flush doesn't write a file the first one wrote", {
      ScratchDir scratch;
      StringSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "value");
      store.Flush();
      WriteFile(scratch.path / "files.test.snap", NOT_WRITTEN);
      store.Flush();

      ASSERT_EQUAL(ReadFile(scratch.path / "files.test.snap"), NOT_WRITTEN);
    });

    TEST("creates the snapshot directory when it's missing", {
      ScratchDir scratch;
      const fs::path root = scratch.path / "nested" / "__snapshots__";
      StringSnapshotStore store(root.string());
      store.Save(FIRST_KEY, "value");
      store.Flush();

      ASSERT_TRUE(fs::is_regular_file(root / "files.test.snap"));
    });
  });

  DESCRIBE("Hash snapshot files", {
    TEST("a run that only loads doesn't write the file", {
      ScratchDir scratch;
      {
        HashSnapshotStore setup(scratch.path.string());
        setup.Save(FIRST_KEY, "0123456789abcdef");
        setup.Flush();
      }

      HashSnapshotStore store(scratch.path.string());
      ASSERT_EQUAL(store.Load(FIRST_KEY).value_or("<missing>"), std::string("0123456789abcdef"));
      WriteFile(scratch.path / "files.test.snaphash", NOT_WRITTEN);
      store.Flush();

      ASSERT_EQUAL(ReadFile(scratch.path / "files.test.snaphash"), NOT_WRITTEN);
    });

    TEST("loading from a missing file doesn't create it", {
      ScratchDir scratch;
      HashSnapshotStore store(scratch.path.string());
      ASSERT_FALSE(store.Load(FIRST_KEY).has_value());
      store.Flush();

      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{});
    });

    TEST("saving a new hash writes the file, leaving no temporary file", {
      ScratchDir scratch;
      HashSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "0123456789abcdef");
      store.Flush();

      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{"files.test.snaphash"});
      HashSnapshotStore reloaded(scratch.path.string());
      ASSERT_EQUAL(reloaded.Load(FIRST_KEY).value_or("<missing>"), std::string("0123456789abcdef"));
    });

    TEST("saving the hash already stored doesn't write the file", {
      ScratchDir scratch;
      {
        HashSnapshotStore setup(scratch.path.string());
        setup.Save(FIRST_KEY, "0123456789abcdef");
        setup.Flush();
      }

      HashSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "0123456789abcdef");
      WriteFile(scratch.path / "files.test.snaphash", NOT_WRITTEN);
      store.Flush();

      ASSERT_EQUAL(ReadFile(scratch.path / "files.test.snaphash"), NOT_WRITTEN);
    });

    TEST("saving a changed hash rewrites the file and keeps its other entries", {
      ScratchDir scratch;
      {
        HashSnapshotStore setup(scratch.path.string());
        setup.Save(FIRST_KEY, "1111111111111111");
        setup.Save(SECOND_KEY, "2222222222222222");
        setup.Flush();
      }

      HashSnapshotStore store(scratch.path.string());
      store.Save(FIRST_KEY, "3333333333333333");
      store.Flush();

      HashSnapshotStore reloaded(scratch.path.string());
      ASSERT_EQUAL(reloaded.Load(FIRST_KEY).value_or("<missing>"), std::string("3333333333333333"));
      ASSERT_EQUAL(
          reloaded.Load(SECOND_KEY).value_or("<missing>"), std::string("2222222222222222")
      );
    });
  });

  DESCRIBE("Writing a file atomically", {
    using Cimmerian::Snapshot::WriteFileAtomically;

    TEST("replaces an existing file's contents", {
      ScratchDir scratch;
      const fs::path path = scratch.path / "file.txt";
      WriteFile(path, "old contents that are longer than the new ones\n");

      std::string error;
      ASSERT_TRUE(WriteFileAtomically(path, "new\n", &error));
      ASSERT_EQUAL(error, std::string(""));
      ASSERT_EQUAL(ReadFile(path), std::string("new\n"));
      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{"file.txt"});
    });

    TEST("fails with a reason, and no file, when the directory can't be created", {
      ScratchDir scratch;
      // A regular file where the directory should be.
      WriteFile(scratch.path / "blocker", "");

      std::string error;
      ASSERT_FALSE(WriteFileAtomically(scratch.path / "blocker" / "file.txt", "contents", &error));
      ASSERT_NOT_EQUAL(error, std::string(""));
      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{"blocker"});
    });

    TEST("fails with a reason, and no temporary file, when the rename fails", {
      ScratchDir scratch;
      // A non-empty directory where the file should be, so it can't be replaced.
      fs::create_directories(scratch.path / "file.txt" / "inside");

      std::string error;
      ASSERT_FALSE(WriteFileAtomically(scratch.path / "file.txt", "contents", &error));
      ASSERT_NOT_EQUAL(error, std::string(""));
      ASSERT_EQUAL(FileNames(scratch.path), std::vector<std::string>{"file.txt"});
      ASSERT_TRUE(fs::is_directory(scratch.path / "file.txt"));
    });
  });
});
