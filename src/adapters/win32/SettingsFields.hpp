#pragma once

#include <optional>
#include <string_view>

namespace nenenib::adapters::win32
{
// version=1 の直列化 DTO。domain へ変換した後は保持しない（ARC-009）。
struct SettingsFields
{
    std::optional<std::string_view> version;
    std::optional<std::string_view> colorscheme;
    std::optional<std::string_view> font_family;
    std::optional<std::string_view> font_size;
};
} // namespace nenenib::adapters::win32
