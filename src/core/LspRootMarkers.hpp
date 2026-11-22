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

// 同じ優先の印の組（どれか 1 つがあれば当たり）。
using LspRootTier = std::span<const std::string_view>;

// 言語サーバーの root を探す印の表の唯一の写し（ADR 0161 の 2026-11-22 の追補・段 S3）。組の列は
// 優先の順（Neovim の `vim.fs.root` と同じ意味）で、空でない。中身と順は nvim-lspconfig の
// `lsp/<server>.lua` に照らした。glob の印（`*.sln`）は持たない。OS に触れない。
[[nodiscard]] std::span<const LspRootTier> lsp_root_markers(Language language) noexcept;

// folder とその上のフォルダを近い順に（folder が 1 つ目・上限 lsp_root_search_depth）。ドライブの根
// （`C:\`・`\\?\C:\`）と共有の根（`\\server\share`・`\\?\UNC\server\share`）より上へは行かない。
// ceiling（設定の root・#1960）が folder を含めば、ceiling のフォルダを最後の 1 つにしてそれより
// 上へは行かない（ceiling 自身は列に入る）。folder を含まない ceiling は無いのと同じ。
[[nodiscard]] std::vector<FilePath> lsp_root_folders(const FilePath &folder,
                                                     const std::optional<FilePath> &ceiling);

// folder の下の印 marker の経路（区切りは folder の末尾に無ければ `\` を 1 つ）。
[[nodiscard]] FilePath lsp_root_marker_path(const FilePath &folder, std::string_view marker);

// folders（近い順）のうち、組 tier の印のどれかがある最初のフォルダの位置（無ければ終わり）。
template <typename Present>
[[nodiscard]] std::vector<FilePath>::const_iterator
lsp_root_tier_found(const std::vector<FilePath> &folders, LspRootTier tier, const Present &present)
{
    return std::ranges::find_if(folders,
                                [tier, &present](const FilePath &here)
                                {
                                    return std::ranges::any_of(
                                        tier, [&here, &present](std::string_view marker)
                                        { return present(lsp_root_marker_path(here, marker)); });
                                });
}

// 印の組を優先の順に 1 つずつ、folder から上へ当て（present が「ある」と答える経路）、最初に
// 当たったフォルダ（段 S3）。ceiling は上る上限（lsp_root_folders・#1960）。どの組も当たらなければ
// 値なし。確かめは多くて（印の数 × 上る段の数）回。ファイルに触れるのは present を渡す側（adapter・
// 試験の替え玉）だけ（std::function を core に持ち込まないよう、写しはこの 1 本の雛形）。
template <typename Present>
[[nodiscard]] std::optional<FilePath> lsp_root_found(const FilePath &folder,
                                                     const std::optional<FilePath> &ceiling,
                                                     Language language, const Present &present)
{
    const auto folders = lsp_root_folders(folder, ceiling);
    for (const LspRootTier tier : lsp_root_markers(language))
    {
        const auto found = lsp_root_tier_found(folders, tier, present);
        if (found != folders.end())
        {
            return *found;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
