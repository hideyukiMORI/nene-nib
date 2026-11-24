#pragma once

#include "FilePath.hpp"
#include "ServerLaunch.hpp"

#include <optional>
#include <string_view>

namespace nenenib::application
{
// 設定の `lsp.<language>.command` の 1 行を起こす要求にする唯一の写し（ADR 0160 の決定 7）。
// 空白（SP・HT）で区切り、`"` で囲んだ間の空白は区切らない（`"` 自体は語に含めない・逆斜線の
// 逃がしは無い）。最初の語が実行ファイル、残りが引数。語が 1 つも無い・閉じない `"`・実行
// ファイルが経路にならなければ値なし。絶対パスかどうかは adapter が見る（追補）。OS に触れない。
[[nodiscard]] std::optional<ServerLaunch> server_launch_of(std::string_view command,
                                                           const core::FilePath &root);
// 言語サーバーの席の鍵にする命令の行（ADR 0160 / 0161 の 2026-11-24 の追補・#1952）。前後の空白
// （SP・HT）を落とすだけで、大小・引用符・間の空白の違いは別の命令のまま（賢く畳まない）。
[[nodiscard]] std::string_view server_command_key(std::string_view command) noexcept;
} // namespace nenenib::application
