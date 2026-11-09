#include "SaveQuestion.hpp"

#include "Utf16.hpp"

#include <commctrl.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <string>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] std::wstring utf16_of(core::SaveQuestionText text)
{
    return core::to_utf16(core::save_question_text(text)).value_or(std::wstring{});
}

// TD_WARNING_ICON（MAKEINTRESOURCEW(-1)）を C の cast を書かずに作る（CPP-009）。
[[nodiscard]] PCWSTR warning_icon() noexcept
{
    return std::bit_cast<PCWSTR>(static_cast<ULONG_PTR>(static_cast<WORD>(-1)));
}
} // namespace

// MessageBoxW のボタンの文言は OS が決め、hide の実機（Windows 11 26300・日本語）では
// MB_YESNOCANCEL が「OK／さらに詳しく／キャンセル」と出た（#1282）。文言は自分で持つ。
SaveAnswer ask_save(HWND owner, const wchar_t *title, SaveQuestion question)
{
    const auto buttons = save_buttons(question);
    std::vector<std::wstring> labels;
    labels.reserve(buttons.size());
    std::vector<TASKDIALOG_BUTTON> rows;
    rows.reserve(buttons.size());
    for (const auto &button : buttons)
    {
        labels.push_back(utf16_of(button.second));
    }
    // 文字列の置き場が動かなくなってから指す（labels はこの後に伸ばさない）。
    for (std::size_t index = 0; index < buttons.size(); ++index)
    {
        rows.push_back(TASKDIALOG_BUTTON{buttons[index].first, labels[index].c_str()});
    }
    const std::wstring message = utf16_of(save_message(question));
    // キャンセルのボタンがある形だけ Esc・Alt+F4・× で閉じられる（IDCANCEL が返る）。
    const bool cancellable = std::ranges::any_of(buttons, [](const SaveButton &button)
                                                 { return button.first == IDCANCEL; });
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = cancellable ? TDF_ALLOW_DIALOG_CANCELLATION : 0;
    config.pszWindowTitle = title;
    config.pszMainIcon = warning_icon();
    config.pszContent = message.c_str();
    config.cButtons = static_cast<UINT>(rows.size());
    config.pButtons = rows.data();
    config.nDefaultButton = buttons.front().first;
    int pressed = 0;
    if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr)))
    {
        return SaveAnswer::cancel;
    }
    return save_answer_of(pressed);
}
} // namespace nenenib::ui::win32
