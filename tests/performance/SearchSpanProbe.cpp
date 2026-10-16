#include "SearchSpanProbe.hpp"

#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"
#include "VimSearch.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t span_lines = 200000U;
// 1 行の内容は 83 バイト（改行を足して 84 バイト・最後の行は改行なし・16,799,999 バイト）。
constexpr std::size_t span_line_bytes = 83U;
constexpr std::string_view span_word = "zzzz";
// next の当たりは最後の行の末尾の 4 バイト（79 バイト目から・桁 80）。
constexpr std::size_t span_hit_column = span_line_bytes - span_word.size() + 1U;

[[nodiscard]] std::string span_line()
{
    constexpr std::string_view words = "lorem ipsum dolor sit amet ";
    std::string line;
    line.reserve(span_line_bytes);
    while (line.size() < span_line_bytes)
    {
        line += words.at(line.size() % words.size());
    }
    return line;
}
} // namespace

std::string search_span_input(SearchSpanWorkload workload)
{
    const std::string line = span_line();
    std::string input;
    input.reserve(span_lines * (span_line_bytes + 1U));
    for (std::size_t index = 0; index < span_lines; ++index)
    {
        input += line;
        input += index + 1U < span_lines ? "\n" : "";
    }
    if (workload == SearchSpanWorkload::next_16mib)
    {
        input.replace(input.size() - span_word.size(), span_word.size(), span_word);
    }
    return input;
}

std::expected<std::uint64_t, ProbeFailure> searched_spans(SearchSpanWorkload workload,
                                                          const std::string &input,
                                                          application::TimingPort &timing)
{
    const auto text = core::TextBuffer::from_utf8(input);
    const auto pattern = core::VimPattern::parse(span_word, core::VimSearchDirection::forward);
    if (!text.has_value() || !pattern.has_value() || text.value().line_count() != span_lines ||
        text.value().piece_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::VimMatchRequest request{
        {core::LineNumber{1U}, core::Column{1U}}, core::VimSearchDirection::forward, 1U};
    std::cerr << "preparedPieces=1 preparedHash=" << checksum_of(input) << '\n';
    timing.mark(core::Milestone::probe_started);
    const auto hit = core::vim_find_match(text.value(), pattern.value(), request);
    timing.mark(core::Milestone::probe_finished);
    if (workload == SearchSpanWorkload::miss_16mib)
    {
        return hit.has_value() ? std::unexpected(ProbeFailure::wrong_result)
                               : std::expected<std::uint64_t, ProbeFailure>{checksum_of(input)};
    }
    if (!hit.has_value() || hit.value().position.line.value != span_lines ||
        hit.value().position.column.value != span_hit_column || hit.value().wrapped)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + hit.value().position.line.value + hit.value().position.column.value;
}
} // namespace nenenib::tests::performance
