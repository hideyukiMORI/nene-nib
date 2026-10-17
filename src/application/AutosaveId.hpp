#pragma once

#include <cstdint>

namespace nenenib::application
{
// タブの退避番号（ADR 0137 の決定 3）。EditorState がタブを足すときに振る通し番号で、閉じても
// 再利用しない。帯の位置・パス・題名ではタブを名指ししない。値は単独で妥当なので公開 aggregate。
// 比較は非メンバーで書く（CPP-003）。
struct AutosaveId
{
    std::uint64_t value;
};

[[nodiscard]] constexpr bool operator==(AutosaveId left, AutosaveId right) noexcept
{
    return left.value == right.value;
}

[[nodiscard]] constexpr bool operator<(AutosaveId left, AutosaveId right) noexcept
{
    return left.value < right.value;
}
} // namespace nenenib::application
