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
    move_line_middle,
    // `g^` は画面行の始まり（`g0` と同じ源）から最初の非空白へ、`g_` は行の最後の非空白へ
    // （Issue #721・ADR 0147 の決定 8）。
    move_display_line_first_non_blank,
    move_last_non_blank,
    // `_`。回数 − 1 行下の行の最初の非空白（Issue #976）。
    move_first_non_blank_below,
    // `J` と `gJ`。回数の行（最小 2）をつなぐ。オペレータではない（ADR 0015 の決定 9）。
    join_lines,
    join_lines_raw,
    // `~`。tildeop off の 1 文字の反転（ADR 0015 の決定 10）。g~ の後ろでは行単位、VISUAL では
    // 選択の反転。
    toggle_case_character,
    // `g~` `gu` `gU` と `>` `<`。保留するオペレータ（ADR 0015 の決定 10・11）。
    toggle_case_operator,
    lower_case_operator,
    upper_case_operator,
    shift_right_operator,
    shift_left_operator,
    // `U`。NORMAL の行の undo は未実装で、今までの表に無い鍵と同じに打ち消す。gU の後ろでは
    // 行単位（gUU）、VISUAL では選択の大文字化（ADR 0015 の決定 10）。
    undo_line,
    // `%`。対応する括弧、回数つきは本文の回数 % の行（Issue #767）。
    move_matching_pair,
    // `m`。次の鍵がマークの名前（ADR 0152 の決定 3）。
    set_mark,
    // `'` と `` ` ``。次の鍵がマークの名前で、着地は move_mark_line / move_mark_exact（決定 3）。
    prefix_mark_line,
    prefix_mark_exact,
    // マークへの移動。鍵からは引かず、`'` `` ` `` の次の鍵が名前を解いてから写す（決定 3）。
    move_mark_line,
    move_mark_exact,
    // `gv`。前の選択を選び直す。VISUAL の中では今の選択と入れ替える（ADR 0018 の追補・#1039）。
    reselect_visual,
    // `gi`。最後に INSERT を抜けた位置（`'^`）で `i` を始める（ADR 0152 の追補・#1039）。
    insert_at_last_insert,
    // `Ctrl-a` `Ctrl-x`。キャレットの上か右の数を回数だけ増減する（ADR 0015 の決定 14）。VISUAL
    // では 各行の最初の数。
    increment_number,
    decrement_number,
    // VISUAL の `g Ctrl-a` `g Ctrl-x`。行ごとに回数 × 何番目の数かだけ増減する（連番）。NORMAL では
    // Vim と同じく失敗する。
    increment_sequence,
    decrement_sequence,
    // ジャンプの列と変更の列を歩く（`Ctrl-o` `Ctrl-i`（`<Tab>`）と `g;` `g,`・ADR 0152 の追補
    // #1037）。NORMAL でだけ動き、オペレータの後ろと VISUAL では失敗する（Vim の checkclearopq）。
    jump_older,
    jump_newer,
    change_older,
    change_newer
};

// 動作の個数（末尾の値から導く）。動作 → 大分類の表の大きさをこれに固定し、欠落と重複を
// static_assert で落とす（CPP-012 の表と網羅性。VimStep.cpp）。新しい値は末尾に足す。
constexpr std::size_t vim_action_count = static_cast<std::size_t>(VimAction::change_newer) + 1;
} // namespace nenenib::core
