#pragma once

#include <cstdint>
#include <string>

namespace nenenib::ui::win32
{
// プレビューの字体の設定（ADR 0151 の決定 16）。本文の字体（guifont）と pt と描画先の DPI で、
// 等幅の字体（コード）はここから PreviewTextLayouts が引く（D58）。3 つは互いに独立して妥当なので
// 公開 aggregate（CPP-003）。
struct PreviewTextStyle
{
    std::string family;
    float points;
    std::uint32_t dpi;
};

[[nodiscard]] inline bool operator==(const PreviewTextStyle &left, const PreviewTextStyle &right)
{
    return left.family == right.family && left.points == right.points && left.dpi == right.dpi;
}
} // namespace nenenib::ui::win32
