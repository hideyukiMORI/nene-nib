#pragma once

#include <cstddef>

namespace nenenib::application
{
// ADR 0139 の決定 3〜5: text の表示の巨大な行は、錨の周りの窓だけを表示値に持つ。active な行の
// text / display / selection / matches / composed は窓の中だけで、表示文字列の中の窓の始まりは
// 常に 0。byte は行頭から窓の始まりまでのバイト数、code_point はその前のコードポイントの数
// （本文の桁は code_point + 窓の中の桁）、column と end_column は窓の両端の仮想桁（行頭から・
// 0 始まり）。x は窓の始まりの見積もった x（仮想桁 × 字幅）を、キャレットの行の窓の始まりから
// の相対で持つ（物理画素・キャレットの行の窓なら 0）。active でない行は今日どおり行の全体。
struct LineWindow
{
    bool active = false;
    std::size_t byte = 0;
    std::size_t code_point = 0;
    std::size_t column = 0;
    std::size_t end_column = 0;
    float x = 0;
};
} // namespace nenenib::application
