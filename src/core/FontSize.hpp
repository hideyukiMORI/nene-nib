#pragma once

#include "FontSizeAdjustment.hpp"
#include "FontSizeFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// pt の正本。DIP / 物理画素はこの値から導く（D14 / ADR 0020）。
class FontSize final
{
  public:
    static constexpr float minimum_points = 8.0F;
    static constexpr float maximum_points = 40.0F;
    static constexpr float default_points = 13.5F;

    [[nodiscard]] static std::expected<FontSize, FontSizeFailure> from_points(float points);
    [[nodiscard]] static std::expected<FontSize, FontSizeFailure> parse(std::string_view text);
    [[nodiscard]] float points() const noexcept;

  private:
    explicit FontSize(float points);
    float points_;
};

[[nodiscard]] FontSize default_font_size();
[[nodiscard]] FontSize adjusted_font_size(FontSize current, FontSizeAdjustment adjustment,
                                          std::size_t steps);
// 字の大きさの 10 進（小数 1 桁まで・FontSize::parse が読める形）。`:set fontsize?` と設定の面の
// 値の行が共用する 1 本。core は libm を呼ばないので 0.1 pt の整数で丸める。
[[nodiscard]] std::string font_size_text(FontSize size);
[[nodiscard]] float font_size_dips(FontSize size) noexcept;
[[nodiscard]] float font_size_ratio(FontSize size) noexcept;
} // namespace nenenib::core
