// 1.2.0 が書く settings.v6 の見本の試験（ADR 0170 と ADR 0193 の 2026-11-24 の追補）。見本を読むと
// 全部の値が戻り、書き戻すとバイト列が一致し、未知の鍵と公開前の古い 2 つの名前は拒み、v5
// と取り違え ず、鍵の行を 1 つ外すと既定を補う。古い名前の v6
// のファイルの試験だけがファイルに触れる。
#include "SettingsReleasedSixTests.hpp"

#include "DisplayLanguageChoice.hpp"
#include "EditorSettings.hpp"
#include "FinderPreview.hpp"
#include "IndentGuideStyle.hpp"
#include "SettingsCodec.hpp"
#include "SettingsFailure.hpp"
#include "SettingsVersion.hpp"
#include "Win32SettingsAdapter.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests::adapters
{
namespace
{
using nenenib::adapters::win32::decode_settings;
using nenenib::adapters::win32::encode_settings;
using nenenib::adapters::win32::SettingsPaths;
using nenenib::adapters::win32::SettingsVersion;
using nenenib::adapters::win32::Win32FileAdapter;
using nenenib::adapters::win32::Win32SettingsAdapter;
using nenenib::application::SettingsFailure;
using nenenib::core::EditorSettings;

// key の行を外した bytes（行は見本の 2 行目より後ろにある）。
std::string without_key(std::string_view bytes, std::string_view key)
{
    std::string kept(bytes);
    const auto start = kept.find("\n" + std::string(key) + "=") + 1;
    kept.erase(start, kept.find('\n', start) + 1 - start);
    return kept;
}

// from の行の鍵の名前を to に替えた bytes。
std::string renamed(std::string_view bytes, std::string_view from, std::string_view to)
{
    std::string changed(bytes);
    changed.replace(changed.find("\n" + std::string(from) + "=") + 1, from.size(), to);
    return changed;
}

// 外す v6 の鍵と、その欄だけを既定に戻す関数。
using Reset = void (*)(EditorSettings &settings);
using MissingKey = std::pair<std::string_view, Reset>;

constexpr std::array<MissingKey, 9> v6_keys{{
    {"caret_motion", [](EditorSettings &settings) { settings.caret_motion = false; }},
    {"caret_motion_time", [](EditorSettings &settings) { settings.caret_motion_time = 120; }},
    {"caret_sparks", [](EditorSettings &settings) { settings.caret_sparks = false; }},
    {"pane_resize_step", [](EditorSettings &settings) { settings.pane_resize_step = 5; }},
    {"pane_resize_step_vim",
     [](EditorSettings &settings) { settings.pane_resize_step_vim = false; }},
    {"indent_guide_style", [](EditorSettings &settings)
     { settings.indent_guide_style = core::IndentGuideStyle::rainbow; }},
    {"indent_guide_color_count",
     [](EditorSettings &settings) { settings.indent_guide_color_count = 4; }},
    {"indent_guide_highlight_current",
     [](EditorSettings &settings) { settings.indent_guide_highlight_current = true; }},
    {"indent_guide_width", [](EditorSettings &settings) { settings.indent_guide_width = 4; }},
}};

// 未知の鍵・公開前の古い 2 つの名前・同じ鍵の 2 行目は、1.2.0 と同じく全体を拒む。
void verify_refused_keys(Expect expect)
{
    for (const auto extra :
         {"list_eol=on\n", "indent_guide_colors=6\n", "indent_guide_active=off\n",
          "indent_guide_color_count=6\n", "later_key=1\n"})
    {
        const auto refused =
            decode_settings(std::string(released_v6_settings) + extra, SettingsVersion::v6);
        expect(!refused && refused.error() == SettingsFailure::malformed,
               "the 1.2.0 v6 with an unknown, old or second key is refused");
    }
    for (const auto &[from, to] :
         {std::pair{"indent_guide_color_count", "indent_guide_colors"},
          std::pair{"indent_guide_highlight_current", "indent_guide_active"}})
    {
        const auto old =
            decode_settings(renamed(released_v6_settings, from, to), SettingsVersion::v6);
        expect(!old && old.error() == SettingsFailure::malformed,
               "a v6 written with the names used before 1.2.0 is refused as unknown keys");
    }
}

// 版の取り違え: 1.2.0 の v6 を v5 として読まず、1.1.0 の v5 を v6 として読まない。
void verify_versions_kept_apart(Expect expect)
{
    const auto as_v5 = decode_settings(released_v6_settings, SettingsVersion::v5);
    const auto v5_as_v6 = decode_settings(released_v5_settings, SettingsVersion::v6);
    expect(!as_v5 && as_v5.error() == SettingsFailure::unsupported_version && !v5_as_v6 &&
               v5_as_v6.error() == SettingsFailure::unsupported_version,
           "the 1.2.0 v6 is not read as v5 and the 1.1.0 v5 is not read as v6");
}

// v6 の 9 つの鍵の行をどれか 1 つ外すと、その欄だけが既定になり、ほかの値は見本のまま。
void verify_missing_keys(Expect expect, const EditorSettings &released)
{
    for (const auto &[key, reset] : v6_keys)
    {
        const auto read =
            decode_settings(without_key(released_v6_settings, key), SettingsVersion::v6);
        auto wanted = released;
        reset(wanted);
        expect(read && core::same_settings(read.value(), wanted),
               "a 1.2.0 v6 without one v6 key takes that default and keeps the rest");
    }
}
} // namespace

bool released_v6_values(const EditorSettings &settings)
{
    return settings.theme.has_value() && settings.theme.value().name() == "ubuntu-aubergine" &&
           settings.ui_language == core::DisplayLanguageChoice::english && settings.lsp &&
           settings.explorer && settings.scrolloff == 3 && settings.explorer_width == 320 &&
           settings.features == std::vector<std::string>{"minimap", "indentguides"} &&
           settings.lsp_commands.size() == 2 && settings.mapleader == "<Space>" &&
           settings.maps.size() == 1 && settings.autoindent &&
           settings.finder_preview == core::FinderPreview::right && settings.caret_motion &&
           settings.caret_motion_time == 200 && settings.caret_sparks &&
           settings.pane_resize_step == 12 && settings.pane_resize_step_vim &&
           settings.indent_guide_style == core::IndentGuideStyle::plain &&
           settings.indent_guide_color_count == 6 && !settings.indent_guide_highlight_current &&
           settings.indent_guide_width == 2;
}

void verify_released_v6_codec(Expect expect)
{
    const auto read = decode_settings(released_v6_settings, SettingsVersion::v6);
    expect(read && released_v6_values(read.value()),
           "the 1.2.0 settings.v6 is read with every value");
    expect(read && encode_settings(read.value()) == released_v6_settings,
           "the 1.2.0 settings.v6 is written back byte for byte");
    verify_refused_keys(expect);
    verify_versions_kept_apart(expect);
    verify_missing_keys(expect, read.value_or(core::default_editor_settings()));
}

void verify_old_v6_names_file(Expect expect, Win32FileAdapter &files, const SettingsPaths &paths)
{
    const auto old =
        renamed(renamed(released_v6_settings, "indent_guide_color_count", "indent_guide_colors"),
                "indent_guide_highlight_current", "indent_guide_active");
    expect(files.write(paths.current, old).has_value() &&
               files.write(paths.shipped, released_v5_settings).has_value(),
           "prepare a v6 with the old indent guide names above the 1.1.0 v5");
    Win32SettingsAdapter store(files, paths);
    const auto reading = store.read();
    expect(!reading && reading.error() == SettingsFailure::malformed &&
               !store.write(core::default_editor_settings()),
           "a v6 with the old names is refused, does not fall back to v5 and blocks saving");
    const auto current = files.read(paths.current, 4096);
    const auto shipped = files.read(paths.shipped, 4096);
    expect(current && current.value().view() == old && shipped &&
               shipped.value().view() == released_v5_settings,
           "the refused v6 and the v5 stay byte for byte");
}
} // namespace nenenib::tests::adapters
