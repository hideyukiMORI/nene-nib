// adapters/win32
// のファイルと文字コードの往復を、temporary_test_directory（#1103）の中の一時フォルダで測る
// （QLT-013 / ADR 0010）。中核の分岐カバレッジ（QLT-009）の対象ではない。
// 一時フォルダの名前は固定で、冒頭で消して作り直す。時刻も乱数も使わない（ARC-007）。
#include "AbsolutePath.hpp"
#include "ClipboardCalls.hpp"
#include "ClipboardFailure.hpp"
#include "CodePageFailure.hpp"
#include "CompleteFileRead.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DocumentByteLimit.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "FileHandle.hpp"
#include "FilePath.hpp"
#include "FindHandle.hpp"
#include "HtmlClipboardFormat.hpp"
#include "MapRow.hpp"
#include "Milestone.hpp"
#include "SettingsCodec.hpp"
#include "TemporaryFiles.hpp"
#include "TemporaryTestDirectory.hpp"
#include "TextEncoding.hpp"
#include "VimModeSet.hpp"
#include "Win32ClipboardAdapter.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32FontCollationAdapter.hpp"
#include "Win32SettingsAdapter.hpp"
#include "Win32TimingAdapter.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using nenenib::adapters::win32::absolute_file_path;
using nenenib::adapters::win32::ClipboardCalls;
using nenenib::adapters::win32::collated_font_names;
using nenenib::adapters::win32::decode_settings;
using nenenib::adapters::win32::encode_settings;
using nenenib::adapters::win32::FileHandle;
using nenenib::adapters::win32::FindHandle;
using nenenib::adapters::win32::issued_temporary_sequence;
using nenenib::adapters::win32::reclaim_temporaries_beside;
using nenenib::adapters::win32::reclaimable_temporary;
using nenenib::adapters::win32::SettingsPaths;
using nenenib::adapters::win32::SettingsVersion;
using nenenib::adapters::win32::system_clipboard_calls;
using nenenib::adapters::win32::Win32ClipboardAdapter;
using nenenib::adapters::win32::Win32CodePageAdapter;
using nenenib::adapters::win32::Win32FileAdapter;
using nenenib::adapters::win32::Win32SettingsAdapter;
using nenenib::adapters::win32::Win32TimingAdapter;
using nenenib::application::ClipboardFailure;
using nenenib::application::CodePageFailure;
using nenenib::application::FileFailure;
using nenenib::application::FileWriteMode;
using nenenib::application::SettingsFailure;
using nenenib::core::byte_order_mark;
using nenenib::core::default_editor_settings;
using nenenib::core::detect_encoding;
using nenenib::core::FilePath;
using nenenib::core::Milestone;
using nenenib::core::TextEncoding;

// application が渡す上限と同じ値（ADR 0159 の決定 7）。正本は core の 1 か所。
constexpr std::size_t read_limit = nenenib::core::maximum_document_bytes;
constexpr wchar_t folder[] = L"nib-adapter-files";
constexpr wchar_t recorded_marks[] = L"nib-adapter-files/timing.json";
constexpr wchar_t unrecorded_marks[] = L"nib-adapter-files/silent.json";
constexpr std::array<const wchar_t *, 14> file_names{
    L"nib-adapter-files/utf8.txt",
    L"nib-adapter-files/bom.txt",
    L"nib-adapter-files/sjis.txt",
    L"nib-adapter-files/replaced.txt",
    recorded_marks,
    unrecorded_marks,
    L"nib-adapter-files/settings/settings.v1",
    L"nib-adapter-files/settings/settings.v1.lock",
    L"nib-adapter-files/settings/settings.v2",
    L"nib-adapter-files/settings/settings.v2.lock",
    L"nib-adapter-files/settings/settings.v3",
    L"nib-adapter-files/settings/settings.v3.lock",
    L"nib-adapter-files/settings/settings.v4",
    L"nib-adapter-files/settings/settings.v4.lock"};

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

int test_result()
{
    return failure_count() == 0 ? 0 : 1;
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

// 封印済みの列（ADR 0141）を比べやすい所有の文字列に写す。試験の都合の複写。
std::expected<std::string, FileFailure> read_text(Win32FileAdapter &files, const FilePath &path,
                                                  std::size_t maximum_bytes)
{
    const auto bytes = files.read(path, maximum_bytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    return std::string(bytes.value().view());
}

std::wstring process_hex()
{
    return std::format(L"{:x}", GetCurrentProcessId());
}

std::wstring temporary_beside(std::wstring_view target, std::wstring_view tag)
{
    return std::wstring(target) + L".nib-tmp-" + std::wstring(tag);
}

bool file_exists(const std::wstring &name)
{
    return GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// 別の保存が書いている途中の一時ファイル。共有 0 で開いたまま持つ。
FileHandle held_temporary(const std::wstring &name)
{
    return FileHandle(CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
}

using HeldNames = std::vector<std::unique_ptr<FileHandle>>;

// このプロセスの次の連番の名前（ADR 0135 の決定 1）。連番はプロセスで 1 つなので、次の保存が
// 最初に試すのは払い出し済みの連番の 1 つ後。
std::wstring upcoming_name(std::wstring_view target, std::uint32_t ahead)
{
    return temporary_beside(target, process_hex() + L"-" +
                                        std::to_wstring(issued_temporary_sequence() + ahead));
}

// 次の保存が試す 8 つの名前を、共有 0 で開いたまま塞ぐ（ADR 0135 の決定 2）。手放すと自分の
// 古い連番の残骸になり、次の保存の直前に回収される。
HeldNames occupy_upcoming_names(std::wstring_view target)
{
    HeldNames held;
    for (std::uint32_t ahead = 1; ahead <= 8; ++ahead)
    {
        const std::wstring name = upcoming_name(target, ahead);
        held.push_back(std::make_unique<FileHandle>(CreateFileW(
            name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)));
        expect(held.back()->valid(), "a temporary name the save may try is held");
    }
    return held;
}

// 保存先の一時ファイル `<target>.nib-tmp-*` が 1 つも残っていない（ADR 0135）。
bool no_temporaries_of(std::wstring_view target)
{
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileW((std::wstring(target) + L".nib-tmp-*").c_str(), &data));
    return !search.valid();
}

void verify_round_trip(Win32FileAdapter &files, std::string_view name, std::string_view bytes,
                       TextEncoding expected)
{
    const FilePath path = path_of(name);
    expect(files.write(path, bytes).has_value(), "the bytes are written");
    const auto read = files.read(path, read_limit);
    expect(read.has_value() && read.value().view() == bytes, "the same bytes come back");
    const auto detected = detect_encoding(read.has_value() ? read.value().view() : "");
    expect(detected.has_value() && detected.value() == expected, "the encoding is detected");
}

void verify_utf8_files(Win32FileAdapter &files)
{
    const std::string utf8 = "一行目\r\n二行目\r\n三行目";
    verify_round_trip(files, "nib-adapter-files/utf8.txt", utf8, TextEncoding::utf8);
    verify_round_trip(files, "nib-adapter-files/bom.txt", std::string(byte_order_mark()) + utf8,
                      TextEncoding::utf8_bom);
}

void verify_read_boundaries(Win32FileAdapter &files)
{
    const FilePath path = path_of("nib-adapter-files/utf8.txt");
    expect(files.write(path, "").has_value(), "an empty file can be written");
    const auto empty = files.read(path, 0);
    expect(empty.has_value() && empty.value().size() == 0,
           "zero-byte reads succeed at a zero limit");
    expect(files.write(path, "x").has_value(), "a one-byte file can be written");
    expect(read_text(files, path, 0).error() == FileFailure::too_large,
           "nonempty files are rejected before a zero-length read");
    expect(read_text(files, path, 1).value() == "x", "a one-byte read accepts the exact limit");
    const std::string bytes("a\0b\n", 4);
    expect(files.write(path, bytes).has_value(), "binary NUL bytes can be written");
    expect(read_text(files, path, bytes.size()).value() == bytes,
           "the initialized read length includes NUL and a trailing newline");
    expect(read_text(files, path, bytes.size() - 1).error() == FileFailure::too_large,
           "one byte above the limit is rejected");
    expect(files.write(path, "z").has_value() && read_text(files, path, 32).value() == "z",
           "a small file does not retain the previous read's longer tail");
}

void expect_read_steps(std::span<const std::expected<std::size_t, FileFailure>> steps,
                       bool succeeds)
{
    const std::string_view input("a\0bcde", 6);
    std::size_t calls = 0;
    std::size_t received = 0;
    const auto result = nenenib::adapters::win32::complete_file_read(
        input.size(),
        [&](std::span<char> target) noexcept -> std::expected<std::size_t, FileFailure>
        {
            expect(target.size() == input.size() - received,
                   "each read requests only the declared bytes still missing");
            const auto index = calls;
            ++calls;
            if (index >= steps.size())
            {
                return std::unexpected(FileFailure::unreadable);
            }
            const auto step = steps[index];
            if (!step)
            {
                return step;
            }
            const auto copied = (std::min)(step.value(), target.size());
            std::copy_n(input.data() + received, copied, target.data());
            received += copied;
            return step;
        });
    expect(calls == steps.size(), "completion or failure stops at the expected read call");
    if (succeeds)
    {
        expect(result.has_value() && result.value().view() == input,
               "complete reads preserve every byte including NUL in the correct position");
    }
    if (!succeeds)
    {
        expect(!result.has_value() && result.error() == FileFailure::unreadable,
               "an incomplete read returns unreadable without exposing partial bytes");
    }
}

void verify_complete_read_steps()
{
    using Step = std::expected<std::size_t, FileFailure>;
    const Step failure = std::unexpected(FileFailure::access_denied);
    expect_read_steps(std::array<Step, 1>{6}, true);
    expect_read_steps(std::array<Step, 2>{2, 4}, true);
    expect_read_steps(std::array<Step, 3>{1, 2, 3}, true);
    expect_read_steps(std::array<Step, 1>{0}, false);
    expect_read_steps(std::array<Step, 2>{2, 0}, false);
    expect_read_steps(std::array<Step, 1>{failure}, false);
    expect_read_steps(std::array<Step, 2>{2, failure}, false);
    expect_read_steps(std::array<Step, 1>{7}, false);
    expect_read_steps(std::array<Step, 2>{2, 5}, false);
    std::size_t calls = 0;
    const auto empty = nenenib::adapters::win32::complete_file_read(
        0,
        [&calls](std::span<char>) noexcept -> Step
        {
            ++calls;
            return std::unexpected(FileFailure::unreadable);
        });
    expect(empty.has_value() && empty.value().size() == 0 && calls == 0,
           "an empty declaration succeeds without invoking the read operation");
}

void verify_read_request_size()
{
    using nenenib::adapters::win32::file_read_request_size;
    const auto maximum = static_cast<std::size_t>((std::numeric_limits<DWORD>::max)());
    expect(file_read_request_size(0) == 0, "zero remaining bytes request nothing");
    expect(file_read_request_size(read_limit) == read_limit,
           "a complete 1 GiB read keeps its single request");
    expect(file_read_request_size(maximum - 1) == maximum - 1,
           "one byte below DWORD maximum is not split");
    expect(file_read_request_size(maximum) == maximum, "DWORD maximum fits one read request");
    expect(file_read_request_size(maximum + 1) == maximum,
           "one byte above DWORD maximum is capped without allocating that input");
    expect(file_read_request_size((std::numeric_limits<std::size_t>::max)()) == maximum,
           "the request cap does not overflow at size_t maximum");
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
    const auto read = read_text(files, path, read_limit);
    expect(read.has_value() && read.value() == "second", "the replacement is what comes back");
    // 一時ファイルは置き換えのあとに残らない。
    expect(no_temporaries_of(L"nib-adapter-files/replaced.txt"), "the temporary file is gone");
}

void verify_failures(Win32FileAdapter &files)
{
    const auto missing = read_text(files, path_of("nib-adapter-files/absent.txt"), read_limit);
    expect(!missing.has_value() && missing.error() == FileFailure::not_found,
           "an absent path is not_found");
    const auto directory = read_text(files, path_of("nib-adapter-files"), read_limit);
    expect(!directory.has_value() && directory.error() == FileFailure::unreadable,
           "a folder is unreadable");
    const auto refused = files.write(path_of("nib-adapter-files/absent/deep.txt"), "x");
    expect(!refused.has_value(), "writing into a folder that is not there fails");
    // 上限は引数で来る。小さく渡せば、読む前に断ることが見える。
    const auto capped = read_text(files, path_of("nib-adapter-files/utf8.txt"), 4);
    expect(!capped.has_value() && capped.error() == FileFailure::too_large,
           "a file larger than the limit the caller passed is too_large");
    // 保存に上限は無い。開ける大きさを超えて貼り付けた本文も書ける。
    const FilePath big = path_of("nib-adapter-files/replaced.txt");
    expect(files.write(big, std::string(1024U * 1024U, 'z')).has_value(),
           "a body larger than the read limit still writes");
    expect(read_text(files, big, read_limit).value_or(std::string{}).size() == 1024U * 1024U,
           "and it comes back whole");
}

void verify_create_only(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/replaced.txt");
    // 前の版の固定名は今の一時ファイルの名前の形に当たらないので、使いも消しもしない。
    const auto legacy = path_of("nib-adapter-files/replaced.txt.nib-tmp");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt") != 0, "remove our previous fixture");
    expect(files.write(path, "new content", FileWriteMode::create_new).has_value(),
           "create_new places a complete new file");
    const auto refused = files.write(path, "replacement", FileWriteMode::create_new);
    expect(!refused && refused.error() == FileFailure::already_exists,
           "create_new refuses a file that already exists");
    expect(read_text(files, path, read_limit).value_or(std::string{}) == "new content",
           "a rejected create preserves all existing bytes");
    // Issue #738: `:w! <file>` は既存の別のファイルを保存と同じ置き換えの道で上書きする。
    expect(files.write(path, "overwritten", FileWriteMode::replace).has_value() &&
               read_text(files, path, read_limit).value_or(std::string{}) == "overwritten",
           "replace overwrites an existing file that create_new refused");
    expect(SetFileAttributesW(L"nib-adapter-files/replaced.txt", FILE_ATTRIBUTE_READONLY) != 0,
           "mark our fixture read-only");
    expect(!files.write(path, "must not appear", FileWriteMode::replace),
           "replace does not force past a read-only target");
    expect(SetFileAttributesW(L"nib-adapter-files/replaced.txt", FILE_ATTRIBUTE_NORMAL) != 0,
           "clear the read-only mark on our fixture");
    expect(read_text(files, path, read_limit).value_or(std::string{}) == "overwritten",
           "a refused overwrite preserves all existing bytes");
    expect(no_temporaries_of(L"nib-adapter-files/replaced.txt"),
           "successful and rejected creation leave no temporary file");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt") != 0, "remove only our created file");
    expect(files.write(legacy, "someone else's temporary").has_value(),
           "prepare a file at the previous fixed temporary name");
    expect(files.write(path, "new content", FileWriteMode::create_new).has_value(),
           "the previous fixed temporary name does not block creation");
    expect(read_text(files, legacy, read_limit).value_or(std::string{}) ==
               "someone else's temporary",
           "an unowned file at the previous fixed name is neither overwritten nor deleted");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt.nib-tmp") != 0,
           "remove only the fixture we prepared at the previous fixed name");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt") != 0, "remove our created file again");
}

void verify_resolved_write(Win32FileAdapter &files)
{
    const auto input = path_of("nib-adapter-files/../nib-adapter-files/replaced.txt");
    const auto absolute = files.resolve(input);
    expect(absolute.has_value(), "FilePort resolves relative names through the OS boundary");
    if (!absolute)
    {
        return;
    }
    expect(absolute.value().file_name() == "replaced.txt" &&
               absolute.value().text().find("..") == std::string_view::npos,
           "resolved names preserve the filename and remove dot segments");
    expect(files.write(absolute.value(), "resolved", FileWriteMode::create_new).has_value(),
           "the resolved path can be created");
    expect(read_text(files, input, read_limit).value_or(std::string{}) == "resolved",
           "relative and resolved paths read the same bytes");
}

void verify_segmented_write(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/replaced.txt");
    constexpr std::array<std::string_view, 5> parts{"", "a\r", "\n日", "本\n",
                                                    std::string_view("z\0", 2)};
    expect(files.write(path, parts, FileWriteMode::replace).has_value(),
           "byte fragments are written as one file");
    expect(read_text(files, path, read_limit).value_or(std::string{}) ==
               std::string("a\r\n日本\nz\0", 12),
           "fragment boundaries do not change UTF-8, CRLF or NUL bytes");
    expect(files.write(path, parts, FileWriteMode::create_new).error() ==
               FileFailure::already_exists,
           "segmented create_new preserves an existing target");
    const std::array<std::string_view, 0> empty{};
    expect(files.write(path, empty, FileWriteMode::replace).has_value() &&
               read_text(files, path, 0).value_or("not empty").empty(),
           "an empty segment list replaces with an empty file");
    expect(files.write(path, "resolved").has_value(), "restore the replacement fixture");
}

void verify_failed_replacement(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/replaced.txt");
    {
        const FileHandle held(CreateFileW(L"nib-adapter-files/replaced.txt", GENERIC_READ,
                                          FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(held.valid(), "hold the target without sharing replacement");
        constexpr std::array<std::string_view, 2> parts{"must not ", "replace"};
        expect(!files.write(path, parts, FileWriteMode::replace),
               "a locked target refuses replacement after all fragments are written");
    }
    expect(read_text(files, path, read_limit).value_or(std::string{}) == "resolved",
           "failed replacement retains the complete original file");
    expect(no_temporaries_of(L"nib-adapter-files/replaced.txt"),
           "failed replacement removes the temporary file it owned");
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

void verify_code_page_bytes(Win32CodePageAdapter &code_pages)
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 5> cases{{
        {"ASCII\r\n", "ASCII\r\n"},
        {"\x93\xfa\x96\x7b\x8c\xea", "日本語"},
        {"\xb6\xc5", "ｶﾅ"},
        {std::string_view{"a\0b", 3}, std::string_view{"a\0b", 3}},
        {std::string_view{"A\xb6\x93\xfa\0Z", 6}, std::string_view{"Aｶ日\0Z", 9}},
    }};
    for (const auto &[cp932, utf8] : cases)
    {
        const auto decoded = code_pages.to_utf8(cp932);
        expect(decoded && decoded.value() == utf8, "explicit CP932 bytes decode exactly");
        const auto encoded = code_pages.from_utf8(utf8);
        expect(encoded && encoded.value() == cp932, "explicit UTF-8 bytes encode exactly");
    }
    const auto empty = code_pages.to_utf8("");
    expect(empty && empty.value().empty(), "empty CP932 decoding succeeds");
    for (const auto broken : {"\x81\x30", "\x82", "A\x82"})
    {
        const auto decoded = code_pages.to_utf8(broken);
        expect(!decoded && decoded.error() == CodePageFailure::undecodable,
               "invalid pairs and trailing lead bytes are rejected");
    }
}

void verify_code_page_independent_calls(Win32CodePageAdapter &code_pages)
{
    std::string cp932;
    std::string utf8;
    for (std::size_t repeat = 0; repeat < 4096; ++repeat)
    {
        cp932 += "\x93\xfa\x96\x7b\x8c\xea";
        utf8 += "日本語";
    }
    const auto first = code_pages.to_utf8(cp932);
    const auto short_text = code_pages.to_utf8("x");
    const auto empty = code_pages.to_utf8("");
    const auto failed = code_pages.to_utf8("\x82");
    const auto repeated = code_pages.to_utf8(cp932);
    expect(first && first.value() == utf8, "long CP932 input has all expected UTF-8 bytes");
    expect(short_text && short_text.value() == "x", "short decoding retains no previous tail");
    expect(empty && empty.value().empty(), "empty decoding retains no previous tail");
    expect(!failed && failed.error() == CodePageFailure::undecodable,
           "a rejected call does not become an empty success");
    expect(repeated && repeated.value() == utf8 && first && first.value() == utf8,
           "repeated long decoding preserves independently owned previous results");
}

void verify_code_page_scope(Win32CodePageAdapter &code_pages)
{
    verify_code_pages(code_pages);
    verify_code_page_bytes(code_pages);
    verify_code_page_independent_calls(code_pages);
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
    const auto bytes = read_text(files, recorded.value(), read_limit);
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
    const auto missing = read_text(files, unrecorded.value(), read_limit);
    expect(!missing.has_value() && missing.error() == FileFailure::not_found,
           "an adapter that was never bound writes nothing");
}

void verify_settings_codec()
{
    const auto defaults = default_editor_settings();
    const auto decoded = decode_settings(encode_settings(defaults), SettingsVersion::v4);
    expect(decoded.has_value() && same_settings(decoded.value(), defaults),
           "default settings round trip");
    const auto custom = decode_settings("\xEF\xBB\xBF\r\nversion = 1\r\n"
                                        " colorscheme = neutral-light \r\n"
                                        "\vfont_family = 日本語 Font=Test\r\nfont_size=21.25\f",
                                        SettingsVersion::v1);
    expect(custom.has_value(), "BOM, CRLF, whitespace and an equals sign in a font are accepted");
    if (!custom)
    {
        return;
    }
    expect(custom.value().font_family.text() == "日本語 Font=Test" &&
               custom.value().font_size.points() == 21.25F,
           "custom settings retain their values");
    const auto round_trip = decode_settings(encode_settings(custom.value()), SettingsVersion::v4);
    expect(round_trip.has_value() && same_settings(round_trip.value(), custom.value()),
           "custom settings round trip including an explicit theme");
}

void verify_bad_settings_fields()
{
    const std::string valid = encode_settings(default_editor_settings());
    for (const auto suffix : {"version=1\n", "unknown=yes\n", "broken\n", "lsp.nosuch.command=x\n"})
    {
        const auto result = decode_settings(valid + suffix, SettingsVersion::v4);
        expect(!result && result.error() == SettingsFailure::malformed,
               "duplicate keys, unknown keys and lines without equals are malformed");
    }
    const auto missing = decode_settings("version=1\n", SettingsVersion::v1);
    expect(!missing && missing.error() == SettingsFailure::malformed, "missing fields fail");
    const auto version =
        decode_settings("version=5\n" + valid.substr(valid.find('\n') + 1), SettingsVersion::v4);
    expect(!version && version.error() == SettingsFailure::unsupported_version,
           "an unknown version is incompatible");
    const auto theme = decode_settings("version=1\ncolorscheme=missing\nfont_family=Consolas\n"
                                       "font_size=13.5\n",
                                       SettingsVersion::v1);
    expect(!theme &&
               std::get<nenenib::core::ThemeLookupFailure>(theme.error()).name.text() == "missing",
           "unknown theme keeps its name");
}

void verify_bad_settings_values()
{
    for (const auto value : {"", "7.99", "40.01", "nan", "inf", "13,5", "13.5junk", "1e100"})
    {
        const auto result = decode_settings(
            std::string("version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=") + value,
            SettingsVersion::v1);
        expect(!result && result.error() == SettingsFailure::invalid_font_size,
               "invalid sizes and non-finite values are rejected");
    }
    for (const auto &value : {std::string{}, std::string("\xFF"), std::string("A\x01"),
                              std::string("A\0B", 3), std::string(257, 'a')})
    {
        const auto result = decode_settings(
            std::string("version=1\ncolorscheme=system\nfont_size=13.5\nfont_family=") + value,
            SettingsVersion::v1);
        expect(!result && result.error() == SettingsFailure::invalid_font_family,
               "invalid font names use the domain validator");
    }
}

constexpr std::string_view v3_settings =
    "version=3\ncolorscheme=system\nfont_family=Consolas\nfont_size=13.5\nguide=off\nwrap=off\n";

void verify_settings_versions()
{
    auto settings = default_editor_settings();
    settings.guide = nenenib::core::GuideVisibility::hidden;
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    const auto bytes = encode_settings(settings);
    const auto decoded = decode_settings(bytes, SettingsVersion::v4);
    expect(decoded && same_settings(decoded.value(), settings),
           "guide and wrapping round trip through v4");
    expect(bytes.starts_with("version=4\n") &&
               bytes.ends_with("guide=off\nwrap=off\nui_language=system\nlsp=off\nexplorer=off\n"
                               "root=\nfeatures=\nignorecase=off\nsmartcase=off\n"
                               "relativenumber=off\nscrolloff=0\n"),
           "v4 writer emits the canonical fifteen-field schema");
    const auto v3 = decode_settings(v3_settings, SettingsVersion::v3);
    expect(v3 && v3.value().guide == nenenib::core::GuideVisibility::hidden &&
               v3.value().wrapping == nenenib::core::LineWrapping::no_wrap &&
               v3.value().ui_language == nenenib::core::DisplayLanguageChoice::japanese &&
               !v3.value().lsp && !v3.value().explorer && !v3.value().root.has_value() &&
               v3.value().features.empty() && !v3.value().ignorecase && !v3.value().smartcase &&
               !v3.value().relativenumber && v3.value().scrolloff == 0,
           "v3 keeps its values, stays Japanese and takes the other v4 defaults");
    for (const auto extra :
         {"ui_language=system\n", "lsp=off\n", "explorer=off\n", "root=\n", "features=\n",
          "ignorecase=off\n", "smartcase=off\n", "relativenumber=off\n", "scrolloff=0\n"})
    {
        const auto refused = decode_settings(std::string(v3_settings) + extra, SettingsVersion::v3);
        expect(!refused && refused.error() == SettingsFailure::malformed,
               "an older version rejects every v4 key as unknown");
    }
    expect(!decode_settings(bytes + "wrap=on\n", SettingsVersion::v4), "duplicate wrap refused");
    expect(!decode_settings(bytes + "guide=on\n", SettingsVersion::v4), "duplicate guide refused");
    const auto wrong_new = decode_settings(bytes, SettingsVersion::v1);
    const auto wrong_v3 = decode_settings(v3_settings, SettingsVersion::v4);
    expect(!wrong_new && wrong_new.error() == SettingsFailure::unsupported_version && !wrong_v3 &&
               wrong_v3.error() == SettingsFailure::unsupported_version,
           "expected filename version must match document version");
}

// v2 と v1 の読み（欄が足りない版の既定と、新しい版の欄の拒否）。
void verify_older_settings_versions()
{
    const auto common = std::string(v3_settings.substr(v3_settings.find('\n') + 1));
    const auto v2 = "version=2\n" + common.substr(0, common.rfind("wrap="));
    const auto old = decode_settings(v2, SettingsVersion::v2);
    expect(old && old.value().guide == nenenib::core::GuideVisibility::hidden &&
               old.value().wrapping == nenenib::core::LineWrapping::wrap &&
               old.value().ui_language == nenenib::core::DisplayLanguageChoice::japanese,
           "v2 preserves guide and initializes wrapping on");
    const auto extra = decode_settings(v2 + "wrap=off\n", SettingsVersion::v2);
    expect(!extra && extra.error() == SettingsFailure::malformed, "v2 rejects a wrap field");
    const auto no_wrap = std::string(v3_settings.substr(0, v3_settings.rfind("wrap=")));
    expect(!decode_settings(no_wrap, SettingsVersion::v3), "v3 requires wrap");
    for (const auto value : {"", "true", "ON", "shown", "on off"})
    {
        const auto bad = decode_settings(no_wrap + "wrap=" + value + "\n", SettingsVersion::v3);
        expect(!bad && bad.error() == SettingsFailure::malformed, "invalid wrap value refused");
    }
    const auto prefix = v2.substr(0, v2.rfind("guide="));
    expect(!decode_settings(prefix, SettingsVersion::v2), "v2 still requires guide");
    for (const auto value : {"", "true", "ON", "shown", "on off"})
    {
        expect(!decode_settings(prefix + "guide=" + value + "\n", SettingsVersion::v2),
               "invalid v2 guide value refused");
    }
    const auto v1 = "version=1\n" + common.substr(0, common.rfind("guide="));
    expect(!decode_settings(v1 + "guide=on\n", SettingsVersion::v1), "v1 rejects guide");
    expect(!decode_settings(v1 + "wrap=on\n", SettingsVersion::v1), "v1 rejects wrap");
    const auto wrong_old = decode_settings(v1, SettingsVersion::v4);
    expect(!wrong_old && wrong_old.error() == SettingsFailure::unsupported_version,
           "a v1 document is not read as v4");
}

// `lsp.<language>.command` の行（ADR 0170 の決定 3・#1044）。0
// 行以上・言語は表の名前だけ・同じ言語は 1 行・値は空でない。書くときは features
// と検索の大小の後ろに言語の名前の順。
void verify_settings_lsp_commands()
{
    auto settings = default_editor_settings();
    settings.lsp_commands = {{"python", "C:\\py\\pylsp.exe"},
                             {"cpp", "C:\\llvm\\clangd.exe --log=error"}};
    const auto bytes = encode_settings(settings);
    expect(bytes.ends_with("features=\nignorecase=off\nsmartcase=off\nrelativenumber=off\n"
                           "scrolloff=0\nlsp.cpp.command=C:\\llvm\\clangd.exe --log=error\n"
                           "lsp.python.command=C:\\py\\pylsp.exe\n"),
           "commands follow the fixed keys in language name order");
    const auto decoded = decode_settings(bytes, SettingsVersion::v4);
    expect(decoded && decoded.value().lsp_commands.size() == 2 &&
               decoded.value().lsp_commands.front().first == "cpp" &&
               encode_settings(decoded.value()) == bytes,
           "commands round trip in language name order");
    const auto plain = encode_settings(default_editor_settings());
    const auto none = decode_settings(plain, SettingsVersion::v4);
    expect(none && none.value().lsp_commands.empty(), "zero command lines are accepted");
    for (const auto line : {"lsp.nosuch.command=x\n", "lsp.cpp.command=\n", "lsp..command=x\n",
                            "lsp.CPP.command=x\n", "lsp.cpp.args=x\n"})
    {
        const auto result = decode_settings(plain + line, SettingsVersion::v4);
        expect(!result && result.error() == SettingsFailure::malformed,
               "unknown languages, empty values and other lsp keys are refused");
    }
    expect(!decode_settings(plain + "lsp.cpp.command=a\nlsp.cpp.command=b\n", SettingsVersion::v4),
           "a second line for the same language is refused");
    const auto old =
        decode_settings(std::string(v3_settings) + "lsp.cpp.command=x\n", SettingsVersion::v3);
    expect(!old && old.error() == SettingsFailure::malformed, "v3 rejects command lines");
    auto other = settings;
    other.lsp_commands.pop_back();
    expect(!same_settings(other, settings), "commands take part in the settings comparison");
}

// Vim の `map.<modes>.<lhs>=<rhs>` の行と `mapleader=`（ADR 0176 の決定 1・#1081）。0 行以上・
// 書くときは lsp の行の後ろに持っている順・読むときはファイルの順・壊れた行は全体の拒否。
void verify_settings_maps()
{
    using nenenib::core::MapRow;
    using nenenib::core::VimModeSet;
    auto settings = default_editor_settings();
    settings.mapleader = "<Space>";
    settings.maps = {
        MapRow{VimModeSet{false, true, false, false, false}, "jj", "<Esc>", false},
        MapRow{VimModeSet{true, false, true, false, false}, "<leader>w", ":w<CR>", true}};
    const auto bytes = encode_settings(settings);
    expect(bytes.ends_with("features=\nignorecase=off\nsmartcase=off\nrelativenumber=off\n"
                           "scrolloff=0\nmapleader=<Space>\n"
                           "map.i.jj=!<Esc>\nmap.nv.<leader>w=:w<CR>\n"),
           "the leader and the map rows follow the other lines in order");
    const auto decoded = decode_settings(bytes, SettingsVersion::v4);
    expect(decoded && same_settings(decoded.value(), settings) &&
               encode_settings(decoded.value()) == bytes,
           "map rows and the leader round trip");
    const auto plain = encode_settings(default_editor_settings());
    expect(plain.find("map") == std::string::npos, "no rows and the default leader write nothing");
    const auto none = decode_settings(plain, SettingsVersion::v4);
    expect(none && none.value().maps.empty() && none.value().mapleader == "\\",
           "zero rows and no leader line read as the defaults");
    const auto ordered =
        decode_settings(plain + "map.x.q=b\nmap.n.zz=a\nmap.v.q=c\n", SettingsVersion::v4);
    expect(ordered && ordered.value().maps.size() == 3 && ordered.value().maps.at(0).rhs == "b" &&
               ordered.value().maps.at(1).rhs == "a" && ordered.value().maps.at(2).rhs == "c",
           "rows keep the order of the file");
    for (const auto line : {"map.i.jj=\n", "map.i.abcd=x\n", "map.q.j=x\n", "map.i.<C-s>=x\n",
                            "map.i.j=<Nope>\n", "map.ii.j=x\n", "map.i=x\n", "mapleader=ab\n",
                            "mapleader=\n", "map.i.j=a\nmap.i.j=b\n", "map.vn.x=a\nmap.nv.x=b\n"})
    {
        const auto result = decode_settings(plain + line, SettingsVersion::v4);
        expect(!result && result.error() == SettingsFailure::malformed,
               "broken map rows and leaders refuse the whole settings");
    }
    const auto old =
        decode_settings(std::string(v3_settings) + "map.i.jj=<Esc>\n", SettingsVersion::v3);
    const auto old_leader =
        decode_settings(std::string(v3_settings) + "mapleader=,\n", SettingsVersion::v3);
    expect(!old && !old_leader, "v3 rejects map rows and the leader");
    const auto spelled = decode_settings(plain + "mapleader=,\nmap.vn.x=a\n", SettingsVersion::v4);
    expect(spelled && spelled.value().mapleader == "," &&
               encode_settings(spelled.value()) == plain + "mapleader=,\nmap.nv.x=a\n" &&
               decode_settings(encode_settings(spelled.value()), SettingsVersion::v4),
           "a row is written with its canonical key and a comma leader round trips");
    auto other = settings;
    other.maps.pop_back();
    auto leader = settings;
    leader.mapleader = ",";
    expect(!same_settings(other, settings) && !same_settings(leader, settings),
           "rows and the leader take part in the settings comparison");
}

// 接頭辞の族の行（`lsp.<language>.command` と `map.<modes>.<lhs>`）。
void verify_settings_lines()
{
    verify_settings_lsp_commands();
    verify_settings_maps();
}

// 設定 v4 の 1 行を value に替えた bytes（key の行が無ければ bytes のまま）。
std::string with_setting(const std::string &bytes, std::string_view key, std::string_view value)
{
    const auto line = bytes.find("\n" + std::string(key) + "=");
    if (line == std::string::npos)
    {
        return bytes;
    }
    const auto start = line + 1;
    const auto end = bytes.find('\n', start);
    return bytes.substr(0, start) + std::string(key) + "=" + std::string(value) + bytes.substr(end);
}

// 設定 v4 から key の行を除いた bytes。
std::string without_setting(const std::string &bytes, std::string_view key)
{
    const auto start = bytes.find("\n" + std::string(key) + "=") + 1;
    return bytes.substr(0, start) + bytes.substr(bytes.find('\n', start) + 1);
}

// v4 の鍵の集合（ADR 0170 の決定 3）。全欄の往復・必須・値の検証・features の空と 3
// 件・重複の拒否。
// v4 の最初の定義（#1055）の後に足した鍵（ADR 0170 決定 3 の追補・#1083）。無い v4 は既定 off で
// 読め、書き戻すと鍵が入る。有れば厳密に読み、重複は拒否する。
void verify_settings_v4_later_keys()
{
    const auto bytes = encode_settings(default_editor_settings());
    auto stripped = bytes;
    for (const auto key : {"ignorecase", "smartcase", "relativenumber", "scrolloff"})
    {
        stripped = without_setting(stripped, key);
    }
    const auto older = decode_settings(stripped, SettingsVersion::v4);
    expect(older && !older.value().ignorecase && !older.value().smartcase &&
               !older.value().relativenumber && older.value().scrolloff == 0 &&
               encode_settings(older.value()) == bytes,
           "a v4 without the later keys reads them as off and 0 and writes them back");
    expect(!decode_settings(bytes + "ignorecase=on\n", SettingsVersion::v4) &&
               !decode_settings(with_setting(bytes, "smartcase", "yes"), SettingsVersion::v4) &&
               !decode_settings(bytes + "scrolloff=0\n", SettingsVersion::v4),
           "a present later key is still read strictly");
}

void verify_settings_v4_fields()
{
    auto settings = default_editor_settings();
    settings.ui_language = nenenib::core::DisplayLanguageChoice::simplified_chinese;
    settings.lsp = true;
    settings.explorer = true;
    settings.root = nenenib::core::FilePath::parse("C:\\work\\my dir=x").value();
    settings.features = {"matchparen", "change_marks", "todo_marks2"};
    settings.ignorecase = true;
    settings.smartcase = true;
    settings.relativenumber = true;
    settings.scrolloff = 999;
    const auto bytes = encode_settings(settings);
    expect(bytes.find("\nui_language=zh-hans\nlsp=on\nexplorer=on\nroot=C:\\work\\my dir=x\n"
                      "features=matchparen,change_marks,todo_marks2\nignorecase=on\nsmartcase=on\n"
                      "relativenumber=on\nscrolloff=999\n") != std::string::npos,
           "v4 writes the language, the switches, the root, the features and the Vim options");
    const auto decoded = decode_settings(bytes, SettingsVersion::v4);
    expect(decoded && same_settings(decoded.value(), settings) &&
               decoded.value().features == settings.features,
           "every v4 field round trips with three features");
    for (const auto language : {"system", "ja", "en", "zh-hans", "ko", "es", "de"})
    {
        const auto line = with_setting(bytes, "ui_language", language);
        const auto read = decode_settings(line, SettingsVersion::v4);
        expect(read && encode_settings(read.value()) == line,
               "each of the seven language values round trips");
    }
    using Row = std::pair<std::string_view, std::string_view>;
    for (const auto &[key, value] :
         {Row{"ui_language", "ja-JP"}, Row{"ui_language", "JA"}, Row{"ui_language", ""},
          Row{"lsp", "ON"}, Row{"explorer", "yes"}, Row{"ignorecase", "1"}, Row{"smartcase", ""},
          Row{"relativenumber", "1"}, Row{"scrolloff", "1000"}, Row{"scrolloff", "07"},
          Row{"scrolloff", ""}, Row{"scrolloff", "-1"}, Row{"features", "a,,b"},
          Row{"features", ",a"}, Row{"features", "a,"}, Row{"features", "Wrap"},
          Row{"features", "a b"}, Row{"features", "a,a"}, Row{"root", "C:\\a\x01"}})
    {
        const auto result = decode_settings(with_setting(bytes, key, value), SettingsVersion::v4);
        expect(!result && result.error() == SettingsFailure::malformed,
               "invalid v4 values are malformed");
    }
    const auto empty = decode_settings(with_setting(bytes, "features", ""), SettingsVersion::v4);
    expect(empty && empty.value().features.empty(), "an empty feature list is accepted");
    const auto unrooted = decode_settings(with_setting(bytes, "root", ""), SettingsVersion::v4);
    expect(unrooted && !unrooted.value().root.has_value(), "an empty root means no root");
    for (const auto key : {"ui_language", "lsp", "explorer", "root", "features", "wrap", "guide"})
    {
        const auto result = decode_settings(without_setting(bytes, key), SettingsVersion::v4);
        expect(!result && result.error() == SettingsFailure::malformed, "every v4 key is required");
    }
    expect(!decode_settings(bytes + "features=\n", SettingsVersion::v4) &&
               !decode_settings(bytes + "root=\n", SettingsVersion::v4),
           "duplicate v4 keys are refused");
    verify_settings_v4_later_keys();
}

void verify_settings_restart(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v4");
    Win32SettingsAdapter first(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v3"),
                             path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
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
    Win32SettingsAdapter restarted(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v3"),
                             path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto restored = restarted.read();
    expect(restored.has_value(), "a fresh adapter reads the saved file");
    const auto loaded = restored.value_or(std::nullopt);
    expect(loaded.has_value() && same_settings(loaded.value(), settings),
           "restart restores saved settings");
    expect(restarted.write(default_editor_settings()).has_value(), "another window can save");
    const auto stale = first.write(settings);
    expect(!stale && stale.error() == SettingsFailure::changed_externally,
           "stale settings never overwrite another window's update");
    expect(read_text(files, path, 4096).value_or("") == encode_settings(default_editor_settings()),
           "the competing update remains intact");
}

void verify_settings_lock(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v4");
    Win32SettingsAdapter settings(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v3"),
                             path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    expect(settings.read().has_value(), "the settings adapter loads before lock checks");
    const auto before = read_text(files, path, 4096);
    {
        const FileHandle lock(CreateFileW(L"nib-adapter-files/settings/settings.v4.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(lock.valid(), "the competing lock is acquired");
        const auto result = settings.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "a held lock refuses a write without waiting");
        expect(read_text(files, path, 4096) == before,
               "lock failure leaves the original bytes intact");
    }
    expect(settings.write(default_editor_settings()).has_value(), "a released lock allows retry");
    expect(SetFileAttributesW(L"nib-adapter-files/settings/settings.v4", FILE_ATTRIBUTE_READONLY) !=
               0,
           "the settings file becomes read only");
    const auto refused = settings.write(default_editor_settings());
    expect(!refused && refused.error() == SettingsFailure::unwritable,
           "read-only files refuse save");
    expect(read_text(files, path, 4096) == before, "failed replacement preserves settings bytes");
    expect(SetFileAttributesW(L"nib-adapter-files/settings/settings.v4", FILE_ATTRIBUTE_NORMAL) !=
               0,
           "the test restores the original attributes");
}

void verify_rejected_settings(Win32FileAdapter &files, std::string_view bytes,
                              SettingsFailure expected)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v4");
    expect(files.write(path, bytes).has_value(), "the invalid settings fixture is written");
    Win32SettingsAdapter settings(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v3"),
                             path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto loaded = settings.read();
    expect(!loaded && loaded.error() == expected, "invalid settings return their typed failure");
    const auto written = settings.write(default_editor_settings());
    expect(!written && written.error() == expected, "a failed load blocks later writes");
    expect(read_text(files, path, read_limit).value_or("") == bytes,
           "invalid settings remain untouched");
}

void verify_settings_failures(Win32FileAdapter &files)
{
    verify_rejected_settings(files, "invalid", SettingsFailure::malformed);
    verify_rejected_settings(files,
                             "version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=13.5",
                             SettingsFailure::unsupported_version);
    verify_rejected_settings(files, std::string(16385, 'x'), SettingsFailure::too_large);
    Win32SettingsAdapter unavailable(files, std::unexpected(SettingsFailure::location_unavailable));
    const auto failed = unavailable.read();
    expect(!failed && failed.error() == SettingsFailure::location_unavailable,
           "an unavailable location is distinct from missing settings");
    expect(!unavailable.write(default_editor_settings()), "an unavailable location cannot save");
    Win32SettingsAdapter directory(
        files, SettingsPaths{path_of("nib-adapter-files/settings"),
                             path_of("nib-adapter-files/settings/settings.v3"),
                             path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto unreadable = directory.read();
    expect(!unreadable && unreadable.error() == SettingsFailure::unreadable,
           "a directory is not mistaken for missing settings");
    expect(!directory.write(default_editor_settings()), "unreadable settings cannot be replaced");
}

constexpr std::string_view previous_bytes =
    "version=1\ncolorscheme=neutral-light\nfont_family=Consolas\nfont_size=21.25\n";

SettingsPaths settings_paths()
{
    return {path_of("nib-adapter-files/settings/settings.v4"),
            path_of("nib-adapter-files/settings/settings.v3"),
            path_of("nib-adapter-files/settings/settings.v2"),
            path_of("nib-adapter-files/settings/settings.v1")};
}

void reset_settings(Win32FileAdapter &files, std::optional<std::string_view> previous)
{
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v4"));
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v3"));
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v2"));
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v1"));
    if (previous.has_value())
    {
        expect(files.write(settings_paths().legacy, previous.value()).has_value(),
               "prepare previous settings bytes");
    }
}

void verify_migration_success(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    expect(reading && reading.value().has_value(), "v1 is read only when v2 is missing");
    auto loaded = reading.value_or(std::nullopt).value_or(default_editor_settings());
    expect(loaded.font_size.points() == 21.25F &&
               loaded.guide == nenenib::core::GuideVisibility::shown,
           "v1 retains settings and initializes shown guide");
    expect(!read_text(files, settings_paths().current, 4096),
           "loading v1 does not migrate eagerly");
    loaded.guide = nenenib::core::GuideVisibility::hidden;
    expect(store.write(loaded).has_value(), "first changed settings migrate into v4");
    expect(read_text(files, settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "migration leaves v1 bytes unchanged");
    expect(read_text(files, settings_paths().current, 4096).value_or("") == encode_settings(loaded),
           "migration writes v4 including hidden guide");
    // Both reading and locking v1 now fail: current saves must not touch either.
    const FileHandle old_file(CreateFileW(L"nib-adapter-files/settings/settings.v1", GENERIC_WRITE,
                                          0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                          nullptr));
    const FileHandle old_lock(CreateFileW(L"nib-adapter-files/settings/settings.v1.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
    expect(old_file.valid() && old_lock.valid(), "hold both previous file and previous lock");
    expect(store.write(default_editor_settings()).has_value(),
           "successful migration stops reading and locking previous settings");
    Win32SettingsAdapter restarted(files, settings_paths());
    expect(restarted.read().has_value() && restarted.write(loaded).has_value(),
           "existing v2 wins even when v1 cannot be read or locked");
}

void verify_migration_conflict(Win32FileAdapter &files, std::optional<std::string_view> before,
                               std::optional<std::string_view> after)
{
    reset_settings(files, before);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value(), "load migration snapshot before external v1 edit");
    if (after.has_value())
    {
        expect(files.write(settings_paths().legacy, after.value()).has_value(),
               "old editor creates or changes v1");
    }
    if (!after.has_value())
    {
        expect(DeleteFileW(L"nib-adapter-files/settings/settings.v1") != 0,
               "old settings are deleted externally");
    }
    auto changed = default_editor_settings();
    changed.guide = nenenib::core::GuideVisibility::hidden;
    const auto result = store.write(changed);
    expect(!result && result.error() == SettingsFailure::changed_externally,
           "v1 change creation and deletion each block migration");
    expect(!read_text(files, settings_paths().current, 4096), "refused migration creates no v4");
    const auto remaining = read_text(files, settings_paths().legacy, 4096);
    expect(after.has_value() ? remaining && remaining.value() == after.value() : !remaining,
           "refused migration preserves externally edited v1 bytes");
}

void verify_migration_locks(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value(), "load migration before lock checks");
    for (const auto name : {L"nib-adapter-files/settings/settings.v4.lock",
                            L"nib-adapter-files/settings/settings.v3.lock",
                            L"nib-adapter-files/settings/settings.v2.lock",
                            L"nib-adapter-files/settings/settings.v1.lock"})
    {
        const FileHandle held(CreateFileW(name, GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(held.valid(), "hold each migration lock independently");
        const auto result = store.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "either migration lock blocks saving without waiting");
        expect(!read_text(files, settings_paths().current, 4096) &&
                   read_text(files, settings_paths().legacy, 4096).value_or("") == previous_bytes,
               "lock refusal leaves current absent and previous bytes intact");
    }
    const FileHandle previous(CreateFileW(L"nib-adapter-files/settings/settings.v1", GENERIC_WRITE,
                                          0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                          nullptr));
    expect(previous.valid(), "hold v1 unreadable after snapshot");
    const auto result = store.write(default_editor_settings());
    expect(!result && result.error() == SettingsFailure::unreadable,
           "migration snapshot read failure blocks save");
    expect(!read_text(files, settings_paths().current, 4096), "read failure creates no v4");
}

void verify_previous_rejection(Win32FileAdapter &files, std::string_view invalid,
                               SettingsFailure failure)
{
    reset_settings(files, invalid);
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    const auto writing = store.write(default_editor_settings());
    expect(!reading && reading.error() == failure && !writing && writing.error() == failure,
           "invalid v1 load blocks all later writes");
    expect(!read_text(files, settings_paths().current, 4096) &&
               read_text(files, settings_paths().legacy, read_limit).value_or("") == invalid,
           "invalid previous bytes remain untouched and current remains absent");
}

void verify_migration_current_created(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value(), "load previous before another window creates v2");
    const auto external = encode_settings(default_editor_settings());
    expect(files.write(settings_paths().current, external).has_value(),
           "another window creates v2");
    const auto result = store.write(default_editor_settings());
    expect(!result && result.error() == SettingsFailure::changed_externally,
           "new current settings block stale migration");
    expect(read_text(files, settings_paths().current, 4096).value_or("") == external &&
               read_text(files, settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "refused migration preserves both external current and original previous");
}

void verify_migration_write_failure(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    // 次の保存が試す 8 つの名前を、共有 0 で開いたまま塞ぐ（ADR 0135）。前提: 設定の保存は
    // `files.write` を 1 回だけ呼び、lock は CreateFileW で直に取る（連番を進めない）。
    Win32FileAdapter fresh;
    Win32SettingsAdapter store(fresh, settings_paths());
    expect(store.read().has_value(), "load previous before failed migration write");
    {
        const HeldNames occupied = occupy_upcoming_names(L"nib-adapter-files/settings/settings.v4");
        const auto result = store.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "failed atomic migration write is reported");
        expect(!read_text(files, settings_paths().current, 4096) &&
                   read_text(files, settings_paths().legacy, 4096).value_or("") == previous_bytes,
               "failed migration write preserves current absence and previous bytes");
    }
    expect(store.write(default_editor_settings()).has_value(), "failed write keeps retry snapshot");
    expect(no_temporaries_of(L"nib-adapter-files/settings/settings.v4"),
           "the retry reclaims the temporary names that are no longer held");
}

constexpr std::string_view v2_bytes =
    "version=2\ncolorscheme=system\nfont_family=Consolas\nfont_size=18\nguide=off\n";

void verify_v2_migration(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    expect(files.write(settings_paths().previous, v2_bytes).has_value(),
           "prepare v2 migration source");
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    const auto loaded = reading.value_or(std::nullopt).value_or(default_editor_settings());
    expect(reading && loaded.font_size.points() == 18 &&
               loaded.guide == nenenib::core::GuideVisibility::hidden &&
               loaded.wrapping == nenenib::core::LineWrapping::wrap,
           "v2 takes precedence over v1 and initializes wrapping");
    expect(!read_text(files, settings_paths().current, 4096),
           "reading v2 does not migrate eagerly");
    auto changed = loaded;
    changed.wrapping = nenenib::core::LineWrapping::no_wrap;
    expect(store.write(changed).has_value(), "v2 can migrate to v4 with wrapping disabled");
    expect(read_text(files, settings_paths().previous, 4096).value_or("") == v2_bytes &&
               read_text(files, settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "v4 migration preserves both older files byte for byte");
    const auto saved = decode_settings(
        read_text(files, settings_paths().current, 4096).value_or(""), SettingsVersion::v4);
    expect(saved && same_settings(saved.value(), changed), "migrated v4 holds the chosen wrapping");
    const FileHandle old_lock(CreateFileW(L"nib-adapter-files/settings/settings.v2.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
    expect(old_lock.valid() && store.write(loaded).has_value(),
           "after migration v2 lock is irrelevant");
}

void verify_v2_conflicts(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter old(files, settings_paths());
    expect(old.read().has_value(), "load v1 while v2, v3 and v4 are absent");
    expect(files.write(settings_paths().previous, v2_bytes).has_value(),
           "another version creates v2");
    const auto raced = old.write(default_editor_settings());
    expect(!raced && raced.error() == SettingsFailure::changed_externally &&
               !read_text(files, settings_paths().current, 4096),
           "an intervening v2 creation blocks migration from v1");
    Win32SettingsAdapter active(files, settings_paths());
    expect(active.read().has_value(), "load the existing v2 snapshot");
    expect(files.write(settings_paths().previous, std::string(v2_bytes) + "\n").has_value(),
           "externally change v2 bytes");
    const auto changed = active.write(default_editor_settings());
    expect(!changed && changed.error() == SettingsFailure::changed_externally,
           "v2 changes block stale migration");
    Win32SettingsAdapter removed(files, settings_paths());
    expect(removed.read().has_value(), "load updated v2 snapshot");
    expect(DeleteFileW(L"nib-adapter-files/settings/settings.v2") != FALSE, "remove v2 externally");
    const auto erased = removed.write(default_editor_settings());
    expect(!erased && erased.error() == SettingsFailure::changed_externally,
           "v2 deletion blocks stale migration");
}

void verify_v2_rejection(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    expect(files.write(settings_paths().previous, "invalid").has_value(), "prepare malformed v2");
    Win32SettingsAdapter broken(files, settings_paths());
    const auto reading = broken.read();
    expect(!reading && reading.error() == SettingsFailure::malformed,
           "malformed v2 never falls back to valid v1");
    expect(!broken.write(default_editor_settings()) &&
               read_text(files, settings_paths().previous, 4096).value_or("") == "invalid",
           "malformed v2 blocks writes and preserves the source");
    expect(files.write(settings_paths().previous, v2_bytes).has_value(), "restore valid v2");
    Win32SettingsAdapter current(files, settings_paths());
    expect(current.read().has_value(), "load v2 before a lower-version change");
    expect(files.write(settings_paths().legacy, "invalid").has_value(), "change lower v1 version");
    expect(current.write(default_editor_settings()).has_value(),
           "a v1 change does not invalidate the higher v2 snapshot");
}

// v3 → v4 の移行（ADR 0114 の形・ADR 0170 の決定 3）。v4 が無いときだけ v3 を読み、最初の保存で
// v4 だけを書き、古い版は元の bytes のまま残す。
void verify_v3_migration(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    expect(files.write(settings_paths().previous, v2_bytes).has_value() &&
               files.write(settings_paths().prior, v3_settings).has_value(),
           "prepare v3 above v2 and v1");
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    const auto loaded = reading.value_or(std::nullopt).value_or(default_editor_settings());
    expect(reading && reading.value().has_value() && loaded.font_size.points() == 13.5F &&
               loaded.wrapping == nenenib::core::LineWrapping::no_wrap &&
               loaded.ui_language == nenenib::core::DisplayLanguageChoice::japanese &&
               !loaded.explorer && !loaded.root.has_value(),
           "v3 takes precedence over v2 and v1 and fills the v4 defaults");
    expect(!read_text(files, settings_paths().current, 4096),
           "reading v3 does not migrate eagerly");
    auto changed = loaded;
    changed.explorer = true;
    changed.root = nenenib::core::FilePath::parse("C:\\work").value();
    changed.features = {"matchparen"};
    expect(store.write(changed).has_value(), "the first change migrates v3 to v4");
    expect(read_text(files, settings_paths().prior, 4096).value_or("") == v3_settings &&
               read_text(files, settings_paths().previous, 4096).value_or("") == v2_bytes &&
               read_text(files, settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "v4 migration preserves v3, v2 and v1 byte for byte");
    const auto saved = decode_settings(
        read_text(files, settings_paths().current, 4096).value_or(""), SettingsVersion::v4);
    expect(saved && same_settings(saved.value(), changed),
           "migrated v4 holds the explorer, the root and the features");
    const FileHandle old_lock(CreateFileW(L"nib-adapter-files/settings/settings.v3.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
    expect(old_lock.valid() && store.write(loaded).has_value(),
           "after migration the v3 lock is irrelevant");
}

void verify_v3_conflicts(Win32FileAdapter &files)
{
    reset_settings(files, std::nullopt);
    expect(files.write(settings_paths().prior, v3_settings).has_value(), "prepare v3");
    Win32SettingsAdapter active(files, settings_paths());
    expect(active.read().has_value(), "load the v3 snapshot");
    expect(files.write(settings_paths().prior, std::string(v3_settings) + "\n").has_value(),
           "externally change v3 bytes");
    const auto changed = active.write(default_editor_settings());
    expect(!changed && changed.error() == SettingsFailure::changed_externally &&
               !read_text(files, settings_paths().current, 4096),
           "a v3 change blocks the stale migration");
    Win32SettingsAdapter raced(files, settings_paths());
    expect(raced.read().has_value(), "load the changed v3 snapshot");
    const auto external = encode_settings(default_editor_settings());
    expect(files.write(settings_paths().current, external).has_value(), "another window makes v4");
    const auto stale = raced.write(default_editor_settings());
    expect(!stale && stale.error() == SettingsFailure::changed_externally &&
               read_text(files, settings_paths().current, 4096).value_or("") == external,
           "a new v4 blocks the migration from v3 and stays intact");
    expect(files.write(settings_paths().current, "invalid").has_value(), "break v4");
    Win32SettingsAdapter broken(files, settings_paths());
    const auto reading = broken.read();
    expect(!reading && reading.error() == SettingsFailure::malformed &&
               !broken.write(default_editor_settings()),
           "a malformed v4 never falls back to a valid v3 and blocks writes");
    expect(read_text(files, settings_paths().current, 4096).value_or("") == "invalid" &&
               read_text(files, settings_paths().prior, 4096).value_or("") ==
                   std::string(v3_settings) + "\n",
           "a refused v4 load leaves both files untouched");
}

// 長いルートの経路で 16 KiB を超える設定は書かない（次の読込が too_large で止まるため）。
void verify_oversized_settings(Win32FileAdapter &files)
{
    reset_settings(files, std::nullopt);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value() && store.write(default_editor_settings()).has_value(),
           "save the defaults before the oversized write");
    const auto before = read_text(files, settings_paths().current, 16384).value_or("");
    auto huge = default_editor_settings();
    huge.root = nenenib::core::FilePath::parse("C:\\" + std::string(16384, 'a')).value();
    const auto refused = store.write(huge);
    expect(!refused && refused.error() == SettingsFailure::too_large &&
               read_text(files, settings_paths().current, 16384).value_or("") == before,
           "settings over 16 KiB are refused and the saved bytes stay");
    expect(store.write(default_editor_settings()).has_value(),
           "a refused oversized write keeps the snapshot for the next save");
}

void verify_settings_migration(Win32FileAdapter &files)
{
    verify_v3_migration(files);
    verify_v3_conflicts(files);
    verify_oversized_settings(files);
    verify_v2_migration(files);
    verify_v2_conflicts(files);
    verify_v2_rejection(files);
    verify_migration_success(files);
    verify_migration_conflict(
        files, previous_bytes,
        "version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=18\n");
    verify_migration_conflict(files, std::nullopt, previous_bytes);
    verify_migration_conflict(files, previous_bytes, std::nullopt);
    verify_migration_locks(files);
    verify_migration_current_created(files);
    verify_migration_write_failure(files);
    verify_previous_rejection(files, "invalid", SettingsFailure::malformed);
    verify_previous_rejection(files, encode_settings(default_editor_settings()),
                              SettingsFailure::unsupported_version);
    verify_previous_rejection(files, std::string(previous_bytes) + "guide=on\n",
                              SettingsFailure::malformed);
    verify_previous_rejection(files, std::string(16385, 'x'), SettingsFailure::too_large);
    reset_settings(files, previous_bytes);
    verify_rejected_settings(files, "invalid", SettingsFailure::malformed);
    // Valid v1 must not conceal an unreadable current path.
    const FileHandle current(CreateFileW(L"nib-adapter-files/settings/settings.v4", GENERIC_WRITE,
                                         0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                         nullptr));
    Win32SettingsAdapter blocked(files, settings_paths());
    const auto reading = blocked.read();
    expect(current.valid() && !reading && reading.error() == SettingsFailure::unreadable,
           "unreadable v4 never falls back to valid v1");
    expect(!blocked.write(default_editor_settings()), "unreadable v4 blocks later writes");
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
// 同じファイルの判定（ADR 0056 の決定 5）。大文字と小文字だけが違う絶対パスは同じ、違う
// ファイルは違う。短い名前（8.3）とリンクは解かない（ADR の「残る穴」）。
void verify_same_file(const Win32FileAdapter &files)
{
    const auto lower = FilePath::parse("C:\\work\\note.txt");
    const auto upper = FilePath::parse("C:\\WORK\\NOTE.TXT");
    const auto other = FilePath::parse("C:\\work\\note2.txt");
    expect(lower.has_value() && upper.has_value() && other.has_value(), "the paths parse");
    if (!lower.has_value() || !upper.has_value() || !other.has_value())
    {
        return;
    }
    expect(files.same_file(lower.value(), upper.value()),
           "paths that differ only in case are the same file");
    expect(files.same_file(lower.value(), lower.value()), "a path is the same file as itself");
    expect(!files.same_file(lower.value(), other.value()), "different paths are different files");
}
// 一時ファイルの名前と回収（ADR 0135 の決定 5）。連番はプロセスで 1 つなので、名前を予言する
// 試験は `issued_temporary_sequence` から次の連番を読む。
constexpr wchar_t temporaries_folder[] = L"nib-adapter-files/temporaries";
constexpr wchar_t kept_target[] = L"nib-adapter-files/temporaries/kept.txt";
constexpr wchar_t created_target[] = L"nib-adapter-files/temporaries/created.txt";
constexpr std::array<const wchar_t *, 9> temporaries_fixtures{
    kept_target,
    created_target,
    L"nib-adapter-files/temporaries/settings.v4",
    L"nib-adapter-files/temporaries/settings.v4.lock",
    L"nib-adapter-files/temporaries/settings.v3.lock",
    L"nib-adapter-files/temporaries/settings.v2.lock",
    L"nib-adapter-files/temporaries/settings.v1.lock",
    L"nib-adapter-files/temporaries/kept.txt.nib-tmp",
    L"nib-adapter-files/temporaries/notes.nib-tmp"};

// クラッシュや電源断で残った一時ファイル。誰も開いていない。
void leave_residue(const std::wstring &name)
{
    const FileHandle made(held_temporary(name));
    expect(made.valid(), "a residue temporary file is prepared");
}

// 試験が作った名前だけを消す。残ったものがあればフォルダを消せずに落ちる。
void remove_temporaries_folder(std::span<const std::wstring> created)
{
    for (const wchar_t *name : temporaries_fixtures)
    {
        static_cast<void>(DeleteFileW(name));
    }
    for (const std::wstring &name : created)
    {
        static_cast<void>(DeleteFileW(name.c_str()));
    }
    expect(RemoveDirectoryW(temporaries_folder) != 0,
           "the temporaries folder holds nothing the test did not name");
}

// 決定 5 (a)(c): 同じ保存先の一時ファイルが別の名前で同時に在り得る。ほかの保存が共有 0 で
// 開いたまま持つ一時ファイル（前の版の固定名・別のプロセス・プロセス ID の再利用）があっても、
// 自分の一時ファイルを CREATE_NEW で作って保存が通り、相手のものは消さない。
void verify_concurrent_temporaries(std::span<const std::wstring> others)
{
    Win32FileAdapter fresh;
    const auto target = path_of("nib-adapter-files/temporaries/kept.txt");
    {
        const FileHandle legacy(held_temporary(others[0]));
        const FileHandle other(held_temporary(others[1]));
        const FileHandle reused(held_temporary(others[2]));
        expect(legacy.valid() && other.valid() && reused.valid(),
               "other savers hold their temporary files without sharing");
        expect(fresh.write(target, "first").has_value(),
               "a save succeeds while other savers hold temporary files for the same target");
        expect(fresh.write(target, "second").has_value(),
               "a replacing save also succeeds beside live temporary files");
    }
    expect(read_text(fresh, target, read_limit).value_or(std::string{}) == "second",
           "the save beside live temporary files placed its own bytes");
    expect(file_exists(others[0]) && file_exists(others[1]) && file_exists(others[2]),
           "temporary files held open by other savers are not reclaimed");
}

// 決定 5 (b): 誰も開いていない残骸は、その保存先の保存の直前に消える。
void verify_reclaimed_residue(const std::wstring &residue)
{
    // pid は 4 の倍数なので 0xdeadbeef のプロセスは無い。OpenProcess で確かめてから使う。
    const HANDLE none = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0xdeadbeefU);
    expect(none == nullptr && GetLastError() == ERROR_INVALID_PARAMETER,
           "no process has the pid the residue names");
    Win32FileAdapter fresh;
    leave_residue(residue);
    expect(
        fresh.write(path_of("nib-adapter-files/temporaries/kept.txt"), "after residue").has_value(),
        "a save succeeds beside a residue temporary file");
    expect(!file_exists(residue), "a residue nobody holds is reclaimed before the save");
}

// 決定 5 (d): 設定の保存も同じ adapter を通るので、設定の置き場の残骸も保存の直前に消える。
void verify_reclaimed_settings_residue(const std::wstring &residue)
{
    Win32FileAdapter fresh;
    leave_residue(residue);
    Win32SettingsAdapter store(fresh,
                               SettingsPaths{path_of("nib-adapter-files/temporaries/settings.v4"),
                                             path_of("nib-adapter-files/temporaries/settings.v3"),
                                             path_of("nib-adapter-files/temporaries/settings.v2"),
                                             path_of("nib-adapter-files/temporaries/settings.v1")});
    expect(store.read().has_value(), "absent settings read before the save");
    expect(store.write(default_editor_settings()).has_value(), "the settings are saved");
    expect(!file_exists(residue), "a settings residue is reclaimed by the settings save");
}

// 決定 2: 試す 8 つの名前がすべて生きている一時ファイルなら、保存は unwritable で断り、
// 保存先も相手の一時ファイルも変えない。
void verify_exhausted_names()
{
    Win32FileAdapter fresh;
    const auto created = path_of("nib-adapter-files/temporaries/created.txt");
    {
        const HeldNames occupied = occupy_upcoming_names(created_target);
        const auto refused = fresh.write(created, "must not appear", FileWriteMode::create_new);
        expect(!refused && refused.error() == FileFailure::unwritable,
               "create_new refuses when every temporary name it may try is taken");
        expect(!file_exists(created_target), "the destination is not partially created");
    }
    expect(fresh.write(created, "created", FileWriteMode::create_new).has_value(),
           "create_new succeeds once the names are released");
    expect(no_temporaries_of(created_target), "released names of this process are reclaimed");
    const auto target = path_of("nib-adapter-files/temporaries/kept.txt");
    {
        const HeldNames occupied = occupy_upcoming_names(kept_target);
        const auto refused = fresh.write(target, "must not replace");
        expect(!refused && refused.error() == FileFailure::unwritable,
               "a save refuses when every temporary name it may try is taken");
        expect(read_text(fresh, target, read_limit).value_or(std::string{}) == "after residue",
               "the refused save keeps the complete previous file");
    }
    expect(fresh.write(target, "after release").has_value(),
           "the next save tries new names and reclaims the released ones");
    expect(no_temporaries_of(kept_target), "released temporary names are reclaimed");
}

// 決定 3 の (b): 起動時に設定の置き場の `*.nib-tmp-*` を回収する。開いたままのものと、
// 名前の形に当たらないもの（前の版の固定名）は残す。
void verify_reclaimed_beside(std::span<const std::wstring> residues)
{
    constexpr wchar_t legacy[] = L"nib-adapter-files/temporaries/notes.nib-tmp";
    leave_residue(residues[0]);
    leave_residue(residues[1]);
    leave_residue(legacy);
    {
        const FileHandle live(held_temporary(residues[2]));
        expect(live.valid(), "a live temporary file beside the settings is held");
        reclaim_temporaries_beside(path_of("nib-adapter-files/temporaries/settings.v4"));
        expect(!file_exists(residues[0]) && !file_exists(residues[1]),
               "start-up reclaims every residue beside the settings");
        expect(file_exists(residues[2]), "start-up keeps a temporary file someone holds");
    }
    expect(file_exists(legacy), "start-up leaves names outside the temporary pattern");
}

// 決定 3: 回収するのは名前の形が `<16 進の pid>-<10 進の連番>` で、pid のプロセスが生きて
// いないものと、自分の古い連番だけ。生きているプロセス（自分・System）の名前と、形の違う使う人
// のファイルは、誰も開いていなくても残す。
void verify_reclaim_rule(std::span<const std::wstring> kept)
{
    // System（pid 4）は常に生きている。開ければ STILL_ACTIVE、開けなければ拒否で、無いではない。
    const HANDLE system = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 4);
    expect(system != nullptr || GetLastError() == ERROR_ACCESS_DENIED,
           "the System process is there");
    if (system != nullptr)
    {
        CloseHandle(system);
    }
    Win32FileAdapter fresh;
    const std::wstring own_older = upcoming_name(kept_target, 0);
    for (const std::wstring &name : kept)
    {
        leave_residue(name);
    }
    leave_residue(own_older);
    expect(fresh.write(path_of("nib-adapter-files/temporaries/kept.txt"), "after rule").has_value(),
           "a save succeeds beside names it must not reclaim");
    expect(!file_exists(own_older), "an older sequence of this process is reclaimed");
    expect(file_exists(kept[0]), "a later sequence of this live process is not reclaimed");
    expect(file_exists(kept[1]), "a residue named by a live process (System) is not reclaimed");
    expect(file_exists(kept[2]), "a user's file outside the name form is not reclaimed");
    expect(file_exists(kept[3]), "a pid with an uppercase digit is outside the name form");
}

// 決定 3 の名前の読み（純関数）。self は 0x1a、払い出し済みの連番は 5。
void verify_reclaimable_names()
{
    constexpr auto alive = [](std::uint32_t) noexcept { return true; };
    constexpr auto gone = [](std::uint32_t) noexcept { return false; };
    constexpr std::uint32_t self = 0x1aU;
    constexpr std::uint32_t issued = 5;
    expect(reclaimable_temporary(L"deadbeef-1", self, issued, gone),
           "a residue of a process that is gone is reclaimable");
    expect(!reclaimable_temporary(L"deadbeef-1", self, issued, alive),
           "a temporary file of a live process is not reclaimable");
    expect(reclaimable_temporary(L"1a-5", self, issued, alive) &&
               reclaimable_temporary(L"1a-1", self, issued, alive),
           "an older sequence of this process is reclaimable without asking liveness");
    expect(!reclaimable_temporary(L"1a-6", self, issued, gone),
           "a sequence this process has not issued yet is not reclaimable");
    expect(reclaimable_temporary(L"ffffffff-4294967295", self, issued, gone),
           "the widest pid and sequence still read");
    constexpr std::array<std::wstring_view, 14> outside{L"backup",
                                                        L"",
                                                        L"-1",
                                                        L"1b-",
                                                        L"1b",
                                                        L"1B-1",
                                                        L"01b-1",
                                                        L"1b-01",
                                                        L"1b-1x",
                                                        L"1b-1-2",
                                                        L"1b- 1",
                                                        L"100000000-1",
                                                        L"1b-4294967296",
                                                        L"1b-1.nib-tmp-2-3"};
    for (const std::wstring_view suffix : outside)
    {
        expect(!reclaimable_temporary(suffix, self, issued, gone),
               "a name outside `<hex pid>-<decimal sequence>` is never reclaimable");
    }
}

void verify_temporary_files()
{
    verify_reclaimable_names();
    const BOOL created = CreateDirectoryW(temporaries_folder, nullptr);
    expect(created != 0 || GetLastError() == ERROR_ALREADY_EXISTS,
           "the temporaries folder is there");
    const std::array<std::wstring, 12> named{
        std::wstring(kept_target) + L".nib-tmp",
        temporary_beside(kept_target, L"deadbeef-1"),
        upcoming_name(kept_target, 1),
        temporary_beside(kept_target, L"deadbeef-2"),
        temporary_beside(L"nib-adapter-files/temporaries/settings.v4", L"deadbeef-3"),
        temporary_beside(L"nib-adapter-files/temporaries/history.v1", L"deadbeef-4"),
        temporary_beside(L"nib-adapter-files/temporaries/session.v2", process_hex() + L"-9"),
        temporary_beside(L"nib-adapter-files/temporaries/bookmarks.v1", L"deadbeef-5"),
        upcoming_name(kept_target, 1000),
        temporary_beside(kept_target, L"4-1"),
        temporary_beside(kept_target, L"backup"),
        temporary_beside(kept_target, L"DEADBEEF-1")};
    for (const std::wstring &name : named)
    {
        static_cast<void>(DeleteFileW(name.c_str()));
    }
    verify_concurrent_temporaries(std::span(named).first(3));
    verify_reclaimed_residue(named[3]);
    verify_reclaimed_settings_residue(named[4]);
    verify_exhausted_names();
    verify_reclaimed_beside(std::span(named).subspan(5, 3));
    verify_reclaim_rule(std::span(named).subspan(8));
    remove_temporaries_folder(named);
}

void verify_files(Win32FileAdapter &files, Win32CodePageAdapter &code_pages)
{
    verify_utf8_files(files);
    verify_shift_jis_file(files, code_pages);
    verify_replacement(files);
    verify_failures(files);
    verify_create_only(files);
    verify_resolved_write(files);
    verify_segmented_write(files);
    verify_failed_replacement(files);
    verify_code_page_scope(code_pages);
    verify_absolute_path();
    verify_same_file(files);
    verify_read_boundaries(files);
    verify_complete_read_steps();
    verify_read_request_size();
    verify_temporary_files();
}

// 替え玉の OS のクリップボード（ADR 0134 の決定 4）。実機のクリップボードは触らない
// （施主の了承が要る）。開く口は refused_opens() 回だけ断り、その後は開く。
constexpr std::size_t always_refused = (std::numeric_limits<std::size_t>::max)();

std::size_t &refused_opens()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &open_calls()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &empty_calls()
{
    static std::size_t count = 0;
    return count;
}

DWORD &waited_milliseconds()
{
    static DWORD total = 0;
    return total;
}

std::size_t &wait_calls()
{
    static std::size_t count = 0;
    return count;
}

bool &set_refused()
{
    static bool refused = false;
    return refused;
}

std::wstring &placed_text()
{
    static std::wstring text;
    return text;
}

HGLOBAL &held_global()
{
    static HGLOBAL handle = nullptr;
    return handle;
}

BOOL WINAPI fake_open(HWND)
{
    ++open_calls();
    return open_calls() > refused_opens() ? TRUE : FALSE;
}

BOOL WINAPI fake_close()
{
    return TRUE;
}

BOOL WINAPI fake_empty()
{
    ++empty_calls();
    placed_text().clear();
    return TRUE;
}

// 置けたら中身を写して解放する（本物なら所有権が OS へ移る）。
HANDLE WINAPI fake_set(UINT, HANDLE handle)
{
    if (set_refused())
    {
        return nullptr;
    }
    const auto *memory = static_cast<const wchar_t *>(GlobalLock(handle));
    placed_text() = memory == nullptr ? std::wstring{} : std::wstring(memory);
    GlobalUnlock(handle);
    GlobalFree(handle);
    return handle;
}

HANDLE WINAPI fake_get(UINT)
{
    return held_global();
}

BOOL WINAPI fake_available(UINT)
{
    return TRUE;
}

// "HTML Format" の番号の替え玉。OS に登録しない。
UINT WINAPI fake_register(LPCWSTR)
{
    constexpr UINT html_format = 0xC0DEU;
    return html_format;
}

void WINAPI fake_wait(DWORD milliseconds)
{
    ++wait_calls();
    waited_milliseconds() += milliseconds;
}

void reset_fake_clipboard(std::size_t refusals, bool refuse_set)
{
    refused_opens() = refusals;
    set_refused() = refuse_set;
    open_calls() = 0;
    empty_calls() = 0;
    wait_calls() = 0;
    waited_milliseconds() = 0;
    placed_text().clear();
}

[[nodiscard]] Win32ClipboardAdapter &faked_clipboard()
{
    static Win32ClipboardAdapter adapter(ClipboardCalls{
        .open = &fake_open,
        .close = &fake_close,
        .empty = &fake_empty,
        .set = &fake_set,
        .get = &fake_get,
        .available = &fake_available,
        .wait = &fake_wait,
        .register_format = &fake_register,
    });
    adapter.bind(GetDesktopWindow());
    return adapter;
}

// 読みの替え玉が返す本文。試験の終わりにこちらで解放する。
void hold_global_text(std::wstring_view text)
{
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    held_global() = GlobalAlloc(GMEM_MOVEABLE, bytes);
    auto *memory = static_cast<wchar_t *>(GlobalLock(held_global()));
    if (memory != nullptr)
    {
        std::memcpy(memory, std::wstring(text).c_str(), bytes);
    }
    GlobalUnlock(held_global());
}

void release_global_text()
{
    GlobalFree(held_global());
    held_global() = nullptr;
}

// ほかのプロセスが一瞬開いている：5 回の再試行の内なら書けて読める（決定 2）。
void verify_clipboard_open_retried()
{
    reset_fake_clipboard(5, false);
    const auto written = faked_clipboard().write("abc");
    expect(written.has_value(), "a clipboard that opens on the sixth try is written");
    expect(placed_text() == L"abc", "the retried write places the text");
    expect(open_calls() == 6 && wait_calls() == 5 && waited_milliseconds() == 50,
           "the write retries five times with 10 ms between tries");
    reset_fake_clipboard(5, false);
    hold_global_text(L"xyz");
    const auto read = faked_clipboard().read();
    release_global_text();
    expect(read.has_value() && read.value() == "xyz",
           "a clipboard that opens on the sixth try is read");
    expect(open_calls() == 6 && wait_calls() == 5, "the read retries through the same opening");
}

// ずっと開けない：50 ms で諦めて unavailable。前の中身は消さない（決定 2）。
void verify_clipboard_never_opens()
{
    reset_fake_clipboard(always_refused, false);
    const auto written = faked_clipboard().write("abc");
    expect(!written && written.error() == ClipboardFailure::unavailable,
           "a clipboard that never opens is unavailable for writing");
    expect(open_calls() == 6 && wait_calls() == 5 && waited_milliseconds() == 50,
           "the write gives up after five retries");
    expect(empty_calls() == 0, "a clipboard that never opens is not emptied");
    reset_fake_clipboard(always_refused, false);
    const auto read = faked_clipboard().read();
    expect(!read && read.error() == ClipboardFailure::unavailable,
           "a clipboard that never opens is unavailable for reading");
    expect(open_calls() == 6, "the read gives up after five retries");
}

// 空にした後で置けない：write_failed を返す（決定 3）。
void verify_clipboard_set_refused()
{
    reset_fake_clipboard(0, true);
    const auto written = faked_clipboard().write("abc");
    expect(!written && written.error() == ClipboardFailure::write_failed,
           "a refused SetClipboardData is a failed write");
    expect(open_calls() == 1 && wait_calls() == 0, "an opened clipboard is not retried");
}

// 変換できない本文：開かずに unrepresentable。空文字列を置いて成功と言わない（決定 1）。
void verify_clipboard_unrepresentable()
{
    constexpr std::array<std::string_view, 3> broken{"\xFF", "a\xC3", "\xED\xA0\x80"};
    for (const std::string_view text : broken)
    {
        reset_fake_clipboard(0, false);
        const auto written = faked_clipboard().write(text);
        expect(!written && written.error() == ClipboardFailure::unrepresentable,
               "text that cannot become UTF-16 is refused");
        expect(open_calls() == 0 && empty_calls() == 0,
               "unrepresentable text never opens or empties the clipboard");
    }
}

void verify_clipboard_unbound()
{
    Win32ClipboardAdapter unbound(system_clipboard_calls());
    const auto written = unbound.write("abc");
    expect(!written && written.error() == ClipboardFailure::unavailable,
           "an adapter without a window is unavailable");
    const auto html = unbound.html();
    expect(!html && html.error() == ClipboardFailure::unavailable,
           "an adapter without a window cannot read HTML");
}

// CF_HTML の形（見出しの位置は 10 桁のゼロ埋め・CRLF）。位置は UTF-8 のバイト位置で数える。
[[nodiscard]] std::string cf_html(std::string_view fragment)
{
    constexpr std::string_view before = "<html><body>\r\n<!--StartFragment-->";
    constexpr std::string_view after = "<!--EndFragment-->\r\n</body></html>";
    const std::size_t header =
        std::format("Version:0.9\r\nStartHTML:{:010}\r\nEndHTML:{:010}\r\nStartFragment:{:010}\r\n"
                    "EndFragment:{:010}\r\n",
                    0, 0, 0, 0)
            .size();
    const std::size_t start = header + before.size();
    const std::size_t end = start + fragment.size();
    return std::format("Version:0.9\r\nStartHTML:{:010}\r\nEndHTML:{:010}\r\nStartFragment:{:010}"
                       "\r\nEndFragment:{:010}\r\n{}{}{}",
                       header, end + after.size(), start, end, before, fragment, after);
}

// 見出しの切り出し（ADR 0157 の決定 4）。実機のクリップボードを触らない純関数の単体。
void verify_html_fragment_header()
{
    using nenenib::adapters::win32::html_fragment_of;
    expect(html_fragment_of(cf_html("<b>x</b>")) == std::optional<std::string_view>{"<b>x</b>"},
           "the fragment is cut between StartFragment and EndFragment");
    const std::string japanese = cf_html("<p>日本語</p>");
    expect(html_fragment_of(japanese) == std::optional<std::string_view>{"<p>日本語</p>"},
           "the positions count UTF-8 bytes, not characters");
    expect(html_fragment_of(cf_html("")) == std::optional<std::string_view>{""},
           "an empty fragment is empty");
    expect(!html_fragment_of("<html>no header</html>").has_value(),
           "bytes without the header have no fragment");
    expect(!html_fragment_of("Version:0.9\r\nStartFragment:0000000005\r\nEndFragment:00000000"
                             "99\r\n<b>x</b>")
                .has_value(),
           "an end past the bytes has no fragment");
    expect(
        !html_fragment_of("StartFragment:0000000009\r\nEndFragment:0000000005\r\n<b>").has_value(),
        "a start after the end has no fragment");
    expect(!html_fragment_of("StartFragment:x\r\nEndFragment:0000000005\r\n<b>").has_value(),
           "a position that is not a number has no fragment");
    expect(!html_fragment_of("Version:0.9\r\n<p>StartFragment:1 EndFragment:2</p>").has_value(),
           "the keys are read only before the first <");
}

void hold_global_bytes(std::string_view bytes)
{
    held_global() = GlobalAlloc(GMEM_MOVEABLE, bytes.size() + 1U);
    auto *memory = static_cast<char *>(GlobalLock(held_global()));
    if (memory != nullptr)
    {
        std::memcpy(memory, std::string(bytes).c_str(), bytes.size() + 1U);
    }
    GlobalUnlock(held_global());
}

// 替え玉の束で html() の道を通す（決定 4）。OS のクリップボードは開かない。
void verify_clipboard_html()
{
    reset_fake_clipboard(0, false);
    hold_global_bytes(cf_html("<i>y</i>"));
    const auto html = faked_clipboard().html();
    expect(html.has_value() && html.value() == std::optional<std::string>{"<i>y</i>"},
           "html() returns the fragment of the HTML Format");
    release_global_text();
    hold_global_bytes("no header");
    const auto broken = faked_clipboard().html();
    expect(broken.has_value() && !broken.value().has_value(),
           "an HTML Format without the header is no HTML");
    release_global_text();
    hold_global_bytes(cf_html("a\xFF"));
    const auto invalid = faked_clipboard().html();
    expect(!invalid && invalid.error() == ClipboardFailure::unsupported_format,
           "a fragment that is not UTF-8 is refused");
    release_global_text();
}

void verify_clipboard()
{
    verify_clipboard_open_retried();
    verify_clipboard_never_opens();
    verify_clipboard_set_refused();
    verify_clipboard_unrepresentable();
    verify_clipboard_unbound();
    verify_html_fragment_header();
    verify_clipboard_html();
}

// #696: 字体の名前は OS の照合順（大文字小文字と空白の扱いはロケールの照合）。ASCII の名前は
// どのロケールでも同じ並びになる例だけを見る。照合が同じ名前どうしはバイト順で決まる。
void verify_font_collation()
{
    using Names = std::vector<std::string>;
    expect(collated_font_names(Names{"MS Gothic", "Meiryo", "consolas", "Arial"}) ==
               Names{"Arial", "consolas", "Meiryo", "MS Gothic"},
           "font names follow the OS collation, not the byte order");
    const auto repeated = collated_font_names(Names{"Consolas", "Arial", "Consolas"});
    expect(repeated == Names{"Arial", "Consolas", "Consolas"},
           "equal names stay next to each other for the core to fold");
    expect(collated_font_names(Names{"b", "B", "a"}) == collated_font_names(Names{"B", "a", "b"}),
           "the order does not depend on the given order");
    expect(collated_font_names(Names{}).empty(), "no names stay empty");
}

bool known_scope(std::string_view scope)
{
    return scope == "--settings-codec" || scope == "--settings" || scope == "--files" ||
           scope == "--code-pages" || scope == "--clipboard" || scope == "--fonts";
}

int run_memory_tests(std::string_view scope)
{
    if (scope == "--fonts")
    {
        verify_font_collation();
        std::printf("Font collation adapter: %zu checks, %zu failures\n", check_count(),
                    failure_count());
        return test_result();
    }
    if (scope == "--code-pages")
    {
        Win32CodePageAdapter code_pages;
        verify_code_page_scope(code_pages);
        std::printf("Code page adapter: %zu checks, %zu failures\n", check_count(),
                    failure_count());
        return test_result();
    }
    verify_settings_codec();
    verify_bad_settings_fields();
    verify_bad_settings_values();
    verify_settings_versions();
    verify_older_settings_versions();
    verify_settings_v4_fields();
    verify_settings_lines();
    std::printf("Settings codec: %zu checks, %zu failures\n", check_count(), failure_count());
    return test_result();
}
int run_tests(int argc, char **argv)
{
    const std::string_view scope = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && !known_scope(scope)))
    {
        std::fprintf(stderr, "Unknown adapter test scope\n");
        return 1;
    }
    if (scope == "--code-pages" || scope == "--settings-codec" || scope == "--fonts")
    {
        return run_memory_tests(scope);
    }
    if (scope == "--clipboard")
    {
        verify_clipboard();
        std::printf("Clipboard adapter: %zu checks, %zu failures\n", check_count(),
                    failure_count());
        return test_result();
    }
    reset_folder();
    Win32FileAdapter files;
    if (scope == "--settings")
    {
        verify_settings_restart(files);
        verify_settings_lock(files);
        verify_settings_failures(files);
        verify_settings_migration(files);
        std::printf("Settings adapter: %zu checks, %zu failures\n", check_count(), failure_count());
        return test_result();
    }
    Win32CodePageAdapter code_pages;
    verify_files(files, code_pages);
    if (scope == "--files")
    {
        std::printf("File adapter tests: %zu checks, %zu failures\n", check_count(),
                    failure_count());
        return test_result();
    }
    verify_timing_marks(files);
    verify_settings_codec();
    verify_bad_settings_fields();
    verify_bad_settings_values();
    verify_settings_versions();
    verify_older_settings_versions();
    verify_settings_v4_fields();
    verify_settings_lines();
    verify_settings_restart(files);
    verify_settings_lock(files);
    verify_settings_failures(files);
    verify_settings_migration(files);
    verify_clipboard();
    verify_font_collation();
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
} // namespace

int main(int argc, char **argv)
{
    const auto directory = nenenib::tests::temporary_test_directory(L"adapter");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made\n");
        return 1;
    }
    const int result = run_tests(argc, argv);
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
