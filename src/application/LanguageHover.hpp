#pragma once

#include "LspRequestContext.hpp"

#include <optional>
#include <string>

namespace nenenib::application
{
// 文脈の照合を通った hover の答え（ADR 0190 の決定 1・6）。context は要求の文脈、text は
// core::lsp_hover_text の平文（空の答え・誤りの答えは値なし＝札を出さずに 1 行知らせる）。
// 公開 aggregate（CPP-003）。
struct LanguageHover
{
    LspRequestContext context;
    std::optional<std::string> text;
};
} // namespace nenenib::application
