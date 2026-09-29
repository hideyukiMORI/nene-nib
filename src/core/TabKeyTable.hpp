#pragma once

#include "EditMode.hpp"
#include "TabCommand.hpp"
#include "TabKey.hpp"

#include <optional>

namespace nenenib::core
{
// 鍵とモードからタブの命令を決める 1 か所（ADR 0056 の決定 10）。値なしは「何もしない」で、窓は
// どの表へも流さない。Vim モードの Ctrl+W（Vim の語の削除と窓の命令の接頭辞）はここで値なしになる。
[[nodiscard]] std::optional<TabCommand> tab_command_for(TabKey key, EditMode mode) noexcept;
} // namespace nenenib::core
