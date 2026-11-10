#include "SaveQuestionText.hpp"

#include <utility>

namespace nenenib::core
{
std::string_view save_question_text(SaveQuestionText text) noexcept
{
    switch (text)
    {
    case SaveQuestionText::unsaved_question:
        return "変更が保存されていません。保存しますか。";
    case SaveQuestionText::rendering_lost_question:
        return "描画できなくなったので閉じます。保存しますか。";
    case SaveQuestionText::save_button:
        return "保存する(&Y)";
    case SaveQuestionText::discard_button:
        return "保存しない(&N)";
    case SaveQuestionText::cancel_button:
        return "キャンセル";
    }
    std::unreachable();
}
} // namespace nenenib::core
