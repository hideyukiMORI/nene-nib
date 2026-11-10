#pragma once

#include "LineNumber.hpp"

#include <cstdint>

namespace nenenib::core
{
// 画面移動の後に application が窓の先頭をどう決め直すか（ADR 0019 の追補 #1232 / #1362）。
// margin_followed は効果を置くときに余白の最小の追従を掛け、鍵の終わりに Vim の update_topline
// の写しも掛ける（今までの形）。updated は余白の追従を掛けず、update_topline の写しだけを
// 掛ける（Vim で VALID_TOPLINE が落ちた形）。kept はどちらも掛けない（Vim の VALID_TOPLINE が
// 残った形・Ctrl-e Ctrl-y の scroll_redraw と、1 画面の送りの FAIL でキャレットが動かなかった形）。
enum class VimToplineSettle : std::uint8_t
{
    margin_followed,
    updated,
    kept
};

// 1 画面の送り（Vim の onepage）が FAIL で返った後の決め直し（#1362）。onepage は FAIL で
// 最後の check_top_offset の寄せを飛ばす。後の update_topline は VALID_TOPLINE が残っていれば
// 飛ぶ。VALID_TOPLINE は、周がキャレットを動かすと次の周の validate_botline（comp_botline の
// check_cursor_moved）が落とし、cursor_correct が余白の外のキャレットを置き直すと立て直す。
// 置き直した行が周の後の行と違えば、update_topline の check_cursor_moved がまた落とす。
// before は鍵の前のキャレットの行、paged は周の後（cursor_correct の前）の行、corrected は
// cursor_correct の後の行、within は cursor_correct が早めに戻ったか。
[[nodiscard]] constexpr VimToplineSettle vim_failed_page_topline(LineNumber before,
                                                                 LineNumber paged,
                                                                 LineNumber corrected,
                                                                 bool within) noexcept
{
    if (!(corrected == paged))
    {
        return VimToplineSettle::updated;
    }
    return within && !(paged == before) ? VimToplineSettle::updated : VimToplineSettle::kept;
}
} // namespace nenenib::core
