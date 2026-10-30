#include "Win32ShellRegistration.hpp"

#include "RegistryKey.hpp"
#include "ShellRegistration.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <shlobj.h>

#include <bit>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using application::ShellRegistrationFailure;
using application::ShellRegistrationStatus;

// 計画の key_path の先頭（ADR 0178 の決定 2・HKCU からの副鍵）。ここを構築子の根に置き換える。
constexpr std::wstring_view planned_root = L"Software";
// GetModuleFileNameW の最初の大きさと上限（長いパスの 32,767 文字まで）。
constexpr DWORD first_path_capacity = MAX_PATH;
constexpr DWORD last_path_capacity = 32768;

[[nodiscard]] ShellRegistrationFailure failure_of(LSTATUS status) noexcept
{
    return status == ERROR_ACCESS_DENIED ? ShellRegistrationFailure::access_denied
                                         : ShellRegistrationFailure::io_error;
}

// 計画の `Software\…` を根の副鍵へ写す。`Software` で始まらない鍵と不正な UTF-8 は書かない。
[[nodiscard]] std::expected<std::wstring, ShellRegistrationFailure>
rooted_key(std::wstring_view root, std::string_view key_path)
{
    const auto wide = core::to_utf16(key_path);
    if (!wide.has_value())
    {
        return std::unexpected(ShellRegistrationFailure::io_error);
    }
    const std::wstring_view path = wide.value();
    if (!path.starts_with(planned_root) ||
        (path.size() > planned_root.size() && path[planned_root.size()] != L'\\'))
    {
        return std::unexpected(ShellRegistrationFailure::io_error);
    }
    std::wstring rooted(root);
    rooted += path.substr(planned_root.size());
    return rooted;
}

// 値の名前。空は鍵の既定値（`(Default)`）で、API には nullptr を渡す。
[[nodiscard]] const wchar_t *value_name_of(const std::wstring &name) noexcept
{
    return name.empty() ? nullptr : name.c_str();
}

// 1 つの値を REG_SZ で書く（鍵が無ければ作る・既にある値は上書き）。
[[nodiscard]] std::expected<void, ShellRegistrationFailure>
written(std::wstring_view root, const core::ShellRegistrationEntry &entry)
{
    const auto key_path = rooted_key(root, entry.key_path);
    const auto name = core::to_utf16(entry.value_name);
    const auto value = core::to_utf16(entry.value);
    if (!key_path.has_value() || !name.has_value() || !value.has_value())
    {
        return std::unexpected(ShellRegistrationFailure::io_error);
    }
    RegistryKey key;
    const auto created =
        RegCreateKeyExW(HKEY_CURRENT_USER, key_path.value().c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, key.receive(), nullptr);
    if (created != ERROR_SUCCESS)
    {
        return std::unexpected(failure_of(created));
    }
    const auto &text = value.value();
    const auto bytes = static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t));
    const auto set = RegSetValueExW(key.get(), value_name_of(name.value()), 0, REG_SZ,
                                    std::bit_cast<const BYTE *>(text.c_str()), bytes);
    if (set != ERROR_SUCCESS)
    {
        return std::unexpected(failure_of(set));
    }
    return {};
}

// 共有の鍵の中の値だけを消す（removal が value・決定 2）。無い鍵と値は消えている。
[[nodiscard]] std::expected<void, ShellRegistrationFailure>
value_removed(std::wstring_view root, const core::ShellRegistrationEntry &entry)
{
    const auto key_path = rooted_key(root, entry.key_path);
    const auto name = core::to_utf16(entry.value_name);
    if (!key_path.has_value() || !name.has_value())
    {
        return std::unexpected(ShellRegistrationFailure::io_error);
    }
    RegistryKey key;
    const auto opened =
        RegOpenKeyExW(HKEY_CURRENT_USER, key_path.value().c_str(), 0, KEY_SET_VALUE, key.receive());
    if (opened == ERROR_FILE_NOT_FOUND)
    {
        return {};
    }
    if (opened != ERROR_SUCCESS)
    {
        return std::unexpected(failure_of(opened));
    }
    const auto deleted = RegDeleteValueW(key.get(), value_name_of(name.value()));
    if (deleted != ERROR_SUCCESS && deleted != ERROR_FILE_NOT_FOUND)
    {
        return std::unexpected(failure_of(deleted));
    }
    return {};
}

// Nib だけが持つ木を根ごと消す（removal が subtree・根は shell_registration_subtrees）。
[[nodiscard]] std::expected<void, ShellRegistrationFailure> tree_removed(std::wstring_view root,
                                                                         std::string_view subtree)
{
    const auto key_path = rooted_key(root, subtree);
    if (!key_path.has_value())
    {
        return std::unexpected(key_path.error());
    }
    const auto deleted = RegDeleteTreeW(HKEY_CURRENT_USER, key_path.value().c_str());
    if (deleted != ERROR_SUCCESS && deleted != ERROR_FILE_NOT_FOUND)
    {
        return std::unexpected(failure_of(deleted));
    }
    return {};
}

// 鍵の既定値（REG_SZ）を読む。無い・読めないときは失敗の値。
[[nodiscard]] std::expected<std::wstring, LSTATUS> default_value(const std::wstring &key_path)
{
    DWORD bytes = 0;
    const auto sized = RegGetValueW(HKEY_CURRENT_USER, key_path.c_str(), nullptr, RRF_RT_REG_SZ,
                                    nullptr, nullptr, &bytes);
    if (sized != ERROR_SUCCESS)
    {
        return std::unexpected(sized);
    }
    std::wstring text((bytes / sizeof(wchar_t)) + 1, L'\0');
    bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    const auto read = RegGetValueW(HKEY_CURRENT_USER, key_path.c_str(), nullptr, RRF_RT_REG_SZ,
                                   nullptr, text.data(), &bytes);
    if (read != ERROR_SUCCESS)
    {
        return std::unexpected(read);
    }
    text.resize(bytes / sizeof(wchar_t));
    while (!text.empty() && text.back() == L'\0')
    {
        text.pop_back();
    }
    return text;
}

// command の値（`"<exe>" "%1"`）の最初の引用の中の exe のパス。引用が無ければ値の全体。
[[nodiscard]] std::wstring_view quoted_executable(std::wstring_view command) noexcept
{
    if (!command.starts_with(L'"'))
    {
        return command;
    }
    const auto end = command.find(L'"', 1);
    return end == std::wstring_view::npos ? command.substr(1) : command.substr(1, end - 1);
}

// Windows のパスは大小を区別しないので、序数の大小無視で比べる。
[[nodiscard]] bool same_path(std::wstring_view left, std::wstring_view right) noexcept
{
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

// 右クリックの項目の command（計画で最初の `…\command` の既定値）。
[[nodiscard]] const core::ShellRegistrationEntry *
verb_command(const core::ShellRegistrationPlan &plan) noexcept
{
    for (const auto &entry : plan)
    {
        if (entry.value_name.empty() && std::string_view(entry.key_path).ends_with("\\command"))
        {
            return &entry;
        }
    }
    return nullptr;
}

// 関連付けが変わったことをエクスプローラーに知らせる（書いた後と消した後・決定 1）。
void notify_association_change() noexcept
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// 自分の exe の完全なパス（UTF-16）。足りなければ倍の大きさで読み直す。読めなければ空。
[[nodiscard]] std::wstring module_path()
{
    for (DWORD capacity = first_path_capacity; capacity <= last_path_capacity; capacity *= 2)
    {
        std::wstring path(capacity, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), capacity);
        if (length < capacity)
        {
            path.resize(length);
            return path;
        }
    }
    return {};
}
} // namespace

Win32ShellRegistration::Win32ShellRegistration(std::wstring root) : root_(std::move(root)) {}

std::expected<std::string, ShellRegistrationFailure> Win32ShellRegistration::executable_path()
{
    const auto path = module_path();
    const auto utf8 = core::to_utf8(path);
    if (path.empty() || !utf8.has_value())
    {
        return std::unexpected(ShellRegistrationFailure::io_error);
    }
    return utf8.value();
}

std::expected<void, ShellRegistrationFailure>
Win32ShellRegistration::apply(const core::ShellRegistrationPlan &plan)
{
    std::expected<void, ShellRegistrationFailure> result;
    for (const auto &entry : plan)
    {
        result = written(root_, entry);
        if (!result.has_value())
        {
            break;
        }
    }
    notify_association_change();
    return result;
}

std::expected<void, ShellRegistrationFailure>
Win32ShellRegistration::remove(const core::ShellRegistrationPlan &plan)
{
    // 途中で失敗しても残りは消し続け、最初の失敗を返す。
    std::expected<void, ShellRegistrationFailure> result;
    for (const auto &subtree : core::shell_registration_subtrees(plan))
    {
        const auto removed = tree_removed(root_, subtree);
        if (!removed.has_value() && result.has_value())
        {
            result = removed;
        }
    }
    for (const auto &entry : plan)
    {
        if (entry.removal != core::ShellRegistrationRemoval::value)
        {
            continue;
        }
        const auto removed = value_removed(root_, entry);
        if (!removed.has_value() && result.has_value())
        {
            result = removed;
        }
    }
    notify_association_change();
    return result;
}

bool Win32ShellRegistration::partly_registered(const core::ShellRegistrationPlan &plan) const
{
    for (const auto &subtree : core::shell_registration_subtrees(plan))
    {
        const auto key_path = rooted_key(root_, subtree);
        RegistryKey key;
        if (key_path.has_value() && RegOpenKeyExW(HKEY_CURRENT_USER, key_path.value().c_str(), 0,
                                                  KEY_READ, key.receive()) == ERROR_SUCCESS)
        {
            return true;
        }
    }
    for (const auto &entry : plan)
    {
        const auto key_path = rooted_key(root_, entry.key_path);
        const auto name = core::to_utf16(entry.value_name);
        if (entry.removal == core::ShellRegistrationRemoval::value && key_path.has_value() &&
            name.has_value() &&
            RegGetValueW(HKEY_CURRENT_USER, key_path.value().c_str(), value_name_of(name.value()),
                         RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
        {
            return true;
        }
    }
    return false;
}

ShellRegistrationStatus Win32ShellRegistration::status(const core::ShellRegistrationPlan &plan)
{
    // 右クリックの項目の command が無いときは、ほかの群が 1 つでも残っていれば途中の登録として
    // registered_elsewhere を返し、「登録を外す」で消せるようにする（5 群がどれも無いときだけ
    // absent）。
    const auto missing = partly_registered(plan) ? ShellRegistrationStatus::registered_elsewhere
                                                 : ShellRegistrationStatus::absent;
    const auto *const command = verb_command(plan);
    if (command == nullptr)
    {
        return missing;
    }
    const auto key_path = rooted_key(root_, command->key_path);
    const auto planned = core::to_utf16(command->value);
    if (!key_path.has_value() || !planned.has_value())
    {
        return missing;
    }
    const auto current = default_value(key_path.value());
    if (!current.has_value())
    {
        return missing;
    }
    return same_path(quoted_executable(current.value()), quoted_executable(planned.value()))
               ? ShellRegistrationStatus::registered
               : ShellRegistrationStatus::registered_elsewhere;
}
} // namespace nenenib::adapters::win32
