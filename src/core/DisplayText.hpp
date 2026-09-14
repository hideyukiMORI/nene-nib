#pragma once

#include "TextFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 表示できると検証済みの 1 行。生成経路は parse ただ 1 つ（CPP-007）。
class DisplayText final
{
  public:
    static constexpr std::size_t maximum_bytes = 256;

    [[nodiscard]] static std::expected<DisplayText, TextFailure> parse(std::string_view text);
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] std::size_t code_point_count() const noexcept;

  private:
    DisplayText(std::string text, std::size_t code_point_count);
    std::string text_;
    std::size_t code_point_count_;
};
} // namespace nenenib::core
