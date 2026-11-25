#pragma once

#include "LspAnswerFailure.hpp"
#include "LspCodeAction.hpp"
#include "LspEditContext.hpp"
#include "LspPositionEncoding.hpp"

#include <expected>
#include <string>

namespace nenenib::application
{
// `codeAction/resolve` の答え（#2005・ADR 0194 の決定 10）。context
// は元のクイックフィックスを頼んだ ときの文脈と写し（照合は当てるときにする）、action は
// core::lsp_code_action の値（誤りの答えは invalid_result）、server_error は誤りの答えの
// message、encoding は答えた席の位置の単位。公開 aggregate（CPP-003）。
struct LanguageCodeActionResolve
{
    LspEditContext context;
    std::expected<core::LspCodeAction, core::LspAnswerFailure> action;
    std::string server_error;
    core::LspPositionEncoding encoding;
};
} // namespace nenenib::application
