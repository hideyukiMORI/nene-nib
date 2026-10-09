#pragma once
#include <cstddef>

namespace nenenib::tests::ui
{
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
void verify_gutter_layouts();
} // namespace nenenib::tests::ui
