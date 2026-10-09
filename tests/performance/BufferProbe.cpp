#include "BufferProbe.hpp"

#include "Milestone.hpp"
#include "TextBuffer.hpp"

#include <array>
#include <iostream>

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
} // namespace nenenib::tests::performance
