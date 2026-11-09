// Issue #1290: 自動退避の置き場（ADR 0137 の決定 4〜6）の一覧 `autosave.v1` と本文 `<番号>.body` の
// 2 ファイルの組に、固定 seed
// の決定的な生成器で傷（本文の欠け・切り詰め・入れ替え・古い改訂の本文・
// 古い改訂の一覧・壊れた一覧のバイト・一覧の欠け・余分なファイル・余分なフォルダ）を入れ、本物の
// フォルダの上で Win32AutosaveAdapter の remnants・read・forget を素朴な模型と照らす。模型は
// 「この試験が最後に各ファイルへ書いたバイト列」の写しで、次を確かめる。
//   1. remnants は、名前が作る形で生きていない pid のフォルダのうち、一覧が読めるものの行だけを
//      全部返す（location は `<pid>/<番号>`・パス・題名・キャレットは一覧の行）。一覧を壊していない
//      フォルダは書いた行そのもの、壊したフォルダは一覧の復号（--persisted-property が別に守る）。
//      自分の pid・生きている pid（4）・大文字・先頭の
//      0・フォルダでないもの・一覧の無いものは出ない。
//   2. read は、その番号の本文に最後に書いたバイト列をそのまま返し、本文が無ければ unreadable。
//      別の番号・別のフォルダの本文を返さない（模型との完全な一致で見る）。
//   3. forget を全部の残骸に流すと残骸は 0 になり、残骸の本文だけが消え、一覧に無い本文・余分な
//      ファイル・ほかのフォルダは残る。
// 置き場は temporary_test_directory（#1103）の中で、件ごとに作って消す。時刻は読まない。生成器は
// std::mt19937_64 の剰余だけを使い、分布の型は使わない。死んだ pid は 4 の倍数でない大きな数
// （Windows の pid は 4 の倍数・AutosaveAdapterTests の deadbeef と同じ手）。回数は Debug で
// 数秒で終わる値（forget は flush する原子的な書きを伴うので 8 件に 1 件だけ流す）。HDD の D:
// の置き場では書き込みが遅い（#1290 の報告）。 製品の 1
// 行を手元で壊して落ちることを確かめた（#1290・commit しない）。
#include "AutosaveCodec.hpp"
#include "AutosaveEntry.hpp"
#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "Column.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "FindHandle.hpp"
#include "LineNumber.hpp"
#include "TemporaryTestDirectory.hpp"
#include "TextPosition.hpp"
#include "Win32AutosaveAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::AutosaveFailure;
using nenenib::application::AutosaveId;
using nenenib::application::AutosaveRemnant;
using Random = std::mt19937_64;
using Entries = std::vector<adapters::AutosaveEntry>;
// 番号 → その本文のファイルに最後に書いたバイト列（ファイルが無ければ鍵が無い）。
using Bodies = std::map<std::uint64_t, std::string>;
// 比べる形: location・パス・題名・行・桁。
using Row = std::tuple<std::string, std::string, std::string, std::size_t, std::size_t>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t pair_cases = 16U * rounds;
constexpr std::uint64_t pair_seed = 12900U;
constexpr wchar_t base[] = L"nib-autosave-pair";
constexpr char root_text[] = "nib-autosave-pair/autosave";
constexpr wchar_t root_wide[] = L"nib-autosave-pair\\autosave";

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

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

// --- フォルダとファイル（模型の外の、本物のファイルを書く・消す道具） ---

void remove_tree(const std::wstring &folder);

void remove_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data)
{
    const std::wstring name(static_cast<const wchar_t *>(data.cFileName));
    if (name == L"." || name == L"..")
    {
        return;
    }
    const auto path = folder + L"\\" + name;
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

[[nodiscard]] std::wstring widened(std::string_view ascii)
{
    return {ascii.begin(), ascii.end()};
}

[[nodiscard]] std::wstring folder_path(std::string_view name)
{
    return std::wstring(root_wide) + L"\\" + widened(name);
}

[[nodiscard]] std::wstring file_path(std::string_view folder, std::string_view name)
{
    return folder_path(folder) + L"\\" + widened(name);
}

[[nodiscard]] bool exists(const std::wstring &path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void make_folder(std::string_view name)
{
    expect(CreateDirectoryW(folder_path(name).c_str(), nullptr) != 0, "a folder is made");
}

// 試験の見本は原子的な書きを通さず、そのまま書く（flush しない・速さのため）。
void write_raw(const std::wstring &path, std::string_view bytes)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a sample file is created");
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    expect(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) !=
                   0 &&
               written == bytes.size(),
           "a sample file is written whole");
    CloseHandle(file);
}

// --- 生成器 ---

// 4 の倍数でない大きな pid（生きているプロセスにならない）の小文字 16 進。
[[nodiscard]] std::uint32_t dead_pid(Random &random)
{
    return 0xC0000001U | (static_cast<std::uint32_t>(below(random, 0x0FFFFFFFU)) << 2U);
}

[[nodiscard]] std::string random_bytes(Random &random, std::size_t limit)
{
    std::string bytes;
    const std::size_t count = below(random, limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        bytes.push_back(static_cast<char>(static_cast<unsigned char>(below(random, 256U))));
    }
    return bytes;
}

[[nodiscard]] core::DisplayText title_of(std::string_view text)
{
    return core::DisplayText::parse(text).value();
}

[[nodiscard]] Entries entries_of(Random &random, std::size_t index)
{
    Entries entries;
    std::set<std::uint64_t> used;
    const std::size_t count = below(random, 5U);
    for (std::size_t at = 0; at < count; ++at)
    {
        const std::uint64_t id = below(random, 12U);
        if (!used.insert(id).second)
        {
            continue;
        }
        std::optional<core::FilePath> path;
        std::string title = "無題";
        if (below(random, 3U) != 0U)
        {
            title = std::format("r{}-{}.txt", index, id);
            path = core::FilePath::parse("C:/work/" + title).value();
        }
        entries.push_back(
            adapters::AutosaveEntry{AutosaveId{id}, std::move(path), title_of(title),
                                    core::TextPosition{core::LineNumber{1U + below(random, 50U)},
                                                       core::Column{1U + below(random, 80U)}}});
    }
    return entries;
}

// 生きていない pid のフォルダ 1 つの模型。一覧のバイト列（無ければ値なし）と、それを壊したか
// （壊していなければ一覧の行は entries そのもの）と、本文の写し。
struct DeadFolder
{
    std::string name;
    Entries entries;
    std::optional<std::string> list;
    bool list_damaged = false;
    Bodies bodies;
};

void write_folder(const DeadFolder &folder)
{
    make_folder(folder.name);
    if (folder.list.has_value())
    {
        write_raw(file_path(folder.name, "autosave.v1"), folder.list.value());
    }
    for (const auto &[id, bytes] : folder.bodies)
    {
        write_raw(file_path(folder.name, adapters::autosave_body_name(AutosaveId{id})), bytes);
    }
}

[[nodiscard]] std::uint64_t some_id(Random &random, const DeadFolder &folder)
{
    if (folder.bodies.empty())
    {
        return below(random, 12U);
    }
    auto at = folder.bodies.begin();
    std::advance(at, static_cast<std::ptrdiff_t>(below(random, folder.bodies.size())));
    return at->first;
}

// 傷の 1 つ（ファイルを書く前の模型の上で入れる）。
using Wound = void (*)(Random &, DeadFolder &);

void body_missing(Random &random, DeadFolder &folder)
{
    folder.bodies.erase(some_id(random, folder));
}

void body_truncated(Random &random, DeadFolder &folder)
{
    const std::uint64_t id = some_id(random, folder);
    if (folder.bodies.contains(id))
    {
        auto &bytes = folder.bodies.at(id);
        bytes.resize(below(random, bytes.size() + 1U));
    }
}

void bodies_swapped(Random &random, DeadFolder &folder)
{
    const std::uint64_t left = some_id(random, folder);
    const std::uint64_t right = some_id(random, folder);
    if (folder.bodies.contains(left) && folder.bodies.contains(right))
    {
        std::swap(folder.bodies.at(left), folder.bodies.at(right));
    }
}

// 古い改訂の本文（同じ番号に別のバイト列）か、一覧に無い番号の本文の残り。
void body_rewritten(Random &random, DeadFolder &folder)
{
    folder.bodies.insert_or_assign(some_id(random, folder) + (below(random, 2U) * 20U),
                                   random_bytes(random, 64U));
}

// 古い改訂の一覧（行を 1 つ落とし、キャレットが古い）。書き直した一覧は壊れていない。
void list_outdated(Random &random, DeadFolder &folder)
{
    if (!folder.entries.empty())
    {
        folder.entries.erase(folder.entries.begin() +
                             static_cast<std::ptrdiff_t>(below(random, folder.entries.size())));
    }
    for (auto &entry : folder.entries)
    {
        entry.caret = core::TextPosition{core::LineNumber{1U + below(random, 9U)},
                                         core::Column{1U + below(random, 9U)}};
    }
    folder.list = adapters::encode_autosave_entries(folder.entries).value();
    folder.list_damaged = false;
}

// 一覧のバイトを 1 つ変える・抜く・足す。
void list_byte_damaged(Random &random, DeadFolder &folder)
{
    if (!folder.list.has_value() || folder.list.value().empty())
    {
        return;
    }
    auto &bytes = folder.list.value();
    const std::size_t at = below(random, bytes.size());
    const char value = static_cast<char>(static_cast<unsigned char>(below(random, 256U)));
    const std::size_t how = below(random, 3U);
    if (how == 0U)
    {
        bytes.at(at) = value;
    }
    else if (how == 1U)
    {
        bytes.erase(at, 1U);
    }
    else
    {
        bytes.insert(at, 1U, value);
    }
    folder.list_damaged = true;
}

void list_truncated(Random &random, DeadFolder &folder)
{
    if (folder.list.has_value())
    {
        folder.list.value().resize(below(random, folder.list.value().size() + 1U));
        folder.list_damaged = true;
    }
}

void list_missing(Random &, DeadFolder &folder)
{
    folder.list.reset();
}

constexpr std::array<Wound, 8> wounds{{body_missing, body_truncated, bodies_swapped, body_rewritten,
                                       list_outdated, list_byte_damaged, list_truncated,
                                       list_missing}};

void damaged(Random &random, DeadFolder &folder)
{
    wounds.at(below(random, wounds.size()))(random, folder);
}

[[nodiscard]] DeadFolder dead_folder(Random &random, std::size_t index,
                                     std::set<std::string> &names)
{
    DeadFolder folder;
    do
    {
        folder.name = std::format("{:x}", dead_pid(random));
    } while (!names.insert(folder.name).second);
    folder.entries = entries_of(random, index);
    folder.list = adapters::encode_autosave_entries(folder.entries).value();
    for (const auto &entry : folder.entries)
    {
        folder.bodies.insert_or_assign(entry.id.value, random_bytes(random, 96U));
    }
    const std::size_t count = below(random, 5U);
    for (std::size_t at = 0; at < count; ++at)
    {
        damaged(random, folder);
    }
    return folder;
}

// 余分なファイル（一覧に無い名前）を生きていない pid のフォルダに足す。forget の後も残る。
[[nodiscard]] std::vector<std::wstring> extra_files(Random &random, const DeadFolder &folder)
{
    constexpr std::array<std::string_view, 5> names{
        {"notes.txt", "autosave.v1.nib-tmp-1-0", "7.body.nib-tmp-3-1", "x.body", "autosave.v2"}};
    std::vector<std::wstring> written;
    if (below(random, 2U) == 0U)
    {
        const auto path = file_path(folder.name, names.at(below(random, names.size())));
        write_raw(path, random_bytes(random, 32U));
        written.push_back(path);
    }
    return written;
}

// 残骸にならないフォルダ（自分・生きている System・大文字・先頭の 0・名前が 16 進でない）と、
// 名前は死んだ pid だがフォルダでないファイル。どれにも読める一覧と本文を置く。
[[nodiscard]] std::vector<std::wstring> foreign_folders(Random &random,
                                                        std::set<std::string> &names)
{
    const Entries entries{{AutosaveId{1}, std::nullopt, title_of("無題"),
                           core::TextPosition{core::LineNumber{1}, core::Column{1}}}};
    const std::string list = adapters::encode_autosave_entries(entries).value();
    const std::string upper = std::format("{:X}", dead_pid(random));
    const std::string zero = std::format("0{:x}", dead_pid(random));
    std::vector<std::wstring> kept;
    for (const std::string &name : {std::format("{:x}", GetCurrentProcessId()), std::string("4"),
                                    upper, zero, std::string("zz")})
    {
        if (below(random, 2U) == 0U || !names.insert(name).second)
        {
            continue;
        }
        make_folder(name);
        write_raw(file_path(name, "autosave.v1"), list);
        write_raw(file_path(name, "1.body"), "foreign");
        kept.push_back(file_path(name, "1.body"));
    }
    const std::string file = std::format("{:x}", dead_pid(random));
    if (names.insert(file).second)
    {
        write_raw(folder_path(file), list);
        kept.push_back(folder_path(file));
    }
    return kept;
}

// --- 模型と答えの照合 ---

[[nodiscard]] Row row_of(std::string location, const std::optional<core::FilePath> &path,
                         const core::DisplayText &title, core::TextPosition caret)
{
    return {std::move(location), path.has_value() ? std::string(path.value().text()) : "",
            std::string(title.text()), caret.line.value, caret.column.value};
}

// 模型が期待する残骸の行（壊した一覧は復号の答え・壊していない一覧は書いた行）。
[[nodiscard]] std::vector<Row> expected_rows(const std::vector<DeadFolder> &folders)
{
    std::vector<Row> rows;
    for (const DeadFolder &folder : folders)
    {
        if (!folder.list.has_value())
        {
            continue;
        }
        const Entries listed =
            folder.list_damaged
                ? adapters::decode_autosave_entries(folder.list.value()).value_or(Entries{})
                : folder.entries;
        for (const auto &entry : listed)
        {
            rows.push_back(row_of(std::format("{}/{}", folder.name, entry.id.value), entry.path,
                                  entry.title, entry.caret));
        }
    }
    std::ranges::sort(rows);
    return rows;
}

[[nodiscard]] std::vector<Row> actual_rows(const std::vector<AutosaveRemnant> &remnants)
{
    std::vector<Row> rows;
    for (const AutosaveRemnant &remnant : remnants)
    {
        rows.push_back(row_of(remnant.location, remnant.path, remnant.title, remnant.caret));
    }
    std::ranges::sort(rows);
    return rows;
}

[[nodiscard]] const DeadFolder *folder_named(const std::vector<DeadFolder> &folders,
                                             std::string_view location)
{
    const std::string_view name = location.substr(0, location.find('/'));
    const auto found = std::ranges::find_if(folders, [name](const DeadFolder &folder)
                                            { return folder.name == name; });
    return found == folders.end() ? nullptr : &*found;
}

// read の答えが模型の本文と同じか（本文が無ければ unreadable）。
[[nodiscard]] bool read_matches(adapters::Win32AutosaveAdapter &autosave,
                                const std::vector<DeadFolder> &folders,
                                const AutosaveRemnant &remnant)
{
    const DeadFolder *folder = folder_named(folders, remnant.location);
    if (folder == nullptr)
    {
        return false;
    }
    const auto read = autosave.read(remnant);
    const auto found = folder->bodies.find(remnant.id.value);
    if (found == folder->bodies.end())
    {
        return !read.has_value() && read.error() == AutosaveFailure::unreadable;
    }
    if (!read.has_value() || read.value().size() != found->second.size())
    {
        return false;
    }
    return std::ranges::equal(read.value(), found->second, [](std::byte left, char right)
                              { return left == static_cast<std::byte>(right); });
}

// forget を全部に流した後: 残骸は無く、残骸の本文は消え、ほかのファイルは残る。
[[nodiscard]] bool forgotten_cleanly(adapters::Win32AutosaveAdapter &autosave,
                                     const std::vector<DeadFolder> &folders,
                                     const std::vector<AutosaveRemnant> &remnants,
                                     const std::vector<std::wstring> &kept)
{
    std::set<std::pair<std::string, std::uint64_t>> forgotten;
    for (const AutosaveRemnant &remnant : remnants)
    {
        autosave.forget(remnant);
        forgotten.emplace(remnant.location.substr(0, remnant.location.find('/')), remnant.id.value);
    }
    const auto left = autosave.remnants();
    if (!left.has_value() || !left.value().empty())
    {
        return false;
    }
    const auto bodies_follow = [&forgotten](const DeadFolder &folder)
    {
        return std::ranges::all_of(
            folder.bodies,
            [&forgotten, &folder](const auto &body)
            {
                const bool gone = !exists(
                    file_path(folder.name, adapters::autosave_body_name(AutosaveId{body.first})));
                return gone == forgotten.contains({folder.name, body.first});
            });
    };
    return std::ranges::all_of(folders, bodies_follow) && std::ranges::all_of(kept, exists);
}

// 1 件ぶんを流して、最初に見つけた違いの説明を返す（無ければ空）。
[[nodiscard]] std::string_view case_violation(Random &random, std::size_t index,
                                              adapters::Win32AutosaveAdapter &autosave)
{
    remove_tree(base);
    CreateDirectoryW(base, nullptr);
    CreateDirectoryW(root_wide, nullptr);
    std::set<std::string> names;
    std::vector<DeadFolder> folders;
    std::vector<std::wstring> kept = foreign_folders(random, names);
    const std::size_t count = 1U + below(random, 3U);
    for (std::size_t at = 0; at < count; ++at)
    {
        folders.push_back(dead_folder(random, index, names));
        write_folder(folders.back());
        const auto extra = extra_files(random, folders.back());
        kept.insert(kept.end(), extra.begin(), extra.end());
    }
    const auto listed = autosave.remnants();
    if (!listed.has_value() || actual_rows(listed.value()) != expected_rows(folders))
    {
        return "remnants are the rows of the readable lists of dead folders only";
    }
    for (const AutosaveRemnant &remnant : listed.value())
    {
        if (!read_matches(autosave, folders, remnant))
        {
            return "read returns the last bytes written to that number's body or unreadable";
        }
    }
    if (index % 8U == 0U && !forgotten_cleanly(autosave, folders, listed.value(), kept))
    {
        return "forgetting every remnant removes just their bodies and leaves the rest";
    }
    return {};
}

void verify_known_case(adapters::Win32AutosaveAdapter &autosave)
{
    remove_tree(base);
    CreateDirectoryW(base, nullptr);
    CreateDirectoryW(root_wide, nullptr);
    DeadFolder folder;
    folder.name = "deadbeef";
    folder.entries = {{AutosaveId{3}, core::FilePath::parse("C:/work/a.txt").value(),
                       title_of("a.txt"), core::TextPosition{core::LineNumber{2}, core::Column{3}}},
                      {AutosaveId{5}, std::nullopt, title_of("無題"),
                       core::TextPosition{core::LineNumber{1}, core::Column{1}}}};
    folder.list = adapters::encode_autosave_entries(folder.entries).value();
    // 3 の本文は 5 の本文と入れ替わり、5 の本文は欠け、一覧に無い 9 の本文が残る。
    folder.bodies = {{3U, "five"}, {9U, "orphan"}};
    write_folder(folder);
    const auto listed = autosave.remnants();
    const std::vector<DeadFolder> folders{folder};
    expect(listed.has_value() && actual_rows(listed.value()) == expected_rows(folders) &&
               listed.value().size() == 2U,
           "the known pair lists its two rows and not the orphan body");
    for (const AutosaveRemnant &remnant : listed.value_or(std::vector<AutosaveRemnant>{}))
    {
        expect(read_matches(autosave, folders, remnant),
               "the known pair reads the swapped body and refuses the missing one");
    }
}

[[nodiscard]] std::size_t broken_cases(adapters::Win32AutosaveAdapter &autosave)
{
    Random random{pair_seed};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < pair_cases; ++index)
    {
        const auto violation = case_violation(random, index, autosave);
        if (!violation.empty())
        {
            ++broken;
            std::fprintf(stderr, "autosave pair case %zu (seed %llu): %.*s\n", index,
                         static_cast<unsigned long long>(pair_seed),
                         static_cast<int>(violation.size()), violation.data());
        }
    }
    return broken;
}

int run_tests()
{
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32AutosaveAdapter autosave(worker, files,
                                            core::FilePath::parse(root_text).value());
    verify_known_case(autosave);
    expect(broken_cases(autosave) == 0U,
           "the remnants, reads and forgets of damaged pairs follow the files as written");
    remove_tree(base);
    std::printf("Autosave pair property: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"autosave-pair");
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
