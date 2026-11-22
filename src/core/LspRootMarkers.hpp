#pragma once

#include "FilePath.hpp"
#include "Language.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace nenenib::core
{
// 印を探して上る階層の上限（ADR 0161 の 2026-11-22 の追補・段 S3）。文書のフォルダを 1
// 段目に数える。
inline constexpr std::size_t lsp_root_search_depth = 32;

// 言語サーバーの root を探す印の表の唯一の写し（ADR 0161 の 2026-11-22 の追補・段 S3）。言語ごとの
// 印の列の後ろに全言語に共通の最後の印 `.git` が付く（列は空でない）。中身は nvim-lspconfig の
// `root_markers` に照らした。glob の印（`*.sln`）は持たない。OS に触れない。
[[nodiscard]] std::span<const std::string_view> lsp_root_markers(Language language) noexcept;

// folder から上へ、印のどれか（present がある・と答える folder の下の名前）がある最初のフォルダ
// （段 S3）。上限は lsp_root_search_depth 段で、ドライブや共有の根で止まる。見つからなければ
// 値なし。ファイルに触れるのは present を渡す側（adapter・試験の替え玉）だけ。
[[nodiscard]] std::optional<FilePath>
lsp_root_found(const FilePath &folder, Language language,
               const std::function<bool(const FilePath &)> &present);
} // namespace nenenib::core
