// scope `--settings-panel` の単体テスト（施主決定 D102 の右の引き出し・ADR 0170 の決定 5・#1836）。
// 配置 core::settings_layout（幅 1280 の引き出し・700 DIP 未満の全幅・札の折り返し・高さが足りない
// ときの送りと切り詰め）・当たり settings_hit（✕・札・組・行・選ぶ欄・段の上下・領域の外へ送られた
// 行は当たらない）・焦点の追従とホイールとページの送り・行の焦点と送りの意図・操作 open_settings の
// 鍵。時計・ファイル・乱数の種を読まない。
#include "ChangeSetting.hpp"
#include "CloseSettings.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorOperation.hpp"
#include "FeatureCosts.hpp"
#include "FocusSettingsRow.hpp"
#include "KeyChord.hpp"
#include "LayoutRect.hpp"
#include "OpenSettings.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "Scopes.hpp"
#include "ScrollSettings.hpp"
#include "SettingsControl.hpp"
#include "SettingsItemKind.hpp"
#include "SettingsLayout.hpp"
#include "SettingsLayoutItem.hpp"
#include "SettingsLayoutRequest.hpp"
#include "SettingsLayoutRow.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPart.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsTarget.hpp"
#include "SettingsTextWidths.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::uint32_t dpi = 96;
// タイトルバー 40・ステータスバー 28 の窓（1280 × 800）の、面を置ける帯。
constexpr core::LayoutRect wide_area{0, 40, 1280, 772};
constexpr core::SettingsTextWidths short_widths{{40, 40, 40, 20, 40, 60}, {60, 60, 60}, 50, 40};

// 表の全部の行（探す欄が空のときに見えている行）。切れない機能は「常にオン」。
[[nodiscard]] std::vector<core::SettingsLayoutRow> every_row()
{
    std::vector<core::SettingsLayoutRow> rows;
    for (const auto &row : core::settings_rows)
    {
        const bool feature = std::holds_alternative<core::EditorFeature>(row.subject);
        const bool switchable =
            feature && core::feature_switchable(std::get<core::EditorFeature>(row.subject));
        rows.push_back(core::SettingsLayoutRow{row.section,
                                               core::settings_control_of(row.subject, switchable)});
    }
    return rows;
}

[[nodiscard]] core::SettingsLayout laid(std::span<const core::SettingsLayoutRow> rows,
                                        const core::LayoutRect &area, std::int32_t scroll)
{
    return core::settings_layout(
        core::SettingsLayoutRequest{area, dpi, rows, 0, short_widths, scroll});
}

[[nodiscard]] std::size_t count_of(const core::SettingsLayout &layout, core::SettingsItemKind kind)
{
    return static_cast<std::size_t>(
        std::ranges::count(layout.items, kind, &core::SettingsLayoutItem::kind));
}

[[nodiscard]] const core::SettingsLayoutItem &row_item(const core::SettingsLayout &layout,
                                                       std::size_t row)
{
    return *std::ranges::find_if(
        layout.items, [row](const core::SettingsLayoutItem &item)
        { return item.kind == core::SettingsItemKind::row && item.index == row; });
}

// 部品 control を持つ最初の行の番号。
[[nodiscard]] std::size_t row_with(std::span<const core::SettingsLayoutRow> rows,
                                   core::SettingsControl control)
{
    return static_cast<std::size_t>(
        std::ranges::find(rows, control, &core::SettingsLayoutRow::control) - rows.begin());
}

[[nodiscard]] core::SettingsTarget hit_centre(const core::SettingsLayout &layout,
                                              const core::LayoutRect &box)
{
    return core::settings_hit(layout, (box.left + box.right) / 2, (box.top + box.bottom) / 2);
}

// 幅 1280 の窓は右に 460 DIP の引き出し。見出し・探す欄・札・行の領域・下の帯の順に並ぶ。
void verify_drawer()
{
    const auto rows = every_row();
    const auto layout = laid(rows, wide_area, 0);
    expect(layout.panel == core::LayoutRect{820, 40, 1280, 772} && !layout.narrow &&
               layout.border == core::LayoutRect{820, 40, 821, 772},
           "a wide window puts a 460 DIP drawer at the right with a 1 DIP border");
    expect(layout.close == core::LayoutRect{1230, 56, 1262, 88} &&
               layout.search == core::LayoutRect{839, 98, 1262, 134},
           "the header holds the close mark and the search field sits under it");
    expect(layout.chips.at(0) == core::LayoutRect{839, 144, 901, 170} &&
               layout.chips.at(5).top == 144 && layout.divider.top == 180 &&
               layout.list.top == 181 && layout.list.bottom == 713,
           "the six chips fit one line and the list runs from the divider to the footer line");
    expect(layout.footer_line == core::LayoutRect{821, 713, 1280, 714} &&
               layout.close_button == core::LayoutRect{1178, 726, 1262, 760} &&
               layout.revert.right == 1170 && core::width_of(layout.revert) == 84,
           "the footer holds the two buttons at the right, each at least 84 DIP wide");
    expect(count_of(layout, core::SettingsItemKind::row) == rows.size() &&
               count_of(layout, core::SettingsItemKind::summary) == 1 &&
               count_of(layout, core::SettingsItemKind::empty) == 0 &&
               layout.items.front().kind == core::SettingsItemKind::heading &&
               layout.items.front().box.top == 189,
           "every row is listed under its section heading, with one summary for features");
    const auto &first = row_item(layout, 0);
    expect(core::height_of(first.box) == 56 && first.box.left == 831 && first.box.right == 1270,
           "a row is 56 DIP high and reaches 10 DIP from the drawer edges");
}

// 行の右端の部品の大きさと、要約の中の組のボタン。
void verify_controls()
{
    const auto rows = every_row();
    const auto layout = laid(rows, wide_area, 0);
    const auto &toggled = row_item(layout, row_with(rows, core::SettingsControl::toggle));
    const auto switch_box = core::settings_control_box(layout, toggled);
    expect(core::width_of(switch_box) == 40 && core::height_of(switch_box) == 22 &&
               switch_box.right == toggled.box.right - 12,
           "a switch is 40 x 22 DIP at the right inside of the row");
    const auto &stepped = row_item(layout, row_with(rows, core::SettingsControl::stepper));
    const auto value_box = core::settings_control_box(layout, stepped);
    const auto minus = core::settings_step_box(layout, value_box, false);
    const auto plus = core::settings_step_box(layout, value_box, true);
    expect(core::width_of(value_box) == 172 && core::height_of(value_box) == 30 &&
               minus.left == value_box.left + 2 && plus.right == value_box.right - 2 &&
               core::width_of(minus) == 28,
           "a stepper is 172 x 30 DIP with the minus and plus buttons at its ends");
    const auto summary = std::ranges::find(layout.items, core::SettingsItemKind::summary,
                                           &core::SettingsLayoutItem::kind);
    expect(layout.presets.at(0).left > summary->box.left &&
               layout.presets.at(2).right < summary->box.right &&
               layout.presets.at(0).bottom < summary->box.bottom &&
               layout.presets.at(1).left == layout.presets.at(0).right + 6,
           "the three preset buttons sit in a line inside the summary");
}

// 幅 700 DIP 未満の窓は全幅で、`:set` の名前を隠す印が立つ。
void verify_narrow()
{
    const auto rows = every_row();
    const auto narrow = laid(rows, core::LayoutRect{0, 40, 699, 772}, 0);
    expect(narrow.narrow && narrow.panel.left == 0 && core::width_of(narrow.border) == 0 &&
               narrow.search.left == 18,
           "a window under 700 DIP makes the panel full width without the border");
    const auto edge = laid(rows, core::LayoutRect{0, 40, 700, 772}, 0);
    expect(!edge.narrow && edge.panel.left == 240, "700 DIP still shows the drawer");
    const auto scaled = core::settings_layout(core::SettingsLayoutRequest{
        core::LayoutRect{0, 60, 1920, 1158}, 144, rows, 0, short_widths, 0});
    expect(scaled.panel.left == 1920 - 690 && !scaled.narrow &&
               core::height_of(row_item(scaled, 0).box) == 84,
           "at 150% the drawer and the rows scale with the DPI");
}

// 札は右端を越えると次の段へ折り返し、行の領域がその分だけ下がる。
void verify_chip_wrap()
{
    const auto rows = every_row();
    const core::SettingsTextWidths wide{{120, 120, 120, 120, 120, 120}, {60, 60, 60}, 50, 40};
    const auto layout =
        core::settings_layout(core::SettingsLayoutRequest{wide_area, dpi, rows, 0, wide, 0});
    expect(layout.chips.at(1).top == layout.chips.at(0).top &&
               layout.chips.at(2).top == layout.chips.at(0).top + 32 &&
               layout.chips.at(2).left == layout.chips.at(0).left &&
               layout.chips.at(4).top == layout.chips.at(0).top + 64,
           "chips wrap to the next line at the right edge");
    expect(layout.list.top == laid(rows, wide_area, 0).list.top + 64,
           "the list starts below the wrapped chips");
}

// 高さが足りなければ行の領域だけを送る。送りは 0 から上限へ切り詰める。
void verify_scroll()
{
    const auto rows = every_row();
    const core::LayoutRect small{0, 40, 1280, 440};
    const auto top = laid(rows, small, 0);
    expect(top.scroll_limit > 0 && top.scroll == 0 && laid(rows, wide_area, 0).scroll_limit >= 0,
           "a short window leaves rows below the list");
    const auto bottom = laid(rows, small, 100000);
    expect(bottom.scroll == top.scroll_limit && laid(rows, small, -5).scroll == 0 &&
               bottom.items.back().box.bottom + 16 == bottom.list.bottom &&
               bottom.search == top.search && bottom.close_button == top.close_button,
           "the scroll is clamped and only the list moves");
    const std::size_t last = rows.size() - 1;
    const auto revealed = core::settings_scroll_revealing(top, last);
    const auto shown = laid(rows, small, revealed);
    expect(revealed > 0 && row_item(shown, last).box.bottom <= shown.list.bottom &&
               row_item(shown, last).box.top >= shown.list.top,
           "revealing the last row scrolls just enough to show it");
    expect(core::settings_scroll_revealing(bottom, 0) == 0 &&
               core::settings_scroll_revealing(top, 0) == 0,
           "revealing the first row of a section also shows its heading");
    expect(core::settings_scroll_by(top, 3) == 60 && core::settings_scroll_by(top, -3) == 0 &&
               core::settings_scroll_by(bottom, 3) == bottom.scroll,
           "the wheel moves 20 DIP a line and stops at the ends");
    expect(core::settings_paged_row(top, std::size_t{0}, true).value_or(0) > 0 &&
               core::settings_paged_row(top, last, true) == std::optional{last} &&
               core::settings_paged_row(top, std::size_t{0}, false) ==
                   std::optional<std::size_t>{0} &&
               core::settings_paged_row(top, std::nullopt, true) == std::optional<std::size_t>{0},
           "paging moves the focus by a list height and stops at the ends");
}

// 当たりは描画と同じ矩形から引く。
void verify_hit()
{
    const auto rows = every_row();
    const auto layout = laid(rows, wide_area, 0);
    expect(core::settings_hit(layout, 400, 300).part == core::SettingsPart::outside &&
               core::settings_hit(layout, 825, 60).part == core::SettingsPart::panel &&
               hit_centre(layout, layout.close).part == core::SettingsPart::close &&
               hit_centre(layout, layout.search).part == core::SettingsPart::search &&
               hit_centre(layout, layout.chips.at(4)) ==
                   core::SettingsTarget{core::SettingsPart::chip, 4} &&
               hit_centre(layout, layout.revert).part == core::SettingsPart::revert &&
               hit_centre(layout, layout.close_button).part == core::SettingsPart::close_button,
           "the header, the chips and the footer buttons are hit by their boxes");
    // 組のボタンは行の領域に見えているときだけ当たる（機能の節の最初の行が見える所へ送る）。
    const auto featured =
        static_cast<std::size_t>(std::ranges::find(rows, core::SettingsSection::features,
                                                   &core::SettingsLayoutRow::section) -
                                 rows.begin());
    const auto shown = laid(rows, wide_area, core::settings_scroll_revealing(layout, featured));
    expect(hit_centre(layout, layout.presets.at(2)).part != core::SettingsPart::preset &&
               hit_centre(shown, shown.presets.at(2)) ==
                   core::SettingsTarget{core::SettingsPart::preset, 2},
           "a preset button is hit only while the summary is inside the list");
    const auto stepper = row_with(rows, core::SettingsControl::stepper);
    const auto &stepped = row_item(layout, stepper);
    const auto box = core::settings_control_box(layout, stepped);
    expect(hit_centre(layout, core::settings_step_box(layout, box, false)) ==
                   core::SettingsTarget{core::SettingsPart::row_minus, stepper} &&
               hit_centre(layout, core::settings_step_box(layout, box, true)) ==
                   core::SettingsTarget{core::SettingsPart::row_plus, stepper} &&
               hit_centre(layout, box) == core::SettingsTarget{core::SettingsPart::row, stepper},
           "a stepper is hit on its minus and plus buttons");
    const auto choice = row_with(rows, core::SettingsControl::choice);
    const auto &chosen = row_item(layout, choice);
    expect(hit_centre(layout, core::settings_control_box(layout, chosen)) ==
                   core::SettingsTarget{core::SettingsPart::row_value, choice} &&
               core::settings_hit(layout, chosen.box.left + 20, chosen.box.top + 20) ==
                   core::SettingsTarget{core::SettingsPart::row, choice},
           "a choice row is hit on its value box and on the row itself");
    // 上へ送られて見出しの行の下に隠れた行は当たらない。
    const core::LayoutRect small{0, 40, 1280, 440};
    const auto bottom = laid(rows, small, 100000);
    const auto &hidden = row_item(bottom, 0);
    expect(hidden.box.bottom <= bottom.list.top &&
               core::settings_hit(bottom, hidden.box.left + 20, hidden.box.top + 20).part !=
                   core::SettingsPart::row,
           "a row scrolled out of the list is not hit");
}

// 当たる行が無ければ 1 行の知らせだけ。命令の行は言語サーバーの節の後ろ。
void verify_empty_and_commands()
{
    const auto none = laid({}, wide_area, 0);
    expect(none.items.size() == 1 && none.items.front().kind == core::SettingsItemKind::empty &&
               none.scroll_limit == 0 && none.presets.at(0) == core::LayoutRect{0, 0, 0, 0},
           "no visible row leaves one line for the message");
    const auto rows = every_row();
    const auto served = core::settings_layout(
        core::SettingsLayoutRequest{wide_area, dpi, rows, 2, short_widths, 0});
    expect(count_of(served, core::SettingsItemKind::command) == 2 &&
               served.items.back().kind == core::SettingsItemKind::command,
           "the command lines follow the language server section");
    const std::span<const core::SettingsLayoutRow> head{rows.data(), 2};
    const auto filtered = core::settings_layout(
        core::SettingsLayoutRequest{wide_area, dpi, head, 2, short_widths, 0});
    expect(count_of(filtered, core::SettingsItemKind::command) == 0,
           "the command lines are hidden with the language server section");
}

[[nodiscard]] const app::SettingsView &panel_of(const app::EditorFrame &frame)
{
    static const app::SettingsView empty{};
    return frame.settings_panel.has_value() ? frame.settings_panel.value() : empty;
}

// 行の焦点と送りの意図。面は開いたままで、保存は閉じるときの 1 回だけ。本文は変わらない。
void verify_intents()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    const auto focused = controller.apply_frame(app::FocusSettingsRow{3});
    expect(panel_of(focused).focused == std::optional<std::size_t>{3},
           "FocusSettingsRow puts the focus on the visible row");
    const auto last = controller.apply_frame(app::FocusSettingsRow{1000});
    expect(panel_of(last).focused == std::optional{panel_of(last).rows.size() - 1},
           "a row past the end focuses the last row");
    const auto scrolled = controller.apply_frame(app::ScrollSettings{120});
    const auto clamped = controller.apply_frame(app::ScrollSettings{-4});
    expect(panel_of(scrolled).scroll == 120 && panel_of(clamped).scroll == 0 &&
               clamped.settings_panel.has_value() && editing.settings().writes() == 0,
           "ScrollSettings keeps the panel open and stores the scroll without writing");
    static_cast<void>(controller.apply(app::FocusSettingsRow{0}));
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto closed = controller.apply_frame(app::CloseSettings{});
    expect(!closed.settings_panel.has_value() && editing.settings().writes() == 1 &&
               controller.text().size_bytes() == 0,
           "closing after a change writes once and the text is untouched");
    static_cast<void>(controller.apply(app::ScrollSettings{10}));
    static_cast<void>(controller.apply(app::FocusSettingsRow{1}));
    expect(!controller.settings_open() && editing.settings().writes() == 1,
           "the panel intents do nothing while the panel is closed");
}

// 操作「設定」は Ctrl+, で両方のモード。
void verify_operation()
{
    const core::KeyChord chord{true, false, core::OperationKey::comma};
    expect(core::operation_for(chord, core::EditMode::ordinary) ==
                   std::optional{core::EditorOperation::open_settings} &&
               core::operation_for(chord, core::EditMode::vim) ==
                   std::optional{core::EditorOperation::open_settings} &&
               core::key_chord_label(chord) == "Ctrl+," &&
               core::shown_chord(core::EditorOperation::open_settings, core::EditMode::vim)
                   .has_value(),
           "Ctrl+, opens the settings in both modes");
}
} // namespace

void verify_settings_panel_scope()
{
    verify_drawer();
    verify_controls();
    verify_narrow();
    verify_chip_wrap();
    verify_scroll();
    verify_hit();
    verify_empty_and_commands();
    verify_intents();
    verify_operation();
}
} // namespace nenenib::tests
