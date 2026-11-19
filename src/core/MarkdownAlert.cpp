#include "MarkdownAlert.hpp"

#include "Offset.hpp"
#include "PreviewSpan.hpp"
#include "PreviewStyle.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::array<PreviewAlert, 5> alerts{PreviewAlert::note, PreviewAlert::tip,
                                             PreviewAlert::important, PreviewAlert::warning,
                                             PreviewAlert::caution};

[[nodiscard]] bool blank(char value) noexcept
{
    return value == ' ' || value == '\t';
}

[[nodiscard]] char upper(char value) noexcept
{
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A') : value;
}

// text が `[!` 名前 `]` の綴りか（大小を区別しない）。
[[nodiscard]] bool spells(std::string_view text, PreviewAlert alert) noexcept
{
    const auto name = preview_alert_name(alert);
    return text.size() == name.size() + 3U && text.starts_with("[!") && text.ends_with(']') &&
           std::ranges::equal(text.substr(2U, name.size()), name, {},
                              [](char value) { return upper(value); });
}

[[nodiscard]] std::optional<PreviewAlert> spelled(std::string_view text) noexcept
{
    const auto found =
        std::ranges::find_if(alerts, [text](PreviewAlert alert) { return spells(text, alert); });
    return found == alerts.end() ? std::nullopt : std::optional(*found);
}
} // namespace

std::optional<PreviewAlert> markdown_alert_line(std::string_view line) noexcept
{
    while (!line.empty() && blank(line.front()))
    {
        line.remove_prefix(1U);
    }
    while (!line.empty() && (blank(line.back()) || line.back() == '\r'))
    {
        line.remove_suffix(1U);
    }
    return spelled(line);
}

std::optional<std::ptrdiff_t> markdown_alert_titled(PreviewBlock &block, PreviewAlert alert)
{
    const auto name = preview_alert_name(alert);
    const auto marker = name.size() + 3U;
    const std::string_view text = block.text;
    const bool ends = text.size() == marker || text[marker] == ' ' || text[marker] == '\n';
    if (text.size() < marker || !ends || spelled(text.substr(0U, marker)) != alert ||
        std::ranges::any_of(block.spans, [marker](const PreviewSpan &span)
                            { return span.begin.value <= marker; }))
    {
        return std::nullopt;
    }
    // 印と続く空白か改行 1 つを、名前と改行に置き換える（印だけの段落は名前だけ）。
    const auto removed = text.size() == marker ? marker : marker + 1U;
    std::string titled(name);
    if (removed > marker)
    {
        titled.push_back('\n');
    }
    titled.append(text.substr(removed));
    const auto shift =
        static_cast<std::ptrdiff_t>(titled.size()) - static_cast<std::ptrdiff_t>(block.text.size());
    for (auto &span : block.spans)
    {
        span.begin =
            Offset{static_cast<std::size_t>(static_cast<std::ptrdiff_t>(span.begin.value) + shift)};
        span.end =
            Offset{static_cast<std::size_t>(static_cast<std::ptrdiff_t>(span.end.value) + shift)};
    }
    block.spans.insert(block.spans.begin(),
                       PreviewSpan{Offset{0U}, Offset{name.size()}, PreviewStyle::alert_title});
    block.text = std::move(titled);
    return shift;
}
} // namespace nenenib::core
