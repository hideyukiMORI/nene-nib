#pragma once

#include "VimPatternMatch.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
class VimPattern;

// 一回の照合の優先順と訪問済み位置。生成と操作は VimPattern::matched に閉じる（ADR 0093）。
class VimPatternEvaluation final
{
  private:
    friend class VimPattern;

    // 番号ごとの（訪れた位置, 開始 byte）。訪れた位置が今の位置 at_ と同じ番号だけが、その位置で
    // 訪問済み（#1584）。
    using Visits = std::vector<std::pair<std::size_t, std::size_t>>;
    using Expansion = std::optional<VimPatternMatch> (VimPatternEvaluation::*)(
        std::vector<std::size_t> &, Visits &, std::size_t, std::size_t);

    VimPatternEvaluation(const VimPattern &pattern, std::string_view line);
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::size_t from);
    [[nodiscard]] std::optional<VimPatternMatch>
    expanded(std::vector<std::size_t> &order, Visits &begins, std::size_t index, std::size_t begin);
    // 最短の量（ADR 0184 の決定 2）を含む列の展開。0 回の枝を先に展開し、消費を後に置く。
    [[nodiscard]] std::optional<VimPatternMatch> lazily_expanded(std::vector<std::size_t> &order,
                                                                 Visits &begins, std::size_t index,
                                                                 std::size_t begin);
    // 受理の状態に着いた候補（1 文字の境なら一致・ADR 0184 の決定 2 の後に置いた消費も扱う）。
    [[nodiscard]] std::optional<VimPatternMatch> finished(std::vector<std::size_t> &order,
                                                          std::size_t begin);
    // 原子 index を展開し、幅の無い遷移で続く原子の番号（続かなければ値なし）。
    [[nodiscard]] std::optional<std::size_t> passed(std::vector<std::size_t> &order,
                                                    std::size_t index);
    void deferred_placed(std::vector<std::size_t> &order);
    // 食べ終えた候補を target へ進める。1 文字の途中なら途中の印のまま次の列に残す（#1146）。
    [[nodiscard]] std::optional<VimPatternMatch> arrived(std::size_t target, std::size_t begin);
    // before は食べる code point の位置（結合文字だけの原子は同じ 1 文字の続きを読む・#1506）。
    [[nodiscard]] std::optional<VimPatternMatch> stepped(std::size_t index, std::size_t begin,
                                                         std::size_t before, char32_t code);
    void advanced();

    const VimPattern &pattern_;
    std::string_view line_;
    std::size_t at_;
    // 列の番号のうち、これ以上は「原子 (番号 - skipping_) へ向かって結合文字を食べている途中」。
    std::size_t skipping_;
    std::vector<std::size_t> current_;
    std::vector<std::size_t> next_;
    // 印の位置が at_ と同じ原子は、その位置で訪問済み。最初の開始byteだけを残す。at_ は 1 文字
    // ごとに必ず進むので、前の文字の印は位置が古くなるだけで、列を埋め直さない（#1584・1 文字の
    // 費用は候補の数に比例）。どの位置にもならない印（npos）で始める。
    Visits current_begins_;
    Visits next_begins_;
    // 展開の関数（最短の量を含む列だけ lazily_expanded・構築で 1 回だけ選ぶ）。
    Expansion expand_;
    // 展開の途中で後に置く最短の原子の消費（ADR 0184 の決定 2・最短の無いパターンでは確保しない）。
    std::vector<std::size_t> deferred_;
    std::optional<VimPatternMatch> found_;
};
} // namespace nenenib::core
