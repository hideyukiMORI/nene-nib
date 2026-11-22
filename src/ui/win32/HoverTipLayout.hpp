#pragma once

#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "TabTipPaint.hpp"

#include <cstdint>
#include <dwrite.h>
#include <optional>
#include <string>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// hover の札の文の layout の保持（ADR 0190 の決定 6・上限 1）。札は 1 つしか出ないので、最後に
// 作った 1 本だけを覚え、同じ文・同じ幅の上限・同じ書式ならそれを返す。文は幅の上限で折り返し
// （語が上限より長ければ語の途中で折る）、折り返した画面行が core::hover_tip_max_lines を越えたら、
// 収まる最も多い文字までを残して「…」を付ける（測るのは DirectWrite・切るのは core の
// hover_tip_clipped・二分探索）。作り直すのは札を出したときと幅・DPI・書式が変わったときだけで、
// 打鍵と frame の道では作らない。描く値はタブの札と同じ TabTipPaint で、描くのも同じ
// paint_tab_tip の 1 本（別の描画器を持たない）。書式の作り直しで clear する。
class HoverTipLayout final
{
  public:
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

    // 収めた layout（作れなければ null）。所有はこの保持で、次の fitted か clear まで有効。
    [[nodiscard]] IDWriteTextLayout *fitted(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                            std::string_view text, std::int32_t limit);
    // 最後に収めた layout の太字の範囲を emphasis にする（UTF-16・長さ 0 なら無し・引数の情報の札の
    // 今の引数・#1959）。前の範囲は戻す。同じ範囲なら何もしない。layout が無いか当てられなければ
    // false（当てられなければ保持を捨てる）。
    [[nodiscard]] bool emphasized(DWRITE_TEXT_RANGE emphasis);
    // 最後に収めた layout の札を、キャレットの矩形 caret の上（入らなければ下）に領域 area の中で
    // 置いて描く値（置き場は core::hover_tip_rect・色は palette の panel / panel_border / text）。
    // layout が無ければ値なし。
    [[nodiscard]] std::optional<TabTipPaint> painted(const core::LayoutRect &caret,
                                                     const core::LayoutRect &area,
                                                     const core::Palette &palette,
                                                     std::uint32_t dpi) const;
    void clear() noexcept;

  private:
    std::string text_;
    std::int32_t limit_ = 0;
    IDWriteTextFormat *format_ = nullptr;
    DWRITE_TEXT_RANGE emphasis_{0, 0};
    TextLayout layout_;
};
} // namespace nenenib::ui::win32
