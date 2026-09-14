#include "Palette.hpp"

#include "BuiltinTheme.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// OS の外観 → 組み込みテーマ。テーマファイルが入るまでは対応は 1 対 1（ADR 0008）。
[[nodiscard]] constexpr BuiltinTheme theme_for(Appearance appearance) noexcept
{
    switch (appearance)
    {
    case Appearance::light:
        return BuiltinTheme::neutral_light;
    case Appearance::dark:
        return BuiltinTheme::ubuntu_aubergine;
    }
    std::unreachable();
}
} // namespace

Palette palette_for(Appearance appearance) noexcept
{
    return palette_of(theme_for(appearance));
}
} // namespace nenenib::core
