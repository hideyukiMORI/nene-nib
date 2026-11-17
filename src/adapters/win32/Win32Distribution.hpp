#pragma once

#include "Distribution.hpp"

#include <cstdint>

namespace nenenib::adapters::win32
{
// 製品がパッケージ（MSIX・Store 版）で動いているかを OS に問う唯一の場所（ARC-007・ADR 0178 の
// 追補 #1809）。合成ルートが起動時に 1 回だけ呼び、値を controller に渡す。
[[nodiscard]] core::Distribution current_distribution() noexcept;

// GetCurrentPackageFullName の戻り値の写し。APPMODEL_ERROR_NO_PACKAGE は package identity が無い
// （ZIP 版）ので unpackaged、ほか（名前の置き場が足りない・成功）は identity が在るので packaged。
[[nodiscard]] core::Distribution distribution_of(std::int32_t package_name_result) noexcept;
} // namespace nenenib::adapters::win32
