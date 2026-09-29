#include "VimWordMotion.hpp"

#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimCharacterRange.hpp"
#include "VimRepeatFailure.hpp"
#include "VimScanPoint.hpp"
#include "VimWordAdvance.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

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
// 周の始めに本文の先頭にいたときだけ失敗（Vim の return FAIL・Issue #224）。
constexpr int word_stopped = 0;
constexpr int word_at_empty_line = 1;
constexpr int word_overshot = 2;
constexpr int word_failed = 3;
// 手前の語の末尾へ 1 つ戻った結果（Vim の bckend_word の 1 周）。最初の 1 歩で本文の先頭に
// 当たったときだけ失敗で、途中で当たったら・行をまたいで止まったらそこで回数ごと終わる。
constexpr int end_failed = 0;
constexpr int end_finished = 1;
constexpr int end_continues = 2;

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

[[nodiscard]] std::uint32_t table_class(char32_t code) noexcept
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

[[nodiscard]] VimScanPoint scan_point(const TextBuffer &text, Offset caret, VimWordClass kind)
{
    const LineNumber line = text.position_of(caret).line;
    const Offset start = text.line_start(line);
    std::string content = text.text_range(start, text.line_end(line));
    const std::size_t index = caret.value - start.value;
    return VimScanPoint{line, std::move(content), index, kind};
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
    return vim_character_class(code_point_at(point.content, Offset{point.index}), point.kind);
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
        point.index = vim_character_end(point.content, Offset{point.index}).value;
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
        point.index = vim_character_start(point.content, Offset{point.index}).value;
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

// 前へ 1 語（Vim の fwd_word の 1 周）。失敗は周の始めの 1 歩で本文の最後の文字（か終わり）から
// 出られないときだけ（`i == -1 || (i >= 1 && last_line)`）。周の途中で本文が尽きたら、行末で
// 止まる特例と同じく回数の残りを捨てた成功（Issue #226 で実測）。
[[nodiscard]] VimWordAdvance forward_word(const TextBuffer &text, VimScanPoint &point,
                                          VimWordStop stop)
{
    const std::uint32_t group = class_at(point);
    const bool on_last_line = point.line.value >= text.line_count();
    const int stepped = step_forward(text, point);
    if (stepped == stepped_past_end || (stepped != stepped_inside && on_last_line))
    {
        return VimWordAdvance::failed_at_the_end;
    }
    if (stepped != stepped_inside && stop == VimWordStop::at_line_end)
    {
        return VimWordAdvance::stopped;
    }
    if (group != blank_group && !skipped_group(text, point, group, stop))
    {
        return VimWordAdvance::stopped;
    }
    return skipped_blanks(text, point, stop) ? VimWordAdvance::continues : VimWordAdvance::stopped;
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
        return word_failed;
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

// bck_word の stop=TRUE の本体。空白を飛んでから同じ種類の連なりの先頭まで戻る。
// 真なら今の位置が答え（本文の先頭か空行に着いた）。偽なら 1 つ行き過ぎている。
[[nodiscard]] bool retreated_to_run_start(const TextBuffer &text, VimScanPoint &point)
{
    while (class_at(point) == blank_group)
    {
        if (at_empty_line(point) || step_backward(text, point) == stepped_past_end)
        {
            return true;
        }
    }
    const std::uint32_t group = class_at(point);
    while (class_at(point) == group)
    {
        if (step_backward(text, point) == stepped_past_end)
        {
            return true;
        }
    }
    return false;
}

// bckend_word の 1 歩戻り（dec_cursor）。本文の先頭に着いたか、at_line_end で行をまたいだら偽
// （そこが答え＝Vim の return OK）。
[[nodiscard]] bool retreated_one(const TextBuffer &text, VimScanPoint &point, VimWordStop stop)
{
    const int stepped = step_backward(text, point);
    return stepped == stepped_inside ||
           (stepped == stepped_over_line && stop == VimWordStop::across_lines);
}

// bckend_word の 1 周の後半。この語の始まりより前へ戻り、空白を戻って手前の語の末尾に着く。
// 空白の途中の空行ではそこで止まる。偽ならそこで回数ごと終わる（Vim の return OK）。
[[nodiscard]] bool retreated_to_previous_end(const TextBuffer &text, VimScanPoint &point,
                                             std::uint32_t group, VimWordStop stop)
{
    while (group != blank_group && class_at(point) == group)
    {
        if (!retreated_one(text, point, stop))
        {
            return false;
        }
    }
    while (class_at(point) == blank_group && !at_empty_line(point))
    {
        if (!retreated_one(text, point, stop))
        {
            return false;
        }
    }
    return true;
}

// 手前の語の末尾へ 1 つ（Vim の bckend_word の 1 周）。at_line_end は eol=TRUE（テキスト
// オブジェクト）、across_lines は eol=FALSE（ge gE）。空白を戻る途中の空行ではそこで止まり、
// 回数の残りは次の周が数える。
[[nodiscard]] int backward_word_end(const TextBuffer &text, VimScanPoint &point, VimWordStop stop)
{
    const std::uint32_t group = class_at(point);
    const int stepped = step_backward(text, point);
    if (stepped == stepped_past_end)
    {
        return end_failed;
    }
    if (stepped == stepped_over_line && stop == VimWordStop::at_line_end)
    {
        return end_finished;
    }
    return retreated_to_previous_end(text, point, group, stop) ? end_continues : end_finished;
}

// end_word の empty=TRUE。空白を飛ぶ途中に空行があればそこで止まる（Vim の goto finished）。
[[nodiscard]] bool skipped_blanks_to_word(const TextBuffer &text, VimScanPoint &point)
{
    while (class_at(point) == blank_group)
    {
        if (at_empty_line(point))
        {
            return true;
        }
        if (step_forward(text, point) == stepped_past_end)
        {
            return false;
        }
    }
    return true;
}
// end_word の empty=TRUE で空白の上から始めた枝。空行で止まったらそこが末尾（後戻りしない）。
[[nodiscard]] std::optional<Offset> blank_object_end(const TextBuffer &text, VimScanPoint &point)
{
    if (!skipped_blanks_to_word(text, point))
    {
        return std::nullopt;
    }
    if (at_empty_line(point))
    {
        return offset_of(text, point);
    }
    if (!skipped_to_the_end(text, point, class_at(point)))
    {
        return std::nullopt;
    }
    static_cast<void>(step_backward(text, point));
    return offset_of(text, point);
}
} // namespace

VimMotionLanding vim_next_word(const TextBuffer &text, Offset caret, VimWordWalk walk,
                               VimWordStop stop)
{
    VimScanPoint point = scan_point(text, caret, walk.kind);
    for (std::size_t step = 0; step < walk.count; ++step)
    {
        // 行末で止まる特例が効くのは最後の 1 回だけ（Vim の fwd_word の count == 0 の条件）。
        const VimWordStop limit = step + 1 == walk.count ? stop : VimWordStop::across_lines;
        switch (forward_word(text, point, limit))
        {
        case VimWordAdvance::continues:
            continue;
        case VimWordAdvance::stopped:
            return VimMotionLanding{offset_of(text, point)};
        case VimWordAdvance::failed_at_the_end:
            return VimMotionLanding{offset_of(text, point), VimRepeatFailure::not_moved};
        }
        std::unreachable();
    }
    return VimMotionLanding{offset_of(text, point)};
}

VimMotionLanding vim_word_end(const TextBuffer &text, Offset caret, VimWordWalk walk,
                              VimWordEndStop stop)
{
    VimScanPoint point = scan_point(text, caret, walk.kind);
    VimWordEndStop limit = stop;
    for (std::size_t step = 0; step < walk.count; ++step)
    {
        // Vim の end_word は周のどの歩でも本文の終わりの先へ出ようとしたら FAIL（Issue #226 で
        // 実測）。着地はそこで止まった位置のまま。
        if (!forward_word_end(text, point, limit))
        {
            return VimMotionLanding{offset_of(text, point), VimRepeatFailure::not_moved};
        }
        // 止まる特例が効くのは最初の 1 回だけ（Vim の end_word の stop = FALSE）。
        limit = VimWordEndStop::enter_the_next_word;
    }
    return VimMotionLanding{offset_of(text, point)};
}

std::uint32_t vim_character_class(char32_t code, VimWordClass kind) noexcept
{
    const std::uint32_t group = table_class(code);
    switch (kind)
    {
    case VimWordClass::word:
        return group;
    case VimWordClass::big_word:
        return group == blank_group ? blank_group : word_group;
    }
    std::unreachable();
}

Offset vim_word_stop_forward(const TextBuffer &text, Offset caret, VimWordClass kind)
{
    VimScanPoint point = scan_point(text, caret, kind);
    static_cast<void>(forward_word(text, point, VimWordStop::at_line_end));
    return offset_of(text, point);
}

std::optional<Offset> vim_word_object_end(const TextBuffer &text, Offset caret, VimWordClass kind)
{
    VimScanPoint point = scan_point(text, caret, kind);
    const std::uint32_t group = class_at(point);
    if (step_forward(text, point) == stepped_past_end)
    {
        return std::nullopt;
    }
    if (group == blank_group)
    {
        return blank_object_end(text, point);
    }
    if (class_at(point) == group && !skipped_to_the_end(text, point, group))
    {
        return std::nullopt;
    }
    static_cast<void>(step_backward(text, point));
    return offset_of(text, point);
}

std::optional<Offset> vim_word_object_begin(const TextBuffer &text, Offset caret, VimWordClass kind)
{
    VimScanPoint point = scan_point(text, caret, kind);
    const std::uint32_t group = class_at(point);
    if (step_backward(text, point) == stepped_past_end)
    {
        return std::nullopt;
    }
    // 1 つ手前が違う種類なら動かない（stop = TRUE）。それ以外は連なりの先頭まで戻る。
    const bool retreats = group == blank_group || group == class_at(point);
    if (!retreats || !retreated_to_run_start(text, point))
    {
        static_cast<void>(step_forward(text, point));
    }
    return offset_of(text, point);
}

std::optional<Offset> vim_word_object_previous_end(const TextBuffer &text, Offset caret,
                                                   VimWordClass kind)
{
    VimScanPoint point = scan_point(text, caret, kind);
    if (backward_word_end(text, point, VimWordStop::at_line_end) == end_failed)
    {
        return std::nullopt;
    }
    return offset_of(text, point);
}

VimMotionLanding vim_previous_word_end(const TextBuffer &text, Offset caret, VimWordWalk walk)
{
    VimScanPoint point = scan_point(text, caret, walk.kind);
    for (std::size_t step = 0; step < walk.count; ++step)
    {
        const int outcome = backward_word_end(text, point, VimWordStop::across_lines);
        if (outcome == end_failed)
        {
            return VimMotionLanding{offset_of(text, point), VimRepeatFailure::not_moved};
        }
        if (outcome == end_finished)
        {
            break;
        }
    }
    return VimMotionLanding{offset_of(text, point)};
}

std::optional<OffsetRange> vim_word_at(const TextBuffer &text, Offset caret)
{
    VimScanPoint point = scan_point(text, caret, VimWordClass::word);
    // 空白と記号の上では、同じ行の後ろにある最初の語の文字まで進む（行はまたがない）。
    while (class_at(point) <= symbol_group)
    {
        if (point.index >= point.content.size())
        {
            return std::nullopt;
        }
        point.index = next_code_point(point.content, Offset{point.index}).value;
    }
    const std::uint32_t group = class_at(point);
    std::size_t begin = point.index;
    while (begin > 0 &&
           vim_character_class(
               code_point_at(point.content, previous_code_point(point.content, Offset{begin})),
               VimWordClass::word) == group)
    {
        begin = previous_code_point(point.content, Offset{begin}).value;
    }
    std::size_t end = point.index;
    while (end < point.content.size() &&
           vim_character_class(code_point_at(point.content, Offset{end}), VimWordClass::word) ==
               group)
    {
        end = next_code_point(point.content, Offset{end}).value;
    }
    const Offset start = text.line_start(point.line);
    return OffsetRange{Offset{start.value + begin}, Offset{start.value + end}};
}

VimMotionLanding vim_previous_word(const TextBuffer &text, Offset caret, VimWordWalk walk)
{
    VimScanPoint point = scan_point(text, caret, walk.kind);
    for (std::size_t step = 0; step < walk.count; ++step)
    {
        const int outcome = backward_word(text, point);
        if (outcome == word_failed)
        {
            return VimMotionLanding{offset_of(text, point), VimRepeatFailure::not_moved};
        }
        if (outcome == word_stopped)
        {
            break;
        }
        if (outcome == word_overshot)
        {
            static_cast<void>(step_forward(text, point));
        }
    }
    return VimMotionLanding{offset_of(text, point)};
}
} // namespace nenenib::core
