#pragma once

#include "EasyMotionKind.hpp"
#include "EasyMotionOutcome.hpp"
#include "EasyMotionState.hpp"
#include "Offset.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// イージーモーションの純関数（ADR 0163 の決定 3・5）。engine の鍵の道（VimStepEasyMotion.cpp）が
// 呼び、契約と性質の試験が直接呼ぶ。どれも OS・時刻に触れない。

// 印の文字（vim-easymotion の既定 `g:EasyMotion_keys`）。27 文字で、印は多くとも 27 × 27 個。
inline constexpr std::string_view easymotion_keys = "asdghklqwertyuiopzxcvbnmfj;";
inline constexpr std::size_t easymotion_target_limit =
    easymotion_keys.size() * easymotion_keys.size();

// 見えている行（view.viewport の最初の行から見えている行数ぶん・本文の行の数で切る）の跳び先を
// キャレットから近い順に返す（距離は（行の差・仮想桁の差）の辞書順・同じならキャレットより後ろが
// 先・ADR 0163 の追補 2026-11-17 の 2）。印は easymotion_labels の順にこの列の位置へ付く（最初の
// 印 `a` がいちばん近い跳び先）。キャレットの位置は除き、w e f j は後ろ、b F k は前、s l
// は両方。語の切れ目は `w` と同じ語の分類（vim_character_class の word・1 文字は
// vim_character_end）で、空行は語の頭。 j k は行の最初の非空白でキャレットの行を除く。f F s は
// character と同じ code point で始まる 文字（character
// が無ければ空）。表示領域が無い・格子の表示（VimScrollUnit::row）・行が無い
// 本文では空。easymotion_target_limit を超えたら近い順に並べてから切る。
[[nodiscard]] std::vector<Offset> easymotion_targets(const VimEditorView &view, EasyMotionKind kind,
                                                     std::optional<char32_t> character);

// count 個の印（多くとも easymotion_target_limit 個）。27 以下なら 1 文字を easymotion_keys の
// 順に。超えたら後ろの鍵を組の鍵にして 2 文字目で選ぶ（vim-easymotion の組の割り当てと同じに、
// 前の跳び先ほど 1 文字の印・組の数は最小・どの組も 2 つ以上）。どの印も他の印の接頭辞に
// ならず、重複しない。決定的。
[[nodiscard]] std::vector<std::string> easymotion_labels(std::size_t count);

// 印が出ているあいだの 1 鍵（決定 5）。typed に続けて印の接頭辞なら絞り、印に一致すれば跳び、
// どの印にも当たらない文字・文字でない鍵は取消。
[[nodiscard]] EasyMotionOutcome easymotion_step(const EasyMotionState &state, const VimKey &key);
} // namespace nenenib::core
