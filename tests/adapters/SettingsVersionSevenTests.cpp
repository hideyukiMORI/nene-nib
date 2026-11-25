// settings.v7 の codec と置き場の試験（ADR 0193 の決定 4 と 2026-11-24 の追補 2・#1977）。1.2.0 の
// v6 の見本を 1.2.0 と同じ厳しさで読み、v7 の 4 つの鍵に既定を補って v7 の形で書き戻す。v7 の鍵の
// 往復・欠けた鍵の既定・形の違いの拒否・v6 以前の拒否。置き場は v7 が無いときだけ v6 を読み、v6 を
// bytes のまま残し、壊れた v7 は v6 へ戻らない。
#include "SettingsVersionSevenTests.hpp"

#include "EditorSettings.hpp"
#include "Invisibles.hpp"
#include "ListSpace.hpp"
#include "SettingsCodec.hpp"
#include "SettingsFailure.hpp"
#include "SettingsReleasedSixTests.hpp"
#include "SettingsVersion.hpp"
#include "Win32SettingsAdapter.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

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

constexpr std::array<std::string_view, 4> list_keys{"list_eol", "list_tab", "list_wide_space",
                                                    "list_space"};

// key の行の値を value に替えた bytes。
std::string with_value(std::string bytes, std::string_view key, std::string_view value)
{
    const auto start = bytes.find("\n" + std::string(key) + "=") + 1 + key.size() + 1;
    bytes.replace(start, bytes.find('\n', start) - start, value);
    return bytes;
}

// key の行を外した bytes。
std::string without_key(std::string bytes, std::string_view key)
{
    const auto start = bytes.find("\n" + std::string(key) + "=") + 1;
    bytes.erase(start, bytes.find('\n', start) + 1 - start);
    return bytes;
}

// 1.2.0 の v6 と 1.1.0 の v5 を読むと不可視文字の既定を補い、書くと v7 の形になる。v6 以前に v7 の
// 鍵が在れば未知の鍵として拒む（v6 の読みは 1.2.0 と同じ厳しさ）。
void verify_older_written_as_v7(Expect expect)
{
    const auto six = decode_settings(released_v6_settings, SettingsVersion::v6);
    const auto defaults = core::default_editor_settings();
    expect(six && six.value().list_eol && six.value().list_tab && six.value().list_wide_space &&
               six.value().list_space == core::ListSpace::trail && defaults.list_eol &&
               defaults.list_space == core::ListSpace::trail,
           "the 1.2.0 v6 and the defaults take on, on, on and trail");
    expect(six && encode_settings(six.value()) == v6_as_v7(released_v6_settings),
           "the 1.2.0 v6 is written back in the v7 form with the list keys after "
           "indent_guide_width");
    const auto five = decode_settings(released_v5_settings, SettingsVersion::v5);
    expect(five && encode_settings(five.value()).starts_with("version=7\n") &&
               encode_settings(five.value())
                       .find(std::string("indent_guide_width=4\n") + std::string(list_defaults)) !=
                   std::string::npos,
           "the 1.1.0 v5 is written back in the v7 form");
    for (const auto extra :
         {"list_eol=on\n", "list_tab=off\n", "list_wide_space=on\n", "list_space=trail\n"})
    {
        const auto refused =
            decode_settings(std::string(released_v6_settings) + extra, SettingsVersion::v6);
        const auto older =
            decode_settings(std::string(released_v5_settings) + extra, SettingsVersion::v5);
        expect(!refused && refused.error() == SettingsFailure::malformed && !older &&
                   older.error() == SettingsFailure::malformed,
               "a v6 or a v5 holding a v7 key is refused as an unknown key");
    }
    const auto seven = v6_as_v7(released_v6_settings);
    const auto as_v6 = decode_settings(seven, SettingsVersion::v6);
    const auto v6_as_seven = decode_settings(released_v6_settings, SettingsVersion::v7);
    expect(!as_v6 && as_v6.error() == SettingsFailure::unsupported_version && !v6_as_seven &&
               v6_as_seven.error() == SettingsFailure::unsupported_version,
           "a v7 is not read as v6 and a v6 is not read as v7");
}

// v7 の 4 つの鍵。並びは indent_guide_width の後ろ・lsp の行の前。無ければ既定、有れば厳密に読み、
// 往復する。
void verify_list_keys(Expect expect)
{
    auto settings = core::default_editor_settings();
    settings.list_eol = false;
    settings.list_tab = false;
    settings.list_wide_space = false;
    settings.list_space = core::ListSpace::all;
    settings.lsp_commands = {{"cpp", "C:\\llvm\\clangd.exe"}};
    const auto bytes = encode_settings(settings);
    const auto read = decode_settings(bytes, SettingsVersion::v7);
    expect(read && core::same_settings(read.value(), settings) &&
               bytes.find("\nindent_guide_width=4\nlist_eol=off\nlist_tab=off\n"
                          "list_wide_space=off\nlist_space=all\nlsp.cpp.command=") !=
                   std::string::npos,
           "the list keys follow indent_guide_width and round trip through v7");
    for (const auto space : {"none", "trail"})
    {
        const auto chosen =
            decode_settings(with_value(bytes, "list_space", space), SettingsVersion::v7);
        expect(chosen && core::list_space_name(chosen.value().list_space) == space,
               "list_space reads none and trail");
    }
    std::string bare = bytes;
    for (const auto key : list_keys)
    {
        bare = without_key(bare, key);
    }
    const auto missing = decode_settings(bare, SettingsVersion::v7);
    expect(missing && missing.value().list_eol && missing.value().list_tab &&
               missing.value().list_wide_space &&
               missing.value().list_space == core::ListSpace::trail,
           "a v7 without the list keys takes the defaults");
    for (const auto &[key, value] :
         {std::pair{"list_eol", "yes"}, std::pair{"list_eol", ""}, std::pair{"list_tab", "1"},
          std::pair{"list_wide_space", "On"}, std::pair{"list_space", "trailing"},
          std::pair{"list_space", ""}, std::pair{"list_space", "All"}})
    {
        const auto broken = decode_settings(with_value(bytes, key, value), SettingsVersion::v7);
        expect(!broken && broken.error() == SettingsFailure::malformed,
               "a malformed list value is refused");
    }
    expect(!decode_settings(bytes + "list_space=none\n", SettingsVersion::v7) &&
               !decode_settings(bytes + "list_tab=on\n", SettingsVersion::v7),
           "a duplicate list key is refused");
}
} // namespace

std::string v6_as_v7(std::string_view bytes)
{
    std::string seven(bytes);
    seven.replace(0, std::string_view{"version=6"}.size(), "version=7");
    const auto width = seven.find("\nindent_guide_width=");
    const auto end = seven.find('\n', width + 1) + 1;
    seven.insert(end, list_defaults);
    return seven;
}

void verify_settings_v7_codec(Expect expect)
{
    verify_older_written_as_v7(expect);
    verify_list_keys(expect);
}

void verify_settings_v7_files(Expect expect, Win32FileAdapter &files, const SettingsPaths &paths)
{
    expect(files.write(paths.delivered, released_v6_settings).has_value() &&
               files.write(paths.shipped, released_v5_settings).has_value(),
           "prepare the 1.2.0 v6 above the 1.1.0 v5 without a v7");
    {
        Win32SettingsAdapter store(files, paths);
        const auto reading = store.read();
        const auto read = reading.value_or(std::nullopt);
        expect(read.has_value() &&
                   released_v6_values(read.value_or(core::default_editor_settings())) &&
                   read.value_or(core::default_editor_settings()).list_space ==
                       core::ListSpace::trail,
               "without a v7 the 1.2.0 v6 is read strictly with the list defaults");
        expect(!files.read(paths.current, 4096).has_value(), "reading writes no v7");
        auto changed = read.value_or(core::default_editor_settings());
        changed.list_space = core::ListSpace::all;
        expect(store.write(changed).has_value(), "a settings change writes the v7");
        const auto current = files.read(paths.current, 4096);
        const auto delivered = files.read(paths.delivered, 4096);
        expect(current && current.value().view() == encode_settings(changed) && delivered &&
                   delivered.value().view() == released_v6_settings,
               "the change goes to v7 and the v6 stays byte for byte");
    }
    expect(files.write(paths.current, "version=7\nbroken").has_value(), "break the v7");
    Win32SettingsAdapter broken(files, paths);
    const auto refused = broken.read();
    expect(!refused && refused.error() == SettingsFailure::malformed &&
               !broken.write(core::default_editor_settings()),
           "a broken v7 does not fall back to v6 and blocks saving");
    const auto delivered = files.read(paths.delivered, 4096);
    expect(delivered && delivered.value().view() == released_v6_settings,
           "the v6 under a broken v7 stays byte for byte");
}
} // namespace nenenib::tests::adapters
