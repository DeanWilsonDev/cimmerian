#include "cimmerian/snapshot.hpp"
#include "cimmerian/test.hpp"
#include <vector>

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
});
