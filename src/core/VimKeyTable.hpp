#pragma once

#include "VimAction.hpp"
#include "VimActionGroup.hpp"
#include "VimMotion.hpp"
#include "VimTextObject.hpp"

#include <optional>

namespace nenenib::core
{
// Vim の鍵の表を引く純関数（ADR 0042 の決定 6・CPP-012）。表を触るときはここ（VimKeyTable.cpp）。
// NORMAL と VISUAL は同じ鍵 → 動作の表を引き、動作 → 大分類・動作 → 範囲の移動・i / a の後ろの
// 鍵 → テキストオブジェクト・exclusive な移動の表もここに 1 本ずつある（ARC-001）。表は
// VimKeyTable.cpp の無名名前空間に閉じ、動作と大分類の表の行の欠落と重複はそこで static_assert が
// 落とす。

// NORMAL / VISUAL の鍵 → 動作。表に無い鍵は nullopt。
[[nodiscard]] std::optional<VimAction> vim_action_for(char32_t key) noexcept;

// 動作 → 大分類（NORMAL と VISUAL の写し先）。
[[nodiscard]] std::optional<VimActionGroup> vim_group_for(VimAction action) noexcept;

// オペレータの後ろで範囲になる動作。範囲にならない動作は nullopt。
[[nodiscard]] std::optional<VimMotion> vim_motion_for(VimAction action) noexcept;

// i / a の後ろの鍵 → テキストオブジェクト（ADR 0031 の決定 1）。表に無い鍵は nullopt（取消）。
[[nodiscard]] std::optional<VimTextObject> vim_text_object_for(char32_t key) noexcept;

// 終わりの位置を範囲に入れない移動（Vim の exclusive）か。
[[nodiscard]] bool vim_motion_is_exclusive(VimMotion motion) noexcept;
} // namespace nenenib::core
