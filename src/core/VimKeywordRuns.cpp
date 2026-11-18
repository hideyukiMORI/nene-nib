#include "VimKeywordRuns.hpp"

#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VirtualColumn.hpp"

namespace nenenib::core
{
namespace
{
constexpr std::uint32_t blank_group = 0;

// ASCII の上限（これより小さいバイトは 1 バイトの code point）。
constexpr unsigned char ascii_end = 0x80U;

// line の at から始まる code point（ASCII は decode を呼ばずにそのバイト）。
[[nodiscard]] char32_t code_at(std::string_view line, std::size_t at) noexcept
{
    const auto byte = static_cast<unsigned char>(line[at]);
    return byte < ascii_end ? char32_t{byte} : code_point_at(line, Offset{at});
}

// line の at の次の code point の先頭。
[[nodiscard]] std::size_t after(std::string_view line, std::size_t at) noexcept
{
    return static_cast<unsigned char>(line[at]) < ascii_end
               ? at + 1
               : next_code_point(line, Offset{at}).value;
}

// 1 行（改行を含まない）の語を runs に足す。base は content の中の行の始まり。
// code point は 1 回だけ読む。行頭でない幅 0 の code point は手前の 1 文字に付け
// （vim_character_end と同じ切れ方）、1 文字の種類はその最初の code point で決める（#1872）。
void add_line_runs(std::string_view line, std::size_t base, std::vector<OffsetRange> &runs)
{
    std::uint32_t open = blank_group;
    std::size_t start = 0;
    for (std::size_t at = 0; at < line.size(); at = after(line, at))
    {
        const char32_t code = code_at(line, at);
        if (at > 0 && display_width(code) == DisplayWidth::zero)
        {
            continue;
        }
        const std::uint32_t group = vim_character_class(code, VimWordClass::word);
        if (group == open)
        {
            continue;
        }
        if (vim_keyword_group(open))
        {
            runs.push_back(OffsetRange{Offset{base + start}, Offset{base + at}});
        }
        open = group;
        start = at;
    }
    if (vim_keyword_group(open))
    {
        runs.push_back(OffsetRange{Offset{base + start}, Offset{base + line.size()}});
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
