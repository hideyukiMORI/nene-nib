// scope `--minimap-property` の単体テスト（#1449・ADR 0171 の追補・ADR 0042 決定 2）。ミニマップを
// 出したまま、固定 seed の決定的な意図の列（打鍵・削除・貼り付け・全選択・undo / redo・行への移動・
// ホイールの送り・窓の寸法（0 × 0 を含む）・`:set minimap` の切り替え・`:syntax` の切り替え・タブの
// 開く / 切り替え / 閉じる・新しいタブ・枠の分割と切り替えと閉じる・全面プレビュー）を
// EditorController に流し、frame を作るたびに次を見る。
//   1. frame の minimap は controller の minimap_shown と同じときにだけ値を持ち、値があれば行の塊を
//      借りている。
//   2. 窓は文書の中（1 行目以上・最後の行が文書の行数以内）で、窓の行の数は塊の行の数と同じ。面の
//      最上段の行は窓の中。
//   3. 窓の行の塊は今の本文から作り直した塊と同じ桁を塗る（役割は字句の状態に依るので桁だけを
//      比べる。全部の行を比べると遅いので、帯の周りと窓の端と seed で選んだ行）。
//   4. renderer と同じ規則（前の絵の serial が base で帯があれば帯だけ・そうでなければ窓ごと）で
//      絵の写しを積むと、帯は前と今の行の数に収まり、積んだ写しは今の塊と同じ。
// frame は毎回は作らない（帯が何歩ぶんも重なる形を作る）。port は替え玉だけ。時計・ファイル・乱数の
// 種を読まない（std::mt19937_64 に固定 seed・剰余で範囲に畳む）。
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "DeleteDirection.hpp"
#include "DeleteUnit.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "HistoryDirection.hpp"
#include "LineNumber.hpp"
#include "Minimap.hpp"
#include "MinimapBand.hpp"
#include "MinimapBlock.hpp"
#include "MinimapFrame.hpp"
#include "MinimapRows.hpp"
#include "Offset.hpp"
#include "PlaceCaret.hpp"
#include "PreviewPlacement.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "SetPreviewPlacement.hpp"
#include "SplitLayout.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "Utf8.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

using Random = std::mt19937_64;
// renderer の絵の写し: 前に描いた塊の借り・その serial・積んだ行の塊。
using Picture = std::tuple<std::shared_ptr<const app::MinimapRows>, std::uint64_t,
                           std::vector<core::MinimapRow>>;
// 1 行の塗った桁の集まり（役割を見ない）。
using Columns = std::vector<bool>;
// 届いた状態の数: 面を出した frame・窓が 1 行目より下から始まる frame・帯を載せた frame。
using Reached = std::tuple<std::size_t, std::size_t, std::size_t>;

// Debug（ASan）で 1 seed 約 12 秒。nib_unit_scopes（--every-scope）だけが回す。
constexpr std::size_t seeds = 2U;
constexpr std::size_t steps_per_seed = 2'000U;
constexpr std::size_t sampled_rows = 24U;

constexpr std::array<std::string_view, 4> paths{"C:\\mm\\a.cpp", "C:\\mm\\b.cpp", "C:\\mm\\c.cpp",
                                                "C:\\mm\\d.md"};
constexpr std::array<std::string_view, 14> typed_texts{
    "a",  "Z",  " ",  "\t", "あ",           "\n",      "\n\n\n",
    "/*", "*/", "//", "\"", "int x = 1;\n", "e\u0301", "\r\n"};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] core::FilePath path_at(std::size_t index)
{
    auto parsed = core::FilePath::parse(paths.at(index));
    return std::move(parsed).value();
}

[[nodiscard]] std::string numbered(std::size_t count, std::size_t comment_every)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += line % comment_every == 0 ? "/* c\n" : "";
        // 字下げを行ごとに変え、行がずれた塊は桁の違いで見つかるようにする。
        text += std::string(line % 13U, ' ') + "int value_" + std::to_string(line) + " = " +
                std::to_string(line) + ";\n";
        text += line % comment_every == 0 ? "*/\n" : "";
    }
    return text;
}

// 4 つの本文: 短い・中くらい・窓（4,000 行）より長い・4 KiB を超える行と空行の Markdown。
void hold_files(Editing &editing)
{
    editing.files().hold_at(std::string(paths.at(0)), Bytes{"int a;\n/* x\ny */\n\n"});
    editing.files().hold_at(std::string(paths.at(1)), Bytes{numbered(300, 7)});
    editing.files().hold_at(std::string(paths.at(2)), Bytes{numbered(5'200, 50)});
    editing.files().hold_at(std::string(paths.at(3)),
                            Bytes{"# t\n\n" + std::string(9'000, 'w') + "\n\n- a\n"});
}

// 窓の寸法（0 × 0・縦か横だけ 0・面より狭い・普通・縦に長い）。
void resize(app::EditorController &controller, Random &random)
{
    constexpr std::array<std::pair<std::int32_t, std::int32_t>, 7> sizes{
        {{0, 0}, {800, 0}, {0, 600}, {40, 30}, {360, 200}, {800, 600}, {1200, 4000}}};
    const auto [width, height] = sizes.at(below(random, sizes.size()));
    const auto lines = static_cast<std::size_t>(height / 20);
    static_cast<void>(controller.apply(
        app::VisibleLines{lines, core::TextViewGeometry{std::max(width - 100, 0), 20, 96, 10.0F},
                          width, height, lines}));
}

void toggle_by_ex(app::EditorController &controller, std::string_view command)
{
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(run_ex(controller, std::string(command)));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::ordinary}));
}

void place(app::EditorController &controller, Random &random, bool extend)
{
    const std::size_t total = controller.text().line_count();
    const std::size_t line = 1U + below(random, total);
    static_cast<void>(controller.apply(app::PlaceCaret{
        core::TextPosition{core::LineNumber{line}, core::Column{1U + below(random, 6U)}},
        extend ? core::SelectionAnchoring::extend : core::SelectionAnchoring::collapse}));
}

using Act = void (*)(app::EditorController &, Random &);

void run_one(app::EditorController &controller, Random &random, std::span<const Act> acts)
{
    acts[below(random, acts.size())](controller, random);
}

void edit_step(app::EditorController &controller, Random &random)
{
    static constexpr std::array<Act, 7> acts{
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(
                app::InsertText{std::string(typed_texts.at(below(chance, typed_texts.size())))}));
        },
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(app::DeleteText{below(chance, 2U) == 0U
                                                               ? core::DeleteDirection::backward
                                                               : core::DeleteDirection::forward,
                                                           core::DeleteUnit::character}));
        },
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(
                app::ClipboardAction{below(chance, 2U) == 0U ? app::ClipboardOperation::cut
                                                             : app::ClipboardOperation::paste}));
        },
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(app::HistoryAction{below(chance, 2U) == 0U
                                                                  ? core::HistoryDirection::undo
                                                                  : core::HistoryDirection::redo}));
        },
        [](app::EditorController &target, Random &chance)
        { place(target, chance, below(chance, 2U) == 0U); },
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(
                app::MoveCaret{below(chance, 2U) == 0U ? core::CaretMotion::document_end
                                                       : core::CaretMotion::document_start,
                               below(chance, 2U) == 0U ? core::SelectionAnchoring::extend
                                                       : core::SelectionAnchoring::collapse}));
        },
        [](app::EditorController &target, Random &)
        { static_cast<void>(target.apply(app::SelectAll{})); }};
    run_one(controller, random, acts);
}

void tab_step(app::EditorController &controller, Random &random)
{
    static constexpr std::array<Act, 4> acts{
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(app::OpenDocument{
                path_at(below(chance, 2U) == 0U ? 2U : below(chance, paths.size()))}));
        },
        [](app::EditorController &target, Random &chance)
        { static_cast<void>(target.apply(app::SwitchTab{below(chance, 5U)})); },
        [](app::EditorController &target, Random &chance)
        { static_cast<void>(target.apply(app::CloseTab{below(chance, 5U)})); },
        [](app::EditorController &target, Random &)
        { static_cast<void>(target.apply(app::NewTab{})); }};
    run_one(controller, random, acts);
}

void view_step(app::EditorController &controller, Random &random)
{
    static constexpr std::array<Act, 9> acts{
        [](app::EditorController &target, Random &chance) { resize(target, chance); },
        [](app::EditorController &target, Random &chance)
        { toggle_by_ex(target, below(chance, 4U) == 0U ? "set nominimap" : "set minimap"); },
        [](app::EditorController &target, Random &chance)
        { toggle_by_ex(target, below(chance, 2U) == 0U ? "syntax off" : "syntax on"); },
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(app::SplitPane{below(chance, 2U) == 0U
                                                              ? core::SplitLayout::vertical
                                                              : core::SplitLayout::horizontal}));
        },
        [](app::EditorController &target, Random &)
        { static_cast<void>(target.apply(app::SwitchPane{})); },
        [](app::EditorController &target, Random &)
        { static_cast<void>(target.apply(app::ClosePane{})); },
        [](app::EditorController &target, Random &)
        { static_cast<void>(target.apply(app::SetPreviewPlacement{app::PreviewPlacement::full})); },
        [](app::EditorController &target, Random &)
        { static_cast<void>(target.apply(app::CancelSelection{})); },
        [](app::EditorController &target, Random &chance)
        {
            static_cast<void>(target.apply(
                app::ScrollLines{static_cast<std::int32_t>(below(chance, 400U)) - 200}));
        }};
    run_one(controller, random, acts);
}

void step(app::EditorController &controller, Random &random)
{
    const std::size_t family = below(random, 10U);
    if (family < 6U)
    {
        edit_step(controller, random);
    }
    else if (family < 7U)
    {
        tab_step(controller, random);
    }
    else
    {
        view_step(controller, random);
    }
}

// ---------------------------------------------------------------- 塊の桁（不変条件 3）

[[nodiscard]] Columns columns_of(const core::MinimapRow &row)
{
    Columns painted(core::minimap_columns, false);
    for (const auto &block : row)
    {
        for (std::size_t column = block.left;
             column < static_cast<std::size_t>(block.left) + block.width &&
             column < core::minimap_columns;
             ++column)
        {
            painted.at(column) = true;
        }
    }
    return painted;
}

// MinimapCache の行の読みと同じ形（4 KiB を超える行は code point の境で先頭だけ）で作った塊。
[[nodiscard]] core::MinimapRow fresh_row(const core::TextBuffer &text, core::LineNumber line)
{
    const core::Offset start = text.line_start(line);
    const core::Offset end = text.line_end(line);
    if (end.value - start.value <= core::minimap_line_bytes)
    {
        return core::minimap_row(text.text_range(start, end), {});
    }
    std::string head = text.text_range(start, core::Offset{start.value + core::minimap_line_bytes});
    head.resize(core::whole_code_points(head));
    return core::minimap_row(head, {});
}

[[nodiscard]] std::set<std::size_t> sampled(const app::MinimapFrame &minimap, Random &random)
{
    const std::size_t count = minimap.rows->rows.size();
    std::set<std::size_t> rows;
    if (count == 0U)
    {
        return rows;
    }
    rows.insert(0U);
    rows.insert(count - 1U);
    if (minimap.band.has_value())
    {
        const auto &band = minimap.band.value();
        for (std::size_t index = band.begin; index < band.begin + band.inserted + 2U; ++index)
        {
            rows.insert(std::min(index, count - 1U));
        }
    }
    for (std::size_t index = 0; index < sampled_rows; ++index)
    {
        rows.insert(below(random, count));
    }
    return rows;
}

[[nodiscard]] const char *window_violation(const app::EditorController &controller,
                                           const app::MinimapFrame &minimap)
{
    const auto &rows = *minimap.rows;
    const std::size_t total = controller.text().line_count();
    if (rows.window.rows != rows.rows.size())
    {
        return "the window rows differ from the row blocks";
    }
    if (rows.window.first.value < 1U || rows.window.first.value - 1U + rows.window.rows > total)
    {
        return "the window is outside the document";
    }
    if (rows.window.rows > 0U && (minimap.top.value < rows.window.first.value ||
                                  minimap.top.value >= rows.window.first.value + rows.window.rows))
    {
        return "the panel top is outside the window";
    }
    return nullptr;
}

[[nodiscard]] const char *rows_violation(const app::EditorController &controller,
                                         const app::MinimapFrame &minimap, Random &random)
{
    const auto &rows = *minimap.rows;
    for (const std::size_t index : sampled(minimap, random))
    {
        const core::LineNumber line{rows.window.first.value + index};
        if (columns_of(rows.rows.at(index)) != columns_of(fresh_row(controller.text(), line)))
        {
            return "a window row does not paint the columns of its line";
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------- 絵の写し（不変条件 4）

[[nodiscard]] const char *banded(Picture &picture, const app::MinimapRows &rows,
                                 const core::MinimapBand &band)
{
    auto &painted = std::get<2>(picture);
    const std::size_t now = rows.rows.size();
    const std::size_t before = painted.size();
    if (band.begin + band.inserted > now || band.begin + band.removed > before ||
        before + band.inserted != now + band.removed)
    {
        return "the band does not fit the rows before and after";
    }
    const auto at = static_cast<std::ptrdiff_t>(band.begin);
    painted.erase(painted.begin() + at,
                  painted.begin() + at + static_cast<std::ptrdiff_t>(band.removed));
    painted.insert(painted.begin() + at, rows.rows.begin() + at,
                   rows.rows.begin() + at + static_cast<std::ptrdiff_t>(band.inserted));
    return nullptr;
}

[[nodiscard]] const char *picture_violation(Picture &picture, const app::MinimapFrame &minimap)
{
    const auto &rows = *minimap.rows;
    auto &[held, serial, painted] = picture;
    const bool same = held == minimap.rows;
    if (same && serial == rows.serial)
    {
        return painted == rows.rows ? nullptr : "a held picture differs from its rows";
    }
    if (same && serial == minimap.base && minimap.band.has_value())
    {
        const char *broken = banded(picture, rows, minimap.band.value());
        if (broken != nullptr)
        {
            return broken;
        }
    }
    else
    {
        painted = rows.rows;
    }
    held = minimap.rows;
    serial = rows.serial;
    return painted == rows.rows ? nullptr : "the banded picture differs from the rows";
}

void count_reached(const app::EditorFrame &frame, Reached &reached)
{
    const bool shown = frame.minimap.has_value() && frame.minimap.value().rows != nullptr;
    std::get<0>(reached) += shown ? 1U : 0U;
    std::get<1>(reached) += shown && frame.minimap.value().rows->window.first.value > 1U ? 1U : 0U;
    std::get<2>(reached) += shown && frame.minimap.value().band.has_value() ? 1U : 0U;
}

[[nodiscard]] const char *frame_violation(app::EditorController &controller, Picture &picture,
                                          Random &random, Reached &reached)
{
    const auto frame = controller.frame();
    count_reached(frame, reached);
    if (frame.minimap.has_value() != controller.minimap_shown())
    {
        return "the frame minimap differs from minimap_shown";
    }
    if (!frame.minimap.has_value())
    {
        return nullptr;
    }
    const auto &minimap = frame.minimap.value();
    if (minimap.rows == nullptr)
    {
        return "a shown minimap borrows no rows";
    }
    const char *broken = window_violation(controller, minimap);
    if (broken == nullptr)
    {
        broken = rows_violation(controller, minimap, random);
    }
    return broken == nullptr ? picture_violation(picture, minimap) : broken;
}

// seed 1 本ぶん。最初に破れた不変条件の歩を出して、破れた数を返す。
[[nodiscard]] std::size_t broken_steps(std::uint64_t seed, Reached &reached)
{
    Editing editing{
        SettingsReading{std::optional{core::minimap_set(core::default_editor_settings(), true)}}};
    auto &controller = editing.controller();
    hold_files(editing);
    Random random{seed};
    resize(controller, random);
    static_cast<void>(controller.apply(app::OpenDocument{path_at(2U)}));
    Picture picture{nullptr, 0, {}};
    for (std::size_t index = 0; index < steps_per_seed; ++index)
    {
        step(controller, random);
        if (below(random, 3U) != 0U)
        {
            continue;
        }
        const char *broken = frame_violation(controller, picture, random, reached);
        if (broken != nullptr)
        {
            std::fprintf(stderr, "minimap property: seed %llu step %zu: %s\n",
                         static_cast<unsigned long long>(seed), index, broken);
            return 1U;
        }
    }
    return 0U;
}
} // namespace

void verify_minimap_property_scope()
{
    std::size_t broken = 0;
    Reached reached{0U, 0U, 0U};
    for (std::uint64_t seed = 1; seed <= seeds; ++seed)
    {
        broken += broken_steps(seed, reached);
    }
    std::fprintf(
        stderr,
        "minimap property: %zu frames with the minimap, %zu below line 1, %zu with a band\n",
        std::get<0>(reached), std::get<1>(reached), std::get<2>(reached));
    expect(std::get<0>(reached) > 0U && std::get<1>(reached) > 0U && std::get<2>(reached) > 0U,
           "the random intents show the minimap and move its window below line 1");
    expect(broken == 0U, "the minimap keeps its window, rows and bands over random intents");
}
} // namespace nenenib::tests
