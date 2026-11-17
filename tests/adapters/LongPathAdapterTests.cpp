// Issue #498: MAX_PATH を超える経路のファイルを、本物のフォルダで書く → 読む → 一時ファイル経由で
// 置き換える → 列挙する → 消す。経路の正規化は adapters の native_path の 1 本。
// この試験の exe は manifest に longPathAware を持たないので、`\\?\` を付けない長い経路は
// レジストリの LongPathsEnabled に依らず落ちる（反例）。製品の窓の exe は manifest でも宣言する。
// Issue #1805: 予約名（NUL・CON・AUX・PRN・COMn・LPTn）に拡張子を付けた名前のファイルも、同じ
// native_path の道で普通のファイルとして書いて読む。Windows 10 / Server 2022（CI の runner）の
// GetFullPathNameW は `nul.txt` を `\\.\nul` に写すので、直す前の製品ではそこで落ちる。Windows 11
// は拡張子つきを写さないので、手元では直す前でも通る。拡張子の無い `nul` は装置のまま。
// 一時フォルダは
// temporary_test_directory（#1103）の中に固定の名前で作り、冒頭で前回の残りを消して終わりに消す。
// 時刻は読まない。待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest
// の制限時間で落ちる。
#include "AbsolutePath.hpp"
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
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
    expect(adapters::native_path(shallow) == L"\\\\?\\" + shallow,
           "a short path also gets the verbatim prefix");
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

// 長さの境界は無い（#1805 で常に付ける）。相対の経路は絶対にした後の形に付く。
void verify_prefix_boundary()
{
    const std::wstring base = absolute(root_name) + L"\\";
    const std::wstring below = padded(base, prefixed_length - 1);
    expect(adapters::native_path(below) == L"\\\\?\\" + below,
           "an absolute path of 247 is prefixed");
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

// 前の保存の残骸（自分の古い連番）は、長い経路でも成功した保存の後に回収される（#1689）。
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

// 書いた先がディスクの上の普通のファイルなら、その大きさ。経路は試験が手で `\\?\` を付けた形で、
// native_path に頼らずに確かめる（装置へ書いて「成功」した場合はここで無い）。
[[nodiscard]] std::optional<std::uint64_t> size_on_disk(const std::wstring &path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW((L"\\\\?\\" + path).c_str(), GetFileExInfoStandard, &data) == 0 ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return std::nullopt;
    }
    return (std::uint64_t{data.nFileSizeHigh} << 32U) | data.nFileSizeLow;
}

constexpr std::array<std::wstring_view, 8> reserved_names{
    L"nul.txt", L"con.md", L"aux.c", L"prn.txt", L"com1.txt", L"lpt1.txt", L"NuL.TxT", L"CON.Md",
};

// 書く → ディスクに在って大きさが同じ → 読む → 同じバイト列。2 回目は一時ファイル経由の置き換え。
void verify_reserved_round_trip(adapters::Win32FileAdapter &files, const std::wstring &folder)
{
    for (const std::wstring_view name : reserved_names)
    {
        const std::wstring target = folder + L"\\" + std::wstring(name);
        const std::string bytes = "reserved:" + core::to_utf8(name).value();
        expect(files.write(path_of(target), bytes).has_value(), "a reserved name is written");
        const auto size = size_on_disk(target);
        expect(size.has_value() && size.value() == bytes.size(),
               "a reserved name is a file of the written size on disk");
        expect(read_text(files, target) == bytes, "a reserved name reads back the same bytes");
        expect(files.write(path_of(target), bytes + "!").has_value(),
               "a reserved name is replaced through a temporary");
        expect(read_text(files, target) == bytes + "!", "the replaced reserved name reads back");
    }
    WIN32_FIND_DATAW data{};
    const adapters::FindHandle left(
        FindFirstFileW(adapters::native_path(folder + L"\\*.nib-tmp-*").c_str(), &data));
    expect(!left.valid(), "no temporary is left beside the reserved names");
}

// OS は最後の段の末尾の空白とドットを落とす。予約名でも落とした名前の普通のファイルになる。
void verify_reserved_trailing(adapters::Win32FileAdapter &files, const std::wstring &folder)
{
    expect(files.write(path_of(folder + L"\\aux.c. "), "trimmed").has_value(),
           "a reserved name with trailing dot and space is written");
    const auto size = size_on_disk(folder + L"\\aux.c");
    expect(size.has_value() && size.value() == 7, "the trailing dot and space are dropped on disk");
    expect(read_text(files, folder + L"\\aux.c") == "trimmed", "the trimmed name reads back");
}

// 予約名の形: 拡張子つきは装置に写さず、フォルダの絶対の形の後ろに最後の段を付ける（どの
// Windows でも）。装置の名前そのもの（`NUL` `nul. `）は Windows 11 と同じく装置のまま。
void verify_reserved_forms(const std::wstring &folder)
{
    const std::wstring current = absolute(L".");
    expect(adapters::full_path(folder + L"/nul.txt") == folder + L"\\nul.txt",
           "a slashed reserved name is a file in its folder");
    expect(adapters::full_path(L"nul.txt") == current + L"\\nul.txt",
           "a relative reserved name is a file in the current folder");
    expect(adapters::full_path(current.substr(0, 2) + L"con.md") == current + L"\\con.md",
           "a drive relative reserved name is a file in the drive's current folder");
    expect(adapters::full_path(L"\\\\server\\share\\aux.c") == L"\\\\server\\share\\aux.c",
           "a UNC reserved name is a file on the share");
    expect(adapters::full_path(folder + L"\\prn.txt. ") == folder + L"\\prn.txt",
           "a reserved name drops its trailing dot and space");
    expect(adapters::native_path(folder + L"\\con.md") == L"\\\\?\\" + folder + L"\\con.md",
           "a reserved name is passed to Win32 in the verbatim form");
    for (const std::wstring &bare : {folder + L"\\NUL", folder + L"\\nul. ", std::wstring(L"nul")})
    {
        expect(adapters::full_path(bare).starts_with(L"\\\\.\\") &&
                   adapters::native_path(bare).starts_with(L"\\\\.\\"),
               "a bare device name stays the device as on Windows 11");
    }
    expect(adapters::native_path(L"\\\\.\\nul") == L"\\\\.\\nul",
           "an explicit device path is kept");
    const auto opened = adapters::absolute_file_path(folder + L"\\nul.txt");
    expect(opened.has_value() &&
               opened.value().text() == core::to_utf8(folder + L"\\nul.txt").value(),
           "a launch argument with a reserved name keeps its plain file path");
}

// 予約名でない経路の形は変わらない: full_path は GetFullPathNameW の答えのまま、native_path は
// それに `\\?\`（UNC は `\\?\UNC\`）を付けた形。
void verify_plain_forms()
{
    const std::wstring drive = absolute(L".").substr(0, 2);
    const std::array<std::wstring, 12> inputs{
        L"plain.txt",
        L".",
        L"..",
        L".\\a\\..\\b.txt",
        L"a/b/c.txt",
        drive + L"foo.txt",
        L"\\\\server\\share\\dir\\x.txt",
        L"folder\\",
        L" lead and trail ",
        L"foo. .",
        drive + L"\\x\\..\\y\\",
        L"\\rooted.txt",
    };
    for (const std::wstring &input : inputs)
    {
        const std::wstring os = absolute(input.c_str());
        expect(adapters::full_path(input) == os, "a plain path resolves like GetFullPathNameW");
        const std::wstring native =
            os.starts_with(L"\\\\") ? L"\\\\?\\UNC\\" + os.substr(2) : L"\\\\?\\" + os;
        expect(adapters::native_path(input) == native, "a plain path gets the verbatim prefix");
    }
    const std::wstring verbatim = L"\\\\?\\" + drive + L"\\x\\..\\y";
    expect(adapters::native_path(verbatim) == verbatim, "a verbatim path is passed unchanged");
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
    verify_reserved_round_trip(files, shallow);
    verify_reserved_trailing(files, shallow);
    verify_reserved_forms(shallow);
    verify_plain_forms();
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
