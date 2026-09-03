#pragma once
#include "test-fail-handler-registry.hpp"
#include "ansi-formatter.hpp"
#include <algorithm>
#include <concepts>
#include <iterator>
#include <string>
#include <string_view>
#include <cstddef>
#include <format>
#include <span>
#include <type_traits>
#include <typeinfo>

namespace Cimmerian::Assertions {

// Every public assertion takes (actual, expected).
//   ASSERT_EQUAL(lexer.TextOf(token), "signal");
//                ^ actual             ^ expected
// The output helpers take (expected, actual), matching the order the two lines
// are printed in.

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

inline std::string FormatDiff(const std::string& expectedLine, const std::string& actualLine)
{
  return "  " + Ansi::AnsiFormatter::ExpectedPrefix() + " " + expectedLine + "\n";
  return "  " + Ansi::AnsiFormatter::ReceivedPrefix() + " " + actualLine;
}

inline std::string
FormatStringDiff(const std::string& expectedString, const std::string& actualString)
{
  std::string expectedFormatted;
  std::string actualFormatted;

  const std::size_t expectedLength = expectedString.size();
  const std::size_t actualLength = actualString.size();
  const std::size_t commonLength = std::min(expectedLength, actualLength);

  for (std::size_t charIndex = 0; charIndex < commonLength; ++charIndex) {
    const char expectedChar = expectedString[charIndex];
    const char actualChar = actualString[charIndex];

    if (expectedChar == actualChar) {
      expectedFormatted += expectedChar;
      actualFormatted += actualChar;
    }
    else {
      expectedFormatted += Ansi::AnsiFormatter::DiffExpected(std::string(1, expectedChar));
      actualFormatted += Ansi::AnsiFormatter::DiffReceived(std::string(1, actualChar));
    }
  }

  if (expectedLength > actualLength) {
    const std::size_t missingCount = expectedLength - actualLength;
    for (std::size_t missingIndex = 0; missingIndex < missingCount; ++missingIndex) {
      expectedFormatted += Ansi::AnsiFormatter::DiffExpected(
          std::string(1, expectedString[actualLength + missingIndex])
      );
    }
    actualFormatted += Ansi::AnsiFormatter::DiffMissing(missingCount);
  }

  if (actualLength > expectedLength) {
    const std::string extraChars = actualString.substr(expectedLength);
    actualFormatted += Ansi::AnsiFormatter::DiffExtra(extraChars);
  }

  return FormatDiff("\"" + expectedFormatted + "\"", "\"" + actualFormatted + "\"");
}

template <Iterable ExpectedContainer, Iterable ActualContainer>
std::string FormatContainerDiff(
    const ExpectedContainer& expectedContainer,
    const ActualContainer& actualContainer
)
{
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

  return FormatDiff(expectedFormatted, actualFormatted);
}

inline void fail(const char* file, int line, const std::string& msg)
{
  TestFailHandlerRegistry::GetInstance().NotifyTestFail(file, line, msg.c_str());
}

inline void fail(const char* file, int line, const char* msg)
{
  fail(file, line, std::string(msg));
}

inline std::string WithDetail(std::string_view message, const std::string& detail)
{
  return std::string(message) + "\n" + detail;
}

template <typename TValue, typename TEpsilon>
inline void
assert_near(TValue actual, TValue expected, TEpsilon epsilon, const char* file, int line)
{
  const TValue difference = (actual > expected) ? (actual - expected) : (expected - actual);

  if (difference > epsilon) {
    fail(
        file, line,
        WithDetail(
            "Values not within epsilon:",
            FormatDiff(
                Ansi::AnsiFormatter::DiffExpected(
                    FormatValue(expected) + " ± " + FormatValue(epsilon),
                    Ansi::AnsiFormatter::DiffReceived(FormatValue(actual))
                )
            )
        )
    );
  }
}

template <typename A, typename B>
  requires(!Iterable<A> && !Iterable<B> && !(StringLike<A> && StringLike<B>))
void assert_equal_impl(const A& actualValue, const B& expectedValue, const char* file, int line)
{
  if (!(actualValue == expectedValue)) {
    fail(
        file, line,
        WithDetail(
            "Values differ:", FormatDiff(
                                  Ansi::AnsiFormatter::DiffExpected(FormatValue(expectedValue)),
                                  Ansi::AnsiFormatter::DiffReceived(FormatValue(actualValue))
                              )
        )
    );
  }
}

template <StringLike ActualString, StringLike ExpectedString>
void assert_equal_impl(
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
        WithDetail(
            "Strings differ:", FormatStringDiff(std::string(expectedText), std::string(actualText))
        )
    );
  }
}

template <class T, std::size_t actualArraySize, class U, std::size_t expectedArraySize>
  requires(!std::same_as<std::remove_cv_t<T>, char>)
void assert_equal_impl(
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
    fail(
        file, line,
        WithDetail(
            actualArraySize != expectedArraySize ? "Array sizes differ:" : "Arrays differ:",
            FormatContainerDiff(expectedSpan, actualSpan)
        )
    );
  }
}

template <Iterable ActualContainer, Iterable ExpectedContainer>
  requires(!(StringLike<ActualContainer> && StringLike<ExpectedContainer>))
void assert_equal_impl(
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
    fail(
        file, line,
        WithDetail("Containers differ:", FormatContainerDiff(expectedContainer, actualContainer))
    );
  }
}

template <class A, class B>
void assert_equal(const A& actual, const B& expected, const char* file, int line)
{
  assert_equal_impl(actual, expected, file, line);
}

template <typename A, typename B>
  requires(!Iterable<A> && !Iterable<B> && !(StringLike<A> && StringLike<B>))
void assert_not_equal_impl(const A& actualValue, const B& expectedValue, const char* file, int line)
{
  if (actualValue == expectedValue) {
    fail(
        file, line,
        WithDetail(
            "Expected values to differ but they were equal:",
            " both were: " + Ansi::AnsiFormatter::DiffReceived(FormatValue(actualValue))
        )
    );
  }
}

template <StringLike ActualString, StringLike ExpectedString>
void assert_not_equal_impl(
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
        file, line,
        WithDetail(
            "Expected strings to differ but they were equal:",
            "  both were: \"" + Ansi::AnsiFormatter::DiffReceived(std::string(actualText)) + "\""
        )
    );
  }
}

template <Iterable ActualContainer, Iterable ExpectedContainer>
  requires(!(StringLike<ActualContainer> && StringLike<ExpectedContainer>))
void assert_not_equal_impl(
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
        file, line,
        WithDetail(
            "Expected containers to differ but they were equal:",
            FormatContainerDiff(expectedContainer, actualContainer)
        )
    );
  }
}

template <class A, class B>
void assert_not_equal(const A& actual, const B& expected, const char* file, int line)
{
  assert_not_equal_impl(actual, expected, file, line);
}

template <typename TExceptionType, typename TCallable>
inline void assert_throws(TCallable&& callable, const char* expression, const char* file, int line)
{
  bool didThrow = false;

  try {
    callable();
  }
  catch (const TExceptionType&) {
    didThrow = true;
  }
  catch (...) {
    const std::string message =
        std::string("ASSERT_THROWS failed: ") + expression + " threw an unexpected exception type";
    fail(file, line, message.c_str());
    return;
  }

  if (!didThrow) {
    const std::string message =
        std::string("ASSERT_THROWS failed: ") + expression + " did not throw";
    fail(file, line, message.c_str());
  }
}

} // namespace Cimmerian::Assertions
