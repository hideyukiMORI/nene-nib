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
} // namespace nenenib::tests::ui
