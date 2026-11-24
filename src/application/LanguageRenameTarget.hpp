#pragma once

#include "LspAnswerFailure.hpp"
#include "LspPositionEncoding.hpp"
#include "LspRenameTarget.hpp"
#include "LspRequestContext.hpp"

#include <expected>
#include <optional>
#include <string>

namespace nenenib::application
{
// 変えられる所の問い（prepareRename）の答え（#2004・ADR 0194 の決定 7）。context は頼んだときの
// 文脈（使うのは文書・改訂番号・キャレットが同じで何も待っていないときだけ）、target は
// core::lsp_rename_target の値（誤りの答えも invalid_result）、server_error は誤りの答えの
// message（誤りでなければ空）、encoding は答えた席の位置の単位。公開 aggregate（CPP-003）。
struct LanguageRenameTarget
{
    LspRequestContext context;
    std::expected<std::optional<core::LspRenameTarget>, core::LspAnswerFailure> target;
    std::string server_error;
    core::LspPositionEncoding encoding;
};
} // namespace nenenib::application
