#pragma once

#include <cstdint>

namespace nenenib::core
{
// 表の横の送りの棒のつまみ（#1883・DIP）。left は溝の左端から、length は長さ。公開 aggregate
// （CPP-003）。
struct PreviewTableThumb
{
    std::int32_t left;
    std::int32_t length;
};
} // namespace nenenib::core
