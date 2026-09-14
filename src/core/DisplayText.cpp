#include "DisplayText.hpp"

#include "Utf8.hpp"

#include <utility>

namespace nenenib::core
{
DisplayText::DisplayText(std::string text, std::size_t code_point_count)
    : text_(std::move(text)), code_point_count_(code_point_count)
{
}

std::expected<DisplayText, TextFailure> DisplayText::parse(std::string_view text)
{
    if (text.empty())
    {
        return std::unexpected(TextFailure::empty);
    }
    if (text.size() > maximum_bytes)
    {
        return std::unexpected(TextFailure::too_long);
    }
    // 符号としての正しさは Utf8 が唯一の経路で見る（ARC-001）。表示できるかの判断だけをここで足す。
    const auto code_points = validate_utf8(text);
    if (!code_points)
    {
        return std::unexpected(code_points.error());
    }
    if (has_control_character(text))
    {
        return std::unexpected(TextFailure::control_character);
    }
    return DisplayText(std::string(text), code_points.value());
}

std::string_view DisplayText::text() const noexcept
{
    return text_;
}

std::size_t DisplayText::code_point_count() const noexcept
{
    return code_point_count_;
}
} // namespace nenenib::core
