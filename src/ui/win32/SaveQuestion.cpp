#include "SaveQuestion.hpp"

#include "Utf16.hpp"

#include <commctrl.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// 固定の文は ASCII でないので、変換の失敗は空の文字に倒さず箱の失敗（cancel）と同じ道へ。
[[nodiscard]] std::optional<std::wstring> utf16_of(core::SaveQuestionText text,
                                                   core::DisplayLanguage language)
{
    auto wide = core::to_utf16(core::save_question_text(text, language));
    if (!wide.has_value())
    {
        return std::nullopt;
    }
    return std::move(wide.value());
}

// TD_WARNING_ICON（MAKEINTRESOURCEW(-1)）を C の cast を書かずに作る（CPP-009）。
[[nodiscard]] PCWSTR warning_icon() noexcept
{
    return std::bit_cast<PCWSTR>(static_cast<ULONG_PTR>(static_cast<WORD>(-1)));
}

// 箱を出して押された答えを返す。箱の入れ子のループの間に posted の WM_CLOSE で閉じる確認が
// もう一度来ても、2 つ目の箱を出さずに cancel（閉じない側）を返す。描画を失った後の確認は今どおり
// 重ねて聞く（閉じるが捨てない側へ倒すと、退避の後の打鍵を聞かずに失う）。UI のスレッドだけが通る。
[[nodiscard]] SaveAnswer shown_answer(const TASKDIALOGCONFIG &config, SaveQuestion question)
{
    static bool asking = false;
    if (asking && question == SaveQuestion::unsaved)
    {
        return SaveAnswer::cancel;
    }
    const bool outer = !asking;
    asking = true;
    int pressed = 0;
    const HRESULT shown = TaskDialogIndirect(&config, &pressed, nullptr, nullptr);
    asking = !outer;
    if (FAILED(shown))
    {
        return SaveAnswer::cancel;
    }
    return save_answer_of(pressed);
}
} // namespace

// MessageBoxW のボタンの文言は OS が決め、hide の実機（Windows 11 26300・日本語）では
// MB_YESNOCANCEL が「OK／さらに詳しく／キャンセル」と出た（#1282）。文言は自分で持つ。
SaveAnswer ask_save(HWND owner, const wchar_t *title, SaveQuestion question,
                    core::DisplayLanguage language)
{
    const auto buttons = save_buttons(question);
    std::vector<std::wstring> labels;
    labels.reserve(buttons.size());
    std::vector<TASKDIALOG_BUTTON> rows;
    rows.reserve(buttons.size());
    for (const auto &button : buttons)
    {
        auto label = utf16_of(button.second, language);
        if (!label.has_value())
        {
            return SaveAnswer::cancel;
        }
        labels.push_back(std::move(label.value()));
    }
    // 文字列の置き場が動かなくなってから指す（labels はこの後に伸ばさない）。
    for (std::size_t index = 0; index < buttons.size(); ++index)
    {
        rows.push_back(TASKDIALOG_BUTTON{buttons[index].first, labels[index].c_str()});
    }
    const auto message = utf16_of(save_message(question), language);
    if (!message.has_value())
    {
        return SaveAnswer::cancel;
    }
    // キャンセルのボタンがある形だけ Esc・Alt+F4・× で閉じられる（IDCANCEL が返る）。
    const bool cancellable = std::ranges::any_of(buttons, [](const SaveButton &button)
                                                 { return button.first == IDCANCEL; });
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    // 箱は本窓の上に出す（owner のモニタの中央ではなく）。
    config.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW |
                     (cancellable ? TDF_ALLOW_DIALOG_CANCELLATION : TASKDIALOG_FLAGS{0});
    config.pszWindowTitle = title;
    config.pszMainIcon = warning_icon();
    config.pszContent = message.value().c_str();
    config.cButtons = static_cast<UINT>(rows.size());
    config.pButtons = rows.data();
    config.nDefaultButton = buttons.front().first;
    return shown_answer(config, question);
}
} // namespace nenenib::ui::win32
