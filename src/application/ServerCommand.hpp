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
} // namespace nenenib::application
