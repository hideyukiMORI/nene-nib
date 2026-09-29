#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ex のタブの命令（ADR 0057 の決定 4）。next は `:tabnext`、previous は `:tabprevious` と
// `:tabNext`、open は `:tabnew`、close は `:tabclose`、list は `:tabs`。値が増えたら controller の
// 写し先の switch がコンパイルで落ちる（CPP-002）。
enum class ExTabVerb : std::uint8_t
{
    next,
    previous,
    open,
    close,
    list
};
} // namespace nenenib::core
