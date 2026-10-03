#pragma once

#include "ExDocumentVerb.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
struct ExDocumentName
{
    std::string_view name;
    std::size_t shortest;
    ExDocumentVerb verb;
};
} // namespace nenenib::core
