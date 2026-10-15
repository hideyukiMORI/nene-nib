#include "DetectedText.hpp"
#include "NewlineIndex.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "simd/AsciiNewlineScan.hpp"
#include "simd/CpuFeatures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::core::DetectedText;
using nenenib::core::NewlineIndex;
using nenenib::core::TextBuffer;
using nenenib::core::simd::AsciiNewlineScan;

constexpr std::array<std::size_t, 18> lengths{0,  1,  15, 16,  17,  31,  32,  33,  63,
                                              64, 65, 95, 100, 511, 512, 513, 575, 1025};
constexpr std::array<std::size_t, 6> bit_offsets{0, 1, 31, 33, 63, 100};

// x64 で必ず走る scalar と sse2 を名指しで回す。avx2 は CPU を OS に尋ねられる adapter の試験が
// scalar と照合する（ADR 0006・__builtin_cpu_supports は __cpu_model が未解決でリンクできない）。
constexpr std::array<AsciiNewlineScan, 2> scans{AsciiNewlineScan::scalar, AsciiNewlineScan::sse2};

[[nodiscard]] std::size_t reference_ascii_end(std::string_view text, std::size_t index)
{
    while (index < text.size() && static_cast<unsigned char>(text.at(index)) < 0x80U)
    {
        ++index;
    }
    return index;
}

// CRLF の並びと、32 byte の block と 64 bit の word の境目にまたがる LF。
[[nodiscard]] std::string crlf_text(std::size_t length)
{
    std::string text;
    for (std::size_t at = 0; at < length; ++at)
    {
        text.push_back(at % 2 == 0 ? '\r' : '\n');
    }
    return text;
}

[[nodiscard]] std::string straddling_text(std::size_t length)
{
    std::string text(length, 'x');
    for (std::size_t at = 0; at < length; ++at)
    {
        const bool edge = at % 31 == 30 || at % 32 == 31 || at % 64 == 0;
        text.at(at) = edge ? '\n' : 'x';
    }
    return text;
}

[[nodiscard]] std::vector<std::string> samples()
{
    std::vector<std::string> result;
    for (const auto length : lengths)
    {
        result.emplace_back(length, 'x');
        result.emplace_back(length, '\n');
        result.emplace_back(length, '\r');
        result.push_back(crlf_text(length));
        result.push_back(straddling_text(length));
    }
    return result;
}

void verify_ascii_end(AsciiNewlineScan scan, std::string_view text)
{
    for (std::size_t from = 0; from <= text.size(); ++from)
    {
        expect(core::simd::ascii_end(scan, text, from) == reference_ascii_end(text, from),
               "every scan stops at the first non-ASCII byte");
    }
}

[[nodiscard]] std::vector<std::uint64_t> reference_words(std::string_view text, std::size_t bit)
{
    std::vector<std::uint64_t> words((bit + text.size()) / 64 + 1, 0);
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        const std::uint64_t found = text.at(at) == '\n' ? 1U : 0U;
        words.at((bit + at) / 64) |= found << ((bit + at) % 64);
    }
    return words;
}

void verify_marks(AsciiNewlineScan scan, std::string_view text)
{
    const auto count = static_cast<std::size_t>(std::ranges::count(text, '\n'));
    for (const auto bit : bit_offsets)
    {
        const auto expected = reference_words(text, bit);
        std::vector<std::uint64_t> words(expected.size(), 0);
        expect(core::simd::mark_newlines(scan, text, words, bit) == count,
               "every scan counts each LF once");
        expect(words == expected, "every scan places LF bits across word boundaries");
    }
}

void verify_mixed(AsciiNewlineScan scan)
{
    for (const auto length : {40U, 70U, 600U})
    {
        for (std::size_t at = 0; at + 3 <= length; at += 7)
        {
            std::string text(length, 'a');
            text.replace(at, 3, "日");
            text.at(length - 1) = '\n';
            verify_ascii_end(scan, text);
            verify_marks(scan, text);
            expect(core::validate_utf8(text, scan) == core::validate_utf8(text),
                   "validation with a scan agrees when non-ASCII sits inside a block");
            std::string broken = text;
            broken.at(at) = '\xFF';
            expect(!core::validate_utf8(broken, scan).has_value(),
                   "an invalid lead byte after an ASCII run is rejected by every scan");
        }
    }
}

void verify_index_and_buffer(AsciiNewlineScan scan, const std::string &text)
{
    const auto plain = NewlineIndex::from_text(text);
    const auto scanned = NewlineIndex::from_text(text, scan);
    expect(scanned.size() == plain.size(), "scanned index keeps the LF count");
    for (std::size_t at = 0; at <= text.size(); ++at)
    {
        expect(scanned.count_before(at) == plain.count_before(at),
               "scanned index keeps rank at every byte and block boundary");
    }
    const auto detected = DetectedText::from_bytes(text, scan);
    expect(detected.has_value(), "ASCII samples are detected as UTF-8 by every scan");
    const auto buffer = TextBuffer::from_utf8(detected.value(), scan);
    expect(buffer.has_value() && buffer.value().line_count() == plain.size() + 1 &&
               buffer.value().text() == text,
           "a buffer opened through a scan keeps text and lines");
}
} // namespace

void verify_ascii_newline_scan_scope()
{
    expect(core::simd::select_ascii_newline_scan(core::simd::CpuFeatures{.avx2 = true}) ==
               AsciiNewlineScan::avx2,
           "AVX2 is chosen when the composition root reports it");
    expect(core::simd::select_ascii_newline_scan(core::simd::CpuFeatures{}) ==
               AsciiNewlineScan::sse2,
           "x64 always has the SSE2 scan");
    const auto texts = samples();
    for (const auto scan : scans)
    {
        for (const auto &text : texts)
        {
            verify_ascii_end(scan, text);
            verify_marks(scan, text);
            verify_index_and_buffer(scan, text);
        }
        verify_mixed(scan);
    }
}
} // namespace nenenib::tests
