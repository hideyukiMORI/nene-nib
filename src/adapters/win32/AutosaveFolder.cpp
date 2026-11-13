#include "AutosaveFolder.hpp"

#include "AutosaveCodec.hpp"
#include "AutosaveEntry.hpp"
#include "FindHandle.hpp"
#include "NativePath.hpp"
#include "ParentDirectory.hpp"
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
        static_cast<void>(DeleteFileW(native_path(widen(list.value().text())).c_str()));
        static_cast<void>(RemoveDirectoryW(native_path(widen(folder)).c_str()));
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

template <typename Number>
[[nodiscard]] std::optional<Number> number_of(std::string_view text, int base)
{
    Number value = 0;
    const auto *const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value, base);
    if (text.empty() || error != std::errc{} || stop != end)
    {
        return std::nullopt;
    }
    return value;
}

// 作る形の名前（今の形 `<pid>-<作成時刻>` か前の形 `<pid>`・どちらも小文字の 16 進）。
[[nodiscard]] std::string folder_text(const AutosaveFolderName &name)
{
    return name.created.has_value() ? std::format("{:x}-{:x}", name.pid, name.created.value())
                                    : std::format("{:x}", name.pid);
}

// 数を読んでから作る形に書き直して元と比べるので、大文字・先頭の 0・余分な字は通らない。
[[nodiscard]] std::optional<AutosaveFolderName> folder_name_of(std::string_view text)
{
    const std::size_t dash = text.find('-');
    const auto pid = number_of<std::uint32_t>(text.substr(0, dash), 16);
    if (!pid.has_value())
    {
        return std::nullopt;
    }
    AutosaveFolderName name{pid.value(), std::nullopt};
    if (dash != std::string_view::npos)
    {
        const auto created = number_of<std::uint64_t>(text.substr(dash + 1), 16);
        if (!created.has_value())
        {
            return std::nullopt;
        }
        name.created = created.value();
    }
    if (folder_text(name) != text)
    {
        return std::nullopt;
    }
    return name;
}

// ASCII だけの名前を狭い文字列に写す（作る形は ASCII だけ）。ほかの字があれば無し。
[[nodiscard]] std::optional<std::string> ascii_of(std::wstring_view wide)
{
    std::string text;
    for (const wchar_t letter : wide)
    {
        if (letter > L'\x7f')
        {
            return std::nullopt;
        }
        text.push_back(static_cast<char>(letter));
    }
    return text;
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
                                                        entry.id, entry.path, entry.title,
                                                        entry.caret});
    }
}

// 残骸のフォルダの名前（autosave_remnant_name が真のフォルダだけ）。
[[nodiscard]] std::optional<std::string>
remnant_name(const WIN32_FIND_DATAW &data, ProcessInstance self, ProcessCreatedOf created_of)
{
    const std::wstring_view name(static_cast<const wchar_t *>(data.cFileName));
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        !autosave_remnant_name(name, self, created_of))
    {
        return std::nullopt;
    }
    return ascii_of(name);
}

void delete_file_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data)
{
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        const std::wstring name(static_cast<const wchar_t *>(data.cFileName));
        static_cast<void>(DeleteFileW(native_path(folder + L"\\" + name).c_str()));
    }
}
// フォルダの直下のファイルを消す（一覧・本文・保存の一時ファイルの残り）。消せないものは残す。
void delete_files_in(const std::wstring &folder)
{
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW(native_path(folder + L"\\*").c_str(), FindExInfoBasic,
                                             &data, FindExSearchNameMatch, nullptr, 0));
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

std::string autosave_folder(const core::FilePath &root, ProcessInstance self)
{
    return std::format("{}/{}", root.text(), folder_text({self.pid, self.created}));
}

std::optional<AutosaveFolderName> autosave_folder_name_of(std::wstring_view name)
{
    const auto text = ascii_of(name);
    return text.has_value() ? folder_name_of(text.value()) : std::nullopt;
}

bool autosave_remnant_name(std::wstring_view name, ProcessInstance self,
                           ProcessCreatedOf created_of)
{
    const auto parsed = autosave_folder_name_of(name);
    if (!parsed.has_value())
    {
        return false;
    }
    const AutosaveFolderName &folder = parsed.value();
    if (folder.pid == self.pid)
    {
        return !folder.created.has_value() || folder.created.value() != self.created;
    }
    const auto created = created_of(folder.pid);
    if (!created.has_value())
    {
        return created.error() == ProcessQueryFailure::gone;
    }
    return folder.created.has_value() && folder.created.value() != created.value();
}

std::expected<void, Failure> autosave_stored(application::FilePort &files,
                                             const std::optional<std::string> &own,
                                             const application::AutosaveSnapshot &snapshot)
{
    if (!own.has_value())
    {
        return std::unexpected(Failure::location_unavailable);
    }
    const std::string &folder = own.value();
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
    static_cast<void>(DeleteFileW(native_path(widen(body.value().text())).c_str()));
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
    static_cast<void>(RemoveDirectoryW(native_path(wide).c_str()));
}

Remnants autosave_remnants_in(application::FilePort &files, const core::FilePath &root,
                              ProcessInstance self, ProcessCreatedOf created_of)
{
    Remnants remnants;
    const std::wstring wide = widen(root.text());
    if (wide.empty())
    {
        return remnants;
    }
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW(native_path(wide + L"\\*").c_str(), FindExInfoBasic,
                                             &data, FindExSearchNameMatch, nullptr, 0));
    if (!search.valid())
    {
        return remnants;
    }
    do
    {
        const auto name = remnant_name(data, self, created_of);
        if (name.has_value())
        {
            listed_remnants(files, std::format("{}/{}", root.text(), name.value()), name.value(),
                            remnants);
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
    const auto name = folder_name_of(location.substr(0, slash));
    const auto id = number_of<std::uint64_t>(location.substr(slash + 1), 10);
    if (!name.has_value() || !id.has_value() || id.value() != remnant.id.value ||
        std::format("{}/{}", folder_text(name.value()), id.value()) != location)
    {
        return std::nullopt;
    }
    return std::format("{}/{}", root.text(), folder_text(name.value()));
}
} // namespace nenenib::adapters::win32
