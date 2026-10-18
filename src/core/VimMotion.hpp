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
    // 欲しい列は着地の桁ではなく N を置く（Vim の nv_pipe の curswant = N − 1）。
    column,
    // `g0` `g$` `gm`。画面行の端と中（ADR 0147 の決定 1・2）。着地は engine が鍵を写す前に、横に
    // 送る文書では見えている升から、折り返す文書では今の画面行の範囲から解く。`g$` は inclusive。
    display_line_start,
    display_line_end,
    display_line_middle,
    // `gM`。行の本文の幅の半分（回数 1〜100 は %）の升を含む文字（ADR 0147 の決定 2・3）。
    line_middle
};
} // namespace nenenib::core
