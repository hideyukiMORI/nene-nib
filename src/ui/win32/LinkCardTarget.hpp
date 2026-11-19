#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 点の下のリンクの札の部品（施主決定 D106・#1869 の段 L2）。outside は札の外（押すと札を閉じて
// 今までの道へ）、card は札の地（押しても何もしない・本文へ渡さない）、secondary と primary は左と
// 右のボタン（押した部品と離した部品が同じときだけ ActOnLinkCard を送る）。
enum class LinkCardTarget : std::uint8_t
{
    outside,
    card,
    secondary,
    primary
};
} // namespace nenenib::ui::win32
