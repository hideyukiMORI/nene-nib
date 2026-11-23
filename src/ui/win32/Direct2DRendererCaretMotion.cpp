// キャレットの動き（尾を引く・ADR 0192・#1933）。frame の入口と出口（draw の 1 本・本文を描かない
// 全面のプレビューの frame も通る）で覚え（CaretMotionFollower）を進め、本文の行と格子の行が
// キャレットの行き先の矩形を渡すと core の純関数で四隅を進め、動いている間は本文の上に多角形を
// 重ねる。オフの道は設定の欄を 1 つ読むだけ（決定 3）。
#include "Direct2DRenderer.hpp"

#include "CaretGlide.hpp"
#include "CaretMotionPainter.hpp"
#include "CaretMotionStart.hpp"
#include "CaretPoint.hpp"
#include "CaretSparksPaint.hpp"
#include "CaretSparksPainter.hpp"
#include "Direct2DRendererDetail.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
namespace
{
// にじみの広がり（DIP）。
constexpr float caret_glow_dips = 6.0F;
} // namespace

using detail::to_color;
using detail::to_rect;

void Direct2DRenderer::allow_caret_motion(bool allowed, std::uint32_t floor) noexcept
{
    caret_motion_.allow(allowed, floor);
}

std::optional<std::uint32_t> Direct2DRenderer::caret_motion_generation() const noexcept
{
    return caret_motion_.generation();
}

void Direct2DRenderer::settle_caret_motion() noexcept
{
    caret_motion_.settle();
}

void Direct2DRenderer::allow_caret_sparks(bool allowed, std::uint32_t floor) noexcept
{
    caret_sparks_.allow(allowed, floor);
}

std::optional<std::uint32_t> Direct2DRenderer::caret_sparks_generation() const noexcept
{
    return caret_sparks_.generation();
}

void Direct2DRenderer::settle_caret_sparks() noexcept
{
    caret_sparks_.settle();
}

// 動かさないとき（決定 6）: 設定がオフ・Windows の「アニメーション効果」がオフ・IME の変換中。
// 覚えは空にする（次にオンになった最初の frame は跳ぶ）。タブが変わった frame と、操作中の枠の側が
// 同じまま本文の領域が変わった frame（split の境界線のドラッグなど）は跳ぶ（決定 5）。
void Direct2DRenderer::begin_caret_motion(const application::EditorFrame &frame,
                                          const core::BodyLayout &body)
{
    caret_motion_.begin(CaretMotionStart{
        frame.settings.caret_motion && !frame.composition.has_value(), frame.active_tab,
        frame.caret_motion, frame.settings.caret_motion_time,
        core::caret_quad_of(
            static_cast<float>(body.content.left), static_cast<float>(body.content.top),
            static_cast<float>(body.content.right), static_cast<float>(body.content.bottom)),
        frame.active_side});
    // 打ったときの粒（追補・#1979）は動きと独立。オフは欄 1 つを読むだけで覚えを空にする。
    caret_sparks_.begin(frame.settings.caret_sparks, frame.composition.has_value(), frame.typed,
                        frame.caret_sparks);
}

bool Direct2DRenderer::caret_moves(std::optional<D2D1_RECT_F> box)
{
    if (!box.has_value())
    {
        return false;
    }
    const auto &shown = box.value();
    if (!caret_motion_.moves(core::caret_quad_of(shown.left, shown.top, shown.right, shown.bottom)))
    {
        return false;
    }
    // IME の候補窓の位置は行き先（着いた後と同じ・ふつうのキャレットと同じ rect_of）。
    caret_rectangle_ = GridPainter::rect_of(shown);
    return true;
}

// 尾の先の色は palette の ime（組み込みの 2 つの配色では茄子色の側の紫・採用案の表に足さない既存の
// トークン・決定 4）。頭はキャレットの色（accent）。
void Direct2DRenderer::paint_caret_motion(const application::EditorFrame &frame)
{
    const auto &track = caret_motion_.track();
    if (!caret_motion_.paints() || !track.target.has_value())
    {
        return;
    }
    const auto &bounds = track.bounds.corners;
    context_->PushAxisAlignedClip(
        D2D1::RectF(bounds.at(0).x, bounds.at(0).y, bounds.at(2).x, bounds.at(2).y),
        D2D1_ANTIALIAS_MODE_ALIASED);
    static_cast<void>(CaretMotionPainter::paint(
        *context_.Get(), CaretMotionPaint{track.shown, track.from, track.target.value(),
                                          to_color(frame.palette.accent),
                                          to_color(frame.palette.ime), scaled(caret_glow_dips)}));
    context_->PopAxisAlignedClip();
}

// frame の出口。動かす frame でキャレットが見えなかった（全面のプレビュー・入力行・印）なら
// 覚えを捨てる（#1956 の M1）。
void Direct2DRenderer::finish_caret_motion() noexcept
{
    caret_motion_.end();
    caret_sparks_.end();
}

void Direct2DRenderer::see_caret_sparks(const std::optional<D2D1_RECT_F> &box) noexcept
{
    if (!caret_sparks_.on() || !box.has_value())
    {
        return;
    }
    const auto &shown = box.value();
    caret_sparks_.see(
        core::CaretPoint{(shown.left + shown.right) / 2.0F, (shown.top + shown.bottom) / 2.0F});
}

// 粒の 2 つの色は尾を引く動きと同じ accent（頭）と ime（尾の先・見本の紫）。トークンは足さない（追
// 補・#1979）。本文の領域の外へ散った粒は切る。
void Direct2DRenderer::paint_caret_sparks(const application::EditorFrame &frame,
                                          const core::BodyLayout &body)
{
    const auto &track = caret_sparks_.advance();
    if (!caret_sparks_.paints())
    {
        return;
    }
    context_->PushAxisAlignedClip(to_rect(body.content), D2D1_ANTIALIAS_MODE_ALIASED);
    CaretSparksPainter::paint(*context_.Get(), *brush_.Get(), track,
                              CaretSparksPaint{to_color(frame.palette.accent),
                                               to_color(frame.palette.ime), scaled(1.0F)});
    context_->PopAxisAlignedClip();
}
} // namespace nenenib::ui::win32
