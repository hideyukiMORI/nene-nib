#include "Win32FontCollationAdapter.hpp"

#include "Utf16.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Keyed = std::pair<std::wstring, std::string>;

// 使う人の既定ロケールの照合（CSTR_LESS_THAN / CSTR_EQUAL / CSTR_GREATER_THAN）。比べられないときは
// 「同じ」とし、呼び出し元がバイト順で決める。
[[nodiscard]] int collated(std::wstring_view left, std::wstring_view right) noexcept
{
    constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (left.size() > limit || right.size() > limit)
    {
        return CSTR_EQUAL;
    }
    const auto result =
        CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0, left.data(), static_cast<int>(left.size()),
                        right.data(), static_cast<int>(right.size()), nullptr, nullptr, 0);
    return result == 0 ? CSTR_EQUAL : result;
}

[[nodiscard]] bool keyed_before(const Keyed &left, const Keyed &right) noexcept
{
    const auto order = collated(left.first, right.first);
    if (order != CSTR_EQUAL)
    {
        return order == CSTR_LESS_THAN;
    }
    return left.second < right.second;
}
} // namespace

Win32FontCollationAdapter::Win32FontCollationAdapter(application::FontFamilyPort &families) noexcept
    : families_(families)
{
}

std::expected<std::vector<std::string>, application::FontFamilyFailure>
Win32FontCollationAdapter::list()
{
    auto listed = families_.list();
    if (!listed.has_value())
    {
        return std::unexpected(listed.error());
    }
    return collated_font_names(std::move(listed).value());
}

std::vector<std::string> collated_font_names(std::vector<std::string> names)
{
    std::vector<Keyed> keyed;
    keyed.reserve(names.size());
    for (auto &name : names)
    {
        auto wide = core::to_utf16(name).value_or(std::wstring{});
        keyed.emplace_back(std::move(wide), std::move(name));
    }
    std::sort(keyed.begin(), keyed.end(), keyed_before);
    std::vector<std::string> sorted;
    sorted.reserve(keyed.size());
    for (auto &entry : keyed)
    {
        sorted.push_back(std::move(entry.second));
    }
    return sorted;
}
} // namespace nenenib::adapters::win32
