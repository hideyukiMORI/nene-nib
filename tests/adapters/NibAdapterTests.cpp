// adapters/win32 のファイルと文字コードの往復を、ビルドディレクトリ配下の一時フォルダで測る
// （QLT-013 / ADR 0010）。中核の分岐カバレッジ（QLT-009）の対象ではない。
// 一時フォルダの名前は固定で、冒頭で消して作り直す。時刻も乱数も使わない（ARC-007）。
#include "AbsolutePath.hpp"
#include "CodePageFailure.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "TextEncoding.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32FileAdapter.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

namespace
{
using nenenib::adapters::win32::absolute_file_path;
using nenenib::adapters::win32::Win32CodePageAdapter;
using nenenib::adapters::win32::Win32FileAdapter;
using nenenib::application::CodePageFailure;
using nenenib::application::FileFailure;
using nenenib::core::byte_order_mark;
using nenenib::core::detect_encoding;
using nenenib::core::FilePath;
using nenenib::core::TextEncoding;

// application が渡す上限と同じ値（ADR 0010 の決定 12）。正本は EditorController にある。
constexpr std::size_t read_limit = 64U * 1024U * 1024U;
constexpr wchar_t folder[] = L"nib-adapter-files";
constexpr std::array<const wchar_t *, 4> file_names{
    L"nib-adapter-files/utf8.txt", L"nib-adapter-files/bom.txt", L"nib-adapter-files/sjis.txt",
    L"nib-adapter-files/replaced.txt"};

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

int main()
{
    reset_folder();
    Win32FileAdapter files;
    Win32CodePageAdapter code_pages;
    verify_utf8_files(files);
    verify_shift_jis_file(files, code_pages);
    verify_replacement(files);
    verify_failures(files);
    verify_code_pages(code_pages);
    verify_absolute_path();
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
