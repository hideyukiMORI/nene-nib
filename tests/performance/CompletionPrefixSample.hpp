#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
namespace nenenib::tests::performance
{
struct CompletionPrefixSample
{
    std::string_view input;
    std::string_view text;
    std::size_t caret;
    std::optional<std::size_t> completion_index;
    std::span<const std::string_view> expected;
    std::string_view metadata;
    std::uint64_t checksum;
};
} // namespace nenenib::tests::performance
