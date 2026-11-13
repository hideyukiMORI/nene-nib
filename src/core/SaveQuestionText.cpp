#include "SaveQuestionText.hpp"

#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] UiMessage save_question_message(SaveQuestionText text) noexcept
{
    switch (text)
    {
    case SaveQuestionText::unsaved_question:
        return UiMessage::dialog_save_question;
    case SaveQuestionText::rendering_lost_question:
        return UiMessage::dialog_save_question_rendering_lost;
    case SaveQuestionText::save_button:
        return UiMessage::dialog_save_button_save;
    case SaveQuestionText::discard_button:
        return UiMessage::dialog_save_button_discard;
    case SaveQuestionText::cancel_button:
        return UiMessage::dialog_save_button_cancel;
    }
    std::unreachable();
}
} // namespace

std::string_view save_question_text(SaveQuestionText text, DisplayLanguage language) noexcept
{
    return ui_text(save_question_message(text), language);
}

std::string_view save_question_text(SaveQuestionText text) noexcept
{
    return save_question_text(text, DisplayLanguage::japanese);
}
} // namespace nenenib::core
