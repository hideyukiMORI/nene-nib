#pragma once

#include "VimAction.hpp"
#include "VimDisplayNavigation.hpp"
#include "VimEditorView.hpp"
#include "VimState.hpp"

#include <expected>
#include <optional>

namespace nenenib::core
{
// 表示領域（VimViewport）を読む鍵か（ADR 0129 の決定 1）。画面の行に関わる H M L と
// Ctrl-d/u/f/b（PgUp/PgDn も同じ動作）で、表は画面移動の解決と同じ 1 つ。
[[nodiscard]] bool vim_reads_viewport(VimAction action) noexcept;

// 純粋な画面移動の解決。行数が足りなければ必要な論理行を返し、状態は変えない。
[[nodiscard]] std::expected<std::optional<VimDisplayNavigation>, LineNumber>
vim_display_navigation(const VimState &state, const VimEditorView &view, VimAction action);
} // namespace nenenib::core
