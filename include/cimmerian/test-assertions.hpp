#pragma once
#include "test-fail-handler-registry.hpp"
#include "ansi-formatter.hpp"
#include <algorithm>
#include <concepts>
#include <iostream>
#include <string>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <format>
#include <span>

namespace Cimmerian::Assertions {

template <typename T>
concept StringLike = std::convertible_to<const T&, std::string_view>;

template <typename T>
concept Formattable = std::semiregular<std::formatter<std::decay_t<T>, char>>;

template <typename T>
concept Iterable = requires(T& t) {
  { std::begin(t) } -> std::input_iterator;
  { std::end(t) } -> std::input_iterator;
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

inline void OutputDiffToStderr(const std::string& expectedLine, const std::string& receivedLine)
{
  std::cerr << "  " << Ansi::AnsiFormatter::ExpectedPrefix() << " " << expectedLine << "\n";
  std::cerr << "  " << Ansi::AnsiFormatter::ReceivedPrefix() << " " << receivedLine << "\n";
}

inline void OutputStringDiff(const std::string& expectedString, const std::string& receivedString)
{
  std::string expectedFormatted;
  std::string receivedFormatted;

  const std::size_t expectedLength = expectedString.size();
  const std::size_t receivedLength = receivedString.size();
  const std::size_t commonLength = std::min(expectedLength, receivedLength);

  for (std::size_t charIndex = 0; charIndex < commonLength; ++charIndex) {
    const char expectedChar = expectedString[charIndex];
    const char receivedChar = receivedString[charIndex];

    if (expectedChar == receivedChar) {
      expectedFormatted += expectedChar;
      receivedFormatted += receivedChar;
    }
    else {
      expectedFormatted += Ansi::AnsiFormatter::DiffExpected(std::string(1, expectedChar));
      receivedFormatted += Ansi::AnsiFormatter::DiffReceived(std::string(1, receivedChar));
    }
  }

  if (expectedLength > receivedLength) {
    const std::size_t missingCount = expectedLength - receivedLength;
    for (std::size_t i = 0; i < missingCount; ++i) {
      expectedFormatted +=
          Ansi::AnsiFormatter::DiffExpected(std::string(1, expectedString[receivedLength + i]));
    }
    receivedFormatted += Ansi::AnsiFormatter::DiffMissing(missingCount);
  }

  if (receivedLength > expectedLength) {
    std::string extraChars = receivedString.substr(expectedLength);
    receivedFormatted += Ansi::AnsiFormatter::DiffExtra(extraChars);
  }

  OutputDiffToStderr("\"" + expectedFormatted + "\"", "\"" + receivedFormatted + "\"");
}

template <Iterable ContainerA, Iterable ContainerB>
void OutputContainerDiff(const ContainerA& expectedContainer, const ContainerB& receivedContainer)
{
  const auto expectedSize =
      std::distance(std::begin(expectedContainer), std::end(expectedContainer));
  const auto receivedSize =
      std::distance(std::begin(receivedContainer), std::end(receivedContainer));
  const auto commonSize = std::min(expectedSize, receivedSize);

  auto expectedIterator = std::begin(expectedContainer);
  auto receivedIterator = std::begin(receivedContainer);

  std::string expectedFormatted = "[";
  std::string receivedFormatted = "[";

  bool expectedNeedsLeadingComma = false;
  bool receivedNeedsLeadingComma = false;

  for (std::ptrdiff_t elementIndex = 0; elementIndex < commonSize; ++elementIndex) {
    const auto& expectedElement = *expectedIterator;
    const auto& receivedElement = *receivedIterator;

    if (expectedNeedsLeadingComma)
      expectedFormatted += ", ";
    if (receivedNeedsLeadingComma)
      receivedFormatted += ", ";

    if (expectedElement == receivedElement) {
      expectedFormatted += FormatValue(expectedElement);
      receivedFormatted += FormatValue(receivedElement);
    }
    else {
      expectedFormatted += Ansi::AnsiFormatter::DiffExpected(FormatValue(expectedElement));
      receivedFormatted += Ansi::AnsiFormatter::DiffReceived(FormatValue(receivedElement));
    }

    expectedNeedsLeadingComma = true;
    receivedNeedsLeadingComma = true;

    ++expectedIterator;
    ++receivedIterator;
  }

  // Elements expected has that received is missing
  for (std::ptrdiff_t missingIndex = commonSize; missingIndex < expectedSize; ++missingIndex) {
    if (expectedNeedsLeadingComma)
      expectedFormatted += ", ";
    if (receivedNeedsLeadingComma)
      receivedFormatted += ", ";

    expectedFormatted += Ansi::AnsiFormatter::DiffExpected(FormatValue(*expectedIterator));
    receivedFormatted += Ansi::AnsiFormatter::DiffMissing(1);

    expectedNeedsLeadingComma = true;
    receivedNeedsLeadingComma = true;

    ++expectedIterator;
  }

  // Elements received has that expected does not
  for (std::ptrdiff_t extraIndex = commonSize; extraIndex < receivedSize; ++extraIndex) {
    if (receivedNeedsLeadingComma)
      receivedFormatted += ", ";

    // expected side gets nothing — no placeholder, no comma advance beyond the bracket
    receivedFormatted += Ansi::AnsiFormatter::DiffExtra(FormatValue(*receivedIterator));

    receivedNeedsLeadingComma = true;

    ++receivedIterator;
  }

  expectedFormatted += "]";
  receivedFormatted += "]";

  OutputDiffToStderr(expectedFormatted, receivedFormatted);
}

inline void fail(const char* file, int line, const char* msg)
{
  std::cerr << file << ":" << line << ": ASSERTION FAILED: " << msg << "\n";
  TestFailHandlerRegistry::GetInstance().NotifyTestFail(file, line, msg);
}

template <typename TValue, typename TEpsilon>
inline void
assert_near(TValue actual, TValue expected, TEpsilon epsilon, const char* file, int line)
{
  if (std::abs(actual - expected) > epsilon) {
    fail(file, line, "Values not within epsilon:");
    OutputDiffToStderr(
        Ansi::AnsiFormatter::DiffExpected(FormatValue(expected) + " ± " + FormatValue(epsilon)),
        Ansi::AnsiFormatter::DiffReceived(FormatValue(actual))
    );
  }
}

template <typename A, typename B>
  requires(!Iterable<A> && !Iterable<B> && !(StringLike<A> && StringLike<B>))
void assert_equal_impl(const A& expectedValue, const B& receivedValue, const char* file, int line)
{
  if (!(expectedValue == receivedValue)) {
    fail(file, line, "Values differ:");
    OutputDiffToStderr(
        Ansi::AnsiFormatter::DiffExpected(FormatValue(expectedValue)),
        Ansi::AnsiFormatter::DiffReceived(FormatValue(receivedValue))
    );
  }
}

template <StringLike ExpectedString, StringLike ReceivedString>
void assert_equal_impl(
    const ExpectedString& expectedString,
    const ReceivedString& receivedString,
    const char* file,
    int line
)
{
  const std::string_view expectedText{expectedString};
  const std::string_view receivedText{receivedString};

  if (expectedText != receivedText) {
    fail(file, line, "Strings differ:");
    OutputStringDiff(std::string(expectedText), std::string(receivedString));
  }
}

template <class T, std::size_t expectedArraySize, class U, std::size_t receivedArraySize>
  requires(!std::same_as<std::remove_cv_t<T>, char>)
void assert_equal_impl(
    const T (&expectedArray)[expectedArraySize],
    const U (&receivedArray)[receivedArraySize],
    const char* file,
    int line
)
{
  if constexpr (expectedArraySize != receivedArraySize) {
    fail(file, line, "Array sizes differ:");
  }
  std::span expectedSpan(expectedArray, expectedArraySize);
  std::span receivedSpan(receivedArray, receivedArraySize);
  bool arraysAreEqual =
      (expectedArraySize == receivedArraySize) &&
      std::equal(std::begin(expectedArray), std::end(expectedArray), std::begin(receivedArray));

  if (!arraysAreEqual) {
    fail(
        file, line,
        expectedArraySize != receivedArraySize ? "Array sizes differ:" : "Arrays differ:"
    );
    OutputContainerDiff(expectedSpan, receivedSpan);
  }
}

template <Iterable ContainerA, Iterable ContainerB>
  requires(!(StringLike<ContainerA> && StringLike<ContainerB>))
void assert_equal_impl(
    const ContainerA& expectedContainer,
    const ContainerB& receivedContainer,
    const char* file,
    int line
)
{
  const auto expectedSize =
      std::distance(std::begin(expectedContainer), std::end(expectedContainer));
  const auto receivedSize =
      std::distance(std::begin(receivedContainer), std::end(receivedContainer));

  bool containersAreEqual =
      (expectedSize == receivedSize) &&
      std::equal(
          std::begin(expectedContainer), std::end(expectedContainer), std::begin(receivedContainer)
      );

  if (!containersAreEqual) {
    fail(file, line, "Containers differ:");
    OutputContainerDiff(expectedContainer, receivedContainer);
  }
}

template <class A, class B>
void assert_equal(const A& expected, const B& received, const char* file, int line)
{
  assert_equal_impl(expected, received, file, line);
}

template <class A, class B>
void assert_not_equal(const A& expected, const B& received, const char* file, int line)
{
  if (expected == received) {
    fail(file, line, "Expected values to differ but they were equal:");
    OutputDiffToStderr(FormatValue(expected), FormatValue(received));
  }
}

template <typename TExceptionType, typename TCallable>
inline void assert_throws(TCallable&& callable, const char* expression, const char* file, int line)
{
  bool did_throw = false;
  try {
    callable();
  }
  catch (const TExceptionType&) {
    did_throw = true;
  }
  catch (...) {
    TestFailHandlerRegistry::GetInstance().NotifyTestFail(
        file, line,
        (std::string("ASSERT_THROWS failed: ") + expression + " threw an unexpected exception type")
            .c_str()
    );
    return;
  }
  if (!did_throw)
    TestFailHandlerRegistry::GetInstance().NotifyTestFail(
        file, line, (std::string("ASSERT_THROWS failed: ") + expression + " did not throw").c_str()
    );
}

} // namespace Cimmerian::Assertions
