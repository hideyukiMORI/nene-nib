#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// プレビューの文字組みの当たりの引き方（#1876 のレビューの軽い 3）。edge は点に近い文字の端
// （全面の選択・PreviewPane::point_at）、character は点の真下の文字の始まりで、文字の外なら当たら
// ない（リンクの札・PreviewPane::character_at）。
enum class PreviewHitMode : std::uint8_t
{
    edge,
    character
};
} // namespace nenenib::ui::win32
