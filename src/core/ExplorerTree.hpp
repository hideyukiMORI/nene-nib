#pragma once

#include "ExplorerEntry.hpp"
#include "ExplorerNode.hpp"
#include "ExplorerRow.hpp"
#include "FilePath.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nenenib::core
{
struct ExplorerStep;

// エクスプローラーの木（ADR 0162 の決定 1）。純粋な値で、操作は新しい木を返す。節は深さ優先の前順の
// 列で、0 番がルート。ルートを変えるときは新しい木を作り直す（開いていたフォルダは引き継がない）。
// 行の番号は visible_rows の位置、節の番号は nodes の位置。loaded は子を置き直すので、その節より
// 後ろの節の番号は変わる（読み終えたフォルダは経路で node_of を引いて名指しする）。
class ExplorerTree final
{
  public:
    // 1 つのフォルダに置く子の上限。超えた分は並べた後の末尾を捨てて truncated にする。
    static constexpr std::size_t entry_limit = 2000;

    // ルートの無い木（行は 0 本）。
    ExplorerTree() = default;
    // root をルートにした、まだ子を読んでいない木。
    explicit ExplorerTree(FilePath root);

    [[nodiscard]] const std::optional<FilePath> &root() const noexcept;
    [[nodiscard]] std::span<const ExplorerNode> nodes() const noexcept;
    // 開いているフォルダの子だけを深さ優先で並べた行（ルートの行は含めない）。
    [[nodiscard]] std::vector<ExplorerRow> visible_rows() const;
    // フォルダの行なら開閉を入れ替える。ファイル・「…」・範囲の外の行なら同じ木。
    [[nodiscard]] ExplorerTree toggled(std::size_t row) const;
    // 開いているフォルダの行なら閉じて同じ行。ほかの行なら木は同じで親のフォルダの行へ（親がルート
    // か範囲の外なら同じ行）。
    [[nodiscard]] ExplorerStep collapsed_to_parent(std::size_t row) const;
    // フォルダの節 node の子を entries で置き直す（前の子とその下は捨てる）。並びはフォルダが先、
    // 名前は ASCII の大小を畳んだバイト順（同じならバイト順・OS の順に依らない）。entry_limit を
    // 超えた分は捨てて truncated。ファイルの節・範囲の外なら同じ木。
    [[nodiscard]] ExplorerTree loaded(std::size_t node,
                                      std::span<const ExplorerEntry> entries) const;
    // 節の経路（ルートの経路に名前を `\` で継ぐ）。ルートが無い・範囲の外なら値なし。
    [[nodiscard]] std::optional<FilePath> path_of(std::size_t node) const;
    // 経路が path の節の番号。無ければ値なし。
    [[nodiscard]] std::optional<std::size_t> node_of(const FilePath &path) const;

  private:
    [[nodiscard]] std::size_t subtree_end(std::size_t node) const noexcept;
    [[nodiscard]] std::optional<std::size_t> folder_of_row(std::size_t row) const;

    std::optional<FilePath> root_;
    std::vector<ExplorerNode> nodes_;
};
} // namespace nenenib::core
