#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternMatch.hpp"
#include "VimPatternRange.hpp"
#include "VimSearchDirection.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// Vim の `magic` の部分集合の照合器（ADR 0032 の決定 4）。`std::regex` と `<locale>` は
// 使わない（ロケール依存のシンボルが core の外へ出る）。一致は 1 行の中だけで、行をまたがない。
// 生成経路は parse ただ 1 つで、未対応の構文は閉じた失敗で拒否する（CPP-005 / CPP-007）。
class VimPattern final
{
  public:
    // separator は打たれた向き。前向きの `/` と後ろ向きの `?` の区切りがパターンの中に
    // 現れたら offset として拒否する（offset は未対応）。
    [[nodiscard]] static std::expected<VimPattern, VimPatternFailure>
    parse(std::string_view pattern, VimSearchDirection separator);

    // line の from バイト以降で最も左の一致（ADR 0032 の決定 4 の追記）。長さ 0 の一致も返す。
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::string_view line,
                                                         std::size_t from) const;

  private:
    VimPattern(std::vector<VimPatternAtom> atoms, std::vector<VimPatternRange> ranges);
    // index 番目の原子から at で照合して、一致した末尾を返す。`*` は貪欲に伸ばして後戻りする。
    [[nodiscard]] std::optional<std::size_t> matched_atoms(std::string_view line, std::size_t index,
                                                           std::size_t at) const;
    // 1 文字を食べる原子。幅の無い原子は何も食べない。
    [[nodiscard]] std::optional<std::size_t>
    stepped(std::string_view line, const VimPatternAtom &atom, std::size_t at) const;
    // `*` が止まれる位置の列（0 回から最長まで）。
    [[nodiscard]] std::vector<std::size_t>
    repeated_stops(std::string_view line, const VimPatternAtom &atom, std::size_t at) const;
    [[nodiscard]] bool in_set(const VimPatternAtom &atom, char32_t code) const;

    std::vector<VimPatternAtom> atoms_;
    std::vector<VimPatternRange> ranges_;
};
} // namespace nenenib::core
