// Issue #278: 明示登録（bookmarks.v1）のコーデックと adapter
// を、ビルドディレクトリ配下の 一時フォルダで測る（QLT-013 / ADR 0063 の決定 4）。使う人の
// %LOCALAPPDATA% には触れない。
// 一時フォルダの名前は固定で、冒頭で消して作り直す。時刻も乱数も使わない（ARC-007）。
#include "BookmarkCodec.hpp"
#include "FileBookmarks.hpp"
#include "FileBookmarksFailure.hpp"
#include "FilePath.hpp"
#include "Win32BookmarkAdapter.hpp"
#include "Win32FileAdapter.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::FileBookmarks;
using Failure = nenenib::application::FileBookmarksFailure;

constexpr wchar_t folder[] = L"nib-bookmark-files";
constexpr wchar_t profile[] = L"nib-bookmark-files/profile";
constexpr wchar_t listed[] = L"nib-bookmark-files/profile/bookmarks.v1";
constexpr wchar_t large[] = L"nib-bookmark-files/large.v1";

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

void remove_file(const std::wstring &name)
{
    DeleteFileW(name.c_str());
    DeleteFileW((name + L".nib-tmp").c_str());
}

// 固定の名前なので、前回の実行が残していたものを消してから作り直す。
void reset_folder()
{
    remove_file(listed);
    remove_file(large);
    RemoveDirectoryW(profile);
    RemoveDirectoryW(folder);
    expect(CreateDirectoryW(folder, nullptr) != 0, "the temporary folder is created");
}

void clean_folder()
{
    remove_file(listed);
    remove_file(large);
    RemoveDirectoryW(profile);
    expect(RemoveDirectoryW(folder) != 0, "the temporary folder is removed");
}

core::FilePath path_of(std::string_view text)
{
    auto parsed = core::FilePath::parse(text);
    expect(parsed.has_value(), "the test path is valid");
    return std::move(parsed).value_or(core::FilePath::parse("C:\\invalid").value());
}

bool same_bookmarks(const FileBookmarks &left, const FileBookmarks &right)
{
    if (left.files.size() != right.files.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.files.size(); ++index)
    {
        if (left.files.at(index) != right.files.at(index))
        {
            return false;
        }
    }
    return true;
}

FileBookmarks three_files()
{
    return FileBookmarks{{path_of("C:\\work\\a b\\日本語.txt"), path_of("D:/notes/x,y=z.md"),
                          path_of("\\\\server\\share\\log, 2026=09.txt")}};
}

void round_trip(const FileBookmarks &bookmarks, const char *message)
{
    const auto bytes = adapters::encode_bookmarks(bookmarks);
    expect(bytes.has_value(), "the bookmarks is encoded");
    const auto decoded = adapters::decode_bookmarks(bytes.value_or(std::string{}));
    expect(decoded.has_value() && same_bookmarks(decoded.value(), bookmarks), message);
}

void verify_round_trips()
{
    round_trip(FileBookmarks{}, "an empty bookmarks round trips");
    round_trip(FileBookmarks{{path_of("C:\\a.txt")}}, "one file round trips");
    round_trip(three_files(), "spaces, Japanese, commas and equals signs in paths round trip");
    const auto bytes = adapters::encode_bookmarks(three_files());
    expect(bytes.value_or(std::string{}) ==
               "version=1\nC:\\work\\a b\\日本語.txt\nD:/notes/x,y=z.md\n"
               "\\\\server\\share\\log, 2026=09.txt\n",
           "the bookmarks is written with LF, in registration order");
    const auto empty = adapters::encode_bookmarks(FileBookmarks{});
    expect(empty.value_or(std::string{}) == "version=1\n", "an empty bookmarks is one line");
}

void verify_line_ends()
{
    const auto crlf =
        adapters::decode_bookmarks("\xEF\xBB\xBFversion=1\r\nC:\\a b.txt\r\nD:\\c.txt\r\n");
    expect(crlf.has_value() && crlf.value().files.size() == 2 &&
               crlf.value().files.front().text() == "C:\\a b.txt" &&
               crlf.value().files.back().text() == "D:\\c.txt",
           "BOM and CRLF are accepted and CR is not part of the path");
    const auto unterminated = adapters::decode_bookmarks("version=1\nC:\\a.txt");
    expect(unterminated.has_value() && unterminated.value().files.size() == 1,
           "the last line may lack its line feed");
    const auto header_only = adapters::decode_bookmarks("version=1");
    expect(header_only.has_value() && header_only.value().files.empty(),
           "a header without a line feed is an empty bookmarks");
}

void rejected(std::string_view bytes, Failure failure, const char *message)
{
    const auto result = adapters::decode_bookmarks(bytes);
    expect(!result && result.error() == failure, message);
}

void verify_rejected()
{
    rejected("", Failure::malformed, "an empty file is malformed");
    rejected("C:\\a.txt\n", Failure::malformed, "the version comes first");
    rejected("version=2\nC:\\a.txt\n", Failure::unsupported_version, "another version is refused");
    rejected("version=\n", Failure::unsupported_version, "an empty version is not version 1");
    rejected("version=1 \n", Failure::unsupported_version, "no spaces after the version");
    const std::string head = "version=1\n";
    rejected(head + "C:\\a.txt\n\nC:\\b.txt\n", Failure::malformed, "a blank line is malformed");
    rejected(head + "C:\\a.txt\n\n", Failure::malformed, "a trailing blank line is malformed");
    rejected(head + "a.txt\n", Failure::malformed, "a relative path is refused");
    rejected(head + "\\a.txt\n", Failure::malformed, "a path without a drive is refused");
    rejected(head + "C:\\a\tb.txt\n", Failure::malformed, "a control character is refused");
    rejected(head + "C:\\a\rb.txt\n", Failure::malformed, "a stray CR is refused");
    rejected(head + "C:\\\xFF.txt\n", Failure::malformed, "broken UTF-8 is refused");
    rejected(head + "C:\\a.txt\nb.txt\n", Failure::malformed,
             "one bad line refuses the whole bookmarks");
}

std::string listed_files(std::size_t count)
{
    std::string bytes = "version=1\n";
    for (std::size_t index = 0; index < count; ++index)
    {
        bytes += std::format("C:\\{}.txt\n", index);
    }
    return bytes;
}

void verify_limits()
{
    const auto most = adapters::decode_bookmarks(listed_files(adapters::maximum_bookmark_lines));
    expect(most.has_value() && most.value().files.size() == adapters::maximum_bookmark_lines,
           "the most lines are read");
    rejected(listed_files(adapters::maximum_bookmark_lines + 1), Failure::too_large,
             "one line more than the most is not read");
    FileBookmarks many;
    for (std::size_t index = 0; index <= adapters::maximum_bookmark_lines; ++index)
    {
        many.files.push_back(path_of(std::format("C:\\{}.txt", index)));
    }
    const auto encoded = adapters::encode_bookmarks(many);
    expect(!encoded && encoded.error() == Failure::too_large,
           "a bookmarks over the line limit is not written");
    const std::string long_path = "C:\\" + std::string(8000, 'a');
    FileBookmarks heavy;
    for (std::size_t index = 0; index < 140; ++index)
    {
        heavy.files.push_back(path_of(long_path));
    }
    const auto heavy_bytes = adapters::encode_bookmarks(heavy);
    expect(!heavy_bytes && heavy_bytes.error() == Failure::too_large,
           "a bookmarks over 1 MiB is not written");
    rejected(std::string(adapters::maximum_bookmark_bytes + 1, 'x'), Failure::too_large,
             "the codec refuses input over the byte limit too");
    const auto relative = adapters::encode_bookmarks(FileBookmarks{{path_of("relative.txt")}});
    expect(!relative && relative.error() == Failure::malformed,
           "a relative path cannot be written");
    expect(adapters::maximum_bookmark_lines == nenenib::application::bookmark_limit,
           "the line limit is the bookmark limit");
}

void verify_adapter_round_trip(adapters::Win32FileAdapter &files)
{
    adapters::Win32BookmarkAdapter bookmarks(files,
                                             path_of("nib-bookmark-files/profile/bookmarks.v1"));
    const auto missing = bookmarks.read();
    expect(missing.has_value() && missing.value().files.empty(),
           "a missing bookmarks reads as an empty bookmarks");
    expect(bookmarks.write(three_files()).has_value(),
           "the first write creates the profile folder");
    adapters::Win32BookmarkAdapter restarted(files,
                                             path_of("nib-bookmark-files/profile/bookmarks.v1"));
    const auto restored = restarted.read();
    expect(restored.has_value() && same_bookmarks(restored.value(), three_files()),
           "the written bookmarks is read back");
    expect(SetFileAttributesW(listed, FILE_ATTRIBUTE_READONLY) != 0,
           "the stored list is made read-only");
    const auto refused = bookmarks.write(FileBookmarks{});
    expect(!refused && refused.error() == Failure::unwritable, "replacing a read-only list fails");
    const auto protected_list = bookmarks.read();
    expect(protected_list.has_value() && same_bookmarks(protected_list.value(), three_files()),
           "a failed atomic replacement preserves the original list");
    expect(SetFileAttributesW(listed, FILE_ATTRIBUTE_NORMAL) != 0,
           "the stored list is made writable again");
    expect(bookmarks.write(FileBookmarks{}).has_value(),
           "an empty bookmarks replaces the previous one");
    const auto emptied = bookmarks.read();
    expect(emptied.has_value() && emptied.value().files.empty(),
           "the empty bookmarks is read back");
}

void verify_adapter_failures(adapters::Win32FileAdapter &files)
{
    const auto large_path = path_of("nib-bookmark-files/large.v1");
    expect(
        files.write(large_path, std::string(adapters::maximum_bookmark_bytes + 1, 'x')).has_value(),
        "the oversized fixture is written");
    adapters::Win32BookmarkAdapter oversized(files, large_path);
    const auto too_large = oversized.read();
    expect(!too_large && too_large.error() == Failure::too_large, "a file over 1 MiB is refused");
    expect(files.write(large_path, "version=3\nC:\\a.txt\n").has_value(),
           "a v3 bookmarks is written");
    const auto version = oversized.read();
    expect(!version && version.error() == Failure::unsupported_version,
           "the adapter reports another version");
    expect(files.write(large_path, "version=1\nrelative.txt\n").has_value(),
           "a broken bookmarks is written");
    const auto broken = oversized.read();
    expect(!broken && broken.error() == Failure::malformed,
           "the adapter reports a broken bookmarks");
    adapters::Win32BookmarkAdapter directory(files, path_of("nib-bookmark-files/profile"));
    const auto unreadable = directory.read();
    expect(!unreadable && unreadable.error() == Failure::unreadable,
           "a folder is not mistaken for a missing bookmarks");
    adapters::Win32BookmarkAdapter deeper(
        files, path_of("nib-bookmark-files/missing/deeper/bookmarks.v1"));
    const auto unwritable = deeper.write(three_files());
    expect(!unwritable && unwritable.error() == Failure::unwritable,
           "only one missing folder level is created");
    adapters::Win32BookmarkAdapter unavailable(files,
                                               std::unexpected(Failure::location_unavailable));
    const auto nowhere = unavailable.read();
    expect(!nowhere && nowhere.error() == Failure::location_unavailable,
           "an unavailable location is distinct from a missing bookmarks");
    const auto unsaved = unavailable.write(three_files());
    expect(!unsaved && unsaved.error() == Failure::location_unavailable,
           "an unavailable location cannot be written");
}
} // namespace

int main()
{
    verify_round_trips();
    verify_line_ends();
    verify_rejected();
    verify_limits();
    reset_folder();
    adapters::Win32FileAdapter files;
    verify_adapter_round_trip(files);
    verify_adapter_failures(files);
    clean_folder();
    std::printf("Bookmark adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
