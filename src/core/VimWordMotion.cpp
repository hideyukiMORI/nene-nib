#include "VimWordMotion.hpp"

#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCharacterRange.hpp"
#include "VimScanPoint.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace nenenib::core
{
namespace
{
constexpr std::uint32_t blank_group = 0;
constexpr std::uint32_t symbol_group = 1;
constexpr std::uint32_t word_group = 2;
// 1 文字進んだ結果（Vim の inc() と同じ 4 つ）。行の内容の終わり（NUL の桁）に着いたことと、
// 次の行へ渡ったことを区別するのは、d + w が行末で止まる特例がその 2 つで効くからである。
constexpr int stepped_inside = 0;
constexpr int stepped_over_line = 1;
constexpr int stepped_to_line_end = 2;
constexpr int stepped_past_end = -1;
// 後ろへ 1 語ぶん歩いた結果。空行で止まったときは回数の残りを数え続ける（Vim の bck_word の
// goto finished）が、本文の先頭に着いたらそこで終わる。行き過ぎたときだけ 1 つ進め直す。
constexpr int word_stopped = 0;
constexpr int word_at_empty_line = 1;
constexpr int word_overshot = 2;

// Vim の utf_class_tab のうち、この縦切りが扱う範囲（Issue #22 で本物の Vim と突き合わせた）。
// 表に無い非 ASCII は語の文字（2）に落ちる。全角の英数字もそちら（'ＡＢabc' が 1 語＝実測）。
constexpr std::array<VimCharacterRange, 12> character_ranges{{{0x00A0, 0x00A0, blank_group},
                                                              {0x00A1, 0x00BF, symbol_group},
                                                              {0x2000, 0x206F, symbol_group},
                                                              {0x3000, 0x3000, blank_group},
                                                              {0x3001, 0x303F, symbol_group},
                                                              {0x3041, 0x309F, 0x3000},
                                                              {0x30A0, 0x30FF, 0x3001},
                                                              {0x3400, 0x4DBF, 0x4E00},
                                                              {0x4E00, 0x9FFF, 0x4E00},
                                                              {0xF900, 0xFAFF, 0x4E00},
                                                              {0xFF01, 0xFF20, symbol_group},
                                                              {0xFF5B, 0xFF65, symbol_group}}};

[[nodiscard]] bool ascii_word_byte(char32_t code) noexcept
{
    return (code >= U'0' && code <= U'9') || (code >= U'A' && code <= U'Z') ||
           (code >= U'a' && code <= U'z') || code == U'_';
}

[[nodiscard]] std::uint32_t ascii_class(char32_t code) noexcept
{
    if (code == U' ' || code == U'\t')
    {
        return blank_group;
    }
    return ascii_word_byte(code) ? word_group : symbol_group;
}

[[nodiscard]] std::uint32_t character_class(char32_t code) noexcept
{
    if (code < 0x80)
    {
        return ascii_class(code);
    }
    for (const VimCharacterRange range : character_ranges)
    {
        if (code >= range.first && code <= range.last)
        {
            return range.group;
        }
    }
    return word_group;
}

[[nodiscard]] VimScanPoint scan_point(const TextBuffer &text, Offset caret)
{
    const LineNumber line = text.position_of(caret).line;
    const Offset start = text.line_start(line);
    std::string content = text.text_range(start, text.line_end(line));
    const std::size_t index = caret.value - start.value;
    return VimScanPoint{line, std::move(content), index};
}

[[nodiscard]] Offset offset_of(const TextBuffer &text, const VimScanPoint &point)
{
    return Offset{text.line_start(point.line).value + point.index};
}

// 行の内容の終わり（Vim が NUL を置く桁）は空白として数える。そこが語の切れ目になる。
[[nodiscard]] std::uint32_t class_at(const VimScanPoint &point) noexcept
{
    if (point.index >= point.content.size())
    {
        return blank_group;
    }
    return character_class(code_point_at(point.content, Offset{point.index}));
}

[[nodiscard]] bool at_empty_line(const VimScanPoint &point) noexcept
{
    return point.index == 0 && point.content.empty();
}

void load_line(const TextBuffer &text, VimScanPoint &point, LineNumber line)
{
    point.line = line;
    point.content = text.text_range(text.line_start(line), text.line_end(line));
}

[[nodiscard]] int step_forward(const TextBuffer &text, VimScanPoint &point)
{
    if (point.index < point.content.size())
    {
        point.index = next_code_point(point.content, Offset{point.index}).value;
        return point.index >= point.content.size() ? stepped_to_line_end : stepped_inside;
    }
    if (point.line.value >= text.line_count())
    {
        return stepped_past_end;
    }
    load_line(text, point, LineNumber{point.line.value + 1});
    point.index = 0;
    return stepped_over_line;
}

[[nodiscard]] int step_backward(const TextBuffer &text, VimScanPoint &point)
{
    if (point.index > 0)
    {
        point.index = previous_code_point(point.content, Offset{point.index}).value;
        return stepped_inside;
    }
    if (point.line.value <= 1)
    {
        return stepped_past_end;
    }
    load_line(text, point, LineNumber{point.line.value - 1});
    point.index = point.content.size();
    return stepped_over_line;
}

// 1 文字進んで、まだ走査を続けてよいか。行の境目で止まるのは d + w の特例（Issue #22 の実測）。
[[nodiscard]] bool advanced(const TextBuffer &text, VimScanPoint &point, VimWordStop stop)
{
    const int stepped = step_forward(text, point);
    if (stepped == stepped_past_end)
    {
        return false;
    }
    return stepped == stepped_inside || stop == VimWordStop::across_lines;
}

[[nodiscard]] bool skipped_group(const TextBuffer &text, VimScanPoint &point, std::uint32_t group,
                                 VimWordStop stop)
{
    while (class_at(point) == group)
    {
        if (!advanced(text, point, stop))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool skipped_blanks(const TextBuffer &text, VimScanPoint &point, VimWordStop stop)
{
    while (class_at(point) == blank_group && !at_empty_line(point))
    {
        if (!advanced(text, point, stop))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool forward_word(const TextBuffer &text, VimScanPoint &point, VimWordStop stop)
{
    const std::uint32_t group = class_at(point);
    if (!advanced(text, point, stop))
    {
        return false;
    }
    if (group != blank_group && !skipped_group(text, point, group, stop))
    {
        return false;
    }
    return skipped_blanks(text, point, stop);
}

// e の走査（Vim の skip_chars）。同じ種類の文字を進み、本文の終わりで尽きたら偽。
[[nodiscard]] bool skipped_to_the_end(const TextBuffer &text, VimScanPoint &point,
                                      std::uint32_t group)
{
    while (class_at(point) == group)
    {
        if (step_forward(text, point) == stepped_past_end)
        {
            return false;
        }
    }
    return true;
}

// 語の末尾へ 1 つ（Vim の end_word の 1 周）。stay_in_this_word なら、もう語の末尾に
// いたときに次の語へ渡らない（cw の特例）。行の内容の終わりは空白として素通りする。
[[nodiscard]] bool forward_word_end(const TextBuffer &text, VimScanPoint &point,
                                    VimWordEndStop stop)
{
    const std::uint32_t group = class_at(point);
    if (step_forward(text, point) == stepped_past_end)
    {
        return false;
    }
    if (class_at(point) == group && group != blank_group)
    {
        if (!skipped_to_the_end(text, point, group))
        {
            return false;
        }
    }
    else if (stop == VimWordEndStop::enter_the_next_word || group == blank_group)
    {
        if (!skipped_to_the_end(text, point, blank_group) ||
            !skipped_to_the_end(text, point, class_at(point)))
        {
            return false;
        }
    }
    static_cast<void>(step_backward(text, point));
    return true;
}

[[nodiscard]] int retreated_over_blanks(const TextBuffer &text, VimScanPoint &point)
{
    while (class_at(point) == blank_group)
    {
        if (at_empty_line(point))
        {
            return word_at_empty_line;
        }
        if (step_backward(text, point) == stepped_past_end)
        {
            return word_stopped;
        }
    }
    return word_overshot;
}

// 後ろへ 1 語（Vim の bck_word）。戻り値は上の 3 つ。
[[nodiscard]] int backward_word(const TextBuffer &text, VimScanPoint &point)
{
    if (step_backward(text, point) == stepped_past_end)
    {
        return word_stopped;
    }
    const int blanks = retreated_over_blanks(text, point);
    if (blanks != word_overshot)
    {
        return blanks;
    }
    const std::uint32_t group = class_at(point);
    while (class_at(point) == group)
    {
        if (step_backward(text, point) == stepped_past_end)
        {
            return word_stopped;
        }
    }
    return word_overshot;
}
} // namespace

Offset vim_next_word(const TextBuffer &text, Offset caret, std::size_t count, VimWordStop stop)
{
    VimScanPoint point = scan_point(text, caret);
    for (std::size_t step = 0; step < count; ++step)
    {
        // 行末で止まる特例が効くのは最後の 1 回だけ（Vim の fwd_word の count == 0 の条件）。
        const VimWordStop limit = step + 1 == count ? stop : VimWordStop::across_lines;
        if (!forward_word(text, point, limit))
        {
            break;
        }
    }
    return offset_of(text, point);
}

Offset vim_word_end(const TextBuffer &text, Offset caret, std::size_t count, VimWordEndStop stop)
{
    VimScanPoint point = scan_point(text, caret);
    VimWordEndStop limit = stop;
    for (std::size_t step = 0; step < count; ++step)
    {
        if (!forward_word_end(text, point, limit))
        {
            break;
        }
        // 止まる特例が効くのは最初の 1 回だけ（Vim の end_word の stop = FALSE）。
        limit = VimWordEndStop::enter_the_next_word;
    }
    return offset_of(text, point);
}

Offset vim_previous_word(const TextBuffer &text, Offset caret, std::size_t count)
{
    VimScanPoint point = scan_point(text, caret);
    for (std::size_t step = 0; step < count; ++step)
    {
        const int outcome = backward_word(text, point);
        if (outcome == word_stopped)
        {
            break;
        }
        if (outcome == word_overshot)
        {
            static_cast<void>(step_forward(text, point));
        }
    }
    return offset_of(text, point);
}
} // namespace nenenib::core
