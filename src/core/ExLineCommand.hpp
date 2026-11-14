#pragma once

#include "ExFailure.hpp"
#include "ExGlobalRequest.hpp"
#include "ExLinesRequest.hpp"

#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 範囲の後ろの命令が `:d[elete]` か `:y[ank]` なら、その字面（ADR 0168 の決定 1・段 3a・Vim の
// do_one_cmd のレジスタと回数の読み）。ほかの命令は値なし。範囲は呼ぶ側が置く。字面の失敗は `!`
// （E477）と後ろの余り（E488）で、段 3a がまだ受けない形（`:dl` `:dp` の表示のフラグ・`|` の後ろの
// 命令・レジスタ `#`）は unsupported_argument。`"` から後ろは注釈。
[[nodiscard]] std::optional<std::expected<ExLinesRequest, ExFailure>>
lines_command(std::string_view text);

// 範囲の後ろの命令が `:g[lobal]` `:g!` `:v[global]` なら、その字面（ADR 0168 の決定 1・段 3a・Vim の
// ex_global の読み）。ほかの命令は値なし。字面の失敗はパターンが無い（E148）・区切りが英字
// （E146）・`:v!`（E477）と、当たった行に流す命令の字面の失敗。段 3a が受けない命令（範囲つき・
// `:d` と `:s` の族のほか・`:s` の回数と `n`）と、`\/` `\?` `\&` の区切りは unsupported_argument。
[[nodiscard]] std::optional<std::expected<ExGlobalRequest, ExFailure>>
global_command(std::string_view text);
} // namespace nenenib::core
