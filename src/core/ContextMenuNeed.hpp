#pragma once

#include <cstdint>

namespace nenenib::core
{
// メニューの項目が使えるために要るもの（ADR 0078 の追補 #1051）。選択は本文に選んだ範囲があること、
// 書き換えは本文を OS のクリップボードの鍵で書き換えてよい所にいること（通常モードと Vim の
// INSERT・ 全面プレビューでない）。満たさない項目は薄く出して選べない。
enum class ContextMenuNeed : std::uint8_t
{
    nothing,
    selection,
    editable,
    editable_selection
};
} // namespace nenenib::core
