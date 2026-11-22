#pragma once

#include "LspPositionEncoding.hpp"
#include "LspRequestContext.hpp"
#include "LspSymbol.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// 文脈の照合を通った記号の答え（ADR 0191 の決定 5）。context は要求の文脈、symbols は
// core::lsp_document_symbols の列（文書の順・空なら見つからない）、encoding は答えた席の位置の単位
// （着地の写しに運ぶ）。total は切る前の記号の数（切ったら 1 行知らせる・#1958 のレビュー L1）。
// 公開 aggregate（CPP-003）。
struct LanguageSymbols
{
    LspRequestContext context;
    std::vector<core::LspSymbol> symbols;
    core::LspPositionEncoding encoding;
    std::size_t total;
};
} // namespace nenenib::application
