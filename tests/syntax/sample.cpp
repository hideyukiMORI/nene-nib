// 構文の色分けの見本（#816・ADR 0156）。eng/verify-window.py --syntax が開いて撮る。
#include "Lexer.hpp"
#include <cstdint>

#define NIB_LIMIT 16

/* 行をまたぐ
   ブロックコメント */
namespace nenenib::sample
{
constexpr std::size_t limit = 0x40U;
constexpr double ratio = 12.5e-3;

struct Point
{
    int x = 0;
    int y = 1'000;
};

[[nodiscard]] bool inside(const Point &point, unsigned count) noexcept
{
    const char *name = "nib\t\"quoted\"";
    const auto raw = R"tag(raw (string) line)tag";
    if (point.x < 0 || point.y >= limit)
    {
        return false; // 範囲の外
    }
    return count != 0 && name != nullptr && raw[0] == 'r';
}
} // namespace nenenib::sample
