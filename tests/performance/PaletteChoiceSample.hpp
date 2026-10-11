#pragma once
#include "PaletteChoiceExpected.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
namespace nenenib::tests::performance
{
struct PaletteChoiceSample
{
    std::string_view input;
    std::size_t rounds;
    std::uint64_t checksum;
    std::span<const PaletteChoiceExpected> expected;
    std::size_t catalog_records;
    std::string_view metadata;
};
} // namespace nenenib::tests::performance
