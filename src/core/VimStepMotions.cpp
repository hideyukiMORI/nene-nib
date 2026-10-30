#include "VimStepDetail.hpp"

#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimCharacterSearchInvocation.hpp"
#include "VimCharacterSearchRequest.hpp"
#include "VimCharacterSearchScan.hpp"
#include "VimCursorCorrect.hpp"
#include "VimCursorWindow.hpp"
#include "VimJumpLists.hpp"
#include "VimJumpMove.hpp"
#include "VimKeyTable.hpp"
#include "VimLineEndStop.hpp"
#include "VimMarkEdits.hpp"
#include "VimMatchingPair.hpp"
#include "VimScreenPosition.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"
#include "VimWordWalk.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
using detail::backward_characters;
using detail::cancelled;
using detail::caret_on_line;
using detail::character_search_position;
using detail::column_landing;
using detail::context_marked;
using detail::document_line;
using detail::failed;
using detail::forward_characters;
using detail::jumped_by;
using detail::landed_step;
using detail::landing_of;
using detail::last_view_line;
using detail::line_above;
using detail::line_below;
using detail::line_of;
using detail::mark_landing_of;
using detail::motion_step;
using detail::moved_by;
using detail::moved_or_failed;
using detail::opposite;
using detail::percent_line;
using detail::resolved_count;
using detail::resolved_motion_count;
using detail::rested_in;
using detail::row_landing_in;
using detail::screen_line;
using detail::screen_position_of;
using detail::searches_forward;
using detail::single_step;
using detail::wanted_after;
using detail::wanted_column_of;
using detail::wanted_for;
using detail::wrapped_backward;
using detail::wrapped_forward;

namespace
{
// `{N}%` が受ける回数の上限と、本文の割合から行を求める分母（Vim の nv_percent・Issue #767）。
constexpr std::size_t whole_percent = 100;

constexpr std::size_t percent_round_up = 99;
} // namespace

namespace detail
{
[[nodiscard]] bool searches_forward(VimCharacterSearchKind kind) noexcept
{
    switch (kind)
    {
    case VimCharacterSearchKind::find_forward:
    case VimCharacterSearchKind::till_forward:
        return true;
    case VimCharacterSearchKind::find_backward:
    case VimCharacterSearchKind::till_backward:
        return false;
    }
    std::unreachable();
}
} // namespace detail

namespace
{
[[nodiscard]] bool searches_until(VimCharacterSearchKind kind) noexcept
{
    switch (kind)
    {
    case VimCharacterSearchKind::till_forward:
    case VimCharacterSearchKind::till_backward:
        return true;
    case VimCharacterSearchKind::find_forward:
    case VimCharacterSearchKind::find_backward:
        return false;
    }
    std::unreachable();
}
} // namespace

namespace detail
{
[[nodiscard]] VimCharacterSearchKind opposite(VimCharacterSearchKind kind) noexcept
{
    switch (kind)
    {
    case VimCharacterSearchKind::find_forward:
        return VimCharacterSearchKind::find_backward;
    case VimCharacterSearchKind::find_backward:
        return VimCharacterSearchKind::find_forward;
    case VimCharacterSearchKind::till_forward:
        return VimCharacterSearchKind::till_backward;
    case VimCharacterSearchKind::till_backward:
        return VimCharacterSearchKind::till_forward;
    }
    std::unreachable();
}
} // namespace detail

namespace
{
[[nodiscard]] Offset search_destination(std::string_view content, Offset match,
                                        VimCharacterSearchKind kind) noexcept
{
    switch (kind)
    {
    case VimCharacterSearchKind::find_forward:
    case VimCharacterSearchKind::find_backward:
        return match;
    case VimCharacterSearchKind::till_forward:
        return vim_character_start(content, match);
    case VimCharacterSearchKind::till_backward:
        return vim_character_end(content, match);
    }
    std::unreachable();
}

[[nodiscard]] bool skips_adjacent(Offset caret, Offset destination,
                                  const VimCharacterSearchRequest &request) noexcept
{
    return request.invocation == VimCharacterSearchInvocation::repeat && request.count == 1 &&
           searches_until(request.search.kind) && destination == caret;
}

[[nodiscard]] std::optional<Offset> matched_destination(std::string_view content, Offset at,
                                                        Offset caret,
                                                        VimCharacterSearchScan &scan) noexcept
{
    if (code_point_at(content, at) != scan.request.search.target)
    {
        return std::nullopt;
    }
    const Offset destination = search_destination(content, at, scan.request.search.kind);
    if (skips_adjacent(caret, destination, scan.request))
    {
        return std::nullopt;
    }
    --scan.remaining;
    return scan.remaining == 0 ? std::optional<Offset>{destination} : std::nullopt;
}

[[nodiscard]] std::optional<Offset>
searched_forward(std::string_view content, Offset caret,
                 const VimCharacterSearchRequest &request) noexcept
{
    VimCharacterSearchScan scan{request, request.count};
    Offset at = vim_character_end(content, caret);
    while (at.value < content.size())
    {
        const auto destination = matched_destination(content, at, caret, scan);
        if (destination.has_value())
        {
            return destination;
        }
        at = vim_character_end(content, at);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Offset>
searched_backward(std::string_view content, Offset caret,
                  const VimCharacterSearchRequest &request) noexcept
{
    if (caret.value == 0)
    {
        return std::nullopt;
    }
    VimCharacterSearchScan scan{request, request.count};
    Offset at = vim_character_start(content, caret);
    while (true)
    {
        const auto destination = matched_destination(content, at, caret, scan);
        if (destination.has_value())
        {
            return destination;
        }
        if (at.value == 0)
        {
            return std::nullopt;
        }
        at = vim_character_start(content, at);
    }
}

[[nodiscard]] std::optional<Offset>
searched_in_line(std::string_view content, Offset caret,
                 const VimCharacterSearchRequest &request) noexcept
{
    return searches_forward(request.search.kind) ? searched_forward(content, caret, request)
                                                 : searched_backward(content, caret, request);
}
} // namespace

namespace detail
{
[[nodiscard]] std::optional<Offset>
character_search_position(const VimEditorView &view, const VimState &state,
                          VimCharacterSearch search, VimCharacterSearchInvocation invocation)
{
    const LineNumber line = view.text.position_of(view.selection.caret).line;
    const Offset start = view.text.line_start(line);
    const std::string content = view.text.text_range(start, view.text.line_end(line));
    const Offset caret{view.selection.caret.value - start.value};
    const VimCharacterSearchRequest request{search, resolved_count(state), invocation};
    const auto found = searched_in_line(content, caret, request);
    return found.has_value() ? std::optional<Offset>{Offset{start.value + found.value().value}}
                             : std::nullopt;
}
} // namespace detail

namespace
{
// `<Space>` の 1 歩（ADR 0049 の決定 2）。行の最後の文字の上からは、止まる側は行末の位置へ、
// 止まらない側は次の行の 0 桁目へ。行末の位置（空行の 0 桁目も）からは次の行の 0 桁目へ。
// 最終行で渡れなければその場に残る（呼び手が「動けない」失敗にする）。
[[nodiscard]] Offset wrapped_forward_step(const TextBuffer &text, Offset at, VimLineEndStop stop)
{
    const LineNumber line = line_of(text, at);
    const Offset end = text.line_end(line);
    const Offset next = forward_characters(text, at, single_step);
    if (next != end || (at != end && stop == VimLineEndStop::rests))
    {
        return next;
    }
    if (line.value >= text.line_count())
    {
        return at;
    }
    return text.line_start(LineNumber{line.value + 1});
}

// `<BS>` の 1 歩。対称に、0
// 桁目からは前の行の行末の位置（止まる側）か最後の文字（止まらない側）へ。
[[nodiscard]] Offset wrapped_backward_step(const TextBuffer &text, Offset at, VimLineEndStop stop)
{
    const LineNumber line = line_of(text, at);
    if (at != text.line_start(line))
    {
        return backward_characters(text, at, single_step);
    }
    if (line.value <= 1)
    {
        return at;
    }
    const Offset end = text.line_end(LineNumber{line.value - 1});
    return stop == VimLineEndStop::rests ? end : backward_characters(text, end, single_step);
}
} // namespace

namespace detail
{
// 回数ぶん 1 歩ずつ。途中で動けなくなったらそこで止まる（Vim は 1 歩も動けないときだけ鳴らす）。
[[nodiscard]] Offset wrapped_forward(const TextBuffer &text, Offset caret, std::size_t count,
                                     VimLineEndStop stop)
{
    Offset at = caret;
    for (std::size_t step = 0; step < count; ++step)
    {
        const Offset next = wrapped_forward_step(text, at, stop);
        if (next == at)
        {
            break;
        }
        at = next;
    }
    return at;
}

[[nodiscard]] Offset wrapped_backward(const TextBuffer &text, Offset caret, std::size_t count,
                                      VimLineEndStop stop)
{
    Offset at = caret;
    for (std::size_t step = 0; step < count; ++step)
    {
        const Offset next = wrapped_backward_step(text, at, stop);
        if (next == at)
        {
            break;
        }
        at = next;
    }
    return at;
}
} // namespace detail

namespace
{
// 移動としての `<Space>` `<BS>` が行末の位置に止まるか（ADR 0049 の決定 2）。VISUAL は止まり、
// 裸の NORMAL は止まらない（rested_in が VISUAL だけ行末の位置に載せるのと同じ理由）。
[[nodiscard]] VimLineEndStop line_end_stop_in(VimMode mode) noexcept
{
    switch (mode)
    {
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return VimLineEndStop::rests;
    case VimMode::normal:
    case VimMode::insert:
        return VimLineEndStop::passes;
    }
    std::unreachable();
}

[[nodiscard]] LineNumber first_view_line(const VimEditorView &view) noexcept
{
    return LineNumber{std::min(vim_viewport_of(view).first_visible.value, view.text.line_count())};
}
} // namespace

namespace detail
{
[[nodiscard]] LineNumber last_view_line(const VimEditorView &view) noexcept
{
    const std::size_t first = first_view_line(view).value;
    const std::size_t lines =
        std::max<std::size_t>(vim_viewport_of(view).visible_lines, single_step);
    const std::size_t remaining = view.text.line_count() - first;
    return LineNumber{first + std::min(lines - single_step, remaining)};
}

[[nodiscard]] LineNumber screen_line(const VimEditorView &view, VimScreenPosition position,
                                     std::size_t count) noexcept
{
    if (view.navigation.has_value())
    {
        return view.navigation.value().caret;
    }
    const LineNumber first = first_view_line(view);
    const LineNumber last = last_view_line(view);
    switch (position)
    {
    case VimScreenPosition::top:
        return LineNumber{first.value + std::min(count - single_step, last.value - first.value)};
    case VimScreenPosition::middle:
        return LineNumber{first.value + (last.value - first.value) / 2};
    case VimScreenPosition::bottom:
        return LineNumber{last.value - std::min(count - single_step, last.value - first.value)};
    }
    std::unreachable();
}

[[nodiscard]] LineNumber document_line(const TextBuffer &text, std::size_t count) noexcept
{
    return LineNumber{std::min(count, text.line_count())};
}
} // namespace detail

namespace
{
// `+` `-` `<CR>` の行き先。行が変わらなければ（最終行の `+`・1 行目の `-`）キャレットのまま
// 返し、j k と同じ「動けない」失敗にする（ADR 0048 の決定 9）。
[[nodiscard]] Offset first_non_blank_of_other_line(const TextBuffer &text, Offset caret,
                                                   LineNumber from, LineNumber target)
{
    if (target.value == from.value)
    {
        return caret;
    }
    return vim_first_non_blank(text, text.line_start(target));
}
} // namespace

namespace detail
{
// H M L の画面の位置。ほかの移動は呼ばない。
[[nodiscard]] VimScreenPosition screen_position_of(VimMotion motion) noexcept
{
    if (motion == VimMotion::screen_top)
    {
        return VimScreenPosition::top;
    }
    return motion == VimMotion::screen_middle ? VimScreenPosition::middle
                                              : VimScreenPosition::bottom;
}
} // namespace detail

namespace
{
// H M L の裸の移動の行。余白の内側へ寄せる（Vim の nv_scroll はオペレータの無いときだけ
// cursor_correct を呼ぶ・#1099・ADR 0019 の追補）。文字組みの道は寄せた行を navigation
// に置いてある。
[[nodiscard]] LineNumber settled_screen_line(const VimEditorView &view, LineNumber line) noexcept
{
    if (view.navigation.has_value())
    {
        return view.navigation.value().settled.value_or(line);
    }
    const LineNumber below{last_view_line(view).value + single_step};
    return vim_cursor_corrected(VimCursorWindow{first_view_line(view), below,
                                                vim_viewport_of(view).visible_lines,
                                                view.scroll_margin},
                                line, view.text.line_count());
}

// H M L の着地（画面の行の最初の非空白）。
[[nodiscard]] Offset screen_first_non_blank(const VimEditorView &view, VimScreenPosition position,
                                            std::size_t count)
{
    const LineNumber line = settled_screen_line(view, screen_line(view, position, count));
    return vim_first_non_blank(view.text, view.text.line_start(line));
}

// 裸の w W e E の着地（Issue #226）。本文の終わりの NUL の桁に止まったら本文の上へ戻し
// （Vim の adjust_cursor）、歩きの失敗の印はそのまま運ぶ。
[[nodiscard]] VimMotionLanding forward_word_landing(const VimEditorView &view,
                                                    const VimState &state, VimMotion motion)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const VimWordWalk walk{resolved_motion_count(state, text, motion),
                           vim_motion_word_class(motion)};
    const bool to_word_start = motion == VimMotion::next_word || motion == VimMotion::next_big_word;
    const VimMotionLanding walked =
        to_word_start ? vim_next_word(text, caret, walk, VimWordStop::across_lines)
                      : vim_word_end(text, caret, walk, VimWordEndStop::enter_the_next_word);
    return VimMotionLanding{rested_in(text, walked.offset, state.mode), walked.failure};
}
} // namespace

namespace detail
{
// gj gk の着地をモードで寄せる（ADR 0144 の決定 4）。着地は display_view が解いて置き、解けていない
// view では gj gk は j k に読み替わってここへ来ない（laid_out_motion）。
[[nodiscard]] VimMotionLanding row_landing_in(const VimEditorView &view, VimMode mode)
{
    if (!view.row_landing.has_value())
    {
        return VimMotionLanding{view.selection.caret, VimRepeatFailure::not_moved};
    }
    const VimMotionLanding &landing = view.row_landing.value().landing;
    return VimMotionLanding{rested_in(view.text, landing.offset, mode), landing.failure};
}
} // namespace detail

namespace
{
// j k の着地。今の欲しい列でその行へ（$ の「行末」はその行の最後の文字）。
[[nodiscard]] Offset wanted_caret_on(const TextBuffer &text, const VimState &state, Offset caret,
                                     LineNumber line)
{
    return caret_on_line(text, wanted_column_of(text, state, caret), line, state.mode);
}
} // namespace

namespace detail
{
// `{N}|` の着地。行頭からの仮想桁 N（Tab の中なら Tab・全角の 2 桁目ならその字）で、行より右なら
// NORMAL は最後の文字・VISUAL は行の内容の終わり（$ と同じ側）。空行は行頭（Issue #660・Vim の
// coladvance）。オペレータの範囲は NORMAL の寄せの着地で作る（d40| は最後の文字を残す）。
[[nodiscard]] Offset column_landing(const TextBuffer &text, LineNumber line, std::size_t count,
                                    VimMode mode)
{
    return rested_in(text, offset_at_virtual_column(text, line, VirtualColumn{count}), mode);
}
} // namespace detail

namespace
{
// g_ と _ の着地（Issue #721・ADR 0147 の決定 8・#976）。回数 − 1 行下（本文の末尾で
// 止まる）の行の最後の非空白（g_）と最初の非空白（_）。最終行で回数が 2 以上なら Vim の
// cursor_down が失敗し、動かずにオペレータを打ち消す。g_ は VISUAL でも行の内容の終わりには
// 載らない（Vim の nv_g_cmd が NUL から 1 つ戻す）。
[[nodiscard]] VimMotionLanding counted_line_landing(const TextBuffer &text, Offset caret,
                                                    std::size_t count, VimMotion motion)
{
    const LineNumber line = line_of(text, caret);
    if (count > single_step && line.value >= text.line_count())
    {
        return VimMotionLanding{caret, VimRepeatFailure::not_moved};
    }
    const LineNumber target = line_below(text, line, count - single_step);
    return VimMotionLanding{motion == VimMotion::last_non_blank
                                ? vim_last_non_blank(text, target)
                                : vim_first_non_blank(text, text.line_start(target))};
}
} // namespace

namespace detail
{
// `{N}%` の行（Vim の nv_percent）。本文の N % の行（切り上げ）で、1〜100 の外は無い。
[[nodiscard]] std::optional<LineNumber> percent_line(const TextBuffer &text, std::size_t count)
{
    if (count == 0 || count > whole_percent)
    {
        return std::nullopt;
    }
    const std::size_t line = (count * text.line_count() + percent_round_up) / whole_percent;
    return LineNumber{std::clamp<std::size_t>(line, single_step, text.line_count())};
}
} // namespace detail

namespace
{
// `%` の着地（Issue #767）。回数なしは対応する括弧、回数つきは本文の割合の行の最初の非空白。
// 見つからない・回数が 100 を越えるときは動かずに失敗する（Vim の clearopbeep）。
[[nodiscard]] VimMotionLanding matching_landing(const TextBuffer &text, Offset caret,
                                                std::size_t count)
{
    if (count == 0)
    {
        const auto matched = vim_matching_pair(text, caret);
        return matched.has_value() ? VimMotionLanding{matched.value()}
                                   : VimMotionLanding{caret, VimRepeatFailure::not_found};
    }
    const auto line = percent_line(text, count);
    if (!line.has_value())
    {
        return VimMotionLanding{caret, VimRepeatFailure::not_found};
    }
    return VimMotionLanding{vim_first_non_blank(text, text.line_start(line.value()))};
}
} // namespace

namespace detail
{
// マークへの移動の着地（ADR 0152 の決定 3）。`'` は行の最初の非空白、`` ` `` はマークの位置で、
// NORMAL では文字の上へ寄せる（Vim の check_cursor）。名前は鍵を写す前に engine が解いて置く。
[[nodiscard]] Offset mark_landing_of(const VimEditorView &view, VimMotion motion, VimMode mode)
{
    const Offset landing = view.mark_landing.value_or(view.selection.caret);
    if (motion == VimMotion::mark_line)
    {
        return vim_first_non_blank(view.text, view.text.line_start(line_of(view.text, landing)));
    }
    return rested_in(view.text, landing, mode);
}

[[nodiscard]] VimMotionLanding landing_of(const VimEditorView &view, const VimState &state,
                                          VimMotion motion);
} // namespace detail

namespace
{
// j k の着地（欲しい列の上の行・下の行）。
[[nodiscard]] Offset vertical_caret(const VimEditorView &view, const VimState &state,
                                    VimMotion motion, std::size_t count)
{
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(view.text, caret);
    return wanted_caret_on(view.text, state, caret,
                           motion == VimMotion::up ? line_above(line, count)
                                                   : line_below(view.text, line, count));
}

// `$` の着地（回数 − 1 行下の行の内容の終わり・本文の末尾で止まる・Issue #966）。最終行で回数が
// 2 以上なら Vim の nv_dollar の cursor_down が失敗し、動かずにオペレータを打ち消す（g_ と同じ）。
// 欲しい列は失敗でも「行末」（nv_dollar は cursor_down の前に curswant を貼る・wanted_after）。
[[nodiscard]] VimMotionLanding line_end_landing(const TextBuffer &text, Offset caret,
                                                std::size_t count, VimMode mode)
{
    const LineNumber line = line_of(text, caret);
    if (count > single_step && line.value >= text.line_count())
    {
        return VimMotionLanding{caret, VimRepeatFailure::not_moved};
    }
    return VimMotionLanding{
        rested_in(text, text.line_end(line_below(text, line, count - single_step)), mode)};
}

// H M L gg G の着地（行の最初の非空白）。
[[nodiscard]] Offset line_jump_landing(const VimEditorView &view, VimMotion motion,
                                       std::size_t count)
{
    if (motion == VimMotion::document_first || motion == VimMotion::document_last)
    {
        return vim_first_non_blank(view.text,
                                   view.text.line_start(document_line(view.text, count)));
    }
    return screen_first_non_blank(view, screen_position_of(motion), count);
}

// `<Space>` `<BS>` の着地（行をまたぐ l h・ADR 0049）。
[[nodiscard]] Offset wrapped_landing(const VimEditorView &view, const VimState &state,
                                     VimMotion motion)
{
    const std::size_t count = resolved_motion_count(state, view.text, motion);
    const VimLineEndStop stop = line_end_stop_in(state.mode);
    return motion == VimMotion::wrap_right
               ? wrapped_forward(view.text, view.selection.caret, count, stop)
               : wrapped_backward(view.text, view.selection.caret, count, stop);
}

// `+` `<CR>` と `-` の着地（下・上の行の最初の非空白）。
[[nodiscard]] Offset other_line_landing(const TextBuffer &text, Offset caret, bool below,
                                        std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    return first_non_blank_of_other_line(
        text, caret, line, below ? line_below(text, line, count) : line_above(line, count));
}
} // namespace

namespace detail
{
// 移動の着地。語の移動と $ g_ と、鍵を写す前に解いた着地（gj gk g0 g$ gm gM
// g^）は失敗の印といっしょに landing_of が決め、ここはその位置だけを読む。
[[nodiscard]] Offset moved_by(const VimEditorView &view, const VimState &state, VimMotion motion)
{
    const std::size_t count = resolved_motion_count(state, view.text, motion);
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(text, caret);
    switch (motion)
    {
    case VimMotion::left:
        return backward_characters(text, caret, count);
    case VimMotion::right:
        return rested_in(text, forward_characters(text, caret, count), state.mode);
    case VimMotion::up:
    case VimMotion::down:
        return vertical_caret(view, state, motion, count);
    case VimMotion::line_start:
        return text.line_start(line);
    case VimMotion::first_non_blank:
        return vim_first_non_blank(text, caret);
    case VimMotion::next_word:
    case VimMotion::next_big_word:
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
    case VimMotion::big_word_end:
    case VimMotion::big_word_end_for_change:
    case VimMotion::previous_word:
    case VimMotion::previous_big_word:
    case VimMotion::previous_word_end:
    case VimMotion::previous_big_word_end:
    case VimMotion::display_down:
    case VimMotion::display_up:
    case VimMotion::display_line_start:
    case VimMotion::display_line_end:
    case VimMotion::display_line_middle:
    case VimMotion::line_middle:
    case VimMotion::display_line_first_non_blank:
    case VimMotion::line_end:
    case VimMotion::last_non_blank:
    case VimMotion::first_non_blank_below:
    case VimMotion::matching_pair:
    case VimMotion::mark_line:
    case VimMotion::mark_exact:
        return landing_of(view, state, motion).offset;
    case VimMotion::screen_top:
    case VimMotion::screen_middle:
    case VimMotion::screen_bottom:
    case VimMotion::document_first:
    case VimMotion::document_last:
        return line_jump_landing(view, motion, count);
    case VimMotion::next_line:
    case VimMotion::previous_line:
        return other_line_landing(text, caret, motion == VimMotion::next_line, count);
    case VimMotion::wrap_right:
    case VimMotion::wrap_left:
        return wrapped_landing(view, state, motion);
    case VimMotion::column:
        return column_landing(text, line, count, state.mode);
    }
    std::unreachable();
}
} // namespace detail

namespace
{
// j k の着地と失敗の印（Issue #730）。Vim の cursor_down / cursor_up は最終行・最初の行にいるとき
// だけ失敗し、ほかは本文の端で止まる。動いたかどうかの位置では決めない: VISUAL で欲しい列が `$` の
// 「行末」なら、最終行の `j` も最後の文字から行の内容の終わりへ動いてしまうから（`$vjd`）。
[[nodiscard]] VimMotionLanding vertical_landing(const VimEditorView &view, const VimState &state,
                                                VimMotion motion)
{
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(view.text, caret);
    const bool at_edge =
        motion == VimMotion::up ? line.value <= 1 : line.value >= view.text.line_count();
    if (at_edge)
    {
        return VimMotionLanding{caret, VimRepeatFailure::not_moved};
    }
    return VimMotionLanding{moved_by(view, state, motion)};
}
} // namespace

namespace detail
{
// 移動の着地と失敗の印（Issue #224 / #226 / #730）。語の移動は Vim の歩き（bck_word
// bckend_word fwd_word end_word）の FAIL を、j k と回数つきの $ g_ は端の行の
// FAIL を持ち、ほかの移動は失敗を持たない（動かなかったときのビープは
// moved_or_failed が決める）。前向きの失敗は裸の移動と VISUAL だけが見て、
// オペレータの範囲は motion_range が着地だけで作る（Vim の nv_wordcmd は FAIL を
// OP_NOP のときだけ見る）。
[[nodiscard]] VimMotionLanding landing_of(const VimEditorView &view, const VimState &state,
                                          VimMotion motion)
{
    const VimWordWalk walk{resolved_motion_count(state, view.text, motion),
                           vim_motion_word_class(motion)};
    switch (motion)
    {
    case VimMotion::previous_word:
    case VimMotion::previous_big_word:
        return vim_previous_word(view.text, view.selection.caret, walk);
    case VimMotion::previous_word_end:
    case VimMotion::previous_big_word_end:
        return vim_previous_word_end(view.text, view.selection.caret, walk);
    case VimMotion::next_word:
    case VimMotion::next_big_word:
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
    case VimMotion::big_word_end:
    case VimMotion::big_word_end_for_change:
        return forward_word_landing(view, state, motion);
    case VimMotion::display_down:
    case VimMotion::display_up:
    case VimMotion::display_line_start:
    case VimMotion::display_line_end:
    case VimMotion::display_line_middle:
    case VimMotion::line_middle:
    case VimMotion::display_line_first_non_blank:
        return row_landing_in(view, state.mode);
    case VimMotion::line_end:
        return line_end_landing(view.text, view.selection.caret,
                                resolved_motion_count(state, view.text, motion), state.mode);
    case VimMotion::last_non_blank:
    case VimMotion::first_non_blank_below:
        return counted_line_landing(view.text, view.selection.caret,
                                    resolved_motion_count(state, view.text, motion), motion);
    case VimMotion::up:
    case VimMotion::down:
        return vertical_landing(view, state, motion);
    case VimMotion::matching_pair:
        return matching_landing(view.text, view.selection.caret,
                                resolved_motion_count(state, view.text, motion));
    case VimMotion::mark_line:
    case VimMotion::mark_exact:
        return VimMotionLanding{mark_landing_of(view, motion, state.mode)};
    case VimMotion::left:
    case VimMotion::right:
    case VimMotion::line_start:
    case VimMotion::first_non_blank:
    case VimMotion::screen_top:
    case VimMotion::screen_middle:
    case VimMotion::screen_bottom:
    case VimMotion::document_first:
    case VimMotion::document_last:
    case VimMotion::next_line:
    case VimMotion::previous_line:
    case VimMotion::wrap_right:
    case VimMotion::wrap_left:
    case VimMotion::column:
        return VimMotionLanding{moved_by(view, state, motion)};
    }
    std::unreachable();
}

// 移動のあとの欲しい列。j / k は前の値を引き継ぎ、$ は「行末」を貼り、ほかは動いた先の桁。
// gj gk と `{N}|` は wanted_for が motion と回数から決めた値を引き継ぐ（ADR 0144 の決定 3・
// Issue #660）。
[[nodiscard]] VimWantedColumn wanted_after(const VimWantedColumn &wanted, VirtualColumn landed,
                                           VimMotion motion)
{
    switch (motion)
    {
    case VimMotion::up:
    case VimMotion::down:
    case VimMotion::display_down:
    case VimMotion::display_up:
    case VimMotion::column:
        return wanted;
    case VimMotion::line_end:
        return VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    case VimMotion::display_line_end:
        // 回数 1 は着地の桁、折り返す文書の {N}g$ だけが行末の印（ADR 0147 の決定 4）。
        return wanted.wish == VimColumnWish::at_line_end
                   ? wanted
                   : VimWantedColumn{VimColumnWish::at_column, landed};
    case VimMotion::left:
    case VimMotion::right:
    case VimMotion::line_start:
    case VimMotion::first_non_blank:
    case VimMotion::next_word:
    case VimMotion::previous_word:
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
    case VimMotion::screen_top:
    case VimMotion::screen_middle:
    case VimMotion::screen_bottom:
    case VimMotion::document_first:
    case VimMotion::document_last:
    case VimMotion::next_line:
    case VimMotion::previous_line:
    case VimMotion::wrap_right:
    case VimMotion::wrap_left:
    case VimMotion::next_big_word:
    case VimMotion::previous_big_word:
    case VimMotion::big_word_end:
    case VimMotion::big_word_end_for_change:
    case VimMotion::previous_word_end:
    case VimMotion::previous_big_word_end:
    case VimMotion::display_line_start:
    case VimMotion::display_line_middle:
    case VimMotion::line_middle:
    case VimMotion::display_line_first_non_blank:
    case VimMotion::last_non_blank:
    case VimMotion::first_non_blank_below:
    case VimMotion::matching_pair:
    case VimMotion::mark_line:
    case VimMotion::mark_exact:
        return VimWantedColumn{VimColumnWish::at_column, landed};
    }
    std::unreachable();
}

// 移動に渡す欲しい列。gj gk は画面行の問いで着地と一緒に解いた後の値、`{N}|` は着地の桁ではなく
// 回数の桁 N（持つのは 1 始まりの N・Vim の nv_pipe の curswant は N − 1・空行でも行より右でも
// 同じ・Issue #660）で、ほかは今の値。
[[nodiscard]] VimWantedColumn wanted_for(const VimEditorView &view, const VimState &state,
                                         VimMotion motion)
{
    const bool by_rows = motion == VimMotion::display_down || motion == VimMotion::display_up ||
                         motion == VimMotion::display_line_end;
    if (by_rows && view.row_landing.has_value())
    {
        return view.row_landing.value().wanted;
    }
    if (motion == VimMotion::column)
    {
        return VimWantedColumn{VimColumnWish::at_column,
                               VirtualColumn{resolved_motion_count(state, view.text, motion)}};
    }
    return wanted_column_of(view.text, state, view.selection.caret);
}

[[nodiscard]] VimStep cancelled(const VimState &state)
{
    return VimStep{vim_resting_from(state, state.unnamed_register), VimNoEffect{}};
}

// 閉じた失敗（Vim のビープ）の印を添える（ADR 0046 の決定 3）。状態と効果は変えない。
[[nodiscard]] VimStep failed(VimStep step, VimRepeatFailure failure)
{
    step.failure = failure;
    return step;
}
} // namespace detail

namespace
{
// 動かなかったらビープする移動（h l j k w b e・Vim は 1 つも動けないときだけ鳴らす）。
// 0 $ ^ H M L gg G は動かなくても鳴らない（ADR 0046 の決定 3）。
[[nodiscard]] bool fails_unmoved(VimMotion motion) noexcept
{
    switch (motion)
    {
    case VimMotion::left:
    case VimMotion::right:
    case VimMotion::up:
    case VimMotion::down:
    case VimMotion::next_word:
    case VimMotion::previous_word:
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
    case VimMotion::next_line:
    case VimMotion::previous_line:
    case VimMotion::wrap_right:
    case VimMotion::wrap_left:
    case VimMotion::next_big_word:
    case VimMotion::previous_big_word:
    case VimMotion::big_word_end:
    case VimMotion::big_word_end_for_change:
    case VimMotion::previous_word_end:
    case VimMotion::previous_big_word_end:
    case VimMotion::display_down:
    case VimMotion::display_up:
        return true;
    case VimMotion::line_start:
    case VimMotion::first_non_blank:
    case VimMotion::line_end:
    case VimMotion::screen_top:
    case VimMotion::screen_middle:
    case VimMotion::screen_bottom:
    case VimMotion::document_first:
    case VimMotion::document_last:
    case VimMotion::column:
    case VimMotion::display_line_start:
    case VimMotion::display_line_end:
    case VimMotion::display_line_middle:
    case VimMotion::line_middle:
    case VimMotion::display_line_first_non_blank:
    case VimMotion::last_non_blank:
    case VimMotion::first_non_blank_below:
    case VimMotion::matching_pair:
    case VimMotion::mark_line:
    case VimMotion::mark_exact:
        return false;
    }
    std::unreachable();
}
} // namespace

namespace detail
{
// 移動が失敗を返したか（Issue #224）、1 つも動けなかったらビープする。
[[nodiscard]] VimStep moved_or_failed(VimStep step, VimMotion motion, Offset from,
                                      const VimMotionLanding &landing)
{
    if (landing.failure.has_value())
    {
        return failed(std::move(step), landing.failure.value());
    }
    if (landing.offset == from && fails_unmoved(motion))
    {
        return failed(std::move(step), VimRepeatFailure::not_moved);
    }
    return step;
}

// 飛ぶ鍵の前の位置（キャレット）を `''` に置いた値を添える（ADR 0152 の決定 4・Vim の setpcmark）。
// 動かなかったときに戻すのは鍵の終わりの vim_marks_checked（決定 5）。
[[nodiscard]] VimStep context_marked(VimStep step, const VimEditorView &view)
{
    step.marks =
        vim_marks_jumped(*view.marks, vim_mark_position_of(view.text, view.selection.caret));
    return step;
}

// 飛ぶ移動なら前の位置を添える。失敗した移動は置かない（Vim は見つからない `%` で setpcmark
// しない）。
[[nodiscard]] VimStep jumped_by(VimStep step, const VimEditorView &view, VimMotion motion)
{
    if (!vim_motion_is_jump(motion) || step.failure.has_value())
    {
        return step;
    }
    return context_marked(std::move(step), view);
}

// 移動の着地・欲しい列・失敗の印（前の位置は置かない）。鍵の移動は motion_step が前の位置を
// 足し、Ex の `:N` は jump ではないのでこのまま使う（Issue #847）。
[[nodiscard]] VimStep landed_step(const VimState &state, const VimEditorView &view,
                                  VimMotion motion)
{
    const VimWantedColumn wanted = wanted_for(view, state, motion);
    const VimMotionLanding landing = landing_of(view, state, motion);
    const Offset moved = landing.offset;
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.wanted_column = wanted_after(
        wanted, landed_column(view.text, state.mode, view.selection.anchor, moved), motion);
    return moved_or_failed(VimStep{std::move(next), VimMoveTo{moved}}, motion, view.selection.caret,
                           landing);
}

[[nodiscard]] VimStep motion_step(const VimState &state, const VimEditorView &view,
                                  VimMotion motion)
{
    return jumped_by(landed_step(state, view, motion), view, motion);
}

// `Ctrl-o` `Ctrl-i` `g;` `g,`（ADR 0152 の追補 #1037）。列を歩いた値を添え、着いた位置へ
// `` `a `` と同じ着地（NORMAL の寄せ・欲しい列は着地の桁）で動く。列の端と本文の外の行（E19）は
// 鳴らす失敗で、失敗でも歩いた値（片付けと今の位置の積み）は残す（Vim の movemark）。
[[nodiscard]] VimStep jump_list_action(const VimState &state, const VimEditorView &view,
                                       VimAction action)
{
    const std::size_t count = resolved_count(state);
    const bool older = action == VimAction::jump_older || action == VimAction::change_older;
    const bool changes = action == VimAction::change_older || action == VimAction::change_newer;
    VimJumpMove move =
        changes ? vim_change_moved(*view.marks, older, count)
                : vim_jump_moved(*view.marks, vim_mark_position_of(view.text, view.selection.caret),
                                 older, count);
    const std::optional<Offset> landing =
        move.target.has_value() ? vim_mark_offset(view.text, move.target.value()) : std::nullopt;
    if (!landing.has_value())
    {
        VimStep unmoved{vim_resting_from(state, state.unnamed_register), VimNoEffect{}};
        unmoved.marks = std::move(move.marks);
        return failed(std::move(unmoved), VimRepeatFailure::not_found);
    }
    VimEditorView marked = view;
    marked.mark_landing = landing.value();
    VimStep step = landed_step(state, marked, VimMotion::mark_exact);
    step.marks = std::move(move.marks);
    return step;
}
} // namespace detail
} // namespace nenenib::core
