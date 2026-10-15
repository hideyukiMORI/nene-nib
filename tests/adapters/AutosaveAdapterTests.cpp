// Issue #515: 自動退避の置き場（Win32AutosaveAdapter）を、本物のフォルダとワーカー 1 本で測る
// （ADR 0137 の決定 4〜6・8）。置き場はビルドディレクトリの直下の固定の名前のフォルダで、冒頭で
// 前回の残りを消して終わりに消す。時刻は読まない（Sleep・時計・タイムアウトの待ちを書かない）。
// 待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest の制限時間で落ちる。
#include "AutosaveCodec.hpp"
#include "AutosaveEntry.hpp"
#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "FindHandle.hpp"
#include "TextPosition.hpp"
#include "Win32AutosaveAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <format>
#include <optional>
#include <semaphore>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::AutosaveFailure;
using nenenib::application::AutosaveId;
using nenenib::application::AutosaveRemnant;
using nenenib::application::AutosaveSnapshot;
using Signals = std::counting_semaphore<>;

constexpr wchar_t base[] = L"nib-autosave-files";
constexpr char root_text[] = "nib-autosave-files/autosave";
constexpr wchar_t root[] = L"nib-autosave-files/autosave";

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

bool exists(const std::wstring &path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring own_hex()
{
    return std::format(L"{:x}", GetCurrentProcessId());
}

std::wstring own_folder()
{
    return std::wstring(root) + L"/" + own_hex();
}

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

std::optional<core::FilePath> root_path(std::string_view text)
{
    return path_of(text);
}

std::string_view text_of(const std::optional<core::FilePath> &path)
{
    return path.has_value() ? path.value().text() : std::string_view{};
}

core::DisplayText title_of(std::string_view text)
{
    return core::DisplayText::parse(text).value();
}

core::TextPosition caret_at(std::size_t line, std::size_t column)
{
    return core::TextPosition{core::LineNumber{line}, core::Column{column}};
}

std::vector<std::byte> bytes_of(std::string_view text)
{
    const auto view = std::as_bytes(std::span{text});
    return {view.begin(), view.end()};
}

AutosaveSnapshot snapshot(std::uint64_t id, std::optional<core::FilePath> path,
                          std::string_view title, std::string_view text)
{
    return AutosaveSnapshot{AutosaveId{id}, std::move(path), title_of(title), bytes_of(text),
                            caret_at(2, 5)};
}

// ワーカーは載せた順に 1 つずつ走るので、後ろに載せた合図が来れば前の仕事は終わっている。
void drain(adapters::Win32Worker &worker)
{
    Signals done{0};
    worker.post([&done](const std::stop_token &) { done.release(); });
    done.acquire();
}

std::string read_text(adapters::Win32FileAdapter &files, std::string_view path)
{
    return files.read(path_of(path), 1024U * 1024U).value_or(std::string{});
}

std::vector<adapters::AutosaveEntry> own_entries(adapters::Win32FileAdapter &files)
{
    const auto list = std::format("{}/{:x}/autosave.v1", root_text, GetCurrentProcessId());
    return adapters::decode_autosave_entries(read_text(files, list))
        .value_or(std::vector<adapters::AutosaveEntry>{});
}

std::string own_body(adapters::Win32FileAdapter &files, std::uint64_t id)
{
    return read_text(files, std::format("{}/{:x}/{}.body", root_text, GetCurrentProcessId(), id));
}

// (a) store は本文と一覧を書く。同じ番号をもう 1 回 store すると本文を置き換え、一覧は 1 行のまま。
void verify_store(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files,
                  adapters::Win32AutosaveAdapter &autosave)
{
    autosave.store(snapshot(3, path_of("C:/work/a.txt"), "a.txt", "first\r\n"));
    autosave.store(snapshot(4, std::nullopt, "無題", "untitled"));
    drain(worker);
    expect(own_body(files, 3) == "first\r\n", "store writes the body bytes as given");
    expect(own_body(files, 4) == "untitled", "store writes an untitled body too");
    auto entries = own_entries(files);
    expect(entries.size() == 2, "the list has one entry per stored tab");
    expect(entries.size() == 2 && entries[0].id == AutosaveId{3} &&
               text_of(entries[0].path) == "C:/work/a.txt" && entries[0].title.text() == "a.txt" &&
               entries[0].caret == caret_at(2, 5),
           "the entry keeps the id, path, title and caret");
    expect(entries.size() == 2 && !entries[1].path.has_value() && entries[1].title.text() == "無題",
           "an untitled entry has no path");
    autosave.store(snapshot(3, path_of("C:/work/a.txt"), "a.txt", "second"));
    drain(worker);
    expect(own_body(files, 3) == "second", "a later store replaces the body");
    expect(own_entries(files).size() == 2, "a later store of the same id keeps one entry");
    expect(autosave.collect().empty(), "successful work leaves no failure");
}

// (b) discard は本文を消して一覧から外す。
void verify_discard(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files,
                    adapters::Win32AutosaveAdapter &autosave)
{
    autosave.discard(AutosaveId{4});
    drain(worker);
    expect(!exists(own_folder() + L"/4.body"), "discard removes the body");
    const auto entries = own_entries(files);
    expect(entries.size() == 1 && entries[0].id == AutosaveId{3},
           "discard removes the entry and keeps the others");
    autosave.discard(AutosaveId{99});
    drain(worker);
    expect(own_entries(files).size() == 1 && autosave.collect().empty(),
           "discarding an unknown id changes nothing and does not fail");
}

// (c) 置き場を作れなければ、失敗が 1 件たまって合図が 1 回だけ来る。
void verify_failure(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files)
{
    adapters::Win32AutosaveAdapter broken(worker, files,
                                          root_path("nib-autosave-files/missing/deeper/autosave"));
    Signals failed{0};
    std::size_t signals = 0;
    broken.bind(
        [&failed, &signals]
        {
            ++signals;
            failed.release();
        });
    broken.store(snapshot(1, std::nullopt, "無題", "lost"));
    failed.acquire();
    broken.store(snapshot(2, std::nullopt, "無題", "lost"));
    drain(worker);
    const auto first = broken.collect();
    expect(first.size() == 2 && first[0] == AutosaveFailure::location_unavailable,
           "a store into a folder that cannot be made fails as location_unavailable");
    expect(signals == 1, "the signal comes once while failures are waiting");
    expect(broken.collect().empty(), "collect empties the failures");
    adapters::Win32AutosaveAdapter nowhere(worker, files, std::nullopt);
    nowhere.store(snapshot(1, std::nullopt, "無題", "lost"));
    drain(worker);
    const auto second = nowhere.collect();
    expect(second.size() == 1 && second[0] == AutosaveFailure::location_unavailable,
           "without a root a store fails as location_unavailable");
}

// (d) clear は走っている store を待ってから自分のフォルダを消す。以後の仕事は受けない。
void verify_clear(adapters::Win32Worker &worker, adapters::Win32AutosaveAdapter &autosave)
{
    autosave.store(snapshot(5, std::nullopt, "無題", "in flight"));
    autosave.clear();
    expect(!exists(own_folder()), "clear removes the own folder after the work in flight");
    autosave.store(snapshot(6, std::nullopt, "無題", "too late"));
    drain(worker);
    expect(!exists(own_folder()), "a store after clear writes nothing");
    expect(autosave.collect().empty(), "a store after clear does not fail");
}

void write_list(adapters::Win32FileAdapter &files, std::string_view folder,
                const std::vector<adapters::AutosaveEntry> &entries)
{
    const auto bytes = adapters::encode_autosave_entries(entries);
    expect(bytes.has_value(), "the list encodes");
    expect(files
               .write(path_of(std::format("{}/{}/autosave.v1", root_text, folder)),
                      bytes.value_or(std::string{}))
               .has_value(),
           "a list is written for the start-up");
}

void write_body(adapters::Win32FileAdapter &files, std::string_view folder, std::uint64_t id,
                std::string_view text)
{
    expect(
        files.write(path_of(std::format("{}/{}/{}.body", root_text, folder, id)), text).has_value(),
        "a body is written for the start-up");
}

void make_folder(std::wstring_view name)
{
    const auto folder = std::wstring(root) + L"/" + std::wstring(name);
    expect(CreateDirectoryW(folder.c_str(), nullptr) != 0, "a remnant folder is made");
}

// 前回のプロセスの残骸。deadbeef は 4 の倍数でないので pid にならない（--files と同じ手）。
// System（pid 4）は生きている。大文字の 16 進と壊れた一覧は飛ばす。
void leave_remnants(adapters::Win32FileAdapter &files)
{
    const std::vector<adapters::AutosaveEntry> entries{
        {AutosaveId{7}, path_of("C:/work/b.txt"), title_of("b.txt"), caret_at(3, 5)},
        {AutosaveId{8}, std::nullopt, title_of("無題"), caret_at(1, 1)}};
    for (const std::wstring_view name : {L"deadbeef", L"4", L"DEADBEE1", L"deadbeed"})
    {
        make_folder(name);
    }
    write_list(files, "deadbeef", entries);
    write_body(files, "deadbeef", 7, "remnant seven");
    write_body(files, "deadbeef", 8, "remnant eight");
    write_list(files, "4", entries);
    write_list(files, "DEADBEE1", entries);
    write_body(files, "deadbeed", 1, "broken list");
    expect(files.write(path_of(std::format("{}/deadbeed/autosave.v1", root_text)), "version=9\n")
               .has_value(),
           "a broken list is written");
}

std::optional<AutosaveRemnant> remnant_with(std::span<const AutosaveRemnant> remnants,
                                            std::uint64_t id)
{
    for (const AutosaveRemnant &remnant : remnants)
    {
        if (remnant.id == AutosaveId{id})
        {
            return remnant;
        }
    }
    return std::nullopt;
}

// (e) 残骸は生きていない pid のフォルダの一覧だけ。自分と生きている pid は出ない。
void verify_remnants(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files,
                     adapters::Win32AutosaveAdapter &autosave)
{
    autosave.store(snapshot(9, std::nullopt, "無題", "mine"));
    drain(worker);
    leave_remnants(files);
    const auto remnants = autosave.remnants();
    expect(remnants.has_value(), "the remnants are listed");
    const auto listed = remnants.value_or(std::vector<AutosaveRemnant>{});
    expect(listed.size() == 2, "only the entries of the dead process are remnants");
    const auto seven = remnant_with(listed, 7);
    expect(seven.has_value() && seven.value().location == "deadbeef/7" &&
               text_of(seven.value().path) == "C:/work/b.txt" &&
               seven.value().title.text() == "b.txt",
           "a remnant carries its location, path and title");
    expect(seven.has_value() && seven.value().caret == caret_at(3, 5),
           "a remnant carries the caret written in the list");
    const auto eight = remnant_with(listed, 8);
    expect(eight.has_value() && !eight.value().path.has_value(), "an untitled remnant has no path");
}

std::string text_of(const std::vector<std::byte> &bytes)
{
    std::string text;
    for (const std::byte value : bytes)
    {
        text.push_back(static_cast<char>(value));
    }
    return text;
}

// (f) read は書いたバイト列を返す。forget は本文と行を消し、空になればフォルダも消す。
void verify_read_and_forget(adapters::Win32FileAdapter &files,
                            adapters::Win32AutosaveAdapter &autosave)
{
    const auto listed = autosave.remnants().value_or(std::vector<AutosaveRemnant>{});
    const auto seven = remnant_with(listed, 7);
    const auto eight = remnant_with(listed, 8);
    if (!seven.has_value() || !eight.has_value())
    {
        expect(false, "both remnants are there to read and forget");
        return;
    }
    const auto read = autosave.read(seven.value());
    expect(read.has_value() && text_of(read.value()) == "remnant seven",
           "read returns the bytes of the remnant");
    autosave.forget(seven.value());
    expect(!exists(std::wstring(root) + L"/deadbeef/7.body"), "forget removes the body");
    const auto left = autosave.remnants().value_or(std::vector<AutosaveRemnant>{});
    expect(left.size() == 1 && left[0].id == AutosaveId{8}, "forget removes only that entry");
    expect(!autosave.read(seven.value()).has_value(), "a forgotten remnant cannot be read");
    autosave.forget(eight.value());
    expect(!exists(std::wstring(root) + L"/deadbeef"),
           "forgetting the last entry removes the folder");
    const auto none = autosave.remnants();
    expect(none.has_value() && none.value().empty(), "no remnant is left");
    AutosaveRemnant stray = eight.value();
    stray.location = "../8";
    expect(!autosave.read(stray).has_value(), "a location outside the form is not read");
    expect(read_text(files, "nib-autosave-files/autosave/4/autosave.v1").size() > 0,
           "the folder of a live process is left alone");
}

void verify_autosave()
{
    remove_tree(base);
    expect(CreateDirectoryW(base, nullptr) != 0, "the temporary folder is made");
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    {
        adapters::Win32AutosaveAdapter autosave(worker, files, root_path(root_text));
        verify_store(worker, files, autosave);
        verify_discard(worker, files, autosave);
        verify_clear(worker, autosave);
    }
    verify_failure(worker, files);
    {
        adapters::Win32AutosaveAdapter autosave(worker, files, root_path(root_text));
        verify_remnants(worker, files, autosave);
        verify_read_and_forget(files, autosave);
        autosave.clear();
        expect(!exists(own_folder()), "clear removes the own folder again");
    }
    remove_tree(base);
}
} // namespace

int main(int argc, char **argv)
{
    const std::string_view scope = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && scope != "--autosave"))
    {
        std::fprintf(stderr, "Unknown autosave test scope\n");
        return 1;
    }
    verify_autosave();
    std::printf("Autosave adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
