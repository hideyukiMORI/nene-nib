// adapters/win32 のファイルと文字コードの往復を、ビルドディレクトリ配下の一時フォルダで測る
// （QLT-013 / ADR 0010）。中核の分岐カバレッジ（QLT-009）の対象ではない。
// 一時フォルダの名前は固定で、冒頭で消して作り直す。時刻も乱数も使わない（ARC-007）。
#include "AbsolutePath.hpp"
#include "CodePageFailure.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "FileHandle.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "SettingsCodec.hpp"
#include "TextEncoding.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32SettingsAdapter.hpp"
#include "Win32TimingAdapter.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

namespace
{
using nenenib::adapters::win32::absolute_file_path;
using nenenib::adapters::win32::decode_settings;
using nenenib::adapters::win32::encode_settings;
using nenenib::adapters::win32::FileHandle;
using nenenib::adapters::win32::Win32CodePageAdapter;
using nenenib::adapters::win32::Win32FileAdapter;
using nenenib::adapters::win32::Win32SettingsAdapter;
using nenenib::adapters::win32::Win32TimingAdapter;
using nenenib::application::CodePageFailure;
using nenenib::application::FileFailure;
using nenenib::application::SettingsFailure;
using nenenib::core::byte_order_mark;
using nenenib::core::default_editor_settings;
using nenenib::core::detect_encoding;
using nenenib::core::FilePath;
using nenenib::core::Milestone;
using nenenib::core::TextEncoding;

// application が渡す上限と同じ値（ADR 0010 の決定 12）。正本は EditorController にある。
constexpr std::size_t read_limit = 64U * 1024U * 1024U;
constexpr wchar_t folder[] = L"nib-adapter-files";
constexpr wchar_t recorded_marks[] = L"nib-adapter-files/timing.json";
constexpr wchar_t unrecorded_marks[] = L"nib-adapter-files/silent.json";
constexpr std::array<const wchar_t *, 8> file_names{L"nib-adapter-files/utf8.txt",
                                                    L"nib-adapter-files/bom.txt",
                                                    L"nib-adapter-files/sjis.txt",
                                                    L"nib-adapter-files/replaced.txt",
                                                    recorded_marks,
                                                    unrecorded_marks,
                                                    L"nib-adapter-files/settings/settings.v1",
                                                    L"nib-adapter-files/settings/settings.v1.lock"};

std::size_t &failure_count()
{
    static std::size_t failures = 0;
    return failures;
}

std::size_t &check_count()
{
    static std::size_t checks = 0;
    return checks;
}

void expect(bool condition, const char *description)
{
    ++check_count();
    if (!condition)
    {
        ++failure_count();
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

// 固定の名前なので、前回の実行が残していたものを消してから作り直す。
void reset_folder()
{
    for (const wchar_t *name : file_names)
    {
        DeleteFileW(name);
        DeleteFileW((std::wstring(name) + L".nib-tmp").c_str());
    }
    RemoveDirectoryW(L"nib-adapter-files/settings");
    RemoveDirectoryW(folder);
    const BOOL created = CreateDirectoryW(folder, nullptr);
    expect(created != 0 || GetLastError() == ERROR_ALREADY_EXISTS, "the temporary folder is there");
}

FilePath path_of(std::string_view name)
{
    auto parsed = FilePath::parse(name);
    expect(parsed.has_value(), "the test path is valid");
    return std::move(parsed).value();
}

void verify_round_trip(Win32FileAdapter &files, std::string_view name, std::string_view bytes,
                       TextEncoding expected)
{
    const FilePath path = path_of(name);
    expect(files.write(path, bytes).has_value(), "the bytes are written");
    const auto read = files.read(path, read_limit);
    expect(read.has_value() && read.value() == bytes, "the same bytes come back");
    const auto detected = detect_encoding(read.value_or(std::string{}));
    expect(detected.has_value() && detected.value() == expected, "the encoding is detected");
}

void verify_utf8_files(Win32FileAdapter &files)
{
    const std::string utf8 = "一行目\r\n二行目\r\n三行目";
    verify_round_trip(files, "nib-adapter-files/utf8.txt", utf8, TextEncoding::utf8);
    verify_round_trip(files, "nib-adapter-files/bom.txt", std::string(byte_order_mark()) + utf8,
                      TextEncoding::utf8_bom);
}

void verify_shift_jis_file(Win32FileAdapter &files, Win32CodePageAdapter &code_pages)
{
    const auto encoded = code_pages.from_utf8("日本語\n改行");
    expect(encoded.has_value(), "Japanese text fits in CP932");
    verify_round_trip(files, "nib-adapter-files/sjis.txt", encoded.value_or(std::string{}),
                      TextEncoding::shift_jis);
    const auto decoded = code_pages.to_utf8(encoded.value());
    expect(decoded.has_value() && decoded.value() == "日本語\n改行", "CP932 comes back as UTF-8");
}

void verify_replacement(Win32FileAdapter &files)
{
    const FilePath path = path_of("nib-adapter-files/replaced.txt");
    expect(files.write(path, "first content").has_value(), "the first content is written");
    expect(files.write(path, "second").has_value(), "the second content replaces it");
    const auto read = files.read(path, read_limit);
    expect(read.has_value() && read.value() == "second", "the replacement is what comes back");
    // 一時ファイルは置き換えのあとに残らない。
    expect(GetFileAttributesW(L"nib-adapter-files/replaced.txt.nib-tmp") == INVALID_FILE_ATTRIBUTES,
           "the temporary file is gone");
}

void verify_failures(Win32FileAdapter &files)
{
    const auto missing = files.read(path_of("nib-adapter-files/absent.txt"), read_limit);
    expect(!missing.has_value() && missing.error() == FileFailure::not_found,
           "an absent path is not_found");
    const auto directory = files.read(path_of("nib-adapter-files"), read_limit);
    expect(!directory.has_value() && directory.error() == FileFailure::unreadable,
           "a folder is unreadable");
    const auto refused = files.write(path_of("nib-adapter-files/absent/deep.txt"), "x");
    expect(!refused.has_value(), "writing into a folder that is not there fails");
    // 上限は引数で来る。小さく渡せば、読む前に断ることが見える。
    const auto capped = files.read(path_of("nib-adapter-files/utf8.txt"), 4);
    expect(!capped.has_value() && capped.error() == FileFailure::too_large,
           "a file larger than the limit the caller passed is too_large");
    // 保存に上限は無い。開ける大きさを超えて貼り付けた本文も書ける。
    const FilePath big = path_of("nib-adapter-files/replaced.txt");
    expect(files.write(big, std::string(1024U * 1024U, 'z')).has_value(),
           "a body larger than the read limit still writes");
    expect(files.read(big, read_limit).value_or(std::string{}).size() == 1024U * 1024U,
           "and it comes back whole");
}

void verify_code_pages(Win32CodePageAdapter &code_pages)
{
    const auto empty = code_pages.from_utf8("");
    expect(empty.has_value() && empty.value().empty(), "an empty body encodes to nothing");
    const auto emoji = code_pages.from_utf8("😀");
    expect(!emoji.has_value() && emoji.error() == CodePageFailure::unencodable,
           "an emoji does not fit in CP932");
    const auto broken = code_pages.to_utf8("\xFF\xFF");
    expect(!broken.has_value() && broken.error() == CodePageFailure::undecodable,
           "bytes that are not CP932 are undecodable");
}

// 節目を積むのは bind のあとだけで、JSON はデストラクタが 1 度だけ書く（ADR 0011 の決定 2）。
void verify_timing_marks(Win32FileAdapter &files)
{
    {
        Win32TimingAdapter timing;
        timing.bind(recorded_marks);
        timing.mark(Milestone::input_received);
        timing.mark(Milestone::frame_presented);
    }
    {
        Win32TimingAdapter silent;
        silent.mark(Milestone::input_received);
    }
    const auto recorded = absolute_file_path(recorded_marks);
    const auto unrecorded = absolute_file_path(unrecorded_marks);
    expect(recorded.has_value() && unrecorded.has_value(), "both measurement paths resolve");
    if (!recorded.has_value() || !unrecorded.has_value())
    {
        return;
    }
    const auto bytes = files.read(recorded.value(), read_limit);
    expect(bytes.has_value(), "the bound adapter wrote its measurement file");
    const std::string_view text = bytes.has_value() ? std::string_view(bytes.value()) : "";
    expect(text.find("\"processCreationToFirstFrameMs\"") != std::string_view::npos,
           "the report opens with the process creation to first frame value");
    // プロセス生成 → bind は節目で測れないので、計測器はこの値を最初の区間に使う（Issue #19）。
    expect(text.find("\"processCreationToOriginMs\"") != std::string_view::npos,
           "the report carries the process creation to origin value");
    expect(text.find("\"qpcFrequency\"") != std::string_view::npos,
           "the report records the counter frequency");
    expect(text.find("\"input_received\"") != std::string_view::npos,
           "the input milestone is in the report");
    expect(text.find("\"frame_presented\"") != std::string_view::npos,
           "the presented milestone is in the report");
    expect(text.find("\"qpcMicroseconds\"") != std::string_view::npos,
           "each milestone carries its counter reading");
    const auto missing = files.read(unrecorded.value(), read_limit);
    expect(!missing.has_value() && missing.error() == FileFailure::not_found,
           "an adapter that was never bound writes nothing");
}

void verify_settings_codec()
{
    const auto defaults = default_editor_settings();
    const auto decoded = decode_settings(encode_settings(defaults));
    expect(decoded.has_value() && same_settings(decoded.value(), defaults),
           "default settings round trip");
    const auto custom = decode_settings("\xEF\xBB\xBF\r\nversion = 1\r\n"
                                        " colorscheme = neutral-light \r\n"
                                        "\vfont_family = 日本語 Font=Test\r\nfont_size=21.25\f");
    expect(custom.has_value(), "BOM, CRLF, whitespace and an equals sign in a font are accepted");
    if (!custom)
    {
        return;
    }
    expect(custom.value().font_family.text() == "日本語 Font=Test" &&
               custom.value().font_size.points() == 21.25F,
           "custom settings retain their values");
    const auto round_trip = decode_settings(encode_settings(custom.value()));
    expect(round_trip.has_value() && same_settings(round_trip.value(), custom.value()),
           "custom settings round trip including an explicit theme");
}

void verify_bad_settings_fields()
{
    const std::string valid = encode_settings(default_editor_settings());
    for (const auto suffix : {"version=1\n", "unknown=yes\n", "broken\n"})
    {
        const auto result = decode_settings(valid + suffix);
        expect(!result && result.error() == SettingsFailure::malformed,
               "duplicate keys, unknown keys and lines without equals are malformed");
    }
    const auto missing = decode_settings("version=1\n");
    expect(!missing && missing.error() == SettingsFailure::malformed, "missing fields fail");
    const auto version = decode_settings("version=2\n" + valid.substr(valid.find('\n') + 1));
    expect(!version && version.error() == SettingsFailure::unsupported_version,
           "an unknown version is incompatible");
    const auto theme = decode_settings("version=1\ncolorscheme=missing\nfont_family=Consolas\n"
                                       "font_size=13.5\n");
    expect(!theme && theme.error() == SettingsFailure::unknown_theme, "unknown themes fail");
}

void verify_bad_settings_values()
{
    for (const auto value : {"", "7.99", "40.01", "nan", "inf", "13,5", "13.5junk", "1e100"})
    {
        const auto result = decode_settings(
            std::string("version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=") + value);
        expect(!result && result.error() == SettingsFailure::invalid_font_size,
               "invalid sizes and non-finite values are rejected");
    }
    for (const auto &value : {std::string{}, std::string("\xFF"), std::string("A\x01"),
                              std::string("A\0B", 3), std::string(257, 'a')})
    {
        const auto result = decode_settings(
            std::string("version=1\ncolorscheme=system\nfont_size=13.5\nfont_family=") + value);
        expect(!result && result.error() == SettingsFailure::invalid_font_family,
               "invalid font names use the domain validator");
    }
}

void verify_settings_restart(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v1");
    Win32SettingsAdapter first(files, path);
    const auto premature = first.write(default_editor_settings());
    expect(!premature && premature.error() == SettingsFailure::not_loaded,
           "writing before loading is refused");
    const auto missing = first.read();
    expect(missing.has_value() && !missing.value().has_value(), "missing settings are optional");
    expect(GetFileAttributesW(L"nib-adapter-files/settings") == INVALID_FILE_ATTRIBUTES,
           "reading defaults creates no settings directory");
    auto settings = default_editor_settings();
    settings.font_size = nenenib::core::FontSize::from_points(21.25F).value();
    expect(first.write(settings).has_value(), "first write creates the settings directory");
    Win32SettingsAdapter restarted(files, path);
    const auto restored = restarted.read();
    expect(restored.has_value(), "a fresh adapter reads the saved file");
    const auto loaded = restored.value_or(std::nullopt);
    expect(loaded.has_value() && same_settings(loaded.value(), settings),
           "restart restores saved settings");
    expect(restarted.write(default_editor_settings()).has_value(), "another window can save");
    const auto stale = first.write(settings);
    expect(!stale && stale.error() == SettingsFailure::changed_externally,
           "stale settings never overwrite another window's update");
    expect(files.read(path, 4096).value_or("") == encode_settings(default_editor_settings()),
           "the competing update remains intact");
}

void verify_settings_lock(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v1");
    Win32SettingsAdapter settings(files, path);
    expect(settings.read().has_value(), "the settings adapter loads before lock checks");
    const auto before = files.read(path, 4096);
    {
        const FileHandle lock(CreateFileW(L"nib-adapter-files/settings/settings.v1.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(lock.valid(), "the competing lock is acquired");
        const auto result = settings.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "a held lock refuses a write without waiting");
        expect(files.read(path, 4096) == before, "lock failure leaves the original bytes intact");
    }
    expect(settings.write(default_editor_settings()).has_value(), "a released lock allows retry");
    expect(SetFileAttributesW(L"nib-adapter-files/settings/settings.v1", FILE_ATTRIBUTE_READONLY) !=
               0,
           "the settings file becomes read only");
    const auto refused = settings.write(default_editor_settings());
    expect(!refused && refused.error() == SettingsFailure::unwritable,
           "read-only files refuse save");
    expect(files.read(path, 4096) == before, "failed replacement preserves settings bytes");
    expect(SetFileAttributesW(L"nib-adapter-files/settings/settings.v1", FILE_ATTRIBUTE_NORMAL) !=
               0,
           "the test restores the original attributes");
}

void verify_rejected_settings(Win32FileAdapter &files, std::string_view bytes,
                              SettingsFailure expected)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v1");
    expect(files.write(path, bytes).has_value(), "the invalid settings fixture is written");
    Win32SettingsAdapter settings(files, path);
    const auto loaded = settings.read();
    expect(!loaded && loaded.error() == expected, "invalid settings return their typed failure");
    const auto written = settings.write(default_editor_settings());
    expect(!written && written.error() == expected, "a failed load blocks later writes");
    expect(files.read(path, read_limit).value_or("") == bytes, "invalid settings remain untouched");
}

void verify_settings_failures(Win32FileAdapter &files)
{
    verify_rejected_settings(files, "invalid", SettingsFailure::malformed);
    verify_rejected_settings(files,
                             "version=2\ncolorscheme=system\nfont_family=Consolas\nfont_size=13.5",
                             SettingsFailure::unsupported_version);
    verify_rejected_settings(files, std::string(4097, 'x'), SettingsFailure::too_large);
    Win32SettingsAdapter unavailable(files, std::unexpected(SettingsFailure::location_unavailable));
    const auto failed = unavailable.read();
    expect(!failed && failed.error() == SettingsFailure::location_unavailable,
           "an unavailable location is distinct from missing settings");
    expect(!unavailable.write(default_editor_settings()), "an unavailable location cannot save");
    Win32SettingsAdapter directory(files, path_of("nib-adapter-files/settings"));
    const auto unreadable = directory.read();
    expect(!unreadable && unreadable.error() == SettingsFailure::unreadable,
           "a directory is not mistaken for missing settings");
    expect(!directory.write(default_editor_settings()), "unreadable settings cannot be replaced");
}

void verify_absolute_path()
{
    const auto absolute = absolute_file_path(L"nib-adapter-files/utf8.txt");
    expect(absolute.has_value(), "a relative argument becomes an absolute path");
    if (!absolute.has_value())
    {
        return;
    }
    expect(absolute.value().file_name() == "utf8.txt", "the file name survives");
    expect(absolute.value().text().find(':') != std::string_view::npos,
           "the absolute path carries a drive");
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--settings-codec")
    {
        verify_settings_codec();
        verify_bad_settings_fields();
        verify_bad_settings_values();
        std::printf("Settings codec: %zu checks, %zu failures\n", check_count(), failure_count());
        return failure_count() == 0 ? 0 : 1;
    }
    reset_folder();
    Win32FileAdapter files;
    Win32CodePageAdapter code_pages;
    verify_utf8_files(files);
    verify_shift_jis_file(files, code_pages);
    verify_replacement(files);
    verify_failures(files);
    verify_code_pages(code_pages);
    verify_timing_marks(files);
    verify_absolute_path();
    verify_settings_codec();
    verify_bad_settings_fields();
    verify_bad_settings_values();
    verify_settings_restart(files);
    verify_settings_lock(files);
    verify_settings_failures(files);
    if (failure_count() != 0)
    {
        std::fprintf(stderr, "Nib adapter tests: %zu of %zu checks failed\n", failure_count(),
                     check_count());
        return 1;
    }
    std::printf("Nib adapter tests passed: %zu checks over files, code pages and paths\n",
                check_count());
    return 0;
}
