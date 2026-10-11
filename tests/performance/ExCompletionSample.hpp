#pragma once
#include <cstdint>
#include <span>
#include <string_view>
namespace nenenib::tests::performance
{
struct ExCompletionSample
{
    std::string_view prefix;
    std::size_t rounds;
    std::uint64_t checksum;
    std::span<const std::string_view> expected;
    std::string_view metadata;
};
} // namespace nenenib::tests::performance
