#pragma once
#include <cstddef>
#include <optional>
#include <string_view>
namespace nenenib::tests::performance
{
// 期待の候補と checksum は持たない（Ex の補完と同じ正本から組み立てる・#690）。
struct CompletionPrefixSample
{
    std::string_view input;
    std::string_view text;
    std::size_t caret;
    std::optional<std::size_t> completion_index;
    std::string_view metadata;
};
} // namespace nenenib::tests::performance
