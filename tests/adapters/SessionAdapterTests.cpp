// Issue #252: 前回のタブの一覧（session.v1）のコーデックと adapter を、ビルドディレクトリ配下の
// 一時フォルダで測る（QLT-013 / ADR 0059 の決定 2）。使う人の %LOCALAPPDATA% には触れない。
// 一時フォルダの名前は固定で、冒頭で消して作り直す。時刻も乱数も使わない（ARC-007）。
#include "FilePath.hpp"
#include "Session.hpp"
#include "SessionCodec.hpp"
#include "SessionFailure.hpp"
#include "SessionTab.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32SessionAdapter.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::Session;
using nenenib::application::SessionTab;
using Failure = nenenib::application::SessionFailure;

constexpr wchar_t folder[] = L"nib-session-files";
constexpr wchar_t profile[] = L"nib-session-files/profile";
constexpr wchar_t listed[] = L"nib-session-files/profile/session.v1";
constexpr wchar_t large[] = L"nib-session-files/large.v1";

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

// 数は「カーソルの行・桁・画面の先頭の行・使った順の順位」の順。
SessionTab tab_of(std::string_view path, std::array<std::size_t, 4> numbers)
{
    return SessionTab{
        path_of(path),
        core::TextPosition{core::LineNumber{numbers.at(0)}, core::Column{numbers.at(1)}},
        core::LineNumber{numbers.at(2)}, numbers.at(3)};
}

bool same_tab(const SessionTab &left, const SessionTab &right)
{
    return left.path == right.path && left.caret == right.caret &&
           left.first_visible == right.first_visible && left.recency == right.recency;
}

bool same_session(const Session &left, const Session &right)
{
    if (left.active != right.active || left.tabs.size() != right.tabs.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.tabs.size(); ++index)
    {
        if (!same_tab(left.tabs.at(index), right.tabs.at(index)))
        {
            return false;
        }
    }
    return true;
}

Session three_tabs()
{
    return Session{{tab_of("C:\\work\\a b\\日本語.txt", {12, 5, 3, 1}),
                    tab_of("D:/notes/x,y=z.md", {1, 1, 1, 0}),
                    tab_of("\\\\server\\share\\log, 2026=09.txt", {400, 17, 380, 2})},
                   1};
}

void round_trip(const Session &session, const char *message)
{
    const auto bytes = adapters::encode_session(session);
    expect(bytes.has_value(), "the list is encoded");
    const auto decoded = adapters::decode_session(bytes.value_or(std::string{}));
    expect(decoded.has_value() && same_session(decoded.value(), session), message);
}

void verify_round_trips()
{
    round_trip(Session{{}, 0}, "an empty list round trips");
    round_trip(Session{{tab_of("C:\\a.txt", {2, 3, 1, 0})}, 0}, "one tab round trips");
    round_trip(three_tabs(), "spaces, Japanese, commas and equals signs in paths round trip");
    const auto bytes = adapters::encode_session(three_tabs());
    expect(
        bytes.value_or(std::string{}) ==
            "version=1\nactive=1\n12,5,3,1,C:\\work\\a b\\日本語.txt\n1,1,1,0,D:/notes/x,y=z.md\n"
            "400,17,380,2,\\\\server\\share\\log, 2026=09.txt\n",
        "the list is written with LF in band order");
    const auto empty = adapters::encode_session(Session{{}, 0});
    expect(empty.value_or(std::string{}) == "version=1\nactive=0\n", "an empty list is two lines");
}

void verify_line_ends()
{
    const auto crlf = adapters::decode_session("\xEF\xBB\xBFversion=1\r\nactive=0\r\n"
                                               "3,2,1,0,C:\\a b.txt\r\n");
    expect(crlf.has_value() && crlf.value().tabs.size() == 1 &&
               crlf.value().tabs.front().path.text() == "C:\\a b.txt" &&
               crlf.value().tabs.front().caret.line.value == 3,
           "BOM and CRLF are accepted and CR is not part of the path");
    const auto unterminated = adapters::decode_session("version=1\nactive=0\n1,1,1,0,C:\\a.txt");
    expect(unterminated.has_value() && unterminated.value().tabs.size() == 1,
           "the last line may lack its line feed");
}

void rejected(std::string_view bytes, Failure failure, const char *message)
{
    const auto result = adapters::decode_session(bytes);
    expect(!result && result.error() == failure, message);
}

void verify_rejected_headers()
{
    rejected("", Failure::malformed, "an empty file is malformed");
    rejected("version=2\nactive=0\n", Failure::unsupported_version, "another version is refused");
    rejected("active=0\nversion=1\n", Failure::malformed, "the version comes first");
    rejected("version=1\n", Failure::malformed, "the active line is required");
    rejected("version=1\nactive=x\n", Failure::malformed, "active must be a number");
    rejected("version=1\nactive=-1\n", Failure::malformed, "active has no sign");
    rejected("version=1\nactive=1\n", Failure::malformed, "active 1 is outside an empty list");
    rejected("version=1\nactive=1\n1,1,1,0,C:\\a.txt\n", Failure::malformed,
             "active past the last tab is refused");
    rejected("version=1\nactive= 0\n", Failure::malformed, "no spaces around numbers");
}

void verify_rejected_tabs()
{
    const std::string head = "version=1\nactive=0\n";
    rejected(head + "1,1,1,C:\\a.txt\n", Failure::malformed, "a missing field is malformed");
    rejected(head + "1,x,1,0,C:\\a.txt\n", Failure::malformed, "a column must be a number");
    rejected(head + "0,1,1,0,C:\\a.txt\n", Failure::malformed, "lines start at 1");
    rejected(head + "1,1,0,0,C:\\a.txt\n", Failure::malformed,
             "the first visible line starts at 1");
    rejected(head + "1,1,1,0,\n", Failure::malformed, "an empty path is malformed");
    rejected(head + "1,1,1,0,a.txt\n", Failure::malformed, "a relative path is refused");
    rejected(head + "1,1,1,0,\\a.txt\n", Failure::malformed, "a path without a drive is refused");
    rejected(head + "1,1,1,0,C:\\a\tb.txt\n", Failure::malformed, "a control character is refused");
    rejected(head + "1,1,1,0,C:\\a\rb.txt\n", Failure::malformed, "a stray CR is refused");
    rejected(head + "1,1,1,0,C:\\\xFF.txt\n", Failure::malformed, "broken UTF-8 is refused");
    rejected(head + "1,1,1,0,C:\\a.txt\n\n", Failure::malformed, "a blank line is malformed");
    rejected(head + "1,1,1,0,C:\\a.txt\n1,1,1,0,C:\\b.txt\n", Failure::malformed,
             "a repeated rank is refused");
    rejected(head + "1,1,1,0,C:\\a.txt\n1,1,1,2,C:\\b.txt\n", Failure::malformed,
             "a gap in the ranks is refused");
}

std::string listed_tabs(std::size_t count)
{
    std::string bytes = "version=1\nactive=0\n";
    for (std::size_t index = 0; index < count; ++index)
    {
        bytes += std::format("1,1,1,{},C:\\{}.txt\n", index, index);
    }
    return bytes;
}

void verify_limits()
{
    Session full{{}, 0};
    for (std::size_t index = 0; index < adapters::maximum_session_tabs; ++index)
    {
        full.tabs.push_back(tab_of(std::format("C:\\{}.txt", index), {1, 1, 1, index}));
    }
    round_trip(full, "256 tabs round trip");
    full.tabs.push_back(tab_of("C:\\last.txt", {1, 1, 1, adapters::maximum_session_tabs}));
    const auto encoded = adapters::encode_session(full);
    expect(!encoded && encoded.error() == Failure::too_large, "257 tabs are not written");
    rejected(listed_tabs(adapters::maximum_session_tabs + 1), Failure::too_large,
             "257 tab lines are not read");
    const std::string long_path = "C:\\" + std::string(8000, 'a');
    Session heavy{{}, 0};
    for (std::size_t index = 0; index < 140; ++index)
    {
        heavy.tabs.push_back(tab_of(long_path, {1, 1, 1, index}));
    }
    const auto heavy_bytes = adapters::encode_session(heavy);
    expect(!heavy_bytes && heavy_bytes.error() == Failure::too_large,
           "a list over 1 MiB is not written");
}

void verify_adapter_round_trip(adapters::Win32FileAdapter &files)
{
    adapters::Win32SessionAdapter session(files, path_of("nib-session-files/profile/session.v1"));
    const auto missing = session.read();
    expect(missing.has_value() && !missing.value().has_value(), "a missing list is no list");
    expect(session.write(three_tabs()).has_value(), "the first write creates the profile folder");
    const auto restored = session.read().value_or(std::nullopt);
    expect(restored.has_value() && same_session(restored.value(), three_tabs()),
           "the written list is read back");
    expect(session.write(Session{{}, 0}).has_value(), "an empty list replaces the previous one");
    const auto emptied = session.read().value_or(std::nullopt);
    expect(emptied.has_value() && emptied.value().tabs.empty(),
           "the empty list is read back as an empty list, not as no list");
}

void verify_adapter_failures(adapters::Win32FileAdapter &files)
{
    const auto large_path = path_of("nib-session-files/large.v1");
    expect(
        files.write(large_path, std::string(adapters::maximum_session_bytes + 1, 'x')).has_value(),
        "the oversized fixture is written");
    adapters::Win32SessionAdapter oversized(files, large_path);
    const auto too_large = oversized.read();
    expect(!too_large && too_large.error() == Failure::too_large, "a file over 1 MiB is refused");
    expect(files.write(large_path, "version=3\nactive=0\n").has_value(), "a v3 list is written");
    const auto version = oversized.read();
    expect(!version && version.error() == Failure::unsupported_version,
           "the adapter reports another version");
    adapters::Win32SessionAdapter directory(files, path_of("nib-session-files/profile"));
    const auto unreadable = directory.read();
    expect(!unreadable && unreadable.error() == Failure::unreadable,
           "a folder is not mistaken for a missing list");
    adapters::Win32SessionAdapter deeper(files,
                                         path_of("nib-session-files/missing/deeper/session.v1"));
    const auto unwritable = deeper.write(three_tabs());
    expect(!unwritable && unwritable.error() == Failure::unwritable,
           "only one missing folder level is created");
    adapters::Win32SessionAdapter unavailable(files,
                                              std::unexpected(Failure::location_unavailable));
    const auto nowhere = unavailable.read();
    expect(!nowhere && nowhere.error() == Failure::location_unavailable,
           "an unavailable location is distinct from a missing list");
    const auto unsaved = unavailable.write(three_tabs());
    expect(!unsaved && unsaved.error() == Failure::location_unavailable,
           "an unavailable location cannot be written");
}
} // namespace

int main()
{
    verify_round_trips();
    verify_line_ends();
    verify_rejected_headers();
    verify_rejected_tabs();
    verify_limits();
    reset_folder();
    adapters::Win32FileAdapter files;
    verify_adapter_round_trip(files);
    verify_adapter_failures(files);
    clean_folder();
    std::printf("Session adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
