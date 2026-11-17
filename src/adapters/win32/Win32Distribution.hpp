#pragma once

#include "Distribution.hpp"

#include <cstdint>

namespace nenenib::adapters::win32
{
// 製品がパッケージ（MSIX・Store 版）で動いているかを OS に問う唯一の場所（ARC-007・ADR 0178 の
// 追補 #1809）。合成ルートが起動時に 1 回だけ呼び、値を controller に渡す。
[[nodiscard]] core::Distribution current_distribution() noexcept;

// GetCurrentPackageFullName の戻り値の写し。名前の置き場が足りない・成功は package identity が在る
// ので packaged。APPMODEL_ERROR_NO_PACKAGE（ZIP 版）と想定外の値は unpackaged（分からないときは
// 今までどおりに倒す・誤って packaged にすると ZIP 版で効く機能が消える）。
[[nodiscard]] core::Distribution distribution_of(std::int32_t package_name_result) noexcept;
} // namespace nenenib::adapters::win32
