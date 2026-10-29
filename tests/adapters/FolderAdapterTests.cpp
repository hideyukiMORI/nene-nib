// Issue #271: 同じフォルダの列挙（Win32FolderAdapter）とワーカー 1 本（Win32Worker）を、
// 本物のフォルダと本物のスレッドで測る（ADR 0062 の決定 5〜9）。
// 一時フォルダは
// temporary_test_directory（#1103）の中に固定の名前で作り、冒頭で前回の残りを消して終わりに消す。
// 時刻は読まない（Sleep・時計・タイムアウトの待ちを書かない）。
// 待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest の制限時間で落ちる。
#include "ExplorerTree.hpp"
#include "FilePath.hpp"
#include "FindHandle.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderListingKind.hpp"
#include "FolderProgress.hpp"
#include "FolderRequest.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Win32FolderAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <memory>
#include <optional>
#include <semaphore>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::FolderBatch;
using nenenib::application::FolderEntry;
using nenenib::application::FolderListingKind;
using nenenib::application::FolderProgress;
using nenenib::application::FolderRequest;
using Batches = std::vector<FolderBatch>;
using Arrivals = std::counting_semaphore<>;

constexpr wchar_t root[] = L"nib-folder-files";
constexpr std::size_t small_batch = 4;
constexpr std::size_t small_limit = 10;
constexpr std::size_t no_limit = 100;

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

void remove_tree(const std::wstring &folder);

void remove_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data)
{
    const std::wstring name(static_cast<const wchar_t *>(data.cFileName));
    if (name == L"." || name == L"..")
    {
        return;
    }
    const auto path = folder + L"\\" + name;
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        remove_tree(path);
        return;
    }
    DeleteFileW(path.c_str());
}

void remove_entries(const std::wstring &folder)
{
    WIN32_FIND_DATAW data{};
    const adapters::FindHandle search(FindFirstFileW((folder + L"\\*").c_str(), &data));
    if (!search.valid())
    {
        return;
    }
    do
    {
        remove_entry(folder, data);
    } while (FindNextFileW(search.get(), &data) != 0);
}

void remove_tree(const std::wstring &folder)
{
    remove_entries(folder);
    RemoveDirectoryW(folder.c_str());
}

std::wstring inside(std::wstring_view name)
{
    return std::wstring(root) + L"\\" + std::wstring(name);
}

void make_folder(const std::wstring &folder)
{
    expect(CreateDirectoryW(folder.c_str(), nullptr) != 0, "a temporary folder is created");
}

void make_file(const std::wstring &path, DWORD attributes)
{
    const HANDLE file =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, attributes, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a temporary file is created");
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
}

// 0.txt から count - 1.txt までの空のファイルを持つフォルダ。
std::wstring numbered_folder(std::wstring_view name, std::size_t count)
{
    const auto folder = inside(name);
    make_folder(folder);
    for (std::size_t index = 0; index < count; ++index)
    {
        make_file(std::format(L"{}\\{}.txt", folder, index), FILE_ATTRIBUTE_NORMAL);
    }
    return folder;
}

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

core::FilePath folder_path(std::string_view name)
{
    return path_of(std::format("nib-folder-files\\{}", name));
}

[[nodiscard]] bool is_final(const FolderBatch &batch, std::uint64_t ticket)
{
    return batch.ticket == ticket && batch.progress != FolderProgress::more;
}

[[nodiscard]] bool has_final(const Batches &batches, std::uint64_t ticket)
{
    for (const auto &batch : batches)
    {
        if (is_final(batch, ticket))
        {
            return true;
        }
    }
    return false;
}

// 合図を待っては受け取り、券 ticket の最後の batch が来るまで続ける。
Batches until_final(adapters::Win32FolderAdapter &folders, Arrivals &arrived, std::uint64_t ticket)
{
    Batches seen;
    while (!has_final(seen, ticket))
    {
        arrived.acquire();
        for (auto &batch : folders.collect())
        {
            seen.push_back(std::move(batch));
        }
    }
    return seen;
}

Batches listed(const core::FilePath &folder, std::size_t batch_files, std::size_t file_limit)
{
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32FolderAdapter folders(worker, batch_files, file_limit);
    folders.bind([&arrived] { arrived.release(); });
    folders.list(FolderRequest{folder, 1});
    auto batches = until_final(folders, arrived, 1);
    expect(folders.collect().empty(), "nothing is left after the last batch");
    return batches;
}

std::vector<std::size_t> sizes_of(const Batches &batches)
{
    std::vector<std::size_t> sizes;
    for (const auto &batch : batches)
    {
        sizes.push_back(batch.files.size());
    }
    return sizes;
}

std::vector<FolderProgress> progress_of(const Batches &batches)
{
    std::vector<FolderProgress> progress;
    for (const auto &batch : batches)
    {
        progress.push_back(batch.progress);
    }
    return progress;
}

void expect_shape(const Batches &batches, const std::vector<std::size_t> &sizes,
                  const std::vector<FolderProgress> &progress, const char *message)
{
    expect(sizes_of(batches) == sizes && progress_of(batches) == progress, message);
}

constexpr auto more = FolderProgress::more;
constexpr auto complete = FolderProgress::complete;
constexpr auto truncated = FolderProgress::truncated;

void verify_batches()
{
    numbered_folder(L"even", 8);
    numbered_folder(L"odd", 10);
    numbered_folder(L"empty", 0);
    expect_shape(listed(folder_path("even"), small_batch, no_limit), {4, 4}, {more, complete},
                 "a count that divides evenly ends with a full complete batch");
    expect_shape(listed(folder_path("odd"), small_batch, no_limit), {4, 4, 2},
                 {more, more, complete}, "the remainder is the complete batch");
    expect_shape(listed(folder_path("empty"), small_batch, no_limit), {0}, {complete},
                 "an empty folder gives one empty complete batch");
    const auto files = listed(folder_path("odd"), small_batch, no_limit);
    expect(files.front().ticket == 1 && files.back().ticket == 1, "every batch carries the ticket");
    expect(files.front().files.front() == path_of("nib-folder-files\\odd\\0.txt"),
           "files come in the order the file system returns");
}

void verify_limits()
{
    numbered_folder(L"over", 11);
    expect_shape(listed(folder_path("over"), small_batch, small_limit), {4, 4, 2},
                 {more, more, truncated}, "a folder over the limit stops at the limit");
    expect_shape(listed(folder_path("odd"), small_batch, small_limit), {4, 4, 2},
                 {more, more, complete}, "a folder exactly at the limit is complete");
    expect_shape(listed(folder_path("even"), small_batch, small_batch), {4}, {truncated},
                 "a limit on a batch boundary truncates without an empty batch");
    expect(adapters::folder_batch_files == 1024 && adapters::folder_file_limit == 8192,
           "the default batch is 1024 files and the default limit is 8192");
}

bool contains(const Batches &batches, const core::FilePath &file)
{
    return std::ranges::any_of(
        batches, [&file](const FolderBatch &batch)
        { return std::ranges::find(batch.files, file) != batch.files.end(); });
}

void verify_skipped()
{
    const auto folder = inside(L"mixed");
    make_folder(folder);
    make_folder(folder + L"\\below");
    make_file(folder + L"\\below\\deep.txt", FILE_ATTRIBUTE_NORMAL);
    make_file(folder + L"\\plain.txt", FILE_ATTRIBUTE_NORMAL);
    make_file(folder + L"\\日本語の名前.txt", FILE_ATTRIBUTE_NORMAL);
    make_file(folder + L"\\hidden.txt", FILE_ATTRIBUTE_HIDDEN);
    make_file(folder + L"\\system.txt", FILE_ATTRIBUTE_SYSTEM);
    const auto batches = listed(folder_path("mixed"), small_batch, no_limit);
    expect_shape(batches, {2}, {complete}, "only the two plain files are listed");
    expect(contains(batches, path_of("nib-folder-files\\mixed\\plain.txt")),
           "a plain file is listed with the folder and one separator");
    expect(contains(batches, path_of("nib-folder-files\\mixed\\日本語の名前.txt")),
           "a Japanese name is listed in UTF-8");
    const auto trailing = listed(path_of("nib-folder-files\\mixed\\"), small_batch, no_limit);
    expect(contains(trailing, path_of("nib-folder-files\\mixed\\plain.txt")),
           "a folder ending in a separator does not double it");
}

// エクスプローラーの要求（ADR 0162 の決定 5）。`.` `..` を除く直下の項目を、フォルダか・隠しかと
// 一緒に返す（ファイルの列は空）。上限は entries の上限で、ファイルの上限ではない。
Batches listed_entries(const core::FilePath &folder, std::size_t file_limit,
                       std::size_t entry_limit)
{
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32FolderAdapter folders(worker, small_batch, file_limit, entry_limit);
    folders.bind([&arrived] { arrived.release(); });
    folders.list(FolderRequest{folder, 1, FolderListingKind::entries});
    return until_final(folders, arrived, 1);
}

[[nodiscard]] std::vector<FolderEntry> entries_of(const Batches &batches)
{
    std::vector<FolderEntry> entries;
    for (const auto &batch : batches)
    {
        entries.insert(entries.end(), batch.entries.begin(), batch.entries.end());
    }
    return entries;
}

[[nodiscard]] std::optional<FolderEntry> entry_named(const Batches &batches, std::string_view name)
{
    const auto entries = entries_of(batches);
    const auto found = std::ranges::find_if(entries, [name](const FolderEntry &entry)
                                            { return entry.path.file_name() == name; });
    return found == entries.end() ? std::nullopt : std::optional{*found};
}

void verify_entries()
{
    const auto batches = listed_entries(folder_path("mixed"), no_limit, no_limit);
    std::size_t total = 0;
    bool files_empty = true;
    for (const auto &batch : batches)
    {
        total += batch.entries.size();
        files_empty = files_empty && batch.files.empty();
    }
    expect(total == 5 && files_empty && batches.back().progress == FolderProgress::complete,
           "entries list every item but . and .. and no files");
    const auto below = entry_named(batches, "below");
    const auto hidden = entry_named(batches, "hidden.txt");
    const auto plain = entry_named(batches, "plain.txt");
    expect(below.has_value() && below.value().folder && !below.value().hidden,
           "a folder entry is marked as a folder");
    expect(hidden.has_value() && hidden.value().hidden && !hidden.value().folder,
           "a hidden file is listed and marked hidden");
    expect(plain.has_value() && plain.value().path == path_of("nib-folder-files\\mixed\\plain.txt"),
           "an entry carries the full path");
    expect(entry_named(batches, "system.txt").has_value() &&
               entry_named(batches, "日本語の名前.txt").has_value(),
           "system files and Japanese names are entries too");
    const auto limited = listed_entries(folder_path("odd"), 2, 4);
    std::size_t kept = 0;
    for (const auto &batch : limited)
    {
        kept += batch.entries.size();
    }
    expect(kept == 4 && limited.back().progress == FolderProgress::truncated,
           "entries stop at the entry limit, not the file limit");
    expect(adapters::folder_entry_limit == core::ExplorerTree::entry_limit + 1,
           "the default entry limit is one over the tree's limit so the tree can tell");
}

void verify_failed()
{
    expect_shape(listed(folder_path("missing"), small_batch, no_limit), {0},
                 {FolderProgress::failed}, "a missing folder gives one empty failed batch");
    expect_shape(listed(path_of("nib-folder-files\\odd\\0.txt"), small_batch, no_limit), {0},
                 {FolderProgress::failed}, "a file is not a folder");
}

std::vector<std::size_t> ticket_sizes(const Batches &batches, std::uint64_t ticket)
{
    std::vector<std::size_t> sizes;
    for (const auto &batch : batches)
    {
        if (batch.ticket == ticket)
        {
            sizes.push_back(batch.files.size());
        }
    }
    return sizes;
}

// 1 つ目の batch が届いた合図の中（ワーカーのスレッド）で次の list を呼ぶので、前の列挙は必ず
// 1 つ目の batch の後でやめる。たまたまの速さに頼らない。
void verify_new_ticket_stops_previous()
{
    numbered_folder(L"long", 12);
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32FolderAdapter folders(worker, small_batch, no_limit);
    bool relisted = false; // 合図の中（ワーカーのスレッド）だけが読み書きする
    folders.bind(
        [&]
        {
            if (!relisted)
            {
                relisted = true;
                folders.list(FolderRequest{folder_path("odd"), 2});
            }
            arrived.release();
        });
    folders.list(FolderRequest{folder_path("long"), 1});
    const auto batches = until_final(folders, arrived, 2);
    expect(ticket_sizes(batches, 1) == std::vector<std::size_t>{4},
           "the previous ticket gives only its first batch");
    expect(!has_final(batches, 1), "the previous ticket never ends");
    expect(ticket_sizes(batches, 2) == std::vector<std::size_t>{4, 4, 2},
           "the new ticket is listed to the end");
    expect(batches.back().ticket == 2 && batches.back().progress == complete,
           "the new ticket ends complete");
}

// 列挙が合図の中で止まっている間に adapter を壊す（列挙は必ず途中）。
void destroy_adapter_while_signalling(adapters::Win32Worker &worker, Arrivals &arrived,
                                      Arrivals &resume, std::size_t &signals)
{
    adapters::Win32FolderAdapter folders(worker, small_batch, no_limit);
    folders.bind(
        [&]
        {
            ++signals;
            arrived.release();
            resume.acquire();
        });
    folders.list(FolderRequest{folder_path("long"), 1});
    arrived.acquire();
}

// adapter を壊してから合図を放し、ワーカーを壊す。
void verify_stops_during_listing()
{
    Arrivals arrived{0};
    Arrivals resume{0};
    std::size_t signals = 0; // ワーカーのスレッドだけが書き、join の後に読む
    {
        adapters::Win32Worker worker;
        destroy_adapter_while_signalling(worker, arrived, resume, signals);
        resume.release();
    }
    expect(signals == 1, "a destroyed adapter no longer signals");
}

// 列挙の途中でワーカーを先に壊しても戻ってくる。
void verify_worker_stops_first()
{
    Arrivals arrived{0};
    auto worker = std::make_unique<adapters::Win32Worker>();
    adapters::Win32FolderAdapter folders(*worker, small_batch, no_limit);
    folders.bind([&arrived] { arrived.release(); });
    folders.list(FolderRequest{folder_path("long"), 1});
    expect(worker->started(), "the first request starts the thread");
    arrived.acquire();
    worker.reset();
    expect(!folders.collect().empty(), "what was read before stopping can still be collected");
}

void verify_never_started()
{
    adapters::Win32Worker worker;
    {
        adapters::Win32FolderAdapter folders(worker);
        expect(folders.collect().empty(), "nothing is collected before a request");
    }
    expect(!worker.started(), "a worker without a request has no thread");
}
int run_tests()
{
    remove_tree(root);
    make_folder(root);
    verify_never_started();
    verify_batches();
    verify_limits();
    verify_skipped();
    verify_entries();
    verify_failed();
    verify_new_ticket_stops_previous();
    verify_stops_during_listing();
    verify_worker_stops_first();
    remove_tree(root);
    expect(GetFileAttributesW(root) == INVALID_FILE_ATTRIBUTES, "the temporary folder is removed");
    std::printf("Folder adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"folder");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made\n");
        return 1;
    }
    const int result = run_tests();
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
