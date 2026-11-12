#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternMatch.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class VimPattern;

// 一致 1 つの群の範囲を引く道の探索（ADR 0168 の決定 4）。照合器（VimPatternEvaluation）と同じ
// 優先順（前の原子が先・`*` は長い方から）で、一致の始まりから終わりまでの道を 1 本だけ探し、原子の
// 境ごとに通った位置を positions_ に残す。試して着かなかった（原子・位置）は failed_ に印を付けて
// 2 度試さない（後方参照が無いので同じ所からの答えは同じ）。生成と操作は VimPattern::captured に
// 閉じる（ADR 0093 と同じ形）。
class VimPatternCapture final
{
  private:
    friend class VimPattern;

    VimPatternCapture(const VimPattern &pattern, std::string_view line, VimPatternMatch match);
    // index の原子から at で始めて一致の終わりに着くか。
    [[nodiscard]] bool walked(std::size_t index, std::size_t at);
    // index の原子を at で食べてから先へ進めるか。
    [[nodiscard]] bool stepped_on(std::size_t index, std::size_t at);
    // atom が at から食べて着ける位置（at 自身を含む・一致の終わりまで・`*` でなければ 1 歩まで）。
    [[nodiscard]] std::vector<std::size_t> reached(const VimPatternAtom &atom,
                                                   std::size_t at) const;

    const VimPattern &pattern_;
    std::string_view line_;
    VimPatternMatch match_;
    std::vector<std::size_t> positions_;
    std::vector<bool> failed_;
};
} // namespace nenenib::core
