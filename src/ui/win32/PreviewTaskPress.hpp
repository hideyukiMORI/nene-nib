#pragma once

#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// プレビューの枠で押したチェックボックス（ADR 0155 の追補 24・ADR 0056 の決定 9
// の形）。押した要素と 離した要素が同じときだけ番号を返す。プレビューの枠は SetCapture
// しないので、窓の外で離すと WM_LBUTTONUP が来ない。窓は WM_LBUTTONDOWN の入口で毎回 forget
// を呼び、前の押しを残さない。
class PreviewTaskPress final
{
  public:
    void forget() noexcept
    {
        pressed_.reset();
    }
    void press(std::optional<std::size_t> block) noexcept
    {
        pressed_ = block;
    }
    [[nodiscard]] bool pressed() const noexcept
    {
        return pressed_.has_value();
    }
    // 離した点の要素 block が押した要素と同じなら番号、ほかは nullopt。押しはここで消える。
    [[nodiscard]] std::optional<std::size_t> release(std::optional<std::size_t> block) noexcept
    {
        const auto pressed = std::exchange(pressed_, std::nullopt);
        return pressed.has_value() && pressed == block ? pressed : std::nullopt;
    }

  private:
    std::optional<std::size_t> pressed_;
};
} // namespace nenenib::ui::win32
