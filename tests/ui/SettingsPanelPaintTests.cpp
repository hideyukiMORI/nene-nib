// #1836（施主決定 D102・ADR 0170 の決定 5）: 設定の面（右の引き出し）の画素と鍵の振り分け。WIC の
// bitmap（SoftwareTarget）に SettingsPanelPainter で描き、面の地が panel・左の境が panel_border の
// 1 DIP・面の外には何も描かない（暗幕なし）・焦点の行に current_line の面と text
// の枠（#1929）・スイッチのオンは accent で つまみが右・オフは toggle でつまみが左・段の印は high
// だけが heavy の色・細い窓（700 DIP 未満）は 全幅で `:set`
// の名前を描かない・文字組みの保持が描き直しで増えないことを、ダークとライトで
// 確かめる。配置は製品と同じ core::settings_layout。bitmap は 80 画素の高さなので、帯を平行移動して
// 見たい行を bitmap に入れる。窓・device は作らない。字の画素は字体に依るので、Segoe UI と Consolas
// が無い機械では測らない（QLT-013）。鍵の表（settings_key_action）は窓を作らずに確かめる。
#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CostTier.hpp"
#include "DirectWriteFactory.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "EditorFeature.hpp"
#include "EditorOperation.hpp"
#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"
#include "LayoutRect.hpp"
#include "OperationBindings.hpp"
#include "OperationShortcut.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "RgbColor.hpp"
#include "SettingsChoiceLayout.hpp"
#include "SettingsChoiceView.hpp"
#include "SettingsChoices.hpp"
#include "SettingsItemKind.hpp"
#include "SettingsKeyAction.hpp"
#include "SettingsKeys.hpp"
#include "SettingsLayout.hpp"
#include "SettingsLayoutItem.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPanelFormats.hpp"
#include "SettingsPanelPainter.hpp"
#include "SettingsPanelText.hpp"
#include "SettingsRow.hpp"
#include "SettingsRowView.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSectionTab.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using nenenib::ui::win32::PaletteRowLayouts;
using nenenib::ui::win32::SettingsKeyAction;
using nenenib::ui::win32::SettingsPanelFormats;
using nenenib::ui::win32::SettingsPanelPainter;
using nenenib::ui::win32::SettingsPanelText;
using Pixels = SoftwareTarget::Pixels;

constexpr std::uint32_t dpi = 96;
constexpr std::size_t channels = 4;
constexpr std::int32_t strip_height = 80;
// 帯の高さ（行が全部入る）と、見たい行の上端を置く bitmap の y。
constexpr std::int32_t band_height = 2000;
constexpr std::int32_t shown_top = 10;
constexpr std::int32_t wide_width = 1280;
constexpr std::int32_t narrow_width = 520;
constexpr core::RgbColor heavy{255, 107, 107};
constexpr core::RgbColor black{0, 0, 0};
// 見本の行: 配色（選ぶ欄）・折り返し（オン）・操作の案内（オフ）・字の大きさ（段の上下）・括弧の
// 相手（常にオン・low）・言語サーバー（オン・high・裏）。
constexpr std::size_t wrap_row = 1;
constexpr std::size_t guide_row = 2;
constexpr std::size_t matchparen_row = 4;
constexpr std::size_t lsp_row = 5;

[[nodiscard]] std::size_t index_of(std::int32_t x, std::int32_t y) noexcept
{
    return ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
           channels;
}

[[nodiscard]] bool pixel_is(const Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at = index_of(x, y);
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

// 矩形の中（bitmap に入る所）で color の画素の数。
[[nodiscard]] std::size_t painted_with(const Pixels &pixels, const core::LayoutRect &area,
                                       core::RgbColor color)
{
    std::size_t count = 0;
    for (auto y = std::max(area.top, 0); y < std::min(area.bottom, strip_height); ++y)
    {
        for (auto x = std::max(area.left, 0); x < area.right; ++x)
        {
            count += pixel_is(pixels, x, y, color) ? 1U : 0U;
        }
    }
    return count;
}

[[nodiscard]] app::SettingsRowView row_of(core::SettingsSection section,
                                          const core::SettingsSubject &subject)
{
    return app::SettingsRowView{core::SettingsRow{section, subject},
                                core::settings_row_title(subject),
                                core::settings_row_description(subject),
                                core::settings_row_set_name(subject),
                                false,
                                true,
                                {},
                                std::nullopt,
                                false};
}

[[nodiscard]] app::SettingsView sample_view(std::optional<std::size_t> focused)
{
    app::SettingsView view{};
    for (std::size_t index = 0; index < core::settings_sections.size(); ++index)
    {
        const auto section = core::settings_sections.at(index);
        view.tabs.at(index) =
            app::SettingsSectionTab{section, core::settings_section_title(section), true};
    }
    view.rows.push_back(row_of(core::SettingsSection::display, core::SettingsValue::theme));
    view.rows.back().value = "ubuntu-aubergine";
    view.rows.push_back(row_of(core::SettingsSection::display, core::EditorFeature::wrap));
    view.rows.back().on = true;
    view.rows.push_back(row_of(core::SettingsSection::display, core::EditorFeature::guide));
    view.rows.push_back(row_of(core::SettingsSection::text, core::SettingsValue::font_size));
    view.rows.back().value = "13 pt";
    view.rows.push_back(row_of(core::SettingsSection::features, core::EditorFeature::matchparen));
    view.rows.back().on = true;
    view.rows.back().switchable = false;
    view.rows.back().tier = core::CostTier::low;
    view.rows.push_back(row_of(core::SettingsSection::features, core::EditorFeature::lsp));
    view.rows.back().on = true;
    view.rows.back().tier = core::CostTier::high;
    view.rows.back().background = true;
    view.focused = focused;
    view.section = core::SettingsSection::display;
    view.summary.estimate_us = 841;
    view.summary.high = 1;
    view.summary.heavy.push_back(core::EditorFeature::lsp);
    return view;
}

// 面を幅 width の窓に描く。見たい矩形の上端が bitmap の shown_top に来るよう帯をずらす。
class PanelStrip final
{
  public:
    PanelStrip(SoftwareTarget &target, const SettingsPanelText &text, PaletteRowLayouts &layouts,
               const core::Palette &palette)
        : target_(target), text_(text), layouts_(layouts), palette_(palette)
    {
    }
    // 帯の上端を top に置いた配置。
    [[nodiscard]] core::SettingsLayout layout_of(const app::SettingsView &view, std::int32_t width,
                                                 std::int32_t top) const
    {
        core::SettingsLayout layout{};
        static_cast<void>(target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                const SettingsPanelPainter painter(surface, *brush, text_, layouts_);
                layout = painter.arranged(view, core::LayoutRect{0, top, width, top + band_height});
            }));
        return layout;
    }
    [[nodiscard]] Pixels operator()(const app::SettingsView &view, std::int32_t width,
                                    std::int32_t top) const
    {
        const auto layout = layout_of(view, width, top);
        return target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                SettingsPanelPainter painter(surface, *brush, text_, layouts_);
                painter.paint(view, layout, palette_, heavy);
            });
    }
    // 見えている行 row の上端が bitmap の shown_top に来る帯の上端。
    // 帯の上端を top に置いた選ぶ欄の一覧の配置（幅 wide_width・#1926）。
    [[nodiscard]] std::optional<core::SettingsChoiceLayout>
    choice_layout_of(const app::SettingsView &view, std::int32_t top) const
    {
        const auto layout = layout_of(view, wide_width, top);
        std::optional<core::SettingsChoiceLayout> choice;
        static_cast<void>(target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                const SettingsPanelPainter painter(surface, *brush, text_, layouts_);
                choice = painter.arranged_choice(view, layout);
            }));
        return choice;
    }
    [[nodiscard]] std::int32_t top_showing(const app::SettingsView &view, std::int32_t width,
                                           std::size_t row) const
    {
        return shown_top - row_box(layout_of(view, width, 0), row).box.top;
    }
    [[nodiscard]] static const core::SettingsLayoutItem &row_box(const core::SettingsLayout &layout,
                                                                 std::size_t row)
    {
        return *std::ranges::find_if(
            layout.items, [row](const core::SettingsLayoutItem &item)
            { return item.kind == core::SettingsItemKind::row && item.index == row; });
    }

  private:
    SoftwareTarget &target_;
    const SettingsPanelText &text_;
    PaletteRowLayouts &layouts_;
    const core::Palette &palette_;
};

// 面の地・左の境・面の外（暗幕なし）・文字組みの保持。
void verify_panel(const PanelStrip &strip, PaletteRowLayouts &layouts, const core::Palette &palette)
{
    const auto view = sample_view(std::nullopt);
    const auto layout = strip.layout_of(view, wide_width, 0);
    const auto pixels = strip(view, wide_width, 0);
    expect(layout.panel.left == wide_width - 460 &&
               pixel_is(pixels, layout.panel.left + 6, 4, palette.panel) &&
               pixel_is(pixels, layout.panel.right - 2, 4, palette.panel),
           "the drawer is filled with the panel color");
    expect(pixel_is(pixels, layout.panel.left, 4, palette.panel_border) &&
               pixel_is(pixels, layout.panel.left - 1, 4, black) &&
               pixel_is(pixels, layout.panel.left - 40, 40, black),
           "the left edge is a 1 DIP panel border and nothing is drawn outside the drawer");
    expect(painted_with(pixels, layout.header, palette.text) > 0 &&
               pixel_is(pixels, layout.search.left + 4,
                        (layout.search.top + layout.search.bottom) / 2, palette.background),
           "the title is written and the search field has the background face");
    const auto held = layouts.size();
    static_cast<void>(strip(view, wide_width, 0));
    expect(held > 0 && held <= SettingsPanelPainter::retained && layouts.size() == held,
           "drawing the same panel again makes no new layouts");
}

// 焦点の行の面と、スイッチのオン・オフ。
void verify_rows(const PanelStrip &strip, const core::Palette &palette)
{
    const auto plain = sample_view(std::nullopt);
    const auto top = strip.top_showing(plain, wide_width, wrap_row);
    const auto layout = strip.layout_of(plain, wide_width, top);
    const auto &on_row = PanelStrip::row_box(layout, wrap_row);
    const auto &off_row = PanelStrip::row_box(layout, guide_row);
    const auto probe_x = on_row.box.left + 5;
    const auto probe_y = on_row.box.top + 28;
    const auto unfocused = strip(plain, wide_width, top);
    const auto focused = strip(sample_view(wrap_row), wide_width, top);
    expect(on_row.box.top == shown_top && pixel_is(unfocused, probe_x, probe_y, palette.panel) &&
               pixel_is(focused, probe_x, probe_y, palette.current_line) &&
               pixel_is(focused, probe_x, on_row.box.top - 4, palette.panel) &&
               off_row.box.top == on_row.box.bottom,
           "the focused row lays the current line face and the others stay on the panel");
    // #1929: 設定の行の焦点も text の 1 DIP の枠（言語サーバーの行と同じ形）。
    expect(pixel_is(focused, on_row.box.left + 20, on_row.box.top, palette.text) &&
               !pixel_is(unfocused, on_row.box.left + 20, on_row.box.top, palette.text),
           "the focused settings row has a text frame and an unfocused row has none");
    const auto on_box = core::settings_control_box(layout, on_row);
    const auto on_middle = (on_box.top + on_box.bottom) / 2;
    expect(pixel_is(unfocused, on_box.left + 8, on_middle, palette.accent) &&
               pixel_is(unfocused, on_box.right - 11, on_middle, palette.on_accent),
           "a switch that is on has the accent face with the knob at the right");
    const auto off_top = strip.top_showing(plain, wide_width, guide_row);
    const auto off_layout = strip.layout_of(plain, wide_width, off_top);
    const auto off_box =
        core::settings_control_box(off_layout, PanelStrip::row_box(off_layout, guide_row));
    const auto off_middle = (off_box.top + off_box.bottom) / 2;
    const auto off_pixels = strip(plain, wide_width, off_top);
    expect(pixel_is(off_pixels, off_box.right - 8, off_middle, palette.toggle) &&
               pixel_is(off_pixels, off_box.left + 11, off_middle, palette.on_accent),
           "a switch that is off has the toggle face with the knob at the left");
}

// 段の印: high は heavy の色、low は heavy を使わない。切れない機能は語だけ（スイッチの面が無い）。
void verify_tiers(const PanelStrip &strip, const core::Palette &palette)
{
    const auto view = sample_view(std::nullopt);
    const auto high_top = strip.top_showing(view, wide_width, lsp_row);
    const auto high_layout = strip.layout_of(view, wide_width, high_top);
    const auto high = strip(view, wide_width, high_top);
    const auto &high_row = PanelStrip::row_box(high_layout, lsp_row);
    expect(painted_with(high, high_row.box, heavy) >= 3 * 9,
           "a high tier draws three dots in the heavy color");
    const auto low_top = strip.top_showing(view, wide_width, matchparen_row);
    const auto low_layout = strip.layout_of(view, wide_width, low_top);
    const auto low = strip(view, wide_width, low_top);
    const auto &low_row = PanelStrip::row_box(low_layout, matchparen_row);
    const auto fixed = core::settings_control_box(low_layout, low_row);
    expect(painted_with(low, low_row.box, heavy) == 0 &&
               painted_with(low, low_row.box, palette.muted) >= 9 &&
               painted_with(low, fixed, palette.accent) == 0 &&
               painted_with(low, fixed, palette.toggle) == 0 &&
               painted_with(low, fixed, palette.panel) <
                   static_cast<std::size_t>(core::width_of(fixed) * core::height_of(fixed)),
           "a low tier draws a muted dot and an always-on row writes words instead of a switch");
}

// 「言語サーバー」の節の行（#1945）: 「使う」は主ボタンと同じ accent の地と on_accent
// の字・「外す」 は accent を使わない・状態の語は色分けしない（構文の色を使わない・止めた物だけ
// heavy）・押す物の 無い行はボタンを描かない・焦点の行は text の 1 DIP の枠（レビュー M1 / R4）。
void verify_servers(const PanelStrip &strip, const core::Palette &palette)
{
    using State = app::LanguageServerRowState;
    using Action = app::LanguageServerRowAction;
    auto view = sample_view(std::nullopt);
    view.servers = {{"C / C++", "clangd", State::running, R"(C:\LLVM\bin\clangd.exe)", true,
                     Action::remove, "cpp"},
                    {"Python", "pyright-langserver", State::found, R"(C:\npm\langserver.js)", true,
                     Action::use, "python"},
                    {"Go", "gopls", State::missing, "go install gopls", true, Action::none, "go"}};
    view.servers_hidden = 2;
    const std::size_t first = view.rows.size();
    const auto top = strip.top_showing(view, wide_width, first);
    const auto layout = strip.layout_of(view, wide_width, top);
    const auto pixels = strip(view, wide_width, top);
    const auto &running = PanelStrip::row_box(layout, first);
    const auto remove = core::settings_control_box(layout, running);
    expect(
        painted_with(pixels, running.box, heavy) == 0 &&
            painted_with(pixels, remove, palette.accent) == 0 &&
            pixel_is(pixels, remove.left + 4, (remove.top + remove.bottom) / 2, palette.background),
        "a running server writes its state without a state color and Remove has no accent");
    const auto found_top = strip.top_showing(view, wide_width, first + 1);
    const auto found_layout = strip.layout_of(view, wide_width, found_top);
    const auto found = strip(view, wide_width, found_top);
    const auto &usable = PanelStrip::row_box(found_layout, first + 1);
    const auto button = core::settings_control_box(found_layout, usable);
    expect(pixel_is(found, button.left + 4, (button.top + button.bottom) / 2, palette.accent) &&
               painted_with(found, button, palette.on_accent) > 0,
           "Use is the filled primary button: the accent face with an on_accent label");
    const auto missing_top = strip.top_showing(view, wide_width, first + 2);
    const auto missing_layout = strip.layout_of(view, wide_width, missing_top);
    const auto &missing = PanelStrip::row_box(missing_layout, first + 2);
    expect(core::width_of(core::settings_control_box(missing_layout, missing)) == 0 &&
               painted_with(strip(view, wide_width, missing_top), missing.box, palette.accent) == 0,
           "a missing server has no button");
    auto focused = view;
    focused.focused = first + 1;
    const auto ringed = strip(focused, wide_width, found_top);
    expect(pixel_is(ringed, usable.box.left + 20, usable.box.top, palette.text) &&
               pixel_is(ringed, usable.box.left + 5, usable.box.top + 28, palette.current_line),
           "the focused language row has the current line face inside a text frame");
    auto stopped = view;
    stopped.servers.at(0).state = State::stopped;
    expect(painted_with(strip(stopped, wide_width, top), running.box, heavy) > 0,
           "a stopped server writes its state in the heavy (error) color");
}

// #1926: 選ぶ欄の一覧（配色の行に開いた 12 個・今の値 1・選ぼうとしている位置 0）。一覧は値の箱の
// 下に右端をそろえて出て、地は background・選ぼうとしている行は text の枠・今の値の行に ✓（text）・
// 送れるので右端に panel_border の棒・値の箱の枠は text。帯をずらして一覧の上端を bitmap に入れる。
[[nodiscard]] app::SettingsView choice_view()
{
    auto view = sample_view(0);
    std::vector<std::string> choices;
    for (std::size_t index = 0; index < 12; ++index)
    {
        choices.push_back("scheme-" + std::to_string(index));
    }
    view.choice = app::SettingsChoiceView{std::move(choices), 1, 0, 0, {}};
    return view;
}

void verify_choice_list(const PanelStrip &strip, const core::Palette &palette)
{
    const auto view = choice_view();
    const auto first = strip.choice_layout_of(view, 0);
    expect(first.has_value(), "an open list has a layout under the focused value box");
    if (!first.has_value())
    {
        return;
    }
    // 一覧の上端が bitmap の y = 5 に来るよう帯をずらす。
    const auto top = 5 - first.value().panel.top;
    const auto layout = strip.layout_of(view, wide_width, top);
    const auto shifted = strip.choice_layout_of(view, top);
    if (!shifted.has_value())
    {
        return;
    }
    const auto &list = shifted.value();
    const auto pixels = strip(view, wide_width, top);
    const auto box = core::settings_control_box(layout, PanelStrip::row_box(layout, 0));
    const auto chosen = core::settings_choice_row(list, 0);
    const auto current = core::settings_choice_row(list, 1);
    expect(list.panel.top == 5 && list.panel.right == box.right && !list.above && list.rows == 10 &&
               list.panel.top == box.bottom + 4,
           "the list opens under the value box with its right edge on the box");
    expect(pixel_is(pixels, chosen.left + 20, chosen.top, palette.text) &&
               pixel_is(pixels, current.left + 6, (current.top + current.bottom) / 2,
                        palette.background),
           "the row to choose has a text frame on the background of the list");
    // 印の欄の上下 3 画素は除く（選ぼうとしている行の枠の線が通る）。
    const auto inner = [&list](std::size_t index)
    {
        const auto mark = core::settings_choice_mark(list, index);
        return core::LayoutRect{mark.left, mark.top + 3, mark.right, mark.bottom - 3};
    };
    expect(painted_with(pixels, inner(1), palette.text) > 0 &&
               painted_with(pixels, inner(0), palette.text) == 0,
           "only the current choice has a check mark");
    expect(painted_with(pixels, core::settings_choice_thumb(list), palette.panel_border) > 0,
           "a list longer than ten rows has a scroll bar");
}

// 細い窓（700 DIP 未満）は全幅で、`:set` の名前を描かない。
void verify_narrow(const PanelStrip &strip, const core::Palette &palette)
{
    const auto view = sample_view(std::nullopt);
    const auto wide_top = strip.top_showing(view, wide_width, wrap_row);
    const auto wide_layout = strip.layout_of(view, wide_width, wide_top);
    const auto wide = strip(view, wide_width, wide_top);
    const auto narrow_top = strip.top_showing(view, narrow_width, wrap_row);
    const auto narrow_layout = strip.layout_of(view, narrow_width, narrow_top);
    const auto narrow = strip(view, narrow_width, narrow_top);
    expect(narrow_layout.narrow && narrow_layout.panel.left == 0 &&
               pixel_is(narrow, 2, 4, palette.panel) &&
               pixel_is(narrow, narrow_width - 2, 4, palette.panel) &&
               pixel_is(narrow, narrow_width + 2, 4, black),
           "a narrow window fills its whole width with the panel");
    // `:set` の名前の欄（部品の左 14 DIP から 60 DIP ぶん）。
    const auto named = [](const core::SettingsLayout &layout)
    {
        const auto &row = PanelStrip::row_box(layout, wrap_row);
        const auto box = core::settings_control_box(layout, row);
        return core::LayoutRect{box.left - 14 - 60, row.box.top, box.left - 14, row.box.bottom};
    };
    const auto wide_area = named(wide_layout);
    const auto narrow_area = named(narrow_layout);
    const auto full = [](const core::LayoutRect &area)
    {
        return static_cast<std::size_t>(core::width_of(area) *
                                        (std::min(area.bottom, strip_height) - area.top));
    };
    expect(painted_with(wide, wide_area, palette.panel) < full(wide_area) &&
               painted_with(narrow, narrow_area, palette.panel) == full(narrow_area),
           "the :set name is written in the drawer and hidden in the narrow panel");
}

// 鍵の振り分けの表。面の中は Vim のモードでも同じで、ここに無い鍵は捨てる。
void verify_keys()
{
    using nenenib::ui::win32::settings_key_action;
    using nenenib::ui::win32::settings_section_key;
    using Row = std::pair<WPARAM, SettingsKeyAction>;
    constexpr std::array<Row, 13> plain{{
        {VK_ESCAPE, SettingsKeyAction::close},
        {VK_UP, SettingsKeyAction::step_up},
        {VK_DOWN, SettingsKeyAction::step_down},
        {VK_TAB, SettingsKeyAction::step_down},
        {VK_PRIOR, SettingsKeyAction::page_up},
        {VK_NEXT, SettingsKeyAction::page_down},
        {VK_HOME, SettingsKeyAction::first},
        {VK_END, SettingsKeyAction::last},
        {VK_RETURN, SettingsKeyAction::change},
        {VK_LEFT, SettingsKeyAction::previous_value},
        {VK_RIGHT, SettingsKeyAction::next_value},
        {VK_BACK, SettingsKeyAction::erase},
        // 文字の鍵（Vim の j）と F1 は WM_KEYDOWN では何もしない（文字は WM_CHAR が探す欄へ）。
        {'J', SettingsKeyAction::swallow},
    }};
    bool matched = true;
    for (const auto &[key, action] : plain)
    {
        matched = matched && settings_key_action(key, false, false) == action;
    }
    expect(matched && settings_key_action(VK_TAB, false, true) == SettingsKeyAction::step_up &&
               settings_key_action(VK_F1, false, false) == SettingsKeyAction::swallow,
           "the keys of the open panel move the focus, change the value, erase or close");
    expect(settings_key_action(VK_OEM_COMMA, true, false) == SettingsKeyAction::close &&
               settings_key_action('Z', true, false) == SettingsKeyAction::revert &&
               settings_key_action('S', true, false) == SettingsKeyAction::swallow &&
               settings_key_action(VK_TAB, true, false) == SettingsKeyAction::swallow &&
               settings_key_action(VK_OEM_COMMA, false, false) == SettingsKeyAction::swallow,
           "Ctrl+, closes, Ctrl+Z reverts and other Ctrl keys do not reach the text");
    expect(settings_section_key('1', true) == std::optional<std::size_t>{0} &&
               settings_section_key('6', true) == std::optional<std::size_t>{5} &&
               !settings_section_key('7', true).has_value() &&
               !settings_section_key('1', false).has_value(),
           "Ctrl+1 to Ctrl+6 jump to the sections");
    const auto chord = nenenib::ui::win32::operation_chord(VK_OEM_COMMA, true, false);
    const auto shifted = nenenib::ui::win32::operation_chord(VK_OEM_COMMA, true, true);
    expect(chord.has_value() &&
               core::operation_for(chord.value(), core::EditMode::vim) ==
                   std::optional{core::EditorOperation::open_settings} &&
               core::operation_for(chord.value(), core::EditMode::ordinary) ==
                   std::optional{core::EditorOperation::open_settings} &&
               shifted.has_value() &&
               !core::operation_for(shifted.value(), core::EditMode::ordinary).has_value(),
           "Ctrl+, opens the settings in both modes and Ctrl+Shift+, does nothing");
}
} // namespace

void verify_settings_panel_paint()
{
    verify_keys();
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the settings panel needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI") || !installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("the settings panel pixels need Segoe UI and Consolas");
        return;
    }
    SettingsPanelFormats formats;
    if (!formats.made(*factory.Get(), {L"Segoe UI", L"Consolas"}, 1.0F, L"ja-jp"))
    {
        unmeasured("the settings panel formats could not be made");
        return;
    }
    const auto text = formats.text(factory.Get(), dpi, core::DisplayLanguage::japanese);
    for (const auto appearance : {core::Appearance::dark, core::Appearance::light})
    {
        const auto palette = core::palette_for(appearance);
        PaletteRowLayouts layouts{SettingsPanelPainter::retained};
        const PanelStrip strip(target, text, layouts, palette);
        verify_panel(strip, layouts, palette);
        verify_rows(strip, palette);
        verify_tiers(strip, palette);
        verify_narrow(strip, palette);
        verify_servers(strip, palette);
        verify_choice_list(strip, palette);
    }
    // accent が背景に近いテーマ（synthwave-84 の accent は選択範囲の地の色）でも「使う」と焦点が
    // 見える形のまま（レビュー M1）。
    const auto &faint = core::theme_of(core::BuiltinTheme::synthwave_84).ui;
    PaletteRowLayouts layouts{SettingsPanelPainter::retained};
    verify_servers(PanelStrip(target, text, layouts, faint), faint);
}
} // namespace nenenib::tests::ui
