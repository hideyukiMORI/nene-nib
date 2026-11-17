#include "VimKeywordRuns.hpp"

#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"

namespace nenenib::core
{
namespace
{
constexpr std::uint32_t blank_group = 0;

// 1 行（改行を含まない）の語を runs に足す。base は content の中の行の始まり。
void add_line_runs(std::string_view line, std::size_t base, std::vector<OffsetRange> &runs)
{
    std::size_t at = 0;
    while (at < line.size())
    {
        const std::uint32_t group = vim_keyword_class_at(line, at);
        std::size_t end = vim_character_end(line, Offset{at}).value;
        if (!vim_keyword_group(group))
        {
            at = end;
            continue;
        }
        while (end < line.size() && vim_keyword_class_at(line, end) == group)
        {
            end = vim_character_end(line, Offset{end}).value;
        }
        runs.push_back(OffsetRange{Offset{base + at}, Offset{base + end}});
        at = end;
    }
}
} // namespace

std::uint32_t vim_keyword_class_at(std::string_view content, std::size_t at) noexcept
{
    if (at >= content.size())
    {
        return blank_group;
    }
    return vim_character_class(code_point_at(content, Offset{at}), VimWordClass::word);
}

std::vector<OffsetRange> vim_keyword_runs(std::string_view content)
{
    std::vector<OffsetRange> runs;
    std::size_t start = 0;
    while (start <= content.size())
    {
        const std::size_t newline = content.find('\n', start);
        const std::size_t stop = newline == std::string_view::npos ? content.size() : newline;
        add_line_runs(content.substr(start, stop - start), start, runs);
        if (newline == std::string_view::npos)
        {
            break;
        }
        start = newline + 1;
    }
    return runs;
}

bool vim_keyword_has_two_characters(std::string_view content, const OffsetRange &range) noexcept
{
    const std::string_view word =
        content.substr(range.begin.value, range.end.value - range.begin.value);
    return vim_character_end(word, Offset{0}).value < word.size();
}
} // namespace nenenib::core
