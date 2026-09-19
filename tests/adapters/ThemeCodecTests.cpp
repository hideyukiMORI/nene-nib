// Issue #68: 利用者テーマと共通field解析の直接影響だけを検証する。
#include "BuiltinThemes.hpp"
#include "ColorCodec.hpp"
#include "KeyValueFields.hpp"
#include "SettingsCodec.hpp"
#include "ThemeCodec.hpp"
#include "ThemeContrast.hpp"
#include "ThemeDerivation.hpp"
#include "Utf16.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32SettingsAdapter.hpp"
#include "Win32ThemeAdapter.hpp"

#include <windows.h>

#include <cstdio>
#include <string>
#include <utility>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using Failure = nenenib::core::ThemeFailure;

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

std::string fixture()
{
    return "version=1\nname=my-theme\nappearance=dark\nauthor=hide\nlicense=MIT\nurl=local\n"
           "body.foreground=#EEEEEC\nbody.background=#300A24\nbody.cursor=#E95420\n"
           "body.selection=#010101\nbody.current_line=#020202\nbody.line_number=#030303\n"
           "body.comment=#040404\nbody.keyword=#050505\nbody.string=#060606\n"
           "body.number=#070707\nbody.type=#080808\nbody.function=#090909\n"
           "body.constant=#101010\nbody.operator=#111111\nbody.error=#121212\nbody.warning=#"
           "131313\n";
}

core::ThemeName name_of(std::string_view text = "my-theme")
{
    return core::ThemeName::parse(text).value();
}

std::string changed(std::string text, std::string_view before, std::string_view after)
{
    const auto at = text.find(before);
    expect(at != std::string::npos, "test replacement has a target");
    text.replace(at, before.size(), after);
    return text;
}

void rejected(std::string_view text, Failure failure)
{
    const auto result = adapters::decode_theme(text, name_of());
    expect(!result && result.error() == failure, "theme reports its specific failure");
}

void verify_colors()
{
    expect(adapters::decode_rgb("#aBcDeF").value() == core::RgbColor{0xAB, 0xCD, 0xEF},
           "RGB channels and case");
    expect(adapters::decode_rgba("#12345678").value() ==
               core::RgbaColor{core::RgbColor{0x12, 0x34, 0x56}, 0x78},
           "RGBA uses alpha last");
    expect(adapters::decode_rgba("#FFFFFFFF").has_value(), "maximum RGBA accepted");
    for (const auto text : {"", "#12345", "#1234567", "123456", "#GG0000", "#-00001", "#+00001",
                            "#12 456", "#0x1234"})
    {
        expect(!adapters::decode_rgb(text), "RGB format strictly rejects malformed values");
    }
    for (const auto text : {"#123456", "#123456789", "#123456GG", "#+1234567"})
    {
        expect(!adapters::decode_rgba(text), "RGBA format is distinct from RGB");
    }
}

void verify_body()
{
    const auto document = adapters::decode_theme(fixture(), name_of()).value();
    const auto &body = document.body;
    expect(body.foreground == core::RgbColor{0xEE, 0xEE, 0xEC} &&
               body.background == core::RgbColor{0x30, 0x0A, 0x24},
           "body foreground and background mapped");
    expect(body.cursor == core::RgbColor{0xE9, 0x54, 0x20} &&
               body.selection == core::RgbColor{1, 1, 1},
           "body cursor and selection mapped");
    expect(body.current_line == core::RgbColor{2, 2, 2} &&
               body.line_number == core::RgbColor{3, 3, 3},
           "body line tokens mapped");
    expect(body.comment == core::RgbColor{4, 4, 4} && body.keyword == core::RgbColor{5, 5, 5},
           "body comment and keyword mapped");
    expect(body.string == core::RgbColor{6, 6, 6} && body.number == core::RgbColor{7, 7, 7},
           "body literals mapped");
    expect(body.type == core::RgbColor{8, 8, 8} && body.function == core::RgbColor{9, 9, 9},
           "body type and function mapped");
    expect(body.constant == core::RgbColor{0x10, 0x10, 0x10} &&
               body.operators == core::RgbColor{0x11, 0x11, 0x11},
           "body constant and operator mapped");
    expect(body.error == core::RgbColor{0x12, 0x12, 0x12} &&
               body.warning == core::RgbColor{0x13, 0x13, 0x13},
           "body diagnostics mapped");
    const auto derived =
        core::derive_ui(body.background, body.foreground, body.cursor, document.appearance);
    expect(document.ui.panel == derived.panel && document.ui.selection == derived.selection &&
               document.ui.text == derived.text,
           "UI follows the existing derivation");
    const auto light =
        adapters::decode_theme(changed(fixture(), "appearance=dark", "appearance=light"), name_of())
            .value();
    expect(light.appearance == core::Appearance::light &&
               light.ui.panel == core::RgbColor{255, 255, 255},
           "appearance controls derived panel");
}

void verify_overrides()
{
    const auto document =
        adapters::decode_theme(
            fixture() +
                "ui.background=#000000\nui.text=#FFFFFF\nui.muted=#010101\nui.gutter=#020202\n"
                "ui.current_line=#030303\nui.title_bar=#040404\nui.tab_active=#050505\nui.status=#"
                "060606\n"
                "ui.accent=#070707\nui.toggle=#080808\nui.on_accent=#090909\nui.panel=#101010\n"
                "ui.panel_border=#111111\nui.ime=#121212\nui.selection=#12345678\nui.search=#"
                "90abcdef\n",
            name_of())
            .value();
    const auto &ui = document.ui;
    expect(ui.background == core::RgbColor{0, 0, 0} && ui.text == core::RgbColor{255, 255, 255},
           "UI foreground override");
    expect(ui.muted == core::RgbColor{1, 1, 1} && ui.gutter == core::RgbColor{2, 2, 2},
           "UI secondary text override");
    expect(ui.current_line == core::RgbColor{3, 3, 3} && ui.title_bar == core::RgbColor{4, 4, 4},
           "UI line and title override");
    expect(ui.tab_active == core::RgbColor{5, 5, 5} && ui.status == core::RgbColor{6, 6, 6},
           "UI bars override");
    expect(ui.accent == core::RgbColor{7, 7, 7} && ui.toggle == core::RgbColor{8, 8, 8},
           "UI accent override");
    expect(ui.on_accent == core::RgbColor{9, 9, 9} && ui.panel == core::RgbColor{0x10, 0x10, 0x10},
           "UI surfaces override");
    expect(ui.panel_border == core::RgbColor{0x11, 0x11, 0x11} &&
               ui.ime == core::RgbColor{0x12, 0x12, 0x12},
           "UI border and IME override");
    expect(ui.selection == core::RgbaColor{core::RgbColor{0x12, 0x34, 0x56}, 0x78} &&
               ui.search == core::RgbaColor{core::RgbColor{0x90, 0xAB, 0xCD}, 0xEF},
           "UI alpha overrides");
    expect(document.body.foreground == core::RgbColor{0xEE, 0xEE, 0xEC},
           "UI override does not rewrite body");
}

void verify_rejected_fields()
{
    rejected("", Failure::missing_field);
    rejected(fixture() + "broken\n", Failure::malformed);
    rejected(fixture() + "=value\n", Failure::malformed);
    rejected(fixture() + "\tversion = 1\n", Failure::duplicate_key);
    rejected(fixture() + "body.comment=#555555\n", Failure::duplicate_key);
    rejected(fixture() + "unknown=yes\n", Failure::unknown_key);
    rejected(fixture() + "body.operators=#000000\n", Failure::unknown_key);
    rejected(changed(fixture(), "version=1", "version=2"), Failure::unsupported_version);
    rejected(changed(fixture(), "license=MIT\n", ""), Failure::missing_field);
    rejected(changed(fixture(), "body.warning=#131313\n", ""), Failure::missing_field);
    rejected(changed(fixture(), "appearance=dark", "appearance=automatic"),
             Failure::invalid_appearance);
    rejected(changed(fixture(), "name=my-theme", "name=My-Theme"), Failure::invalid_name);
    rejected(changed(fixture(), "name=my-theme", "name=other"), Failure::name_mismatch);
    for (const auto name : {"system", "dracula", "solarized_dark"})
    {
        const auto result = adapters::decode_theme(
            changed(fixture(), "name=my-theme", "name=" + std::string(name)), name_of(name));
        expect(!result && result.error() == Failure::reserved_name,
               "built-in names and aliases cannot be overridden");
    }
    rejected(changed(fixture(), "author=hide", "author="), Failure::invalid_text);
    rejected(changed(fixture(), "author=hide", "author=" + std::string(257, 'x')),
             Failure::invalid_text);
    rejected(changed(fixture(), "author=hide", "author=bad\ttext"), Failure::invalid_text);
    rejected(fixture() + std::string("\xFF"), Failure::invalid_text);
    rejected(changed(fixture(), "#EEEEEC", "#EEEEECFF"), Failure::invalid_color);
    rejected(fixture() + "ui.selection=#123456\n", Failure::invalid_color);
    rejected(fixture() + "ui.accent=#12345678\n", Failure::invalid_color);
}

void verify_contrast_and_limits()
{
    for (const auto &theme : core::builtin_themes)
    {
        expect(adapters::theme_has_contrast(theme),
               "existing accepted palettes satisfy the file validator");
    }
    rejected(changed(fixture(), "#EEEEEC", "#300A24"), Failure::insufficient_contrast);
    rejected(fixture() + "ui.text=#111111\nui.background=#111111\n",
             Failure::insufficient_contrast);
    auto white = changed(changed(fixture(), "#300A24", "#FFFFFF"), "#EEEEEC", "#767676");
    expect(adapters::decode_theme(white, name_of()).has_value(), "4.54 contrast is accepted");
    rejected(changed(white, "#767676", "#777777"), Failure::insufficient_contrast);
    auto at_limit = fixture();
    at_limit.append(adapters::maximum_theme_bytes - at_limit.size(), '\n');
    expect(adapters::decode_theme(at_limit, name_of()).has_value(), "exact byte limit accepted");
    rejected(at_limit + "\n", Failure::too_large);
    auto crlf = fixture();
    std::size_t at = 0;
    while ((at = crlf.find('\n', at)) != std::string::npos)
    {
        crlf.insert(at, "\r");
        at += 2;
    }
    expect(adapters::decode_theme("\xEF\xBB\xBF\r\n" + crlf, name_of()).has_value(),
           "BOM and CRLF accepted");
    const auto spaced = changed(fixture(), "name=my-theme", "\v name \t= my_theme \f");
    expect(adapters::decode_theme(spaced, name_of()).has_value(),
           "ASCII whitespace and normalized name accepted");
}

void verify_filename_contract(adapters::Win32FileAdapter &files)
{
    for (const auto filename : {"nib-theme-files/wrong.txt", "nib-theme-files/my_theme.v1.theme",
                                "nib-theme-files/.v1.theme"})
    {
        const auto invalid_name =
            adapters::load_theme(files, core::FilePath::parse(filename).value());
        expect(!invalid_name && invalid_name.error() == Failure::invalid_name,
               "loader requires canonical basename and extension before reading");
    }
}

void verify_file_loading()
{
    constexpr wchar_t folder[] = L"nib-theme-files";
    constexpr wchar_t file[] = L"nib-theme-files/my-theme.v1.theme";
    DeleteFileW(file);
    DeleteFileW(L"nib-theme-files/my-theme.v1.theme.nib-tmp");
    const BOOL created = CreateDirectoryW(folder, nullptr);
    expect(created != 0 || GetLastError() == ERROR_ALREADY_EXISTS, "isolated theme folder exists");
    adapters::Win32FileAdapter files;
    const auto path = core::FilePath::parse("nib-theme-files/my-theme.v1.theme").value();
    const auto missing = adapters::load_theme(files, path);
    expect(!missing && missing.error() == Failure::not_found,
           "missing themes have a distinct failure");
    expect(files.write(path, fixture()).has_value(), "theme fixture is written");
    const auto loaded = adapters::load_theme(files, path);
    expect(loaded.has_value() && loaded.value().name == name_of(),
           "bounded file load decodes a theme");
    verify_filename_contract(files);
    expect(files.read(path, adapters::maximum_theme_bytes).value_or("") == fixture(),
           "load never writes the theme");
    expect(files.write(path, changed(fixture(), "name=my-theme", "name=another-theme")).has_value(),
           "mismatched name fixture written");
    const auto mismatched = adapters::load_theme(files, path);
    expect(!mismatched && mismatched.error() == Failure::name_mismatch,
           "basename determines the expected document name");
    expect(files.write(path, fixture() + "invalid\n").has_value(), "invalid fixture is written");
    const auto invalid = adapters::load_theme(files, path);
    expect(!invalid && invalid.error() == Failure::malformed,
           "file load preserves codec diagnostics");
    expect(files.write(path, std::string(adapters::maximum_theme_bytes + 1, 'x')).has_value(),
           "oversized fixture is written");
    const auto large = adapters::load_theme(files, path);
    expect(!large && large.error() == Failure::too_large, "read limit rejects before parsing");
    const BOOL subfolder = CreateDirectoryW(L"nib-theme-files/unreadable.v1.theme", nullptr);
    expect(subfolder != 0 || GetLastError() == ERROR_ALREADY_EXISTS, "directory fixture exists");
    const auto directory = core::FilePath::parse("nib-theme-files/unreadable.v1.theme").value();
    const auto unreadable = adapters::load_theme(files, directory);
    expect(!unreadable && unreadable.error() == Failure::unreadable,
           "directory is not a missing theme");
    expect(RemoveDirectoryW(L"nib-theme-files/unreadable.v1.theme") != 0,
           "directory fixture removed");
    expect(DeleteFileW(file) != 0 && RemoveDirectoryW(folder) != 0,
           "fixture is removed without touching other paths");
}

void verify_owned_view()
{
    auto original =
        adapters::decode_theme(changed(fixture(), "author=hide", "author=作者=hide"), name_of())
            .value();
    auto copy = original;
    original = adapters::decode_theme(fixture(), name_of()).value();
    auto moved = std::move(copy);
    const auto view = core::theme_view(moved);
    expect(view.name == "my-theme" && view.source.author == "作者=hide",
           "copy and move retain independent owned strings after input destruction");
    expect(view.source.license == "MIT" && view.source.url == "local",
           "all source fields are owned");
    expect(view.ui.text == moved.ui.text && view.body.keyword == moved.body.keyword,
           "Theme view carries identical colors");
}
core::FilePath catalog_path(std::string_view name)
{
    return core::FilePath::parse("nib-theme-catalog/" + std::string(name)).value();
}

void put_theme(adapters::Win32FileAdapter &files, std::string_view filename, std::string_view bytes)
{
    expect(files.write(catalog_path(filename), bytes).has_value(), "catalog fixture written");
}

void remove_fixture(std::string_view name)
{
    const auto path = core::to_utf16(catalog_path(name).text()).value();
    expect(DeleteFileW(path.c_str()) != 0, "catalog fixture removed");
}

void verify_catalog_restore(adapters::Win32FileAdapter &files, const core::ThemeCatalog &catalog)
{
    auto settings = core::default_editor_settings();
    settings.theme = catalog.find(name_of()).value();
    const auto encoded = adapters::encode_settings(settings);
    expect(encoded.find("colorscheme=my-theme\n") != std::string::npos,
           "settings store only canonical name");
    const auto decoded = adapters::decode_settings(encoded, catalog);
    expect(decoded && core::same_settings(decoded.value(), settings),
           "settings restore user choice from catalog");
    const auto path = catalog_path("settings.v1");
    adapters::Win32SettingsAdapter store(files, path);
    expect(store.read(catalog).has_value() && store.write(settings).has_value(),
           "adapter saves selected theme");
    adapters::Win32SettingsAdapter restarted(files, path);
    expect(core::same_settings(
               restarted.read(catalog).value().value_or(core::default_editor_settings()), settings),
           "new adapter restores same user data");
    for (const auto &bad :
         {core::ThemeCatalog::builtins(),
          core::ThemeCatalog::from({{name_of(), std::unexpected(Failure::invalid_color)}}).value()})
    {
        adapters::Win32SettingsAdapter blocked(files, path);
        const auto reading = blocked.read(bad);
        expect(!reading && std::get<core::ThemeLookupFailure>(reading.error()).name == name_of(),
               "missing or broken saved theme returns its name");
        const auto writing = blocked.write(core::default_editor_settings());
        expect(!writing && writing.error() == reading.error(),
               "named startup error also blocks later writes");
        expect(files.read(path, 4096).value() == encoded, "blocked write preserves original bytes");
    }
    remove_fixture("settings.v1");
    remove_fixture("settings.v1.lock");
}

void verify_catalog_files(adapters::Win32FileAdapter &files, adapters::Win32ThemeAdapter &adapter)
{
    put_theme(files, "z-theme.v1.theme", changed(fixture(), "my-theme", "z-theme"));
    put_theme(files, "broken.v1.theme", "broken");
    put_theme(files, "my-theme.v1.theme", fixture());
    put_theme(files, "invalid_name.v1.theme", "broken");
    put_theme(files, "dracula.v1.theme", "broken");
    put_theme(files, "ignored.txt", "ignored");
    const auto inventory = adapter.read();
    expect(inventory.catalog.records().size() == 3 && inventory.catalog.names().back() == "z-theme",
           "sorted catalog keeps valid and failed themes, skips noncanonical and reserved names");
    expect(inventory.notice.has_value() &&
               inventory.notice.value().text().starts_with("dracula.v1.theme:"),
           "first invalid filename is reported deterministically");
    const auto selected = inventory.catalog.find(name_of()).value();
    expect(selected.name() == "my-theme", "valid file is selectable");
    expect(inventory.catalog.find(name_of("broken")).error().reason == Failure::malformed,
           "failed file is selectable with original reason");
    verify_catalog_restore(files, inventory.catalog);
    for (const auto name : {"z-theme.v1.theme", "broken.v1.theme", "my-theme.v1.theme",
                            "invalid_name.v1.theme", "dracula.v1.theme", "ignored.txt"})
    {
        remove_fixture(name);
    }
    std::string unicode;
    for (std::size_t index = 0; index < 100; ++index)
    {
        unicode += "界";
    }
    unicode += ".v1.theme";
    put_theme(files, unicode, "invalid");
    const auto invalid = adapter.read();
    expect(invalid.notice.has_value() && invalid.notice.value().text().size() <= 256,
           "long Unicode filename produces bounded valid UTF-8 notice");
    remove_fixture(unicode);
}

void verify_catalog_limit(adapters::Win32FileAdapter &files, adapters::Win32ThemeAdapter &adapter)
{
    for (std::size_t index = 0; index < adapters::maximum_theme_files; ++index)
    {
        put_theme(files, "t-" + std::to_string(index) + ".v1.theme", "broken");
    }
    const auto limit = adapter.read();
    expect(limit.catalog.records().size() == 128, "exactly 128 themes are retained");
    put_theme(files, "overflow.v1.theme", "broken");
    const auto overflow = adapter.read();
    expect(overflow.catalog.records().empty() && overflow.notice.has_value() &&
               overflow.notice.value().text().find("128") != std::string::npos,
           "overflow rejects the whole user set instead of an arbitrary subset");
    remove_fixture("overflow.v1.theme");
    for (std::size_t index = 0; index < adapters::maximum_theme_files; ++index)
    {
        remove_fixture("t-" + std::to_string(index) + ".v1.theme");
    }
}

void verify_catalog_loading()
{
    expect(CreateDirectoryW(L"nib-theme-catalog", nullptr) != 0,
           "isolated catalog directory created");
    adapters::Win32FileAdapter files;
    adapters::Win32ThemeAdapter missing(files, catalog_path("missing"));
    const auto empty = missing.read();
    expect(empty.catalog.records().empty() && !empty.notice.has_value(),
           "absent directory is normal");
    adapters::Win32ThemeAdapter unavailable(files, std::unexpected(Failure::unreadable));
    expect(unavailable.read().notice.has_value(), "unknown location is diagnosed");
    adapters::Win32ThemeAdapter adapter(files, core::FilePath::parse("nib-theme-catalog").value());
    expect(CreateDirectoryW(L"nib-theme-catalog/nested.v1.theme", nullptr) != 0,
           "nested directory created");
    put_theme(files, "nested.v1.theme/hidden.v1.theme", fixture());
    const auto nested = adapter.read();
    expect(nested.catalog.records().empty(), "directories are not followed recursively");
    remove_fixture("nested.v1.theme/hidden.v1.theme");
    expect(RemoveDirectoryW(L"nib-theme-catalog/nested.v1.theme") != 0, "nested fixture removed");
    verify_catalog_files(files, adapter);
    verify_catalog_limit(files, adapter);
    expect(RemoveDirectoryW(L"nib-theme-catalog") != 0, "catalog directory removed");
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--catalog")
    {
        verify_catalog_loading();
        std::printf("Theme catalog: %zu checks, %zu failures\n", checks(), failures());
        return failures() == 0 ? 0 : 1;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--file-loading")
    {
        verify_file_loading();
        std::printf("Theme file loading: %zu checks, %zu failures\n", checks(), failures());
        return failures() == 0 ? 0 : 1;
    }
    verify_catalog_loading();
    verify_colors();
    verify_body();
    verify_overrides();
    verify_owned_view();
    verify_rejected_fields();
    verify_contrast_and_limits();
    verify_file_loading();
    std::printf("Theme codec: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
