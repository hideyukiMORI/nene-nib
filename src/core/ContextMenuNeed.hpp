#pragma once

#include <cstdint>

namespace nenenib::core
{
// 通常モード・Vim の INSERT・全面プレビューで、メニューの項目が使えるために要るもの（ADR 0078 の
// 追補 #1051）。選択は本文に選んだ範囲があること、書き換えは本文を書き換えてよい所（全面
// プレビューでない）にいること。Vim の NORMAL と VISUAL は Vim の鍵の列があるかで決める。満たさない
// 項目は薄く出して選べない。
enum class ContextMenuNeed : std::uint8_t
{
    nothing,
    selection,
    editable,
    editable_selection
};
} // namespace nenenib::core
