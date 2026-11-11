#pragma once
#include <cstddef>

namespace nenenib::tests::ui
{
void verify_body_text_layouts();
// 1 つの確かめ。外れたら 1 行書いて数える（落とすのは main の終わり）。
void expect(bool condition, const char *message);
// 環境依存の検査（QLT-013）がこの機械では測れなかった。数に入れずに 1 行言う。
void unmeasured(const char *what);
[[nodiscard]] std::size_t checks();
[[nodiscard]] std::size_t failures();
[[nodiscard]] std::size_t unmeasured_count();

// 試験の組（ADR 0077）。
void verify_fallback_keys();
void verify_fallback_cache();
void verify_glyph_collector();
void verify_glyph_pixels();
void verify_fallback_ranges();
void verify_fallback_segment_memo();
void verify_replaced_tints();
void verify_gutter_layouts();
void verify_file_drop();
void verify_grid_render();
void verify_presented_milestones();
void verify_device_warmup();
void verify_window_timers();
void verify_wheel_turns();
void verify_preview_layouts();
void verify_status_demand();
void verify_syntax_render();
void verify_palette_layouts();
void verify_explorer_paint();
void verify_pointer_presses();
void verify_guide_key_width();
void verify_print_render();
void verify_diagnostic_waves();
// matchparen の相手の括弧の枠（ADR 0164 の決定 4・#999）。
void verify_matchparen_outline();
// ミニマップの絵（ADR 0171 の決定 4・7・#1017 の段 1）。
void verify_minimap_paint();
void verify_save_question();
// Vim モードの仮想キー → 特別な鍵の表（#1325・Delete は `<Del>`）。
void verify_vim_special_keys();
// 閉じる手順の列（ADR 0133 の追補 #1340）。
void verify_closing_steps();
// Ctrl+Shift+M はどのモードでも操作 toggle_preview（施主決定 D77・#1354）。
void verify_preview_cycle_key();
// 通常モードの Windows の標準の鍵（Ctrl+N・Ctrl+Shift+Z・Ctrl / Shift+Insert・Shift+Delete・D82・
// #1395）。
void verify_standard_keys();
void verify_word_deletion_keys();
void verify_preview_images();
} // namespace nenenib::tests::ui
