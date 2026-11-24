#pragma once

#include "LspPositionEncoding.hpp"
#include "LspRequestContext.hpp"
#include "LspTextEdit.hpp"

#include <optional>
#include <vector>

namespace nenenib::application
{
// 整形の答え（#1993・ADR 0191 の決定 7）。context は要求を出したときの文脈（当てる前に文書と
// revision を今と比べる）、edits は core::lsp_text_edits の値（誤りの答えと読めない答えは値なし）、
// encoding は答えた席の位置の単位。公開 aggregate（CPP-003）。
struct LanguageFormatting
{
    LspRequestContext context;
    std::optional<std::vector<core::LspTextEdit>> edits;
    core::LspPositionEncoding encoding;
};
} // namespace nenenib::application
