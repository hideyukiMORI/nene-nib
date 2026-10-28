#include "ExplorerTree.hpp"

#include "AsciiCase.hpp"
#include "ExplorerStep.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// ルートの節に見せる名前。「C:\」のように区切りで終わる経路では最後の名前が空なので経路そのもの。
[[nodiscard]] std::string root_name(const FilePath &root)
{
    const std::string_view name = root.file_name();
    return std::string(name.empty() ? root.text() : name);
}

// 並び: フォルダが先、名前は ASCII の大小を畳んだバイト順（全順序）、同じ名前なら隠しが後。
[[nodiscard]] bool listed_before(const ExplorerEntry &left, const ExplorerEntry &right) noexcept
{
    if (left.folder != right.folder)
    {
        return left.folder;
    }
    if (left.name != right.name)
    {
        return ascii_folded_before(left.name, right.name);
    }
    return !left.hidden && right.hidden;
}

[[nodiscard]] ExplorerNode child_of(const ExplorerEntry &entry, std::size_t parent,
                                    std::size_t depth)
{
    return ExplorerNode{.parent = parent,
                        .depth = depth,
                        .name = entry.name,
                        .folder = entry.folder,
                        .hidden = entry.hidden,
                        .open = false,
                        .loaded = false,
                        .truncated = false};
}

[[nodiscard]] ExplorerRow row_of(std::size_t index, const ExplorerNode &node) noexcept
{
    return ExplorerRow{.node = index,
                       .depth = node.depth - 1,
                       .folder = node.folder,
                       .open = node.folder && node.open,
                       .hidden = node.hidden,
                       .more = false};
}

// 2,000 件を超えたフォルダの子の後ろの「…」の行。段は子と同じ。
[[nodiscard]] ExplorerRow more_row(std::size_t folder, const ExplorerNode &node) noexcept
{
    return ExplorerRow{.node = folder,
                       .depth = node.depth,
                       .folder = false,
                       .open = false,
                       .hidden = false,
                       .more = true};
}

// 段 depth 以上の末尾を捨てたフォルダの下が終わったので、深い方から「…」の行を置いて外す。
void close_truncated(std::vector<ExplorerRow> &rows, std::vector<std::size_t> &pending,
                     std::span<const ExplorerNode> nodes, std::size_t depth)
{
    while (!pending.empty() && depth <= nodes[pending.back()].depth)
    {
        rows.push_back(more_row(pending.back(), nodes[pending.back()]));
        pending.pop_back();
    }
}

[[nodiscard]] bool ends_with_separator(std::string_view text) noexcept
{
    return text.ends_with('\\') || text.ends_with('/');
}
} // namespace

ExplorerTree::ExplorerTree(FilePath root)
    : nodes_{ExplorerNode{.parent = std::nullopt,
                          .depth = 0,
                          .name = root_name(root),
                          .folder = true,
                          .hidden = false,
                          .open = true,
                          .loaded = false,
                          .truncated = false}}
{
    root_ = std::move(root);
}

const std::optional<FilePath> &ExplorerTree::root() const noexcept
{
    return root_;
}

std::span<const ExplorerNode> ExplorerTree::nodes() const noexcept
{
    return nodes_;
}

// node の下の節が終わる位置（node の後ろで最初に深さが node 以下になる節・無ければ列の末尾）。
std::size_t ExplorerTree::subtree_end(std::size_t node) const noexcept
{
    std::size_t end = node + 1;
    while (end < nodes_.size() && nodes_[end].depth > nodes_[node].depth)
    {
        ++end;
    }
    return end;
}

std::vector<ExplorerRow> ExplorerTree::visible_rows() const
{
    std::vector<ExplorerRow> rows;
    if (nodes_.empty())
    {
        return rows;
    }
    // 末尾を捨てた開いているフォルダの番号。その下の節が終わったら「…」の行を置く。
    std::vector<std::size_t> pending;
    if (nodes_.front().truncated)
    {
        pending.push_back(0);
    }
    std::size_t index = 1;
    while (index < nodes_.size())
    {
        const ExplorerNode &node = nodes_[index];
        close_truncated(rows, pending, nodes_, node.depth);
        rows.push_back(row_of(index, node));
        if (node.folder && !node.open)
        {
            index = subtree_end(index);
            continue;
        }
        if (node.folder && node.truncated)
        {
            pending.push_back(index);
        }
        ++index;
    }
    close_truncated(rows, pending, nodes_, 0);
    return rows;
}

std::optional<std::size_t> ExplorerTree::folder_of_row(std::size_t row) const
{
    const auto rows = visible_rows();
    if (row >= rows.size() || rows[row].more || !rows[row].folder)
    {
        return std::nullopt;
    }
    return rows[row].node;
}

ExplorerTree ExplorerTree::toggled(std::size_t row) const
{
    ExplorerTree next = *this;
    const auto folder = folder_of_row(row);
    if (folder.has_value())
    {
        ExplorerNode &node = next.nodes_[folder.value()];
        node.open = !node.open;
    }
    return next;
}

ExplorerStep ExplorerTree::collapsed_to_parent(std::size_t row) const
{
    const auto rows = visible_rows();
    if (row >= rows.size())
    {
        return ExplorerStep{*this, row};
    }
    const ExplorerRow &current = rows[row];
    if (current.open)
    {
        ExplorerTree next = *this;
        next.nodes_[current.node].open = false;
        return ExplorerStep{std::move(next), row};
    }
    // 「…」の行の親はそのフォルダ自身。ほかの行は節の親。
    const std::optional<std::size_t> parent =
        current.more ? std::optional<std::size_t>{current.node} : nodes_[current.node].parent;
    if (!parent.has_value() || parent.value() == 0)
    {
        return ExplorerStep{*this, row};
    }
    // 親の行は前順なので子の行より前にある。
    std::size_t target = row;
    while (target > 0)
    {
        --target;
        if (!rows[target].more && rows[target].node == parent.value())
        {
            return ExplorerStep{*this, target};
        }
    }
    return ExplorerStep{*this, row};
}

ExplorerTree ExplorerTree::loaded(std::size_t node, std::span<const ExplorerEntry> entries) const
{
    if (node >= nodes_.size() || !nodes_[node].folder)
    {
        return *this;
    }
    std::vector<ExplorerEntry> listed(entries.begin(), entries.end());
    std::ranges::sort(listed, listed_before);
    const bool truncated = listed.size() > entry_limit;
    if (truncated)
    {
        listed.resize(entry_limit);
    }
    const std::size_t end = subtree_end(node);
    const std::size_t removed = end - node - 1;
    ExplorerTree next;
    next.root_ = root_;
    next.nodes_.reserve(nodes_.size() - removed + listed.size());
    next.nodes_.insert(next.nodes_.end(), nodes_.begin(),
                       nodes_.begin() + static_cast<std::ptrdiff_t>(node + 1));
    next.nodes_.back().loaded = true;
    next.nodes_.back().truncated = truncated;
    for (const ExplorerEntry &entry : listed)
    {
        next.nodes_.push_back(child_of(entry, node, nodes_[node].depth + 1));
    }
    // 後ろの節は番号がずれる。親が捨てた範囲より後ろならずれの分だけ動かす（前なら同じ）。
    for (std::size_t index = end; index < nodes_.size(); ++index)
    {
        ExplorerNode moved = nodes_[index];
        if (moved.parent.has_value() && moved.parent.value() >= end)
        {
            moved.parent = moved.parent.value() - removed + listed.size();
        }
        next.nodes_.push_back(std::move(moved));
    }
    return next;
}

std::optional<FilePath> ExplorerTree::path_of(std::size_t node) const
{
    if (!root_.has_value() || node >= nodes_.size())
    {
        return std::nullopt;
    }
    std::vector<std::string_view> names;
    std::optional<std::size_t> at = node;
    while (at.has_value() && at.value() != 0)
    {
        names.push_back(nodes_[at.value()].name);
        at = nodes_[at.value()].parent;
    }
    std::string path(root_.value().text());
    for (auto name = names.rbegin(); name != names.rend(); ++name)
    {
        if (!ends_with_separator(path))
        {
            path += '\\';
        }
        path += *name;
    }
    auto parsed = FilePath::parse(path);
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

std::optional<std::size_t> ExplorerTree::node_of(const FilePath &path) const
{
    for (std::size_t index = 0; index < nodes_.size(); ++index)
    {
        if (path_of(index) == path)
        {
            return index;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
