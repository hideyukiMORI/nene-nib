#include "TabTitle.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view untitled = "無題";
constexpr std::string_view modified_mark = "● ";
constexpr std::string_view ellipsis = "…";

[[nodiscard]] std::string_view name_of(const std::optional<FilePath> &path)
{
    if (!path.has_value())
    {
        return untitled;
    }
    // 「C:\」のように区切りで終わる経路ではファイル名が空になる。そのときは経路そのものを出す。
    const std::string_view name = path.value().file_name();
    if (name.empty())
    {
        return path.value().text();
    }
    return name;
}

[[nodiscard]] std::string_view mark_of(SaveState state) noexcept
{
    switch (state)
    {
    case SaveState::saved:
        return std::string_view{};
    case SaveState::modified:
        return modified_mark;
    }
    std::unreachable();
}

[[nodiscard]] DisplayText clipped(std::string_view text, std::string_view prefix)
{
    const bool fits = text.size() <= DisplayText::maximum_bytes - prefix.size();
    if (prefix.empty() && fits)
    {
        return DisplayText::parse(text).value();
    }
    Offset cut{text.size()};
    std::string_view suffix;
    if (!fits)
    {
        cut = Offset{DisplayText::maximum_bytes - ellipsis.size() - prefix.size()};
        while (!is_boundary(text, cut))
        {
            cut = previous_code_point(text, cut);
        }
        suffix = ellipsis;
    }
    std::string shown;
    shown.reserve(prefix.size() + cut.value + suffix.size());
    shown += prefix;
    shown += text.substr(0, cut.value);
    shown += suffix;
    // 材料は検証済みの経路と固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    return DisplayText::parse(shown).value();
}
} // namespace

DisplayText tab_title_for(const std::optional<FilePath> &path, SaveState state)
{
    return clipped(name_of(path), mark_of(state));
}

std::optional<DisplayText> tab_folder_for(const std::optional<FilePath> &path)
{
    if (!path.has_value())
    {
        return std::nullopt;
    }
    const std::string_view text = path.value().text();
    const std::size_t name = path.value().file_name().size();
    // file_name は最後の区切りの後ろなので、その前の 1 文字が区切り（無ければ経路の全体が名前）。
    if (name >= text.size())
    {
        return std::nullopt;
    }
    const std::string_view folder = text.substr(0, text.size() - name - 1);
    if (folder.empty())
    {
        return std::nullopt;
    }
    return clipped(folder, std::string_view{});
}
} // namespace nenenib::core
