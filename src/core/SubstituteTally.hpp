#pragma once

#include <cstddef>

namespace nenenib::core
{
// 置換の 1 周目の数（SubstituteAll.cpp の中だけで使う・ADR 0168 の決定 5・#1376）。当たりの数・
// 当たった行の数・最初と最後の当たった行・当たりが覆うバイトの和・広げた置き換えのバイトの和。
// 値は単独で妥当なので公開 aggregate（CPP-003）。
struct SubstituteTally
{
    std::size_t substitutions = 0;
    std::size_t lines = 0;
    std::size_t first = 0;
    std::size_t last = 0;
    std::size_t removed = 0;
    std::size_t added = 0;
};
} // namespace nenenib::core
