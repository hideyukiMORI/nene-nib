#pragma once

namespace nenenib::tests::performance
{
enum class ExCompletionWorkload
{
    option,
    miss,
    custom_narrow,
    empty,
    set_wide,
    theme_wide,
    all_builtins,
    custom_wide,
    all_custom,
    frame_ex_option,
    frame_ex_empty,
    frame_palette,
};
}
