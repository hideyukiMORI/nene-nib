#include "VimStep.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "Column.hpp"
#include "LineNumber.hpp"
#include "OffsetRange.hpp"
#include "ScrollBounds.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimActionBinding.hpp"
#include "VimActionGroup.hpp"
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
#include "VimPattern.hpp"
#include "VimPatternFailure.hpp"
#include "VimPrefix.hpp"
#include "VimPutSide.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRepeatFailure.hpp"
#include "VimRepeatRecord.hpp"
#include "VimReplay.hpp"
#include "VimScreenPosition.hpp"
#include "VimScrollDirection.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchHit.hpp"
#include "VimSearchNotice.hpp"
#include "VimSearchNoticeKind.hpp"
#include "VimSearchPattern.hpp"
#include "VimSearchRequest.hpp"
#include "VimSelect.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectBinding.hpp"
#include "VimTextObjectRange.hpp"
#include "VimTextObjectRequest.hpp"
#include "VimTextObjectScope.hpp"
#include "VimVisualRange.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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
constexpr std::array<VimBinding, 49> normal_bindings{
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
     {U'o', VimAction::open_line_below},
     {U'O', VimAction::open_line_above},
     {U'f', VimAction::find_character_forward},
     {U'F', VimAction::find_character_backward},
     {U't', VimAction::till_character_forward},
     {U'T', VimAction::till_character_backward},
     {U';', VimAction::repeat_character_search},
     {U',', VimAction::repeat_character_search_opposite},
     {U'g', VimAction::prefix_g},
     {U'r', VimAction::replace_character},
     {U'.', VimAction::repeat_change},
     {U':', VimAction::open_command_line},
     {U'/', VimAction::open_search_forward},
     {U'?', VimAction::open_search_backward},
     {U'n', VimAction::repeat_search},
     {U'N', VimAction::repeat_search_opposite},
     {U'*', VimAction::search_word_forward},
     {U'#', VimAction::search_word_backward}}};

// 動作 → 大分類の表（CPP-012 / ADR 0006）。NORMAL と VISUAL の写し先はこの分類で分かれる。
// 行の無い動作は何もしない（表と動作の一覧が離れたら、その動作の鍵が効かなくなる）。
constexpr std::array<VimActionBinding, 54> action_groups{
    {{VimAction::move_left, VimActionGroup::motion},
     {VimAction::move_down, VimActionGroup::motion},
     {VimAction::move_up, VimActionGroup::motion},
     {VimAction::move_right, VimActionGroup::motion},
     {VimAction::move_line_start, VimActionGroup::motion},
     {VimAction::move_line_end, VimActionGroup::motion},
     {VimAction::move_next_word, VimActionGroup::motion},
     {VimAction::move_previous_word, VimActionGroup::motion},
     {VimAction::move_word_end, VimActionGroup::motion},
     {VimAction::move_first_non_blank, VimActionGroup::motion},
     {VimAction::move_screen_top, VimActionGroup::motion},
     {VimAction::move_screen_middle, VimActionGroup::motion},
     {VimAction::move_screen_bottom, VimActionGroup::motion},
     {VimAction::move_document_first, VimActionGroup::motion},
     {VimAction::move_document_last, VimActionGroup::motion},
     {VimAction::scroll_half_down, VimActionGroup::scroll},
     {VimAction::scroll_half_up, VimActionGroup::scroll},
     {VimAction::scroll_page_down, VimActionGroup::scroll},
     {VimAction::scroll_page_up, VimActionGroup::scroll},
     {VimAction::visual, VimActionGroup::enter_visual},
     {VimAction::visual_line, VimActionGroup::enter_visual},
     {VimAction::open_line_below, VimActionGroup::enter_visual},
     {VimAction::open_line_above, VimActionGroup::enter_visual},
     {VimAction::remove_character, VimActionGroup::edit_range},
     {VimAction::remove_operator, VimActionGroup::edit_range},
     {VimAction::change_operator, VimActionGroup::edit_range},
     {VimAction::yank_operator, VimActionGroup::edit_range},
     {VimAction::put_after, VimActionGroup::edit_line},
     {VimAction::put_before, VimActionGroup::edit_line},
     {VimAction::remove_to_line_end, VimActionGroup::edit_line},
     {VimAction::change_to_line_end, VimActionGroup::edit_line},
     {VimAction::yank_line, VimActionGroup::edit_line},
     {VimAction::insert_before, VimActionGroup::insert_object},
     {VimAction::insert_after, VimActionGroup::insert_object},
     {VimAction::insert_at_line_start, VimActionGroup::insert_line},
     {VimAction::insert_at_line_end, VimActionGroup::insert_line},
     {VimAction::undo, VimActionGroup::history},
     {VimAction::redo, VimActionGroup::history},
     {VimAction::repeat_change, VimActionGroup::history},
     {VimAction::find_character_forward, VimActionGroup::input_wait},
     {VimAction::find_character_backward, VimActionGroup::input_wait},
     {VimAction::till_character_forward, VimActionGroup::input_wait},
     {VimAction::till_character_backward, VimActionGroup::input_wait},
     {VimAction::repeat_character_search, VimActionGroup::input_wait},
     {VimAction::repeat_character_search_opposite, VimActionGroup::input_wait},
     {VimAction::prefix_g, VimActionGroup::input_wait},
     {VimAction::replace_character, VimActionGroup::input_wait},
     {VimAction::open_command_line, VimActionGroup::ex_line},
     {VimAction::open_search_forward, VimActionGroup::search},
     {VimAction::open_search_backward, VimActionGroup::search},
     {VimAction::repeat_search, VimActionGroup::search},
     {VimAction::repeat_search_opposite, VimActionGroup::search},
     {VimAction::search_word_forward, VimActionGroup::search},
     {VimAction::search_word_backward, VimActionGroup::search}}};

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

// i / a の後ろの鍵 → テキストオブジェクトの表（ADR 0031 の決定 1 / CPP-012）。b は paren、
// B は brace、閉じ括弧の鍵は開き括弧と同じ行（Vim 9.1 で実測）。ここに無い鍵は取消。
constexpr std::array<VimTextObjectBinding, 15> text_object_bindings{
    {{U'w', VimTextObject::word},
     {U'W', VimTextObject::big_word},
     {U'\"', VimTextObject::double_quote},
     {U'\'', VimTextObject::single_quote},
     {U'`', VimTextObject::backtick},
     {U'(', VimTextObject::paren},
     {U')', VimTextObject::paren},
     {U'b', VimTextObject::paren},
     {U'{', VimTextObject::brace},
     {U'}', VimTextObject::brace},
     {U'B', VimTextObject::brace},
     {U'[', VimTextObject::bracket},
     {U']', VimTextObject::bracket},
     {U'<', VimTextObject::angle},
     {U'>', VimTextObject::angle}}};

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

[[nodiscard]] std::optional<VimActionGroup> group_for(VimAction action) noexcept
{
    for (const VimActionBinding binding : action_groups)
    {
        if (binding.action == action)
        {
            return binding.group;
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

[[nodiscard]] std::optional<VimTextObject> text_object_for(char32_t key) noexcept
{
    for (const VimTextObjectBinding binding : text_object_bindings)
    {
        if (binding.key == key)
        {
            return binding.object;
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
        return lines_between(text, first, previous);
    }
    return characters_between(range.range.begin, text.line_end(previous));
}

[[nodiscard]] VimMotionRange adjusted_for_exclusive(const TextBuffer &text,
                                                    const VimMotionRange &range, VimMotion motion)
{
    return exclusive(motion) ? exclusive_range(text, range) : range;
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

// y のあとのキャレット。行単位VISUALは下向き・単一行なら先頭、上向きなら現在位置。
// NORMALの行単位は同じ桁を保つ。VISUAL終了時の行末の寄せはcontrollerの共通経路を使う。
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
        if (state.mode == VimMode::visual_line)
        {
            return line_of(text, caret) == line_of(text, range.range.end) ? range.range.begin
                                                                          : caret;
        }
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

// オペレータ保留中と VISUAL の i / a（ADR 0031 の決定 1）。次の鍵まで待つだけで本文は動かない。
[[nodiscard]] VimStep started_text_object(const VimState &state, VimTextObjectScope scope)
{
    VimState next = state;
    next.input_wait = VimInputWait{scope};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep text_object_action(const VimState &state, VimAction action)
{
    return started_text_object(state, action == VimAction::insert_before
                                          ? VimTextObjectScope::inner
                                          : VimTextObjectScope::around);
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

[[nodiscard]] std::expected<std::string, VimRepeatFailure> repeated(std::string_view body,
                                                                    std::size_t count)
{
    std::string result;
    if (body.empty())
    {
        return result;
    }
    if (count > result.max_size() / body.size())
    {
        return std::unexpected(VimRepeatFailure::too_large);
    }
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

[[nodiscard]] VimInsertAt put_characters(const TextBuffer &text, Offset caret, std::string body,
                                         VimPutSide side)
{
    const Offset at =
        side == VimPutSide::after ? forward_characters(text, caret, single_step) : caret;
    const Offset rest{at.value + body.size() - last_code_point_size(body)};
    return VimInsertAt{at, std::move(body), rest, EditBoundary::separate};
}

// 行単位の put。上の行へ入れるときと最終行でない行の下へ入れるときは行の先頭にそのまま入る。
// 最終行の下だけは、そこにもう改行が無いので本文の末尾に改行から書く。
[[nodiscard]] VimInsertAt put_lines(const TextBuffer &text, Offset caret, std::string body,
                                    VimPutSide side)
{
    const LineNumber line = line_of(text, caret);
    const std::size_t indent = first_non_blank_index(body);
    if (side == VimPutSide::before)
    {
        const Offset at = text.line_start(line);
        return VimInsertAt{at, std::move(body), Offset{at.value + indent}, EditBoundary::separate};
    }
    if (line.value < text.line_count())
    {
        const Offset at = text.line_terminator_end(line);
        return VimInsertAt{at, std::move(body), Offset{at.value + indent}, EditBoundary::separate};
    }
    const Offset at = text.line_end(line);
    std::string tail = "\n";
    tail.append(body, 0, body.size() - single_step);
    return VimInsertAt{at, std::move(tail), Offset{at.value + single_step + indent},
                       EditBoundary::separate};
}

[[nodiscard]] VimStep put_step(const VimState &state, const TextBuffer &text, Offset caret,
                               VimPutSide side)
{
    if (state.unnamed_register.text.empty())
    {
        return cancelled(state);
    }
    auto body = repeated(state.unnamed_register.text, count_of(state.count));
    if (!body.has_value())
    {
        return cancelled(state);
    }
    VimState next = vim_resting_from(state, state.unnamed_register);
    switch (state.unnamed_register.kind)
    {
    case VimRegisterKind::uninitialized:
        // An uninitialized register is empty and returned above before put dispatch.
        std::unreachable();
    case VimRegisterKind::characters:
        return VimStep{std::move(next), put_characters(text, caret, std::move(body).value(), side)};
    case VimRegisterKind::lines:
        return VimStep{std::move(next), put_lines(text, caret, std::move(body).value(), side)};
    }
    std::unreachable();
}

// ---------------------------------------------------------------- r（ADR 0029）

[[nodiscard]] std::optional<OffsetRange> normal_replacement_range(const TextBuffer &text,
                                                                  Offset caret, std::size_t count)
{
    const Offset end = text.line_end(line_of(text, caret));
    const std::string available = text.text_range(caret, end);
    if (count > code_point_count(available))
    {
        return std::nullopt;
    }
    return OffsetRange{caret, forward_characters(text, caret, count)};
}

[[nodiscard]] std::optional<OffsetRange> replacement_range(const VimState &state,
                                                           const VimEditorView &view)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_replacement_range(view.text, view.selection.caret, count_of(state.count));
    case VimMode::visual:
    case VimMode::visual_line:
        return vim_visual_range(view.text, view.selection, state.mode).range;
    case VimMode::insert:
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] VimStep started_replacement(const VimState &state, const VimEditorView &view)
{
    if (!replacement_range(state, view).has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    return started_prefix(state, VimPrefix::r);
}

// 文字だけを置換する。CRLF/LFの行境界は残し、効果の本文はLFへ揃える。
[[nodiscard]] std::string replacement_text(std::string_view source, char32_t target)
{
    std::string result;
    result.reserve(source.size());
    Offset at{0};
    while (at.value < source.size())
    {
        const char32_t code = code_point_at(source, at);
        at = next_code_point(source, at);
        if (code == carriage_return_character && at.value < source.size() &&
            source.at(at.value) == '\n')
        {
            continue;
        }
        append_utf8(result, code == line_feed ? line_feed : target);
    }
    return result;
}

[[nodiscard]] VimStep replaced_character(const VimState &state, const VimEditorView &view,
                                         char32_t target)
{
    const bool newline = target == line_feed || target == carriage_return_character;
    const bool unsupported = (newline && state.mode != VimMode::normal) ||
                             (!newline && target < U' ' && target != U'\t') || target == U'\x7f';
    const auto range = replacement_range(state, view);
    if (unsupported || !range.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const OffsetRange selected = range.value();
    std::string body =
        newline ? std::string("\n")
                : replacement_text(view.text.text_range(selected.begin, selected.end), target);
    Offset caret = selected.begin;
    if (state.mode == VimMode::normal)
    {
        caret.value += newline ? body.size() : previous_code_point(body, Offset{body.size()}).value;
    }
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimReplaceRange{selected, std::move(body), caret}};
}

[[nodiscard]] VimStep replacement_key(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return replaced_character(state, view, std::get<VimCharacter>(key).code);
    }
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    switch (std::get<VimSpecialKey>(key))
    {
    case VimSpecialKey::enter:
        return replaced_character(state, view, line_feed);
    case VimSpecialKey::escape:
    case VimSpecialKey::backspace:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::control_r:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    std::unreachable();
}

// ---------------------------------------------------------------- 鍵から動作へ

// i a I A。回数は o / O と同じで、Esc のときに入力を残りの回数だけ繰り返す（ADR 0028 の
// 決定 2 の機構をそのまま使う。`3ifoo<Esc>` が foofoofoo になるのは Vim も同じ）。
[[nodiscard]] VimStep entered_insert(const VimState &state, Offset caret)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = VimMode::insert;
    const std::size_t count = count_of(state.count);
    if (count > single_step)
    {
        next.insert_repeat = VimInsertRepeat{count - single_step, std::string{}};
    }
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
[[nodiscard]] Offset widened(const TextBuffer &text, Offset caret, const VimState &state,
                             VimMode mode)
{
    const std::size_t steps = count_of(state.count) - single_step;
    switch (mode)
    {
    case VimMode::visual:
        return forward_characters(text, caret, steps);
    case VimMode::visual_line:
    {
        const LineNumber line = line_of(text, caret);
        const LineNumber destination = line_below(text, line, steps);
        return destination == line
                   ? caret
                   : caret_on_line(text, wanted_column_of(text, state, caret), destination, mode);
    }
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
    const Offset moved = widened(text, caret, state, mode);
    next.wanted_column = state.wanted_column;
    if (mode == VimMode::visual && moved != caret)
    {
        next.wanted_column =
            wanted_after(text, wanted_column_of(text, state, caret), moved, VimMotion::right);
    }
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

// 半画面と 1 画面の巻き（ADR 0019）。NORMAL と VISUAL は同じ 1 本を通る。
[[nodiscard]] VimStep scroll_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    if (action == VimAction::scroll_half_down || action == VimAction::scroll_half_up)
    {
        return half_page_action(state, view, action);
    }
    return page_action(state, view, action);
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
    if (action == VimAction::visual || action == VimAction::visual_line)
    {
        return visual_action(state, view, action);
    }
    const LineNumber line = line_of(view.text, view.selection.caret);
    const bool below = action == VimAction::open_line_below;
    const Offset at = below ? view.text.line_end(line) : view.text.line_start(line);
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = VimMode::insert;
    const std::size_t count = count_of(state.count);
    if (count > single_step)
    {
        next.insert_repeat = VimInsertRepeat{count - single_step, "\n"};
    }
    return VimStep{
        std::move(next),
        VimInsertAt{at, "\n", Offset{at.value + (below ? single_step : 0)}, EditBoundary::absorb}};
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

// ---------------------------------------------------------------- `.`（ADR 0030）

// 記録の回数を 10 進の桁に戻す。表が数字を回数として読み直すので、再生に特別な経路は要らない。
[[nodiscard]] std::vector<VimKey> count_keys(VimCount count)
{
    std::vector<VimKey> reversed;
    for (std::size_t rest = count.value; rest > 0; rest /= decimal_base)
    {
        const auto digit = static_cast<char32_t>(U'0' + rest % decimal_base);
        reversed.emplace_back(VimCharacter{digit});
    }
    return std::vector<VimKey>{reversed.rbegin(), reversed.rend()};
}

// 再生する鍵の列（決定 6）。回数の桁を先頭に展開するだけで、鍵の意味は engine の表が決める。
[[nodiscard]] std::vector<VimKey> replayed_keys(const std::optional<VimCount> &count,
                                                const std::vector<VimKey> &keys)
{
    std::vector<VimKey> result =
        count.has_value() ? count_keys(count.value()) : std::vector<VimKey>{};
    result.insert(result.end(), keys.begin(), keys.end());
    return result;
}

// `.`。直前の変更が無ければ何も起きない。回数は `.` に付いた回数が優先で、無ければ記録の回数。
[[nodiscard]] VimStep repeated_change(const VimState &state)
{
    if (!state.last_change.has_value())
    {
        return cancelled(state);
    }
    const VimRepeatRecord &record = state.last_change.value();
    const std::optional<VimCount> count = state.count.has_value() ? state.count : record.count;
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimReplay{replayed_keys(count, record.keys)}};
}

// u / Ctrl-r / `.`。どれも済んだ編集をもう一度たどる（ADR 0030 の決定 6）。
[[nodiscard]] VimStep history_action(const VimState &state, VimAction action)
{
    if (action == VimAction::undo)
    {
        return VimStep{vim_resting_from(state, state.unnamed_register), VimUndo{}};
    }
    if (action == VimAction::redo)
    {
        return VimStep{vim_resting_from(state, state.unnamed_register), VimRedo{}};
    }
    return repeated_change(state);
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

[[nodiscard]] VimStep input_action(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    // VISUAL の i / a はテキストオブジェクトの接頭辞（ADR 0031 の決定 1）。NORMAL の挿入命令は
    // commanded が先に insert_action へ分けるので、ここへは VISUAL からしか来ない。
    if (action == VimAction::insert_before || action == VimAction::insert_after)
    {
        return text_object_action(state, action);
    }
    if (action == VimAction::open_command_line)
    {
        return opened_command_line(state);
    }
    if (action == VimAction::prefix_g)
    {
        return started_prefix(state, VimPrefix::g);
    }
    if (action == VimAction::replace_character)
    {
        return started_replacement(state, view);
    }
    return required_character_search_action(state, view, action);
}

// ---------------------------------------------------------------- 検索（ADR 0032）

// 検索を食べ終わったあとの状態。VISUAL は VISUAL のまま残る（見つからなくても・実測）。
[[nodiscard]] VimState search_rested(const VimState &state)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return next;
    case VimMode::visual:
    case VimMode::visual_line:
        next.mode = state.mode;
        return next;
    }
    std::unreachable();
}

[[nodiscard]] VimSearchNotice notice_of(VimSearchNoticeKind kind)
{
    return VimSearchNotice{kind, std::string{}, std::nullopt};
}

// 報せを残して命令を取り消す（見つからない・直前が無い・語が無い・未対応の構文）。
[[nodiscard]] VimStep search_noticed(const VimState &state, VimSearchNotice notice)
{
    return VimStep{search_rested(state), VimNoEffect{}, std::move(notice)};
}

// 折り返したときだけ出る報せ（wrapscan は既定で有効）。
[[nodiscard]] std::optional<VimSearchNotice> wrap_notice(const VimSearchHit &hit,
                                                         VimSearchDirection direction)
{
    if (!hit.wrapped)
    {
        return std::nullopt;
    }
    return notice_of(direction == VimSearchDirection::forward
                         ? VimSearchNoticeKind::wrapped_to_top
                         : VimSearchNoticeKind::wrapped_to_bottom);
}

// 回数ぶん続けて探す。1 回でも見つからなければ命令ごと取り消しになる（実測）。
[[nodiscard]] std::optional<VimSearchHit> search_hit(const VimEditorView &view,
                                                     const VimPattern &pattern,
                                                     const VimSearchRequest &request,
                                                     std::size_t count)
{
    Offset at = request.origin;
    bool wrapped = false;
    for (std::size_t step = 0; step < count; ++step)
    {
        const auto hit = vim_search(view.text, at, pattern, request.direction);
        if (!hit.has_value())
        {
            return std::nullopt;
        }
        at = hit.value().caret;
        wrapped = wrapped || hit.value().wrapped;
    }
    return VimSearchHit{at, wrapped};
}

// 移動としての検索。NORMAL はキャレット、VISUAL は端点を動かす（決定 3）。
[[nodiscard]] VimStep search_moved(const VimState &state, const VimEditorView &view,
                                   Offset destination)
{
    VimState next = search_rested(state);
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

// オペレータの後ろの検索は exclusive で、既存の 1 本の規則を通る（決定 3）。
// 範囲が空（回数が自分自身へ折り返した）ときは本文もレジスタも変えない（実測）。
[[nodiscard]] VimStep search_operated(const VimState &state, const VimEditorView &view,
                                      const VimSearchRequest &request, Offset destination)
{
    if (destination == request.anchor)
    {
        return VimStep{search_rested(state), VimNoEffect{}};
    }
    return performed(state, view.text, request.anchor,
                     exclusive_range(view.text, characters_between(request.anchor, destination)));
}

// パターンは last_search が正本。解析して回数ぶん探し、着いた先へ 1 本で流す（決定 3・4）。
[[nodiscard]] VimStep search_from(const VimState &state, const VimEditorView &view,
                                  const VimSearchRequest &request)
{
    if (!state.last_search.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    const VimSearchPattern remembered = state.last_search.value();
    const auto parsed = VimPattern::parse(remembered.pattern, remembered.direction);
    if (!parsed)
    {
        return search_noticed(state, VimSearchNotice{VimSearchNoticeKind::unsupported_pattern,
                                                     std::string{}, parsed.error()});
    }
    const auto hit = search_hit(view, parsed.value(), request, resolved_count(state));
    if (!hit.has_value())
    {
        return search_noticed(state, VimSearchNotice{VimSearchNoticeKind::pattern_not_found,
                                                     remembered.pattern, std::nullopt});
    }
    VimStep step = state.pending.has_value()
                       ? search_operated(state, view, request, hit.value().caret)
                       : search_moved(state, view, hit.value().caret);
    step.notice = wrap_notice(hit.value(), request.direction);
    return step;
}

// 入力行の Enter（決定 3）。空のパターンは直前を使い直し、無ければ E35。
// 見つからない検索も last_search を更新する（次の `n` が同じ失敗を繰り返す・実測）。
[[nodiscard]] VimStep searched_key(const VimState &state, const VimEditorView &view,
                                   const VimSearchPattern &key)
{
    const std::string previous =
        state.last_search.has_value() ? state.last_search.value().pattern : std::string{};
    const std::string pattern = key.pattern.empty() ? previous : key.pattern;
    if (pattern.empty())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    VimState remembered = state;
    remembered.last_search = VimSearchPattern{pattern, key.direction};
    const Offset caret = view.selection.caret;
    return search_from(remembered, view, VimSearchRequest{caret, caret, key.direction});
}

// `n` / `N`（決定 3）。覚えた向きのまま、または反対の向きで探す。last_search は変えない。
[[nodiscard]] VimStep repeated_search(const VimState &state, const VimEditorView &view,
                                      VimAction action)
{
    if (!state.last_search.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    const VimSearchDirection remembered = state.last_search.value().direction;
    const VimSearchDirection direction =
        action == VimAction::repeat_search ? remembered : opposite(remembered);
    const Offset caret = view.selection.caret;
    return search_from(state, view, VimSearchRequest{caret, caret, direction});
}

// `*` / `#`（決定 3）。語を \<…\> のパターンにして同じ経路へ。語が無ければ E348。
[[nodiscard]] VimStep word_search(const VimState &state, const VimEditorView &view,
                                  VimAction action)
{
    const auto word = vim_word_at(view.text, view.selection.caret);
    if (!word.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_word_under_cursor));
    }
    const VimSearchDirection direction = action == VimAction::search_word_forward
                                             ? VimSearchDirection::forward
                                             : VimSearchDirection::backward;
    VimState remembered = state;
    remembered.last_search = VimSearchPattern{
        "\\<" + view.text.text_range(word.value().begin, word.value().end) + "\\>", direction};
    // 探し始めるのは語の先頭だが、範囲の端は元のキャレットである（実測）。
    return search_from(remembered, view,
                       VimSearchRequest{word.value().begin, view.selection.caret, direction});
}

// 検索の入力行を開く（決定 2）。VimState は変えないので保留・回数・記録はそのまま残る。
[[nodiscard]] VimStep opened_search(const VimState &state, VimSearchDirection direction)
{
    return VimStep{state, VimOpenSearch{direction}};
}

[[nodiscard]] VimStep search_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    if (action == VimAction::open_search_forward)
    {
        return opened_search(state, VimSearchDirection::forward);
    }
    if (action == VimAction::open_search_backward)
    {
        return opened_search(state, VimSearchDirection::backward);
    }
    if (action == VimAction::repeat_search || action == VimAction::repeat_search_opposite)
    {
        return repeated_search(state, view, action);
    }
    return word_search(state, view, action);
}

// 鍵から引いた動作を、分類ごとの写し先へ（CPP-012 / ADR 0006）。分類が増えたらここで落ちる。
[[nodiscard]] VimStep commanded(const VimState &state, const VimEditorView &view, VimAction action)
{
    const auto group = group_for(action);
    if (!group.has_value())
    {
        return cancelled(state);
    }
    switch (group.value())
    {
    case VimActionGroup::motion:
        return moved_step(state, view, action);
    case VimActionGroup::scroll:
        return scroll_action(state, view, action);
    case VimActionGroup::enter_visual:
        return normal_visual_action(state, view, action);
    case VimActionGroup::edit_range:
    case VimActionGroup::edit_line:
        return normal_edit_action(state, view, action);
    case VimActionGroup::insert_object:
    case VimActionGroup::insert_line:
        return insert_action(state, view, action);
    case VimActionGroup::history:
        return history_action(state, action);
    case VimActionGroup::input_wait:
    case VimActionGroup::ex_line:
        return input_action(state, view, action);
    case VimActionGroup::search:
        return search_action(state, view, action);
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
    if (action == VimAction::insert_before || action == VimAction::insert_after)
    {
        return text_object_action(state, action);
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
    // 検索の 6 つの鍵は保留中のオペレータの範囲を作る（ADR 0032 の決定 3）。
    if (group_for(action) == VimActionGroup::search)
    {
        return search_action(state, view, action);
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
    VimStep step = state.mode == mode ? left_visual(state, selection)
                                      : VimStep{visual_resting(state, mode), VimSelect{selection}};
    step.next.wanted_column = state.wanted_column;
    return step;
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
    if (action == VimAction::open_line_below || action == VimAction::open_line_above)
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

// VISUAL の写し先（決定 7・決定 8）。行に効く命令・挿入・履歴・Ex は VISUAL では効かない。
[[nodiscard]] VimStep visual_acted(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    const auto group = group_for(action);
    if (!group.has_value())
    {
        return visual_unchanged(state);
    }
    switch (group.value())
    {
    case VimActionGroup::motion:
        return visual_moved(state, view, action);
    case VimActionGroup::scroll:
        return scroll_action(state, view, action);
    case VimActionGroup::enter_visual:
    case VimActionGroup::edit_range:
        return visual_selection_action(state, view, action);
    case VimActionGroup::insert_object:
    case VimActionGroup::input_wait:
        return input_action(state, view, action);
    case VimActionGroup::search:
        return search_action(state, view, action);
    case VimActionGroup::edit_line:
    case VimActionGroup::insert_line:
    case VimActionGroup::history:
    case VimActionGroup::ex_line:
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
    VimState next = state;
    next.insert_repeat = std::nullopt;
    return VimStep{std::move(next), VimMoveTo{moved_caret(text, caret, motion, single_step)}};
}

[[nodiscard]] VimState insert_recorded(const VimState &state, std::string_view input)
{
    VimState next = state;
    if (next.insert_repeat.has_value())
    {
        next.insert_repeat.value().text.append(input);
    }
    return next;
}

[[nodiscard]] VimState insert_record_erased(const VimState &state)
{
    VimState next = state;
    if (!next.insert_repeat.has_value())
    {
        return next;
    }
    std::string &input = next.insert_repeat.value().text;
    if (input.empty())
    {
        next.insert_repeat = std::nullopt;
        return next;
    }
    input.resize(previous_code_point(input, Offset{input.size()}).value);
    return next;
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
    return VimStep{insert_record_erased(state), VimRemoveRange{OffsetRange{begin, caret}}};
}

[[nodiscard]] VimStep left_insert(const VimState &state, const VimEditorView &view)
{
    const Offset caret = view.selection.caret;
    VimStep step{vim_resting_from(state, state.unnamed_register),
                 VimMoveTo{backward_characters(view.text, caret, single_step)}};
    if (!state.insert_repeat.has_value() || state.insert_repeat.value().text.empty())
    {
        return step;
    }
    const VimInsertRepeat &repeat = state.insert_repeat.value();
    auto result = repeated(repeat.text, repeat.remaining);
    if (!result.has_value())
    {
        return step;
    }
    std::string body = std::move(result).value();
    const std::size_t retreat = body.back() == '\n' ? 0 : last_code_point_size(body);
    const Offset rest{caret.value + body.size() - retreat};
    step.effect = VimInsertAt{caret, std::move(body), rest, EditBoundary::absorb};
    return step;
}

[[nodiscard]] VimStep insert_page_moved(const VimState &state, const VimEditorView &view,
                                        VimScrollDirection direction)
{
    VimState next = state;
    next.insert_repeat = std::nullopt;
    return page_step(next, view, direction);
}

[[nodiscard]] VimStep insert_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    switch (key)
    {
    case VimSpecialKey::escape:
        return left_insert(state, view);
    case VimSpecialKey::enter:
        return VimStep{insert_recorded(state, "\n"), VimNewLine{}};
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
        return insert_page_moved(state, view, VimScrollDirection::up);
    case VimSpecialKey::page_down:
        return insert_page_moved(state, view, VimScrollDirection::down);
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_character(const VimState &state, VimCharacter key)
{
    if (key.code == line_feed)
    {
        return VimStep{insert_recorded(state, "\n"), VimNewLine{}};
    }
    std::string utf8;
    append_utf8(utf8, key.code);
    return VimStep{insert_recorded(state, utf8), VimInsertString{std::move(utf8)}};
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
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const VimSpecialKey special = std::get<VimSpecialKey>(key);
    const auto target = character_search_target(special);
    if (!target.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    return awaited_character(state, view, kind, target.value());
}

// ---------------------------------------------------------------- テキストオブジェクト（ADR 0031）

// オペレータの後ろ。exclusive 補正は通さず、d だけが Vi 互換の行単位の規則を通る（決定 3）。
// y のキャレットは範囲の先頭で、行単位の範囲でもその桁へ戻る（Vim 9.1 で実測）。
[[nodiscard]] VimStep operated_on_object(const VimState &state, const VimEditorView &view,
                                         const VimMotionRange &range)
{
    VimState from = state;
    from.input_wait = std::nullopt;
    from.wanted_column = std::nullopt;
    return performed(from, view.text, range.range.begin, range);
}

// VISUAL。選択を範囲に置き換える（決定 4）。行単位になる範囲でも VISUAL は文字単位のままで、
// caret を最後の行の内容の終わりに置くと選択が改行まで届く（実測）。
[[nodiscard]] VimStep selected_object(const VimState &state, const VimEditorView &view,
                                      const VimMotionRange &range)
{
    const Offset last = vim_text_object_caret(view.text, range);
    const bool backward = view.selection.caret < view.selection.anchor;
    const Selection selected =
        backward ? Selection{last, range.range.begin} : Selection{range.range.begin, last};
    VimState next = visual_resting(state, VimMode::visual);
    next.wanted_column =
        VimWantedColumn{VimColumnWish::at_column, view.text.position_of(selected.caret).column};
    return VimStep{std::move(next), VimSelect{selected}};
}

[[nodiscard]] VimStep completed_text_object(const VimState &state, const VimEditorView &view,
                                            const VimMotionRange &range)
{
    if (state.pending.has_value())
    {
        return operated_on_object(state, view, range);
    }
    return selected_object(state, view, range);
}

[[nodiscard]] VimStep awaited_step(const VimState &state, const VimEditorView &view,
                                   VimTextObjectScope scope, VimKey key)
{
    if (!std::holds_alternative<VimCharacter>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const auto object = text_object_for(std::get<VimCharacter>(key).code);
    if (!object.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const auto range =
        vim_text_object_range(view.text, view.selection,
                              VimTextObjectRequest{scope, object.value()}, resolved_count(state));
    if (!range.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    return completed_text_object(state, view, range.value());
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
    switch (prefix)
    {
    case VimPrefix::g:
        if (std::holds_alternative<VimCharacter>(key) && std::get<VimCharacter>(key).code == U'g')
        {
            return completed_prefix(state, view, VimAction::move_document_first);
        }
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    case VimPrefix::r:
        return replacement_key(state, view, key);
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

// 鍵の種類とモードの組（ADR 0012 の決定 4 / ADR 0032 の決定 3）。鍵の種類が増えたら
// std::visit の写し先が足りずコンパイルが落ちる（CPP-002）。
[[nodiscard]] VimStep pressed(const VimState &state, const VimEditorView &view, VimCharacter key)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_character(state, view, key);
    case VimMode::insert:
        return insert_character(state, key);
    case VimMode::visual:
    case VimMode::visual_line:
        return visual_character(state, view, key);
    }
    std::unreachable();
}

[[nodiscard]] VimStep pressed(const VimState &state, const VimEditorView &view, VimSpecialKey key)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_special(state, view, key);
    case VimMode::insert:
        return insert_special(state, view, key);
    case VimMode::visual:
    case VimMode::visual_line:
        return visual_special(state, view, key);
    }
    std::unreachable();
}

// 確定した検索は入力行からだけ来る。INSERT には届かない（斜線は文字・決定 2）。
[[nodiscard]] VimStep pressed(const VimState &state, const VimEditorView &view,
                              const VimSearchPattern &key)
{
    switch (state.mode)
    {
    case VimMode::insert:
        return VimStep{state, VimNoEffect{}};
    case VimMode::normal:
    case VimMode::visual:
    case VimMode::visual_line:
        return searched_key(state, view, key);
    }
    std::unreachable();
}

[[nodiscard]] VimStep stepped(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (state.input_wait.has_value())
    {
        return awaiting_step(state, view, key);
    }
    return std::visit([&](const auto &value) { return pressed(state, view, value); }, key);
}

// ---------------------------------------------------------------- 記録（ADR 0030 の決定 2〜5）

// 本文を変える効果か（決定 3）。写し先が足りなければ std::visit がここで落ちる（CPP-002）。
[[nodiscard]] bool changes_text(const VimNoEffect &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimMoveTo &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimNavigate &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimSelect &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimUndo &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimRedo &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimOpenCommandLine &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimOpenSearch &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimReplay &) noexcept
{
    return false;
}
[[nodiscard]] bool changes_text(const VimRemoveRange &) noexcept
{
    return true;
}
[[nodiscard]] bool changes_text(const VimRemoveLines &) noexcept
{
    return true;
}
[[nodiscard]] bool changes_text(const VimInsertString &) noexcept
{
    return true;
}
[[nodiscard]] bool changes_text(const VimNewLine &) noexcept
{
    return true;
}
[[nodiscard]] bool changes_text(const VimInsertAt &) noexcept
{
    return true;
}
[[nodiscard]] bool changes_text(const VimReplaceRange &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimEffect &effect) noexcept
{
    return std::visit([](const auto &value) { return changes_text(value); }, effect);
}

// INSERT で記録を `i` から取り直す鍵（決定 4）。ADR 0028 の決定 3 が挿入の反復を切るのと
// 同じ境界で、Vim も移動のあとの入力を新しい挿入として扱う（Home / End / 矢印で実測）。
[[nodiscard]] bool restarts_insert(VimKey key) noexcept
{
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return false;
    }
    switch (std::get<VimSpecialKey>(key))
    {
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
        return true;
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
    case VimSpecialKey::control_r:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] VimRepeatRecord appended(VimRepeatRecord record, VimKey key)
{
    record.keys.push_back(key);
    return record;
}

// 命令が完了したか（決定 3）。次キー待ち・保留オペレータ・回数が空で NORMAL のまま。
[[nodiscard]] bool completed(const VimState &next) noexcept
{
    return next.mode == VimMode::normal && !next.input_wait.has_value() &&
           !next.pending.has_value() && !next.count.has_value();
}

// NORMAL の鍵を記録へ（決定 2）。回数の桁は記録せず、回数はそのときの積を 1 つだけ残す。
// 検索の入力行を開く鍵も記録しない。記録に残るのは確定した VimSearchPattern の 1 鍵だけで、
// 再生はその鍵を同じ経路へ流すだけになる（ADR 0032 の決定 3）。
[[nodiscard]] VimRepeatRecord normal_recording(const VimState &before, const VimEffect &effect,
                                               VimKey key)
{
    VimRepeatRecord record = before.recording.value_or(VimRepeatRecord{});
    if (std::holds_alternative<VimOpenSearch>(effect))
    {
        return record;
    }
    if (std::holds_alternative<VimCharacter>(key) &&
        counts_as_digit(before, std::get<VimCharacter>(key).code))
    {
        return record;
    }
    record.count = combined_count(before);
    return appended(std::move(record), key);
}

// INSERT の記録（決定 4）。Esc で 1 つの変更として確定し、移動の鍵は `i` から取り直す。
// VISUAL から入った挿入は記録を持たないので、確定もしない（決定 5）。
[[nodiscard]] VimState insert_recording(const VimState &before, VimState next, VimKey key)
{
    if (!before.recording.has_value())
    {
        return next;
    }
    if (next.mode == VimMode::normal)
    {
        next.last_change = appended(before.recording.value(), key);
        return next;
    }
    if (restarts_insert(key))
    {
        next.recording = VimRepeatRecord{std::nullopt, {VimKey{VimCharacter{U'i'}}}};
        return next;
    }
    next.recording = appended(before.recording.value(), key);
    return next;
}

// NORMAL の記録。完了した鍵だけが効果で分かれ、途中の鍵と INSERT へ入る命令は記録を続ける。
[[nodiscard]] VimState normal_recorded(const VimState &before, VimState next,
                                       const VimEffect &effect, VimKey key)
{
    VimRepeatRecord record = normal_recording(before, effect, key);
    if (next.mode != VimMode::normal)
    {
        // VISUAL への遷移は記録を捨てる（決定 5）。INSERT へ入る命令は Esc まで続ける。
        if (next.mode == VimMode::insert)
        {
            next.recording = std::move(record);
        }
        return next;
    }
    if (!completed(next))
    {
        next.recording = std::move(record);
        return next;
    }
    if (changes_text(effect))
    {
        next.last_change = std::move(record);
    }
    return next;
}

// 記録の更新はこの 1 か所だけ（ARC-001）。鍵の意味ではなく、前のモードと効果で分ける。
[[nodiscard]] VimState vim_recorded(const VimState &before, VimState next, const VimEffect &effect,
                                    VimKey key)
{
    next.recording = std::nullopt;
    next.last_change = before.last_change;
    // `.` 自身は記録しない。再生する鍵に `.` が入らないので再帰は深さ 1 で止まる（決定 7）。
    if (std::holds_alternative<VimReplay>(effect))
    {
        return next;
    }
    switch (before.mode)
    {
    case VimMode::visual:
    case VimMode::visual_line:
        // VISUAL で完了した変更は直前の変更を消す（決定 5）。yank と移動は残す。
        if (changes_text(effect))
        {
            next.last_change = std::nullopt;
        }
        return next;
    case VimMode::insert:
        return insert_recording(before, std::move(next), key);
    case VimMode::normal:
        return normal_recorded(before, std::move(next), effect, key);
    }
    std::unreachable();
}
} // namespace

VimState vim_cancelled_input(const VimState &state)
{
    VimState next = search_rested(state);
    next.recording = std::nullopt;
    next.last_change = state.last_change;
    return next;
}

VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    VimStep step = stepped(state, view, key);
    VimState recorded = vim_recorded(state, std::move(step.next), step.effect, key);
    step.next = std::move(recorded);
    return step;
}
} // namespace nenenib::core
