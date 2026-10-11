#pragma once

namespace nenenib::tests::performance
{
enum class CommandInputWorkload
{
    search_long,
    ex_long,
    palette_long,
    search_short,
    ex_short,
    palette_short,
    ex_left_long,
    palette_fill_long,
    controller_search,
    controller_ex,
    controller_palette,
    controller_fill,
};
} // namespace nenenib::tests::performance
