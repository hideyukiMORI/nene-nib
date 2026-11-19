#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 文書ごとのプレビューの表の横の送りの量（#1883・施主決定 D107・ADR 0151 の 2026-11-19 の追補）。
// offsets は表の通し番号（core::PreviewTableRow::table）ごとの送りの量（DIP・送っていない表は 0・
// 列は送った表の番号まで）。tables は送りを置いたときの結果の表の数で、再解析の結果の表の数が
// 違えば送りを全部捨てる（table_scrolls_for）。端へ寄せるのは読む側（core の
// preview_table_scroll_clamped）。文書の値で、タブを替えても束に乗って残る（DocumentState）。
// どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct PreviewTableScrolls
{
    std::size_t tables = 0;
    std::vector<std::int32_t> offsets{};
};

[[nodiscard]] inline bool operator==(const PreviewTableScrolls &left,
                                     const PreviewTableScrolls &right) noexcept
{
    return left.tables == right.tables && left.offsets == right.offsets;
}

// 表 table の送りの量（置いていない表は 0）。
[[nodiscard]] inline std::int32_t table_scroll_of(const PreviewTableScrolls &scrolls,
                                                  std::size_t table) noexcept
{
    return table < scrolls.offsets.size() ? scrolls.offsets.at(table) : 0;
}

// 表 table の送りの量を offset にした値（tables は表の数・範囲の外の表は置かない）。
[[nodiscard]] inline PreviewTableScrolls table_scroll_set(PreviewTableScrolls scrolls,
                                                          std::size_t tables, std::size_t table,
                                                          std::int32_t offset)
{
    if (scrolls.tables != tables)
    {
        scrolls = PreviewTableScrolls{tables, {}};
    }
    if (table >= tables)
    {
        return scrolls;
    }
    if (scrolls.offsets.size() <= table)
    {
        scrolls.offsets.resize(table + 1U, 0);
    }
    scrolls.offsets.at(table) = std::max(offset, 0);
    return scrolls;
}

// 表の数 tables の結果に差し替えたときの送り（表の数が同じなら残し、違えば全部 0 に戻す）。
[[nodiscard]] inline PreviewTableScrolls table_scrolls_for(PreviewTableScrolls scrolls,
                                                           std::size_t tables)
{
    if (scrolls.tables == tables)
    {
        return scrolls;
    }
    return PreviewTableScrolls{tables, {}};
}
} // namespace nenenib::application
