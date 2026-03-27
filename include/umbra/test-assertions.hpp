#pragma once
#include "test-fail-handler-registry.hpp"
#include "ansi-formatter.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstddef>
#include <cstring>
#include <format>
#include <span>

namespace Cimmerian::Assertions {

template <typename T>
concept Formattable = requires(T t) {
    { std::format("{}", t) } -> std::convertible_to<std::string>;
};

template <typename T>
concept Iterable = requires(T t) {
    { std::begin(t) } -> std::input_iterator;
    { std::end(t) } -> std::input_iterator;
};

template <Formattable T>
std::string FormatValue(const T& value)
{
    return std::format("{}", value);
}

inline void OutputDiffToStderr(
    const std::string& expectedLine,
    const std::string& receivedLine
)
{
    std::cerr << "  " << AnsiFormatter::ExpectedPrefix() << " " << expectedLine << "\n";
    std::cerr << "  " << AnsiFormatter::ReceivedPrefix() << " " << receivedLine << "\n";
}

inline void OutputStringDiff(
    const std::string& expectedString,
    const std::string& receivedString
)
{
    std::string expectedFormatted;
    std::string receivedFormatted;

    const std::size_t expectedLength = expectedString.size();
    const std::size_t receivedLength = receivedString.size();
    const std::size_t commonLength   = std::min(expectedLength, receivedLength);

    for (std::size_t charIndex = 0; charIndex < commonLength; ++charIndex) {
        const char expectedChar = expectedString[charIndex];
        const char receivedChar = receivedString[charIndex];

        if (expectedChar == receivedChar) {
            expectedFormatted += expectedChar;
            receivedFormatted += receivedChar;
        } else {
            expectedFormatted += AnsiFormatter::DiffExpected(std::string(1, expectedChar));
            receivedFormatted += AnsiFormatter::DiffReceived(std::string(1, receivedChar));
        }
    }

    if (expectedLength > receivedLength) {
        const std::size_t missingCount = expectedLength - receivedLength;
        for (std::size_t i = 0; i < missingCount; ++i) {
            expectedFormatted += AnsiFormatter::DiffExpected(
                std::string(1, expectedString[receivedLength + i])
            );
        }
        receivedFormatted += AnsiFormatter::DiffMissing(missingCount);
    }

    if (receivedLength > expectedLength) {
        std::string extraChars = receivedString.substr(expectedLength);
        expectedFormatted += ""; 
        receivedFormatted += AnsiFormatter::DiffExtra(extraChars);
    }

    OutputDiffToStderr(
        "\"" + expectedFormatted + "\"",
        "\"" + receivedFormatted + "\""
    );
}

template <Iterable ContainerA, Iterable ContainerB>
void OutputContainerDiff(const ContainerA& expectedContainer, const ContainerB& receivedContainer)
{
    const auto expectedSize = std::distance(std::begin(expectedContainer), std::end(expectedContainer));
    const auto receivedSize = std::distance(std::begin(receivedContainer), std::end(receivedContainer));
    const auto commonSize   = std::min(expectedSize, receivedSize);

    auto expectedIterator = std::begin(expectedContainer);
    auto receivedIterator = std::begin(receivedContainer);

    std::string expectedFormatted = "[";
    std::string receivedFormatted = "[";

    for (std::ptrdiff_t elementIndex = 0; elementIndex < commonSize; ++elementIndex) {
        const auto& expectedElement = *expectedIterator;
        const auto& receivedElement = *receivedIterator;

        const bool elementsAreEqual = (expectedElement == receivedElement);
        const bool isLastCommonElement = (elementIndex == commonSize - 1)
            && (expectedSize == receivedSize);

        const std::string expectedElementString = FormatValue(expectedElement);
        const std::string receivedElementString = FormatValue(receivedElement);

        if (elementsAreEqual) {
            expectedFormatted += expectedElementString;
            receivedFormatted += receivedElementString;
        } else {
            expectedFormatted += AnsiFormatter::DiffExpected(expectedElementString);
            receivedFormatted += AnsiFormatter::DiffReceived(receivedElementString);
        }

        if (!isLastCommonElement) {
            expectedFormatted += ", ";
            receivedFormatted += ", ";
        }

        ++expectedIterator;
        ++receivedIterator;
    }

    for (std::ptrdiff_t missingIndex = commonSize; missingIndex < expectedSize; ++missingIndex) {
        if (missingIndex > 0) {
            expectedFormatted += ", ";
            receivedFormatted += ", ";
        }
        expectedFormatted += AnsiFormatter::DiffExpected(FormatValue(*expectedIterator));
        receivedFormatted += AnsiFormatter::DiffMissing(1);
        ++expectedIterator;
    }

    for (std::ptrdiff_t extraIndex = commonSize; extraIndex < receivedSize; ++extraIndex) {
        if (extraIndex > 0) {
            expectedFormatted += ", ";
            receivedFormatted += ", ";
        }
        expectedFormatted += ""; 
        receivedFormatted += AnsiFormatter::DiffExtra(FormatValue(*receivedIterator));
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

template <Formattable A, Formattable B>
    requires(!Iterable<A> && !Iterable<B>)
void assert_equal_impl(const A& expectedValue, const B& receivedValue, const char* file, int line)
{
    if (!(expectedValue == receivedValue)) {
        fail(file, line, "Values differ:");
        OutputDiffToStderr(
            AnsiFormatter::DiffExpected(FormatValue(expectedValue)),
            AnsiFormatter::DiffReceived(FormatValue(receivedValue))
        );
    }
}

inline void assert_equal_impl(
    const std::string& expectedString,
    const std::string& receivedString,
    const char* file,
    int line
)
{
    if (expectedString != receivedString) {
        fail(file, line, "Strings differ:");
        OutputStringDiff(expectedString, receivedString);
    }
}

inline void assert_equal_impl(
    const char* expectedString,
    const char* receivedString,
    const char* file,
    int line
)
{
    assert_equal_impl(
        std::string(expectedString),
        std::string(receivedString),
        file, line
    );
}

template <class T, std::size_t expectedArraySize, class U, std::size_t receivedArraySize>
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
    bool arraysAreEqual = (expectedArraySize == receivedArraySize)
        && std::equal(std::begin(expectedArray), std::end(expectedArray), std::begin(receivedArray));
    if (!arraysAreEqual) {
        fail(file, line, "Arrays differ:");
        OutputContainerDiff(expectedSpan, receivedSpan);
    }
}

template <Iterable ContainerA, Iterable ContainerB>
    requires(!std::is_same_v<ContainerA, std::string> && !std::is_same_v<ContainerB, std::string>)
void assert_equal_impl(
    const ContainerA& expectedContainer,
    const ContainerB& receivedContainer,
    const char* file,
    int line
)
{
    const auto expectedSize = std::distance(std::begin(expectedContainer), std::end(expectedContainer));
    const auto receivedSize = std::distance(std::begin(receivedContainer), std::end(receivedContainer));

    bool containersAreEqual = (expectedSize == receivedSize)
        && std::equal(std::begin(expectedContainer), std::end(expectedContainer), std::begin(receivedContainer));

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
        OutputDiffToStderr(
            FormatValue(expected),
            FormatValue(received)
        );
    }
}

} // namespace Cimmerian::Assertions
