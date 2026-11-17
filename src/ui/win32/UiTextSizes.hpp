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
// 全文検索の当たりの行の上の段（本文の字体）と下の段（`相対パス:行`）と、入力欄の右の進み具合
// （ADR 0167 の決定 5・採用案 docs/design/2026-11-16-finder-look.md の「当たりの行」）。
inline constexpr float hit_line_dips = 12.5F;
inline constexpr float hit_place_dips = 11.0F;
inline constexpr float palette_progress_dips = 11.5F;
// プレビュー欄の本文と行番号（本文の字体・面の行の文字と同じ 12.5 DIP・fontsize に追従しない）と
// 見出し（UI の字体・11.5 DIP・ADR 0189 の決定 5・採用案の「プレビュー欄」）。
inline constexpr float preview_line_dips = 12.5F;
inline constexpr float preview_heading_dips = 11.5F;
} // namespace nenenib::ui::win32
