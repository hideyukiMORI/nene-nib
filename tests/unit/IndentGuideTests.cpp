// scope `--indent-guides` の単体テスト（ADR 0193 の決定 3〜9・14〜16 の段
// 1・#1975）。字下げの数え・ 段の幅の推し当ての表（2 / 4 / 8 / Tab / 混在 / 票が足りない・差
// 6・同数）・空の行の続きと 1 画面の 打ち切り・強める段の区間・125 テーマの虹の取れた数（足りない 8
// テーマの名指し）・線の色、`:set` の 5 つと Ctrl+P の候補、設定の画面の行、application の frame
// の欄（オフで空・見えている行だけ・ split
// の両枠と強める段は操作中だけ・巨大な行と差し込み行で付けない・推し当ては開く道だけ）。
#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "Column.hpp"
#include "ComposeText.hpp"
#include "DisplayForm.hpp"
#include "DisplayLanguage.hpp"
#include "Editing.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FeatureCosts.hpp"
#include "FilePath.hpp"
#include "GiantLine.hpp"
#include "IndentGuide.hpp"
#include "IndentGuidePaint.hpp"
#include "IndentGuideStyle.hpp"
#include "IndentGuides.hpp"
#include "IndentUnit.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PlaceCaret.hpp"
#include "RainbowColors.hpp"
#include "RainbowRoleTable.hpp"
#include "RgbColor.hpp"
#include "Scopes.hpp"
#include "SelectDisplay.hpp"
#include "SelectionAnchoring.hpp"
#include "SettingsRow.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "Theme.hpp"
#include "ThemeDerivation.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
using core::IndentUnit;

// ---------------------------------------------------------------- 字下げの数え（決定 7）

void verify_indent_cells()
{
    expect(core::indent_cells("") == 0 && core::indent_cells("x") == 0 &&
               core::indent_cells("    x") == 4 && core::indent_cells("\tx") == 8,
           "spaces count one cell and a Tab reaches the next tab stop");
    expect(core::indent_cells("  \tx") == 8 && core::indent_cells(" \t  x") == 10 &&
               core::indent_cells("\t\tx") == 16,
           "Tabs after spaces stop at the multiple of eight like Vim's get_indent_str");
    expect(core::indent_cells("\xE3\x80\x80x") == 0 && core::indent_cells("  \xE3\x80\x80x") == 2,
           "an ideographic space ends the indent");
    expect(core::indent_bytes(" \t x") == 3 && core::indent_bytes("x") == 0,
           "the indent bytes are the leading spaces and Tabs");
    expect(core::blank_indent_line("") && core::blank_indent_line(" \t ") &&
               core::blank_indent_line("  \r") && !core::blank_indent_line("  x"),
           "a line of spaces and Tabs only is blank");
    const auto text = buffer_of("    a\n\n \t\n\t\tb\n" + std::string(200, ' ') + "c");
    expect(
        core::line_indent_cells(text, core::LineNumber{1}) == std::optional<std::size_t>{4} &&
            !core::line_indent_cells(text, core::LineNumber{2}).has_value() &&
            !core::line_indent_cells(text, core::LineNumber{3}).has_value() &&
            core::line_indent_cells(text, core::LineNumber{4}) == std::optional<std::size_t>{16} &&
            core::line_indent_cells(text, core::LineNumber{5}) == std::optional<std::size_t>{200},
        "a body line's indent reads the leading window and widens it for long indents");
}

// ---------------------------------------------------------------- 段の幅の推し当て（決定 7）

// 推し当ての表の 1 行。
using GuessRow = std::pair<std::string_view, std::optional<IndentUnit>>;

const std::array<GuessRow, 10> guess_rows{{
    {"a\n  b\n    c\n  d\n\n    e\nf\n", IndentUnit::two},
    {"a\n    b\n        c\n    d\ne\n", IndentUnit::four},
    {"a\n        b\nc\n        d\n", IndentUnit::eight},
    {"a\n\tb\n\t\tc\n\td\n", IndentUnit::tab},
    // 混在: Tab の行が 2・空白の差が 3 票なら空白の差。
    {"a\n    b\nc\n    d\ne\n\tf\n\tg\n", IndentUnit::four},
    // 混在: Tab の行が空白の票より多ければ tab。
    {"a\n\tb\n\tc\n\td\n  e\n", IndentUnit::tab},
    // 票が 3 未満。
    {"a\n    b\nc\n", std::nullopt},
    // 差 6 は票にしない。
    {"a\n      b\nc\n      d\ne\n      f\n", std::nullopt},
    // 同数（2 が 2 票・4 が 2 票）は小さい方。
    {"a\n  b\nc\n    d\ne\n", IndentUnit::two},
    // 空の文書。
    {"", std::nullopt},
}};

void verify_guess_table()
{
    for (const auto &[head, expected] : guess_rows)
    {
        expect(core::guessed_indent_unit(head) == expected,
               "the indent unit guess follows the table of the decided rules");
    }
    std::string late(core::indent_guess_lines, 'x');
    for (std::size_t line = 1; line < core::indent_guess_lines; ++line)
    {
        late.at(line - 1) = '\n';
    }
    expect(!core::guessed_indent_unit(late + "\n  a\nb\n  c\nd\n  e\n").has_value(),
           "lines after the first thousand do not vote");
    expect(core::guessed_indent_unit(buffer_of("a\n  b\n    c\n  d\n")) == IndentUnit::two,
           "the guess reads the head of a body");
    expect(core::indent_unit_cells(IndentUnit::tab) == 8 &&
               core::indent_guide_step(std::nullopt, 2) == 2 &&
               core::indent_guide_step(IndentUnit::four, 8) == 4,
           "an unguessed document uses the configured width");
}

// ---------------------------------------------------------------- 空の行の続きと強める段（決定 9）

void verify_continued_and_active()
{
    const std::vector<std::optional<std::size_t>> cells{
        4, std::nullopt, 8, std::nullopt, std::nullopt, 0};
    expect(core::continued_indents(cells, std::nullopt, std::nullopt) ==
               std::vector<std::size_t>{4, 8, 8, 8, 8, 0},
           "a blank line continues with the larger of the indents above and below");
    const std::vector<std::optional<std::size_t>> edges{std::nullopt, 4, std::nullopt};
    expect(core::continued_indents(edges, 8, std::nullopt) == std::vector<std::size_t>{8, 4, 4},
           "the edge blank lines use the reached side when the other side is out of reach");
    expect(core::continued_indents(edges, std::nullopt, 2) == std::vector<std::size_t>{4, 4, 4},
           "the larger side wins at both edges");
    const std::vector<std::optional<std::size_t>> empty{std::nullopt, std::nullopt};
    expect(core::continued_indents(empty, std::nullopt, std::nullopt) ==
               std::vector<std::size_t>{0, 0},
           "blank lines without a reachable line have no guide");
    const std::vector<std::size_t> levels{0, 1, 2, 2, 1, 2, 0};
    const auto run = core::active_indent_run(levels, 2);
    expect(run.has_value() && run.value().begin == 2 && run.value().end == 4 &&
               run.value().level == 1,
           "the strong guide is the caret line's innermost level over the deeper run");
    const auto outer = core::active_indent_run(levels, 4);
    expect(outer.has_value() && outer.value().begin == 1 && outer.value().end == 6 &&
               outer.value().level == 0,
           "the run stops at the first shallower line on either side");
    expect(!core::active_indent_run(levels, 0).has_value() &&
               !core::active_indent_run(levels, 9).has_value(),
           "a caret line without a guide strengthens nothing");
}

// ---------------------------------------------------------------- 虹の色（決定 5）

// 見分けられる色が 4 色に届かないテーマ（決定 5 の名指し）と取れた数。
constexpr std::array<std::pair<std::string_view, std::size_t>, 8> short_themes{{
    {"quiet", 1},
    {"industry", 3},
    {"novum", 3},
    {"winter-sentinel", 3},
    {"crimson-mercenary", 3},
    {"last-outlaw", 3},
    {"noir-vigil", 3},
    {"folded-city", 2},
}};

[[nodiscard]] std::optional<std::size_t> short_count(std::string_view name)
{
    const auto found =
        std::ranges::find(short_themes, name, &std::pair<std::string_view, std::size_t>::first);
    return found == short_themes.end() ? std::nullopt : std::optional{found->second};
}

// 取った役割の印（rainbow_roles の位置のビット）。取った色は役割の色の写しなので、印が同じなら
// 色も同じ。
[[nodiscard]] unsigned picked_roles(const core::Theme &theme, const core::RainbowColors &colors)
{
    unsigned mask = 0;
    std::size_t taken = 0;
    for (std::size_t role = 0; role < core::rainbow_roles.size(); ++role)
    {
        const bool picked = taken < colors.count &&
                            theme.body.*core::rainbow_roles.at(role) == colors.colors.at(taken);
        mask |= picked ? 1U << role : 0U;
        taken += picked ? 1U : 0U;
    }
    return mask;
}

// 表に名指しした印と同じか（表に無いテーマは外れ）。
[[nodiscard]] bool roles_as_recorded(const core::Theme &theme, const core::RainbowColors &colors)
{
    const auto found = std::ranges::find(rainbow_role_table, theme.name,
                                         &std::pair<std::string_view, unsigned>::first);
    return found != rainbow_role_table.end() && found->second == picked_roles(theme, colors);
}

void verify_rainbow_counts()
{
    std::array<std::size_t, 7> counts{};
    bool short_ones_match = true;
    bool recorded = core::builtin_themes.size() == rainbow_role_table.size();
    for (const core::Theme &theme : core::builtin_themes)
    {
        const auto colors = core::rainbow_colors(theme.body, theme.ui, 6);
        recorded = recorded && roles_as_recorded(theme, colors);
        ++counts.at(colors.count);
        const auto named = short_count(theme.name);
        short_ones_match = short_ones_match &&
                           (named.has_value() ? named.value() == colors.count : colors.count >= 4);
    }
    expect(short_ones_match,
           "only the eight named themes have fewer than four distinguishable colors");
    expect(recorded, "every theme takes the recorded roles (the colors do not move)");
    expect(counts == std::array<std::size_t, 7>{0, 1, 1, 6, 22, 41, 54},
           "117 themes give four colors, 95 give five and 54 give six");
    const auto &aubergine = core::theme_of(core::BuiltinTheme::ubuntu_aubergine);
    const auto four = core::rainbow_colors(aubergine.body, aubergine.ui, 4);
    expect(four.count == 4 && four.colors.at(0) == aubergine.body.keyword &&
               four.colors.at(1) == aubergine.body.function,
           "the rainbow takes the roles in the decided order and stops at the wanted count");
}

void verify_guide_colors()
{
    const auto &aubergine = core::theme_of(core::BuiltinTheme::ubuntu_aubergine);
    const auto rainbow =
        core::indent_guide_paint(core::IndentGuideStyle::rainbow, 4, aubergine.body, aubergine.ui);
    const auto first = rainbow.rainbow.colors.at(0);
    expect(core::indent_guide_color(rainbow, 0, true) == first &&
               core::indent_guide_color(rainbow, 4, true) == first &&
               core::indent_guide_color(rainbow, 1, true) != first,
           "the rainbow cycles through the taken colors by level");
    expect(core::indent_guide_color(rainbow, 0, false) ==
               core::mix(first, aubergine.ui.background, core::indent_guide_faded_percent),
           "a guide that is not strengthened is the role color faded toward the background");
    const auto plain =
        core::indent_guide_paint(core::IndentGuideStyle::plain, 4, aubergine.body, aubergine.ui);
    expect(core::indent_guide_color(plain, 3, false) == aubergine.ui.panel_border &&
               core::indent_guide_color(plain, 3, true) == aubergine.ui.muted,
           "the plain style uses panel_border and muted for the strengthened level");
    const auto &quiet = core::theme_of(core::BuiltinTheme::quiet);
    const auto one =
        core::indent_guide_paint(core::IndentGuideStyle::rainbow, 4, quiet.body, quiet.ui);
    expect(one.rainbow.count == 1 &&
               core::indent_guide_color(one, 0, true) == core::indent_guide_color(one, 1, true),
           "a one-color theme cycles a single color");
    const auto none =
        core::indent_guide_paint(core::IndentGuideStyle::rainbow, 0, aubergine.body, aubergine.ui);
    expect(none.rainbow.count == 0 &&
               core::indent_guide_color(none, 2, false) == aubergine.ui.panel_border &&
               core::indent_guide_color(none, 2, true) == aubergine.ui.muted,
           "a rainbow without colors falls back to the plain colors (#2013)");
}

// 幅と色の数の値の読みと送り（IndentGuide.hpp・#2013）: 幅は 2・4・8 だけで端で止まり、色の数は
// 1 文字の 3〜6 だけで端で止まる。
void verify_value_parsers()
{
    expect(core::indent_guide_width_of("2") == std::optional<std::uint8_t>{2} &&
               core::indent_guide_width_of("4") == std::optional<std::uint8_t>{4} &&
               core::indent_guide_width_of("8") == std::optional<std::uint8_t>{8} &&
               !core::indent_guide_width_of("3").has_value() &&
               !core::indent_guide_width_of("").has_value() &&
               !core::indent_guide_width_of("16").has_value(),
           "the guide width reads only 2, 4 and 8");
    expect(core::indent_guide_width_stepped(2, true) == 4 &&
               core::indent_guide_width_stepped(8, true) == 8 &&
               core::indent_guide_width_stepped(4, false) == 2 &&
               core::indent_guide_width_stepped(2, false) == 2,
           "the guide width doubles and halves and stops at 2 and 8");
    expect(core::indent_guide_color_count_of("3") == std::optional<std::uint8_t>{3} &&
               core::indent_guide_color_count_of("6") == std::optional<std::uint8_t>{6} &&
               !core::indent_guide_color_count_of("2").has_value() &&
               !core::indent_guide_color_count_of("7").has_value() &&
               !core::indent_guide_color_count_of("").has_value() &&
               !core::indent_guide_color_count_of("33").has_value(),
           "the color count reads one digit from 3 to 6");
    expect(core::indent_guide_color_count_stepped(3, false) == 3 &&
               core::indent_guide_color_count_stepped(4, false) == 3 &&
               core::indent_guide_color_count_stepped(6, true) == 6 &&
               core::indent_guide_color_count_stepped(5, true) == 6,
           "the color count steps by one and stops at 3 and 6");
}

// ---------------------------------------------------------------- `:set` と設定の画面（決定 3・4）

[[nodiscard]] std::optional<core::EditorSettings> set_by(std::string_view text,
                                                         const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value())
    {
        return std::nullopt;
    }
    return result.value().settings;
}

[[nodiscard]] std::string answer_of(std::string_view text, const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value() || result.value().settings.has_value())
    {
        return {};
    }
    return std::string(result.value().message.text());
}

[[nodiscard]] bool refused(std::string_view text, const core::EditorSettings &settings)
{
    return !core::evaluate_ex(text, settings, core::Appearance::dark).has_value();
}

void verify_switches()
{
    const auto settings = core::default_editor_settings();
    const auto on = set_by("set indentguides", settings).value_or(settings);
    expect(core::feature_enabled(on, core::EditorFeature::indent_guides) &&
               on.features == std::vector<std::string>{"indentguides"},
           ":set indentguides writes the name into features");
    expect(!core::feature_enabled(set_by("set noindentguides", on).value_or(on),
                                  core::EditorFeature::indent_guides),
           ":set noindentguides takes it out");
    expect(answer_of("set indentguides?", settings) == "indentguides=off" &&
               answer_of("set indentguides?", on) == "indentguides=on",
           ":set indentguides? answers the switch");
    expect(core::feature_enabled(set_by("set indentguides!", settings).value_or(settings),
                                 core::EditorFeature::indent_guides) &&
               !core::feature_enabled(set_by("set invindentguides", on).value_or(on),
                                      core::EditorFeature::indent_guides),
           "the switch toggles with ! and inv");
    const auto weak = set_by("set noindentguidehighlightcurrent", settings).value_or(settings);
    expect(!weak.indent_guide_highlight_current &&
               answer_of("set indentguidehighlightcurrent?", weak) ==
                   "indentguidehighlightcurrent=off",
           ":set noindentguidehighlightcurrent turns the strong level off");
    expect(set_by("set indentguidehighlightcurrent&", weak)
               .value_or(weak)
               .indent_guide_highlight_current,
           "indentguidehighlightcurrent& resets to on");
}

void verify_values()
{
    const auto settings = core::default_editor_settings();
    const auto plain = set_by("set indentguidestyle=plain", settings).value_or(settings);
    expect(plain.indent_guide_style == core::IndentGuideStyle::plain &&
               answer_of("set indentguidestyle", plain) == "indentguidestyle=plain" &&
               answer_of("set indentguidestyle?", settings) == "indentguidestyle=rainbow",
           ":set indentguidestyle= sets and the bare name asks");
    expect(refused("set indentguidestyle=stripes", settings), "an unknown style is refused");
    expect(set_by("set indentguidestyle&", plain).value_or(plain).indent_guide_style ==
               core::IndentGuideStyle::rainbow,
           "indentguidestyle& resets to rainbow");
    expect(set_by("set indentguidecolorcount=6", settings)
                       .value_or(settings)
                       .indent_guide_color_count == 6 &&
               set_by("set indentguidecolorcount=3", settings)
                       .value_or(settings)
                       .indent_guide_color_count == 3 &&
               refused("set indentguidecolorcount=2", settings) &&
               refused("set indentguidecolorcount=7", settings),
           "the color count takes 3 to 6");
    expect(answer_of("set indentguidecolorcount", settings) == "indentguidecolorcount=4" &&
               set_by("set indentguidecolorcount+=1", settings)
                       .value_or(settings)
                       .indent_guide_color_count == 5,
           "the color count answers and adds like a number option");
    expect(set_by("set indentguidewidth=2", settings).value_or(settings).indent_guide_width == 2 &&
               set_by("set indentguidewidth=8", settings).value_or(settings).indent_guide_width ==
                   8 &&
               refused("set indentguidewidth=3", settings) &&
               refused("set indentguidewidth=6", settings),
           "the width takes 2, 4 or 8");
    expect(answer_of("set indentguidewidth?", settings) == "indentguidewidth=4",
           ":set indentguidewidth? answers the configured width");
}

void verify_candidates()
{
    const auto candidates = core::ex_command_candidates();
    const auto has = [&candidates](std::string_view name)
    { return std::ranges::find(candidates, name) != candidates.end(); };
    expect(has("set indentguides") && has("set noindentguides") && has("set minimap") &&
               has("set nominimap") && has("set indentguidestyle=plain") &&
               has("set indentguidecolorcount=") && has("set noindentguidehighlightcurrent") &&
               has("set indentguidewidth="),
           "Ctrl+P lists the switch from the feature table and the shape options");
}

void verify_settings_rows()
{
    const auto settings = core::default_editor_settings();
    const auto stepped = [&settings](core::SettingsValue value, bool forward)
    { return core::settings_value_stepped(settings, value, forward, {}); };
    expect(
        stepped(core::SettingsValue::indent_guide_style, true).indent_guide_style ==
                core::IndentGuideStyle::plain &&
            !stepped(core::SettingsValue::indent_guide_highlight_current, true)
                 .indent_guide_highlight_current &&
            stepped(core::SettingsValue::indent_guide_color_count, true).indent_guide_color_count ==
                5 &&
            stepped(core::SettingsValue::indent_guide_color_count, false)
                    .indent_guide_color_count == 3 &&
            stepped(core::SettingsValue::indent_guide_width, true).indent_guide_width == 8 &&
            stepped(core::SettingsValue::indent_guide_width, false).indent_guide_width == 2,
        "the settings rows step the shape values");
    auto edge = settings;
    edge.indent_guide_width = 8;
    edge.indent_guide_color_count = 6;
    expect(core::settings_value_stepped(edge, core::SettingsValue::indent_guide_width, true, {})
                       .indent_guide_width == 8 &&
               core::settings_value_stepped(edge, core::SettingsValue::indent_guide_color_count,
                                            true, {})
                       .indent_guide_color_count == 6,
           "the steppers stop at their ends");
    expect(core::settings_value_text(settings, core::SettingsValue::indent_guide_style,
                                     core::DisplayLanguage::japanese) == "虹" &&
               core::settings_value_text(settings,
                                         core::SettingsValue::indent_guide_highlight_current,
                                         core::DisplayLanguage::english) == "On" &&
               core::settings_value_text(settings, core::SettingsValue::indent_guide_width,
                                         core::DisplayLanguage::english) == "4",
           "the settings rows show the values in the display language");
    const auto found = std::ranges::find_if(
        core::settings_rows, [](const core::SettingsRow &row)
        { return row.subject == core::SettingsSubject{core::EditorFeature::indent_guides}; });
    const std::array<core::SettingsValue, 4> shapes{
        core::SettingsValue::indent_guide_style, core::SettingsValue::indent_guide_color_count,
        core::SettingsValue::indent_guide_highlight_current,
        core::SettingsValue::indent_guide_width};
    bool below = found != core::settings_rows.end() &&
                 found->section == core::SettingsSection::display &&
                 static_cast<std::size_t>(core::settings_rows.end() - found) > shapes.size();
    for (std::size_t index = 0; below && index < shapes.size(); ++index)
    {
        const auto &row = *(found + static_cast<std::ptrdiff_t>(index + 1));
        below = row.section == core::SettingsSection::display &&
                row.subject == core::SettingsSubject{shapes.at(index)};
    }
    expect(below, "the switch and its four shape rows sit in the display section in order");
    expect(core::feature_switchable(core::EditorFeature::indent_guides) &&
               !core::feature_cost(core::EditorFeature::indent_guides).default_on &&
               !core::feature_tier(core::EditorFeature::indent_guides).has_value(),
           "the feature is listed, off by default and has no tier until measured");
}

// ---------------------------------------------------------------- frame の欄（決定 6・9・14・15）

constexpr std::string_view code_path = "C:\\work\\guides.txt";
constexpr std::string_view two_space = "a\n  b\n    c\n  d\n\n    e\nf\n";

[[nodiscard]] core::EditorSettings guides_on()
{
    return core::feature_set(core::default_editor_settings(), core::EditorFeature::indent_guides,
                             true);
}

void open_text(Editing &editing, std::string_view path, std::string text)
{
    editing.files().hold_at(std::string(path), Bytes{std::move(text)});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(path).value()});
}

[[nodiscard]] std::vector<std::size_t> levels_of(const std::vector<app::LineView> &lines)
{
    std::vector<std::size_t> levels;
    for (const auto &line : lines)
    {
        levels.push_back(line.indent_guides.has_value() ? line.indent_guides.value().levels : 99);
    }
    return levels;
}

[[nodiscard]] std::vector<std::size_t> active_of(const std::vector<app::LineView> &lines)
{
    std::vector<std::size_t> active;
    for (const auto &line : lines)
    {
        active.push_back(
            line.indent_guides.has_value() ? line.indent_guides.value().active.value_or(99) : 99);
    }
    return active;
}

// 行 index の欄（無い行・列の外は 99 の印）。
[[nodiscard]] app::IndentGuides guides_at(const std::vector<app::LineView> &lines,
                                          std::size_t index)
{
    const app::IndentGuides none{99, 99, 99};
    return index < lines.size() ? lines[index].indent_guides.value_or(none) : none;
}

void place(app::EditorController &controller, std::size_t line)
{
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{line}, core::Column{1}},
                                        core::SelectionAnchoring::collapse});
}

void verify_frame_off_and_on()
{
    Editing off;
    applied(off.controller(), app::VisibleLines{10});
    open_text(off, code_path, std::string(two_space));
    const auto quiet = off.controller().frame();
    expect(!quiet.lines.empty() && std::ranges::none_of(quiet.lines, [](const app::LineView &line)
                                                        { return line.indent_guides.has_value(); }),
           "with the feature off the frame has no guide fields");
    Editing on(std::optional<core::EditorSettings>{guides_on()});
    applied(on.controller(), app::VisibleLines{10});
    open_text(on, code_path, std::string(two_space));
    place(on.controller(), 6);
    const auto frame = on.controller().frame();
    expect(frame.lines.size() == 8 && guides_at(frame.lines, 1).step == 2,
           "an opened document uses the guessed two-cell step");
    expect(levels_of(frame.lines) == std::vector<std::size_t>{0, 1, 2, 1, 2, 2, 0, 0},
           "the levels follow the indent and the blank line takes the larger neighbour");
    expect(active_of(frame.lines) == std::vector<std::size_t>{99, 99, 99, 99, 1, 1, 99, 99},
           "the caret's level is strengthened over the deeper run around it");
    auto weak_settings = guides_on();
    weak_settings.indent_guide_highlight_current = false;
    Editing weak(std::optional<core::EditorSettings>{weak_settings});
    applied(weak.controller(), app::VisibleLines{10});
    open_text(weak, code_path, std::string(two_space));
    place(weak.controller(), 6);
    expect(active_of(weak.controller().frame().lines) == std::vector<std::size_t>(8, 99),
           "indentguidehighlightcurrent off strengthens nothing");
}

void verify_frame_screen_reach()
{
    Editing reaching(std::optional<core::EditorSettings>{guides_on()});
    applied(reaching.controller(), app::VisibleLines{3});
    open_text(reaching, code_path, "    x\n\n\n        y\n");
    const auto reached = reaching.controller().frame();
    expect(reached.lines.size() == 3 && guides_at(reached.lines, 2).cells == 8,
           "a blank line at the bottom edge reads the next line within one screen");
    Editing distant(std::optional<core::EditorSettings>{guides_on()});
    applied(distant.controller(), app::VisibleLines{3});
    open_text(distant, code_path, "    x\n\n\n\n\n\n\n        y\n");
    const auto cut = distant.controller().frame();
    expect(cut.lines.size() == 3 && guides_at(cut.lines, 2).cells == 4,
           "beyond one screen the blank lines keep the side that was reached");
}

void verify_frame_guess_paths()
{
    Editing untitled(std::optional<core::EditorSettings>{guides_on()});
    applied(untitled.controller(), app::VisibleLines{10});
    applied(untitled.controller(), app::InsertText{"a\n  b\n    c\n  d\n"});
    const auto typed = untitled.controller().frame();
    expect(typed.lines.size() == 5 && guides_at(typed.lines, 1).step == 4 &&
               guides_at(typed.lines, 2).levels == 1,
           "an untitled document uses the configured width without guessing");
    Editing opened(std::optional<core::EditorSettings>{guides_on()});
    applied(opened.controller(), app::VisibleLines{10});
    open_text(opened, code_path, "a\n    b\n        c\n    d\n");
    applied(opened.controller(), app::InsertText{"x\n  y\n    z\n  w\n  v\n"});
    expect(opened.controller()
                   .frame()
                   .lines.front()
                   .indent_guides.value_or(app::IndentGuides{0, 0, 0})
                   .step == 4,
           "editing does not guess the unit again");
}

void verify_frame_split()
{
    Editing editing(std::optional<core::EditorSettings>{guides_on()});
    applied(editing.controller(), app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    open_text(editing, code_path, std::string(two_space));
    place(editing.controller(), 6);
    const auto split =
        editing.controller().apply_frame(app::SplitPane{core::SplitLayout::vertical});
    const auto *other =
        split.other.has_value() ? std::get_if<app::PaneFrame>(&split.other.value()) : nullptr;
    expect(other != nullptr && levels_of(other->lines) == levels_of(split.lines) &&
               active_of(other->lines) == std::vector<std::size_t>(other->lines.size(), 99) &&
               active_of(split.lines).at(4) == 1,
           "both panes carry the guides and only the operated pane strengthens a level");
}

void verify_frame_skipped_rows()
{
    Editing giant(std::optional<core::EditorSettings>{guides_on()});
    applied(giant.controller(), app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    open_text(giant, code_path,
              "  a\n    " + std::string(core::giant_line_threshold + 1, 'x') + "\n  b\n");
    applied(giant.controller(), app::SelectDisplay{core::DisplayForm::text});
    const auto windowed = giant.controller().frame();
    expect(windowed.lines.size() >= 3 && windowed.lines.at(1).window.active &&
               !windowed.lines.at(1).indent_guides.has_value() &&
               windowed.lines.at(0).indent_guides.has_value(),
           "a giant line's window has no guides while its neighbours keep theirs");
    Editing composing(std::optional<core::EditorSettings>{guides_on()});
    applied(composing.controller(), app::VisibleLines{10});
    open_text(composing, code_path, std::string(two_space));
    place(composing.controller(), 3);
    const auto composed =
        composing.controller().apply_frame(app::ComposeText{composed_of("\xE3\x81\x82", {}, 3)});
    expect(composed.lines.at(2).composed.has_value() &&
               !composed.lines.at(2).indent_guides.has_value() &&
               composed.lines.at(1).indent_guides.has_value(),
           "the IME composed line has no guides");
}
} // namespace

void verify_indent_guides_scope()
{
    verify_indent_cells();
    verify_guess_table();
    verify_continued_and_active();
    verify_rainbow_counts();
    verify_guide_colors();
    verify_value_parsers();
    verify_switches();
    verify_values();
    verify_candidates();
    verify_settings_rows();
    verify_frame_off_and_on();
    verify_frame_screen_reach();
    verify_frame_guess_paths();
    verify_frame_split();
    verify_frame_skipped_rows();
    // 推し当ての道と窓の上限の続き（IndentGuidePathTests.cpp・同じ scope）。
    verify_indent_guide_paths();
}
} // namespace nenenib::tests
