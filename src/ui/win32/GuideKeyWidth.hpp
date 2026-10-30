#pragma once

#include "GuideEntry.hpp"

#include <array>
#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// 空の文書の案内の鍵の箱の幅（物理画素・#1122・ADR 0078 の追補）。3 行の鍵の表示名
// （key_chord_label から作った GuideEntry::key）を描画と同じ書式の layout で測り、最も広い字の幅に
// 左右の余白 padding（物理画素）を足して切り上げる。core の operation_guide_layout はこの値を
// 鍵の列の幅に使う。測れない行があれば 0 を返し、core は採用の 64 DIP の列にする。
[[nodiscard]] std::int32_t guide_key_width(IDWriteFactory &factory, IDWriteTextFormat &format,
                                           const std::array<core::GuideEntry, 3> &entries,
                                           float padding);
} // namespace nenenib::ui::win32
