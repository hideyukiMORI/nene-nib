#pragma once

#include "LspEditContext.hpp"
#include "LspPositionEncoding.hpp"
#include "LspWorkspaceEdit.hpp"
#include "LspWorkspaceEditFailure.hpp"

#include <expected>
#include <string>

namespace nenenib::application
{
// 名前の変更（rename）の答え（#2004・ADR 0194 の決定 7）。context は頼んだときの文脈と席の文書の
// 改訂番号の写し（照合は当てるときにする）、edit は core::lsp_workspace_edit の値（誤りの答えは
// malformed）、server_error は誤りの答えの message（誤りでなければ空）、encoding は答えた席の位置の
// 単位。公開 aggregate（CPP-003）。
struct LanguageRename
{
    LspEditContext context;
    std::expected<core::LspWorkspaceEdit, core::LspWorkspaceEditFailure> edit;
    std::string server_error;
    core::LspPositionEncoding encoding;
};
} // namespace nenenib::application
