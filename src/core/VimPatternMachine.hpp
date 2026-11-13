#pragma once

#include "VimPatternMatch.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class VimPattern;

// 命令の列の道（ADR 0184 の決定 3）の 1 回の照合。
// 優先順つきの状態集合（Pike VM）で 1 code point ずつ進む。
// 列は食べる原子の命令の番号を優先順に持ち、
// 幅の無い命令（split・jump・群の端・錨）は置くときに辿る。
// 同じ位置で同じ食べる命令・錨・群の開きに 2 度目に来た候補は捨てる
// （後戻りしない・Vim 9.1 の addstate と同じ規則・entered）。
// 始まりは当たりが見つかるまで位置ごとに最も低い優先で足し（最も左）、
// 当たった候補より低い優先の候補は捨てる
// （最初に当たる枝・Vim 9.1 の nfa_regmatch の NFA_MATCH）。
// 確保は作るときだけ。生成と操作は VimPattern::matched に閉じる
// （ADR 0093 と同じ形）。
class VimPatternMachine final
{
  private:
    friend class VimPattern;

    VimPatternMachine(const VimPattern &pattern, std::string_view line);
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::size_t from);
    // pc から幅の無い遷移を辿り、食べる命令を次の列（next_）か今の列（current_）へ置く。当たりに
    // 着けばそれを返す（残りの低い優先は捨てる）。
    [[nodiscard]] std::optional<VimPatternMatch> placed(bool into_next, std::size_t pc,
                                                        std::size_t begin);
    // 命令 here を辿るか（辿るなら印を付けて真・辿り直す命令は辿り終えの印を積む）。
    [[nodiscard]] bool entered(bool into_next, std::size_t here);
    [[nodiscard]] std::size_t &stamp_of(bool into_next, std::size_t slot);
    // 列の番号の「辿り終えた」印の値（今の世代の 2 倍 + 1・途中は 2 倍）。
    [[nodiscard]] std::size_t done_stamp(bool into_next) const noexcept;
    // 命令 here を 1 つ辿る（幅の無い遷移は pending_ に積み、食べる原子は列に置く）。当たりなら値。
    [[nodiscard]] std::optional<VimPatternMatch> followed(bool into_next, std::size_t here,
                                                          std::size_t begin);
    void atom_placed(bool into_next, std::size_t here, std::size_t begin);
    // 食べ終えた候補を target へ進める。1 文字の途中なら途中の印のまま次の列に残す（#1146）。
    [[nodiscard]] std::optional<VimPatternMatch> arrived(std::size_t target, std::size_t begin);
    [[nodiscard]] std::optional<VimPatternMatch> stepped(std::size_t slot, std::size_t begin,
                                                         std::size_t before, char32_t code);
    void advanced();

    const VimPattern &pattern_;
    std::string_view line_;
    std::size_t at_;
    // 列の番号のうち、これ以上は「命令 (番号 - transit_) へ向かって結合文字を食べている途中」。
    std::size_t transit_;
    std::vector<std::size_t> current_;
    std::vector<std::size_t> next_;
    // 列の番号ごとの始まりの位置（印 stamps が今の世代のときだけ値がある）。
    std::vector<std::size_t> current_begins_;
    std::vector<std::size_t> next_begins_;
    // 列の番号ごとの、最後に辿った列の世代の印（done_stamp なら辿り終えた・1 小さければ途中）。
    std::vector<std::size_t> current_stamps_;
    std::vector<std::size_t> next_stamps_;
    std::size_t current_generation_ = 1;
    std::size_t next_generation_ = 2;
    // 幅の無い遷移を辿る積み（命令の番号の 2
    // 倍・奇数は辿り終えの印・再帰しない・作るときに確保する）。
    std::vector<std::size_t> pending_;
    std::optional<VimPatternMatch> found_;
};
} // namespace nenenib::core
