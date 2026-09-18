#include "Palette.hpp"

#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// OS の外観 → 組み込みテーマ。テーマの選択状態・保存・:colorscheme は C2 / C3 で、
// それまでは対応は 1 対 1（ADR 0008・ADR 0017 の決定 8）。
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
    return theme_of(theme_for(appearance)).ui;
}
} // namespace nenenib::core
