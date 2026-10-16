#pragma once

#include "DisplayForm.hpp"
#include "EditorSettings.hpp"
#include "TextViewGeometry.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace nenenib::application
{
// 同期呼出しの間だけ借用する、完全な表示文字列と書式（ADR 0114）。
// form が grid のときは格子の 1 画面行（ADR 0140 の決定 5）で、等幅の書式で組み、cells（text の
// code point ごとの始まりの升・末尾は升の和）の升の左端へ字形を置く。cells は text から決まる値
// （GridRowView::starts）なので、保持の鍵は今日どおり文字列と書式（と form）。
struct TextLayoutRequest
{
    std::string_view text;
    const core::EditorSettings &settings;
    core::TextViewGeometry geometry;
    core::DisplayForm form = core::DisplayForm::text;
    std::span<const std::size_t> cells{};
};
} // namespace nenenib::application
