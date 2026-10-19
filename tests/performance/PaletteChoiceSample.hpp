#pragma once
#include <cstddef>
#include <string_view>
namespace nenenib::tests::performance
{
// 期待の候補と checksum は持たない（PaletteChoiceProbe の expected_commands が作る・#690）。
struct PaletteChoiceSample
{
    std::string_view input;
    std::size_t rounds;
    std::size_t catalog_records;
    std::string_view metadata;
};
} // namespace nenenib::tests::performance
