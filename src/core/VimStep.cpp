#include "VimStep.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "Column.hpp"
#include "LineNumber.hpp"
#include "OffsetRange.hpp"
#include "ScrollBounds.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBinding.hpp"
#include "VimCaret.hpp"
#include "VimCharacterSearch.hpp"
#include "VimCharacterSearchInvocation.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimCharacterSearchRequest.hpp"
#include "VimCharacterSearchScan.hpp"
#include "VimInputWait.hpp"
#include "VimMotionBinding.hpp"
#include "VimMotionRange.hpp"
#include "VimPrefix.hpp"
#include "VimPutSide.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimScreenPosition.hpp"
#include "VimScrollDirection.hpp"
#include "VimSelect.hpp"
#include "VimVisualRange.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
constexpr std::size_t single_step = 1;
constexpr std::size_t decimal_base = 10;
constexpr char32_t line_feed = U'\n';
constexpr char carriage_return = '\r';
// Ctrl-r は文字としては制御文字なので、表に載せる値は名前で書く（原文に制御文字を置かない）。
constexpr char32_t control_r_character = 0x12;
constexpr char32_t control_b_character = 0x02;
constexpr char32_t control_d_character = 0x04;
constexpr char32_t control_f_character = 0x06;
constexpr char32_t control_u_character = 0x15;
constexpr char32_t carriage_return_character = 0x0D;

// NORMAL の鍵 → 動作の表（ADR 0012 の決定 5 / ADR 0015 の決定 6 / CPP-012）。分岐で書くと
// 関数長で落ちる（T8）。数字は表に無い。回数として積むほうが先で、'0' だけは回数が空のときに
// 行頭として引かれる。
constexpr std::array<VimBinding, 40> normal_bindings{
    {{U'h', VimAction::move_left},
     {U'j', VimAction::move_down},
     {U'k', VimAction::move_up},
     {U'l', VimAction::move_right},
     {U'0', VimAction::move_line_start},
     {U'$', VimAction::move_line_end},
     {U'w', VimAction::move_next_word},
     {U'b', VimAction::move_previous_word},
     {U'e', VimAction::move_word_end},
     {U'^', VimAction::move_first_non_blank},
     {U'H', VimAction::move_screen_top},
     {U'M', VimAction::move_screen_middle},
     {U'L', VimAction::move_screen_bottom},
     {U'G', VimAction::move_document_last},
     {U'x', VimAction::remove_character},
     {U'd', VimAction::remove_operator},
     {U'c', VimAction::change_operator},
     {U'y', VimAction::yank_operator},
     {U'p', VimAction::put_after},
     {U'P', VimAction::put_before},
     {U'D', VimAction::remove_to_line_end},
     {U'C', VimAction::change_to_line_end},
     {U'Y', VimAction::yank_line},
     {U'i', VimAction::insert_before},
     {U'a', VimAction::insert_after},
     {U'I', VimAction::insert_at_line_start},
     {U'A', VimAction::insert_at_line_end},
     {U'u', VimAction::undo},
     {control_r_character, VimAction::redo},
     {U'v', VimAction::visual},
     {U'V', VimAction::visual_line},
     {U'o', VimAction::swap_visual_ends},
     {U'f', VimAction::find_character_forward},
     {U'F', VimAction::find_character_backward},
     {U't', VimAction::till_character_forward},
     {U'T', VimAction::till_character_backward},
     {U';', VimAction::repeat_character_search},
     {U',', VimAction::repeat_character_search_opposite},
     {U'g', VimAction::prefix_g},
     {U':', VimAction::open_command_line}}};

// オペレータの後ろで範囲になる動作。ここに無い鍵（x i a …）は保留中のオペレータを打ち消す。
constexpr std::array<VimMotionBinding, 15> motion_bindings{
    {{VimAction::move_left, VimMotion::left},
     {VimAction::move_down, VimMotion::down},
     {VimAction::move_up, VimMotion::up},
     {VimAction::move_right, VimMotion::right},
     {VimAction::move_line_start, VimMotion::line_start},
     {VimAction::move_line_end, VimMotion::line_end},
     {VimAction::move_next_word, VimMotion::next_word},
     {VimAction::move_previous_word, VimMotion::previous_word},
     {VimAction::move_word_end, VimMotion::word_end},
     {VimAction::move_first_non_blank, VimMotion::first_non_blank},
     {VimAction::move_screen_top, VimMotion::screen_top},
     {VimAction::move_screen_middle, VimMotion::screen_middle},
     {VimAction::move_screen_bottom, VimMotion::screen_bottom},
     {VimAction::move_document_first, VimMotion::document_first},
     {VimAction::move_document_last, VimMotion::document_last}}};

// 終わりの位置を範囲に入れない移動（Vim の exclusive）。$ と e は入れる（inclusive）。
// 行単位の j k はどちらでもないので、この表には無い。
constexpr std::array<VimMotion, 6> exclusive_motions{
    {VimMotion::left, VimMotion::right, VimMotion::line_start, VimMotion::first_non_blank,
     VimMotion::next_word, VimMotion::previous_word}};

[[nodiscard]] std::optional<VimAction> action_for(char32_t key) noexcept
{
    for (const VimBinding binding : normal_bindings)
    {
        if (binding.key == key)
        {
            return binding.action;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<VimMotion> motion_for(VimAction action) noexcept
{
    for (const VimMotionBinding binding : motion_bindings)
    {
        if (binding.action == action)
        {
            return binding.motion;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool exclusive(VimMotion motion) noexcept
{
    for (const VimMotion entry : exclusive_motions)
    {
        if (entry == motion)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::size_t count_of(const std::optional<VimCount> &count) noexcept
{
    return count.has_value() ? count.value().value : single_step;
}

// 明示された動作回数。オペレータ側と移動側のどちらも指定されたときは飽和する積にする。
// どちらも無ければ nullopt のまま保ち、G の既定値だけを motion 解決時に最終行へ写せるようにする。
[[nodiscard]] std::optional<VimCount> combined_count(const VimState &state) noexcept
{
    const auto pending = state.pending.has_value() ? state.pending.value().count : std::nullopt;
    if (!pending.has_value() && !state.count.has_value())
    {
        return std::nullopt;
    }
    const std::size_t operation = count_of(pending);
    const std::size_t motion = count_of(state.count);
    const std::size_t highest = std::numeric_limits<std::size_t>::max();
    return VimCount{operation > highest / motion ? highest : operation * motion};
}

[[nodiscard]] std::size_t resolved_count(const VimState &state) noexcept
{
    return count_of(combined_count(state));
}

[[nodiscard]] std::size_t resolved_motion_count(const VimState &state, const TextBuffer &text,
                                                VimMotion motion) noexcept
{
    const auto specified = combined_count(state);
    if (specified.has_value())
    {
        return specified.value().value;
    }
    return motion == VimMotion::document_last ? text.line_count() : single_step;
}

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

[[nodiscard]] Offset search_destination(std::string_view content, Offset match,
                                        VimCharacterSearchKind kind) noexcept
{
    switch (kind)
    {
    case VimCharacterSearchKind::find_forward:
    case VimCharacterSearchKind::find_backward:
        return match;
    case VimCharacterSearchKind::till_forward:
        return previous_code_point(content, match);
    case VimCharacterSearchKind::till_backward:
        return next_code_point(content, match);
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
    Offset at = next_code_point(content, caret);
    while (at.value < content.size())
    {
        const auto destination = matched_destination(content, at, caret, scan);
        if (destination.has_value())
        {
            return destination;
        }
        at = next_code_point(content, at);
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
    Offset at = previous_code_point(content, caret);
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
        at = previous_code_point(content, at);
    }
}

[[nodiscard]] std::optional<Offset>
searched_in_line(std::string_view content, Offset caret,
                 const VimCharacterSearchRequest &request) noexcept
{
    return searches_forward(request.search.kind) ? searched_forward(content, caret, request)
                                                 : searched_backward(content, caret, request);
}

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

[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset caret)
{
    return text.position_of(caret).line;
}

// 行の中で count 文字ぶん先。行の内容の終わり（Vim が NUL を置く桁）までは進める。
[[nodiscard]] Offset forward_characters(const TextBuffer &text, Offset caret, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = caret.value - start.value;
    for (std::size_t step = 0; step < count && index < content.size(); ++step)
    {
        index = next_code_point(content, Offset{index}).value;
    }
    return Offset{start.value + index};
}

[[nodiscard]] Offset backward_characters(const TextBuffer &text, Offset caret, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = caret.value - start.value;
    for (std::size_t step = 0; step < count && index > 0; ++step)
    {
        index = previous_code_point(content, Offset{index}).value;
    }
    return Offset{start.value + index};
}

// キャレットの下に空白でない文字があるか（Vim の gchar_cursor() != NUL && !VIM_ISWHITE）。
// 行の内容の終わり（NUL の桁）と空行は「文字が無い」ので偽。cw の特例が効く条件そのもの。
[[nodiscard]] bool word_under_caret(const TextBuffer &text, Offset caret)
{
    const LineNumber line = line_of(text, caret);
    if (caret.value >= text.line_end(line).value)
    {
        return false;
    }
    const std::string under = text.text_range(caret, forward_characters(text, caret, single_step));
    return under != " " && under != "\t";
}

[[nodiscard]] VimWantedColumn wanted_column_of(const TextBuffer &text, const VimState &state,
                                               Offset caret)
{
    if (state.wanted_column.has_value())
    {
        return state.wanted_column.value();
    }
    return VimWantedColumn{VimColumnWish::at_column, text.position_of(caret).column};
}

// 動いた先のキャレットの置き所。NORMAL / INSERT は文字の上に寄せ、VISUAL は行の内容の終わり
// （Vim が NUL を置く桁）にも載せる（ADR 0018 の決定 6）。Vim の coladvance の one_more が
// VIsual_active で立つのと同じで、`v$d` が改行まで消し、`vl` が行末に届くのはこれ（Issue #53
// で実測）。
[[nodiscard]] Offset rested_in(const TextBuffer &text, Offset caret, VimMode mode)
{
    switch (mode)
    {
    case VimMode::visual:
    case VimMode::visual_line:
        return caret;
    case VimMode::normal:
    case VimMode::insert:
        return vim_resting_caret(text, caret);
    }
    std::unreachable();
}

// 欲しい列（curswant）で別の行へ。$ が貼り付けた「行末」はその行の最後の文字になる。
[[nodiscard]] Offset caret_on_line(const TextBuffer &text, const VimWantedColumn &wanted,
                                   LineNumber line, VimMode mode)
{
    switch (wanted.wish)
    {
    case VimColumnWish::at_line_end:
        return rested_in(text, text.line_end(line), mode);
    case VimColumnWish::at_column:
        return rested_in(text, text.offset_of(TextPosition{line, wanted.column}), mode);
    }
    std::unreachable();
}

[[nodiscard]] LineNumber line_below(const TextBuffer &text, LineNumber line, std::size_t count)
{
    const std::size_t current = std::min(line.value, text.line_count());
    const std::size_t step = std::min(count, text.line_count() - current);
    return LineNumber{current + step};
}

[[nodiscard]] LineNumber line_above(LineNumber line, std::size_t count)
{
    return LineNumber{line.value > count ? line.value - count : 1};
}

[[nodiscard]] LineNumber first_view_line(const VimEditorView &view) noexcept
{
    return LineNumber{std::min(view.viewport.first_visible.value, view.text.line_count())};
}

[[nodiscard]] LineNumber last_view_line(const VimEditorView &view) noexcept
{
    const std::size_t first = first_view_line(view).value;
    const std::size_t lines = std::max<std::size_t>(view.viewport.visible_lines, single_step);
    const std::size_t remaining = view.text.line_count() - first;
    return LineNumber{first + std::min(lines - single_step, remaining)};
}

[[nodiscard]] LineNumber screen_line(const VimEditorView &view, VimScreenPosition position,
                                     std::size_t count) noexcept
{
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
        return caret_on_line(text, wanted_column_of(text, state, caret), line_above(line, count),
                             state.mode);
    case VimMotion::down:
        return caret_on_line(text, wanted_column_of(text, state, caret),
                             line_below(text, line, count), state.mode);
    case VimMotion::line_start:
        return text.line_start(line);
    case VimMotion::first_non_blank:
        return vim_first_non_blank(text, caret);
    case VimMotion::line_end:
        return rested_in(text, text.line_end(line_below(text, line, count - single_step)),
                         state.mode);
    case VimMotion::next_word:
        return rested_in(text, vim_next_word(text, caret, count, VimWordStop::across_lines),
                         state.mode);
    case VimMotion::previous_word:
        return vim_previous_word(text, caret, count);
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
        return rested_in(text,
                         vim_word_end(text, caret, count, VimWordEndStop::enter_the_next_word),
                         state.mode);
    case VimMotion::screen_top:
        return vim_first_non_blank(
            text, text.line_start(screen_line(view, VimScreenPosition::top, count)));
    case VimMotion::screen_middle:
        return vim_first_non_blank(
            text, text.line_start(screen_line(view, VimScreenPosition::middle, count)));
    case VimMotion::screen_bottom:
        return vim_first_non_blank(
            text, text.line_start(screen_line(view, VimScreenPosition::bottom, count)));
    case VimMotion::document_first:
    case VimMotion::document_last:
        return vim_first_non_blank(text, text.line_start(document_line(text, count)));
    }
    std::unreachable();
}

// 移動のあとの欲しい列。j / k は前の値を引き継ぎ、$ は「行末」を貼り、ほかは動いた先の桁。
[[nodiscard]] VimWantedColumn wanted_after(const TextBuffer &text, const VimWantedColumn &wanted,
                                           Offset moved, VimMotion motion)
{
    switch (motion)
    {
    case VimMotion::up:
    case VimMotion::down:
        return wanted;
    case VimMotion::line_end:
        return VimWantedColumn{VimColumnWish::at_line_end, Column{1}};
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
        return VimWantedColumn{VimColumnWish::at_column, text.position_of(moved).column};
    }
    std::unreachable();
}

[[nodiscard]] VimStep cancelled(const VimState &state)
{
    return VimStep{vim_resting_from(state, state.unnamed_register), VimNoEffect{}};
}

[[nodiscard]] VimStep motion_step(const VimState &state, const VimEditorView &view,
                                  VimMotion motion)
{
    const VimWantedColumn wanted = wanted_column_of(view.text, state, view.selection.caret);
    const Offset moved = moved_by(view, state, motion);
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.wanted_column = wanted_after(view.text, wanted, moved, motion);
    return VimStep{std::move(next), VimMoveTo{moved}};
}

// ---------------------------------------------------------------- 範囲（ADR 0015 の決定 2）

// 文字単位の範囲。^ は前にも後ろにも動くので、端は必ず小さいほうを先にする（Vim も同じ）。
[[nodiscard]] VimMotionRange characters_between(Offset first, Offset second)
{
    const bool ordered = first.value <= second.value;
    return VimMotionRange{OffsetRange{ordered ? first : second, ordered ? second : first},
                          VimRegisterKind::characters};
}

// 行単位の範囲。最初の行の先頭から、最後の行の内容の終わりまで（改行は入れない）。
[[nodiscard]] VimMotionRange lines_between(const TextBuffer &text, LineNumber first,
                                           LineNumber last)
{
    return VimMotionRange{OffsetRange{text.line_start(first), text.line_end(last)},
                          VimRegisterKind::lines};
}

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

[[nodiscard]] std::optional<VimMotionRange> lines_above(const TextBuffer &text, LineNumber line,
                                                        std::size_t steps)
{
    if (steps > 0 && line.value <= 1)
    {
        return std::nullopt;
    }
    return lines_between(text, line_above(line, steps), line);
}

// $ は inclusive で、回数があると count-1 行下の行末まで（Vim の nv_dollar の cursor_down）。
[[nodiscard]] std::optional<VimMotionRange> to_line_end(const TextBuffer &text, Offset caret,
                                                        std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    if (count > single_step && line.value >= text.line_count())
    {
        return std::nullopt;
    }
    return characters_between(caret, text.line_end(line_below(text, line, count - single_step)));
}

// inclusive な移動の範囲の終わり。止まった文字を範囲に入れる（行の内容の終わりは越えない）。
[[nodiscard]] Offset inclusive_end(const TextBuffer &text, Offset at)
{
    return forward_characters(text, at, single_step);
}

[[nodiscard]] VimMotionRange screen_range(const VimEditorView &view, VimScreenPosition position,
                                          std::size_t count, LineNumber line)
{
    const LineNumber target = screen_line(view, position, count);
    const LineNumber first{std::min(line.value, target.value)};
    const LineNumber last{std::max(line.value, target.value)};
    return lines_between(view.text, first, last);
}

[[nodiscard]] std::optional<VimMotionRange> motion_range(const VimEditorView &view,
                                                         VimMotion motion, std::size_t count)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(text, caret);
    switch (motion)
    {
    case VimMotion::down:
        return lines_below(text, line, count);
    case VimMotion::up:
        return lines_above(text, line, count);
    case VimMotion::left:
        return characters_between(backward_characters(text, caret, count), caret);
    case VimMotion::right:
        return characters_between(caret, forward_characters(text, caret, count));
    case VimMotion::line_start:
        return characters_between(text.line_start(line), caret);
    case VimMotion::first_non_blank:
        return characters_between(vim_first_non_blank(text, caret), caret);
    case VimMotion::line_end:
        return to_line_end(text, caret, count);
    case VimMotion::next_word:
        return characters_between(caret,
                                  vim_next_word(text, caret, count, VimWordStop::at_line_end));
    case VimMotion::previous_word:
        return characters_between(vim_previous_word(text, caret, count), caret);
    case VimMotion::word_end:
        return characters_between(
            caret, inclusive_end(text, vim_word_end(text, caret, count,
                                                    VimWordEndStop::enter_the_next_word)));
    case VimMotion::word_end_for_change:
        return characters_between(
            caret, inclusive_end(
                       text, vim_word_end(text, caret, count, VimWordEndStop::stay_in_this_word)));
    case VimMotion::screen_top:
        return screen_range(view, VimScreenPosition::top, count, line);
    case VimMotion::screen_middle:
        return screen_range(view, VimScreenPosition::middle, count, line);
    case VimMotion::screen_bottom:
        return screen_range(view, VimScreenPosition::bottom, count, line);
    case VimMotion::document_first:
    case VimMotion::document_last:
    {
        const LineNumber target = document_line(text, count);
        return lines_between(text, LineNumber{std::min(line.value, target.value)},
                             LineNumber{std::max(line.value, target.value)});
    }
    }
    std::unreachable();
}

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

// :help exclusive の 2 つの言い換え。行頭で終わる exclusive な移動は 1 つ前の行の末尾までになり、
// 始まりが字下げの中なら行単位になる。dw が空行を丸ごと消すのも、db が上の行を消すのもこれ。
[[nodiscard]] VimMotionRange adjusted_for_exclusive(const TextBuffer &text,
                                                    const VimMotionRange &range, VimMotion motion)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    if (!exclusive(motion) || first.value >= last.value || range.range.end != text.line_start(last))
    {
        return range;
    }
    const LineNumber previous{last.value - 1};
    if (starts_in_the_indent(text, range.range.begin))
    {
        return lines_between(text, first, previous);
    }
    return characters_between(range.range.begin, text.line_end(previous));
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
    return lines_between(text, first, last);
}

// ---------------------------------------------------------------- オペレータ（決定 2・3）

// レジスタの本文は LF だけ（決定 3）。CRLF の文書から取った本文はここで CR を落とす。
[[nodiscard]] std::string without_carriage_returns(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char byte : text)
    {
        if (byte != carriage_return)
        {
            result.push_back(byte);
        }
    }
    return result;
}

[[nodiscard]] VimRegister register_of(const TextBuffer &text, const VimMotionRange &range)
{
    std::string body =
        without_carriage_returns(text.text_range(range.range.begin, range.range.end));
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
        // Motion ranges are constructed only as characterwise or linewise.
        std::unreachable();
    case VimRegisterKind::lines:
        // 行単位のレジスタは必ず改行で終わる（Vim は最終行にも付ける）。
        body.push_back('\n');
        return VimRegister{std::move(body), VimRegisterKind::lines};
    case VimRegisterKind::characters:
        return VimRegister{std::move(body), VimRegisterKind::characters};
    }
    std::unreachable();
}

// 空の文字単位の範囲は無名レジスタを書き換えない（空行の D が実測でそう振る舞う）。
[[nodiscard]] VimRegister register_after(const VimState &state, const TextBuffer &text,
                                         const VimMotionRange &range)
{
    if (range.kind == VimRegisterKind::characters && is_empty(range.range))
    {
        return state.unnamed_register;
    }
    return register_of(text, range);
}

// 行を消す範囲。最終行を消すときだけ 1 つ前の行の末尾から始める（改行を 1 つだけ消すため）。
[[nodiscard]] OffsetRange removed_lines_range(const TextBuffer &text, const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    const bool through_end = last.value >= text.line_count();
    const Offset begin = through_end && first.value > 1 ? text.line_end(LineNumber{first.value - 1})
                                                        : range.range.begin;
    return OffsetRange{begin, text.line_terminator_end(last)};
}

// 範囲をそのまま消す。VISUAL の `d` `x` はここへ直接来る（Vim の op_delete の「奇妙な Vi の
// 振る舞い」は `!oap->is_VIsual` で守られていて、選択からの削除には掛からない。Issue #53 で実測）。
[[nodiscard]] VimStep removed_exactly(const VimState &state, const TextBuffer &text,
                                      const VimMotionRange &range)
{
    VimState next = vim_resting_from(state, register_after(state, text, range));
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
        // Motion ranges are constructed only as characterwise or linewise.
        std::unreachable();
    case VimRegisterKind::characters:
        return VimStep{std::move(next), VimRemoveRange{range.range}};
    case VimRegisterKind::lines:
        return VimStep{std::move(next), VimRemoveLines{removed_lines_range(text, range)}};
    }
    std::unreachable();
}

[[nodiscard]] VimStep removed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range)
{
    return removed_exactly(state, text, whole_lines_for_delete(text, range));
}

// c。効果は削除そのままで、次の状態が INSERT（決定 2）。行単位でも改行は残す＝行が 1 本残る。
[[nodiscard]] VimStep changed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range)
{
    VimState next = vim_resting_from(state, register_after(state, text, range));
    next.mode = VimMode::insert;
    return VimStep{std::move(next), VimRemoveRange{range.range}};
}

// y のあとのキャレット。文字単位は範囲の先頭、行単位は範囲の最初の行の同じ桁（実測）。
// y は VISUAL を終わらせるので、置き所は NORMAL の規則で寄せる。
[[nodiscard]] Offset yanked_caret(const TextBuffer &text, const VimState &state, Offset caret,
                                  const VimMotionRange &range)
{
    switch (range.kind)
    {
    case VimRegisterKind::uninitialized:
        // Motion ranges are constructed only as characterwise or linewise.
        std::unreachable();
    case VimRegisterKind::characters:
        return range.range.begin;
    case VimRegisterKind::lines:
        return caret_on_line(text, wanted_column_of(text, state, caret),
                             line_of(text, range.range.begin), VimMode::normal);
    }
    std::unreachable();
}

[[nodiscard]] VimStep yanked(const VimState &state, const TextBuffer &text, Offset caret,
                             const VimMotionRange &range)
{
    return VimStep{vim_resting_from(state, register_of(text, range)),
                   VimMoveTo{yanked_caret(text, state, caret, range)}};
}

// いま効かせるオペレータ。保留が無ければ「消す」＝ x が通る道（x は保留を立てずにここへ来る）。
[[nodiscard]] VimOperator operation_of(const VimState &state) noexcept
{
    return state.pending.has_value() ? state.pending.value().operation : VimOperator::remove;
}

[[nodiscard]] VimStep performed(const VimState &state, const TextBuffer &text, Offset caret,
                                const VimMotionRange &range)
{
    switch (operation_of(state))
    {
    case VimOperator::remove:
        return removed(state, text, range);
    case VimOperator::change:
        return changed(state, text, range);
    case VimOperator::yank:
        return yanked(state, text, caret, range);
    }
    std::unreachable();
}

[[nodiscard]] VimState finished_input_wait(const VimState &state)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = std::nullopt;
    next.input_wait = std::nullopt;
    return next;
}

[[nodiscard]] VimState recorded_character_search(const VimState &state, VimCharacterSearch search,
                                                 VimCharacterSearchInvocation invocation)
{
    VimState next = state;
    next.input_wait = std::nullopt;
    if (invocation == VimCharacterSearchInvocation::first)
    {
        next.last_character_search = search;
    }
    return next;
}

[[nodiscard]] VimMotionRange character_search_range(const TextBuffer &text, Offset caret,
                                                    Offset destination, VimCharacterSearchKind kind)
{
    if (searches_forward(kind))
    {
        return characters_between(caret, inclusive_end(text, destination));
    }
    return characters_between(destination, caret);
}

[[nodiscard]] VimStep moved_by_character_search(const VimState &state, const VimEditorView &view,
                                                Offset destination)
{
    VimState next = finished_input_wait(state);
    next.wanted_column =
        VimWantedColumn{VimColumnWish::at_column, view.text.position_of(destination).column};
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return VimStep{std::move(next), VimMoveTo{destination}};
    case VimMode::visual:
    case VimMode::visual_line:
        return VimStep{std::move(next), VimSelect{Selection{view.selection.anchor, destination}}};
    }
    std::unreachable();
}

[[nodiscard]] VimStep completed_character_search(const VimState &state, const VimEditorView &view,
                                                 VimCharacterSearch search,
                                                 VimCharacterSearchInvocation invocation)
{
    const VimState recorded = recorded_character_search(state, search, invocation);
    const auto destination = character_search_position(view, recorded, search, invocation);
    if (!destination.has_value())
    {
        return VimStep{finished_input_wait(recorded), VimNoEffect{}};
    }
    if (recorded.pending.has_value())
    {
        return performed(recorded, view.text, view.selection.caret,
                         character_search_range(view.text, view.selection.caret,
                                                destination.value(), search.kind));
    }
    return moved_by_character_search(recorded, view, destination.value());
}

[[nodiscard]] VimStep started_character_search(const VimState &state, VimCharacterSearchKind kind)
{
    VimState next = state;
    next.input_wait = VimInputWait{kind};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep started_prefix(const VimState &state, VimPrefix prefix)
{
    VimState next = state;
    next.input_wait = VimInputWait{prefix};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep repeated_character_search(const VimState &state, const VimEditorView &view,
                                                VimAction action)
{
    if (!state.last_character_search.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    VimCharacterSearch search = state.last_character_search.value();
    if (action == VimAction::repeat_character_search_opposite)
    {
        search.kind = opposite(search.kind);
    }
    return completed_character_search(state, view, search, VimCharacterSearchInvocation::repeat);
}

// cw の特例（Vim の nv_wordcmd）。語の上（非空白）では ce の範囲になり、空白の上は dw と同じ。
[[nodiscard]] VimMotion motion_for_change(const TextBuffer &text, Offset caret,
                                          VimOperator operation, VimMotion motion)
{
    if (operation != VimOperator::change || motion != VimMotion::next_word ||
        !word_under_caret(text, caret))
    {
        return motion;
    }
    return VimMotion::word_end_for_change;
}

[[nodiscard]] VimStep operated(const VimState &state, const VimEditorView &view, VimMotion motion)
{
    const VimMotion wanted =
        motion_for_change(view.text, view.selection.caret, operation_of(state), motion);
    const auto range = motion_range(view, wanted, resolved_motion_count(state, view.text, wanted));
    if (!range.has_value())
    {
        return cancelled(state);
    }
    VimState performed_from = state;
    const bool document_yank =
        operation_of(state) == VimOperator::yank &&
        (wanted == VimMotion::document_first || wanted == VimMotion::document_last);
    if (document_yank)
    {
        const Offset destination = moved_by(view, state, wanted);
        if (line_of(view.text, destination).value < line_of(view.text, view.selection.caret).value)
        {
            performed_from.wanted_column = VimWantedColumn{
                VimColumnWish::at_column, view.text.position_of(destination).column};
        }
    }
    return performed(performed_from, view.text, view.selection.caret,
                     adjusted_for_exclusive(view.text, range.value(), wanted));
}

// オペレータを自分の回数といっしょに保留する（決定 1）。積んだ回数はここで移る。
[[nodiscard]] VimStep pending_operator(const VimState &state, VimOperator operation)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = VimPendingOperator{operation, state.count};
    next.input_wait = std::nullopt;
    return VimStep{std::move(next), VimNoEffect{}};
}

// dd cc yy と Y。自分の行から count-1 行下まで。保留を立ててから 1 本の performed に流す。
[[nodiscard]] VimStep operated_on_lines(const VimState &state, const TextBuffer &text, Offset caret,
                                        VimOperator operation)
{
    const auto range = lines_below(text, line_of(text, caret), resolved_count(state) - single_step);
    if (!range.has_value())
    {
        return cancelled(state);
    }
    VimState with = state;
    with.pending = VimPendingOperator{operation, std::nullopt};
    return performed(with, text, caret, range.value());
}

// D と C。d$ / c$ と同じ経路（決定 6）。回数は $ が受け取る（2D が 2 行ぶんになる）。
[[nodiscard]] VimStep operated_to_line_end(const VimState &state, const TextBuffer &text,
                                           Offset caret, VimOperator operation)
{
    VimState with = state;
    with.pending = VimPendingOperator{operation, std::nullopt};
    const auto range = to_line_end(text, caret, resolved_count(with));
    if (!range.has_value())
    {
        return cancelled(state);
    }
    return performed(with, text, caret, range.value());
}

// ---------------------------------------------------------------- p / P（決定 4）

[[nodiscard]] std::string repeated(std::string_view body, std::size_t count)
{
    std::string result;
    result.reserve(body.size() * count);
    for (std::size_t step = 0; step < count; ++step)
    {
        result.append(body);
    }
    return result;
}

// 貼った本文の最後の code point の長さ。文字単位の put はその文字の上にキャレットが載る。
[[nodiscard]] std::size_t last_code_point_size(std::string_view body)
{
    return body.size() - previous_code_point(body, Offset{body.size()}).value;
}

// 貼った行の最初の非空白までのバイト数。行単位の put はそこへキャレットが載る。
[[nodiscard]] std::size_t first_non_blank_index(std::string_view body)
{
    std::size_t index = 0;
    while (index < body.size() && (body[index] == ' ' || body[index] == '\t'))
    {
        ++index;
    }
    return index;
}

[[nodiscard]] VimPutString put_characters(const TextBuffer &text, Offset caret, std::string body,
                                          VimPutSide side)
{
    const Offset at =
        side == VimPutSide::after ? forward_characters(text, caret, single_step) : caret;
    const Offset rest{at.value + body.size() - last_code_point_size(body)};
    return VimPutString{at, std::move(body), rest};
}

// 行単位の put。上の行へ入れるときと最終行でない行の下へ入れるときは行の先頭にそのまま入る。
// 最終行の下だけは、そこにもう改行が無いので本文の末尾に改行から書く。
[[nodiscard]] VimPutString put_lines(const TextBuffer &text, Offset caret, std::string body,
                                     VimPutSide side)
{
    const LineNumber line = line_of(text, caret);
    const std::size_t indent = first_non_blank_index(body);
    if (side == VimPutSide::before)
    {
        const Offset at = text.line_start(line);
        return VimPutString{at, std::move(body), Offset{at.value + indent}};
    }
    if (line.value < text.line_count())
    {
        const Offset at = text.line_terminator_end(line);
        return VimPutString{at, std::move(body), Offset{at.value + indent}};
    }
    const Offset at = text.line_end(line);
    std::string tail = "\n";
    tail.append(body, 0, body.size() - single_step);
    return VimPutString{at, std::move(tail), Offset{at.value + single_step + indent}};
}

[[nodiscard]] VimStep put_step(const VimState &state, const TextBuffer &text, Offset caret,
                               VimPutSide side)
{
    if (state.unnamed_register.text.empty())
    {
        return cancelled(state);
    }
    std::string body = repeated(state.unnamed_register.text, count_of(state.count));
    VimState next = vim_resting_from(state, state.unnamed_register);
    switch (state.unnamed_register.kind)
    {
    case VimRegisterKind::uninitialized:
        // An uninitialized register is empty and returned above before put dispatch.
        std::unreachable();
    case VimRegisterKind::characters:
        return VimStep{std::move(next), put_characters(text, caret, std::move(body), side)};
    case VimRegisterKind::lines:
        return VimStep{std::move(next), put_lines(text, caret, std::move(body), side)};
    }
    std::unreachable();
}

// ---------------------------------------------------------------- 鍵から動作へ

[[nodiscard]] VimStep entered_insert(const VimState &state, Offset caret)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = VimMode::insert;
    return VimStep{std::move(next), VimMoveTo{caret}};
}

// x。保留中のオペレータの無い削除なので、範囲を作って同じ 1 本（performed）に流す。
[[nodiscard]] VimStep removed_character(const VimState &state, const TextBuffer &text, Offset caret)
{
    return performed(
        state, text, caret,
        characters_between(caret, forward_characters(text, caret, count_of(state.count))));
}

// 表から引ける移動は 1 本にまとめる（表に無い動作はここへ来ない）。
[[nodiscard]] VimStep moved_step(const VimState &state, const VimEditorView &view, VimAction action)
{
    const auto motion = motion_for(action);
    if (!motion.has_value())
    {
        return cancelled(state);
    }
    return motion_step(state, view, motion.value());
}

[[nodiscard]] std::size_t limited_amount(std::size_t amount, std::size_t total) noexcept
{
    return std::min(amount, total);
}

[[nodiscard]] std::size_t limited_product(std::size_t left, std::size_t right,
                                          std::size_t total) noexcept
{
    if (left == 0 || right == 0)
    {
        return 0;
    }
    return left > total / right ? total : std::min(left * right, total);
}

[[nodiscard]] LineNumber shifted_line(LineNumber line, std::size_t amount, std::size_t total,
                                      VimScrollDirection direction) noexcept
{
    switch (direction)
    {
    case VimScrollDirection::up:
        return LineNumber{line.value > amount ? line.value - amount : 1};
    case VimScrollDirection::down:
    {
        const std::size_t current = std::min(line.value, total);
        return LineNumber{current + std::min(amount, total - current)};
    }
    }
    std::unreachable();
}

[[nodiscard]] LineNumber half_page_top(const VimEditorView &view, std::size_t amount,
                                       VimScrollDirection direction)
{
    switch (direction)
    {
    case VimScrollDirection::up:
        return shifted_line(view.viewport.first_visible, amount, view.text.line_count(), direction);
    case VimScrollDirection::down:
        break;
    }
    const LineNumber last_filled =
        first_visible_within(LineNumber{view.text.line_count()}, view.text.line_count(),
                             view.viewport.visible_lines, ScrollExtent::filled_viewport);
    if (view.viewport.first_visible.value >= last_filled.value)
    {
        return view.viewport.first_visible;
    }
    return shifted_line(view.viewport.first_visible, amount, last_filled.value, direction);
}

[[nodiscard]] LineNumber page_top(const VimEditorView &view, std::size_t pages,
                                  VimScrollDirection direction)
{
    if (direction == VimScrollDirection::down &&
        last_view_line(view).value == view.text.line_count())
    {
        return LineNumber{view.text.line_count()};
    }
    const std::size_t height = std::max<std::size_t>(view.viewport.visible_lines, single_step);
    const std::size_t one_page = height <= 2 ? height : height - 2;
    const std::size_t amount = limited_product(one_page, pages, view.text.line_count());
    return shifted_line(view.viewport.first_visible, amount, view.text.line_count(), direction);
}

[[nodiscard]] LineNumber previous_page_top(const VimEditorView &view, std::size_t pages)
{
    const std::size_t height = std::max<std::size_t>(view.viewport.visible_lines, single_step);
    const std::size_t one_page = height <= 2 ? height : height - 2;
    const std::size_t distance = view.viewport.first_visible.value - single_step;
    const std::size_t steps_to_top = distance / one_page + (distance % one_page != 0 ? 1 : 0);
    const std::size_t effective = std::min(pages, steps_to_top);
    const std::size_t previous_steps = effective > 0 ? effective - single_step : 0;
    const std::size_t amount = limited_product(one_page, previous_steps, view.text.line_count());
    return shifted_line(view.viewport.first_visible, amount, view.text.line_count(),
                        VimScrollDirection::up);
}

[[nodiscard]] Selection navigated_selection(const VimState &state, const Selection &selection,
                                            Offset caret) noexcept
{
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return collapsed_at(caret);
    case VimMode::visual:
    case VimMode::visual_line:
        return Selection{selection.anchor, caret};
    }
    std::unreachable();
}

[[nodiscard]] VimState navigated_state(const VimState &state, const TextBuffer &text, Offset caret)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = std::nullopt;
    next.wanted_column = VimWantedColumn{VimColumnWish::at_column, text.position_of(caret).column};
    return next;
}

[[nodiscard]] VimStep navigated(const VimState &state, const VimEditorView &view, LineNumber first,
                                LineNumber caret_line)
{
    const Offset caret = vim_first_non_blank(view.text, view.text.line_start(caret_line));
    return VimStep{navigated_state(state, view.text, caret),
                   VimNavigate{navigated_selection(state, view.selection, caret), first}};
}

[[nodiscard]] VimStep navigated_at(const VimState &state, const VimEditorView &view,
                                   LineNumber first, Offset caret)
{
    return VimStep{navigated_state(state, view.text, caret),
                   VimNavigate{navigated_selection(state, view.selection, caret), first}};
}

[[nodiscard]] VimStep half_page_step(const VimState &state, const VimEditorView &view,
                                     VimScrollDirection direction)
{
    const std::size_t fallback =
        std::max<std::size_t>(view.viewport.visible_lines / 2, single_step);
    const std::size_t height = std::max<std::size_t>(view.viewport.visible_lines, single_step);
    const std::size_t requested =
        state.count.has_value()
            ? state.count.value().value
            : (state.scroll_lines.has_value() ? state.scroll_lines.value().value : fallback);
    const std::size_t configured = std::min(requested, height);
    const std::size_t amount = limited_amount(configured, view.text.line_count());
    const LineNumber requested_top = half_page_top(view, amount, direction);
    const LineNumber caret_line = shifted_line(line_of(view.text, view.selection.caret), amount,
                                               view.text.line_count(), direction);
    const LineNumber followed = first_visible_for_caret(
        requested_top, caret_line, view.viewport.visible_lines, ScrollFollow::minimal);
    const LineNumber first = first_visible_within(
        followed, view.text.line_count(), view.viewport.visible_lines, ScrollExtent::last_line);
    const LineNumber current_line = line_of(view.text, view.selection.caret);
    VimStep step = caret_line == current_line
                       ? navigated_at(state, view, first, view.selection.caret)
                       : navigated(state, view, first, caret_line);
    if (state.count.has_value() &&
        (first != view.viewport.first_visible || caret_line != current_line))
    {
        step.next.scroll_lines = VimCount{configured};
    }
    return step;
}

[[nodiscard]] VimStep page_step(const VimState &state, const VimEditorView &view,
                                VimScrollDirection direction)
{
    const LineNumber first = page_top(view, count_of(state.count), direction);
    if (first == view.viewport.first_visible)
    {
        const Offset caret = view.selection.caret;
        return VimStep{navigated_state(state, view.text, caret),
                       VimNavigate{view.selection, first}};
    }
    const std::size_t height = std::max<std::size_t>(view.viewport.visible_lines, single_step);
    const std::size_t last =
        first.value + std::min(height - single_step, view.text.line_count() - first.value);
    LineNumber caret_line = first;
    switch (direction)
    {
    case VimScrollDirection::down:
        break;
    case VimScrollDirection::up:
        caret_line = LineNumber{
            std::min(last, previous_page_top(view, count_of(state.count)).value + single_step)};
        break;
    }
    return navigated(state, view, first, caret_line);
}

// v / V で VISUAL に入る。回数があると入った直後にその広さぶんを選ぶ（Vim の nv_visual は
// count1 - 1 だけ v なら右へ・V なら下へ動かす。`3v` は 3 文字・`3V` は 3 行。Issue #53 で実測）。
[[nodiscard]] Offset widened(const TextBuffer &text, Offset caret, std::size_t steps, VimMode mode)
{
    switch (mode)
    {
    case VimMode::visual:
        return forward_characters(text, caret, steps);
    case VimMode::visual_line:
        return caret_on_line(
            text, VimWantedColumn{VimColumnWish::at_column, text.position_of(caret).column},
            line_below(text, line_of(text, caret), steps), mode);
    case VimMode::normal:
    case VimMode::insert:
        return caret;
    }
    std::unreachable();
}

[[nodiscard]] VimStep entered_visual(const VimState &state, const TextBuffer &text, Offset caret,
                                     VimMode mode)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = mode;
    const Offset moved = widened(text, caret, count_of(state.count) - single_step, mode);
    return VimStep{std::move(next), VimSelect{Selection{caret, moved}}};
}

[[nodiscard]] VimStep half_page_action(const VimState &state, const VimEditorView &view,
                                       VimAction action)
{
    const auto direction =
        action == VimAction::scroll_half_down ? VimScrollDirection::down : VimScrollDirection::up;
    return half_page_step(state, view, direction);
}

[[nodiscard]] VimStep page_action(const VimState &state, const VimEditorView &view,
                                  VimAction action)
{
    const auto direction =
        action == VimAction::scroll_page_down ? VimScrollDirection::down : VimScrollDirection::up;
    return page_step(state, view, direction);
}

[[nodiscard]] VimStep visual_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    const VimMode mode = action == VimAction::visual ? VimMode::visual : VimMode::visual_line;
    return entered_visual(state, view.text, view.selection.caret, mode);
}

[[nodiscard]] VimStep put_action(const VimState &state, const VimEditorView &view, VimAction action)
{
    const VimPutSide side = action == VimAction::put_after ? VimPutSide::after : VimPutSide::before;
    return put_step(state, view.text, view.selection.caret, side);
}

[[nodiscard]] VimStep line_end_action(const VimState &state, const VimEditorView &view,
                                      VimAction action)
{
    const VimOperator operation =
        action == VimAction::remove_to_line_end ? VimOperator::remove : VimOperator::change;
    return operated_to_line_end(state, view.text, view.selection.caret, operation);
}

[[nodiscard]] VimStep opened_command_line(const VimState &state)
{
    if (state.count.has_value())
    {
        return cancelled(state);
    }
    return VimStep{vim_resting_from(state, state.unnamed_register), VimOpenCommandLine{}};
}

[[nodiscard]] VimStep appended_insert(const VimState &state, const VimEditorView &view)
{
    return entered_insert(state, forward_characters(view.text, view.selection.caret, single_step));
}

[[nodiscard]] std::optional<VimStep>
character_search_action(const VimState &state, const VimEditorView &view, VimAction action)
{
    if (action == VimAction::find_character_forward)
    {
        return started_character_search(state, VimCharacterSearchKind::find_forward);
    }
    if (action == VimAction::find_character_backward)
    {
        return started_character_search(state, VimCharacterSearchKind::find_backward);
    }
    if (action == VimAction::till_character_forward)
    {
        return started_character_search(state, VimCharacterSearchKind::till_forward);
    }
    if (action == VimAction::till_character_backward)
    {
        return started_character_search(state, VimCharacterSearchKind::till_backward);
    }
    if (action == VimAction::repeat_character_search ||
        action == VimAction::repeat_character_search_opposite)
    {
        return repeated_character_search(state, view, action);
    }
    return std::nullopt;
}

[[nodiscard]] VimStep required_character_search_action(const VimState &state,
                                                       const VimEditorView &view, VimAction action)
{
    auto step = character_search_action(state, view, action);
    if (!step.has_value())
    {
        std::unreachable();
    }
    return std::move(step).value();
}

[[nodiscard]] VimStep pending_action(const VimState &state, VimAction action)
{
    const VimOperator operation = action == VimAction::remove_operator   ? VimOperator::remove
                                  : action == VimAction::change_operator ? VimOperator::change
                                                                         : VimOperator::yank;
    return pending_operator(state, operation);
}

[[nodiscard]] VimStep normal_visual_action(const VimState &state, const VimEditorView &view,
                                           VimAction action)
{
    return action == VimAction::swap_visual_ends ? cancelled(state)
                                                 : visual_action(state, view, action);
}

[[nodiscard]] VimStep insert_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    if (action == VimAction::insert_before)
    {
        return entered_insert(state, view.selection.caret);
    }
    if (action == VimAction::insert_after)
    {
        return appended_insert(state, view);
    }
    if (action == VimAction::insert_at_line_start)
    {
        return entered_insert(state, vim_first_non_blank(view.text, view.selection.caret));
    }
    return entered_insert(state, view.text.line_end(line_of(view.text, view.selection.caret)));
}

[[nodiscard]] VimStep history_action(const VimState &state, VimAction action)
{
    if (action == VimAction::undo)
    {
        return VimStep{vim_resting_from(state, state.unnamed_register), VimUndo{}};
    }
    return VimStep{vim_resting_from(state, state.unnamed_register), VimRedo{}};
}

[[nodiscard]] VimStep normal_edit_action(const VimState &state, const VimEditorView &view,
                                         VimAction action)
{
    if (action == VimAction::remove_character)
    {
        return removed_character(state, view.text, view.selection.caret);
    }
    if (action == VimAction::remove_operator || action == VimAction::change_operator ||
        action == VimAction::yank_operator)
    {
        return pending_action(state, action);
    }
    if (action == VimAction::put_after || action == VimAction::put_before)
    {
        return put_action(state, view, action);
    }
    if (action == VimAction::remove_to_line_end || action == VimAction::change_to_line_end)
    {
        return line_end_action(state, view, action);
    }
    if (action == VimAction::yank_line)
    {
        return operated_on_lines(state, view.text, view.selection.caret, VimOperator::yank);
    }
    std::unreachable();
}

[[nodiscard]] VimStep commanded(const VimState &state, const VimEditorView &view, VimAction action)
{
    switch (action)
    {
    case VimAction::move_left:
    case VimAction::move_down:
    case VimAction::move_up:
    case VimAction::move_right:
    case VimAction::move_line_start:
    case VimAction::move_line_end:
    case VimAction::move_next_word:
    case VimAction::move_previous_word:
    case VimAction::move_word_end:
    case VimAction::move_first_non_blank:
    case VimAction::move_screen_top:
    case VimAction::move_screen_middle:
    case VimAction::move_screen_bottom:
    case VimAction::move_document_first:
    case VimAction::move_document_last:
        return moved_step(state, view, action);
    case VimAction::scroll_half_down:
    case VimAction::scroll_half_up:
        return half_page_action(state, view, action);
    case VimAction::scroll_page_down:
    case VimAction::scroll_page_up:
        return page_action(state, view, action);
    case VimAction::visual:
    case VimAction::visual_line:
    case VimAction::swap_visual_ends:
        return normal_visual_action(state, view, action);
    case VimAction::find_character_forward:
    case VimAction::find_character_backward:
    case VimAction::till_character_forward:
    case VimAction::till_character_backward:
    case VimAction::repeat_character_search:
    case VimAction::repeat_character_search_opposite:
        return required_character_search_action(state, view, action);
    case VimAction::open_command_line:
        return opened_command_line(state);
    case VimAction::prefix_g:
        return started_prefix(state, VimPrefix::g);
    case VimAction::remove_character:
    case VimAction::remove_operator:
    case VimAction::change_operator:
    case VimAction::yank_operator:
    case VimAction::put_after:
    case VimAction::put_before:
    case VimAction::remove_to_line_end:
    case VimAction::change_to_line_end:
    case VimAction::yank_line:
        return normal_edit_action(state, view, action);
    case VimAction::insert_before:
    case VimAction::insert_after:
    case VimAction::insert_at_line_start:
    case VimAction::insert_at_line_end:
        return insert_action(state, view, action);
    case VimAction::undo:
    case VimAction::redo:
        return history_action(state, action);
    }
    std::unreachable();
}

// 同じオペレータの鍵をもう一度押したときの動作（dd cc yy）。ほかの鍵は打ち消しになる。
[[nodiscard]] VimAction doubled_action_of(VimOperator operation) noexcept
{
    switch (operation)
    {
    case VimOperator::remove:
        return VimAction::remove_operator;
    case VimOperator::change:
        return VimAction::change_operator;
    case VimOperator::yank:
        return VimAction::yank_operator;
    }
    std::unreachable();
}

// 保留中のオペレータの後ろ。同じ鍵なら行単位、移動なら範囲、ほかの鍵なら打ち消し（Vim と同じ）。
[[nodiscard]] VimStep pending_step(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    const VimOperator operation = operation_of(state);
    if (action == VimAction::prefix_g)
    {
        return started_prefix(state, VimPrefix::g);
    }
    if (action == doubled_action_of(operation))
    {
        return operated_on_lines(state, view.text, view.selection.caret, operation);
    }
    const auto search_step = character_search_action(state, view, action);
    if (search_step.has_value())
    {
        return search_step.value();
    }
    const auto motion = motion_for(action);
    if (!motion.has_value())
    {
        return cancelled(state);
    }
    return operated(state, view, motion.value());
}

[[nodiscard]] VimStep acted(const VimState &state, const VimEditorView &view, VimAction action)
{
    if (state.pending.has_value())
    {
        return pending_step(state, view, action);
    }
    return commanded(state, view, action);
}

[[nodiscard]] bool counts_as_digit(const VimState &state, char32_t key) noexcept
{
    if (key < U'0' || key > U'9')
    {
        return false;
    }
    return key != U'0' || state.count.has_value();
}

[[nodiscard]] VimStep counted(const VimState &state, char32_t key)
{
    const auto digit = static_cast<std::size_t>(key - U'0');
    const std::size_t previous = state.count.has_value() ? state.count.value().value : 0;
    const std::size_t largest = std::numeric_limits<std::size_t>::max();
    const std::size_t carried =
        previous > (largest - digit) / decimal_base ? largest - digit : previous * decimal_base;
    VimState next = state;
    next.count = VimCount{carried + digit};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep normal_character(const VimState &state, const VimEditorView &view,
                                       VimCharacter key)
{
    if (counts_as_digit(state, key.code))
    {
        return counted(state, key.code);
    }
    const auto action = action_for(key.code);
    if (!action.has_value())
    {
        return cancelled(state);
    }
    return acted(state, view, action.value());
}

// NORMAL の特別な鍵。矢印は h j k l と同じ動作で、Home / End は 0 と $（ADR 0015 の決定 6）。
// Esc は保留を捨てるだけ（ADR 0012 の決定 5）。
[[nodiscard]] VimStep normal_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
        return cancelled(state);
    case VimSpecialKey::arrow_left:
        return acted(state, view, VimAction::move_left);
    case VimSpecialKey::arrow_right:
        return acted(state, view, VimAction::move_right);
    case VimSpecialKey::arrow_up:
        return acted(state, view, VimAction::move_up);
    case VimSpecialKey::arrow_down:
        return acted(state, view, VimAction::move_down);
    case VimSpecialKey::home:
        return acted(state, view, VimAction::move_line_start);
    case VimSpecialKey::end:
        return acted(state, view, VimAction::move_line_end);
    case VimSpecialKey::control_r:
        return acted(state, view, VimAction::redo);
    case VimSpecialKey::page_up:
    case VimSpecialKey::control_b:
        return acted(state, view, VimAction::scroll_page_up);
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_f:
        return acted(state, view, VimAction::scroll_page_down);
    case VimSpecialKey::control_d:
        return acted(state, view, VimAction::scroll_half_down);
    case VimSpecialKey::control_u:
        return acted(state, view, VimAction::scroll_half_up);
    }
    std::unreachable();
}

// ---------------------------------------------------------------- VISUAL（ADR 0018）

// 鍵を 1 つ食べ終わったあとの VISUAL。回数とオペレータは捨て、モードと無名レジスタだけ残る。
[[nodiscard]] VimState visual_resting(const VimState &state, VimMode mode)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = mode;
    return next;
}

// VISUAL で効かない鍵。選択もキャレットも本文も動かない（決定 7）。
[[nodiscard]] VimStep visual_unchanged(const VimState &state)
{
    return VimStep{visual_resting(state, state.mode), VimNoEffect{}};
}

// VISUAL から出る（Esc・同じ鍵）。選択は畳み、キャレットはその場に残す（決定 3）。
[[nodiscard]] VimStep left_visual(const VimState &state, const Selection &selection)
{
    return VimStep{vim_resting_from(state, state.unnamed_register), VimMoveTo{selection.caret}};
}

// 同じ鍵なら出る、違う鍵なら選択を保ったまま種類を切り替える。
[[nodiscard]] VimStep visual_switched(const VimState &state, const Selection &selection,
                                      VimMode mode)
{
    if (state.mode == mode)
    {
        return left_visual(state, selection);
    }
    return VimStep{visual_resting(state, mode), VimSelect{selection}};
}

// VISUAL の移動。anchor はそのままで caret だけ動く（決定 3）。欲しい列は NORMAL と同じ。
[[nodiscard]] VimStep visual_moved(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    const auto motion = motion_for(action);
    if (!motion.has_value())
    {
        return visual_unchanged(state);
    }
    const VimWantedColumn wanted = wanted_column_of(view.text, state, view.selection.caret);
    const Offset moved = moved_by(view, state, motion.value());
    VimState next = visual_resting(state, state.mode);
    next.wanted_column = wanted_after(view.text, wanted, moved, motion.value());
    return VimStep{std::move(next), VimSelect{Selection{view.selection.anchor, moved}}};
}

// `d x y c`。選択を範囲に変えて #43 と同じ経路へ流す（決定 5）。オペレータは VISUAL を終わらせる。
[[nodiscard]] VimStep visual_operated(const VimState &state, const VimEditorView &view,
                                      VimOperator operation)
{
    const VimMotionRange range = vim_visual_range(view.text, view.selection, state.mode);
    switch (operation)
    {
    case VimOperator::remove:
        return removed_exactly(state, view.text, range);
    case VimOperator::change:
        return changed(state, view.text, range);
    case VimOperator::yank:
        return yanked(state, view.text, view.selection.caret, range);
    }
    std::unreachable();
}

[[nodiscard]] VimStep visual_scroll_action(const VimState &state, const VimEditorView &view,
                                           VimAction action)
{
    if (action == VimAction::scroll_half_down || action == VimAction::scroll_half_up)
    {
        return half_page_action(state, view, action);
    }
    return page_action(state, view, action);
}

[[nodiscard]] VimStep visual_selection_action(const VimState &state, const VimEditorView &view,
                                              VimAction action)
{
    if (action == VimAction::remove_character || action == VimAction::remove_operator)
    {
        return visual_operated(state, view, VimOperator::remove);
    }
    if (action == VimAction::change_operator)
    {
        return visual_operated(state, view, VimOperator::change);
    }
    if (action == VimAction::yank_operator)
    {
        return visual_operated(state, view, VimOperator::yank);
    }
    if (action == VimAction::swap_visual_ends)
    {
        return VimStep{visual_resting(state, state.mode),
                       VimSelect{Selection{view.selection.caret, view.selection.anchor}}};
    }
    if (action == VimAction::visual)
    {
        return visual_switched(state, view.selection, VimMode::visual);
    }
    if (action == VimAction::visual_line)
    {
        return visual_switched(state, view.selection, VimMode::visual_line);
    }
    std::unreachable();
}

[[nodiscard]] VimStep visual_acted(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    switch (action)
    {
    case VimAction::move_left:
    case VimAction::move_down:
    case VimAction::move_up:
    case VimAction::move_right:
    case VimAction::move_line_start:
    case VimAction::move_line_end:
    case VimAction::move_next_word:
    case VimAction::move_previous_word:
    case VimAction::move_word_end:
    case VimAction::move_first_non_blank:
    case VimAction::move_screen_top:
    case VimAction::move_screen_middle:
    case VimAction::move_screen_bottom:
    case VimAction::move_document_first:
    case VimAction::move_document_last:
        return visual_moved(state, view, action);
    case VimAction::scroll_half_down:
    case VimAction::scroll_half_up:
    case VimAction::scroll_page_down:
    case VimAction::scroll_page_up:
        return visual_scroll_action(state, view, action);
    // VISUAL の x は d と同じ（決定 7）。
    case VimAction::remove_character:
    case VimAction::remove_operator:
    case VimAction::change_operator:
    case VimAction::yank_operator:
    case VimAction::swap_visual_ends:
    case VimAction::visual:
    case VimAction::visual_line:
        return visual_selection_action(state, view, action);
    case VimAction::find_character_forward:
    case VimAction::find_character_backward:
    case VimAction::till_character_forward:
    case VimAction::till_character_backward:
    case VimAction::repeat_character_search:
    case VimAction::repeat_character_search_opposite:
        return required_character_search_action(state, view, action);
    case VimAction::prefix_g:
        return started_prefix(state, VimPrefix::g);
    // この縦切りの範囲の外の鍵は何もしない（決定 7・決定 8）。
    case VimAction::open_command_line:
    case VimAction::put_after:
    case VimAction::put_before:
    case VimAction::remove_to_line_end:
    case VimAction::change_to_line_end:
    case VimAction::yank_line:
    case VimAction::insert_before:
    case VimAction::insert_after:
    case VimAction::insert_at_line_start:
    case VimAction::insert_at_line_end:
    case VimAction::undo:
    case VimAction::redo:
        return visual_unchanged(state);
    }
    std::unreachable();
}

[[nodiscard]] VimStep visual_character(const VimState &state, const VimEditorView &view,
                                       VimCharacter key)
{
    if (counts_as_digit(state, key.code))
    {
        return counted(state, key.code);
    }
    const auto action = action_for(key.code);
    if (!action.has_value())
    {
        return visual_unchanged(state);
    }
    return visual_acted(state, view, action.value());
}

// VISUAL の特別な鍵。Esc で出て、矢印と Home / End は NORMAL と同じ動作を引く。
// Enter / Backspace / Ctrl-r はこの縦切りに無い（決定 7）。
[[nodiscard]] VimStep visual_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::escape:
        return left_visual(state, view.selection);
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
    case VimSpecialKey::control_r:
        return visual_unchanged(state);
    case VimSpecialKey::arrow_left:
        return visual_acted(state, view, VimAction::move_left);
    case VimSpecialKey::arrow_right:
        return visual_acted(state, view, VimAction::move_right);
    case VimSpecialKey::arrow_up:
        return visual_acted(state, view, VimAction::move_up);
    case VimSpecialKey::arrow_down:
        return visual_acted(state, view, VimAction::move_down);
    case VimSpecialKey::home:
        return visual_acted(state, view, VimAction::move_line_start);
    case VimSpecialKey::end:
        return visual_acted(state, view, VimAction::move_line_end);
    case VimSpecialKey::page_up:
    case VimSpecialKey::control_b:
        return visual_acted(state, view, VimAction::scroll_page_up);
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_f:
        return visual_acted(state, view, VimAction::scroll_page_down);
    case VimSpecialKey::control_d:
        return visual_acted(state, view, VimAction::scroll_half_down);
    case VimSpecialKey::control_u:
        return visual_acted(state, view, VimAction::scroll_half_up);
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_moved(const VimState &state, const TextBuffer &text, Offset caret,
                                   CaretMotion motion)
{
    return VimStep{state, VimMoveTo{moved_caret(text, caret, motion, single_step)}};
}

// INSERT の Backspace。挿入を始めた位置より前も消せる（oracle の backspace=indent,eol,start）。
[[nodiscard]] VimStep insert_erased(const VimState &state, const TextBuffer &text, Offset caret)
{
    if (caret.value == 0)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const Offset begin = caret.value > start.value ? backward_characters(text, caret, single_step)
                                                   : text.line_end(LineNumber{line.value - 1});
    return VimStep{state, VimRemoveRange{OffsetRange{begin, caret}}};
}

[[nodiscard]] VimStep insert_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    switch (key)
    {
    case VimSpecialKey::escape:
        return VimStep{vim_resting_from(state, state.unnamed_register),
                       VimMoveTo{backward_characters(text, caret, single_step)}};
    case VimSpecialKey::enter:
        return VimStep{state, VimNewLine{}};
    case VimSpecialKey::backspace:
        return insert_erased(state, text, caret);
    case VimSpecialKey::arrow_left:
        return insert_moved(state, text, caret, CaretMotion::previous_character);
    case VimSpecialKey::arrow_right:
        return insert_moved(state, text, caret, CaretMotion::next_character);
    case VimSpecialKey::arrow_up:
        return insert_moved(state, text, caret, CaretMotion::previous_line);
    case VimSpecialKey::arrow_down:
        return insert_moved(state, text, caret, CaretMotion::next_line);
    case VimSpecialKey::home:
        return insert_moved(state, text, caret, CaretMotion::line_start);
    case VimSpecialKey::end:
        return insert_moved(state, text, caret, CaretMotion::line_end);
    case VimSpecialKey::control_r:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
        return VimStep{state, VimNoEffect{}};
    case VimSpecialKey::page_up:
        return page_step(state, view, VimScrollDirection::up);
    case VimSpecialKey::page_down:
        return page_step(state, view, VimScrollDirection::down);
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_character(const VimState &state, VimCharacter key)
{
    if (key.code == line_feed)
    {
        return VimStep{state, VimNewLine{}};
    }
    std::string utf8;
    append_utf8(utf8, key.code);
    return VimStep{state, VimInsertString{std::move(utf8)}};
}

[[nodiscard]] std::optional<char32_t> character_search_target(VimSpecialKey key) noexcept
{
    switch (key)
    {
    case VimSpecialKey::enter:
        return carriage_return_character;
    case VimSpecialKey::control_r:
        return control_r_character;
    case VimSpecialKey::control_d:
        return control_d_character;
    case VimSpecialKey::control_u:
        return control_u_character;
    case VimSpecialKey::control_f:
        return control_f_character;
    case VimSpecialKey::control_b:
        return control_b_character;
    case VimSpecialKey::escape:
    case VimSpecialKey::backspace:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] VimStep awaited_character(const VimState &state, const VimEditorView &view,
                                        VimCharacterSearchKind kind, char32_t target)
{
    const VimCharacterSearch search{kind, target};
    return completed_character_search(state, view, search, VimCharacterSearchInvocation::first);
}

[[nodiscard]] VimStep awaited_step(const VimState &state, const VimEditorView &view,
                                   VimCharacterSearchKind kind, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return awaited_character(state, view, kind, std::get<VimCharacter>(key).code);
    }
    const VimSpecialKey special = std::get<VimSpecialKey>(key);
    const auto target = character_search_target(special);
    if (!target.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    return awaited_character(state, view, kind, target.value());
}

[[nodiscard]] VimStep completed_prefix(const VimState &state, const VimEditorView &view,
                                       VimAction action)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return acted(state, view, action);
    case VimMode::insert:
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    case VimMode::visual:
    case VimMode::visual_line:
        return visual_acted(state, view, action);
    }
    std::unreachable();
}

[[nodiscard]] VimStep awaited_step(const VimState &state, const VimEditorView &view,
                                   VimPrefix prefix, VimKey key)
{
    if (!std::holds_alternative<VimCharacter>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const char32_t character = std::get<VimCharacter>(key).code;
    switch (prefix)
    {
    case VimPrefix::g:
        return character == U'g' ? completed_prefix(state, view, VimAction::move_document_first)
                                 : VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    std::unreachable();
}

[[nodiscard]] VimStep awaiting_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (!state.input_wait.has_value())
    {
        std::unreachable();
    }
    return std::visit([&](const auto wait) { return awaited_step(state, view, wait, key); },
                      state.input_wait.value());
}

[[nodiscard]] VimStep normal_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return normal_character(state, view, std::get<VimCharacter>(key));
    }
    return normal_special(state, view, std::get<VimSpecialKey>(key));
}

[[nodiscard]] VimStep insert_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return insert_character(state, std::get<VimCharacter>(key));
    }
    return insert_special(state, view, std::get<VimSpecialKey>(key));
}

[[nodiscard]] VimStep visual_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return visual_character(state, view, std::get<VimCharacter>(key));
    }
    return visual_special(state, view, std::get<VimSpecialKey>(key));
}
} // namespace

VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (state.input_wait.has_value())
    {
        return awaiting_step(state, view, key);
    }
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_step(state, view, key);
    case VimMode::insert:
        return insert_step(state, view, key);
    case VimMode::visual:
    case VimMode::visual_line:
        return visual_step(state, view, key);
    }
    std::unreachable();
}
} // namespace nenenib::core
