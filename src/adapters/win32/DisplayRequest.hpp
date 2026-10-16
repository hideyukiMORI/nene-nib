#pragma once

namespace nenenib::adapters::win32
{
// 試作（ADR 0140）: 環境変数 NIB_DISPLAY が grid なら、開いた文書を格子で見せる。問いの代わり。
[[nodiscard]] bool grid_display_requested();
} // namespace nenenib::adapters::win32
