#pragma once

#include "VimAction.hpp"
#include "VimDisplayNavigation.hpp"
#include "VimEditorView.hpp"
#include "VimState.hpp"

#include <expected>
#include <optional>

namespace nenenib::core
{
// 純粋な画面移動の解決。行数が足りなければ必要な論理行を返し、状態は変えない。
[[nodiscard]] std::expected<std::optional<VimDisplayNavigation>, LineNumber>
vim_display_navigation(const VimState &state, const VimEditorView &view, VimAction action);
} // namespace nenenib::core
