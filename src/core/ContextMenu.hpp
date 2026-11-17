#pragma once

#include "ContextMenuItem.hpp"
#include "ContextMenuLayout.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuRequest.hpp"
#include "ContextMenuShape.hpp"
#include "ContextMenuSituation.hpp"
#include "EditorOperation.hpp"
#include "LayoutRect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 本文の右クリックのメニューの項目の列（ADR 0078 の追補 #1051）。操作の表の列 context_menu の群と
// 順に並べ、群の変わり目の行に区切り線の印を立てる。使えるかは列の need と Vim の鍵の列と situation
// で決める。
[[nodiscard]] std::array<ContextMenuItem, context_menu_size>
context_menu_items(const ContextMenuSituation &situation) noexcept;

// 行の右端の鍵の表示。通常モードと全面プレビューは通常モードの shown_chord を key_chord_label で、
// Vim の INSERT は同じ鍵のうち INSERT で効くもの（クリップボードと全選択）、Vim の NORMAL と VISUAL
// は 表の Vim の鍵の列。無ければ空。
[[nodiscard]] std::string context_menu_key(EditorOperation operation, ContextMenuMode mode);

// Vim の NORMAL と VISUAL でメニューから選ぶと engine に流す鍵の列（鍵の記法）。ほかの所と、列の
// 無い項目は空。
[[nodiscard]] std::string_view context_menu_vim_keys(EditorOperation operation,
                                                     ContextMenuMode mode) noexcept;

// 面と行と区切り線の矩形。幅は 左右の余白 ＋ 名前 ＋ 空き ＋ 鍵（最小 200 DIP）。開く点から右下へ
// 開き、bounds の右端・下端に収まらなければ点から左・上へ開く。それでも出る分は bounds の左上に
// 寄せる。
[[nodiscard]] ContextMenuLayout context_menu_layout(const ContextMenuRequest &request) noexcept;

// 点が載っている項目の番号。区切り線と上下の空きと面の外は値なし。
[[nodiscard]] std::optional<std::size_t> context_menu_hit(const ContextMenuLayout &layout,
                                                          std::int32_t x, std::int32_t y) noexcept;

// 行の内側（左右 12 DIP）。名前は左寄せ、鍵の表示は右寄せでこの中に描く。
[[nodiscard]] LayoutRect context_menu_row_inside(const ContextMenuLayout &layout,
                                                 std::size_t index) noexcept;
} // namespace nenenib::core
