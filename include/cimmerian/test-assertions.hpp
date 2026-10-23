#pragma once
#include "test-fail-handler-registry.hpp"
#include "ansi-formatter.hpp"
#include <algorithm>
#include <concepts>
#include <iterator>
#include <string>
#include <string_view>
#include <cstddef>
#include <exception>
#include <format>
#include <span>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace Cimmerian::Assertions {

// Every public assertion takes (actual, expected).
//   ASSERT_EQUAL(lexer.TextOf(token), "signal");
//                ^ actual             ^ expected
// The output helpers take (expected, actual), matching the order the two lines
// are printed in.
//
// Each assert_* returns whether it passed, so a REQUIRE_ macro can stop the
// test on failure without evaluating its arguments a second time.

template <typename T>
concept StringLike = std::convertible_to<const T&, std::string_view>;

template <typename T>
concept Formattable = std::semiregular<std::formatter<std::decay_t<T>, char>>;

template <typename T>
concept Iterable = requires(T& t) {
  { std::begin(t) } -> std::forward_iterator;
  { std::end(t) } -> std::forward_iterator;
};

template <typename T> std::string FormatValue(const T& value)
{
  if constexpr (Formattable<T>) {
    return std::format("{}", value);
  }
  else if constexpr (std::is_enum_v<T>) {
    return std::format("{}({})", typeid(T).name(), static_cast<std::underlying_type_t<T>>(value));
  }
  else {
    return std::format("<{}: no std::formatter>", typeid(T).name());
  }
}

inline std::vector<std::string>
FormatDiff(const std::string& expectedLine, const std::string& actualLine)
{
  return {
      Ansi::AnsiFormatter::ExpectedPrefix() + "  " + expectedLine,
      Ansi::AnsiFormatter::ReceivedPrefix() + "  " + actualLine,
  };
}

inline std::vector<std::string>
FormatStringDiff(const std::string& expectedString, const std::string& actualString)
{
  std::string expectedFormatted;
  std::string actualFormatted;

  const std::size_t expectedLength = expectedString.size();
  const std::size_t actualLength = actualString.size();
  const std::size_t commonLength = std::min(expectedLength, actualLength);

  // Group consecutive matching/differing characters into runs and wrap each
  // run once, rather than wrapping every character individually - a handful
  // of colored spans reads far better than one ANSI code per character.
  std::size_t runStart = 0;
  while (runStart < commonLength) {
    const bool runIsDiff = expectedString[runStart] != actualString[runStart];

    std::size_t runEnd = runStart + 1;
    while (runEnd < commonLength && (expectedString[runEnd] != actualString[runEnd]) == runIsDiff) {
      ++runEnd;
    }

    const std::string expectedRun = expectedString.substr(runStart, runEnd - runStart);
    const std::string actualRun = actualString.substr(runStart, runEnd - runStart);

    if (runIsDiff) {
      expectedFormatted += Ansi::AnsiFormatter::DiffExpected(expectedRun);
      actualFormatted += Ansi::AnsiFormatter::DiffReceived(actualRun);
    }
    else {
      expectedFormatted += expectedRun;
      actualFormatted += actualRun;
    }

    runStart = runEnd;
  }

  if (expectedLength > actualLength) {
    expectedFormatted += Ansi::AnsiFormatter::DiffExpected(expectedString.substr(actualLength));
    actualFormatted += Ansi::AnsiFormatter::DiffMissing(expectedLength - actualLength);
  }

  if (actualLength > expectedLength) {
    actualFormatted += Ansi::AnsiFormatter::DiffExtra(actualString.substr(expectedLength));
  }

  return FormatDiff("\"" + expectedFormatted + "\"", "\"" + actualFormatted + "\"");
}

// Two containers compared element by element: the diff lines, and what a
// reader would otherwise count brackets to find out.
struct ContainerComparison {
  std::vector<std::string> diffLines;
  std::ptrdiff_t expectedSize = 0;
  std::ptrdiff_t actualSize = 0;
  std::ptrdiff_t firstDifference = -1; // the first index that differs, or -1 if none does
  std::ptrdiff_t differingIndexes = 0; // among the indexes both containers have
};

template <Iterable ExpectedContainer, Iterable ActualContainer>
ContainerComparison CompareContainers(
    const ExpectedContainer& expectedContainer,
    const ActualContainer& actualContainer
)
{
  ContainerComparison comparison;
  const auto expectedSize =
      std::distance(std::begin(expectedContainer), std::end(expectedContainer));
  const auto actualSize = std::distance(std::begin(actualContainer), std::end(actualContainer));
  const auto commonSize = std::min(expectedSize, actualSize);

  auto expectedIterator = std::begin(expectedContainer);
  auto actualIterator = std::begin(actualContainer);

  std::string expectedFormatted = "[";
  std::string actualFormatted = "[";
  bool expectedNeedsLeadingComma = false;
  bool actualNeedsLeadingComma = false;

  for (std::ptrdiff_t elementIndex = 0; elementIndex < commonSize; ++elementIndex) {
    const auto& expectedElement = *expectedIterator;
    const auto& actualElement = *actualIterator;

    if (expectedNeedsLeadingComma)
      expectedFormatted += ", ";
    if (actualNeedsLeadingComma)
      actualFormatted += ", ";

    if (expectedElement == actualElement) {
      expectedFormatted += FormatValue(expectedElement);
      actualFormatted += FormatValue(actualElement);
    }
    else {
      if (comparison.firstDifference < 0) {
        comparison.firstDifference = elementIndex;
      }
      comparison.differingIndexes++;
      expectedFormatted += Ansi::AnsiFormatter::DiffExpected(FormatValue(expectedElement));
      actualFormatted += Ansi::AnsiFormatter::DiffReceived(FormatValue(actualElement));
    }

    expectedNeedsLeadingComma = true;
    actualNeedsLeadingComma = true;
    ++expectedIterator;
    ++actualIterator;
  }

  // Elements expected has that actual is missing
  for (std::ptrdiff_t missingIndex = commonSize; missingIndex < expectedSize; ++missingIndex) {
    if (expectedNeedsLeadingComma)
      expectedFormatted += ", ";
    if (actualNeedsLeadingComma)
      actualFormatted += ", ";

    expectedFormatted += Ansi::AnsiFormatter::DiffExpected(FormatValue(*expectedIterator));
    actualFormatted += Ansi::AnsiFormatter::DiffMissing(1);

    expectedNeedsLeadingComma = true;
    actualNeedsLeadingComma = true;
    ++expectedIterator;
  }

  // Elements actual has that expected does not
  for (std::ptrdiff_t extraIndex = commonSize; extraIndex < actualSize; ++extraIndex) {
    if (actualNeedsLeadingComma)
      actualFormatted += ", ";

    actualFormatted += Ansi::AnsiFormatter::DiffExtra(FormatValue(*actualIterator));
    actualNeedsLeadingComma = true;
    ++actualIterator;
  }

  expectedFormatted += "]";
  actualFormatted += "]";

  if (comparison.firstDifference < 0 && expectedSize != actualSize) {
    comparison.firstDifference = commonSize;
  }
  comparison.expectedSize = expectedSize;
  comparison.actualSize = actualSize;
  comparison.diffLines = FormatDiff(expectedFormatted, actualFormatted);
  return comparison;
}

template <Iterable ExpectedContainer, Iterable ActualContainer>
std::vector<std::string> FormatContainerDiff(
    const ExpectedContainer& expectedContainer,
    const ActualContainer& actualContainer
)
{
  return CompareContainers(expectedContainer, actualContainer).diffLines;
}

// The one-line reason for a failed container comparison, naming the sizes
// when they differ and the first index that differs:
//
//   Containers differ at index 4:
//   Containers differ at 3 indexes, first at index 4:
//   Containers differ in size (expected 2, received 3), first at index 2:
//
// noun replaces "Containers", as in "Arrays".
inline std::string
DescribeContainerDifference(std::string_view noun, const ContainerComparison& comparison)
{
  if (comparison.firstDifference < 0) {
    return std::format("{} differ:", noun);
  }
  if (comparison.expectedSize != comparison.actualSize) {
    return std::format(
        "{} differ in size (expected {}, received {}), first at index {}:", noun,
        comparison.expectedSize, comparison.actualSize, comparison.firstDifference
    );
  }
  if (comparison.differingIndexes == 1) {
    return std::format("{} differ at index {}:", noun, comparison.firstDifference);
  }
  return std::format(
      "{} differ at {} indexes, first at index {}:", noun, comparison.differingIndexes,
      comparison.firstDifference
  );
}

inline void
fail(const char* file, int line, std::string message, std::vector<std::string> details = {})
{
  TestFailHandlerRegistry::GetInstance().NotifyTestFail(
      {.file = file, .line = line, .message = std::move(message), .details = std::move(details)}
  );
}

template <typename TValue, typename TEpsilon>
inline bool
assert_near(TValue actual, TValue expected, TEpsilon epsilon, const char* file, int line)
{
  const TValue difference = (actual > expected) ? (actual - expected) : (expected - actual);

  if (difference > epsilon) {
    fail(
        file, line, "Values not within epsilon:",
        FormatDiff(
            Ansi::AnsiFormatter::DiffExpected(FormatValue(expected) + " ± " + FormatValue(epsilon)),
            Ansi::AnsiFormatter::DiffReceived(FormatValue(actual))
        )
    );
    return false;
  }
  return true;
}

template <typename A, typename B>
  requires(!Iterable<A> && !Iterable<B> && !(StringLike<A> && StringLike<B>))
bool assert_equal_impl(const A& actualValue, const B& expectedValue, const char* file, int line)
{
  if (!(actualValue == expectedValue)) {
    fail(
        file, line, "Values differ:",
        FormatDiff(
            Ansi::AnsiFormatter::DiffExpected(FormatValue(expectedValue)),
            Ansi::AnsiFormatter::DiffReceived(FormatValue(actualValue))
        )
    );
    return false;
  }
  return true;
}

template <StringLike ActualString, StringLike ExpectedString>
bool assert_equal_impl(
    const ActualString& actualString,
    const ExpectedString& expectedString,
    const char* file,
    int line
)
{
  const std::string_view actualText{actualString};
  const std::string_view expectedText{expectedString};

  if (actualText != expectedText) {
    fail(
        file, line,
        "Strings differ:", FormatStringDiff(std::string(expectedText), std::string(actualText))
    );
    return false;
  }
  return true;
}

template <class T, std::size_t actualArraySize, class U, std::size_t expectedArraySize>
  requires(!std::same_as<std::remove_cv_t<T>, char>)
bool assert_equal_impl(
    const T (&actualArray)[actualArraySize],
    const U (&expectedArray)[expectedArraySize],
    const char* file,
    int line
)
{
  const std::span actualSpan(actualArray, actualArraySize);
  const std::span expectedSpan(expectedArray, expectedArraySize);

  const bool arraysAreEqual =
      (actualArraySize == expectedArraySize) &&
      std::equal(std::begin(actualArray), std::end(actualArray), std::begin(expectedArray));

  if (!arraysAreEqual) {
    ContainerComparison comparison = CompareContainers(expectedSpan, actualSpan);
    fail(
        file, line, DescribeContainerDifference("Arrays", comparison),
        std::move(comparison.diffLines)
    );
    return false;
  }
  return true;
}

template <Iterable ActualContainer, Iterable ExpectedContainer>
  requires(!(StringLike<ActualContainer> && StringLike<ExpectedContainer>))
bool assert_equal_impl(
    const ActualContainer& actualContainer,
    const ExpectedContainer& expectedContainer,
    const char* file,
    int line
)
{
  const auto actualSize = std::distance(std::begin(actualContainer), std::end(actualContainer));
  const auto expectedSize =
      std::distance(std::begin(expectedContainer), std::end(expectedContainer));

  const bool containersAreEqual =
      (actualSize == expectedSize) &&
      std::equal(
          std::begin(actualContainer), std::end(actualContainer), std::begin(expectedContainer)
      );

  if (!containersAreEqual) {
    ContainerComparison comparison = CompareContainers(expectedContainer, actualContainer);
    fail(
        file, line, DescribeContainerDifference("Containers", comparison),
        std::move(comparison.diffLines)
    );
    return false;
  }
  return true;
}

template <class A, class B>
bool assert_equal(const A& actual, const B& expected, const char* file, int line)
{
  return assert_equal_impl(actual, expected, file, line);
}

template <typename A, typename B>
  requires(!Iterable<A> && !Iterable<B> && !(StringLike<A> && StringLike<B>))
bool assert_not_equal_impl(const A& actualValue, const B& expectedValue, const char* file, int line)
{
  if (actualValue == expectedValue) {
    fail(
        file, line, "Expected values to differ but they were equal:",
        {"both were: " + Ansi::AnsiFormatter::DiffReceived(FormatValue(actualValue))}
    );
    return false;
  }
  return true;
}

template <StringLike ActualString, StringLike ExpectedString>
bool assert_not_equal_impl(
    const ActualString& actualString,
    const ExpectedString& expectedString,
    const char* file,
    int line
)
{
  const std::string_view actualText{actualString};
  const std::string_view expectedText{expectedString};

  if (actualText == expectedText) {
    fail(
        file, line, "Expected strings to differ but they were equal:",
        {"both were: \"" + Ansi::AnsiFormatter::DiffReceived(std::string(actualText)) + "\""}
    );
    return false;
  }
  return true;
}

template <Iterable ActualContainer, Iterable ExpectedContainer>
  requires(!(StringLike<ActualContainer> && StringLike<ExpectedContainer>))
bool assert_not_equal_impl(
    const ActualContainer& actualContainer,
    const ExpectedContainer& expectedContainer,
    const char* file,
    int line
)
{
  const auto actualSize = std::distance(std::begin(actualContainer), std::end(actualContainer));
  const auto expectedSize =
      std::distance(std::begin(expectedContainer), std::end(expectedContainer));

  const bool containersAreEqual =
      (actualSize == expectedSize) &&
      std::equal(
          std::begin(actualContainer), std::end(actualContainer), std::begin(expectedContainer)
      );

  if (containersAreEqual) {
    fail(
        file, line, "Expected containers to differ but they were equal:",
        FormatContainerDiff(expectedContainer, actualContainer)
    );
    return false;
  }
  return true;
}

template <class A, class B>
bool assert_not_equal(const A& actual, const B& expected, const char* file, int line)
{
  return assert_not_equal_impl(actual, expected, file, line);
}

template <typename TExceptionType, typename TCallable>
inline bool assert_throws(
    TCallable&& callable,
    const char* macroName,
    const char* expression,
    const char* file,
    int line
)
{
  try {
    callable();
  }
  catch (const TExceptionType&) {
    return true;
  }
  catch (...) {
    fail(
        file, line,
        std::string(macroName) + " failed: " + expression + " threw an unexpected exception type"
    );
    return false;
  }

  fail(file, line, std::string(macroName) + " failed: " + expression + " did not throw");
  return false;
}

template <typename TCallable>
inline bool assert_no_throw(
    TCallable&& callable,
    const char* macroName,
    const char* expression,
    const char* file,
    int line
)
{
  try {
    callable();
  }
  catch (const std::exception& thrownException) {
    // The preformatted overload, so a multi-line what() becomes detail lines
    TestFailHandlerRegistry::GetInstance().NotifyTestFail(
        file, line,
        std::string(macroName) + " failed: " + expression + " threw: " + thrownException.what()
    );
    return false;
  }
  catch (...) {
    fail(
        file, line, std::string(macroName) + " failed: " + expression + " threw unknown exception"
    );
    return false;
  }
  return true;
}

} // namespace Cimmerian::Assertions
