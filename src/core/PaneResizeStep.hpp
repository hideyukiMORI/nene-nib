#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 枠の「広げる / 狭める」の 1 回分の升の数（ADR 0148 の 2026-11-23 の追補・#1972・設定
// `pane_resize_step`・`:set paneresizestep=`）。F1 の操作 widen_pane / narrow_pane が読む。既定は
// #1941 の 5 升、範囲は 1〜40、設定の画面の段の上下は 1 ずつ。
inline constexpr std::uint8_t pane_resize_step_default = 5;
inline constexpr std::uint8_t pane_resize_step_minimum = 1;
inline constexpr std::uint8_t pane_resize_step_maximum = 40;

// 升の数（10 進・先頭の 0 なし・範囲の中）を読む 1 本。設定の読みと `:set` が共用する。
[[nodiscard]] std::optional<std::uint8_t> pane_resize_step_of(std::string_view text) noexcept;
// 1 段上下に送る（範囲の端で止まる）。
[[nodiscard]] std::uint8_t pane_resize_step_stepped(std::uint8_t cells, bool forward) noexcept;
// 設定の画面に出す値の文（10 進の数だけ）。
[[nodiscard]] std::string pane_resize_step_text(std::uint8_t cells);
// Vim の回数なしの `Ctrl-w > < + -` の 1 回分（設定 `pane_resize_step_vim`）。既定の off は本物の
// Vim と同じ 1 升、on は 1 回分の設定の升（回数つきはどちらでも回数・hide の決め 2026-11-23）。
[[nodiscard]] std::int32_t vim_uncounted_pane_cells(std::uint8_t step, bool follows) noexcept;
} // namespace nenenib::core
