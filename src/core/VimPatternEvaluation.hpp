#pragma once

#include "VimPatternMatch.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class VimPattern;

// 一回の照合の優先順と訪問済み位置。生成と操作は VimPattern::matched に閉じる（ADR 0093）。
class VimPatternEvaluation final
{
  private:
    friend class VimPattern;

    VimPatternEvaluation(const VimPattern &pattern, std::string_view line);
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::size_t from);
    [[nodiscard]] std::optional<VimPatternMatch>
    expanded(std::vector<std::size_t> &order, std::vector<std::optional<std::size_t>> &begins,
             std::size_t index, std::size_t begin);
    // 食べ終えた候補を target へ進める。1 文字の途中なら途中の印のまま次の列に残す（#1146）。
    [[nodiscard]] std::optional<VimPatternMatch> arrived(std::size_t target, std::size_t begin);
    [[nodiscard]] std::optional<VimPatternMatch> stepped(std::size_t index, std::size_t begin,
                                                         char32_t code);
    void advanced();

    const VimPattern &pattern_;
    std::string_view line_;
    std::size_t at_;
    // 列の番号のうち、これ以上は「原子 (番号 - skipping_) へ向かって結合文字を食べている途中」。
    std::size_t skipping_;
    std::vector<std::size_t> current_;
    std::vector<std::size_t> next_;
    // 値がある原子はその位置で訪問済み。最初の開始byteだけを残す。
    std::vector<std::optional<std::size_t>> current_begins_;
    std::vector<std::optional<std::size_t>> next_begins_;
    std::optional<VimPatternMatch> found_;
};
} // namespace nenenib::core
