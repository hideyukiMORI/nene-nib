#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternMatch.hpp"
#include "VimPatternRange.hpp"
#include "VimSearchDirection.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
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
    // 現れたら offset として拒否する（offset は未対応）。正しい UTF-8 でない pattern は
    // invalid_utf8 で拒否する（#906）。
    [[nodiscard]] static std::expected<VimPattern, VimPatternFailure>
    parse(std::string_view pattern, VimSearchDirection separator);

    // line の from バイト以降で最も左の一致（ADR 0032 の決定 4 の追記）。長さ 0 の一致も返す。
    // line（matched_sequence も）は検証済みの UTF-8 が前提で、行ごとには確かめない（#906）。
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::string_view line,
                                                         std::size_t from) const;

    // 一致の長さの上限（バイト・#556）。食べる原子は 1 code point（UTF-8 で 4 バイトまで）を
    // 食べ、幅の無い原子は何も食べない。反復（`*`）を含めば決まらないので値が無い。
    [[nodiscard]] std::optional<std::size_t> longest_bytes() const noexcept;

    // 魔法の無い文字だけの列なら、その UTF-8 のバイト列（#583）。このパターンが行に当たるのは
    // この列が行に現れるときだけなので、検索は候補の行の絞り込みに使える（当たりの位置は照合器が
    // 決める）。空のパターンと、literal 以外の原子や `*` を含むパターンは値が無い。
    [[nodiscard]] std::optional<std::string> literal_bytes() const;

  private:
    friend class VimPatternEvaluation;

    VimPattern(std::vector<VimPatternAtom> atoms, std::vector<VimPatternRange> ranges);
    // 反復のない原子列を順に評価する。選択肢がないので状態列の確保も再帰も不要。
    [[nodiscard]] std::optional<std::size_t> matched_sequence(std::string_view line,
                                                              std::size_t at) const;
    [[nodiscard]] static bool is_zero_width(VimPatternAtomKind kind) noexcept;
    [[nodiscard]] static bool anchored(std::string_view line, const VimPatternAtom &atom,
                                       std::size_t at);
    [[nodiscard]] bool consumes(const VimPatternAtom &atom, char32_t code) const;
    // 1 文字を食べる原子。幅の無い原子は何も食べない。
    [[nodiscard]] std::optional<std::size_t>
    stepped(std::string_view line, const VimPatternAtom &atom, std::size_t at) const;
    [[nodiscard]] bool in_set(const VimPatternAtom &atom, char32_t code) const;

    std::vector<VimPatternAtom> atoms_;
    std::vector<VimPatternRange> ranges_;
};
} // namespace nenenib::core
