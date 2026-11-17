#pragma once

#include <cstdint>

namespace nenenib::core
{
// メニューを開いた所の種類（ADR 0078 の追補 #1051）。項目が使えるかと鍵の表示と実行の道を決める。
// ordinary は通常モード、reading は全面プレビュー（選択とコピーだけ・どちらのモードでも）、
// vim_insert は Vim の INSERT（通常モードと同じ意図）、vim_normal と vim_visual は Vim の NORMAL と
// VISUAL（3 つの VISUAL）で、項目は表の Vim の鍵の列を engine に流す。
enum class ContextMenuMode : std::uint8_t
{
    ordinary,
    reading,
    vim_insert,
    vim_normal,
    vim_visual
};
} // namespace nenenib::core
