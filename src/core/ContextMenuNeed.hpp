#pragma once

#include <cstdint>

namespace nenenib::core
{
// メニューの項目が使えるために要るもの（ADR 0078 の追補 #1051 / #1790）。満たさない項目は薄く出して
// 選べない。
// 本文のメニュー（通常モード・Vim の INSERT・全面プレビュー）: 選択は本文に選んだ範囲があること、
// 書き換えは本文を書き換えてよい所（全面プレビューでない）にいること。Vim の NORMAL と VISUAL は
// Vim の鍵の列があるかで決める。
// タブのメニュー: named はパスがある、loaded は中身を読んである、other_tabs はほかのタブがある、
// right_tabs は右にタブがある（TabMenuSituation）。
// 本文の行はタブの値を、タブの行は本文の値（nothing を除く）を持たない（ContextMenu.cpp の
// static_assert）。
enum class ContextMenuNeed : std::uint8_t
{
    nothing,
    selection,
    editable,
    editable_selection,
    named,
    loaded,
    other_tabs,
    right_tabs
};
} // namespace nenenib::core
