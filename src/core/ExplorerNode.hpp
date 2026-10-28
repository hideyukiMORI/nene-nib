#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace nenenib::core
{
// エクスプローラーの木の 1 つの節（ADR 0162 の決定 1）。木は節を深さ優先の前順に並べた列で持ち、
// 0 番がルート（深さ 0・いつも開いている）。parent は親の節の番号（ルートは値なし）、name は
// フォルダかファイルの名前（ルートは経路）。open はフォルダが開いているか、loaded は子を読み終えた
// か、truncated は子が 2,000 件を超えて末尾を捨てたか。どの欄も単独で妥当な値なので公開 aggregate
// （CPP-003）。節を作り変えるのは ExplorerTree だけ。
struct ExplorerNode
{
    std::optional<std::size_t> parent;
    std::size_t depth;
    std::string name;
    bool folder;
    bool hidden;
    bool open;
    bool loaded;
    bool truncated;
};
} // namespace nenenib::core
