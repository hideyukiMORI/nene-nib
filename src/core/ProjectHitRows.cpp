#include "ProjectHitRows.hpp"

#include "CommandChoiceKind.hpp"
#include "DisplayText.hpp"
#include "PaletteOrigin.hpp"

#include <algorithm>
#include <optional>
#include <string_view>

namespace nenenib::core
{
namespace
{
// 窓を省いた印（題名と同じ「…」）。
constexpr std::string_view elided_mark = "…";
constexpr char first_printable = 0x20;
constexpr char delete_character = 0x7F;

[[nodiscard]] bool separator(char value) noexcept
{
    return value == '\\' || value == '/';
}

// 制御文字（Tab を含む）を 1 バイトの空白に置き換える（バイトの位置を保つ・DisplayText が断る文字を
// 残さない）。
[[nodiscard]] std::string printable(std::string_view text)
{
    std::string shown(text);
    std::ranges::replace_if(
        shown, [](char value)
        { return (value >= 0 && value < first_printable) || value == delete_character; }, ' ');
    return shown;
}
} // namespace

std::string project_relative_path(const FilePath &root, const FilePath &file)
{
    const std::string_view base = root.text();
    const std::string_view full = file.text();
    if (base.empty() || full.size() <= base.size() || !full.starts_with(base))
    {
        return std::string(full);
    }
    if (separator(base.back()))
    {
        return std::string(full.substr(base.size()));
    }
    if (separator(full.at(base.size())))
    {
        return std::string(full.substr(base.size() + 1));
    }
    return std::string(full);
}

ProjectHitRow project_hit_row(const FilePath &root, const FilePath &file, const ProjectHit &hit)
{
    return ProjectHitRow{project_relative_path(root, file),
                         hit.line,
                         hit.column,
                         printable(hit.line_text),
                         hit.match_begin,
                         hit.match_end,
                         hit.text_begin > 0};
}

CommandChoice project_hit_choice(const FilePath &file, const ProjectHitRow &row)
{
    const std::string place =
        row.path + ":" + std::to_string(row.line.value) + ":" + std::to_string(row.column.value);
    const std::string body = (row.elided ? std::string(elided_mark) : std::string()) + row.text;
    auto detail = clipped_display_text(body);
    // パスはファイル名に使えない制御文字を含まず、行と桁の数字と `:`
    // を足すので空でない（不変条件）。
    return CommandChoice{clipped_display_text(place).value(),
                         std::string(file.text()),
                         CommandChoiceKind::open,
                         detail.has_value() ? std::optional{std::move(detail).value()}
                                            : std::nullopt,
                         PaletteOrigin::project_hit,
                         std::nullopt,
                         {}};
}
} // namespace nenenib::core
