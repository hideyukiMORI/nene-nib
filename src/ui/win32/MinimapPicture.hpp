#pragma once

#include "LayoutRect.hpp"
#include "MinimapBand.hpp"
#include "MinimapBlock.hpp"
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
#include <wrl/client.h>

namespace nenenib::ui::win32
{
[[nodiscard]] MinimapColors minimap_colors(const core::Palette &palette,
                                           const core::SyntaxPalette &body) noexcept;

// ミニマップの絵（ADR 0171 の決定 4）。renderer が 1 つ所有し、アクティブな文書の窓の行の塊を
// オフスクリーンの bitmap（幅は面の幅・1 行 = core::minimap_row_pixels）に持つ。frame の行の塊が
// 前と同じ値で、bitmap がその base の絵なら帯だけを描き直し（行の増減は下の行を CopyFromBitmap で
// ずらす）、そうでなければ窓ごと描く。frame ごとに描くのは面の地・DrawBitmap 1 回・見えている範囲
// の面（accent の 10%）。bitmap を作れない描く先では直接描く（同じ draw_rows の 1 本）。
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
    // 回数（試験が窓ごとと帯だけを数える口）。
    [[nodiscard]] std::size_t painted_rows() const noexcept;
    [[nodiscard]] std::size_t copies() const noexcept;
    [[nodiscard]] std::size_t pictures() const noexcept;
    [[nodiscard]] std::size_t shifts() const noexcept;

  private:
    // 幅 width・行 rows の入る bitmap（今のものが同じ幅で足りればそれ）。作れなければ nullptr。
    [[nodiscard]] ID2D1Bitmap1 *picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                           std::size_t rows);
    // bitmap を frame の rows の絵にする（帯だけか窓ごと・同じ絵なら何もしない）。
    void bring_up(ID2D1DeviceContext &context, const MinimapPen &pen,
                  const application::MinimapFrame &frame);
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
    std::size_t painted_rows_ = 0;
    std::size_t copies_ = 0;
    std::size_t pictures_ = 0;
    std::size_t shifts_ = 0;
};
} // namespace nenenib::ui::win32
