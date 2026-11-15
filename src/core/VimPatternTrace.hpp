#pragma once

#include "VimPatternCaptures.hpp"
#include "VimPatternMatch.hpp"
#include "VimPatternTraceFrame.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class VimPattern;

// 命令の列の道（ADR 0184 の決定 3・4）で、一致 1 つの群の範囲を後から引く探索。照合器と同じ優先順
// （split は先の道から）で、一致の始まりから終わりまでの道を 1 本だけ探す。升は（命令・位置）で、
// 食べる原子と錨と群の開きの升は入ったときに印を付け（2 度目は試さない）、ほかの升（split・jump・
// 群の閉じ・当たり）は着かなかったときに印を付ける。これは Vim 9.1 の addstate が MOPEN と文字の
// 状態だけを「同じ位置で 2 度目は足さない」とし、SPLIT と MCLOSE は辿り直す規則と同じで、空の周を
// 1 回だけ通す（`\(a*\)\+` は `aaa` で `\1` が空・Vim の実測）。群の値は道の上で最後に通った開きと
// 閉じ（最後の周・最後の周で通らなかった内側の群は前の周の値のまま・Vim の部分式の写しと同じ）。
// 積みは明示の列（再帰しない）で、仕事は升の数に比例する。生成と操作は VimPattern::captured に
// 閉じる。道の端は一致のパターン全体の範囲（whole_begin と whole_end・置き直す前）で、`\zs` は
// 群の開きと同じく入ったときに、`\ze` は群の閉じと同じく着かなかったときに印を付けて素通りする
// （ADR 0184 の決定 5・群の値は動かない）。
class VimPatternTrace final
{
  private:
    friend class VimPattern;

    VimPatternTrace(const VimPattern &pattern, std::string_view line, VimPatternMatch match);
    // 道を探し、着いたら群の範囲を captures に残して真。
    [[nodiscard]] bool walked(VimPatternCaptures &captures);
    [[nodiscard]] std::size_t cell(const VimPatternTraceFrame &frame) const noexcept;
    // 入ったときに印を付ける升の命令か（食べる原子・錨・群の開き）。
    [[nodiscard]] bool marked_on_entry(std::size_t pc) const;
    // 頂の段の次の子（試す枝が尽きたら値なし）。頂の phase を進める。
    [[nodiscard]] std::optional<VimPatternTraceFrame> next_child(VimPatternTraceFrame &top) const;
    [[nodiscard]] std::optional<VimPatternTraceFrame>
    first_child(const VimPatternTraceFrame &top) const;
    // 入ったばかりの段を試さないか（升が印つき）。入ったときに印を付ける升はここで付ける。
    [[nodiscard]] bool cut(const VimPatternTraceFrame &top);
    // 段が当たりの命令として一致の終わりに着いたか。
    [[nodiscard]] bool accepted(const VimPatternTraceFrame &top) const;
    [[nodiscard]] std::optional<VimPatternTraceFrame> atom_child(const VimPatternTraceFrame &top,
                                                                 std::size_t atom) const;
    // 積みの道の群の端から群の範囲を写す。
    void captures_recorded(VimPatternCaptures &captures) const;

    const VimPattern &pattern_;
    std::string_view line_;
    VimPatternMatch match_;
    std::vector<bool> marked_;
    std::vector<VimPatternTraceFrame> path_;
};
} // namespace nenenib::core
