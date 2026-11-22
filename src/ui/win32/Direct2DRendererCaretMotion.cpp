// キャレットの動き（尾を引く・ADR 0191・#1933）。frame ごとに動かすかを決め、本文の行と格子の行が
// キャレットの行き先の矩形を渡すと core の純関数で覚えを進め、動いている間は本文の上に多角形を
// 重ねる。オフの道は設定の欄を 1 つ読むだけ（決定 3）。
#include "Direct2DRenderer.hpp"

#include "CaretGlide.hpp"
#include "CaretMotionPainter.hpp"
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

void Direct2DRenderer::allow_caret_motion(bool allowed) noexcept
{
    caret_motion_allowed_ = allowed;
}

std::optional<std::uint32_t> Direct2DRenderer::caret_motion_generation() const noexcept
{
    if (!caret_motion_.moving)
    {
        return std::nullopt;
    }
    return caret_motion_.generation;
}

// 動かさないとき（決定 6）: 設定がオフ・Windows の「アニメーション効果」がオフ・IME
// の変換中。覚えは 空にする（次にオンになった最初の frame は跳ぶ）。タブが変わった frame
// は跳ぶ（決定 5）。
void Direct2DRenderer::begin_caret_motion(const application::EditorFrame &frame,
                                          const core::BodyLayout &body)
{
    caret_motion_seen_ = false;
    caret_motion_on_ =
        frame.settings.caret_motion && caret_motion_allowed_ && !frame.composition.has_value();
    if (!caret_motion_on_)
    {
        if (caret_motion_.target.has_value())
        {
            caret_motion_ = core::caret_motion_settled(caret_motion_, std::nullopt);
        }
        return;
    }
    caret_motion_jump_ = frame.active_tab != caret_motion_tab_;
    caret_motion_tab_ = frame.active_tab;
    caret_motion_clock_ = frame.caret_motion;
    caret_motion_time_ = frame.settings.caret_motion_time;
    caret_motion_.bounds = core::caret_quad_of(
        static_cast<float>(body.content.left), static_cast<float>(body.content.top),
        static_cast<float>(body.content.right), static_cast<float>(body.content.bottom));
}

bool Direct2DRenderer::caret_moves(std::optional<D2D1_RECT_F> box)
{
    if (!box.has_value())
    {
        return false;
    }
    const auto &shown = box.value();
    const auto target = core::caret_quad_of(shown.left, shown.top, shown.right, shown.bottom);
    caret_motion_seen_ = true;
    caret_motion_ = caret_motion_jump_
                        ? core::caret_motion_settled(caret_motion_, target)
                        : core::caret_motion_followed(caret_motion_, target, caret_motion_clock_,
                                                      caret_motion_time_);
    if (!caret_motion_.moving)
    {
        return false;
    }
    // IME の候補窓の位置は行き先（着いた後と同じ）。
    caret_rectangle_ = RECT{static_cast<LONG>(shown.left), static_cast<LONG>(shown.top),
                            static_cast<LONG>(shown.right), static_cast<LONG>(shown.bottom)};
    return true;
}

// 尾の先の色は palette の ime（組み込みの 2 つの配色では茄子色の側の紫・採用案の表に足さない既存の
// トークン・決定 4）。頭はキャレットの色（accent）。
void Direct2DRenderer::finish_caret_motion(const application::EditorFrame &frame)
{
    if (!caret_motion_on_)
    {
        return;
    }
    if (!caret_motion_seen_)
    {
        caret_motion_ = core::caret_motion_settled(caret_motion_, std::nullopt);
        return;
    }
    if (!caret_motion_.moving || !caret_motion_.target.has_value())
    {
        return;
    }
    const auto &bounds = caret_motion_.bounds.corners;
    context_->PushAxisAlignedClip(
        D2D1::RectF(bounds.at(0).x, bounds.at(0).y, bounds.at(2).x, bounds.at(2).y),
        D2D1_ANTIALIAS_MODE_ALIASED);
    static_cast<void>(CaretMotionPainter::paint(
        *context_.Get(),
        CaretMotionPaint{caret_motion_.shown, caret_motion_.from, caret_motion_.target.value(),
                         to_color(frame.palette.accent), to_color(frame.palette.ime),
                         scaled(caret_glow_dips)}));
    context_->PopAxisAlignedClip();
}
} // namespace nenenib::ui::win32
