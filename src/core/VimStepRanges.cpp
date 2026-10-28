#include "VimStepDetail.hpp"

#include "VimCaret.hpp"
#include "VimKeyTable.hpp"
#include "VimLineEndStop.hpp"
#include "VimMatchingPair.hpp"
#include "VimMotionRange.hpp"
#include "VimNumberedRule.hpp"
#include "VimScreenPosition.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"
#include "VimWordWalk.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::core
{
using detail::adjusted_for_exclusive;
using detail::backward_characters;
using detail::characters_between;
using detail::column_landing;
using detail::document_line;
using detail::exclusive_range;
using detail::forward_characters;
using detail::inclusive_end;
using detail::keeps_the_line_break;
using detail::line_above;
using detail::line_below;
using detail::line_of;
using detail::lines_below;
using detail::mark_landing_of;
using detail::motion_range;
using detail::percent_line;
using detail::row_landing_in;
using detail::screen_line;
using detail::screen_position_of;
using detail::single_step;
using detail::to_line_end;
using detail::whole_lines_for_delete;
using detail::wrapped_backward;
using detail::wrapped_forward;
using detail::writing_empty;

namespace detail
{
// ---------------------------------------------------------------- 範囲（ADR 0015 の決定 2）

// 文字単位の範囲。^ は前にも後ろにも動くので、端は必ず小さいほうを先にする（Vim も同じ）。
[[nodiscard]] VimMotionRange characters_between(Offset first, Offset second)
{
    const bool ordered = first.value <= second.value;
    return VimMotionRange{OffsetRange{ordered ? first : second, ordered ? second : first},
                          VimRegisterKind::characters};
}
} // namespace detail

namespace
{
// 行単位の範囲。最初の行の先頭から、最後の行の内容の終わりまで（改行は入れない）。
[[nodiscard]] VimMotionRange lines_between(const TextBuffer &text, LineNumber first,
                                           LineNumber last)
{
    return VimMotionRange{OffsetRange{text.line_start(first), text.line_end(last)},
                          VimRegisterKind::lines};
}
} // namespace

namespace detail
{
// 下へ steps 行ぶんの行の範囲。Vim の cursor_down は最終行にいるときだけ失敗し、
// ほかは本文の末尾で止まる（5dd が 3 行の本文を全部消し、j2dd が最終行では何もしない理由）。
[[nodiscard]] std::optional<VimMotionRange> lines_below(const TextBuffer &text, LineNumber line,
                                                        std::size_t steps)
{
    if (steps > 0 && line.value >= text.line_count())
    {
        return std::nullopt;
    }
    return lines_between(text, line, line_below(text, line, steps));
}
} // namespace detail

namespace
{
[[nodiscard]] std::optional<VimMotionRange> lines_above(const TextBuffer &text, LineNumber line,
                                                        std::size_t steps)
{
    if (steps > 0 && line.value <= 1)
    {
        return std::nullopt;
    }
    return lines_between(text, line_above(line, steps), line);
}
} // namespace

namespace detail
{
// inclusive な移動の範囲の終わり。止まった文字を範囲に入れる（行の内容の終わりは越えない）。
[[nodiscard]] Offset inclusive_end(const TextBuffer &text, Offset at)
{
    return forward_characters(text, at, single_step);
}

// inclusive な行末への移動の範囲。空行で空でも `c` は空の本文を書く（#960・VimEmptyChange）。
[[nodiscard]] VimMotionRange writing_empty(VimMotionRange range)
{
    range.empty_change = VimEmptyChange::writes_empty;
    return range;
}

// $ と g_ は inclusive で、回数があると count-1 行下の行末・最後の非空白まで（Vim の nv_dollar と
// nv_g_cmd の cursor_down・最終行で回数が 2 以上なら範囲が無い）。後ろへ動いた g_ はキャレットの
// 文字まで含む（inclusive な後ろ向きの移動・ge と同じ・Issue #721）。
[[nodiscard]] std::optional<VimMotionRange> to_line_end(const TextBuffer &text, Offset caret,
                                                        std::size_t count, VimMotion motion)
{
    const LineNumber line = line_of(text, caret);
    if (count > single_step && line.value >= text.line_count())
    {
        return std::nullopt;
    }
    const LineNumber target = line_below(text, line, count - single_step);
    if (motion == VimMotion::line_end)
    {
        return writing_empty(characters_between(caret, text.line_end(target)));
    }
    const Offset landed = vim_last_non_blank(text, target);
    return writing_empty(landed.value < caret.value
                             ? characters_between(landed, inclusive_end(text, caret))
                             : characters_between(caret, inclusive_end(text, landed)));
}
} // namespace detail

namespace
{
[[nodiscard]] VimMotionRange screen_range(const VimEditorView &view, VimScreenPosition position,
                                          std::size_t count, LineNumber line)
{
    const LineNumber target = screen_line(view, position, count);
    const LineNumber first{std::min(line.value, target.value)};
    const LineNumber last{std::max(line.value, target.value)};
    return lines_between(view.text, first, last);
}

// オペレータの後ろの `<BS>` が行末の位置に止まるか。d と c だけが前の行の改行に止まり、y は
// 裸の NORMAL と同じに最後の文字へ渡る（Vim の nv_left は OP_DELETE / OP_CHANGE のときだけ NUL に
// 置く・ADR 0049・Vim 9.1 で実測）。`<Space>` はどのオペレータでも止まる。
[[nodiscard]] VimLineEndStop backspace_stop_for(VimOperator operation) noexcept
{
    switch (operation)
    {
    case VimOperator::remove:
    case VimOperator::change:
        return VimLineEndStop::rests;
    case VimOperator::yank:
    case VimOperator::toggle_case:
    case VimOperator::lower_case:
    case VimOperator::upper_case:
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        return VimLineEndStop::passes;
    }
    std::unreachable();
}

// e E の範囲の終わりの止まり方。cw cW の特例は語の末尾にいたら次の語へ渡らない。
[[nodiscard]] VimWordEndStop word_end_stop_for(VimMotion motion) noexcept
{
    const bool for_change =
        motion == VimMotion::word_end_for_change || motion == VimMotion::big_word_end_for_change;
    return for_change ? VimWordEndStop::stay_in_this_word : VimWordEndStop::enter_the_next_word;
}

// b B の範囲（exclusive）。回数の途中で本文の先頭に当たったら Vim はオペレータを打ち消す
// （clearopbeep）ので範囲が無い（Issue #224）。
[[nodiscard]] std::optional<VimMotionRange> previous_word_range(const TextBuffer &text,
                                                                Offset caret, VimWordWalk walk)
{
    const VimMotionLanding landing = vim_previous_word(text, caret, walk);
    if (landing.failure.has_value())
    {
        return std::nullopt;
    }
    return characters_between(landing.offset, caret);
}

// ge gE の範囲（inclusive・キャレットの文字まで含む）。本文の先頭で動けなければ Vim は
// オペレータを打ち消す（clearopbeep）ので範囲が無い（Issue #222 / #224）。
[[nodiscard]] std::optional<VimMotionRange> previous_word_end_range(const TextBuffer &text,
                                                                    Offset caret, VimWordWalk walk)
{
    const VimMotionLanding landing = vim_previous_word_end(text, caret, walk);
    if (landing.failure.has_value())
    {
        return std::nullopt;
    }
    return characters_between(landing.offset, inclusive_end(text, caret));
}

// オペレータの後ろの w W e E の範囲。歩きの失敗は見ず、着地までが範囲になる（Vim の nv_wordcmd は
// FAIL を OP_NOP のときだけ見る・Issue #226）。w は行末で止まり、e は着地の文字を含む。
[[nodiscard]] VimMotionRange forward_word_range(const TextBuffer &text, Offset caret,
                                                VimWordWalk walk, VimMotion motion)
{
    if (motion == VimMotion::next_word || motion == VimMotion::next_big_word)
    {
        return characters_between(
            caret, vim_next_word(text, caret, walk, VimWordStop::at_line_end).offset);
    }
    return characters_between(
        caret,
        inclusive_end(text, vim_word_end(text, caret, walk, word_end_stop_for(motion)).offset));
}

// gj gk g0 gm gM g^ の範囲（文字単位・exclusive）と g$ の範囲（inclusive・着いた文字まで含み、
// 最後の段では行の内容の終わりまでで改行を含まない・ADR 0147 の決定 5）。回数の途中で本文の端の
// 画面行に当たったら Vim はオペレータを打ち消す（nv_gomove・nv_g_dollar_cmd の clearopbeep）ので
// 範囲が無い（ADR 0144 の決定 4）。着地はオペレータ待ちの NORMAL の寄せ（行の内容の終わりには
// 載らない）。
[[nodiscard]] std::optional<VimMotionRange> display_range(const VimEditorView &view,
                                                          VimMotion motion)
{
    const VimMotionLanding landing = row_landing_in(view, VimMode::normal);
    if (landing.failure.has_value())
    {
        return std::nullopt;
    }
    const bool inclusive = motion == VimMotion::display_line_end;
    return characters_between(view.selection.caret, inclusive
                                                        ? inclusive_end(view.text, landing.offset)
                                                        : landing.offset);
}

// `<Space>` `<BS>` の範囲（ADR 0049）。`<Space>` はどのオペレータでも行末の位置に止まり、`<BS>` の
// 止まり方はオペレータで変わる（backspace_stop_for）。
[[nodiscard]] VimMotionRange wrapped_range(const VimEditorView &view, VimMotion motion,
                                           std::size_t count, VimOperator operation)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    if (motion == VimMotion::wrap_right)
    {
        return characters_between(caret,
                                  wrapped_forward(text, caret, count, VimLineEndStop::rests));
    }
    return characters_between(wrapped_backward(text, caret, count, backspace_stop_for(operation)),
                              caret);
}

// gg G の範囲（行単位）。キャレットの行と回数の行の間（どちらが上でも）。
[[nodiscard]] VimMotionRange document_range(const TextBuffer &text, LineNumber line,
                                            std::size_t count)
{
    const LineNumber target = document_line(text, count);
    return lines_between(text, LineNumber{std::min(line.value, target.value)},
                         LineNumber{std::max(line.value, target.value)});
}

// `%` の範囲（Issue #767）。対応する括弧は両端を含む文字単位で、1 行の中でも `"1` へ行く（Vim の
// nv_percent の use_reg_one）。`{N}%` はキャレットの行と割合の行の間の行単位。
[[nodiscard]] std::optional<VimMotionRange> matching_range(const TextBuffer &text, Offset caret,
                                                           std::size_t count)
{
    if (count > 0)
    {
        const auto target = percent_line(text, count);
        if (!target.has_value())
        {
            return std::nullopt;
        }
        const LineNumber line = line_of(text, caret);
        return lines_between(text, LineNumber{std::min(line.value, target.value().value)},
                             LineNumber{std::max(line.value, target.value().value)});
    }
    const auto matched = vim_matching_pair(text, caret);
    if (!matched.has_value())
    {
        return std::nullopt;
    }
    VimMotionRange range = characters_between(caret, matched.value());
    range.range.end = inclusive_end(text, range.range.end);
    range.numbered = VimNumberedRule::always;
    return range;
}

// マークへの範囲（ADR 0152 の決定 3）。`'a` は行単位、`` `a `` は exclusive の文字単位で 1
// 行の中でも
// `"1` へ行く（Vim の nv_gomark の use_reg_one）。着地は NORMAL の寄せ（Vim の check_cursor）。
[[nodiscard]] VimMotionRange mark_range(const VimEditorView &view, VimMotion motion)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const Offset landing = mark_landing_of(view, motion, VimMode::normal);
    if (motion == VimMotion::mark_line)
    {
        const LineNumber line = line_of(text, caret);
        const LineNumber target = line_of(text, landing);
        return lines_between(text, LineNumber{std::min(line.value, target.value)},
                             LineNumber{std::max(line.value, target.value)});
    }
    VimMotionRange range = characters_between(caret, landing);
    range.numbered = VimNumberedRule::always;
    return range;
}

// h l 0 ^ の範囲（行の中の文字単位）。
[[nodiscard]] VimMotionRange sideways_range(const TextBuffer &text, Offset caret, VimMotion motion,
                                            std::size_t count)
{
    if (motion == VimMotion::line_start)
    {
        return characters_between(text.line_start(line_of(text, caret)), caret);
    }
    if (motion == VimMotion::first_non_blank)
    {
        return characters_between(vim_first_non_blank(text, caret), caret);
    }
    return motion == VimMotion::left
               ? characters_between(backward_characters(text, caret, count), caret)
               : characters_between(caret, forward_characters(text, caret, count));
}

// b B ge gE の範囲。
[[nodiscard]] std::optional<VimMotionRange>
backward_word_range(const TextBuffer &text, Offset caret, const VimWordWalk &walk, VimMotion motion)
{
    const bool word_end =
        motion == VimMotion::previous_word_end || motion == VimMotion::previous_big_word_end;
    return word_end ? previous_word_end_range(text, caret, walk)
                    : previous_word_range(text, caret, walk);
}

// 飛ぶ移動の範囲（H M L gg G % 'a `a）。
[[nodiscard]] std::optional<VimMotionRange> jump_range(const VimEditorView &view, VimMotion motion,
                                                       std::size_t count)
{
    if (motion == VimMotion::matching_pair)
    {
        return matching_range(view.text, view.selection.caret, count);
    }
    if (motion == VimMotion::mark_line || motion == VimMotion::mark_exact)
    {
        return mark_range(view, motion);
    }
    const LineNumber line = line_of(view.text, view.selection.caret);
    if (motion == VimMotion::document_first || motion == VimMotion::document_last)
    {
        return document_range(view.text, line, count);
    }
    return screen_range(view, screen_position_of(motion), count, line);
}
} // namespace

namespace detail
{
[[nodiscard]] std::optional<VimMotionRange>
motion_range(const VimEditorView &view, VimMotion motion, std::size_t count, VimOperator operation)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(text, caret);
    const VimWordWalk walk{count, vim_motion_word_class(motion)};
    switch (motion)
    {
    case VimMotion::down:
    case VimMotion::next_line:
        return lines_below(text, line, count);
    case VimMotion::first_non_blank_below:
        return lines_below(text, line, count - single_step);
    case VimMotion::up:
    case VimMotion::previous_line:
        return lines_above(text, line, count);
    case VimMotion::left:
    case VimMotion::right:
    case VimMotion::line_start:
    case VimMotion::first_non_blank:
        return sideways_range(text, caret, motion, count);
    case VimMotion::line_end:
    case VimMotion::last_non_blank:
        return to_line_end(text, caret, count, motion);
    case VimMotion::next_word:
    case VimMotion::next_big_word:
    case VimMotion::word_end:
    case VimMotion::big_word_end:
    case VimMotion::word_end_for_change:
    case VimMotion::big_word_end_for_change:
        return forward_word_range(text, caret, walk, motion);
    case VimMotion::previous_word:
    case VimMotion::previous_big_word:
    case VimMotion::previous_word_end:
    case VimMotion::previous_big_word_end:
        return backward_word_range(text, caret, walk, motion);
    case VimMotion::screen_top:
    case VimMotion::screen_middle:
    case VimMotion::screen_bottom:
    case VimMotion::document_first:
    case VimMotion::document_last:
    case VimMotion::matching_pair:
    case VimMotion::mark_line:
    case VimMotion::mark_exact:
        return jump_range(view, motion, count);
    case VimMotion::wrap_right:
    case VimMotion::wrap_left:
        return wrapped_range(view, motion, count, operation);
    case VimMotion::display_down:
    case VimMotion::display_up:
    case VimMotion::display_line_start:
    case VimMotion::display_line_end:
    case VimMotion::display_line_middle:
    case VimMotion::line_middle:
    case VimMotion::display_line_first_non_blank:
        return display_range(view, motion);
    case VimMotion::column:
        return characters_between(caret, column_landing(text, line, count, VimMode::normal));
    }
    std::unreachable();
}
} // namespace detail

namespace
{
// 始まりが行の字下げの中か（Vim の inindent）。空行の行頭も、最初の非空白そのものも中に数える。
[[nodiscard]] bool starts_in_the_indent(const TextBuffer &text, Offset begin)
{
    return begin.value <= vim_first_non_blank(text, begin).value;
}

// at から行の内容の終わりまでが空白だけか（Vim の skipwhite が NUL に着くか）。
[[nodiscard]] bool blank_to_the_line_end(const TextBuffer &text, Offset at)
{
    const std::string rest = text.text_range(at, text.line_end(line_of(text, at)));
    // 走査は自前で書く。STL の検索は core の外へ memchr を出す（ARC-003 の許可シンボル）。
    for (const char byte : rest)
    {
        if (byte != ' ' && byte != '\t')
        {
            return false;
        }
    }
    return true;
}
} // namespace

namespace detail
{
// :help exclusive の 2 つの言い換え。行頭で終わる範囲は 1 つ前の行の末尾までになり、
// 始まりが字下げの中なら行単位になる。dw が空行を丸ごと消すのも、db が上の行を消すのもこれ。
// 規則はこの 1 か所だけが持ち、exclusive な移動と検索の両方がここを通る（ARC-001）。
[[nodiscard]] VimMotionRange exclusive_range(const TextBuffer &text, const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    if (first.value >= last.value || range.range.end != text.line_start(last))
    {
        return range;
    }
    const LineNumber previous{last.value - 1};
    if (starts_in_the_indent(text, range.range.begin))
    {
        // 行単位に言い換えても、Vim のカーソルは元の範囲の先頭のまま保存する（ADR 0052 の決定 8）。
        VimMotionRange lines = lines_between(text, first, previous);
        lines.restore = range.restore.value_or(range.range.begin);
        return lines;
    }
    return characters_between(range.range.begin, text.line_end(previous));
}

[[nodiscard]] VimMotionRange adjusted_for_exclusive(const TextBuffer &text,
                                                    const VimMotionRange &range, VimMotion motion)
{
    return vim_motion_is_exclusive(motion) ? exclusive_range(text, range) : range;
}

// d<BS> c<BS> が文字のある前の行へ渡ったら、範囲は exclusive の言い換えを通さない（Vim の nv_left
// が前の行の NUL に止まって CA_NO_ADJ_OP_END を立てる・ADR 0049）。行頭の d<BS> が改行 1 つだけを
// 消すのはこれ。渡った行が全部空行なら Vim も NUL に止まらず、言い換えを通って行単位になる。
[[nodiscard]] bool keeps_the_line_break(const TextBuffer &text, const VimMotionRange &range,
                                        VimMotion motion, VimOperator operation)
{
    if (motion != VimMotion::wrap_left || backspace_stop_for(operation) != VimLineEndStop::rests)
    {
        return false;
    }
    const LineNumber last = line_of(text, range.range.end);
    for (std::size_t line = line_of(text, range.range.begin).value; line < last.value; ++line)
    {
        if (text.line_end(LineNumber{line}) != text.line_start(LineNumber{line}))
        {
            return true;
        }
    }
    return false;
}

// op_delete の「奇妙な Vi の振る舞い」。複数行にまたがる文字単位の削除で、終わりの後ろが空白だけ
// かつ始まりが字下げの中なら行単位になる（2D と de の行またぎ）。d だけで、c と y には無い。
[[nodiscard]] VimMotionRange whole_lines_for_delete(const TextBuffer &text,
                                                    const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    if (range.kind == VimRegisterKind::lines || first.value >= last.value ||
        !blank_to_the_line_end(text, range.range.end) ||
        !starts_in_the_indent(text, range.range.begin))
    {
        return range;
    }
    VimMotionRange lines = lines_between(text, first, last);
    lines.numbered = range.numbered;
    lines.restore = range.restore.value_or(range.range.begin);
    return lines;
}
} // namespace detail
} // namespace nenenib::core
