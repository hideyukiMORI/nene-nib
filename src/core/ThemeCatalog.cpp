#include "ThemeCatalog.hpp"

#include "BuiltinThemes.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
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
              { return left.name.text() < right.name.text(); });
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

std::vector<std::string> ThemeCatalog::names() const
{
    std::vector<std::string> names;
    for (const auto &theme : builtin_themes)
    {
        names.emplace_back(theme.name);
    }
    for (const auto &record : records())
    {
        names.emplace_back(record.name.text());
    }
    return names;
}
} // namespace nenenib::core
