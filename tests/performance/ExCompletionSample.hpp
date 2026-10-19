#pragma once
#include <cstddef>
#include <string_view>
namespace nenenib::tests::performance
{
// 期待の候補と checksum は持たない。候補は expected_completions が製品の正本（組み込みテーマの表と
// Ex の固定の候補の並び）から組み立て、checksum は rounds と候補から決まる（#690）。
struct ExCompletionSample
{
    std::string_view prefix;
    std::size_t rounds;
    std::string_view metadata;
};
} // namespace nenenib::tests::performance
