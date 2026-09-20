#pragma once

#include "VimCharacterSearchRequest.hpp"

#include <cstddef>

namespace nenenib::core
{
// 一方向の行内走査で共有する検索条件と残り一致数。
struct VimCharacterSearchScan
{
    VimCharacterSearchRequest request;
    std::size_t remaining;
};
} // namespace nenenib::core
