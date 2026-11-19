#pragma once

#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "TabTipPaint.hpp"
#include "TitleBarLayout.hpp"

#include <cstddef>
#include <cstdint>
#include <dwrite.h>
#include <optional>
#include <string>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 札 1 つを描く値を、札の矩形 box（物理画素）と文の layout から作る。文の左上は box の左上 + 余白、
// 角丸と縁の太さは DPI に合わせ、色は palette の panel / panel_border / text。タブの札と hover の札
// （HoverTipLayout・ADR 0190 の決定 6）が同じこの 1 本を通る。
[[nodiscard]] TabTipPaint tip_paint(const core::LayoutRect &box, IDWriteTextLayout *written,
                                    const core::Palette &palette, std::uint32_t dpi);

// タブの札の文の layout の保持（#1789・上限 1）。札は 1 つしか出ないので、最後に作った 1 本だけを
// 覚え、同じ文・同じ幅の上限・同じ書式ならそれを返す。幅の上限を超える文は core::middle_clipped で
// 真ん中を「…」に替え、上限に収まる最も少ない切り方を二分探索で探す（測るのは DirectWrite・
// 切るのは core の純関数）。作り直すのは札を出したときと窓の幅・DPI・書式が変わったときだけで、
// 打鍵と frame の道では作らない。書式の作り直しで clear する。
class TabTipLayout final
{
  public:
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

    // 収めた 1 行の layout（作れなければ null）。所有はこの保持で、次の fitted か clear まで有効。
    [[nodiscard]] IDWriteTextLayout *fitted(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                            std::string_view text, std::int32_t limit);
    // 最後に収めた layout の札を、帯の配置 layout の位置 tab のタブの下に置いて描く値（置き場は
    // core::tab_tip_rect・色は palette の panel / panel_border / text）。layout が無ければ値なし。
    [[nodiscard]] std::optional<TabTipPaint> painted(const core::TitleBarLayout &layout,
                                                     std::size_t tab, const core::Palette &palette,
                                                     std::uint32_t dpi) const;
    void clear() noexcept;

  private:
    std::string text_;
    std::int32_t limit_ = 0;
    IDWriteTextFormat *format_ = nullptr;
    TextLayout layout_;
};
} // namespace nenenib::ui::win32
