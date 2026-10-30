#pragma once

#include <cstdint>

namespace nenenib::application
{
// 全面プレビューの間に届いた意図の行き先（ADR 0155 の改訂 17）。決めるのは controller の入口の
// 1 か所（preview_full_route）で、意図の種類（と鍵）だけから引く。
// pass は今までどおり写す（タブ・Ctrl+P・F1・窓の操作・Ex の入力行・合図・チェックボックスの
// クリック）。leave は Esc で直前の見せ方へ戻る。line_down / line_up / half_down / half_up /
// page_down / page_up / top / bottom / wheel は錨を本文の行で動かす（wheel は意図の行数）。
// vim_g は Vim の `g` の待ち（次の鍵で `gg` `gt` `gT`）。task は通常モードの Space で、キャレットの
// 行のタスクを切り替える（追補 25）。select は選択とコピーの口（Shift+矢印・Ctrl+A・Ctrl+C・`y`・
// マウス・改訂 18 は N5b）。ignore は届くが何もしない鍵（左右の矢印・Home / End）。refuse は本文に
// 届かない鍵で、1 行知らせる（鳴らさない）。値が増えたら写し先の switch が落ちる（CPP-002）。
enum class PreviewFullRoute : std::uint8_t
{
    pass,
    leave,
    line_down,
    line_up,
    half_down,
    half_up,
    page_down,
    page_up,
    top,
    bottom,
    wheel,
    vim_g,
    task,
    select,
    ignore,
    refuse
};
} // namespace nenenib::application
