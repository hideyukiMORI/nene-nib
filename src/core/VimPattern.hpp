#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternCase.hpp"
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
    // invalid_utf8 で拒否する（#906）。letter_case は大小の旗（#1041）で、パターンの中の `\c`
    // （区別しない）と `\C`（区別する）が旗より強く、両方あれば `\c` が勝つ（Vim 9.1 の実測）。
    // smart の「大文字がある」は `\` に続く文字を数えない（`\S` `\C` は数えない・Vim の
    // pat_has_uppercase）。
    [[nodiscard]] static std::expected<VimPattern, VimPatternFailure>
    parse(std::string_view pattern, VimSearchDirection separator, VimPatternCase letter_case);

    // line の from バイト以降で最も左の一致（ADR 0032 の決定 4 の追記）。長さ 0 の一致も返す。
    // line（matched_sequence も）は検証済みの UTF-8 が前提で、行ごとには確かめない（#906）。
    [[nodiscard]] std::optional<VimPatternMatch> matched(std::string_view line,
                                                         std::size_t from) const;

    // 一致の長さの上限（バイト・#556）。食べる原子は 1 code point（UTF-8 で 4 バイトまで）を
    // 食べ、幅の無い原子は何も食べない。反復（`*`）を含めば決まらないので値が無い。
    [[nodiscard]] std::optional<std::size_t> longest_bytes() const noexcept;

    // 魔法の無い文字だけの列なら、その UTF-8 のバイト列（#583）。このパターンが行に当たるのは
    // この列が行に現れるときだけなので、検索は候補の行の絞り込みに使える（当たりの位置は照合器が
    // 決める）。空のパターンと、literal 以外の原子や `*` を含むパターンは値が無い。大小を畳む
    // パターンは、英字か ASCII の外の文字を含めば値が無い（バイト列の一致で絞れない・#1041）。
    [[nodiscard]] std::optional<std::string> literal_bytes() const;

  private:
    friend class VimPatternEvaluation;

    VimPattern(std::vector<VimPatternAtom> atoms, std::vector<VimPatternRange> ranges,
               bool ignore_case);
    // 反復のない原子列を順に評価する。選択肢がないので状態列の確保も再帰も不要。
    [[nodiscard]] std::optional<std::size_t> matched_sequence(std::string_view line,
                                                              std::size_t at) const;
    [[nodiscard]] static bool is_zero_width(VimPatternAtomKind kind) noexcept;
    [[nodiscard]] static bool anchored(std::string_view line, const VimPatternAtom &atom,
                                       std::size_t at);
    // 位置が Vim の 1 文字の境か（本文の側にだけ結合文字が続けば偽・#1138）。一致の終わりと
    // 始まり（#1146）と `\<` `\>` はここでだけ真になる。
    [[nodiscard]] static bool ends_a_character(std::string_view line, std::size_t end) noexcept;
    // at の code point を食べた後の位置（`.` と集合は 1 文字の残りの結合文字まで・#1146）。
    [[nodiscard]] static std::size_t eaten(std::string_view line, VimPatternAtomKind kind,
                                           std::size_t at);
    [[nodiscard]] static bool whole_character(VimPatternAtomKind kind) noexcept;
    [[nodiscard]] bool consumes(const VimPatternAtom &atom, char32_t code) const;
    // 1 文字を食べる原子。幅の無い原子は何も食べない。
    [[nodiscard]] std::optional<std::size_t>
    stepped(std::string_view line, const VimPatternAtom &atom, std::size_t at) const;
    [[nodiscard]] bool in_set(const VimPatternAtom &atom, char32_t code) const;
    [[nodiscard]] bool in_ranges(const VimPatternAtom &atom, char32_t code) const;

    std::vector<VimPatternAtom> atoms_;
    std::vector<VimPatternRange> ranges_;
    // 大小を畳んで比べるか（parse が旗と `\c` `\C` から 1 回だけ決める）。
    bool ignore_case_;
};
} // namespace nenenib::core
