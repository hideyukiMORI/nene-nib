#pragma once

#include "VimPattern.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternMatch.hpp"
#include "VimPatternNeedle.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// プロジェクトの全文検索の検索語（ADR 0167 の決定 3 と追補）。作る道は parse の 1 つで、照合は
// どちらの形も VimPattern の 1 本（ARC-012・LiteralPattern.hpp）。
// - 既定は literal: 通常モードの Ctrl+F と同じ literal_pattern で逃がした文字列を、大小を畳む
//   VimPattern（VimPatternCase::ignore・畳み方は vim_folded）にする。当たりは `:set ignorecase`
//   のときの Ctrl+F と同じ。`ignorecase` の設定は読まない（ADR 0167 は「大小を区別しない」）。
// - 先頭が `/` なら残りを Vim の `/` の検索と同じ VimPattern（`magic` の部分集合・ADR 0032）に
//   する。大小は Vim の既定（`noignorecase`・区別する）で、`\c` で畳める。`/` は `\/` と書く。
// 空の literal と `/` だけの入力は「何も探さない」検索語（empty）で、どの行にも当たらない。
class ProjectQuery final
{
  public:
    // 失敗は VimPattern::parse の失敗をそのまま返す（正しい UTF-8 でない入力は invalid_utf8）。
    [[nodiscard]] static std::expected<ProjectQuery, VimPatternFailure>
    parse(std::string_view input);

    [[nodiscard]] bool empty() const noexcept;

    // 1 行の本文（改行を除いた正しい UTF-8）の最も左の当たり（バイト・行頭から）。
    [[nodiscard]] std::optional<VimPatternMatch> first_in_line(std::string_view line) const;

    // text（複数の行・正しい UTF-8 でなくてよい）の from バイト以降で、当たりを含みうる最初の位置
    // （バイト）。その位置の行だけを first_in_line で確かめればよい（絞り込みであって当たりでは
    // ない）。VimPattern::required_bytes を持つ検索語はそのバイト列を探し、持たない検索語は from を
    // そのまま返す（どの行も確かめる）。無ければ値なし。
    [[nodiscard]] std::optional<std::size_t> candidate(std::string_view text,
                                                       std::size_t from) const;

  private:
    explicit ProjectQuery(std::optional<VimPattern> pattern);

    // 照合器（empty なら値なし）。
    std::optional<VimPattern> pattern_;
    // 絞り込みのバイト列（pattern_ の required_bytes・無ければ値なし）。
    std::optional<VimPatternNeedle> needle_;
};
} // namespace nenenib::core
