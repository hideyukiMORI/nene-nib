// Issue #498: MAX_PATH を超える経路のファイルを、本物のフォルダで書く → 読む → 一時ファイル経由で
// 置き換える → 列挙する → 消す。経路の正規化は adapters の native_path の 1 本。
// この試験の exe は manifest に longPathAware を持たないので、`\\?\` を付けない長い経路は
// レジストリの LongPathsEnabled に依らず落ちる（反例）。製品の窓の exe は manifest でも宣言する。
// 一時フォルダは
// temporary_test_directory（#1103）の中に固定の名前で作り、冒頭で前回の残りを消して終わりに消す。
// 時刻は読まない。待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest
// の制限時間で落ちる。
#include "FilePath.hpp"
#include "FileWriteMode.hpp"
#include "FindHandle.hpp"
#include "FolderBatch.hpp"
#include "FolderProgress.hpp"
#include "FolderRequest.hpp"
#include "NativePath.hpp"
#include "ParentDirectory.hpp"
#include "TemporaryFiles.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Utf16.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32FolderAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <semaphore>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::FileFailure;
using nenenib::application::FileWriteMode;
using nenenib::application::FolderBatch;
using nenenib::application::FolderProgress;
using nenenib::application::FolderRequest;
using Arrivals = std::counting_semaphore<>;

constexpr wchar_t root_name[] = L"nib-long-path-files";
constexpr wchar_t short_name[] = L"nib-long-path-short";
constexpr std::wstring_view segment = L"long-path-segment-0123456789abcdef";
constexpr std::size_t depth = 10;
constexpr std::size_t long_length = 300;
constexpr std::size_t read_limit = 64;
constexpr std::size_t prefixed_length = MAX_PATH - 12;

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

[[nodiscard]] std::wstring absolute(const wchar_t *name)
{
    const DWORD length = GetFullPathNameW(name, 0, nullptr, nullptr);
    std::wstring buffer(length, L'\0');
    const DWORD written = GetFullPathNameW(name, length, buffer.data(), nullptr);
    buffer.resize(written);
    return buffer;
}

[[nodiscard]] core::FilePath path_of(const std::wstring &wide)
{
    return core::FilePath::parse(core::to_utf8(wide).value()).value();
}

[[nodiscard]] bool exists(const std::wstring &path)
{
    return GetFileAttributesW(adapters::native_path(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

void remove_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data);

// フォルダの中身ごと消す。長い経路でも消せるよう、Win32 へは native_path の形で渡す。
void remove_entries(const std::wstring &folder)
{
    WIN32_FIND_DATAW data{};
    const adapters::FindHandle search(
        FindFirstFileW(adapters::native_path(folder + L"\\*").c_str(), &data));
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
    RemoveDirectoryW(adapters::native_path(folder).c_str());
}

void remove_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data)
{
    const std::wstring name(static_cast<const wchar_t *>(data.cFileName));
    if (name == L"." || name == L"..")
    {
        return;
    }
    const std::wstring path = folder + L"\\" + name;
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        remove_tree(path);
        return;
    }
    SetFileAttributesW(adapters::native_path(path).c_str(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(adapters::native_path(path).c_str());
}

// root の下に 10 段のフォルダを adapters の ensure_parent_directory で 1 段ずつ作る。
[[nodiscard]] std::wstring deep_folder(const std::wstring &root)
{
    std::wstring folder = root;
    for (std::size_t level = 0; level < depth; ++level)
    {
        folder += L"\\";
        folder += segment;
        expect(adapters::ensure_parent_directory(folder + L"\\x"), "each long level is created");
    }
    return folder;
}

[[nodiscard]] std::string read_text(adapters::Win32FileAdapter &files, const std::wstring &path)
{
    const auto bytes = files.read(path_of(path), read_limit);
    return bytes ? std::string(bytes.value().view()) : std::string{};
}

void verify_native_form(const std::wstring &deep)
{
    const std::wstring shallow = absolute(short_name);
    expect(adapters::native_path(shallow) == shallow, "a short path is passed unchanged");
    const std::wstring native = adapters::native_path(deep);
    expect(native.starts_with(L"\\\\?\\") && native.substr(4) == deep,
           "a long drive path gets the verbatim prefix");
    expect(adapters::native_path(native) == native, "a prefixed path is not prefixed twice");
    std::wstring slashed = deep;
    std::ranges::replace(slashed, L'\\', L'/');
    expect(adapters::native_path(slashed) == native, "forward slashes are normalized first");
    const std::wstring dotted = deep + L"\\..\\" + std::wstring(segment);
    expect(adapters::native_path(dotted) == native, "dot segments are resolved first");
}

// base の後ろを 'a' で埋めて length 文字にする（ファイルは作らない）。
[[nodiscard]] std::wstring padded(const std::wstring &base, std::size_t length)
{
    return base + std::wstring(length - base.size(), L'a');
}

// 境界は絶対にした長さの MAX_PATH − 12。相対の経路は絶対にした後の長さで決まる。
void verify_prefix_boundary()
{
    const std::wstring base = absolute(root_name) + L"\\";
    const std::wstring below = padded(base, prefixed_length - 1);
    expect(adapters::native_path(below) == below, "an absolute path of 247 is not prefixed");
    const std::wstring at = padded(base, prefixed_length);
    expect(adapters::native_path(at) == L"\\\\?\\" + at, "an absolute path of 248 is prefixed");
    const std::wstring stem = std::wstring(root_name) + L"\\";
    const std::wstring relative = padded(stem, stem.size() + prefixed_length + 2 - base.size());
    const std::wstring full = absolute(relative.c_str());
    expect(relative.size() < prefixed_length && full.size() > prefixed_length,
           "the relative fixture is short but its absolute form is long");
    expect(adapters::native_path(relative) == L"\\\\?\\" + full,
           "a short relative path that becomes long is prefixed");
}

// UNC は `\\server\share\...` → `\\?\UNC\server\share\...`。文字列の形だけで共有には触れない。
void verify_unc_form()
{
    const std::wstring tail = L"server\\share\\";
    const std::wstring shared = padded(L"\\\\" + tail, long_length);
    expect(adapters::native_path(shared) == L"\\\\?\\UNC\\" + shared.substr(2),
           "a long UNC path gets the verbatim UNC prefix");
}

// 長い経路のフォルダを読むと、短い経路と同じく unreadable（directory() が長い経路を見る）。
void verify_folder_read(adapters::Win32FileAdapter &files, const std::wstring &deep)
{
    const auto folder = files.read(path_of(deep), read_limit);
    expect(!folder && folder.error() == FileFailure::unreadable,
           "reading a long folder is unreadable");
}

// 反例: `\\?\` を付けない長い経路は、longPathAware でない exe では作れない。
void verify_unprefixed_fails(const std::wstring &deep)
{
    const std::wstring plain = deep + L"\\plain.txt";
    expect(plain.size() > long_length, "the fixture path is longer than 300 characters");
    const HANDLE file = CreateFileW(plain.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file == INVALID_HANDLE_VALUE, "an unprefixed long path is refused by Win32");
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
}

void verify_round_trip(adapters::Win32FileAdapter &files, const std::wstring &deep)
{
    const std::wstring target = deep + L"\\note.txt";
    const auto path = path_of(target);
    expect(path.text().size() > long_length, "the stored path keeps its plain long form");
    expect(files.write(path, "first").has_value(), "a long path is written");
    expect(read_text(files, target) == "first", "a long path is read back");
    expect(files.write(path, "second").has_value(), "a long path is replaced through a temporary");
    expect(read_text(files, target) == "second", "the replaced bytes are read back");
    const auto again = files.write(path, "third", FileWriteMode::create_new);
    expect(!again && again.error() == FileFailure::already_exists,
           "create_new on a long existing path is already_exists");
    WIN32_FIND_DATAW data{};
    const adapters::FindHandle left(
        FindFirstFileW(adapters::native_path(target + L".nib-tmp-*").c_str(), &data));
    expect(!left.valid(), "no temporary is left beside the long path");
}

// 前の保存の残骸（自分の古い連番）は、長い経路でも保存の直前に回収される（ADR 0135 の決定 3）。
void verify_reclaim(adapters::Win32FileAdapter &files, const std::wstring &deep)
{
    const std::wstring target = deep + L"\\note.txt";
    const std::wstring stale =
        adapters::temporary_name(target, adapters::issued_temporary_sequence());
    const HANDLE file = CreateFileW(adapters::native_path(stale).c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a stale temporary is planted on the long path");
    CloseHandle(file);
    expect(files.write(path_of(target), "fourth").has_value(), "the long path is saved again");
    expect(!exists(stale), "the stale temporary on the long path is reclaimed");
}

[[nodiscard]] std::vector<FolderBatch> until_final(adapters::Win32FolderAdapter &folders,
                                                   Arrivals &arrived)
{
    std::vector<FolderBatch> seen;
    bool done = false;
    while (!done)
    {
        arrived.acquire();
        for (auto &batch : folders.collect())
        {
            done = done || batch.progress != FolderProgress::more;
            seen.push_back(std::move(batch));
        }
    }
    return seen;
}

void verify_listing(const std::wstring &deep)
{
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32FolderAdapter folders(worker);
    folders.bind([&arrived] { arrived.release(); });
    folders.list(FolderRequest{path_of(deep), 1});
    const auto batches = until_final(folders, arrived);
    std::size_t listed = 0;
    bool complete = false;
    for (const auto &batch : batches)
    {
        complete = complete || batch.progress == FolderProgress::complete;
        for (const auto &file : batch.files)
        {
            listed += file.file_name() == "note.txt" ? 1U : 0U;
        }
    }
    expect(complete, "the long folder is listed to the end");
    expect(listed == 1, "the file in the long folder is listed once");
}

// 失敗の値は短い経路と同じ（読み取り専用・無い親フォルダ・無いファイル）。
void verify_failures(adapters::Win32FileAdapter &files, const std::wstring &deep)
{
    const std::wstring shallow = absolute(short_name);
    for (const std::wstring &folder : {shallow, deep})
    {
        const std::wstring target = folder + L"\\locked.txt";
        expect(files.write(path_of(target), "x").has_value(), "a file to lock is written");
        expect(SetFileAttributesW(adapters::native_path(target).c_str(), FILE_ATTRIBUTE_READONLY) !=
                   0,
               "the file is made read-only");
    }
    const auto short_locked = files.write(path_of(shallow + L"\\locked.txt"), "y");
    const auto long_locked = files.write(path_of(deep + L"\\locked.txt"), "y");
    expect(!short_locked && !long_locked && short_locked.error() == long_locked.error(),
           "a read-only long path fails like a short one");
    const auto short_orphan = files.write(path_of(shallow + L"\\absent\\x.txt"), "y");
    const auto long_orphan = files.write(path_of(deep + L"\\absent\\x.txt"), "y");
    expect(!short_orphan && !long_orphan && short_orphan.error() == long_orphan.error(),
           "a missing parent folder fails like a short one");
    const auto missing = files.read(path_of(deep + L"\\absent.txt"), read_limit);
    expect(!missing && missing.error() == FileFailure::not_found,
           "an absent long path is not_found");
}
int run_tests()
{
    const std::wstring root = absolute(root_name);
    const std::wstring shallow = absolute(short_name);
    remove_tree(root);
    remove_tree(shallow);
    expect(CreateDirectoryW(root.c_str(), nullptr) != 0, "the temporary folder is created");
    expect(CreateDirectoryW(shallow.c_str(), nullptr) != 0, "the short folder is created");
    const std::wstring deep = deep_folder(root);
    adapters::Win32FileAdapter files;
    verify_native_form(deep);
    verify_prefix_boundary();
    verify_unc_form();
    verify_folder_read(files, deep);
    verify_unprefixed_fails(deep);
    verify_round_trip(files, deep);
    verify_reclaim(files, deep);
    verify_listing(deep);
    verify_failures(files, deep);
    remove_tree(root);
    remove_tree(shallow);
    expect(!exists(deep) && !exists(root), "the long folders are removed");
    std::printf("Long path adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"long-path");
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
