// 言語サーバーを探す 1 回（LSP 段 S4・#1945）。何も起こさない。
#include "ServerSearch.hpp"

#include "FileHandle.hpp"
#include "FilePath.hpp"
#include "FindHandle.hpp"
#include "FoundLanguageServer.hpp"
#include "KnownLanguageServers.hpp"
#include "LanguageServerProbe.hpp"
#include "ServerSearchPlaces.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
// package.json を読む上限（包みの説明の大きさに十分・巨大なファイルを読まない）。
constexpr std::int64_t manifest_limit = std::int64_t{1024} * 1024;
constexpr std::size_t utf8_bom_size = 3;

[[nodiscard]] bool absolute_folder(std::wstring_view folder) noexcept
{
    const bool drive = folder.size() >= 3 &&
                       ((folder.at(0) >= L'A' && folder.at(0) <= L'Z') ||
                        (folder.at(0) >= L'a' && folder.at(0) <= L'z')) &&
                       folder.at(1) == L':' && (folder.at(2) == L'\\' || folder.at(2) == L'/');
    const bool unc = folder.size() >= 3 && folder.at(0) == L'\\' && folder.at(1) == L'\\';
    return drive || unc;
}

[[nodiscard]] std::wstring trimmed(std::wstring_view text)
{
    std::wstring kept;
    for (const wchar_t letter : text)
    {
        if (letter != L'"')
        {
            kept.push_back(letter);
        }
    }
    while (!kept.empty() && (kept.back() == L' ' || kept.back() == L'\t'))
    {
        kept.pop_back();
    }
    std::size_t start = 0;
    while (start < kept.size() && (kept.at(start) == L' ' || kept.at(start) == L'\t'))
    {
        ++start;
    }
    return kept.substr(start);
}

// 環境変数の値（無ければ空）。
[[nodiscard]] std::wstring environment(const wchar_t *name)
{
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0)
    {
        return {};
    }
    std::wstring value(needed, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0 || written >= needed)
    {
        return {};
    }
    value.resize(written);
    return value;
}

// 在って、フォルダでないファイルか。
[[nodiscard]] bool plain_file(const std::wstring &path) noexcept
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

[[nodiscard]] std::wstring joined(const std::wstring &folder, std::wstring_view name)
{
    std::wstring path = folder;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
    {
        path.push_back(L'\\');
    }
    path.append(name);
    return path;
}

[[nodiscard]] std::wstring search_path_of(const std::vector<std::wstring> &folders)
{
    std::wstring path;
    for (const auto &folder : folders)
    {
        path += path.empty() ? L"" : L";";
        path += folder;
    }
    return path;
}

// folders を明示して `<program>.exe` を探す（作業フォルダは見ない）。
[[nodiscard]] std::optional<std::wstring> searched(const std::vector<std::wstring> &folders,
                                                   const std::wstring &program)
{
    if (folders.empty() || program.empty())
    {
        return std::nullopt;
    }
    const std::wstring path = search_path_of(folders);
    const DWORD needed = SearchPathW(path.c_str(), program.c_str(), L".exe", 0, nullptr, nullptr);
    if (needed == 0)
    {
        return std::nullopt;
    }
    std::wstring found(needed, L'\0');
    const DWORD length =
        SearchPathW(path.c_str(), program.c_str(), L".exe", needed, found.data(), nullptr);
    if (length == 0 || length >= needed)
    {
        return std::nullopt;
    }
    found.resize(length);
    return plain_file(found) ? std::optional{found} : std::nullopt;
}

// 包みの名前の 1 段（小文字・数字・`-` `.` `_`・`.` で始まらない）。
[[nodiscard]] bool package_segment(std::string_view segment) noexcept
{
    return !segment.empty() && segment.front() != '.' &&
           std::ranges::all_of(segment,
                               [](char letter)
                               {
                                   return (letter >= 'a' && letter <= 'z') ||
                                          (letter >= '0' && letter <= '9') || letter == '-' ||
                                          letter == '.' || letter == '_';
                               });
}

// 包みの名前の形（1 段か `@scope/name`）。`\` の区切りのフォルダの名前にする。
[[nodiscard]] std::optional<std::wstring> package_folder(std::string_view package)
{
    if (!package.starts_with('@'))
    {
        return package_segment(package)
                   ? std::optional{std::wstring(package.begin(), package.end())}
                   : std::nullopt;
    }
    const auto slash = package.find('/');
    if (slash == std::string_view::npos || !package_segment(package.substr(1, slash - 1)) ||
        !package_segment(package.substr(slash + 1)))
    {
        return std::nullopt;
    }
    std::wstring folder(package.begin(), package.end());
    folder.at(slash) = L'\\';
    return folder;
}

// 小さなファイルを全部読む（上限を超えれば値なし）。
[[nodiscard]] std::optional<std::string> small_file(const std::wstring &path)
{
    const FileHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    LARGE_INTEGER size{};
    if (!file.valid() || GetFileSizeEx(file.get(), &size) == 0 || size.QuadPart > manifest_limit)
    {
        return std::nullopt;
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!bytes.empty() && (ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()),
                                    &read, nullptr) == 0 ||
                           read != bytes.size()))
    {
        return std::nullopt;
    }
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.erase(0, utf8_bom_size);
    }
    return bytes;
}

[[nodiscard]] std::optional<core::FilePath> path_of(const std::wstring &wide)
{
    const auto utf8 = core::to_utf8(wide);
    if (!utf8.has_value())
    {
        return std::nullopt;
    }
    auto parsed = core::FilePath::parse(utf8.value());
    return parsed.has_value() ? std::optional{std::move(parsed).value()} : std::nullopt;
}

// npm の包みのスクリプト（root を順に見る）。
[[nodiscard]] std::optional<std::wstring> npm_script(const ServerSearchPlaces &places,
                                                     const application::LanguageServerProbe &probe)
{
    const auto folder = package_folder(probe.package);
    if (!folder.has_value())
    {
        return std::nullopt;
    }
    for (const auto &root : places.npm_roots)
    {
        const std::wstring package = joined(joined(root, L"node_modules"), folder.value());
        const auto manifest = small_file(joined(package, L"package.json"));
        if (!manifest.has_value())
        {
            continue;
        }
        const auto script = core::npm_bin_script(manifest.value(), probe.package, probe.program);
        const auto wide =
            script.has_value() ? core::to_utf16(script.value()).value_or(L"") : std::wstring{};
        std::wstring file = joined(package, wide);
        if (!wide.empty() && plain_file(file))
        {
            return file;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<application::FoundLanguageServer>
found_npm(const ServerSearchPlaces &places, const application::LanguageServerProbe &probe)
{
    const auto script = npm_script(places, probe);
    const auto node = script.has_value() ? searched(places.folders, L"node") : std::nullopt;
    if (!node.has_value())
    {
        return std::nullopt;
    }
    auto executable = path_of(node.value());
    auto file = path_of(script.value());
    if (!executable.has_value() || !file.has_value())
    {
        return std::nullopt;
    }
    return application::FoundLanguageServer{std::move(executable).value(), std::move(file)};
}

[[nodiscard]] std::optional<application::FoundLanguageServer>
found_program(const ServerSearchPlaces &places, const application::LanguageServerProbe &probe)
{
    const auto program = core::to_utf16(probe.program);
    const auto found =
        program.has_value() ? searched(places.folders, program.value()) : std::nullopt;
    auto executable = found.has_value() ? path_of(found.value()) : std::nullopt;
    if (!executable.has_value())
    {
        return std::nullopt;
    }
    return application::FoundLanguageServer{std::move(executable).value(), std::nullopt};
}

// folder の直下のフォルダ（`.` で始まる物を除く）の絶対パス。
[[nodiscard]] std::vector<std::wstring> subfolders(const std::wstring &folder)
{
    std::vector<std::wstring> found;
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileW(joined(folder, L"*").c_str(), &data));
    for (bool more = search.valid(); more; more = FindNextFileW(search.get(), &data) != 0)
    {
        const std::wstring_view name(static_cast<const wchar_t *>(data.cFileName));
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && !name.starts_with(L"."))
        {
            found.push_back(joined(folder, name));
        }
    }
    return found;
}

// 版のフォルダ year の下の `<種類>\VC\Tools\Llvm\x64\bin` の在る物。
void push_llvm(std::vector<std::wstring> &folders, const std::wstring &year)
{
    for (const auto &edition : subfolders(year))
    {
        std::wstring llvm = joined(edition, L"VC\\Tools\\Llvm\\x64\\bin");
        if (GetFileAttributesW(llvm.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            folders.push_back(std::move(llvm));
        }
    }
}

// base の `Microsoft Visual Studio\<版>\<種類>\VC\Tools\Llvm\x64\bin` の在る物（Visual Studio の
// LLVM）。base が絶対パスでなければ何もしない。
void push_visual_studio(std::vector<std::wstring> &folders, const std::wstring &base)
{
    if (!absolute_folder(base))
    {
        return;
    }
    for (const auto &year : subfolders(joined(base, L"Microsoft Visual Studio")))
    {
        push_llvm(folders, year);
    }
}

// PATH の値の start から始まる段の終わり（`"` の間の `;` は区切らない）。
[[nodiscard]] std::size_t entry_end(const std::wstring &value, std::size_t start) noexcept
{
    std::size_t end = start;
    bool quoted = false;
    while (end < value.size() && (quoted || value.at(end) != L';'))
    {
        quoted = value.at(end) == L'"' ? !quoted : quoted;
        ++end;
    }
    return end;
}

void push_beneath(std::vector<std::wstring> &folders, const std::wstring &base,
                  std::wstring_view rest)
{
    if (absolute_folder(base))
    {
        folders.push_back(joined(base, rest));
    }
}
} // namespace

std::vector<std::wstring> absolute_path_folders(const std::wstring &value)
{
    std::vector<std::wstring> folders;
    for (std::size_t start = 0; start <= value.size();)
    {
        const std::size_t end = entry_end(value, start);
        const std::wstring folder = trimmed(std::wstring_view{value}.substr(start, end - start));
        if (absolute_folder(folder) && folder.find(L';') == std::wstring::npos)
        {
            folders.push_back(folder);
        }
        start = end + 1;
    }
    return folders;
}

std::vector<std::optional<application::FoundLanguageServer>>
find_language_servers(const std::vector<application::LanguageServerProbe> &probes,
                      const ServerSearchPlaces &places)
{
    std::vector<std::optional<application::FoundLanguageServer>> found;
    found.reserve(probes.size());
    for (const auto &probe : probes)
    {
        found.push_back(probe.package.empty() ? found_program(places, probe)
                                              : found_npm(places, probe));
    }
    return found;
}

ServerSearchPlaces system_search_places()
{
    ServerSearchPlaces places;
    const auto path = absolute_path_folders(environment(L"PATH"));
    places.folders = path;
    const std::wstring profile = environment(L"USERPROFILE");
    push_beneath(places.folders, profile, L".cargo\\bin");
    push_beneath(places.folders, profile, L"go\\bin");
    push_beneath(places.folders, profile, L".dotnet\\tools");
    const std::wstring programs = environment(L"ProgramFiles");
    push_beneath(places.folders, programs, L"LLVM\\bin");
    push_visual_studio(places.folders, programs);
    push_visual_studio(places.folders, environment(L"ProgramFiles(x86)"));
    places.npm_roots = path;
    push_beneath(places.npm_roots, environment(L"APPDATA"), L"npm");
    return places;
}
} // namespace nenenib::adapters::win32
