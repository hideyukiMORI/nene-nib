#pragma once

#include "Column.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nenenib::ui::win32
{
class Direct2DRenderer;

// 所有済み表示行を、同期描画の位置変換中だけ借用する（ADR 0097）。
class LineUtf16Evaluation
{
    friend class Direct2DRenderer;

    explicit LineUtf16Evaluation(std::string_view text);
    [[nodiscard]] std::size_t byte_of(core::Column column);
    [[nodiscard]] std::uint32_t units_of(core::Column column);
    void move_to(core::Column column);

    std::string_view text_;
    core::Offset byte_{0};
    core::Column column_{1};
    std::size_t units_{0};
};
} // namespace nenenib::ui::win32
