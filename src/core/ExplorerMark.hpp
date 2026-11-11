#pragma once

#include "SyntaxRole.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// エクスプローラーのファイルの印（16 DIP の升の中の 6 DIP の角丸の四角）の色の役割（#1390・採用案
// docs/design/2026-11-10-explorer-look.md・施主決定 D81）。言語は ADR 0154 の拡張子 → 言語の 1 本
// language_of_path で引き、C と C++ はヘッダ（header_of_path）とソースを分ける。C / C++ のソース・
// Python は function、ヘッダ・JS / TS は type、Markdown は string、JSON / YAML / TOML は keyword、
// HTML / XML / CSS は operators。ほか（plain を含む）は値なしで、renderer は UI の muted で塗る。
// 言語 → 役割の表はこの 1 か所で、renderer は syntax_color で色へ写すだけ。
[[nodiscard]] std::optional<SyntaxRole> explorer_mark_role(std::string_view name) noexcept;
} // namespace nenenib::core
