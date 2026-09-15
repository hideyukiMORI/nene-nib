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

[[nodiscard]] std::string clipped(std::string text)
{
    if (text.size() <= DisplayText::maximum_bytes)
    {
        return text;
    }
    Offset cut{DisplayText::maximum_bytes - ellipsis.size()};
    while (!is_boundary(text, cut))
    {
        cut = previous_code_point(text, cut);
    }
    text.resize(cut.value);
    text += ellipsis;
    return text;
}
} // namespace

DisplayText tab_title_for(const std::optional<FilePath> &path, SaveState state)
{
    std::string title(mark_of(state));
    title += name_of(path);
    // 材料は検証済みの経路と固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    return DisplayText::parse(clipped(std::move(title))).value();
}
} // namespace nenenib::core
