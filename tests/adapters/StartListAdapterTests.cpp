// Issue #1442（#1018 の段 1）:
// 起動画面の読み（Win32StartListAdapter）を、本物の履歴とブックマークの adapter と本物のワーカー 1
// 本で測る（ADR 0172 の決定 3）。一時フォルダは temporary_test_directory
// （#1103）の中に作る。時刻は読まず、待つのは合図の受け皿（セマフォ）だけで、止まらないときは
// CTest の制限時間で落ちる。
#include "FileBookmarks.hpp"
#include "FileHistory.hpp"
#include "FilePath.hpp"
#include "StartLists.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Win32BookmarkAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32HistoryAdapter.hpp"
#include "Win32StartListAdapter.hpp"
#include "Win32Worker.hpp"

#include <cstddef>
#include <cstdio>
#include <semaphore>
#include <string_view>
#include <utility>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::FileBookmarks;
using nenenib::application::FileHistory;
using Arrivals = std::counting_semaphore<>;

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

core::FilePath path_of(std::string_view text)
{
    auto parsed = core::FilePath::parse(text);
    expect(parsed.has_value(), "the test path is valid");
    return std::move(parsed).value_or(core::FilePath::parse("C:\\invalid").value());
}

// 書いた履歴とブックマークを、ワーカーの 1 回の読みが Ctrl+P と同じ読み口で返す。
void verify_written_lists(adapters::Win32FileAdapter &files)
{
    adapters::Win32HistoryAdapter history(files, path_of("nib-start-lists/profile/history.v1"));
    adapters::Win32BookmarkAdapter bookmarks(files,
                                             path_of("nib-start-lists/profile/bookmarks.v1"));
    expect(history.write(FileHistory{{path_of("C:\\work\\b.txt"), path_of("C:\\work\\a.txt")}})
               .has_value(),
           "the history is written");
    expect(bookmarks.write(FileBookmarks{{path_of("C:\\work\\m.txt")}}).has_value(),
           "the bookmarks are written");
    adapters::Win32Worker worker;
    Arrivals arrived(0);
    adapters::Win32StartListAdapter lists(worker, history, bookmarks);
    lists.bind([&arrived] { arrived.release(); });
    expect(!lists.collect().has_value() && !worker.started(),
           "nothing is read and the worker sleeps before the request");
    lists.request();
    arrived.acquire();
    const auto read = lists.collect();
    expect(read.has_value() && read.value().recent.size() == 2 &&
               read.value().recent.at(0) == path_of("C:\\work\\b.txt") &&
               read.value().bookmarks.size() == 1 &&
               read.value().bookmarks.at(0) == path_of("C:\\work\\m.txt"),
           "one request reads the history in its order and the bookmarks on the worker");
    expect(!lists.collect().has_value(), "the read lists are taken once");
}

// ファイルが無ければ空の節（起動画面はその節を出さない）。
void verify_missing_lists(adapters::Win32FileAdapter &files)
{
    adapters::Win32HistoryAdapter history(files, path_of("nib-start-lists/none/history.v1"));
    adapters::Win32BookmarkAdapter bookmarks(files, path_of("nib-start-lists/none/bookmarks.v1"));
    adapters::Win32Worker worker;
    Arrivals arrived(0);
    adapters::Win32StartListAdapter lists(worker, history, bookmarks);
    lists.bind([&arrived] { arrived.release(); });
    lists.request();
    arrived.acquire();
    const auto read = lists.collect();
    expect(read.has_value() && read.value().recent.empty() && read.value().bookmarks.empty(),
           "missing files read as empty sections");
}

int run_tests()
{
    adapters::Win32FileAdapter files;
    verify_written_lists(files);
    verify_missing_lists(files);
    std::fprintf(stdout, "%zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"start-lists");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made (error %lu)\n",
                     static_cast<unsigned long>(directory.error()));
        return 1;
    }
    const int result = run_tests();
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
