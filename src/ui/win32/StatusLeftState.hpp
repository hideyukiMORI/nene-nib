#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// ステータスバーの左側に何を描くか（draw_status_left の分かれ目・ADR 0079 の決定 11）。
// command_input は `:` の入力（保持の枠を使わない）、message は 1 行の知らせ（1 枠）、mode は
// トグルとモード（3 枠）、recording はそれに `recording @a`（4 枠）、diagnostic はトグルとモードに
// キャレットの上の診断の文（4 枠・ADR 0161 の 2026-11-24 の追補・録画とは排他）。
enum class StatusLeftState : std::uint8_t
{
    command_input,
    message,
    mode,
    recording,
    diagnostic
};
} // namespace nenenib::ui::win32
