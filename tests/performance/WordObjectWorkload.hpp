#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 語・引用符・括弧のテキストオブジェクトの巨大な 1 行の費用（#1424）。
// 範囲関数 vim_text_object_range を NORMAL の畳んだキャレットで 1 回引く。
// キャレットは行の真ん中の 2 バイト後ろ、回数は 1。
// 行は改行なしで、巨大な行のしきい値ちょうど（16,384）と 16 MiB の 2 つの大きさ。
// solid は空白の無い 1 語、spaced は `lorem ipsum dolor sit amet ` の繰り返し。
// quote と paren は spaced の本文を `"` と `(` `)` で囲む。
// 最後の 1 本は通常モードのダブルクリックの語の選択（#1250）。
// PointText の PointerClicks::double_ を controller に流す（しきい値ちょうどの solid の行）。
enum class WordObjectWorkload : std::uint8_t
{
    iw_solid_16384,
    aw_solid_16384,
    iw_spaced_16384,
    aw_spaced_16384,
    iquote_16384,
    iparen_16384,
    iw_solid_16mib,
    aw_solid_16mib,
    iw_spaced_16mib,
    aw_spaced_16mib,
    iquote_16mib,
    iparen_16mib,
    double_click_solid_16384
};
} // namespace nenenib::tests::performance
