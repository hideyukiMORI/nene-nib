#pragma once

#include "VimAction.hpp"
#include "VimActionGroup.hpp"
#include "VimMotion.hpp"
#include "VimOperator.hpp"
#include "VimScreenAlignment.hpp"
#include "VimSpecialKey.hpp"
#include "VimTextObject.hpp"
#include "VimWindowCommand.hpp"
#include "VimWordClass.hpp"

#include <optional>

namespace nenenib::core
{
// Vim の鍵の表を引く純関数（ADR 0042 の決定 6・CPP-012）。表を触るときはここ（VimKeyTable.cpp）。
// NORMAL と VISUAL は同じ鍵 → 動作の表を引き、動作 → 大分類・動作 → 範囲の移動・i / a の後ろの
// 鍵 → テキストオブジェクト・g と z の後ろの鍵 → 動作・語の分類・exclusive な移動の表も
// ここに 1 本ずつある（ARC-001）。表は VimKeyTable.cpp の無名名前空間に閉じ、動作と大分類の
// 表の行の欠落と重複はそこで static_assert が落とす。

// NORMAL / VISUAL の鍵 → 動作。表に無い鍵は nullopt。
[[nodiscard]] std::optional<VimAction> vim_action_for(char32_t key) noexcept;

// g の後ろの鍵 → 動作（gg ge gE gt gT gj gk）。表に無い鍵は nullopt（g を打ち消す）。
[[nodiscard]] std::optional<VimAction> vim_g_action_for(char32_t key) noexcept;

// z の後ろの鍵 → 動作（zl zh zL zH zs ze・ADR 0145 の決定 5。zt zz zb z<CR> z. z-・ADR 0146 の
// 決定 2）。<CR> は 0x0D で引く。表に無い鍵は nullopt（失敗して後ろを打ち切る）。
[[nodiscard]] std::optional<VimAction> vim_z_action_for(char32_t key) noexcept;

// `Ctrl-w` の後ろの鍵 → 窓の命令（ADR 0148 の決定 7）。Ctrl-v と Ctrl-w は 0x16 と 0x17 で引く。
// 表に無い鍵は nullopt（失敗して後ろを打ち切る）。
[[nodiscard]] std::optional<VimWindowCommand> vim_window_command_for(char32_t key) noexcept;

// `Ctrl-w` の後ろの特別な鍵 → 窓の命令。Ctrl-v と Ctrl-w は上の表の 0x16 と 0x17 の行を引き、
// ほかの特別な鍵は nullopt。
[[nodiscard]] std::optional<VimWindowCommand> vim_window_command_for(VimSpecialKey key) noexcept;

// Ctrl-v と Ctrl-w の文字（0x16 と 0x17・表の鍵と同じ値）。ほかの特別な鍵は nullopt。
[[nodiscard]] std::optional<char32_t> vim_window_control_character(VimSpecialKey key) noexcept;

// Ctrl-a と Ctrl-x の文字（0x01 と 0x18・`g` の表の鍵と同じ値・ADR 0015 の決定 14）。ほかの
// 特別な鍵は nullopt。
[[nodiscard]] std::optional<char32_t> vim_number_control_character(VimSpecialKey key) noexcept;

// 縦の位置合わせの動作 → 置く所と最初の非空白へ動かすか（ADR 0146 の決定 2）。ほかの動作は
// nullopt。
[[nodiscard]] std::optional<VimScreenAlignment> vim_screen_alignment_for(VimAction action) noexcept;

// 語の移動がどちらの語の分類で切るか（w b e ge は word・W B E gE は big_word・Issue #222）。
// 語の移動でない値は word（使われない）。
[[nodiscard]] VimWordClass vim_motion_word_class(VimMotion motion) noexcept;

// 動作 → 大分類（NORMAL と VISUAL の写し先）。
[[nodiscard]] std::optional<VimActionGroup> vim_group_for(VimAction action) noexcept;

// オペレータの後ろで範囲になる動作。範囲にならない動作は nullopt。
[[nodiscard]] std::optional<VimMotion> vim_motion_for(VimAction action) noexcept;

// 保留を立てる動作 → オペレータ（d c y g~ gu gU > <）。ほかの動作は nullopt。
[[nodiscard]] std::optional<VimOperator> vim_operator_for(VimAction action) noexcept;

// 保留中のオペレータの後ろでその動作が行単位を意味するか（dd g~~ g~g~ guu gugu gUU gUgU >>
// <<・ADR 0015 の決定 10）。
[[nodiscard]] bool vim_operator_doubles(VimOperator operation, VimAction action) noexcept;

// i / a の後ろの鍵 → テキストオブジェクト（ADR 0031 の決定 1）。表に無い鍵は nullopt（取消）。
[[nodiscard]] std::optional<VimTextObject> vim_text_object_for(char32_t key) noexcept;

// 前の位置を `''` に置く移動（Vim の jump-motions・ADR 0152 の決定 4）か。
[[nodiscard]] bool vim_motion_is_jump(VimMotion motion) noexcept;

// 終わりの位置を範囲に入れない移動（Vim の exclusive）か。
[[nodiscard]] bool vim_motion_is_exclusive(VimMotion motion) noexcept;
} // namespace nenenib::core
