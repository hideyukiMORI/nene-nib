#pragma once

#include <cstdint>

namespace nenenib::application
{
// 字体の一覧を読めなかった理由（#597）。どちらでも補完は打った値だけになり、知らせは出さない。
enum class FontFamilyFailure : std::uint8_t
{
    // DirectWrite の factory を作れない。
    unavailable,
    // 字体の集まりか名前を列挙できない。
    unlisted
};
} // namespace nenenib::application
