#include "VimSearchNotice.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"

#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view not_found_prefix = "E486: Pattern not found: ";
constexpr std::string_view not_found_alone = "E486: Pattern not found";

// 未対応の構文の文言（Vim には無い報せなので本実装のもの）。
[[nodiscard]] std::string_view unsupported_text(VimPatternFailure failure)
{
    switch (failure)
    {
    case VimPatternFailure::group:
        return "Unsupported pattern item: \\( \\)";
    case VimPatternFailure::branch:
        return "Unsupported pattern item: \\|";
    case VimPatternFailure::quantifier:
        return "Unsupported pattern item: \\{ \\+ \\= \\?";
    case VimPatternFailure::magic:
        return "Unsupported pattern item: \\v \\V \\m \\M";
    case VimPatternFailure::ignore_case:
        return "Unsupported pattern item: \\c \\C";
    case VimPatternFailure::escape:
        return "Unsupported pattern escape";
    case VimPatternFailure::previous_substitute:
        return "Unsupported pattern item: ~";
    case VimPatternFailure::offset:
        return "Search offset is not supported";
    }
    std::unreachable();
}

// 表示できる長さに収める。切るのは文字の境界で、切れたら E486 の文言だけを出す。
[[nodiscard]] DisplayText not_found_message(std::string_view pattern)
{
    const std::size_t room = DisplayText::maximum_bytes - not_found_prefix.size();
    std::string text(not_found_prefix);
    text += pattern.substr(0, room);
    return DisplayText::parse(text).value_or(DisplayText::parse(not_found_alone).value());
}

[[nodiscard]] DisplayText message_of(std::string_view text)
{
    return DisplayText::parse(text).value_or(DisplayText::parse(not_found_alone).value());
}
} // namespace

DisplayText vim_search_message(const VimSearchNotice &notice)
{
    switch (notice.kind)
    {
    case VimSearchNoticeKind::pattern_not_found:
        return not_found_message(notice.pattern);
    case VimSearchNoticeKind::no_previous_pattern:
        return message_of("E35: No previous regular expression");
    case VimSearchNoticeKind::no_word_under_cursor:
        return message_of("E348: No string under cursor");
    case VimSearchNoticeKind::wrapped_to_top:
        return message_of("search hit BOTTOM, continuing at TOP");
    case VimSearchNoticeKind::wrapped_to_bottom:
        return message_of("search hit TOP, continuing at BOTTOM");
    case VimSearchNoticeKind::unsupported_pattern:
        return message_of(unsupported_text(notice.failure.value_or(VimPatternFailure::escape)));
    }
    std::unreachable();
}
} // namespace nenenib::core
