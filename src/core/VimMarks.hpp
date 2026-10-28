#pragma once

#include "LineNumber.hpp"
#include "VimMarkPosition.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 名前つきのマーク a〜z の本数（ADR 0152 の決定 1）。
inline constexpr std::size_t vim_mark_count = 26;

// Vim のマーク（ADR 0152 の決定 1）。文書の値で、Vim がバッファごとに持つのと同じに文書ごとに
// 1 つ持つ（窓に 1 つの VimState には入れない）。named は `m{a-z}` の 26 本、previous_context は
// 飛ぶ鍵の前の位置（`''` と `` `` ``）、last_change は最後に変更した位置（`'.` と `` `. ``）。
// prior_context は飛ぶ鍵が置き換える前の previous_context で、鍵 1 つの終わりに controller が
// vim_marks_checked で読んで消す（Vim の w_prev_pcmark と checkpcmark・決定 5）。どの値も単独で
// 妥当なので公開 aggregate（CPP-003）。比較は非メンバー。previous_context の既定は 1 行目の桁 0
// で、Vim がバッファを開いた時点で pcmark を (1, 1) に置くのと同じ（ADR 0152 の追補・#962）。
// 開いた文書・新しいタブ・まだ読んでいない文書を読んだ束はこの既定の値で始まる。
struct VimMarks
{
    std::array<std::optional<VimMarkPosition>, vim_mark_count> named{};
    std::optional<VimMarkPosition> previous_context = VimMarkPosition{LineNumber{1}, 0};
    std::optional<VimMarkPosition> prior_context = std::nullopt;
    std::optional<VimMarkPosition> last_change = std::nullopt;
};

[[nodiscard]] constexpr bool operator==(const VimMarks &left, const VimMarks &right) noexcept
{
    return left.named == right.named && left.previous_context == right.previous_context &&
           left.prior_context == right.prior_context && left.last_change == right.last_change;
}

// 開いた時点のマーク（名前つきと最後の変更は無く、前の位置は 1 行目）。VimEditorView が既定で
// 借りる（読んでいない文書・試験の view）。
inline constexpr VimMarks no_vim_marks{};
} // namespace nenenib::core
