#pragma once
#include "umbra/test-fail-handler-registry.hpp"
#include "umbra/test-runner.hpp"
#include <algorithm>
#include <iostream>
#include <cstddef>
#include <cstring>

namespace Umbra {
namespace Test {
namespace Assertions {

template <class T>
concept Iterable = requires(T t) {
  { std::begin(t) } -> std::input_iterator;
  { std::end(t) } -> std::input_iterator;
};

inline void fail(const char* file, int line, const char* msg)
{
  std::cerr << file << ":" << line << ": ASSERTION FAILED: - " << msg << "\n";
  TestFailHandlerRegistry::GetInstance().NotifyTestFail(file, line, "");
}

// -------- Scalars ---------
template <class A, class B>
  requires(!Iterable<A> && !Iterable<B>)
void assert_equal_impl(const A& a, const B& b, const char* file, int line)
{
  if (!(a == b)) {
    fail(file, line, ("Values differ: " + std::to_string(a) + " != " + std::to_string(b)).c_str());
  }
}

// -------- C Style Arrays ---------
template <class T, std::size_t N, class U, std::size_t M>
void assert_equal_impl(const T (&a)[N], const U (&b)[M], const char* file, int line)
{
  if constexpr (N != M) {
    fail(
        file, line,
        ("Array sizes differ: " + std::to_string(N) + " != " + std::to_string(M)).c_str()
    );
    return;
  }
  for (std::size_t i = 0; i < N; ++i) {
    if (!(a[i] == b[i])) {
      fail(
          file, line,
          ("Array element [" + std::to_string(i) + "] differs: " + std::to_string(a[i]) +
           " != " + std::to_string(b[i]))
              .c_str()
      );
      return;
    }
  }
}

// -------- C Style Arrays ---------
template <Iterable A, Iterable B>
void assert_equal_impl(const A& a, const B& b, const char* file, int line)
{

  const auto size_a = std::distance(std::begin(a), std::end(a));
  const auto size_b = std::distance(std::begin(b), std::end(b));

  if (size_a != size_b) {
    fail(
        file, line,
        ("Contianer sizes differ: " + std::to_string(size_a) + " != " + std::to_string(size_b))
            .c_str()
    );
    return;
  }
  if (!std::equal(std::begin(a), std::end(a), std::begin(b))) {
    fail(file, line, "Container elements differ");
    return;
  }
}

// -------------- Public Facade ----------------
template <class A, class B>
void assert_equal(const A& a, const B& b, const char* file = __FILE__, int line = __LINE__)
{
  assert_equal_impl(a, b, file, line);
}

} // namespace Assertions

} // namespace Test

} // namespace Umbra
