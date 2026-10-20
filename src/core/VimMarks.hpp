#pragma once

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
// 妥当なので公開 aggregate（CPP-003）。比較は非メンバー。
struct VimMarks
{
    std::array<std::optional<VimMarkPosition>, vim_mark_count> named{};
    std::optional<VimMarkPosition> previous_context = std::nullopt;
    std::optional<VimMarkPosition> prior_context = std::nullopt;
    std::optional<VimMarkPosition> last_change = std::nullopt;
};

[[nodiscard]] constexpr bool operator==(const VimMarks &left, const VimMarks &right) noexcept
{
    return left.named == right.named && left.previous_context == right.previous_context &&
           left.prior_context == right.prior_context && left.last_change == right.last_change;
}

// 何も置いていないマーク。VimEditorView が既定で借りる（読んでいない文書・試験の view）。
inline constexpr VimMarks no_vim_marks{};
} // namespace nenenib::core
