#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 言語サーバーの記録の 1 行の上限（バイト・ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。これを超える
// 行は code point の境目で切り、後ろに「…」を付ける。
inline constexpr std::size_t server_log_line_bytes = 2048;

// サーバーが書いた 1 行（改行を含まない・標準エラーの 1 行か `window/logMessage` の文の 1 行）を
// 記録に置ける形にする唯一の 1 本（追補）。ANSI のエスケープ列（CSI の ESC [ … 終端・OSC の
// ESC ] … BEL か ESC \・ESC と 1 文字）は落とし、UTF-8 として正しくない列は 1 バイトずつ U+FFFD に
// 置き換え（正しい列は残す）、Tab のほかの C0 の制御文字（U+0000〜U+001F）と DEL は制御文字の
// 図形（U+2400〜U+241F・U+2421）に、C1（U+0080〜U+009F）と双方向の制御（U+202A〜U+202E・
// U+2066〜U+2069）は「<U+XXXX>」に置き換える（記録のタブで行の見た目が入れ替わらない・#1950 の
// レビュー L3）。server_log_line_bytes を超えれば code point の境目で切って「…」を付ける。答えは
// 必ず正しい UTF-8 で、改行を含まない。OS に触れない純関数。
[[nodiscard]] std::string server_log_line(std::string_view raw);
} // namespace nenenib::core
