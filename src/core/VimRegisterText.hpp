#pragma once

#include "VimKey.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 鍵列 ↔ レジスタの本文の写し（ADR 0048 の決定 7）。この 1 対だけが写す（ARC-001）。
// 録画を止める `q` は鍵列を本文にし、`@a` は本文を鍵列にして再生する。

// 鍵列 → 本文。文字はその UTF-8、特殊鍵は Vim が typeahead に置くバイト（`<80>` は U+0080 の
// UTF-8 `C2 80`・本文は正しい UTF-8 のまま）、確定した検索は `/` か `?` ＋ pattern ＋ `\r`。
[[nodiscard]] std::string vim_register_text(std::span<const VimKey> keys);

// 本文 → 鍵列。vim_register_text の逆に加えて `\x08` も backspace（Vim の Ctrl-H）。U+0080 の
// 後ろの 2 文字が特殊鍵の表に無ければ U+0080 も文字で、`\n` は VimCharacter{U'\n'}。確定した
// 検索は組み立てない（本文の `/` は文字のまま・決定 8）。text は正しい UTF-8 でなければならない
// （レジスタの本文は TextBuffer か vim_register_text 由来なので常に満たす）。
// 検索を含まない鍵列では vim_keys_of_text(vim_register_text(keys)) == keys（契約）。
[[nodiscard]] std::vector<VimKey> vim_keys_of_text(std::string_view text);
} // namespace nenenib::core
