#include "VimKeyTable.hpp"

#include "VimAction.hpp"
#include "VimActionBinding.hpp"
#include "VimActionGroup.hpp"
#include "VimBinding.hpp"
#include "VimMotion.hpp"
#include "VimMotionBinding.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectBinding.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace nenenib::core
{
namespace
{
// VimStep.cpp と同じ値（表の中の鍵をリテラルで書くための写し。経路の複製ではない）。
constexpr char32_t line_feed = U'\n';
// Ctrl-r は文字としては制御文字なので、表に載せる値は名前で書く（原文に制御文字を置かない）。
constexpr char32_t control_r_character = 0x12;

// NORMAL の鍵 → 動作の表（ADR 0012 の決定 5 / ADR 0015 の決定 6 / CPP-012）。分岐で書くと
// 関数長で落ちる（T8）。数字は表に無い。回数として積むほうが先で、'0' だけは回数が空のときに
// 行頭として引かれる。
constexpr std::array<VimBinding, 56> normal_bindings{
    {{U'h', VimAction::move_left},
     {U'j', VimAction::move_down},
     {line_feed, VimAction::move_down},
     {U'k', VimAction::move_up},
     {U'l', VimAction::move_right},
     {U'0', VimAction::move_line_start},
     {U'$', VimAction::move_line_end},
     {U'w', VimAction::move_next_word},
     {U'b', VimAction::move_previous_word},
     {U'e', VimAction::move_word_end},
     {U'^', VimAction::move_first_non_blank},
     {U'+', VimAction::move_next_line},
     {U'-', VimAction::move_previous_line},
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
     {U'#', VimAction::search_word_backward},
     {U'q', VimAction::record_macro},
     {U'@', VimAction::replay_macro},
     {U'"', VimAction::select_register},
     {U' ', VimAction::space_right}}};

// 動作 → 大分類の表（CPP-012 / ADR 0006）。NORMAL と VISUAL の写し先はこの分類で分かれる。
// 行の欠落と重複は下の static_assert で落ちる（動作を足したら、この表に行を足すまで通らない）。
constexpr std::array<VimActionBinding, vim_action_count> action_groups{
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
     {VimAction::move_next_line, VimActionGroup::motion},
     {VimAction::move_previous_line, VimActionGroup::motion},
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
     {VimAction::search_word_backward, VimActionGroup::search},
     {VimAction::record_macro, VimActionGroup::input_wait},
     {VimAction::replay_macro, VimActionGroup::input_wait},
     {VimAction::select_register, VimActionGroup::input_wait},
     {VimAction::space_right, VimActionGroup::motion},
     {VimAction::space_left, VimActionGroup::motion}}};

// オペレータの後ろで範囲になる動作。ここに無い鍵（x i a …）は保留中のオペレータを打ち消す。
constexpr std::array<VimMotionBinding, 19> motion_bindings{
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
     {VimAction::move_document_last, VimMotion::document_last},
     {VimAction::move_next_line, VimMotion::next_line},
     {VimAction::move_previous_line, VimMotion::previous_line},
     {VimAction::space_right, VimMotion::wrap_right},
     {VimAction::space_left, VimMotion::wrap_left}}};

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
constexpr std::array<VimMotion, 8> exclusive_motions{
    {VimMotion::left, VimMotion::right, VimMotion::line_start, VimMotion::first_non_blank,
     VimMotion::next_word, VimMotion::previous_word, VimMotion::wrap_right, VimMotion::wrap_left}};

// 表の中でその動作を指す行の数。ちょうど 1 でなければ表が動作の一覧とずれている。
[[nodiscard]] constexpr std::size_t rows_for(VimAction action) noexcept
{
    std::size_t rows = 0;
    for (const VimActionBinding binding : action_groups)
    {
        if (binding.action == action)
        {
            ++rows;
        }
    }
    return rows;
}

[[nodiscard]] constexpr bool every_action_has_one_row() noexcept
{
    for (std::size_t value = 0; value < vim_action_count; ++value)
    {
        if (rows_for(static_cast<VimAction>(value)) != 1)
        {
            return false;
        }
    }
    return true;
}

static_assert(action_groups.size() == vim_action_count,
              "動作 → 大分類の表は動作と同じ数の行を持つ（CPP-012）");
static_assert(every_action_has_one_row(),
              "どの動作も表にちょうど 1 行。欠落と重複はここでコンパイルが落ちる（CPP-002）");
} // namespace

[[nodiscard]] std::optional<VimAction> vim_action_for(char32_t key) noexcept
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

[[nodiscard]] std::optional<VimActionGroup> vim_group_for(VimAction action) noexcept
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

[[nodiscard]] std::optional<VimMotion> vim_motion_for(VimAction action) noexcept
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

[[nodiscard]] std::optional<VimTextObject> vim_text_object_for(char32_t key) noexcept
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

[[nodiscard]] bool vim_motion_is_exclusive(VimMotion motion) noexcept
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
} // namespace nenenib::core
