#include "VimKeyTable.hpp"

#include "VimAction.hpp"
#include "VimActionBinding.hpp"
#include "VimActionGroup.hpp"
#include "VimBinding.hpp"
#include "VimMotion.hpp"
#include "VimMotionBinding.hpp"
#include "VimOperator.hpp"
#include "VimOperatorBinding.hpp"
#include "VimScreenAlignmentBinding.hpp"
#include "VimScreenPlace.hpp"
#include "VimSpecialKey.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectBinding.hpp"
#include "VimWindowBinding.hpp"
#include "VimWindowCommand.hpp"
#include "VimWordClass.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::core
{
namespace
{
// VimStep.cpp と同じ値（表の中の鍵をリテラルで書くための写し。経路の複製ではない）。
constexpr char32_t line_feed = U'\n';
// Ctrl-r は文字としては制御文字なので、表に載せる値は名前で書く（原文に制御文字を置かない）。
constexpr char32_t control_r_character = 0x12;
// z<CR> の <CR>（z の後ろの特別な鍵は VimStep.cpp が文字に写してから表を引く）。
constexpr char32_t carriage_return_character = 0x0D;
// `Ctrl-w` の後ろの Ctrl-v と Ctrl-w（特別な鍵の口 vim_window_command_for(VimSpecialKey) と
// vim_window_control_character もこの値を引く・VimStep.cpp に写しを置かない）。
constexpr char32_t control_v_character = 0x16;
constexpr char32_t control_w_character = 0x17;

// NORMAL の鍵 → 動作の表（ADR 0012 の決定 5 / ADR 0015 の決定 6 / CPP-012）。分岐で書くと
// 関数長で落ちる（T8）。数字は表に無い。回数として積むほうが先で、'0' だけは回数が空のときに
// 行頭として引かれる。
constexpr std::array<VimBinding, 72> normal_bindings{
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
     {U'W', VimAction::move_next_big_word},
     {U'B', VimAction::move_previous_big_word},
     {U'E', VimAction::move_big_word_end},
     {U'^', VimAction::move_first_non_blank},
     {U'|', VimAction::move_to_column},
     {U'+', VimAction::move_next_line},
     {U'-', VimAction::move_previous_line},
     {U'_', VimAction::move_first_non_blank_below},
     {U'H', VimAction::move_screen_top},
     {U'M', VimAction::move_screen_middle},
     {U'L', VimAction::move_screen_bottom},
     {U'G', VimAction::move_document_last},
     {U'x', VimAction::remove_character},
     {U'X', VimAction::remove_character_before},
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
     {U'z', VimAction::prefix_z},
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
     {U' ', VimAction::space_right},
     {U'J', VimAction::join_lines},
     {U'~', VimAction::toggle_case_character},
     {U'U', VimAction::undo_line},
     {U'>', VimAction::shift_right_operator},
     {U'<', VimAction::shift_left_operator},
     {U'%', VimAction::move_matching_pair},
     {U'm', VimAction::set_mark},
     {U'\'', VimAction::prefix_mark_line},
     {U'`', VimAction::prefix_mark_exact}}};

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
     {VimAction::remove_character_before, VimActionGroup::edit_line},
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
     {VimAction::space_left, VimActionGroup::motion},
     {VimAction::move_next_big_word, VimActionGroup::motion},
     {VimAction::move_previous_big_word, VimActionGroup::motion},
     {VimAction::move_big_word_end, VimActionGroup::motion},
     {VimAction::move_previous_word_end, VimActionGroup::motion},
     {VimAction::move_previous_big_word_end, VimActionGroup::motion},
     {VimAction::next_tab, VimActionGroup::tab},
     {VimAction::previous_tab, VimActionGroup::tab},
     {VimAction::move_display_down, VimActionGroup::motion},
     {VimAction::move_display_up, VimActionGroup::motion},
     {VimAction::prefix_z, VimActionGroup::input_wait},
     {VimAction::scroll_column_right, VimActionGroup::horizontal_scroll},
     {VimAction::scroll_column_left, VimActionGroup::horizontal_scroll},
     {VimAction::scroll_half_width_right, VimActionGroup::horizontal_scroll},
     {VimAction::scroll_half_width_left, VimActionGroup::horizontal_scroll},
     {VimAction::scroll_caret_to_left, VimActionGroup::horizontal_scroll},
     {VimAction::scroll_caret_to_right, VimActionGroup::horizontal_scroll},
     {VimAction::align_top, VimActionGroup::screen_alignment},
     {VimAction::align_center, VimActionGroup::screen_alignment},
     {VimAction::align_bottom, VimActionGroup::screen_alignment},
     {VimAction::align_top_first_non_blank, VimActionGroup::screen_alignment},
     {VimAction::align_center_first_non_blank, VimActionGroup::screen_alignment},
     {VimAction::align_bottom_first_non_blank, VimActionGroup::screen_alignment},
     {VimAction::align_below_window_top_first_non_blank, VimActionGroup::screen_alignment},
     {VimAction::align_above_window_bottom_first_non_blank, VimActionGroup::screen_alignment},
     {VimAction::move_to_column, VimActionGroup::motion},
     {VimAction::move_display_line_start, VimActionGroup::motion},
     {VimAction::move_display_line_end, VimActionGroup::motion},
     {VimAction::move_display_line_middle, VimActionGroup::motion},
     {VimAction::move_line_middle, VimActionGroup::motion},
     {VimAction::move_display_line_first_non_blank, VimActionGroup::motion},
     {VimAction::move_last_non_blank, VimActionGroup::motion},
     {VimAction::move_first_non_blank_below, VimActionGroup::motion},
     {VimAction::join_lines, VimActionGroup::edit_line},
     {VimAction::join_lines_raw, VimActionGroup::edit_line},
     {VimAction::toggle_case_character, VimActionGroup::edit_range},
     {VimAction::toggle_case_operator, VimActionGroup::edit_range},
     {VimAction::lower_case_operator, VimActionGroup::edit_range},
     {VimAction::upper_case_operator, VimActionGroup::edit_range},
     {VimAction::shift_right_operator, VimActionGroup::edit_range},
     {VimAction::shift_left_operator, VimActionGroup::edit_range},
     {VimAction::undo_line, VimActionGroup::history},
     {VimAction::move_matching_pair, VimActionGroup::motion},
     {VimAction::set_mark, VimActionGroup::input_wait},
     {VimAction::prefix_mark_line, VimActionGroup::input_wait},
     {VimAction::prefix_mark_exact, VimActionGroup::input_wait},
     {VimAction::move_mark_line, VimActionGroup::motion},
     {VimAction::move_mark_exact, VimActionGroup::motion}}};

// オペレータの後ろで範囲になる動作。ここに無い鍵（x i a …）は保留中のオペレータを打ち消す。
constexpr std::array<VimMotionBinding, 37> motion_bindings{
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
     {VimAction::space_left, VimMotion::wrap_left},
     {VimAction::move_next_big_word, VimMotion::next_big_word},
     {VimAction::move_previous_big_word, VimMotion::previous_big_word},
     {VimAction::move_big_word_end, VimMotion::big_word_end},
     {VimAction::move_previous_word_end, VimMotion::previous_word_end},
     {VimAction::move_previous_big_word_end, VimMotion::previous_big_word_end},
     {VimAction::move_display_down, VimMotion::display_down},
     {VimAction::move_display_up, VimMotion::display_up},
     {VimAction::move_to_column, VimMotion::column},
     {VimAction::move_display_line_start, VimMotion::display_line_start},
     {VimAction::move_display_line_end, VimMotion::display_line_end},
     {VimAction::move_display_line_middle, VimMotion::display_line_middle},
     {VimAction::move_line_middle, VimMotion::line_middle},
     {VimAction::move_display_line_first_non_blank, VimMotion::display_line_first_non_blank},
     {VimAction::move_last_non_blank, VimMotion::last_non_blank},
     {VimAction::move_first_non_blank_below, VimMotion::first_non_blank_below},
     {VimAction::move_matching_pair, VimMotion::matching_pair},
     {VimAction::move_mark_line, VimMotion::mark_line},
     {VimAction::move_mark_exact, VimMotion::mark_exact}}};

// g の後ろの鍵 → 動作（ADR 0027 / Issue #222 / ADR 0057 の決定 2 / ADR 0144 の決定 4 /
// ADR 0147 の決定 1・8 / ADR 0015 の決定 9・10）。ここに無い鍵は g を打ち消す。
constexpr std::array<VimBinding, 17> g_bindings{
    {{U'g', VimAction::move_document_first},
     {U'e', VimAction::move_previous_word_end},
     {U'E', VimAction::move_previous_big_word_end},
     {U't', VimAction::next_tab},
     {U'T', VimAction::previous_tab},
     {U'j', VimAction::move_display_down},
     {U'k', VimAction::move_display_up},
     {U'0', VimAction::move_display_line_start},
     {U'$', VimAction::move_display_line_end},
     {U'm', VimAction::move_display_line_middle},
     {U'M', VimAction::move_line_middle},
     {U'^', VimAction::move_display_line_first_non_blank},
     {U'_', VimAction::move_last_non_blank},
     {U'J', VimAction::join_lines_raw},
     {U'~', VimAction::toggle_case_operator},
     {U'u', VimAction::lower_case_operator},
     {U'U', VimAction::upper_case_operator}}};

// 保留を立てる鍵 → オペレータ（ADR 0015 の決定 1・10・11）。同じ行の鍵を保留の後ろで押すと
// 行単位（dd g~g~ >>）。
constexpr std::array<VimOperatorBinding, 8> operator_bindings{
    {{VimAction::remove_operator, VimOperator::remove},
     {VimAction::change_operator, VimOperator::change},
     {VimAction::yank_operator, VimOperator::yank},
     {VimAction::toggle_case_operator, VimOperator::toggle_case},
     {VimAction::lower_case_operator, VimOperator::lower_case},
     {VimAction::upper_case_operator, VimOperator::upper_case},
     {VimAction::shift_right_operator, VimOperator::shift_right},
     {VimAction::shift_left_operator, VimOperator::shift_left}}};

// 保留の後ろで行単位になる、オペレータの鍵とは別の鍵（g~~ の ~・guu の u・gUU の U。Vim の
// nv_tilde・nv_undo・nv_Undo がオペレータに読み替える・ADR 0015 の決定 10）。
constexpr std::array<VimOperatorBinding, 3> doubled_bindings{
    {{VimAction::toggle_case_character, VimOperator::toggle_case},
     {VimAction::undo, VimOperator::lower_case},
     {VimAction::undo_line, VimOperator::upper_case}}};

// z の後ろの鍵 → 動作（ADR 0145 の決定 5・ADR 0146 の決定 2・9）。横送りの 6 鍵と、縦の位置合わせの
// 8 鍵（zt zz zb z<CR> z. z- z+ z^）。ここに無い鍵は Vim と同じく失敗して後ろを打ち切る。
constexpr std::array<VimBinding, 14> z_bindings{
    {{U'l', VimAction::scroll_column_right},
     {U'h', VimAction::scroll_column_left},
     {U'L', VimAction::scroll_half_width_right},
     {U'H', VimAction::scroll_half_width_left},
     {U's', VimAction::scroll_caret_to_left},
     {U'e', VimAction::scroll_caret_to_right},
     {U't', VimAction::align_top},
     {U'z', VimAction::align_center},
     {U'b', VimAction::align_bottom},
     {carriage_return_character, VimAction::align_top_first_non_blank},
     {U'.', VimAction::align_center_first_non_blank},
     {U'-', VimAction::align_bottom_first_non_blank},
     {U'+', VimAction::align_below_window_top_first_non_blank},
     {U'^', VimAction::align_above_window_bottom_first_non_blank}}};

// `Ctrl-w` の後ろの鍵 → 窓の命令（ADR 0148 の決定 7）。Ctrl-s は窓が写さないので載せない。
constexpr std::array<VimWindowBinding, 15> window_bindings{
    {{U's', VimWindowCommand::split_horizontal},
     {U'S', VimWindowCommand::split_horizontal},
     {U'v', VimWindowCommand::split_vertical},
     {control_v_character, VimWindowCommand::split_vertical},
     {U'w', VimWindowCommand::next},
     {U'W', VimWindowCommand::next},
     {U'p', VimWindowCommand::next},
     {control_w_character, VimWindowCommand::next},
     {U'h', VimWindowCommand::left},
     {U'j', VimWindowCommand::down},
     {U'k', VimWindowCommand::up},
     {U'l', VimWindowCommand::right},
     {U'c', VimWindowCommand::close},
     {U'q', VimWindowCommand::close},
     {U'o', VimWindowCommand::only}}};

// 縦の位置合わせの動作 → 置く所・最初の非空白へ動かすか・行の選び方（ADR 0146 の決定 2・9・
// CPP-012）。
constexpr std::array<VimScreenAlignmentBinding, 8> screen_alignment_bindings{
    {{VimAction::align_top, {VimScreenPlace::top, false, VimAlignedLine::caret}},
     {VimAction::align_center, {VimScreenPlace::center, false, VimAlignedLine::caret}},
     {VimAction::align_bottom, {VimScreenPlace::bottom, false, VimAlignedLine::caret}},
     {VimAction::align_top_first_non_blank, {VimScreenPlace::top, true, VimAlignedLine::caret}},
     {VimAction::align_center_first_non_blank,
      {VimScreenPlace::center, true, VimAlignedLine::caret}},
     {VimAction::align_bottom_first_non_blank,
      {VimScreenPlace::bottom, true, VimAlignedLine::caret}},
     {VimAction::align_below_window_top_first_non_blank,
      {VimScreenPlace::top, true, VimAlignedLine::below_window}},
     {VimAction::align_above_window_bottom_first_non_blank,
      {VimScreenPlace::bottom, true, VimAlignedLine::above_window}}}};

// 空白だけで切る（Vim の WORD の）移動。ほかの語の移動は文字の種類の表で切る（Issue #222）。
constexpr std::array<VimMotion, 5> big_word_motions{
    {VimMotion::next_big_word, VimMotion::previous_big_word, VimMotion::big_word_end,
     VimMotion::big_word_end_for_change, VimMotion::previous_big_word_end}};

// i / a の後ろの鍵 → テキストオブジェクトの表（ADR 0031 の決定 1 / CPP-012）。b は paren、
// B は brace、閉じ括弧の鍵は開き括弧と同じ行（Vim 9.1 で実測）。ここに無い鍵は取消。
constexpr std::array<VimTextObjectBinding, 18> text_object_bindings{
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
     {U'>', VimTextObject::angle},
     {U'p', VimTextObject::paragraph},
     {U's', VimTextObject::sentence},
     {U't', VimTextObject::tag}}};

// 終わりの位置を範囲に入れない移動（Vim の exclusive）。$ と e E ge gE g$ g_ は入れる
// （inclusive）。行単位の j k はどちらでもないので、この表には無い。gj gk は文字単位の
// exclusive（ADR 0144）。g0 gm gM g^ は exclusive（ADR 0147 の決定 1・8）。`%` は inclusive、
// `` `a `` は exclusive（ADR 0152 の決定 3）。
constexpr std::array<VimMotion, 18> exclusive_motions{
    {VimMotion::left, VimMotion::right, VimMotion::line_start, VimMotion::first_non_blank,
     VimMotion::next_word, VimMotion::previous_word, VimMotion::wrap_right, VimMotion::wrap_left,
     VimMotion::next_big_word, VimMotion::previous_big_word, VimMotion::display_down,
     VimMotion::display_up, VimMotion::column, VimMotion::display_line_start,
     VimMotion::display_line_middle, VimMotion::line_middle,
     VimMotion::display_line_first_non_blank, VimMotion::mark_exact}};

// 飛ぶ移動（Vim の `:help jump-motions`・ADR 0152 の決定 4）。前の位置を `''` `` `` `` に置く。
// 検索の 6 つの鍵も飛ぶが、移動の表の外なので検索の写し先が置く。
constexpr std::array<VimMotion, 8> jump_motions{
    {VimMotion::document_first, VimMotion::document_last, VimMotion::screen_top,
     VimMotion::screen_middle, VimMotion::screen_bottom, VimMotion::matching_pair,
     VimMotion::mark_line, VimMotion::mark_exact}};

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

// 縦の位置合わせの大分類の動作は、意味の表にちょうど 1 行（ADR 0146 の決定 2）。
[[nodiscard]] constexpr bool every_alignment_has_one_row() noexcept
{
    for (const VimActionBinding binding : action_groups)
    {
        std::size_t rows = 0;
        for (const VimScreenAlignmentBinding row : screen_alignment_bindings)
        {
            rows += static_cast<std::size_t>(row.action == binding.action);
        }
        if (rows != static_cast<std::size_t>(binding.group == VimActionGroup::screen_alignment))
        {
            return false;
        }
    }
    return true;
}

static_assert(every_alignment_has_one_row(),
              "縦の位置合わせの動作だけが意味の表にちょうど 1 行（CPP-012）");
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

[[nodiscard]] std::optional<VimAction> vim_g_action_for(char32_t key) noexcept
{
    for (const VimBinding binding : g_bindings)
    {
        if (binding.key == key)
        {
            return binding.action;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<VimAction> vim_z_action_for(char32_t key) noexcept
{
    for (const VimBinding binding : z_bindings)
    {
        if (binding.key == key)
        {
            return binding.action;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<VimWindowCommand> vim_window_command_for(char32_t key) noexcept
{
    for (const VimWindowBinding binding : window_bindings)
    {
        if (binding.key == key)
        {
            return binding.command;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<char32_t> vim_window_control_character(VimSpecialKey key) noexcept
{
    switch (key)
    {
    case VimSpecialKey::control_v:
        return control_v_character;
    case VimSpecialKey::control_w:
        return control_w_character;
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
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
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<VimWindowCommand> vim_window_command_for(VimSpecialKey key) noexcept
{
    return vim_window_control_character(key).and_then(
        [](char32_t character) { return vim_window_command_for(character); });
}

[[nodiscard]] VimWordClass vim_motion_word_class(VimMotion motion) noexcept
{
    for (const VimMotion entry : big_word_motions)
    {
        if (entry == motion)
        {
            return VimWordClass::big_word;
        }
    }
    return VimWordClass::word;
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

[[nodiscard]] std::optional<VimScreenAlignment> vim_screen_alignment_for(VimAction action) noexcept
{
    for (const VimScreenAlignmentBinding binding : screen_alignment_bindings)
    {
        if (binding.action == action)
        {
            return binding.alignment;
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

[[nodiscard]] std::optional<VimOperator> vim_operator_for(VimAction action) noexcept
{
    for (const VimOperatorBinding binding : operator_bindings)
    {
        if (binding.action == action)
        {
            return binding.operation;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool vim_operator_doubles(VimOperator operation, VimAction action) noexcept
{
    if (vim_operator_for(action) == operation)
    {
        return true;
    }
    for (const VimOperatorBinding binding : doubled_bindings)
    {
        if (binding.action == action && binding.operation == operation)
        {
            return true;
        }
    }
    return false;
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

[[nodiscard]] bool vim_motion_is_jump(VimMotion motion) noexcept
{
    for (const VimMotion entry : jump_motions)
    {
        if (entry == motion)
        {
            return true;
        }
    }
    return false;
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
