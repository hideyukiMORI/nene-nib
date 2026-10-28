#pragma once

#include <cstdint>

namespace nenenib::core
{
// 大小の写像の表の 1 行（Issue #781・ADR 0015 の決定 10 の追記）。first から last までの step
// 個おきの code point が、それぞれ自分に delta を足した code point へ写る。表は生成物
// CaseMappingTable.hpp で、行は first の昇順で重ならない（VimCase.cpp の static_assert が守る）。
struct CaseMappingRange
{
    char32_t first;
    char32_t last;
    std::uint8_t step;
    std::int32_t delta;
};
} // namespace nenenib::core
