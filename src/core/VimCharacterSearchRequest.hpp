#pragma once

#include "VimCharacterSearch.hpp"
#include "VimCharacterSearchInvocation.hpp"

#include <cstddef>

namespace nenenib::core
{
// 1回の行内走査に必要な値。回数はoperator側とmotion側を掛けた最終値。
struct VimCharacterSearchRequest
{
    VimCharacterSearch search;
    std::size_t count;
    VimCharacterSearchInvocation invocation;
};
} // namespace nenenib::core
