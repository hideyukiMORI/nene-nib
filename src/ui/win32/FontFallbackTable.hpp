#pragma once
#include "FontFallbackEntry.hpp"
#include <cstddef>
#include <unordered_map>
#include <vector>
namespace nenenib::ui::win32
{
// 字体選択の保持の表（ADR 0071 の決定 3・4）。上限 128 件で、超えたら古いものから FIFO で
// 入れ替わる。引くのは鍵の hash の索引から入り、一致は鍵の全欄で決める（hash が同じで鍵が
// 違う枠は別の枠・#586）。
class FontFallbackTable
{
  public:
    FontFallbackTable();
    [[nodiscard]] const FontFallbackEntry *find(const FontFallbackKey &key) const;
    void retain(FontFallbackEntry entry);

  private:
    std::vector<FontFallbackEntry> entries_;
    // 鍵の hash → entries_ の位置。
    std::unordered_multimap<std::size_t, std::size_t> slots_;
    std::size_t next_ = 0;
};
} // namespace nenenib::ui::win32
