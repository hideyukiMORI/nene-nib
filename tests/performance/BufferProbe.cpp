#include "BufferProbe.hpp"

#include "Milestone.hpp"
#include "TextBuffer.hpp"

#include <array>
#include <iostream>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::string repeated(std::string_view block)
{
    std::string result;
    result.reserve(block.size() * 4096U);
    for (std::size_t index = 0; index < 4096U; ++index)
    {
        result += block;
    }
    return result;
}

[[nodiscard]] core::TextBuffer fragmented(core::TextBuffer text, std::size_t stride,
                                          std::size_t first)
{
    for (std::size_t index = 0; index < 4096U; ++index)
    {
        const std::size_t at = stride * index + first;
        text = text.replaced(core::Offset{at}, core::Offset{at + 1U}, "b");
    }
    return text;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
positions_of(const core::TextBuffer &text, std::string_view expected,
             application::TimingPort &timing)
{
    if (text.text() != expected || text.size_bytes() != 57344U || text.line_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto prepared_hash = checksum_of(text.text());
    std::cerr << "preparedPieces=" << text.piece_count() << " preparedHash=" << prepared_hash
              << '\n';
    std::array<core::TextPosition, 128> observed{};
    const core::Offset end{57344U};
    timing.mark(core::Milestone::probe_started);
    for (auto &position : observed)
    {
        position = text.position_of(end);
    }
    timing.mark(core::Milestone::probe_finished);
    for (const auto &position : observed)
    {
        if (position.line.value != 1U || position.column.value != 20481U)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
    }
    return prepared_hash ^ 128U ^ 20481U;
}
} // namespace

std::string crlf_buffer_input()
{
    return repeated(std::string(78U, 'a') + "\r\n");
}

std::string lf_buffer_input()
{
    return repeated(std::string(78U, 'a') + "\n");
}

std::string position_buffer_input()
{
    return repeated("a日本語🖋");
}

std::expected<std::uint64_t, ProbeFailure> queried_lines(const std::string &input,
                                                         application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || (input.size() != 327680U && input.size() != 323584U))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t stride = input.size() / 4096U;
    const auto text = fragmented(parsed.value(), stride, 1U);
    const std::string expected_line = "ab" + std::string(76U, 'a');
    const std::string expected = repeated(expected_line + (stride == 80U ? "\r\n" : "\n"));
    if (text.piece_count() != 8193U || text.text() != expected || text.line_count() != 4097U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto prepared_hash = checksum_of(text.text());
    std::cerr << "preparedPieces=" << text.piece_count() << " preparedHash=" << prepared_hash
              << '\n';
    std::array<std::string, 30> observed{};
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < observed.size(); ++index)
    {
        observed[index] = text.line_text(core::LineNumber{index + 1U});
    }
    timing.mark(core::Milestone::probe_finished);
    std::uint64_t result = prepared_hash;
    for (const auto &line : observed)
    {
        if (line != expected_line)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        result += checksum_of(line);
    }
    return result;
}

std::expected<std::uint64_t, ProbeFailure> queried_long_position(const std::string &input,
                                                                 application::TimingPort &timing)
{
    const auto text = core::TextBuffer::from_utf8(input);
    if (!text.has_value() || text.value().piece_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return positions_of(text.value(), repeated("a日本語🖋"), timing);
}

std::expected<std::uint64_t, ProbeFailure>
queried_scattered_position(const std::string &input, application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input.size() != 57344U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto text = fragmented(parsed.value(), 14U, 0U);
    if (text.piece_count() != 8192U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return positions_of(text, repeated("b日本語🖋"), timing);
}
namespace
{
[[nodiscard]] bool prepared_equals(const core::TextBuffer &text, std::string_view expected,
                                   std::size_t pieces)
{
    return text.text() == expected && text.size_bytes() == expected.size() &&
           text.piece_count() == pieces;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
erased_results(const core::TextBuffer &text, core::Offset begin, core::Offset end,
               application::TimingPort &timing)
{
    const std::string original = text.text();
    std::string expected = original;
    expected.erase(begin.value, end.value - begin.value);
    std::vector<core::TextBuffer> observed;
    observed.reserve(16U);
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < 16U; ++index)
    {
        observed.emplace_back(text.erase(begin, end));
    }
    timing.mark(core::Milestone::probe_finished);
    std::uint64_t checksum = checksum_of(original);
    for (const auto &result : observed)
    {
        const std::string bytes = result.text();
        if (bytes != expected || result.size_bytes() != expected.size() ||
            result.line_count() != (expected.empty() ? 1U : 4097U) ||
            result.line_ending() != core::LineEnding::crlf)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        checksum += checksum_of(bytes) + result.size_bytes() + result.line_count();
    }
    if (text.text() != original)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
offsets_of(const core::TextBuffer &text, const core::TextPosition &position, core::Offset expected,
           application::TimingPort &timing)
{
    const std::string original = text.text();
    std::array<core::Offset, 128> observed{};
    timing.mark(core::Milestone::probe_started);
    for (auto &offset : observed)
    {
        offset = text.offset_of(position);
    }
    timing.mark(core::Milestone::probe_finished);
    std::uint64_t checksum = checksum_of(original);
    for (const auto offset : observed)
    {
        if (offset.value != expected.value)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        checksum += offset.value;
    }
    if (text.text() != original)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace

std::expected<std::uint64_t, ProbeFailure>
erased_buffer(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input != crlf_buffer_input())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const bool single = workload == ProbeWorkload::erase_single_middle;
    const auto text = single ? parsed.value() : fragmented(parsed.value(), 80U, 1U);
    const std::string expected = single ? input : repeated("ab" + std::string(76U, 'a') + "\r\n");
    if (!prepared_equals(text, expected, single ? 1U : 8193U) || text.line_count() != 4097U ||
        text.line_ending() != core::LineEnding::crlf)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    constexpr std::array<std::size_t, 5> begins{0U, 163840U, 327677U, 0U, 163840U};
    constexpr std::array<std::size_t, 5> ends{1U, 163841U, 327678U, 327680U, 163841U};
    const std::size_t index = static_cast<std::size_t>(workload) -
                              static_cast<std::size_t>(ProbeWorkload::erase_scattered_head);
    std::cerr << "preparation=" << (single ? "single" : "replace(80*i+1,1,b);i=0..4095")
              << " preparedPieces=" << text.piece_count()
              << " preparedHash=" << checksum_of(expected) << '\n';
    return erased_results(text, core::Offset{begins.at(index)}, core::Offset{ends.at(index)},
                          timing);
}

std::expected<std::uint64_t, ProbeFailure>
queried_offset(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input != position_buffer_input())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const bool scattered = workload == ProbeWorkload::offset_scattered_middle;
    const auto text = scattered ? fragmented(parsed.value(), 14U, 0U) : parsed.value();
    const std::string expected = scattered ? repeated("b日本語🖋") : input;
    if (!prepared_equals(text, expected, scattered ? 8192U : 1U) || text.line_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    constexpr std::array<std::size_t, 4> columns{2U, 10241U, 20481U, 10241U};
    constexpr std::array<std::size_t, 4> offsets{1U, 28672U, 57344U, 28672U};
    const std::size_t index = static_cast<std::size_t>(workload) -
                              static_cast<std::size_t>(ProbeWorkload::offset_long_head);
    const core::TextPosition position{core::LineNumber{1U}, core::Column{columns.at(index)}};
    std::cerr << "preparation=" << (scattered ? "replace(14*i,1,b);i=0..4095" : "single")
              << " preparedPieces=" << text.piece_count()
              << " preparedHash=" << checksum_of(expected) << '\n';
    return offsets_of(text, position, core::Offset{offsets.at(index)}, timing);
}
} // namespace nenenib::tests::performance
