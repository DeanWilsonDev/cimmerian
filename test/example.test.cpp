#include "umbra/test.hpp"

DESCRIBE("Example Test", {
  DESCRIBE("Math", { TEST("Add", { ASSERT_EQUAL(1, 1); }); });
  DESCRIBE("Arrays", {
    TEST("Compare C Style Array", {
      int arr1[] = {1, 2};
      int arr2[] = {1, 2};

      ASSERT_EQUAL(arr1, arr2);
    });
    TEST("Compare Vectors", {
      std::vector<int> arr1 = {1, 2, 3};
      std::vector<int> arr2 = {1, 2};

      ASSERT_EQUAL(arr1, arr2);
    });
  });
});
