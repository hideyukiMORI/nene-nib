#include "InputLinePromptText.hpp"

#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <utility>

namespace nenenib::ui::win32
{
std::string prompt_text(core::InputLinePrompt prompt, std::string_view detail,
                        core::DisplayLanguage language)
{
    switch (prompt)
    {
    case core::InputLinePrompt::ex:
        return ":";
    case core::InputLinePrompt::palette:
    // `:s///c` の問いは文がそのまま Vim の文言（ADR 0168 の決定 6）。
    case core::InputLinePrompt::substitute_confirm:
        return {};
    case core::InputLinePrompt::search_forward:
        return "/";
    case core::InputLinePrompt::search_backward:
        return "?";
    // 通常モードの検索と行へ移動（ADR 0174 の決定 3）。
    case core::InputLinePrompt::find:
        return std::string(core::ui_text(core::UiMessage::prompt_find, language));
    case core::InputLinePrompt::go_to_line:
        return std::string(core::ui_text(core::UiMessage::prompt_go_to_line, language));
    // 置換の 2 段目は検索語を添える（ADR 0174 の結果「置換後 (abc): 」）。
    case core::InputLinePrompt::replace_with:
        return detail.empty()
                   ? std::string(core::ui_text(core::UiMessage::prompt_replace_with, language))
                   : core::ui_text_filled(core::UiMessage::prompt_replace_with_pattern, language,
                                          {detail});
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
