#pragma once

#include "LspLocation.hpp"
#include "LspRequestContext.hpp"

#include <vector>

namespace nenenib::application
{
// 文脈の照合を通った定義の答え（ADR 0190 の決定 1・7）。context は要求の文脈、locations は
// 届いた順の場所（空なら見つからない・URI → パスは core::file_path_of_uri）。公開 aggregate
// （CPP-003）。
struct LanguageDefinition
{
    LspRequestContext context;
    std::vector<core::LspLocation> locations;
};
} // namespace nenenib::application
