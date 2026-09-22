#pragma once

#include <cstdint>

namespace nenenib::core
{
// 受け付けない検索パターンの閉じた一覧（ADR 0032 の決定 4）。未対応の構文を黙って別の意味に
// せず、この値で拒否してメッセージを出す。Vim には無い失敗なので文言も本実装のものである。
enum class VimPatternFailure : std::uint8_t
{
    // `\(` `\)`
    group,
    // `\|`
    branch,
    // `\{` `\}` `\+` `\=` `\?`
    quantifier,
    // `\v` `\V` `\m` `\M`
    magic,
    // `\c` `\C`
    ignore_case,
    // そのほかの未対応の `\`（`\%` `\_` `\@` `\&` `\z` と `\n` `\t` などの英数字、末尾の単独の
    // `\`）
    escape,
    // `~`（直前の置換の文字列）
    previous_substitute,
    // 区切りの後ろの offset（`/foo/e`）
    offset
};
} // namespace nenenib::core
