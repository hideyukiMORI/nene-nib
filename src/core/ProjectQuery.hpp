#pragma once

#include "VimPattern.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternMatch.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// プロジェクトの全文検索の検索語（ADR 0167 の決定 3）。作る道は parse の 1 つ。
// - 既定は literal: 打った文字列のバイト列をそのまま探し、ASCII の英字だけ大小を区別しない
//   （全角やほかの文字は畳まない・ロケールを読まない・`ignorecase` の設定を読まない）。
// - 先頭が `/` なら残りを Vim の `/` の検索と同じ VimPattern（`magic` の部分集合・ADR 0032）に
//   する。大小は Vim の既定（`noignorecase`・区別する）で、`\c` で畳める。`/` は `\/` と書く。
// 空の literal と `/` だけの入力は「何も探さない」検索語（empty）で、どの行にも当たらない。
class ProjectQuery final
{
  public:
    // literal が正しい UTF-8 でなければ VimPatternFailure::invalid_utf8、pattern の失敗は
    // VimPattern::parse の失敗をそのまま返す。
    [[nodiscard]] static std::expected<ProjectQuery, VimPatternFailure>
    parse(std::string_view input);

    [[nodiscard]] bool empty() const noexcept;

    // 1 行の本文（改行を除いた正しい UTF-8）の最も左の当たり（バイト・行頭から）。
    [[nodiscard]] std::optional<VimPatternMatch> first_in_line(std::string_view line) const;

    // text（複数の行・正しい UTF-8 でなくてよい）の from バイト以降で、当たりを含みうる最初の位置
    // （バイト）。その位置の行だけを first_in_line で確かめればよい（絞り込みであって当たりでは
    // ない）。literal と、VimPattern::literal_bytes を持つ pattern はバイト列を探し、ほかの pattern
    // は from をそのまま返す（どの行も確かめる）。無ければ値なし。
    [[nodiscard]] std::optional<std::size_t> candidate(std::string_view text,
                                                       std::size_t from) const;

  private:
    ProjectQuery(std::string needle, bool folds, std::optional<VimPattern> pattern);

    [[nodiscard]] std::optional<std::size_t> found(std::string_view text, std::size_t from) const;

    // literal なら ASCII を小文字に畳んだ検索語、pattern なら絞り込みのバイト列（無ければ空）。
    std::string needle_;
    // needle_ に ASCII の英字があり、大小を畳んで比べるか（literal だけ）。
    bool folds_;
    // pattern の検索語（literal なら値なし）。
    std::optional<VimPattern> pattern_;
};
} // namespace nenenib::core
