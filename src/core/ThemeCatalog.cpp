#include "ThemeCatalog.hpp"

#include "AsciiCase.hpp"
#include "BuiltinThemes.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 組み込みの名前の名前順（#696）。表の順は表示に使わない。並べるのはコンパイル時の 1 回だけ。
constexpr auto sorted_builtin_names = []
{
    std::array<std::string_view, builtin_themes.size()> names{};
    std::ranges::transform(builtin_themes, names.begin(), &Theme::name);
    std::ranges::sort(names, ascii_folded_before);
    return names;
}();

[[nodiscard]] std::expected<void, ThemeCatalogFailure> checked_record(const ThemeRecord &record)
{
    if (record.name.text() == "system" || theme_named(record.name.text()).has_value())
    {
        return std::unexpected(ThemeCatalogFailure::reserved_name);
    }
    if (record.value.has_value() && record.value.value().name() != record.name.text())
    {
        return std::unexpected(ThemeCatalogFailure::name_mismatch);
    }
    return {};
}
} // namespace

ThemeCatalog::ThemeCatalog(std::shared_ptr<const std::vector<ThemeRecord>> records)
    : records_(std::move(records))
{
}

ThemeCatalog ThemeCatalog::builtins()
{
    return ThemeCatalog(nullptr);
}

std::expected<ThemeCatalog, ThemeCatalogFailure>
ThemeCatalog::from(std::vector<ThemeRecord> records)
{
    std::sort(records.begin(), records.end(), [](const auto &left, const auto &right)
              { return ascii_folded_before(left.name.text(), right.name.text()); });
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const auto checked = checked_record(records.at(index));
        if (!checked)
        {
            return std::unexpected(checked.error());
        }
        if (index > 0 && records.at(index - 1).name == records.at(index).name)
        {
            return std::unexpected(ThemeCatalogFailure::duplicate_name);
        }
    }
    if (records.empty())
    {
        return builtins();
    }
    return ThemeCatalog(std::make_shared<const std::vector<ThemeRecord>>(std::move(records)));
}

std::span<const ThemeRecord> ThemeCatalog::records() const & noexcept
{
    if (records_ == nullptr)
    {
        return {};
    }
    return *records_;
}

std::expected<ThemeChoice, ThemeLookupFailure> ThemeCatalog::find(const ThemeName &name) const
{
    const auto builtin = theme_named(name.text());
    if (builtin.has_value())
    {
        return ThemeChoice::from(builtin.value());
    }
    for (const auto &record : records())
    {
        if (record.name != name)
        {
            continue;
        }
        if (!record.value)
        {
            return std::unexpected(ThemeLookupFailure{name, record.value.error()});
        }
        return record.value.value();
    }
    return std::unexpected(ThemeLookupFailure{name, ThemeFailure::not_found});
}

// 組み込みと利用者のテーマを 1 つの名前順に混ぜる（#696・Vim 9.1 の colorscheme の補完と同じ）。
// 利用者のテーマは from が同じ順に並べてある。
std::vector<std::string> ThemeCatalog::names() const
{
    const auto user = records();
    std::vector<std::string> names;
    names.reserve(sorted_builtin_names.size() + user.size());
    auto builtin = sorted_builtin_names.begin();
    for (const auto &record : user)
    {
        const auto name = record.name.text();
        for (; builtin != sorted_builtin_names.end() && ascii_folded_before(*builtin, name);
             ++builtin)
        {
            names.emplace_back(*builtin);
        }
        names.emplace_back(name);
    }
    for (; builtin != sorted_builtin_names.end(); ++builtin)
    {
        names.emplace_back(*builtin);
    }
    return names;
}
} // namespace nenenib::core
