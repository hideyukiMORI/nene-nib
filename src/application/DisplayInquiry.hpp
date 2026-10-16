#pragma once

#include <utility>

namespace nenenib::application
{
// 開いた文書に格子表示を聞く問いの段階（ADR 0140 の決定 2）。文書ごとの値で、タブを脇に置いても
// 束に載って戻る。
// unjudged … まだ判定していない（開いた直後・まだ読んでいなかった文書）
// unneeded … 判定して、巨大な行が無かった
// waiting  … 巨大な行があり、まだ聞いていない（Ctrl+Tab の歩きの途中・起動の途中）
// raised   … 問いを出し、答えを待っている
// answered … 答えを受けた（はい・いいえ・問いを出せなかった）
// waiting と raised の間は本文の文字組みをしない（問いは最初の本文の文字組みより前）。
enum class DisplayInquiry
{
    unjudged,
    unneeded,
    waiting,
    raised,
    answered,
};

// 問いが未回答で、本文の文字組みを止めている段階か。
[[nodiscard]] constexpr bool display_pending(DisplayInquiry inquiry) noexcept
{
    switch (inquiry)
    {
    case DisplayInquiry::waiting:
    case DisplayInquiry::raised:
        return true;
    case DisplayInquiry::unjudged:
    case DisplayInquiry::unneeded:
    case DisplayInquiry::answered:
        return false;
    }
    std::unreachable();
}
} // namespace nenenib::application
