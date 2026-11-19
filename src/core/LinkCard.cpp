#include "LinkCard.hpp"

#include "DevicePixels.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view scheme_mark = "://";
constexpr std::string_view mail_scheme = "mailto:";

[[nodiscard]] std::string text_of(UiMessage message, DisplayLanguage language)
{
    return std::string(ui_text(message, language));
}

// at から見て最初の stops のどれかの位置（無ければ終わり）。STL の検索は __std_find_* を core の
// 外へ出すので 1 バイトずつ歩く（ARC-003）。
[[nodiscard]] bool one_of(char character, std::string_view stops) noexcept
{
    for (const char stop : stops)
    {
        if (character == stop)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::size_t first_stop(std::string_view text, std::size_t at,
                                     std::string_view stops) noexcept
{
    for (std::size_t index = at; index < text.size(); ++index)
    {
        if (one_of(text[index], stops))
        {
            return index;
        }
    }
    return text.size();
}

// `://` の直後の位置（無ければ nullopt）。
[[nodiscard]] std::optional<std::size_t> after_scheme_mark(std::string_view text) noexcept
{
    for (std::size_t index = 0; index + scheme_mark.size() <= text.size(); ++index)
    {
        if (text.substr(index, scheme_mark.size()) == scheme_mark)
        {
            return index + scheme_mark.size();
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::pair<std::size_t, std::size_t> host_of(std::string_view text,
                                                          std::size_t start) noexcept
{
    return {start, first_stop(text, start, "/?#")};
}

// 最後の `\` か `/` の後ろから終わりまで。
[[nodiscard]] std::pair<std::size_t, std::size_t> last_name_of(std::string_view text) noexcept
{
    std::size_t start = 0;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (one_of(text[index], "\\/"))
        {
            start = index + 1;
        }
    }
    return {start, text.size()};
}

[[nodiscard]] LinkCardTexts refused_texts(DisplayLanguage language)
{
    return LinkCardTexts{text_of(UiMessage::link_card_title_refused, language),
                         text_of(UiMessage::link_card_way_refused, language),
                         ui_text_filled(UiMessage::link_card_hint_refused, language,
                                        {ui_text(UiMessage::link_card_copy, language),
                                         ui_text(UiMessage::link_card_close, language)}),
                         text_of(UiMessage::link_card_close, language),
                         text_of(UiMessage::link_card_copy, language)};
}

[[nodiscard]] LinkCardTexts opening_texts(UiMessage title, UiMessage way, UiMessage verb,
                                          DisplayLanguage language)
{
    return LinkCardTexts{
        text_of(title, language), text_of(way, language),
        ui_text_filled(UiMessage::link_card_hint, language,
                       {ui_text(verb, language), ui_text(UiMessage::link_card_cancel, language),
                        ui_text(UiMessage::link_card_copy, language)}),
        text_of(UiMessage::link_card_cancel, language), text_of(verb, language)};
}
} // namespace

std::optional<std::size_t> preview_link_at(const PreviewBlock &block, std::size_t offset) noexcept
{
    for (std::size_t index = 0; index < block.links.size(); ++index)
    {
        const auto &link = block.links.at(index);
        if (link.begin.value <= offset && offset < link.end.value)
        {
            return index;
        }
    }
    return std::nullopt;
}

LinkCardTexts link_card_texts(LinkKind kind, DisplayLanguage language)
{
    switch (kind)
    {
    case LinkKind::web:
        return opening_texts(UiMessage::link_card_title_web, UiMessage::link_card_way_web,
                             UiMessage::link_card_open, language);
    case LinkKind::mail:
        return opening_texts(UiMessage::link_card_title_mail, UiMessage::link_card_way_mail,
                             UiMessage::link_card_write, language);
    case LinkKind::document:
        return opening_texts(UiMessage::link_card_title_document, UiMessage::link_card_way_document,
                             UiMessage::link_card_open, language);
    case LinkKind::heading:
    case LinkKind::refused:
        return refused_texts(language);
    }
    std::unreachable();
}

std::pair<std::size_t, std::size_t> link_card_emphasis(const LinkDestination &destination) noexcept
{
    const std::string_view text = destination.text;
    switch (destination.kind)
    {
    case LinkKind::web:
    {
        const auto start = after_scheme_mark(text);
        return start.has_value() ? host_of(text, start.value())
                                 : std::pair{text.size(), text.size()};
    }
    case LinkKind::mail:
    {
        const std::size_t start =
            text.size() >= mail_scheme.size() ? mail_scheme.size() : text.size();
        return {start, first_stop(text, start, "?")};
    }
    case LinkKind::refused:
    {
        const auto start = after_scheme_mark(text);
        return start.has_value() ? host_of(text, start.value()) : last_name_of(text);
    }
    case LinkKind::document:
    case LinkKind::heading:
        return last_name_of(text);
    }
    std::unreachable();
}

std::int32_t link_card_width(std::int32_t area_width, std::uint32_t dpi) noexcept
{
    return std::max(std::min(to_pixels(link_card_max_width_dips, dpi),
                             area_width - (2 * to_pixels(link_card_edge_dips, dpi))),
                    0);
}

LayoutRect link_card_rect(const LayoutRect &link, const LayoutRect &area,
                          std::pair<std::int32_t, std::int32_t> size, std::uint32_t dpi) noexcept
{
    const auto [width, height] = size;
    const std::int32_t gap = to_pixels(link_card_gap_dips, dpi);
    const std::int32_t edge = to_pixels(link_card_edge_dips, dpi);
    const std::int32_t right_most = area.right - edge - width;
    const std::int32_t left = std::max(std::min(link.left, right_most), area.left + edge);
    const std::int32_t below = link.bottom + gap;
    const std::int32_t above = link.top - gap - height;
    std::int32_t top = below;
    if (below + height > area.bottom)
    {
        top = above >= area.top ? above : std::max(area.bottom - height, area.top);
    }
    return LayoutRect{left, top, left + width, top + height};
}
} // namespace nenenib::core
