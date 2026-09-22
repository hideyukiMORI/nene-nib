#pragma once

#include <cstdint>

namespace nenenib::core
{
// 検索の当たりの強調（ADR 0037 の決定 1）。既定は on（施主決定 D17。Vim の既定は off）。
// `:set hlsearch` は on、`:set nohlsearch` は off、`:nohlsearch`（`:noh`）は on のときだけ
// suspended にする。検索の鍵（`/ ? n N * #` の確定・見つからなくても）は suspended を on へ
// 戻し、off は戻さない（固定 Vim 9.1 の `v:hlsearch` の遷移で実測）。
enum class VimSearchHighlight : std::uint8_t
{
    on,
    off,
    suspended
};

// Ex の求めを今の値に重ねた結果（ADR 0037 の決定 1・2）。`:nohlsearch` は on を止めるだけで、
// `:set nohlsearch` の off は止めようがない（固定 Vim の 'hlsearch' と no_hlsearch の 2 つの旗を
// 3 値へ畳んだぶん、ここで 1 か所に閉じる）。`:set (no)hlsearch` はそのまま置き換わる。
[[nodiscard]] constexpr VimSearchHighlight
requested_highlight(VimSearchHighlight current, VimSearchHighlight requested) noexcept
{
    return requested == VimSearchHighlight::suspended && current == VimSearchHighlight::off
               ? VimSearchHighlight::off
               : requested;
}
} // namespace nenenib::core
