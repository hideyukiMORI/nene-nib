#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternCaptureFrame.hpp"
#include "VimPatternMatch.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class VimPattern;

// 一致 1 つの群の範囲を引く道の探索（ADR 0168 の決定 4・追補 13）。照合器（VimPatternEvaluation）と
// 同じ優先順（前の原子が先・`*` と `\=` は 1 文字ずつ食べる枝を 0 回の枝より先・最短はその逆）で、
// 一致の始まりから終わりまでの道を 1 本だけ探す。量は原子の写しに開いてあるので（ADR 0184 の決定
// 2）升の表はそのまま成り立つ。積みは明示の列（再帰しない・深さは一致の長さまで）、試して着かなかった
// （原子・位置）の升には印を付けて 2 度試さないので、仕事は升の数（原子の数 + 1）×（一致の長さ +
// 1） に比例する。升の数が上限を超える一致は探さない（呼ぶ側が置換全体を断る）。生成と操作は
// VimPattern::captured に閉じる（ADR 0093 と同じ形）。
class VimPatternCapture final
{
  private:
    friend class VimPattern;

    VimPatternCapture(const VimPattern &pattern, std::string_view line, VimPatternMatch match);
    // 道を探し、着いたら原子の境ごとの位置（positions_）を残して真。
    [[nodiscard]] bool walked();
    // 段の升の番号。
    [[nodiscard]] std::size_t cell(const VimPatternCaptureFrame &frame) const noexcept;
    // 段が終わりの段（原子を全部食べた）として一致の終わりに着いたか。
    [[nodiscard]] bool arrived(const VimPatternCaptureFrame &frame) const noexcept;
    // 積みの頂の段の次の子（試す枝が尽きたら値なし）。頂の phase を進める。
    [[nodiscard]] std::optional<VimPatternCaptureFrame> next_child(VimPatternCaptureFrame &top);
    // 食べる原子の段の phase の枝の子（量の 0 回の枝か、1 文字食べた後・着かなければ値なし）。
    [[nodiscard]] std::optional<VimPatternCaptureFrame>
    quantified_child(const VimPatternAtom &atom, const VimPatternCaptureFrame &top,
                     std::uint8_t phase) const;
    // 幅の無い原子の段の子（最初の枝で錨が立てば次の原子・ほかは値なし）。
    [[nodiscard]] std::optional<VimPatternCaptureFrame>
    zero_width_child(const VimPatternAtom &atom, const VimPatternCaptureFrame &top,
                     std::uint8_t phase) const;
    // atom が at の 1 文字を食べた後の位置（一致の終わりを越えれば値なし）。
    [[nodiscard]] std::optional<std::size_t> consumed(const VimPatternAtom &atom,
                                                      std::size_t at) const;
    // 積みの道から原子の境の位置を写す（同じ原子の段のうち最も下の段の位置）。
    void positions_recorded(const std::vector<VimPatternCaptureFrame> &path);

    const VimPattern &pattern_;
    std::string_view line_;
    VimPatternMatch match_;
    std::vector<std::size_t> positions_;
    std::vector<bool> failed_;
};
} // namespace nenenib::core
