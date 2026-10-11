#pragma once
namespace nenenib::tests::performance
{
enum class PaletteChoiceWorkload
{
    builtin_all,
    custom_all,
    miss,
    short_query,
    theme,
    direct_fill,
    direct_long,
    fallback_long,
    custom_narrow
};
} // namespace nenenib::tests::performance
