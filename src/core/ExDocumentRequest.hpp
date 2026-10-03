#pragma once

#include "ExDocumentVerb.hpp"
#include "FilePath.hpp"

#include <optional>

namespace nenenib::core
{
struct ExDocumentRequest
{
    ExDocumentVerb verb;
    std::optional<FilePath> path;
};
} // namespace nenenib::core
