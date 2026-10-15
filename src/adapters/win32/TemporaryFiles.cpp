#include "TemporaryFiles.hpp"

#include "FindHandle.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nenenib::adapters::win32
{
namespace
{
constexpr std::wstring_view temporary_marker = L".nib-tmp-";

[[nodiscard]] std::wstring digits(std::uint32_t value, int base)
{
    std::array<char, 16> text{};
    const auto end = std::to_chars(text.data(), text.data() + text.size(), value, base).ptr;
    return std::wstring(text.data(), end);
}

// 払い出した連番。UI スレッドの保存だけが進めるが、Win32 の不可分な操作で読み書きする
// （`<atomic>` は CNF-009 で使わない）。
[[nodiscard]] LONG &issued_sequences() noexcept
{
    static LONG issued = 0;
    return issued;
}

[[nodiscard]] std::wstring folder_of(const std::wstring &path)
{
    return path.substr(0, path.find_last_of(L"/\\") + 1);
}

// 短い名前（8.3）でパターンに当たったものを消さないよう、返った長い名前を確かめる。
[[nodiscard]] bool starts_with_ignoring_case(std::wstring_view name, std::wstring_view prefix)
{
    // 長さ 0 の比較は CompareStringOrdinal が失敗で返すので、空の接頭辞はここで通す。
    return prefix.empty() ||
           (name.size() >= prefix.size() &&
            CompareStringOrdinal(name.data(), static_cast<int>(prefix.size()), prefix.data(),
                                 static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL);
}

[[nodiscard]] std::optional<std::uint32_t> digit_of(wchar_t letter, std::uint32_t base) noexcept
{
    if (letter >= L'0' && letter <= L'9')
    {
        return static_cast<std::uint32_t>(letter - L'0');
    }
    if (base == 16 && letter >= L'a' && letter <= L'f')
    {
        return static_cast<std::uint32_t>(letter - L'a') + 10U;
    }
    return std::nullopt;
}

// 作る名前（to_chars）と同じ形の数だけを読む。空・先頭の 0・範囲外の文字・32 bit 超えは無し。
[[nodiscard]] std::optional<std::uint32_t> number_of(std::wstring_view text,
                                                     std::uint32_t base) noexcept
{
    if (text.empty() || (text.size() > 1 && text.front() == L'0'))
    {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const wchar_t letter : text)
    {
        const auto digit = digit_of(letter, base);
        if (!digit.has_value())
        {
            return std::nullopt;
        }
        value = (value * base) + digit.value();
        if (value > UINT32_MAX)
        {
            return std::nullopt;
        }
    }
    return static_cast<std::uint32_t>(value);
}

// 名前から `.nib-tmp-` の後ろを取り出す。prefix が空なら最後の `.nib-tmp-` の後ろ。
[[nodiscard]] std::optional<std::wstring_view> suffix_of(std::wstring_view name,
                                                         std::wstring_view prefix)
{
    if (!prefix.empty())
    {
        return starts_with_ignoring_case(name, prefix)
                   ? std::optional<std::wstring_view>(name.substr(prefix.size()))
                   : std::nullopt;
    }
    const std::size_t marker = name.rfind(temporary_marker);
    if (marker == std::wstring_view::npos)
    {
        return std::nullopt;
    }
    return name.substr(marker + temporary_marker.size());
}

[[nodiscard]] bool reclaimable_name(std::wstring_view name, std::wstring_view prefix)
{
    const auto suffix = suffix_of(name, prefix);
    return suffix.has_value() && reclaimable_temporary(suffix.value(), GetCurrentProcessId(),
                                                       issued_temporary_sequence(), process_alive);
}

void reclaim_matching(const std::wstring &folder, std::wstring_view prefix,
                      const std::wstring &pattern)
{
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr, 0));
    if (!search.valid())
    {
        return;
    }
    do
    {
        const std::wstring_view name(static_cast<const wchar_t *>(data.cFileName));
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            reclaimable_name(name, prefix))
        {
            static_cast<void>(DeleteFileW((folder + std::wstring(name)).c_str()));
        }
    } while (FindNextFileW(search.get(), &data) != 0);
}
} // namespace

std::wstring temporary_name(const std::wstring &target, std::uint32_t sequence)
{
    return target + std::wstring(temporary_marker) + digits(GetCurrentProcessId(), 16) + L'-' +
           digits(sequence, 10);
}

std::uint32_t next_temporary_sequence() noexcept
{
    return static_cast<std::uint32_t>(InterlockedIncrement(&issued_sequences()));
}

std::uint32_t issued_temporary_sequence() noexcept
{
    return static_cast<std::uint32_t>(InterlockedCompareExchange(&issued_sequences(), 0, 0));
}

bool reclaimable_temporary(std::wstring_view suffix, std::uint32_t self, std::uint32_t issued,
                           ProcessAlive alive)
{
    const std::size_t dash = suffix.find(L'-');
    if (dash == std::wstring_view::npos)
    {
        return false;
    }
    const auto pid = number_of(suffix.substr(0, dash), 16);
    const auto sequence = number_of(suffix.substr(dash + 1), 10);
    if (!pid.has_value() || !sequence.has_value())
    {
        return false;
    }
    if (pid.value() == self)
    {
        return sequence.value() <= issued;
    }
    return !alive(pid.value());
}

// 無い pid（ERROR_INVALID_PARAMETER）か、開けて終わっているものだけが生きていない。拒否
// （ERROR_ACCESS_DENIED）とほかの失敗は消さない側に倒す。
bool process_alive(std::uint32_t pid) noexcept
{
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
    {
        return GetLastError() != ERROR_INVALID_PARAMETER;
    }
    DWORD code = 0;
    const BOOL read = GetExitCodeProcess(process, &code);
    static_cast<void>(CloseHandle(process));
    return read == 0 || code == STILL_ACTIVE;
}

std::optional<std::uint32_t> process_id_of(std::wstring_view text) noexcept
{
    return number_of(text, 16);
}

bool owned_temporary(const std::wstring &temporary, const std::wstring &target,
                     std::uint32_t sequence)
{
    return temporary == temporary_name(target, sequence);
}

void reclaim_temporaries_of(const std::wstring &target)
{
    const std::wstring folder = folder_of(target);
    const std::wstring prefix = target.substr(folder.size()) + std::wstring(temporary_marker);
    reclaim_matching(folder, prefix, target + std::wstring(temporary_marker) + L'*');
}

void reclaim_temporaries_beside(const core::FilePath &file)
{
    const auto wide = core::to_utf16(file.text());
    if (!wide)
    {
        return;
    }
    const std::wstring folder = folder_of(wide.value());
    reclaim_matching(folder, {}, folder + L'*' + std::wstring(temporary_marker) + L'*');
}
} // namespace nenenib::adapters::win32
