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

// 使う人が打った長い文（入力行の命令や値・上限は input_line_limit・#1568）を知らせの 1 行にする。
// maximum_bytes を超えれば末尾を code point の境目で落として「…」を付け、parse を通す。空・
// 不正な UTF-8・制御文字は parse と同じ失敗。
[[nodiscard]] std::expected<DisplayText, TextFailure> clipped_display_text(std::string_view text);
} // namespace nenenib::core
