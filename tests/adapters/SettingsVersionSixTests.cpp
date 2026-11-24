// settings.v6 の codec の試験（ADR 0192 の決定 1・#1933）。1.1.0 の v5 を 1.1.0
// と同じ厳しさで読み、 v6 の鍵に既定を補って v6 の形で書き戻すこと、v6 の 2
// つの鍵の往復と拒否。ファイルに触れない。
#include "SettingsVersionSixTests.hpp"

#include "DisplayLanguageChoice.hpp"
#include "EditorSettings.hpp"
#include "FinderPreview.hpp"
#include "IndentGuideStyle.hpp"
#include "SettingsCodec.hpp"
#include "SettingsFailure.hpp"
#include "SettingsVersion.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests::adapters
{
namespace
{
using nenenib::adapters::win32::decode_settings;
using nenenib::adapters::win32::encode_settings;
using nenenib::adapters::win32::SettingsVersion;
using nenenib::application::SettingsFailure;

// 1.1.0 の v5 の見本を v6 の形にした bytes（版の行を 6 にし、finder_preview の後ろに
// caret_motion=off と caret_motion_time=120 と caret_sparks=off と pane_resize_step=5 と
// pane_resize_step_vim=off と、インデントガイドの 4 つの既定を足す）。
std::string released_v5_as_v6()
{
    std::string bytes(released_v5_settings);
    bytes.replace(0, std::string_view{"version=5"}.size(), "version=6");
    const std::string_view preview = "finder_preview=right\n";
    bytes.insert(bytes.find(preview) + preview.size(),
                 "caret_motion=off\ncaret_motion_time=120\ncaret_sparks=off\n"
                 "pane_resize_step=5\npane_resize_step_vim=off\n" +
                     std::string(indent_guide_defaults));
    return bytes;
}

// key の行の値を value に替えた bytes。
std::string with_value(const std::string &bytes, std::string_view key, std::string_view value)
{
    const auto start = bytes.find("\n" + std::string(key) + "=") + 1 + key.size() + 1;
    std::string changed = bytes;
    changed.replace(start, bytes.find('\n', start) - start, value);
    return changed;
}

// key の行を外した bytes。
std::string without_key(std::string bytes, std::string_view key)
{
    const auto start = bytes.find("\n" + std::string(key) + "=") + 1;
    bytes.erase(start, bytes.find('\n', start) + 1 - start);
    return bytes;
}

// v5 の読みは 1.1.0 と同じ厳しさ。1.1.0 の見本を読んで v6 の鍵に既定を補い、書くと v6 の形になる。
// v5 に v6 の鍵が在れば未知の鍵として拒否する。
void verify_released_v5(Expect expect)
{
    const auto read = decode_settings(released_v5_settings, SettingsVersion::v5);
    expect(read && released_v5_values(read.value()),
           "the 1.1.0 settings.v5 is read with every value and the caret motion defaults");
    expect(read && encode_settings(read.value()) == released_v5_as_v6(),
           "the 1.1.0 settings are written back in the v6 form with the caret motion keys after "
           "finder_preview and the unknown language row kept");
    for (const auto extra : {"caret_motion=off\n", "caret_motion=on\n", "caret_motion_time=120\n",
                             "caret_sparks=off\n", "caret_sparks=on\n", "pane_resize_step=5\n",
                             "pane_resize_step_vim=off\n", "pane_resize_step_vim=on\n",
                             "indent_guide_style=rainbow\n", "indent_guide_color_count=4\n",
                             "indent_guide_highlight_current=on\n", "indent_guide_width=4\n"})
    {
        const auto refused =
            decode_settings(std::string(released_v5_settings) + extra, SettingsVersion::v5);
        expect(!refused && refused.error() == SettingsFailure::malformed,
               "a v5 holding a v6 key is refused like 1.1.0 refuses it");
    }
    const auto as_v6 = decode_settings(released_v5_settings, SettingsVersion::v6);
    const auto v6_as_v5 = decode_settings(released_v5_as_v6(), SettingsVersion::v5);
    expect(!as_v6 && as_v6.error() == SettingsFailure::unsupported_version && !v6_as_v5 &&
               v6_as_v5.error() == SettingsFailure::unsupported_version,
           "a v5 is not read as v6 and a v6 is not read as v5");
}

// v6 の 3 つの鍵（打ったときの粒の caret_sparks は ADR 0192 の追補・#1979）。無ければ既定、有れば厳
// 密に読み、往復する。
void verify_v6_keys(Expect expect)
{
    auto settings = core::default_editor_settings();
    settings.caret_motion = true;
    settings.caret_motion_time = 200;
    settings.caret_sparks = true;
    const auto bytes = encode_settings(settings);
    const auto read = decode_settings(bytes, SettingsVersion::v6);
    expect(read && read.value().caret_motion && read.value().caret_motion_time == 200 &&
               read.value().caret_sparks &&
               bytes.find("\ncaret_motion=on\ncaret_motion_time=200\ncaret_sparks=on\n") !=
                   std::string::npos,
           "caret motion, its time and the sparks round trip through v6");
    const auto missing =
        decode_settings(without_key(without_key(without_key(released_v5_as_v6(), "caret_motion"),
                                                "caret_motion_time"),
                                    "caret_sparks"),
                        SettingsVersion::v6);
    expect(missing && !missing.value().caret_motion && missing.value().caret_motion_time == 120 &&
               !missing.value().caret_sparks,
           "a v6 without the caret motion keys takes the defaults");
    for (const auto &[key, value] :
         {std::pair{"caret_motion", "yes"}, std::pair{"caret_motion", ""},
          std::pair{"caret_motion_time", "20"}, std::pair{"caret_motion_time", "301"},
          std::pair{"caret_motion_time", "0120"}, std::pair{"caret_motion_time", ""},
          std::pair{"caret_sparks", "1"}, std::pair{"caret_sparks", ""}})
    {
        const auto broken = decode_settings(with_value(bytes, key, value), SettingsVersion::v6);
        expect(!broken && broken.error() == SettingsFailure::malformed,
               "a malformed caret motion value is refused");
    }
    expect(!decode_settings(bytes + "caret_motion=on\n", SettingsVersion::v6) &&
               !decode_settings(bytes + "caret_sparks=on\n", SettingsVersion::v6),
           "a duplicate caret motion or sparks key is refused");
}

// インデントガイドの v6 の 4 つの鍵（ADR 0193 の決定 4）。並びは pane_resize_step_vim の後ろ。
// 無ければ既定、有れば厳密に読み、往復する。
void verify_indent_guide_keys(Expect expect)
{
    auto settings = core::default_editor_settings();
    settings.indent_guide_style = core::IndentGuideStyle::plain;
    settings.indent_guide_color_count = 6;
    settings.indent_guide_highlight_current = false;
    settings.indent_guide_width = 2;
    const auto bytes = encode_settings(settings);
    const auto read = decode_settings(bytes, SettingsVersion::v6);
    expect(read && core::same_settings(read.value(), settings) &&
               bytes.find("\npane_resize_step_vim=off\nindent_guide_style=plain\n"
                          "indent_guide_color_count=6\nindent_guide_highlight_current=off\nindent_"
                          "guide_width=2\n") != std::string::npos,
           "the indent guide keys follow the pane resize keys and round trip through v6");
    std::string bare = released_v5_as_v6();
    for (const auto key : {"indent_guide_style", "indent_guide_color_count",
                           "indent_guide_highlight_current", "indent_guide_width"})
    {
        bare = without_key(bare, key);
    }
    const auto missing = decode_settings(bare, SettingsVersion::v6);
    expect(missing && missing.value().indent_guide_style == core::IndentGuideStyle::rainbow &&
               missing.value().indent_guide_color_count == 4 &&
               missing.value().indent_guide_highlight_current &&
               missing.value().indent_guide_width == 4,
           "a v6 without the indent guide keys takes the defaults");
    for (const auto &[key, value] :
         {std::pair{"indent_guide_style", "stripes"}, std::pair{"indent_guide_style", ""},
          std::pair{"indent_guide_color_count", "2"}, std::pair{"indent_guide_color_count", "7"},
          std::pair{"indent_guide_color_count", "04"},
          std::pair{"indent_guide_highlight_current", "yes"}, std::pair{"indent_guide_width", "3"},
          std::pair{"indent_guide_width", "16"}, std::pair{"indent_guide_width", ""}})
    {
        const auto broken = decode_settings(with_value(bytes, key, value), SettingsVersion::v6);
        expect(!broken && broken.error() == SettingsFailure::malformed,
               "a malformed indent guide value is refused");
    }
    expect(!decode_settings(bytes + "indent_guide_width=4\n", SettingsVersion::v6),
           "a duplicate indent guide key is refused");
}

// 枠の大きさの 2 つの鍵（#1972）。無ければ既定の 5 升と off、有れば厳密に読み、往復する。
void verify_pane_resize_keys(Expect expect)
{
    auto settings = core::default_editor_settings();
    settings.pane_resize_step = 12;
    settings.pane_resize_step_vim = true;
    const auto bytes = encode_settings(settings);
    const auto read = decode_settings(bytes, SettingsVersion::v6);
    expect(read && read.value().pane_resize_step == 12 && read.value().pane_resize_step_vim &&
               bytes.find("\ncaret_sparks=off\npane_resize_step=12\npane_resize_step_vim=on\n") !=
                   std::string::npos,
           "the pane resize step and its Vim switch round trip right after caret_sparks");
    const auto missing = decode_settings(
        without_key(without_key(released_v5_as_v6(), "pane_resize_step"), "pane_resize_step_vim"),
        SettingsVersion::v6);
    expect(missing && missing.value().pane_resize_step == 5 &&
               !missing.value().pane_resize_step_vim,
           "a v6 without the pane resize keys takes five cells and off");
    for (const auto &[key, value] :
         {std::pair{"pane_resize_step", "0"}, std::pair{"pane_resize_step", "41"},
          std::pair{"pane_resize_step", "05"}, std::pair{"pane_resize_step", ""},
          std::pair{"pane_resize_step_vim", "yes"}, std::pair{"pane_resize_step_vim", ""}})
    {
        const auto broken = decode_settings(with_value(bytes, key, value), SettingsVersion::v6);
        expect(!broken && broken.error() == SettingsFailure::malformed,
               "a malformed pane resize value is refused");
    }
    expect(!decode_settings(bytes + "pane_resize_step=3\n", SettingsVersion::v6),
           "a duplicate pane resize key is refused");
}
} // namespace

bool released_v5_values(const core::EditorSettings &settings)
{
    return settings.theme.has_value() && settings.theme.value().name() == "ubuntu-aubergine" &&
           settings.ui_language == core::DisplayLanguageChoice::english && settings.lsp &&
           settings.explorer && settings.scrolloff == 3 && settings.explorer_width == 320 &&
           settings.lsp_commands.size() == 2 && settings.mapleader == "<Space>" &&
           settings.maps.size() == 1 && settings.autoindent &&
           settings.finder_preview == core::FinderPreview::right && !settings.caret_motion &&
           settings.caret_motion_time == 120 && !settings.caret_sparks &&
           settings.pane_resize_step == 5 && !settings.pane_resize_step_vim &&
           settings.indent_guide_style == core::IndentGuideStyle::rainbow &&
           settings.indent_guide_color_count == 4 && settings.indent_guide_highlight_current &&
           settings.indent_guide_width == 4;
}

void verify_settings_v6_codec(Expect expect)
{
    verify_released_v5(expect);
    verify_v6_keys(expect);
    verify_indent_guide_keys(expect);
    verify_pane_resize_keys(expect);
}

void verify_v3_refuses_later_keys(Expect expect, std::string_view v3_settings)
{
    for (const auto extra : {"ui_language=system\n",
                             "lsp=off\n",
                             "explorer=off\n",
                             "root=\n",
                             "features=\n",
                             "ignorecase=off\n",
                             "smartcase=off\n",
                             "relativenumber=off\n",
                             "scrolloff=0\n",
                             "explorer_width=\n",
                             "explorer_width=300\n",
                             "autoindent=off\n",
                             "autoindent=on\n",
                             "finder_preview=auto\n",
                             "finder_preview=right\n",
                             "caret_motion=off\n",
                             "caret_motion_time=120\n",
                             "caret_sparks=off\n",
                             "pane_resize_step=5\n",
                             "pane_resize_step_vim=off\n",
                             "indent_guide_style=rainbow\n",
                             "indent_guide_color_count=4\n",
                             "indent_guide_highlight_current=on\n",
                             "indent_guide_width=4\n"})
    {
        const auto refused = decode_settings(std::string(v3_settings) + extra, SettingsVersion::v3);
        expect(!refused && refused.error() == SettingsFailure::malformed,
               "an older version rejects every v4 and v5 key as unknown");
    }
}
} // namespace nenenib::tests::adapters
