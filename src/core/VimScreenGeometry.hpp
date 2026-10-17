#pragma once

#include "VimAction.hpp"
#include "VimDisplayNavigation.hpp"
#include "VimEditorView.hpp"
#include "VimLayoutNeed.hpp"
#include "VimState.hpp"

#include <expected>
#include <optional>

namespace nenenib::core
{
// 表示領域（VimViewport）を読む鍵か（ADR 0129 の決定 1）。画面の行に関わる H M L と
// Ctrl-d/u/f/b（PgUp/PgDn も同じ動作）で、表は画面移動の解決と同じ 1 つ。
[[nodiscard]] bool vim_reads_viewport(VimAction action) noexcept;

// 純粋な画面移動の解決。行数か行の中の画面行が足りなければ、その問いを返し、状態は変えない。
// 格子（VimScrollUnit::row）の半画面と 1 画面は画面行の単位で解く（ADR 0144 の決定 5）。
[[nodiscard]] std::expected<std::optional<VimDisplayNavigation>, VimLayoutNeed>
vim_display_navigation(const VimState &state, const VimEditorView &view, VimAction action);
} // namespace nenenib::core
