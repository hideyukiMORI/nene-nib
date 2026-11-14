#include "InputText.hpp"

#include "Utf8.hpp"
#include "VimWordMotion.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char tab_byte = '\t';

// 差し込みの本体。tab_kept なら Tab だけは制御文字に数えない（Vim の `/` `?` の行・#1643）。
[[nodiscard]] std::expected<InputText, ExFailure>
inserted_checked(const InputText &input, std::string_view text, bool tab_kept)
{
    const bool refused =
        tab_kept
            ? std::ranges::any_of(text,
                                  [](const char &value)
                                  {
                                      return value != tab_byte &&
                                             has_control_character(std::string_view{&value, 1});
                                  })
            : has_control_character(text);
    if (!validate_utf8(text) || refused)
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    if (text.size() > input_line_hold - std::min(input.text.size(), input_line_hold))
    {
        InputText kept = input;
        kept.lost = true;
        return kept;
    }
    std::string next = input.text;
    next.insert(input.caret.value, text);
    return InputText{std::move(next), Offset{input.caret.value + text.size()}, input.lost};
}

// 行頭からキャレットまでの語の前を消す（Vim の c_CTRL-W・#1550）。INSERT の `Ctrl-w` と同じ
// vim_word_erase_begin の 1 本で切る。行頭では何もしない。
[[nodiscard]] InputText word_erased(const InputText &input)
{
    InputText next = input;
    if (input.caret.value == 0)
    {
        return next;
    }
    const std::size_t begin =
        vim_word_erase_begin(std::string_view{input.text}.substr(0, input.caret.value), 0);
    next.text.erase(begin, input.caret.value - begin);
    next.caret = Offset{begin};
    return next;
}
} // namespace

std::expected<InputText, ExFailure> inserted_input(const InputText &input, std::string_view text)
{
    return inserted_checked(input, text, false);
}

std::expected<InputText, ExFailure> inserted_tab_input(const InputText &input,
                                                       std::string_view text)
{
    return inserted_checked(input, text, true);
}

bool overflows(const InputText &input) noexcept
{
    return input.lost || input.text.size() > input_line_limit;
}

OffsetRange input_line_window(std::string_view text, Offset from, Offset to) noexcept
{
    // 窓の端で切れた code point は、先頭は leading_continuations、末尾は whole_code_points の
    // 1 本で落とす（#1322・#1292・ARC-001）。
    const std::size_t head = std::min(from.value, text.size());
    const std::size_t start = head > input_line_window_before ? head - input_line_window_before : 0;
    const Offset begin{start + leading_continuations(text.substr(start, head - start))};
    const std::size_t tail = std::min(to.value, text.size());
    const std::string_view after = text.substr(tail, input_line_window_after);
    return OffsetRange{begin, Offset{tail + whole_code_points(after)}};
}

InputText edited_input(const InputText &input, CommandEdit edit)
{
    InputText next = input;
    const auto before = previous_code_point(input.text, input.caret);
    const auto after = next_code_point(input.text, input.caret);
    switch (edit)
    {
    case CommandEdit::left:
        next.caret = before;
        return next;
    case CommandEdit::right:
        next.caret = after;
        return next;
    case CommandEdit::home:
        next.caret = Offset{0};
        return next;
    case CommandEdit::end:
        next.caret = Offset{input.text.size()};
        return next;
    case CommandEdit::backspace:
        next.text.erase(before.value, input.caret.value - before.value);
        next.caret = before;
        return next;
    case CommandEdit::erase:
        next.text.erase(input.caret.value, after.value - input.caret.value);
        return next;
    // Vim の c_CTRL-U はキャレットの前だけを消し、後ろは残す（`:xyzd<Left><C-u>` は `d`・#1550）。
    case CommandEdit::erase_to_start:
        next.text.erase(0, input.caret.value);
        next.caret = Offset{0};
        return next;
    case CommandEdit::erase_word:
        return word_erased(input);
    case CommandEdit::complete_next:
    case CommandEdit::complete_previous:
    case CommandEdit::page_previous:
    case CommandEdit::page_next:
    case CommandEdit::list_first:
    case CommandEdit::list_last:
    case CommandEdit::history_previous:
    case CommandEdit::history_next:
        // 補完と一覧の送りと履歴は候補・履歴を持つ型だけの動作。文字列とキャレットの規則としては何もしない。
        return next;
    }
    std::unreachable();
}
} // namespace nenenib::core
