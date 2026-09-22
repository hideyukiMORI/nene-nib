#pragma once

#include <cstdint>

namespace nenenib::core
{
// 動作の大分類（CPP-012 / ADR 0006）。48 を超えた動作の分岐は 1 つの switch に収まらない
// （関数長 60 行で落ちる）ので、動作 → 分類は表で引き、分類ごとの写し先を NORMAL と VISUAL が
// それぞれ網羅する switch で持つ。分類が増えたら両方の switch がコンパイルで落ちる（CPP-002）。
enum class VimActionGroup : std::uint8_t
{
    // 表から引ける移動（h j k l 0 $ w b e ^ H M L gg G）。
    motion,
    // 半画面と 1 画面の巻き。
    scroll,
    // VISUAL に入る v V と、NORMAL の開行・VISUAL の端点交換 o O。
    enter_visual,
    // 範囲に効く本文の変更（x d c y）。VISUAL では選択に効く。
    edit_range,
    // 行に効く本文の変更（p P D C Y）。VISUAL では効かない。
    edit_line,
    // i a。NORMAL では挿入、VISUAL ではテキストオブジェクトの接頭辞。
    insert_object,
    // I A。VISUAL では効かない。
    insert_line,
    // u Ctrl-r `.`。VISUAL では効かない。
    history,
    // 次の 1 鍵を待つ（f F t T ; , g r）。
    input_wait,
    // Ex の `:`。VISUAL では効かない（範囲は解釈しない）。
    ex_line,
    // 検索（`/` `?` `n` `N` `*` `#`・ADR 0032）。
    search
};
} // namespace nenenib::core
