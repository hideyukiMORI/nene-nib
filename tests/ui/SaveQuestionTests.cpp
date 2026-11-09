#include "SaveAnswer.hpp"
#include "SaveQuestion.hpp"
#include "SaveQuestionText.hpp"
#include "SaveStep.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <algorithm>
#include <initializer_list>
#include <ranges>
#include <utility>

namespace nenenib::tests::ui
{
namespace
{
using nenenib::core::SaveQuestionText;
using nenenib::ui::win32::save_answer_of;
using nenenib::ui::win32::save_buttons;
using nenenib::ui::win32::save_message;
using nenenib::ui::win32::save_step;
using nenenib::ui::win32::SaveAnswer;
using nenenib::ui::win32::SaveButton;
using nenenib::ui::win32::SaveQuestion;
using nenenib::ui::win32::SaveStep;

// ボタンの id → 答え（#1282）。保存する → 保存の道・保存しない → 閉じる・キャンセル → 何もしない。
// 箱を出せなかった 0 と知らない id は閉じない側（キャンセル）に倒す。
void verify_answers()
{
    expect(save_answer_of(IDYES) == SaveAnswer::save, "the save button answers save");
    expect(save_answer_of(IDNO) == SaveAnswer::discard, "the discard button answers discard");
    expect(save_answer_of(IDCANCEL) == SaveAnswer::cancel, "the cancel button answers cancel");
    expect(save_answer_of(0) == SaveAnswer::cancel, "a dialog that failed answers cancel");
    expect(save_answer_of(IDOK) == SaveAnswer::cancel, "an unknown button answers cancel");
}

[[nodiscard]] bool buttons_are(SaveQuestion question, std::initializer_list<SaveButton> expected)
{
    return std::ranges::equal(save_buttons(question), expected);
}

// 2 つの形のボタンの並び・既定（先頭）・文の名前と、各ボタンの答え（ADR 0133 の決定 2・3）。
void verify_buttons()
{
    expect(buttons_are(SaveQuestion::unsaved, {{IDYES, SaveQuestionText::save_button},
                                               {IDNO, SaveQuestionText::discard_button},
                                               {IDCANCEL, SaveQuestionText::cancel_button}}),
           "closing asks save, discard and cancel with save as the default");
    expect(buttons_are(SaveQuestion::rendering_lost, {{IDYES, SaveQuestionText::save_button},
                                                      {IDNO, SaveQuestionText::discard_button}}),
           "losing the rendering asks save and discard without cancel");
    expect(save_message(SaveQuestion::unsaved) == SaveQuestionText::unsaved_question &&
               save_message(SaveQuestion::rendering_lost) ==
                   SaveQuestionText::rendering_lost_question,
           "each form shows its own question");
    const auto answers_match = [](const SaveButton &button)
    {
        switch (button.second)
        {
        case SaveQuestionText::save_button:
            return save_answer_of(button.first) == SaveAnswer::save;
        case SaveQuestionText::discard_button:
            return save_answer_of(button.first) == SaveAnswer::discard;
        case SaveQuestionText::cancel_button:
            return save_answer_of(button.first) == SaveAnswer::cancel;
        case SaveQuestionText::unsaved_question:
        case SaveQuestionText::rendering_lost_question:
            return false;
        }
        std::unreachable();
    };
    expect(std::ranges::all_of(save_buttons(SaveQuestion::unsaved), answers_match) &&
               std::ranges::all_of(save_buttons(SaveQuestion::rendering_lost), answers_match),
           "every button's id answers what its label says");
}

// 答えの後の一歩（ADR 0133 の追補 #1282）。キャンセル（箱を出せなかったも含む）は閉じる確認では
// 閉じず、描画を失った後では閉じるが捨てたことにしない（自動退避を残す・ADR 0137 の決定 5）。
void verify_steps()
{
    for (const auto question : {SaveQuestion::unsaved, SaveQuestion::rendering_lost})
    {
        expect(save_step(question, SaveAnswer::save) == SaveStep::save,
               "saving goes down the save path in both forms");
        expect(save_step(question, SaveAnswer::discard) == SaveStep::close_discarding,
               "discarding closes and counts as discarded in both forms");
    }
    expect(save_step(SaveQuestion::unsaved, SaveAnswer::cancel) == SaveStep::stay,
           "cancelling the closing question keeps the window");
    expect(save_step(SaveQuestion::rendering_lost, SaveAnswer::cancel) == SaveStep::close_keeping,
           "a lost-rendering question that could not be shown closes without discarding");
}

// 文は core の 1 か所で、ボタンは OS の既定の文言（はい／いいえ）に依らない。
void verify_texts()
{
    using nenenib::core::save_question_text;
    expect(save_question_text(SaveQuestionText::unsaved_question) ==
               "変更が保存されていません。保存しますか。",
           "the closing question keeps its wording");
    expect(save_question_text(SaveQuestionText::rendering_lost_question)
               .starts_with("描画できなくなったので閉じます"),
           "the lost-rendering question starts as ADR 0133 decision 3 says");
    expect(save_question_text(SaveQuestionText::save_button) == "保存する(&Y)" &&
               save_question_text(SaveQuestionText::discard_button) == "保存しない(&N)" &&
               save_question_text(SaveQuestionText::cancel_button) == "キャンセル",
           "the buttons carry our own labels with the Y and N access keys");
}
} // namespace

void verify_save_question()
{
    verify_answers();
    verify_buttons();
    verify_steps();
    verify_texts();
}
} // namespace nenenib::tests::ui
