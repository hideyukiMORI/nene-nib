#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// LSP のスニペット形式（`insertTextFormat=2`）の本文 → 印を外した平文（ADR 0190 の決定 4・跳び先は
// 限界）。`$0` `$1` `${1}` と変数 `$NAME` `${NAME}` は消し、`${1:x}` `${NAME:x}` は中身 x を残す
// （入れ子も同じ）。選択 `${1|a,b|}` は最初の a を残す。`\$` `\}` `\\` は後ろの 1 文字にする。
// タブストップは `$` の後ろの数字だけ（`$1abc` は `abc`）、変数は英字か `_` で始まる英数字と `_`。
// 閉じていない印・名前の無い `$` はそのまま残す。
[[nodiscard]] std::string lsp_snippet_text(std::string_view snippet);
} // namespace nenenib::core
