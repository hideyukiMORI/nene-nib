#pragma once

namespace nenenib::core
{
// 文字集合の 1 区間（両端を含む）。`[a-z]` の a と z、1 文字だけの成員は first == last。
// 値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimPatternRange
{
    char32_t first;
    char32_t last;
};
} // namespace nenenib::core
