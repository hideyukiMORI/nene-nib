#include "AutosaveFolder.hpp"

#include "AutosaveCodec.hpp"
#include "AutosaveEntry.hpp"
#include "FindHandle.hpp"
#include "ParentDirectory.hpp"
#include "TemporaryFiles.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <format>
#include <string_view>
#include <system_error>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::AutosaveFailure;
using Entries = std::vector<AutosaveEntry>;
using Remnants = std::vector<application::AutosaveRemnant>;

constexpr std::string_view list_name = "autosave.v1";

[[nodiscard]] std::wstring widen(std::string_view utf8)
{
    return core::to_utf16(utf8).value_or(std::wstring{});
}

[[nodiscard]] std::optional<core::FilePath> inside(const std::string &folder, std::string_view name)
{
    auto path = core::FilePath::parse(folder + "/" + std::string(name));
    if (!path)
    {
        return std::nullopt;
    }
    return std::move(path).value();
}

// 無い一覧は空。読めない・壊れている一覧は unreadable。
[[nodiscard]] std::expected<Entries, Failure> entries_in(application::FilePort &files,
                                                         const std::string &folder)
{
    const auto list = inside(folder, list_name);
    if (!list.has_value())
    {
        return std::unexpected(Failure::unreadable);
    }
    const auto bytes = files.read(list.value(), maximum_autosave_list_bytes);
    if (!bytes)
    {
        if (bytes.error() == application::FileFailure::not_found)
        {
            return Entries{};
        }
        return std::unexpected(Failure::unreadable);
    }
    return decode_autosave_entries(bytes.value().view());
}

// 空の一覧は書かず、一覧とフォルダを消す。
[[nodiscard]] std::expected<void, Failure>
entries_written(application::FilePort &files, const std::string &folder, const Entries &entries)
{
    const auto list = inside(folder, list_name);
    if (!list.has_value())
    {
        return std::unexpected(Failure::unwritable);
    }
    if (entries.empty())
    {
        static_cast<void>(DeleteFileW(widen(list.value().text()).c_str()));
        static_cast<void>(RemoveDirectoryW(widen(folder).c_str()));
        return {};
    }
    const auto bytes = encode_autosave_entries(entries);
    if (!bytes || !files.write(list.value(), bytes.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    return {};
}

// 設定の親・置き場・自分のフォルダの 3 段を、無ければ作る（ensure_parent_directory は 1 段ずつ）。
[[nodiscard]] bool folders_made(const std::string &folder)
{
    const std::wstring own = widen(folder);
    if (own.empty())
    {
        return false;
    }
    const std::wstring root = own.substr(0, own.find_last_of(L"/\\"));
    return ensure_parent_directory(root) && ensure_parent_directory(own) &&
           ensure_parent_directory(own + L"/" + std::wstring(list_name.begin(), list_name.end()));
}

// 本文の byte の列を書きの断片として借りる（写さない）。
[[nodiscard]] std::string_view bytes_view(const std::vector<std::byte> &bytes)
{
    return {std::bit_cast<const char *>(bytes.data()), bytes.size()};
}

void upsert(Entries &entries, AutosaveEntry entry)
{
    const auto found = std::ranges::find_if(entries, [&entry](const AutosaveEntry &kept)
                                            { return kept.id == entry.id; });
    if (found != entries.end())
    {
        *found = std::move(entry);
        return;
    }
    entries.push_back(std::move(entry));
}

[[nodiscard]] std::optional<std::uint64_t> decimal_of(std::string_view text)
{
    std::uint64_t value = 0;
    const auto *const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (text.empty() || error != std::errc{} || stop != end)
    {
        return std::nullopt;
    }
    return value;
}

void listed_remnants(application::FilePort &files, const std::string &folder, std::string_view name,
                     Remnants &remnants)
{
    const auto entries = entries_in(files, folder);
    if (!entries)
    {
        return;
    }
    for (const AutosaveEntry &entry : entries.value())
    {
        remnants.push_back(application::AutosaveRemnant{std::format("{}/{}", name, entry.id.value),
                                                        entry.id, entry.path, entry.title});
    }
}

// 名前が作る形の 16 進の pid で、自分でなく、生きていないフォルダ。
[[nodiscard]] std::optional<std::uint32_t> remnant_process(const WIN32_FIND_DATAW &data,
                                                           std::uint32_t self)
{
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return std::nullopt;
    }
    const auto pid = process_id_of(static_cast<const wchar_t *>(data.cFileName));
    if (!pid.has_value() || pid.value() == self || process_alive(pid.value()))
    {
        return std::nullopt;
    }
    return pid;
}

void delete_file_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data)
{
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        const std::wstring name(static_cast<const wchar_t *>(data.cFileName));
        static_cast<void>(DeleteFileW((folder + L"\\" + name).c_str()));
    }
}
// フォルダの直下のファイルを消す（一覧・本文・保存の一時ファイルの残り）。消せないものは残す。
void delete_files_in(const std::wstring &folder)
{
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW((folder + L"\\*").c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr, 0));
    if (!search.valid())
    {
        return;
    }
    do
    {
        delete_file_entry(folder, data);
    } while (FindNextFileW(search.get(), &data) != 0);
}
} // namespace

std::string autosave_folder(const core::FilePath &root, std::uint32_t pid)
{
    return std::format("{}/{:x}", root.text(), pid);
}

std::expected<void, Failure> autosave_stored(application::FilePort &files,
                                             const std::optional<core::FilePath> &root,
                                             const application::AutosaveSnapshot &snapshot)
{
    if (!root.has_value())
    {
        return std::unexpected(Failure::location_unavailable);
    }
    const std::string folder = autosave_folder(root.value(), GetCurrentProcessId());
    const auto body = inside(folder, autosave_body_name(snapshot.id));
    if (!body.has_value() || !folders_made(folder))
    {
        return std::unexpected(Failure::location_unavailable);
    }
    if (!files.write(body.value(), bytes_view(snapshot.bytes)))
    {
        return std::unexpected(Failure::unwritable);
    }
    // 自分の一覧が壊れていたら、読めた分を捨てて今の行から書き直す（自分のワーカーだけが書く）。
    Entries entries = entries_in(files, folder).value_or(Entries{});
    upsert(entries, AutosaveEntry{snapshot.id, snapshot.path, snapshot.title, snapshot.caret});
    return entries_written(files, folder, entries);
}

std::expected<void, Failure> autosave_dropped(application::FilePort &files,
                                              const std::string &folder, application::AutosaveId id)
{
    auto entries = entries_in(files, folder);
    const auto body = inside(folder, autosave_body_name(id));
    if (!entries || !body.has_value())
    {
        return std::unexpected(Failure::unwritable);
    }
    static_cast<void>(DeleteFileW(widen(body.value().text()).c_str()));
    const auto removed =
        std::erase_if(entries.value(), [id](const AutosaveEntry &entry) { return entry.id == id; });
    if (removed == 0)
    {
        return {};
    }
    return entries_written(files, folder, entries.value());
}

void remove_autosave_folder(const std::string &folder)
{
    const std::wstring wide = widen(folder);
    if (wide.empty())
    {
        return;
    }
    delete_files_in(wide);
    static_cast<void>(RemoveDirectoryW(wide.c_str()));
}

Remnants autosave_remnants_in(application::FilePort &files, const core::FilePath &root,
                              std::uint32_t self)
{
    Remnants remnants;
    const std::wstring wide = widen(root.text());
    if (wide.empty())
    {
        return remnants;
    }
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW((wide + L"\\*").c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr, 0));
    if (!search.valid())
    {
        return remnants;
    }
    do
    {
        const auto pid = remnant_process(data, self);
        if (pid.has_value())
        {
            const std::string name = std::format("{:x}", pid.value());
            listed_remnants(files, autosave_folder(root, pid.value()), name, remnants);
        }
    } while (FindNextFileW(search.get(), &data) != 0);
    return remnants;
}

std::optional<std::string> autosave_remnant_folder(const core::FilePath &root,
                                                   const application::AutosaveRemnant &remnant)
{
    const std::string_view location = remnant.location;
    const auto slash = location.find('/');
    if (slash == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::string_view hex = location.substr(0, slash);
    const auto pid = process_id_of(std::wstring(hex.begin(), hex.end()));
    const auto id = decimal_of(location.substr(slash + 1));
    if (!pid.has_value() || !id.has_value() || id.value() != remnant.id.value ||
        std::format("{:x}/{}", pid.value(), id.value()) != location)
    {
        return std::nullopt;
    }
    return autosave_folder(root, pid.value());
}
} // namespace nenenib::adapters::win32
