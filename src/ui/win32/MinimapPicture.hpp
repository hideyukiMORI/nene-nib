#pragma once

#include "LayoutRect.hpp"
#include "MinimapBand.hpp"
#include "MinimapBlock.hpp"
#include "MinimapFlush.hpp"
#include "MinimapFrame.hpp"
#include "MinimapPen.hpp"
#include "MinimapRows.hpp"
#include "Palette.hpp"
#include "RenderFailure.hpp"
#include "SyntaxPalette.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <d2d1_1.h>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
[[nodiscard]] MinimapColors minimap_colors(const core::Palette &palette) noexcept;

// ミニマップの bitmap の命令に付ける tag（ADR 0171 の追補 #1449）。Flush が返す失敗の tag
// がこれなら ミニマップの命令の失敗、そうでなければ前の命令（本文など）の失敗。
inline constexpr D2D1_TAG minimap_tag = 0x4E49424DU;
// Flush の結果 result と、失敗した命令の最初の tag failed の読み（1 本）。
[[nodiscard]] MinimapFlush minimap_flush_of(HRESULT result, D2D1_TAG failed) noexcept;
// 幅 width・行 rows の入る bitmap（行は 512 行刻みに切り上げる）が、描く先の bitmap の上限
// maximum（GetMaximumBitmapSize）に収まるか。収まらなければ bitmap を作らずに直接描く。
[[nodiscard]] bool minimap_bitmap_fits(std::int32_t width, std::size_t rows,
                                       UINT32 maximum) noexcept;

// ミニマップの絵（ADR 0171 の決定 4）。renderer が 1 つ所有し、アクティブな文書の窓の行の塊を
// オフスクリーンの bitmap（幅は面の幅・1 行 = core::minimap_row_pixels）に持つ。frame の行の塊が
// 前と同じ値で、bitmap がその base の絵なら帯だけを描き直し（行の増減は下の行を CopyFromBitmap で
// ずらす）、そうでなければ窓ごと描く。frame ごとに描くのは面の地・DrawBitmap 1 回・見えている範囲
// の面（accent の 10%）。bitmap を作れない描く先では直接描く（同じ draw_rows の 1 本）。
// bitmap の命令は minimap_tag を付けて流し、ミニマップの命令だけが失敗したら bitmap の道を捨てて
// 直接描く（描画の失敗として窓を閉じる道へ返さない）。上限を超える bitmap は作らない（#1449）。
class MinimapPicture final
{
  public:
    // 面 panel に frame を描く。
    [[nodiscard]] std::expected<void, RenderFailure> paint(const MinimapPen &pen,
                                                           const application::MinimapFrame &frame,
                                                           const core::LayoutRect &panel);
    // bitmap を使わずに同じ面を描く（bitmap を作れない描く先の道・試験が画素を比べる口）。
    void paint_directly(const MinimapPen &pen, const application::MinimapFrame &frame,
                        const core::LayoutRect &panel);
    // 行の列 rows を origin（左上）から下へ描く唯一の関数（1 行 1 本の FillRectangle の列）。直接の
    // 道と bitmap の道が共用する。
    static void draw_rows(const MinimapPen &pen, std::span<const core::MinimapRow> rows,
                          D2D1_POINT_2F origin);
    // 描いた行の数（直接か bitmap へ）・DrawBitmap の回数・作った bitmap の数・下の行をずらした
    // 回数・帯の前に命令を流した回数（試験が窓ごとと帯だけを数える口）。
    [[nodiscard]] std::size_t painted_rows() const noexcept;
    [[nodiscard]] std::size_t copies() const noexcept;
    [[nodiscard]] std::size_t pictures() const noexcept;
    [[nodiscard]] std::size_t shifts() const noexcept;
    [[nodiscard]] std::size_t flushes() const noexcept;
    // bitmap の道を捨てる（以後この絵は直接描く）。ミニマップの命令の失敗を見つけた所（paint と
    // renderer の EndDraw の tag）が呼ぶ。
    void abandon_bitmap() noexcept;
    // bitmap の道を捨てたか（試験が読む口）。
    [[nodiscard]] bool bitmap_abandoned() const noexcept;

  private:
    // 幅 width・行 rows の入る bitmap（今のものが同じ幅で足りればそれ）。作れなければ nullptr。
    [[nodiscard]] ID2D1Bitmap1 *picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                           std::size_t rows);
    // minimap_tag を付けた後の paint の本体。
    [[nodiscard]] std::expected<void, RenderFailure>
    paint_tagged(const MinimapPen &pen, const application::MinimapFrame &frame,
                 const core::LayoutRect &panel);
    // bitmap を frame の rows の絵にする（帯だけか窓ごと・同じ絵なら何もしない）。
    void bring_up(ID2D1DeviceContext &context, const MinimapPen &pen,
                  const application::MinimapFrame &frame);
    // 命令を流し、失敗なら最初の失敗（HRESULT と失敗した命令の tag）を覚える（読みは paint）。
    [[nodiscard]] bool flushed(ID2D1DeviceContext &context);
    // 窓ごと描く。
    void record(ID2D1DeviceContext &context, const MinimapPen &pen,
                const application::MinimapRows &rows);
    // 帯の下の前の行を、行の増減だけ CopyFromBitmap でずらす（できなければ偽）。before は前の行の
    // 数。
    [[nodiscard]] bool shift_rows(ID2D1DeviceContext &context, const core::MinimapBand &band,
                                  std::size_t before);
    // 帯だけ描き直す（できなければ偽）。
    [[nodiscard]] bool redraw_band(ID2D1DeviceContext &context, const MinimapPen &pen,
                                   const application::MinimapRows &rows,
                                   const core::MinimapBand &band);
    static void draw_face(const MinimapPen &pen, const application::MinimapFrame &frame,
                          const core::LayoutRect &panel, std::size_t shown);

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> picture_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> scratch_;
    std::shared_ptr<const application::MinimapRows> held_;
    std::uint64_t held_serial_ = 0;
    std::optional<MinimapColors> held_colors_;
    std::optional<core::SyntaxPalette> held_body_;
    std::size_t painted_rows_ = 0;
    std::size_t copies_ = 0;
    std::size_t pictures_ = 0;
    std::size_t shifts_ = 0;
    std::size_t flushes_ = 0;
    // この frame の Flush の最初の失敗（HRESULT と失敗した命令の tag）。
    std::optional<std::pair<HRESULT, D2D1_TAG>> flush_failure_;
    bool abandoned_ = false;
};
} // namespace nenenib::ui::win32
