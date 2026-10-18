#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// NORMAL の鍵が表から引く動作の閉じた一覧（ADR 0012 の決定 5 / CPP-012）。
enum class VimAction : std::uint8_t
{
    move_left,
    move_down,
    move_up,
    move_right,
    move_line_start,
    move_line_end,
    move_next_word,
    move_previous_word,
    move_word_end,
    move_first_non_blank,
    move_screen_top,
    move_screen_middle,
    move_screen_bottom,
    move_document_first,
    move_document_last,
    // `+` と `<CR>`・`-`。次・前の行の最初の非空白（ADR 0048 の決定 9）。
    move_next_line,
    move_previous_line,
    scroll_half_down,
    scroll_half_up,
    scroll_page_down,
    scroll_page_up,
    remove_character,
    // `X`。`dh` と同じ範囲を同じ道で消す（行頭では空の範囲・失敗の印なし・Issue #206）。
    remove_character_before,
    remove_operator,
    change_operator,
    yank_operator,
    put_after,
    put_before,
    remove_to_line_end,
    change_to_line_end,
    yank_line,
    insert_before,
    insert_after,
    insert_at_line_start,
    insert_at_line_end,
    undo,
    redo,
    // 同じo/OをNORMALでは開行、VISUALでは端点交換へ写す（ADR 0028）。
    visual,
    visual_line,
    open_line_below,
    open_line_above,
    find_character_forward,
    find_character_backward,
    till_character_forward,
    till_character_backward,
    repeat_character_search,
    repeat_character_search_opposite,
    prefix_g,
    replace_character,
    open_command_line,
    // `.`。直前の変更の鍵の列を同じ経路へ再生する（ADR 0030 の決定 6）。
    repeat_change,
    // 検索（ADR 0032）。`/` `?` は入力行を開き、`n` `N` は覚えたパターン、`*` `#` は
    // キャレットの語を \<…\> にして同じ経路を通る。
    open_search_forward,
    open_search_backward,
    repeat_search,
    repeat_search_opposite,
    search_word_forward,
    search_word_backward,
    // マクロ（ADR 0046）。`q` は録画の開始と停止、`@` は再生。どちらも次の鍵が名前。
    record_macro,
    replay_macro,
    // `"`。次の鍵がレジスタの名前（ADR 0048 の決定 2）。
    select_register,
    // `<Space>` `<BS>`。行をまたぐ l h（ADR 0049 の決定 1）。
    space_right,
    space_left,
    // `W` `B` `E` と `ge` `gE`。語の移動と同じ歩き方で、WORD は空白だけで切る（Issue #222）。
    move_next_big_word,
    move_previous_big_word,
    move_big_word_end,
    move_previous_word_end,
    move_previous_big_word_end,
    // `gt` `gT`。行き先は tab_destination が決める（ADR 0057 の決定 2）。
    next_tab,
    previous_tab,
    // `gj` `gk`。画面行で上下する（ADR 0144 の決定 4）。行き先は画面行の問いで engine が解く。
    move_display_down,
    move_display_up,
    // `z` の接頭辞と、窓と wrap off の横送り `zl` `zh` `zL` `zH` `zs` `ze`（ADR 0145 の決定 5）。
    // motion ではなく画面の操作で、新しい leftcol は engine が決める。
    prefix_z,
    scroll_column_right,
    scroll_column_left,
    scroll_half_width_right,
    scroll_half_width_left,
    scroll_caret_to_left,
    scroll_caret_to_right,
    // 縦の位置合わせ `zt` `zz` `zb`（キャレットを動かさない）と `z<CR>` `z.` `z-`（行の最初の
    // 非空白へ）。先頭行は engine が決める（ADR 0146 の決定 2）。値 → 位置の表は VimKeyTable。
    align_top,
    align_center,
    align_bottom,
    align_top_first_non_blank,
    align_center_first_non_blank,
    align_bottom_first_non_blank,
    // `z+`（窓の 1 つ下の行を上へ）と `z^`（窓の 1 つ上の行を下へ）。どちらも行の最初の非空白へ
    // 動く。行の選び方は表の VimAlignedLine（ADR 0146 の決定 9）。
    align_below_window_top_first_non_blank,
    align_above_window_bottom_first_non_blank,
    // `{N}|`。行頭からの仮想桁 N の文字へ（Issue #660）。
    move_to_column,
    // `g0` `g$` `gm` `gM`。画面行の端と中・行の中（ADR 0147 の決定 1）。
    move_display_line_start,
    move_display_line_end,
    move_display_line_middle,
    move_line_middle
};

// 動作の個数（末尾の値から導く）。動作 → 大分類の表の大きさをこれに固定し、欠落と重複を
// static_assert で落とす（CPP-012 の表と網羅性。VimStep.cpp）。新しい値は末尾に足す。
constexpr std::size_t vim_action_count = static_cast<std::size_t>(VimAction::move_line_middle) + 1;
} // namespace nenenib::core
