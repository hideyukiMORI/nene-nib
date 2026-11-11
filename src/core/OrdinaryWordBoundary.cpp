#include "OrdinaryWordBoundary.hpp"

#include "DisplayWidth.hpp"
#include "LineNumber.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VirtualColumn.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 種類の値は Vim の表の値（0 空白・1 記号・2 英数字・それ以外は塊の印）に、通常モードだけの 2 つを
// 足したもの。Tab と半角カタカナは Vim の表に無い区別なので、表の値と重ならない印を使う。
constexpr std::uint32_t blank_group = 0;
constexpr std::uint32_t symbol_group = 1;
constexpr std::uint32_t word_group = 2;
constexpr std::uint32_t tab_group = 0xFFFF'FFFFU;
constexpr std::uint32_t halfwidth_kana_group = 0xFF66;
constexpr char32_t halfwidth_kana_first = 0xFF66;
constexpr char32_t halfwidth_kana_last = 0xFF9F;
constexpr char32_t right_single_quote = 0x2019;
constexpr char32_t zero_width_joiner = 0x200D;

// メモ帳（Office RichEdit）の実測に合わせた読み替え（#1394 の実測の記録 §2・§5）。
[[nodiscard]] std::uint32_t raw_group(char32_t code) noexcept
{
    if (code == U'\t')
    {
        return tab_group;
    }
    if (code == U'_')
    {
        return symbol_group;
    }
    if (code >= halfwidth_kana_first && code <= halfwidth_kana_last)
    {
        return halfwidth_kana_group;
    }
    return vim_character_class(code, VimWordClass::word);
}

// 前の文字に付く幅 0 の文字（結合文字・異体字セレクタ）と ZWJ（表の幅は unprintable だが、メモ帳は
// 前の語に付ける・記録 §5）。制御文字と Tab は付かない。
[[nodiscard]] bool joins_previous(char32_t code) noexcept
{
    const bool control = code < 0x20 || (code >= 0x7F && code <= 0x9F);
    return code == zero_width_joiner || (!control && display_width(code) == DisplayWidth::zero);
}

[[nodiscard]] bool apostrophe(char32_t code) noexcept
{
    return code == U'\'' || code == right_single_quote;
}

// at から始まる 1 文字（基底と後ろに続く幅 0 の文字）の終わり。
[[nodiscard]] std::size_t atom_end(std::string_view content, std::size_t at) noexcept
{
    const bool tab = code_point_at(content, Offset{at}) == U'\t';
    std::size_t end = next_code_point(content, Offset{at}).value;
    while (!tab && end < content.size() && joins_previous(code_point_at(content, Offset{end})))
    {
        end = next_code_point(content, Offset{end}).value;
    }
    return end;
}

// end で終わる 1 文字の頭。
[[nodiscard]] std::size_t atom_begin(std::string_view content, std::size_t end) noexcept
{
    std::size_t begin = previous_code_point(content, Offset{end}).value;
    while (begin > 0 && joins_previous(code_point_at(content, Offset{begin})))
    {
        const std::size_t before = previous_code_point(content, Offset{begin}).value;
        if (code_point_at(content, Offset{before}) == U'\t')
        {
            break;
        }
        begin = before;
    }
    return begin;
}

// [begin, end) の 1 文字の種類。英数字に挟まれた ' と ’ は英数字の語の中に数える。
[[nodiscard]] std::uint32_t atom_group(std::string_view content, std::size_t begin,
                                       std::size_t end) noexcept
{
    const char32_t base = code_point_at(content, Offset{begin});
    if (!apostrophe(base) || begin == 0 || end >= content.size())
    {
        return raw_group(base);
    }
    const std::size_t before = atom_begin(content, begin);
    const bool between = raw_group(code_point_at(content, Offset{before})) == word_group &&
                         raw_group(code_point_at(content, Offset{end})) == word_group;
    return between ? word_group : raw_group(base);
}

[[nodiscard]] std::uint32_t group_after(std::string_view content, std::size_t at) noexcept
{
    return atom_group(content, at, atom_end(content, at));
}

[[nodiscard]] std::uint32_t group_before(std::string_view content, std::size_t at) noexcept
{
    return atom_group(content, atom_begin(content, at), at);
}

[[nodiscard]] std::size_t skip_blanks(std::string_view content, std::size_t at) noexcept
{
    while (at < content.size() && group_after(content, at) == blank_group)
    {
        at = atom_end(content, at);
    }
    return at;
}

// 行の中で Ctrl+→ が着く位置。at は行の内容の終わりより前。
[[nodiscard]] std::size_t forward_in_line(std::string_view content, std::size_t at) noexcept
{
    const std::uint32_t group = group_after(content, at);
    if (group == tab_group)
    {
        return skip_blanks(content, atom_end(content, at));
    }
    if (group != blank_group)
    {
        while (at < content.size() && group_after(content, at) == group)
        {
            at = atom_end(content, at);
        }
    }
    return skip_blanks(content, at);
}

[[nodiscard]] std::size_t skip_blanks_back(std::string_view content, std::size_t at) noexcept
{
    while (at > 0 && group_before(content, at) == blank_group)
    {
        at = atom_begin(content, at);
    }
    return at;
}

// 行の中で Ctrl+← が戻る位置。at は前の空白を越えた後で、行頭より後ろ。
[[nodiscard]] std::size_t backward_in_line(std::string_view content, std::size_t at) noexcept
{
    const std::uint32_t group = group_before(content, at);
    if (group == tab_group)
    {
        return atom_begin(content, at);
    }
    while (at > 0 && group_before(content, at) == group)
    {
        at = atom_begin(content, at);
    }
    return at;
}

// 字下げの行頭から戻るときの着地（メモ帳の実測・記録 §2.3）。空の行は続けて越える。
[[nodiscard]] Offset previous_filled_line_end(const TextBuffer &text, LineNumber line)
{
    std::size_t number = line.value - 1;
    while (number > 1 && text.line_start(LineNumber{number}) == text.line_end(LineNumber{number}))
    {
        --number;
    }
    return text.line_end(LineNumber{number});
}

[[nodiscard]] Offset forward_boundary(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.position_of(at).line;
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (at.value >= end.value)
    {
        return line.value < text.line_count() ? text.line_start(LineNumber{line.value + 1}) : end;
    }
    const std::string content = text.text_range(start, end);
    return Offset{start.value + forward_in_line(content, at.value - start.value)};
}

[[nodiscard]] Offset backward_boundary(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.position_of(at).line;
    const Offset start = text.line_start(line);
    if (at.value <= start.value)
    {
        return line.value > 1 ? text.line_end(LineNumber{line.value - 1}) : start;
    }
    const std::string content = text.text_range(start, text.line_end(line));
    const std::size_t after_blanks = skip_blanks_back(content, at.value - start.value);
    if (after_blanks > 0)
    {
        return Offset{start.value + backward_in_line(content, after_blanks)};
    }
    // 前の空白を越えて行頭に着いた（字下げ）。先頭の行なら行頭で止まる。
    return line.value > 1 ? previous_filled_line_end(text, line) : start;
}
} // namespace

Offset ordinary_word_boundary(const TextBuffer &text, Offset at, OrdinaryWordDirection direction)
{
    switch (direction)
    {
    case OrdinaryWordDirection::backward:
        return backward_boundary(text, at);
    case OrdinaryWordDirection::forward:
        return forward_boundary(text, at);
    }
    std::unreachable();
}
} // namespace nenenib::core
