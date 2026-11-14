#pragma once

namespace nenenib::ui::win32
{
// 画面の文字（本文でない文字）の大きさ（DIP・採用案の寸法 docs/design/2026-09-15-look.md 第 2
// 節）。 renderer の書式と幅の試験（tests/ui/LanguageTextTests.cpp）が同じ値を読む（#1019・V-1598
// の C-L3）。
inline constexpr float tab_text_dips = 12.5F;
inline constexpr float ui_text_dips = 12.0F;
// 案内の鍵・名前・注記（ADR 0079）。
inline constexpr float guide_key_dips = 12.5F;
inline constexpr float guide_label_dips = 13.5F;
inline constexpr float guide_note_dips = 11.5F;
// 鍵の枠（draw_keycap と起動画面）の左右の余白。
inline constexpr float guide_key_padding_dips = 9.0F;
// F1 の一覧（Ctrl+P の面の `?`）の行の右端の鍵の枠の左右の余白（ADR 0078 の決定 12）。枠の幅を
// 測る renderer と幅の試験が読む（#1658）。
inline constexpr float palette_key_padding_dips = 9.0F;
} // namespace nenenib::ui::win32
