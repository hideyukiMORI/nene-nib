#pragma once

#include "ExDocumentVerb.hpp"
#include "FilePath.hpp"

#include <optional>

namespace nenenib::core
{
// overwrite は `:w! <file>` `:saveas! <file>` の印で、別のパスに既存のファイルがあっても置き換える
// （ADR 0066 の追補・Issue #738）。パスが無いときと `!` の無いときは false。
struct ExDocumentRequest
{
    ExDocumentVerb verb;
    std::optional<FilePath> path;
    bool overwrite;
};
} // namespace nenenib::core
