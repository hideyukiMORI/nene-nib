#pragma once

#include "LspRequestContext.hpp"
#include "LspSignature.hpp"

#include <optional>

namespace nenenib::application
{
// 文脈の照合を通った引数の情報の答え（ADR 0191 の決定 6）。context は要求の文脈、signature は
// core::lsp_signature_help の値（値なし・誤り・読めない答えは値なし＝札を消す）。公開 aggregate
// （CPP-003）。
struct LanguageSignature
{
    LspRequestContext context;
    std::optional<core::LspSignature> signature;
};
} // namespace nenenib::application
