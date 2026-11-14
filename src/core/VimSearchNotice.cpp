#include "VimSearchNotice.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"

#include <string>
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
    // 群の失敗は Vim 9.1 の文言（ADR 0184 の決定 3）。
    case VimPatternFailure::group:
        return "E54: Unmatched \\(";
    case VimPatternFailure::unclosed_plain_group:
        return "E53: Unmatched \\%(";
    case VimPatternFailure::unmatched_close:
        return "E55: Unmatched \\)";
    case VimPatternFailure::too_many_groups:
        return "E872: (NFA regexp) Too many '('";
    case VimPatternFailure::quantifier:
        return "Unsupported pattern item: \\}, or a multi on a group edge inside a character";
    // 量の失敗は Vim 9.1 の NFA の文言（ADR 0184 の決定 2）。E866 の文字は値に持たないので
    // 「multi」と書く（Vim は打った文字 `+` `{` を書く）。
    case VimPatternFailure::brace:
        return "E554: Syntax error in \\{...}";
    case VimPatternFailure::misplaced:
        return "E866: (NFA regexp) Misplaced multi";
    case VimPatternFailure::multi_after_multi:
        return "E871: (NFA regexp) Can't have a multi follow a multi";
    case VimPatternFailure::too_complex:
        return "E363: pattern uses more memory than 'maxmempattern'";
    case VimPatternFailure::magic:
        return "Unsupported pattern item: \\v \\V \\m \\M";
    case VimPatternFailure::escape:
        return "Unsupported pattern escape";
    case VimPatternFailure::previous_substitute:
        return "Unsupported pattern item: ~";
    case VimPatternFailure::offset:
        return "Search offset is not supported";
    case VimPatternFailure::invalid_utf8:
        return "Unsupported pattern: invalid UTF-8";
    }
    std::unreachable();
}

// 'nowrapscan' の端の報せ（Vim 9.1 の e_search_hit_bottom_without_match_for_str ほか・#1561）。
constexpr std::string_view hit_bottom_prefix = "E385: Search hit BOTTOM without match for: ";
constexpr std::string_view hit_top_prefix = "E384: Search hit TOP without match for: ";

// 表示できる長さに収める。切るのは入力の文を知らせに入れる所の 1 本 clipped_display_text（code
// point の境目で落として「…」・#1568 / #1616）。表示できない文字があれば E486 の文言だけを出す。
[[nodiscard]] DisplayText not_found_message(std::string_view pattern,
                                            std::string_view prefix = not_found_prefix)
{
    std::string text(prefix);
    text += pattern;
    return clipped_display_text(text).value_or(DisplayText::parse(not_found_alone).value());
}

[[nodiscard]] DisplayText message_of(std::string_view text)
{
    return DisplayText::parse(text).value_or(DisplayText::parse(not_found_alone).value());
}

// `:s` の数の報せ（Vim の do_sub_msg の文言）。count_only なら match、ほかは substitution。
[[nodiscard]] DisplayText counted_message(const VimSearchNotice &notice, bool count_only)
{
    std::string text = std::to_string(notice.count);
    const std::string_view noun = count_only ? " match" : " substitution";
    text += noun;
    if (notice.count != 1)
    {
        text += count_only ? "es" : "s";
    }
    text += " on " + std::to_string(notice.lines) + (notice.lines == 1 ? " line" : " lines");
    return message_of(text);
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
    case VimSearchNoticeKind::hit_bottom_without_match:
        return not_found_message(notice.pattern, hit_bottom_prefix);
    case VimSearchNoticeKind::hit_top_without_match:
        return not_found_message(notice.pattern, hit_top_prefix);
    case VimSearchNoticeKind::unsupported_pattern:
        return message_of(unsupported_text(notice.failure.value_or(VimPatternFailure::escape)));
    case VimSearchNoticeKind::substitutions:
        return counted_message(notice, false);
    case VimSearchNoticeKind::matches:
        return counted_message(notice, true);
    case VimSearchNoticeKind::no_previous_substitute:
        return message_of("E33: No previous substitute regular expression");
    case VimSearchNoticeKind::invalid_range:
        return message_of("E16: Invalid range");
    case VimSearchNoticeKind::mark_not_set:
        return message_of("E20: Mark not set");
    case VimSearchNoticeKind::backwards_range:
        return message_of("E493: Backwards range given");
    case VimSearchNoticeKind::positive_count:
        return message_of("E939: Positive count required");
    case VimSearchNoticeKind::pattern_too_complex:
        return message_of("E363: pattern uses more memory than 'maxmempattern'");
    case VimSearchNoticeKind::confirm_in_replay:
        return message_of("Not supported: the c flag of :s in a replay");
    }
    std::unreachable();
}
} // namespace nenenib::core
