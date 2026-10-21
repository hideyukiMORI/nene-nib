#pragma once

#include <cstddef>

namespace nenenib::tests::preview
{
// nib_preview_probe の 1 回の測りの結果（#818）。blocks は文書の要素の数、first は錨の要素、
// measured は PreviewBlockTops が測った要素の数、layouts_created は PreviewTextLayouts が作った
// layout の数。どれも互いに独立した数なので公開 aggregate（CPP-003）。
struct PreviewProbeOutcome
{
    std::size_t blocks;
    std::size_t first;
    std::size_t measured;
    std::size_t layouts_created;
};
} // namespace nenenib::tests::preview
