#pragma once

#include "FilePath.hpp"
#include "Language.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 印を探して上る階層の上限（ADR 0161 の 2026-11-22 の追補・段 S3）。文書のフォルダを 1
// 段目に数える。
inline constexpr std::size_t lsp_root_search_depth = 32;

// 言語サーバーの root を探す印の表の唯一の写し（ADR 0161 の 2026-11-22 の追補・段 S3）。言語ごとの
// 印の列の後ろに全言語に共通の最後の印 `.git` が付く（列は空でない）。中身は nvim-lspconfig の
// `root_markers` に照らした。glob の印（`*.sln`）は持たない。OS に触れない。
[[nodiscard]] std::span<const std::string_view> lsp_root_markers(Language language) noexcept;

// folder とその上のフォルダを近い順に（folder が 1 つ目・上限 lsp_root_search_depth・ドライブや
// 共有の根で止まる）。
[[nodiscard]] std::vector<FilePath> lsp_root_folders(const FilePath &folder);

// folder の下の印 marker の経路（区切りは folder の末尾に無ければ `\` を 1 つ）。
[[nodiscard]] FilePath lsp_root_marker_path(const FilePath &folder, std::string_view marker);

// folder から上へ、印のどれか（present が「ある」と答える経路）がある最初のフォルダ（段 S3）。
// フォルダごとに表の順に見る。見つからなければ値なし。ファイルに触れるのは present を渡す側
// （adapter・試験の替え玉）だけ（std::function を core に持ち込まないよう、写しはこの 1
// 本の雛形）。
template <typename Present>
[[nodiscard]] std::optional<FilePath> lsp_root_found(const FilePath &folder, Language language,
                                                     const Present &present)
{
    const auto markers = lsp_root_markers(language);
    const auto marked = [&markers, &present](const FilePath &here)
    {
        return std::ranges::any_of(markers, [&here, &present](std::string_view marker)
                                   { return present(lsp_root_marker_path(here, marker)); });
    };
    auto folders = lsp_root_folders(folder);
    const auto found = std::ranges::find_if(folders, marked);
    return found != folders.end() ? std::optional{*found} : std::nullopt;
}
} // namespace nenenib::core
