#include "Win32WslDistributions.hpp"

#include "RegistryKey.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// 副鍵の名前（GUID）と値の名前の上限。レジストリの鍵の名前は 255 文字まで。
constexpr DWORD key_name_capacity = 256;

// 副鍵の REG_SZ の値。無い・型が違う・読めないは値なし（その副鍵を外すだけ）。
[[nodiscard]] std::optional<std::wstring> text_value(HKEY key, const wchar_t *subkey,
                                                     const wchar_t *name)
{
    DWORD bytes = 0;
    if (RegGetValueW(key, subkey, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
        bytes < sizeof(wchar_t))
    {
        return std::nullopt;
    }
    std::wstring text(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(key, subkey, name, RRF_RT_REG_SZ, nullptr, text.data(), &bytes) !=
        ERROR_SUCCESS)
    {
        return std::nullopt;
    }
    // bytes は終端の NUL を含む。
    text.resize((bytes / sizeof(wchar_t)) - 1U);
    return text;
}

[[nodiscard]] std::uint32_t state_value(HKEY key, const wchar_t *subkey)
{
    DWORD state = 0;
    DWORD size = sizeof(state);
    if (RegGetValueW(key, subkey, L"State", RRF_RT_REG_DWORD, nullptr, &state, &size) !=
        ERROR_SUCCESS)
    {
        return 0;
    }
    return state;
}

// 副鍵 1 つのディストリ。名前が無い・UTF-8 に写せないものは値なし（外す）。
[[nodiscard]] std::optional<core::WslDistribution> distribution_of(HKEY key, const wchar_t *subkey)
{
    const auto name = text_value(key, subkey, L"DistributionName");
    if (!name.has_value())
    {
        return std::nullopt;
    }
    auto utf8 = core::to_utf8(std::wstring_view(name.value()));
    if (!utf8.has_value())
    {
        return std::nullopt;
    }
    return core::WslDistribution{std::move(utf8).value(), state_value(key, subkey)};
}
} // namespace

Win32WslDistributions::Win32WslDistributions(std::wstring key_path) : key_path_(std::move(key_path))
{
}

std::expected<std::vector<core::WslDistribution>, application::WslListFailure>
Win32WslDistributions::distributions() const
{
    RegistryKey key;
    const auto opened =
        RegOpenKeyExW(HKEY_CURRENT_USER, key_path_.c_str(), 0, KEY_READ, key.receive());
    if (opened == ERROR_FILE_NOT_FOUND)
    {
        return std::vector<core::WslDistribution>{};
    }
    if (opened != ERROR_SUCCESS)
    {
        return std::unexpected(application::WslListFailure::unreadable);
    }
    std::vector<core::WslDistribution> listed;
    std::array<wchar_t, key_name_capacity> subkey{};
    for (DWORD index = 0;; ++index)
    {
        auto length = key_name_capacity;
        const auto status = RegEnumKeyExW(key.get(), index, subkey.data(), &length, nullptr,
                                          nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            return listed;
        }
        if (status != ERROR_SUCCESS)
        {
            return std::unexpected(application::WslListFailure::unreadable);
        }
        auto distribution = distribution_of(key.get(), subkey.data());
        if (distribution.has_value())
        {
            listed.push_back(std::move(distribution).value());
        }
    }
}
} // namespace nenenib::adapters::win32
