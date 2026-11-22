#pragma once

#include "LspLocation.hpp"
#include "LspPositionEncoding.hpp"
#include "LspRequestContext.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// 文脈の照合を通った定義の答え（ADR 0190 の決定 1・7）。context は要求の文脈、locations は
// 届いた順の場所（空なら見つからない・URI → パスは core::file_path_of_uri）、encoding は答えた
// 席の位置の単位（着地の写しに運ぶ・#1916 のレビュー R2）。total は切る前の答えの件数（参照は
// lsp_reference_limit で切る・ADR 0191 の決定 4）。公開 aggregate（CPP-003）。
struct LanguageDefinition
{
    LspRequestContext context;
    std::vector<core::LspLocation> locations;
    core::LspPositionEncoding encoding;
    std::size_t total = 0;
};
} // namespace nenenib::application
