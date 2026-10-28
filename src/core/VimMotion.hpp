#pragma once

#include <cstdint>

namespace nenenib::core
{
// オペレータが取れる移動の閉じた一覧（ADR 0012 の決定 5）。範囲はこの値ごとに決まる。
// word_end_for_change は cw の特例（語の上では ce の範囲になる。Vim の nv_wordcmd の flag）で、
// 鍵からは引けない＝オペレータが change のときに engine が w から差し替える値である。
enum class VimMotion : std::uint8_t
{
    left,
    down,
    up,
    right,
    line_start,
    line_end,
    next_word,
    previous_word,
    word_end,
    word_end_for_change,
    first_non_blank,
    screen_top,
    screen_middle,
    screen_bottom,
    document_first,
    document_last,
    next_line,
    previous_line,
    // `<Space>` `<BS>`。行をまたぐ exclusive な文字単位の移動（ADR 0049 の決定 1・2）。
    wrap_right,
    wrap_left,
    // `W` `B` `E` と cW の特例、`ge` `gE`（Issue #222）。語の分類は VimKeyTable の表が決める。
    next_big_word,
    previous_big_word,
    big_word_end,
    big_word_end_for_change,
    previous_word_end,
    previous_big_word_end,
    // `gj` `gk`。画面行で上下する文字単位の exclusive な移動（ADR 0144 の決定 4）。文字組みが
    // 無効なら engine が down / up に読み替える。
    display_down,
    display_up,
    // `{N}|`。行頭からの仮想桁 N（既定 1）の文字へ動く exclusive な移動（Issue #660・ADR 0034）。
    // 欲しい列は着地の桁ではなく N を置く（持つのは 1 始まりの N・Vim の nv_pipe の curswant は
    // 0 始まりの N − 1）。
    column,
    // `g0` `g$` `gm`。画面行の端と中（ADR 0147 の決定 1・2）。着地は engine が鍵を写す前に、横に
    // 送る文書では見えている升から、折り返す文書では今の画面行の範囲から解く。`g$` は inclusive。
    display_line_start,
    display_line_end,
    display_line_middle,
    // `gM`。行の本文の幅の半分（回数 1〜100 は %）の升を含む文字（ADR 0147 の決定 2・3）。
    line_middle,
    // `g^`。`g0` の着地から空白（空白と Tab）を右へ越えた最初の文字。行の最後の文字で止まる
    // （Issue #721・ADR 0147 の決定 8・Vim の nv_g_cmd）。exclusive。
    display_line_first_non_blank,
    // `g_`。回数 − 1 行下の行の最後の非空白（空白だけの行と空行は行頭）。inclusive（Issue #721・
    // ADR 0147 の決定 8）。
    last_non_blank,
    // `_`。回数 − 1 行下の行の最初の非空白（`1_` は今の行）。行単位で `d_` は `dd` と同じ
    // 範囲。最終行で回数が 2 以上なら失敗する（Vim の nv_lineop の cursor_down・#976）。
    first_non_blank_below,
    // `%`。対応する括弧（inclusive の文字単位）、回数つきは本文の回数 % の行（行単位・Issue
    // #767）。
    matching_pair,
    // `'{a-z}` `''` `'.` と `` `{a-z} `` `` `` `` `` `. ``。マークの行の最初の非空白（行単位）と、
    // マークの位置（exclusive の文字単位）。着地は engine が鍵を写す前に VimEditorView.mark_landing
    // に置く（ADR 0152 の決定 3）。
    mark_line,
    mark_exact
};
} // namespace nenenib::core
