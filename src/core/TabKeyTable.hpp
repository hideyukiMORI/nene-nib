#pragma once

#include "EditMode.hpp"
#include "TabCommand.hpp"
#include "TabKey.hpp"

#include <optional>

namespace nenenib::core
{
// 鍵とモードからタブの命令を決める 1 か所（ADR 0056 の決定 10）。値なしはタブの命令が無いことで、
// 窓は鍵を次の表へ流す。Vim モードの Ctrl+W（Vim
// の語の削除と窓の命令の接頭辞）はここで値なしになり、 窓が Vim の engine の `Ctrl-w` へ流す（ADR
// 0148 の決定 7）。
[[nodiscard]] std::optional<TabCommand> tab_command_for(TabKey key, EditMode mode) noexcept;
} // namespace nenenib::core
