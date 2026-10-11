#pragma once
#include "EditMode.hpp"
#include "OperationChoiceExpected.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
namespace nenenib::tests::performance
{
struct OperationChoiceSample
{
    std::string_view query;
    core::EditMode mode;
    std::size_t rounds;
    std::uint64_t checksum;
    std::span<const OperationChoiceExpected> expected;
    std::string_view metadata;
};
} // namespace nenenib::tests::performance
