#pragma once

#include <string_view>

namespace nenenib::adapters::win32
{
template <typename Color, typename Palette> struct ColorField
{
    std::string_view key;
    Color Palette::*member;
};
} // namespace nenenib::adapters::win32
