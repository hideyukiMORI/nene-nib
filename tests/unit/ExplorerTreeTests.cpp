// scope `--explorer-tree` の単体テスト（ADR 0162 の決定 1・9・ADR 0042 決定 2）。
// 前半は契約（開閉・並び・2,000 件と「…」・親へ・読み直し・ルートの作り直し・
// 経路）、後半は性質: 固定 seed の決定的な操作の列（フォルダを読む・行を開閉する）
// を ExplorerTree と素朴な模型（経路 → 子の列と開閉）の両方に当て、各歩で次を見る。
//   1. visible_rows は模型を深さ優先でたどった行（経路・段・開閉）と一致する。
//   2. 開いていないフォルダの子は出ない（どの行も祖先がすべて開いている）。
//   3. 行の親の行はその行より前にあり、段は前の行の段 + 1 以下。
//   4. toggled を同じ行に 2 回当てると元の木（節の列）に戻る。
// 時計・ファイル・乱数の種の外部入力を使わない（std::mt19937_64 に固定 seed・剰余で範囲に畳む）。
#include "ExplorerEntry.hpp"
#include "ExplorerNode.hpp"
#include "ExplorerRow.hpp"
#include "ExplorerStep.hpp"
#include "ExplorerTree.hpp"
#include "FilePath.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
using core::ExplorerEntry;
using core::ExplorerRow;
using core::ExplorerTree;

// 50 倍の走りは rounds を 50 にして手元で回し、commit しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t property_cases = 200U * rounds;
constexpr std::size_t operation_limit = 40U;

core::FilePath path(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

ExplorerEntry folder(std::string name)
{
    return ExplorerEntry{std::move(name), true, false};
}

ExplorerEntry file(std::string name)
{
    return ExplorerEntry{std::move(name), false, false};
}

// 行の名前の列（「…」の行は "…"）。
std::vector<std::string> names_of(const ExplorerTree &tree)
{
    std::vector<std::string> names;
    for (const ExplorerRow &row : tree.visible_rows())
    {
        names.push_back(row.more ? std::string("…") : tree.nodes()[row.node].name);
    }
    return names;
}

constexpr std::size_t missing = std::numeric_limits<std::size_t>::max();

// 名前の行の位置。無ければ missing。
std::size_t row_named(const ExplorerTree &tree, std::string_view name)
{
    const auto rows = tree.visible_rows();
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        if (!rows[index].more && tree.nodes()[rows[index].node].name == name)
        {
            return index;
        }
    }
    return missing;
}

// 名前の行の節の番号。無ければ missing。
std::size_t node_named(const ExplorerTree &tree, std::string_view name)
{
    const auto row = row_named(tree, name);
    const auto rows = tree.visible_rows();
    return row < rows.size() ? rows[row].node : missing;
}

// 節の経路の文字列。無ければ空。
std::string path_text(const ExplorerTree &tree, std::size_t node)
{
    return tree.path_of(node)
        .transform([](const core::FilePath &found) { return std::string(found.text()); })
        .value_or(std::string{});
}

ExplorerTree sample_tree()
{
    const std::array<ExplorerEntry, 5> entries{file("b.txt"), folder("Src"), folder("a"),
                                               file("README"), ExplorerEntry{".git", true, true}};
    return ExplorerTree(path("C:\\work")).loaded(0, entries);
}

void verify_empty_and_root()
{
    const ExplorerTree empty;
    expect(!empty.root().has_value() && empty.visible_rows().empty() && empty.nodes().empty(),
           "a tree without a root has no rows");
    expect(empty.toggled(0).nodes().empty() && empty.collapsed_to_parent(0).row == 0,
           "rows of an empty tree change nothing");
    expect(!empty.path_of(0).has_value(), "an empty tree has no paths");
    const ExplorerTree rooted(path("C:\\work"));
    expect(rooted.root() == path("C:\\work") && rooted.nodes().size() == 1 &&
               rooted.nodes()[0].name == "work" && rooted.nodes()[0].open &&
               !rooted.nodes()[0].loaded && rooted.visible_rows().empty(),
           "a new root is one open, unread node without rows");
    expect(ExplorerTree(path("C:\\")).nodes()[0].name == "C:\\",
           "a root ending with a separator shows its whole path");
    const ExplorerTree rebuilt(path("D:\\other"));
    expect(rebuilt.root() == path("D:\\other") && rebuilt.visible_rows().empty(),
           "changing the root builds a new tree");
}

void verify_order()
{
    const auto tree = sample_tree();
    expect(names_of(tree) == std::vector<std::string>{".git", "a", "Src", "b.txt", "README"},
           "folders come first, then names in ASCII-folded byte order");
    expect(tree.nodes()[0].loaded && !tree.nodes()[0].truncated, "the root is read");
    const auto rows = tree.visible_rows();
    expect(std::ranges::all_of(rows, [](const ExplorerRow &row) { return row.depth == 0; }),
           "children of the root are at depth 0");
    expect(rows.at(0).hidden && rows.at(0).folder && !rows.at(1).hidden && !rows.at(3).folder,
           "rows carry the folder and hidden flags");
    const std::array<ExplorerEntry, 4> reversed{file("README"), file("b.txt"), folder("Src"),
                                                folder("a")};
    const std::array<ExplorerEntry, 4> shuffled{folder("a"), file("README"), folder("Src"),
                                                file("b.txt")};
    expect(names_of(ExplorerTree(path("C:\\w")).loaded(0, reversed)) ==
               names_of(ExplorerTree(path("C:\\w")).loaded(0, shuffled)),
           "the order does not depend on the order the OS returned");
    const std::array<ExplorerEntry, 3> cased{file("ab"), file("AB"), file("Aa")};
    expect(names_of(ExplorerTree(path("C:\\w")).loaded(0, cased)) ==
               std::vector<std::string>{"Aa", "AB", "ab"},
           "names equal after folding fall back to byte order");
}

void verify_open_and_close()
{
    auto tree = sample_tree();
    const auto a = row_named(tree, "a");
    tree = tree.toggled(a);
    expect(tree.visible_rows().at(a).open && names_of(tree).size() == 5,
           "opening an unread folder shows no children yet");
    const std::array<ExplorerEntry, 1> children{file("x.cpp")};
    tree = tree.loaded(node_named(tree, "a"), children);
    expect(names_of(tree) ==
               std::vector<std::string>{".git", "a", "x.cpp", "Src", "b.txt", "README"},
           "read children follow their open folder");
    expect(tree.visible_rows().at(2).depth == 1, "a child is one step deeper");
    expect(tree.path_of(tree.visible_rows().at(2).node) == path("C:\\work\\a\\x.cpp") &&
               tree.path_of(node_named(tree, "Src")) == path("C:\\work\\Src"),
           "paths join the names under the root with a backslash");
    const auto closed = tree.toggled(a);
    expect(names_of(closed).size() == 5 && !closed.visible_rows().at(a).open,
           "closing hides the children");
    expect(names_of(closed.toggled(a)) == names_of(tree), "reopening shows the kept children");
    expect(names_of(tree.toggled(row_named(tree, "b.txt"))) == names_of(tree) &&
               names_of(tree.toggled(99)) == names_of(tree),
           "a file row and a row out of range do not toggle");
    expect(ExplorerTree(path("C:\\")).loaded(0, children).path_of(1) == path("C:\\x.cpp"),
           "a root ending with a separator does not double it");
}

void verify_collapse_to_parent()
{
    auto tree = sample_tree();
    tree = tree.toggled(row_named(tree, "a"));
    const std::array<ExplorerEntry, 2> children{file("x.cpp"), file("y.cpp")};
    tree = tree.loaded(node_named(tree, "a"), children);
    const auto child = row_named(tree, "y.cpp");
    const auto up = tree.collapsed_to_parent(child);
    expect(up.row == row_named(tree, "a") && names_of(up.tree) == names_of(tree),
           "h on a child selects its folder and keeps the tree");
    const auto shut = tree.collapsed_to_parent(row_named(tree, "a"));
    expect(shut.row == row_named(tree, "a") && names_of(shut.tree).size() == 5,
           "h on an open folder closes it and keeps the row");
    const auto top = row_named(tree, "README");
    expect(tree.collapsed_to_parent(top).row == top && tree.collapsed_to_parent(99).row == 99,
           "h at the top level or out of range stays");
}

void verify_reload()
{
    auto tree = sample_tree();
    tree = tree.toggled(row_named(tree, "a"));
    const std::array<ExplorerEntry, 2> first{folder("deep"), file("x.cpp")};
    tree = tree.loaded(node_named(tree, "a"), first);
    tree = tree.toggled(row_named(tree, "deep"));
    const std::array<ExplorerEntry, 1> deeper{file("leaf")};
    tree = tree.loaded(node_named(tree, "deep"), deeper);
    expect(names_of(tree) == std::vector<std::string>{".git", "a", "deep", "leaf", "x.cpp", "Src",
                                                      "b.txt", "README"},
           "nested folders open depth first");
    const std::array<ExplorerEntry, 1> again{file("z.cpp")};
    tree = tree.loaded(node_named(tree, "a"), again);
    expect(names_of(tree) ==
               std::vector<std::string>{".git", "a", "z.cpp", "Src", "b.txt", "README"},
           "reading a folder again replaces its children and drops their subtrees");
    const auto src = node_named(tree, "Src");
    expect(tree.nodes()[src].parent == std::optional<std::size_t>{0} &&
               tree.path_of(src) == path("C:\\work\\Src") &&
               tree.node_of(path("C:\\work\\Src")) == std::optional<std::size_t>{src},
           "nodes after a reload keep their parents and paths");
    expect(!tree.node_of(path("C:\\work\\a\\deep")).has_value(), "dropped nodes are gone");
    const auto file_node = node_named(tree, "README");
    expect(names_of(tree.loaded(file_node, again)) == names_of(tree) &&
               names_of(tree.loaded(999, again)) == names_of(tree),
           "loading into a file or out of range changes nothing");
}

void verify_truncation()
{
    std::vector<ExplorerEntry> many;
    for (std::size_t index = ExplorerTree::entry_limit + 1; index > 0; --index)
    {
        std::string name = std::to_string(10000 + index - 1);
        many.push_back(file("f" + name));
    }
    auto tree = ExplorerTree(path("C:\\big")).loaded(0, many);
    const auto rows = tree.visible_rows();
    expect(tree.nodes()[0].truncated && rows.size() == ExplorerTree::entry_limit + 1,
           "2,000 entries are kept and the rest is marked truncated");
    expect(rows.back().more && rows.back().node == 0 && rows.back().depth == 0 &&
               tree.nodes()[rows.at(ExplorerTree::entry_limit - 1).node].name == "f11999",
           "the sorted tail is cut and a more row closes the root");
    std::vector<ExplorerEntry> nested{folder("a"), file("z")};
    tree = ExplorerTree(path("C:\\w")).loaded(0, nested);
    tree = tree.toggled(0).loaded(1, many);
    const auto inner = tree.visible_rows();
    expect(inner.size() == ExplorerTree::entry_limit + 3 &&
               inner.at(ExplorerTree::entry_limit + 1).more &&
               inner.at(ExplorerTree::entry_limit + 1).depth == 1 &&
               tree.nodes()[inner.back().node].name == "z",
           "a truncated folder ends with a more row before its next sibling");
    const auto up = tree.collapsed_to_parent(ExplorerTree::entry_limit + 1);
    expect(up.row == 0, "h on a more row selects its folder");
    expect(names_of(tree.toggled(ExplorerTree::entry_limit + 1)) == names_of(tree),
           "a more row does not toggle");
}

// ---------------------------------------------------------------- 性質

using Random = std::mt19937_64;
// 模型のフォルダ: 子（名前とフォルダか）・開いているか・読み終えたか。
using ModelChild = std::pair<std::string, bool>;
using ModelFolder = std::tuple<std::vector<ModelChild>, bool, bool>;
using Model = std::map<std::string, ModelFolder>;
// 模型の行: 経路・段・開いているか。
using ModelRow = std::tuple<std::string, std::size_t, bool>;

constexpr std::string_view property_root = "R:\\r";
constexpr std::array<std::string_view, 12> pool{"a",    "B",     "c1", "D_x", "e.txt", "F",
                                                "g.md", "H.cpp", "i",  "J2",  "k-k",   "Lm"};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::string folded(std::string_view text)
{
    std::string result(text);
    for (char &letter : result)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter = static_cast<char>(letter - 'A' + 'a');
        }
    }
    return result;
}

// 素朴な並び: フォルダが先、小文字にした名前、同じならそのままの名前。
[[nodiscard]] bool model_before(const ModelChild &left, const ModelChild &right)
{
    if (left.second != right.second)
    {
        return left.second;
    }
    if (folded(left.first) != folded(right.first))
    {
        return folded(left.first) < folded(right.first);
    }
    return left.first < right.first;
}

void model_rows(const Model &model, const std::string &folder_path, std::size_t depth,
                std::vector<ModelRow> &rows)
{
    const auto &[children, open, loaded] = model.at(folder_path);
    static_cast<void>(open);
    static_cast<void>(loaded);
    for (const auto &[name, is_folder] : children)
    {
        const std::string child = folder_path + "\\" + name;
        const bool child_open = is_folder && std::get<1>(model.at(child));
        rows.emplace_back(child, depth, child_open);
        if (child_open)
        {
            model_rows(model, child, depth + 1, rows);
        }
    }
}

[[nodiscard]] std::vector<ModelRow> expected_rows(const Model &model)
{
    std::vector<ModelRow> rows;
    model_rows(model, std::string(property_root), 0, rows);
    return rows;
}

[[nodiscard]] std::vector<ModelRow> actual_rows(const ExplorerTree &tree)
{
    std::vector<ModelRow> rows;
    for (const ExplorerRow &row : tree.visible_rows())
    {
        rows.emplace_back(path_text(tree, row.node), row.depth, row.open);
    }
    return rows;
}

// 経路 p の下（p 自身を含まない）を模型から外す。
void drop_below(Model &model, const std::string &folder_path)
{
    const std::string prefix = folder_path + "\\";
    std::erase_if(model, [&](const auto &entry) { return entry.first.starts_with(prefix); });
}

[[nodiscard]] std::vector<ExplorerEntry> random_entries(Random &random)
{
    std::vector<ExplorerEntry> entries;
    const std::size_t count = below(random, 7);
    std::vector<std::string_view> names(pool.begin(), pool.end());
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::size_t pick = below(random, names.size());
        entries.push_back(ExplorerEntry{std::string(names.at(pick)), below(random, 2) == 0,
                                        below(random, 5) == 0});
        names.erase(names.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    return entries;
}

void model_load(Model &model, const std::string &folder_path,
                const std::vector<ExplorerEntry> &entries)
{
    drop_below(model, folder_path);
    std::vector<ModelChild> children;
    for (const ExplorerEntry &entry : entries)
    {
        children.emplace_back(entry.name, entry.folder);
        if (entry.folder)
        {
            model[folder_path + "\\" + entry.name] = ModelFolder{{}, false, false};
        }
    }
    std::ranges::sort(children, model_before);
    auto &folder_state = model.at(folder_path);
    std::get<0>(folder_state) = std::move(children);
    std::get<2>(folder_state) = true;
}

// 行 index の親の行がそれより前にあるか（親がルートなら真）。
[[nodiscard]] bool parent_before(const ExplorerTree &tree, const std::vector<ExplorerRow> &rows,
                                 std::size_t index)
{
    const auto parent = tree.nodes()[rows[index].node].parent.value_or(0);
    if (parent == 0)
    {
        return true;
    }
    const auto head = rows.begin() + static_cast<std::ptrdiff_t>(index);
    return std::find_if(rows.begin(), head,
                        [&](const ExplorerRow &row) { return row.node == parent; }) != head;
}

// 節 node の祖先がどれも開いているか。
[[nodiscard]] bool ancestors_open(const ExplorerTree &tree, std::size_t node)
{
    auto parent = tree.nodes()[node].parent;
    while (parent.has_value())
    {
        const auto &above = tree.nodes()[parent.value_or(0)];
        if (!above.open)
        {
            return false;
        }
        parent = above.parent;
    }
    return true;
}

// 不変条件 2・3。破れの説明か空。
[[nodiscard]] std::string_view structure_break(const ExplorerTree &tree)
{
    const auto rows = tree.visible_rows();
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        if (index > 0 && rows[index].depth > rows[index - 1].depth + 1)
        {
            return "a row is more than one step deeper than the row before it";
        }
        if (!parent_before(tree, rows, index))
        {
            return "the parent row does not come before its child";
        }
        if (!ancestors_open(tree, rows[index].node))
        {
            return "a row under a closed folder is shown";
        }
    }
    return {};
}

[[nodiscard]] bool same_nodes(const ExplorerTree &left, const ExplorerTree &right)
{
    return std::ranges::equal(left.nodes(), right.nodes(),
                              [](const core::ExplorerNode &a, const core::ExplorerNode &b)
                              {
                                  return a.parent == b.parent && a.depth == b.depth &&
                                         a.name == b.name && a.folder == b.folder &&
                                         a.hidden == b.hidden && a.open == b.open &&
                                         a.loaded == b.loaded && a.truncated == b.truncated;
                              });
}

// フォルダの節を 1 つ選んで読む（見えていない節も選ぶ）。
void load_step(Random &random, ExplorerTree &tree, Model &model)
{
    std::vector<std::size_t> folders;
    for (std::size_t index = 0; index < tree.nodes().size(); ++index)
    {
        if (tree.nodes()[index].folder)
        {
            folders.push_back(index);
        }
    }
    const std::size_t node = folders.at(below(random, folders.size()));
    const auto entries = random_entries(random);
    const std::string folder_path = path_text(tree, node);
    tree = tree.loaded(node, entries);
    model_load(model, folder_path, entries);
}

// 行を 1 つ選んで開閉する（範囲の外の行も選ぶ）。模型はフォルダの行だけ入れ替える。
void toggle_step(Random &random, ExplorerTree &tree, Model &model)
{
    const auto rows = expected_rows(model);
    const std::size_t row = below(random, rows.size() + 1);
    tree = tree.toggled(row);
    if (row >= rows.size())
    {
        return;
    }
    const auto found = model.find(std::get<0>(rows.at(row)));
    if (found != model.end())
    {
        std::get<1>(found->second) = !std::get<1>(found->second);
    }
}

// 1 歩の後の不変条件（1〜4）。
[[nodiscard]] bool step_holds(Random &random, const ExplorerTree &tree, const Model &model)
{
    const auto rows = tree.visible_rows();
    if (!rows.empty())
    {
        const std::size_t row = below(random, rows.size());
        if (!same_nodes(tree.toggled(row).toggled(row), tree))
        {
            return false;
        }
    }
    return actual_rows(tree) == expected_rows(model) && structure_break(tree).empty();
}

// 1 つの操作の列。半分はフォルダを読み、半分は行を開閉する。
[[nodiscard]] bool case_holds(Random &random)
{
    ExplorerTree tree(path(property_root));
    Model model{{std::string(property_root), ModelFolder{{}, true, false}}};
    const std::size_t steps = 1 + below(random, operation_limit);
    for (std::size_t step = 0; step < steps; ++step)
    {
        if (below(random, 2) == 0)
        {
            load_step(random, tree, model);
        }
        else
        {
            toggle_step(random, tree, model);
        }
        if (!step_holds(random, tree, model))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::size_t broken_property_cases(std::uint64_t seed)
{
    Random random{seed};
    std::size_t broken = 0;
    for (std::size_t trial = 0; trial < property_cases; ++trial)
    {
        if (!case_holds(random))
        {
            ++broken;
        }
    }
    return broken;
}

void verify_property()
{
    expect(broken_property_cases(997U) == 0U,
           "random reads and toggles keep visible_rows depth first, hide children of closed "
           "folders, keep parents before children and toggle back to the same tree");
    expect(broken_property_cases(1044U) == 0U, "a second seed keeps the same invariants");
    // 模型が破れを見つけられること: 開閉を写さない模型は 1 回で食い違う。
    ExplorerTree tree(path(property_root));
    const std::array<ExplorerEntry, 1> one{folder("a")};
    tree = tree.loaded(0, one);
    Model model{{std::string(property_root), ModelFolder{{}, true, false}}};
    model_load(model, std::string(property_root), {one.begin(), one.end()});
    expect(actual_rows(tree.toggled(0)) != expected_rows(model),
           "the model notices a toggle it did not apply");
}
} // namespace

void verify_explorer_tree_scope()
{
    verify_empty_and_root();
    verify_order();
    verify_open_and_close();
    verify_collapse_to_parent();
    verify_reload();
    verify_truncation();
    verify_property();
}
} // namespace nenenib::tests
