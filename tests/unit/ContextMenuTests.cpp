// Issue #1051 / ADR 0078
// の追補。本文の右クリックのメニューの項目の列・使えるか・鍵の表示・配置だけを 測る（core
// の純関数）。時計・ファイル・乱数を使わない。
#include "ContextMenu.hpp"
#include "ContextMenuLayout.hpp"
#include "ContextMenuRequest.hpp"
#include "ContextMenuShape.hpp"
#include "ContextMenuSituation.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "LayoutRect.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::ContextMenuSituation;
using core::EditMode;
using core::EditorOperation;
using core::LayoutRect;

// 項目の順と区切り線（設計席の決め: 切り取り・コピー・貼り付け・削除｜すべて選択｜検索・置換）。
using OrderRow = std::pair<EditorOperation, bool>;
constexpr std::array<OrderRow, 7> menu_order{{
    {EditorOperation::cut, false},
    {EditorOperation::copy, false},
    {EditorOperation::paste, false},
    {EditorOperation::delete_selection, false},
    {EditorOperation::select_all, true},
    {EditorOperation::find, true},
    {EditorOperation::replace, false},
}};

// 操作 → 通常モードの鍵の表示・Vim の鍵の表示。
using KeyRow = std::tuple<EditorOperation, std::string_view, std::string_view>;
constexpr std::array<KeyRow, 7> menu_keys{{
    {EditorOperation::cut, "Ctrl+X", "d"},
    {EditorOperation::copy, "Ctrl+C", "y"},
    {EditorOperation::paste, "Ctrl+V", "p"},
    {EditorOperation::delete_selection, "Delete", "x"},
    {EditorOperation::select_all, "Ctrl+A", "ggVG"},
    {EditorOperation::find, "Ctrl+F", "/"},
    {EditorOperation::replace, "Ctrl+H", ":s"},
}};

// 状況 → 7 項目の使える / 使えない（menu_order の順）。
using EnabledRow = std::pair<ContextMenuSituation, std::array<bool, 7>>;
constexpr std::array<EnabledRow, 6> enabled_rows{{
    // 通常モードの選択あり: 全部。
    {{EditMode::ordinary, true, true}, {true, true, true, true, true, true, true}},
    // 選択なし: 切り取り・コピー・削除が薄い（貼り付けはクリップボードを読まずに試す）。
    {{EditMode::ordinary, false, true}, {false, false, true, false, true, true, true}},
    // 書き換えられない所（全面プレビュー）の選択あり: コピーと検索・置換だけ。
    {{EditMode::ordinary, true, false}, {false, true, false, false, false, true, true}},
    // Vim の VISUAL（OS の貼り付けをしない）: コピーと検索・置換。
    {{EditMode::vim, true, false}, {false, true, false, false, false, true, true}},
    // Vim の NORMAL: 検索・置換だけ。
    {{EditMode::vim, false, false}, {false, false, false, false, false, true, true}},
    // Vim の INSERT の選択あり: 全部。
    {{EditMode::vim, true, true}, {true, true, true, true, true, true, true}},
}};

void verify_order()
{
    static_assert(core::context_menu_size == menu_order.size());
    static_assert(core::context_menu_separator_count == 2);
    const auto items =
        core::context_menu_items(ContextMenuSituation{EditMode::ordinary, true, true});
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        expect(items.at(index).operation == menu_order.at(index).first,
               "the menu lists the operations in the order of the table column");
        expect(items.at(index).separated == menu_order.at(index).second,
               "a separator stands above the first row of each later group");
    }
}

void verify_enabled()
{
    for (const auto &[situation, expected] : enabled_rows)
    {
        const auto items = core::context_menu_items(situation);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            expect(items.at(index).enabled == expected.at(index),
                   "a row is enabled when the selection and editing it needs are there");
        }
    }
}

void verify_keys()
{
    for (const auto &[operation, ordinary, vim] : menu_keys)
    {
        expect(core::context_menu_key(operation, EditMode::ordinary) == ordinary,
               "the ordinary mode shows the key label of the table");
        expect(core::context_menu_key(operation, EditMode::vim) == vim,
               "the vim mode shows the vim keys of the table");
    }
}

// 800 × 600 の窓に 96 DPI で開く。
[[nodiscard]] core::ContextMenuLayout laid(std::int32_t x, std::int32_t y, std::int32_t name,
                                           std::int32_t key)
{
    return core::context_menu_layout(
        core::ContextMenuRequest{x, y, LayoutRect{0, 0, 800, 600}, name, key, 96});
}

void verify_layout_size()
{
    const auto narrow = laid(100, 100, 40, 30);
    expect(core::width_of(narrow.panel) == 200, "the menu is at least 200 DIP wide");
    // 上下の空き 4 + 7 行 × 30 + 区切り 2 本 × (4 + 1 + 4)。
    expect(core::height_of(narrow.panel) == 4 + (7 * 30) + (2 * 9) + 4,
           "the menu is as tall as the rows, the separators and the gaps");
    const auto wide = laid(100, 100, 200, 60);
    expect(core::width_of(wide.panel) == 12 + 200 + 24 + 60 + 12,
           "a wide menu fits the widest name and key with the paddings and the gap");
    const auto keyless = laid(100, 100, 220, 0);
    expect(core::width_of(keyless.panel) == 12 + 220 + 12, "no key leaves no gap");
    const auto scaled = core::context_menu_layout(
        core::ContextMenuRequest{100, 100, LayoutRect{0, 0, 800, 600}, 40, 30, 144});
    expect(core::width_of(scaled.panel) == 300 &&
               core::height_of(scaled.rows.at(0)) == core::to_pixels(30, 144),
           "the sizes follow the dpi");
    expect(narrow.rows.at(0).top == narrow.panel.top + 4 &&
               narrow.rows.at(4).top == narrow.rows.at(3).bottom + 9 &&
               narrow.separators.at(0).top == narrow.rows.at(3).bottom + 4 &&
               core::height_of(narrow.separators.at(0)) == 1 &&
               narrow.separators.at(0).left == narrow.panel.left + 12 &&
               narrow.separators.at(1).top == narrow.rows.at(4).bottom + 4,
           "the separators sit 4 DIP below the row above and are 1 DIP thick");
    const auto inside = core::context_menu_row_inside(narrow, 0);
    expect(inside.left == narrow.panel.left + 12 && inside.right == narrow.panel.right - 12,
           "the row inside leaves 12 DIP on both sides");
}

void verify_layout_place()
{
    const auto opened = laid(100, 100, 40, 30);
    expect(opened.panel.left == 100 && opened.panel.top == 100,
           "the menu opens to the lower right of the point");
    const auto height = core::height_of(opened.panel);
    const auto flipped = laid(700, 550, 40, 30);
    expect(flipped.panel.right == 700 && flipped.panel.bottom == 550,
           "the menu opens to the upper left when the right and bottom edges cut it");
    const auto crowded = core::context_menu_layout(
        core::ContextMenuRequest{150, 100, LayoutRect{0, 0, 300, height + 20}, 40, 30, 96});
    expect(crowded.panel.left == 100 && crowded.panel.top == 20,
           "a menu that fits neither way is pushed against the far edge inside the bounds");
    const auto tiny = core::context_menu_layout(
        core::ContextMenuRequest{50, 50, LayoutRect{10, 10, 100, 100}, 40, 30, 96});
    expect(tiny.panel.left == 10 && tiny.panel.top == 10,
           "a menu larger than the bounds starts at their upper left");
}

void verify_hit()
{
    const auto layout = laid(100, 100, 40, 30);
    expect(core::context_menu_hit(layout, 150, layout.rows.at(0).top) == std::size_t{0} &&
               core::context_menu_hit(layout, 150, layout.rows.at(6).bottom - 1) == std::size_t{6},
           "a point on a row hits that row");
    expect(!core::context_menu_hit(layout, 150, layout.separators.at(0).top).has_value() &&
               !core::context_menu_hit(layout, 150, layout.panel.top).has_value() &&
               !core::context_menu_hit(layout, 99, layout.rows.at(0).top).has_value(),
           "the separators, the gaps and the outside hit nothing");
}
} // namespace

void verify_context_menu_scope()
{
    verify_order();
    verify_enabled();
    verify_keys();
    verify_layout_size();
    verify_layout_place();
    verify_hit();
}
} // namespace nenenib::tests
