#pragma once

#include "KnownLanguageServer.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace nenenib::core
{
// よく知られた言語サーバーの表は core のこの 1 か所（LSP 段 S4・#1945・ADR 0160 の決定 7 の追補）。
// 設定の「言語サーバー」の節はこの表の順に 1 行ずつ並べ、PC で見つかった候補を「使う」で
// `lsp.<name>.command` の 1 行に書く。表は実在を確かめた名前だけで、確かめられない物は入れない。
// 設定に書いたパスだけを起こす（決定 7）ので、表は書く前の候補で、起こす道は読まない。

// 畳まずに出す先頭の行の数（よく使う言語・残りは「ほかの N 言語を見る」で畳む）。
inline constexpr std::size_t known_language_servers_leading = 5;

// 表そのもの（言語の行の順）。1 つの言語は高々 1 行にしか現れない（cpp の static_assert）。
[[nodiscard]] std::span<const KnownLanguageServer> known_language_servers() noexcept;

// npm の包みの `package.json` の本文 manifest から、`bin` の名前 bin のスクリプトの相対パス（包みの
// フォルダから・区切りは `\`）。`bin` が文字列なら包みの名前（`@scope/` を除いた所）が bin のとき
// だけ、object なら鍵 bin の値。相対パスは空でなく、絶対パス・ドライブ（`:`）・`..` の段・`"` と
// 制御文字を含まないときだけ受ける（包みの外を指さない）。`./` の段は落とす。読めなければ値なし。
[[nodiscard]] std::optional<std::string>
npm_bin_script(std::string_view manifest, std::string_view package, std::string_view bin);

// 設定の `lsp.<name>.command` に書く 1 行（application の server_launch_of が読み戻す形）。
// executable と script（空なら無し）は空白を含めば `"` で囲み、arguments（表の引数・空なら無し）
// を後ろに続ける。どれかが `"` か改行を含む・executable が空なら値なし。
[[nodiscard]] std::optional<std::string> language_server_command_text(std::string_view executable,
                                                                      std::string_view script,
                                                                      std::string_view arguments);
} // namespace nenenib::core
