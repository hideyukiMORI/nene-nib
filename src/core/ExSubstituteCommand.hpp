#pragma once

#include "ExFailure.hpp"
#include "ExSubstituteRequest.hpp"

#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 範囲の後ろの命令が `:s` の族（`:s` `:su[bstitute]` `:sg` `:si` … `:&` `:&&` `:~`）なら、その字面
// （ADR 0168 の決定 1〜3・Vim の find_ex_command と ex_substitute の読み）。ほかの命令は値なし。
// 範囲は呼ぶ側が置く。字面の失敗は区切りが英字（E146）・回数が 0（E939）・後ろに余り（E488）で、
// 段 1 がまだ受けない形（`c` `p` `#` `l` のフラグ・`\=` の式・`\/` `\?` `\&` の区切り・ASCII の外の
// 区切り）は unsupported_argument。
[[nodiscard]] std::optional<std::expected<ExSubstituteRequest, ExFailure>>
substitute_command(std::string_view text);
} // namespace nenenib::core
