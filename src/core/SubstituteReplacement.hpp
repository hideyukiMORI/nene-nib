#pragma once

#include "VimPatternCaptures.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 置換の置き換えの本文（ADR 0168 の決定 3・ADR 0174 の決定 7）。literal は書いたまま（通常モードの
// 置換）、vim は Vim の `{rep}`（magic・`~` は前もって vim_tilde_expanded で解いた後）で、当たり
// ごとに Vim の vim_regsub と do_sub の後の歩きと同じに広げる: `&` `\0` は当たり、`\1`〜`\9` は群、
// `\r` と CR の文字は改行（'\n'・文書の改行へ直すのは controller）、`\n` は NUL、`\t` は Tab、
// `\b` は BS、`\<CR>` は CR の文字、ほかの `\x` は x（`\\` `\&` `\/` `\~`）、末尾の `\` は `\`、
// `\u` `\l` は次の 1 文字、`\U` `\L` は `\e` `\E` まで大小を変える（VimCase の写像）。
class SubstituteReplacement final
{
  public:
    [[nodiscard]] static SubstituteReplacement literal(std::string_view text);
    [[nodiscard]] static SubstituteReplacement vim(std::string_view text);

    // line の当たり（captures の 0）を置き換える本文を out の後ろに足す。
    void appended(std::string &out, std::string_view line,
                  const VimPatternCaptures &captures) const;
    // appended が足すバイト数（本文を作らずに数える・上限の判定）。
    [[nodiscard]] std::size_t expanded_bytes(std::string_view line,
                                             const VimPatternCaptures &captures) const;
    // `\1`〜`\9` を読むか（パターンの群の範囲を引く要がある）。
    [[nodiscard]] bool reads_subgroups() const noexcept;

  private:
    SubstituteReplacement(std::string text, bool literal);
    template <typename Sink>
    void expanded(std::string_view line, const VimPatternCaptures &captures, Sink &sink) const;

    std::string text_;
    bool literal_;
};

// `{rep}` の `~` を前の置き換え previous（広げた後の値）で置き換えた本文（Vim の regtilde・
// magic）。`\~` は `\~` のまま残し（広げるときに `~` になる）、ほかの `\x` も飛ばす。呼ぶ側は答えを
// 次の previous として覚える。
[[nodiscard]] std::string vim_tilde_expanded(std::string_view replacement,
                                             std::string_view previous);
} // namespace nenenib::core
