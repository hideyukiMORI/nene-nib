#include "OrdinaryWordBoundary.hpp"

#include "DisplayWidth.hpp"
#include "LineNumber.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
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
// 最初に読む窓（キャレットの前後のバイト数）と、UTF-8 の 1 code point の最長。
constexpr std::size_t first_window_bytes = 4096;
constexpr std::size_t longest_code_point_bytes = 4;

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

// 窓の先頭の続きのバイトの数。窓の先頭が code point の途中なら、その code point は窓に入れない。
[[nodiscard]] std::size_t leading_continuations(std::string_view window) noexcept
{
    std::size_t skipped = 0;
    while (skipped < window.size() &&
           (static_cast<unsigned char>(window.at(skipped)) & 0xC0U) == 0x80U)
    {
        ++skipped;
    }
    return skipped;
}

// 窓の読みのうち、両端で code point を割らない部分の [頭, 終わり)。行の端で切っていない側は削らない
// （#1292 の whole_code_points）。
[[nodiscard]] std::pair<std::size_t, std::size_t> whole_part(std::string_view read, bool head_cut,
                                                             bool tail_cut) noexcept
{
    const std::size_t skipped = head_cut ? leading_continuations(read) : 0;
    const std::size_t kept = tail_cut ? whole_code_points(read) : read.size();
    return {skipped, std::max(kept, skipped)};
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

// 窓 content の中の Ctrl+→ の着地。判断が窓の端に掛かった（前の 1 文字が窓の外から始まるかも
// しれない・着いた所の 1 文字が窓の終わりで切れるかもしれない）ときは空で、窓を広げて読み直す。
[[nodiscard]] std::optional<std::size_t>
forward_in_window(std::string_view content, std::size_t caret, bool head_cut, bool tail_cut)
{
    // 前の 1 文字（' の判定が見る）が窓の中で始まっていること。
    if ((head_cut && atom_begin(content, caret) == 0) || caret >= content.size())
    {
        return std::nullopt;
    }
    const std::size_t landed = forward_in_line(content, caret);
    // 着いた所の 1 文字（止まった判断が見た）が窓の中で終わっていること。
    if (tail_cut && atom_end(content, landed) >= content.size())
    {
        return std::nullopt;
    }
    return landed;
}

// 窓 content の中の Ctrl+← の着地。前の空白を越えて窓の頭（= 行頭）に着いたら字下げの印
// indented_line_start。判断が窓の頭に掛かった（止まった所の手前の 1 文字と、それが ' なら
// さらに手前の 1 文字が窓の外から始まるかもしれない）ときは空で、窓を広げて読み直す。
constexpr std::size_t indented_line_start = static_cast<std::size_t>(-1);

[[nodiscard]] std::optional<std::size_t> backward_in_window(std::string_view content,
                                                            std::size_t caret, bool head_cut)
{
    const std::size_t after_blanks = skip_blanks_back(content, caret);
    if (after_blanks == 0)
    {
        return head_cut ? std::nullopt : std::optional<std::size_t>{indented_line_start};
    }
    const std::size_t landed = backward_in_line(content, after_blanks);
    if (!head_cut)
    {
        return landed;
    }
    const std::size_t before = atom_begin(content, landed);
    if (landed == 0 || before == 0 || atom_begin(content, before) == 0)
    {
        return std::nullopt;
    }
    return landed;
}

// 行の全体は写さず、キャレットの周りの窓だけを読む（1 打鍵ごとに呼ばれる・#1394 のレビュー M1）。
// 判断が窓の端に掛かったときだけ窓を倍にして読み直すので、答えは行の全体で歩いた答えと同じ。
[[nodiscard]] Offset forward_boundary(const TextBuffer &text, Offset at, std::size_t window)
{
    const LineNumber line = text.position_of(at).line;
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (at.value >= end.value)
    {
        return line.value < text.line_count() ? text.line_start(LineNumber{line.value + 1}) : end;
    }
    for (std::size_t width = window;; width *= 2)
    {
        const Offset from{at.value - std::min(width, at.value - start.value)};
        const Offset until{at.value + std::min(width, end.value - at.value)};
        const std::string read = text.text_range(from, until);
        const bool head_cut = !(from == start);
        const auto [skipped, kept] = whole_part(read, head_cut, !(until == end));
        const std::string_view content = std::string_view{read}.substr(skipped, kept - skipped);
        const auto landed =
            forward_in_window(content, at.value - from.value - skipped, head_cut, !(until == end));
        if (landed.has_value())
        {
            return Offset{from.value + skipped + landed.value()};
        }
    }
}

// 行の中（at は行頭より後ろ）の Ctrl+← の着地。
[[nodiscard]] Offset backward_windowed(const TextBuffer &text, LineNumber line, Offset at,
                                       std::size_t window)
{
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    // 後ろはキャレットの後の 1 code point だけ（' の判定が見る）。
    const Offset until{at.value + std::min(longest_code_point_bytes, end.value - at.value)};
    for (std::size_t width = window;; width *= 2)
    {
        const Offset from{at.value - std::min(width, at.value - start.value)};
        const std::string read = text.text_range(from, until);
        const bool head_cut = !(from == start);
        const auto [skipped, kept] = whole_part(read, head_cut, !(until == end));
        const std::string_view content = std::string_view{read}.substr(skipped, kept - skipped);
        const auto landed = backward_in_window(content, at.value - from.value - skipped, head_cut);
        if (landed == indented_line_start)
        {
            // 前の空白を越えて行頭に着いた（字下げ）。先頭の行なら行頭で止まる。
            return line.value > 1 ? previous_filled_line_end(text, line) : start;
        }
        if (landed.has_value())
        {
            return Offset{from.value + skipped + landed.value()};
        }
    }
}
[[nodiscard]] Offset backward_boundary(const TextBuffer &text, Offset at, std::size_t window)
{
    const LineNumber line = text.position_of(at).line;
    const Offset start = text.line_start(line);
    if (at.value <= start.value)
    {
        return line.value > 1 ? text.line_end(LineNumber{line.value - 1}) : start;
    }
    return backward_windowed(text, line, at, window);
}
} // namespace

Offset ordinary_word_boundary(const TextBuffer &text, Offset at, OrdinaryWordDirection direction)
{
    return ordinary_word_boundary(text, at, direction, first_window_bytes);
}

Offset ordinary_word_boundary(const TextBuffer &text, Offset at, OrdinaryWordDirection direction,
                              std::size_t window_bytes)
{
    const std::size_t window = std::max<std::size_t>(window_bytes, 1);
    switch (direction)
    {
    case OrdinaryWordDirection::backward:
        return backward_boundary(text, at, window);
    case OrdinaryWordDirection::forward:
        return forward_boundary(text, at, window);
    }
    std::unreachable();
}
} // namespace nenenib::core
