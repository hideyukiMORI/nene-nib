// Issue #1019 の T1b（#1593・ADR 0123 の追補 2026-11-13 の決定 2・3・6・8）。表示言語の解決と
// 切り替え: 起動の解決（設定 × OS の観測）・`:set language=` の全値と保存・`:set language?`・断り・
// 操作「表示言語を選ぶ」・切り替えで frame の文が変わる・本文 / undo / レジスタは変わらない・
// port の回数・全文を英語と中国語で埋めて置き場が残らない。
#include "CommandChoice.hpp"
#include "CommandEdit.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DisplayLanguageNames.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorSettings.hpp"
#include "InputLineView.hpp"
#include "InsertDateTime.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDisplayLanguageList.hpp"
#include "OpenOperationList.hpp"
#include "QuantityText.hpp"
#include "SaveQuestionText.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "SettingsIssue.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "ToggleBookmark.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimMode.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::DisplayLanguage;
using core::DisplayLanguageChoice;
using core::UiMessage;

[[nodiscard]] core::EditorSettings settings_with(DisplayLanguageChoice choice)
{
    auto settings = core::default_editor_settings();
    settings.ui_language = choice;
    return settings;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] std::string ex_message(app::EditorController &controller, std::string text)
{
    return message_of(run_ex(controller, std::move(text)));
}

// 面の i 行目の命令と説明（無ければ空）。
[[nodiscard]] std::pair<std::string, std::string> palette_row(const app::EditorFrame &frame,
                                                              std::size_t index)
{
    if (!frame.command_palette.has_value())
    {
        return {};
    }
    const auto &rows = frame.command_palette.value().rows;
    if (index >= rows.size())
    {
        return {};
    }
    const auto &row = rows.at(index);
    return {row.command,
            row.detail.has_value() ? std::string(row.detail.value().text()) : std::string{}};
}

[[nodiscard]] std::string body_of(app::EditorController &controller)
{
    std::string text;
    for (const auto &line : controller.frame().lines)
    {
        text += line.text;
        text += '\n';
    }
    return text;
}

// 起動の解決（決定 2・3）: 希望値 × 観測の組で、controller が 1 回だけ観測して状態に置く。
void verify_startup_resolution()
{
    using Case = std::tuple<DisplayLanguageChoice, std::optional<DisplayLanguage>, DisplayLanguage>;
    const std::array<Case, 8> cases{{
        {DisplayLanguageChoice::system, DisplayLanguage::japanese, DisplayLanguage::japanese},
        {DisplayLanguageChoice::system, DisplayLanguage::english, DisplayLanguage::english},
        {DisplayLanguageChoice::system, DisplayLanguage::simplified_chinese,
         DisplayLanguage::simplified_chinese},
        {DisplayLanguageChoice::system, std::nullopt, DisplayLanguage::english},
        {DisplayLanguageChoice::japanese, DisplayLanguage::english, DisplayLanguage::japanese},
        {DisplayLanguageChoice::english, DisplayLanguage::japanese, DisplayLanguage::english},
        {DisplayLanguageChoice::simplified_chinese, DisplayLanguage::japanese,
         DisplayLanguage::simplified_chinese},
        {DisplayLanguageChoice::korean, DisplayLanguage::japanese, DisplayLanguage::english},
    }};
    for (const auto &[choice, observed, expected] : cases)
    {
        Editing editing{SettingsReading{settings_with(choice)}, observed};
        auto &controller = editing.controller();
        expect(editing.ui_language().calls() == 1 && controller.display_language() == expected &&
                   controller.frame().language == expected,
               "the start observes the OS language once and resolves it with the wished value");
    }
    Editing editing;
    expect(editing.controller().display_language() == DisplayLanguage::japanese,
           "the default settings (system) on a Japanese OS keep the Japanese display");
}

// `:set language=` の全値と保存（決定 8）。
void verify_set_language_values()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    using Case =
        std::tuple<std::string_view, DisplayLanguageChoice, DisplayLanguage, std::string_view>;
    const std::array<Case, 4> cases{{
        {"en", DisplayLanguageChoice::english, DisplayLanguage::english, "language=en"},
        {"zh-hans", DisplayLanguageChoice::simplified_chinese, DisplayLanguage::simplified_chinese,
         "language=zh-hans"},
        {"system", DisplayLanguageChoice::system, DisplayLanguage::japanese,
         "language=system (ja)"},
        {"ja", DisplayLanguageChoice::japanese, DisplayLanguage::japanese, "language=ja"},
    }};
    std::size_t observed = editing.ui_language().calls();
    for (const auto &[name, choice, language, shown] : cases)
    {
        const auto frame = run_ex(controller, "set language=" + std::string(name));
        if (choice == DisplayLanguageChoice::system)
        {
            ++observed;
        }
        expect(controller.display_language() == language && frame.language == language &&
                   message_of(frame) == shown,
               ":set language= resolves, places the language and echoes the wished value");
        expect(editing.settings()
                       .written()
                       .value_or(settings_with(DisplayLanguageChoice::german))
                       .ui_language == choice,
               ":set language= saves ui_language through the settings entry");
        expect(editing.ui_language().calls() == observed,
               ":set language= observes the OS language only when system is chosen");
    }
    static_cast<void>(ex_message(controller, "set language=en"));
    editing.ui_language().set(DisplayLanguage::simplified_chinese);
    expect(ex_message(controller, "set language&") == "language=system (zh-hans)" &&
               controller.display_language() == DisplayLanguage::simplified_chinese &&
               editing.ui_language().calls() == observed + 1 &&
               editing.settings()
                       .written()
                       .value_or(settings_with(DisplayLanguageChoice::german))
                       .ui_language == DisplayLanguageChoice::system,
           ":set language& goes back to the default system, observing the OS once");
    const std::size_t written = editing.settings().writes();
    for (const std::string_view flag : {"set language!", "set invlanguage"})
    {
        expect(!ex_message(controller, std::string(flag)).starts_with("language=") &&
                   controller.display_language() == DisplayLanguage::simplified_chinese &&
                   editing.settings().writes() == written,
               "language is not a flag, so ! and inv are refused as unknown options (E518)");
    }
}

// `:set language?`・断り・保存の失敗（決定 8）。
void verify_set_language_answers()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(ex_message(controller, "set language=system"));
    editing.ui_language().set(DisplayLanguage::english);
    const std::size_t observed = editing.ui_language().calls();
    const std::size_t asked = editing.settings().writes();
    expect(ex_message(controller, "set language?") == "language=system (ja)" &&
               controller.display_language() == DisplayLanguage::japanese &&
               editing.ui_language().calls() == observed && editing.settings().writes() == asked,
           ":set language? echoes the wished value and the resolved one without observing or "
           "saving");
    expect(ex_message(controller, "set language=system") == "language=system (en)" &&
               controller.display_language() == DisplayLanguage::english,
           "choosing system again observes the OS and follows it");
    static_cast<void>(ex_message(controller, "set language=ja"));
    const std::size_t writes = editing.settings().writes();
    for (const std::string_view refused : {"ko", "es", "de", "fr", ""})
    {
        expect(ex_message(controller, "set language=" + std::string(refused)).starts_with("E474") &&
                   controller.display_language() == DisplayLanguage::japanese,
               ":set language= refuses the languages without a column with E474");
    }
    expect(editing.settings().writes() == writes, "a refused :set language= writes nothing");
    editing.settings().fail(app::SettingsIssue{app::SettingsFailure::unwritable});
    expect(ex_message(controller, "set language=en") == "Settings could not be saved" &&
               controller.display_language() == DisplayLanguage::japanese,
           "an unsaved :set language= is refused and keeps the language");
    editing.settings().fail(std::nullopt);
}

// 操作「表示言語を選ぶ」と Ctrl+P の `:` の候補（決定 8）。候補は 4 つで、説明は言語の自称と
// 「Windows に合わせる」の表の文。
void verify_language_list()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::OpenDisplayLanguageList{});
    auto frame = controller.frame();
    expect(frame.command_palette.has_value() && frame.command_palette.value().total == 4,
           "the operation opens the palette with the four language candidates");
    constexpr std::array<std::string_view, 4> commands{
        "set language=ja", "set language=en", "set language=zh-hans", "set language=system"};
    constexpr std::array<std::string_view, 4> details{"日本語", "English", "简体中文",
                                                      "Windows に合わせる"};
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        const auto [command, detail] = palette_row(frame, index);
        expect(command == commands.at(index) && detail == details.at(index),
               "each language candidate carries its endonym or the Windows note");
    }
    applied(controller, app::SubmitCommand{});
    expect(controller.display_language() == DisplayLanguage::japanese &&
               !controller.frame().command_palette.has_value(),
           "choosing the first candidate runs :set language=ja and closes the palette");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(ex_message(controller, "set language=en"));
    applied(controller, app::OpenDisplayLanguageList{});
    frame = controller.frame();
    expect(palette_row(frame, 3).second == "Match Windows",
           "the Windows note follows the display language");
    applied(controller, app::OpenOperationList{});
    frame = controller.frame();
    expect(frame.command_palette.has_value() && !frame.command_palette.value().rows.empty() &&
               frame.command_palette.value().rows.at(0).label.text() ==
                   core::ui_text(UiMessage::operation_open_file_name, DisplayLanguage::english),
           "the F1 list shows the operation names in the display language");
}

// 切り替えで frame の文が変わる（決定 3・6）: ステータスバー・行と桁・無題のタブ（脇のタブも）・
// 案内・保存の問い。
void verify_frame_follows_language()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::NewTab{});
    static_cast<void>(ex_message(controller, "set language=en"));
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    const auto frame = controller.frame();
    expect(frame.mode_label == "Basic", "the ordinary mode label is Basic in English");
    expect(frame.status_items.at(0).text() == core::ui_text_filled(UiMessage::status_line_column,
                                                                   DisplayLanguage::english,
                                                                   {"1", "1"}),
           "the caret position is the English text");
    expect(frame.tabs.size() == 2 && frame.tabs.at(0).title.text() == "Untitled" &&
               frame.tabs.at(1).title.text() == "Untitled",
           "the active and the parked untitled tabs follow the display language");
    expect(core::save_question_text(core::SaveQuestionText::unsaved_question,
                                    controller.display_language()) ==
               core::ui_text(UiMessage::dialog_save_question, DisplayLanguage::english),
           "the save question is asked in the display language");
}

// 切り替えで知らせの文が変わる（決定 6）。
void verify_notices_follow_language()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(ex_message(controller, "set language=en"));
    applied(controller, app::InsertDateTime{});
    expect(message_of(controller.frame()) ==
               core::ui_text(UiMessage::notice_date_time_insert_only, DisplayLanguage::english),
           "the date and time notice is in English");
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    applied(controller, app::ToggleBookmark{});
    expect(message_of(controller.frame()) ==
               core::ui_text(UiMessage::notice_bookmark_needs_saved_file, DisplayLanguage::english),
           "a bookmark notice is in English");
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    expect(message_of(controller.frame()) ==
               core::ui_text(UiMessage::notice_pane_already_split, DisplayLanguage::english),
           "a pane notice is in English");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(ex_message(controller, "set language=zh-hans"));
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    applied(controller, app::ToggleBookmark{});
    expect(message_of(controller.frame()) ==
               core::ui_text(UiMessage::notice_bookmark_needs_saved_file,
                             DisplayLanguage::simplified_chinese),
           "a notice follows the switch to simplified Chinese");
    expect(core::ui_text_filled(UiMessage::notice_replay_limit, DisplayLanguage::japanese,
                                {core::grouped_decimal_text(100000)}) ==
               "再生を 100,000 鍵で止めました（残りは捨てました）",
           "the replay notice keeps the grouped Japanese number");
}

// 切り替えは本文・undo・レジスタを変えない（決定 3・状態の値が 1 つ変わるだけ）。
void verify_switch_keeps_editing()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{5});
    applied(controller, app::InsertText{"abc\ndef"});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(controller.press_vim_keys(vim_keys_of("ggyy")));
    const auto before = controller.frame();
    static_cast<void>(ex_message(controller, "set language=en"));
    const auto after = controller.frame();
    expect(body_of(controller) == "abc\ndef\n" &&
               after.caret.position.line.value == before.caret.position.line.value &&
               after.caret.position.column.value == before.caret.position.column.value,
           "switching the language keeps the text and the caret");
    static_cast<void>(controller.press_vim_keys(vim_keys_of("p")));
    expect(body_of(controller) == "abc\nabc\ndef\n", "switching keeps the unnamed register");
    static_cast<void>(controller.press_vim_keys(vim_keys_of("u")));
    expect(body_of(controller) == "abc\ndef\n", "switching keeps the undo history");
}

// port の回数（契約 verify_quiet_paths と同じ）: 起動の 1 回だけで、打鍵・タブ・一覧・問いは 0。
void verify_quiet_observation()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"typed"});
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    applied(controller, app::OpenOperationList{});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(controller.press_vim_keys(vim_keys_of("<Esc>ix<Esc>")));
    static_cast<void>(ex_message(controller, "set language?"));
    expect(editing.ui_language().calls() == 1,
           "typing, tabs, the lists and the language question never observe the OS again");
}

// 全文を 3 言語で埋めて置き場が残らない・行と桁の文は固定長の buffer に収まる。
void verify_filled_columns()
{
    for (const DisplayLanguage language :
         {DisplayLanguage::japanese, DisplayLanguage::english, DisplayLanguage::simplified_chinese})
    {
        for (std::size_t index = 0; index < core::ui_message_count; ++index)
        {
            const auto filled =
                core::ui_text_filled(static_cast<UiMessage>(index), language, {"a", "b"});
            expect(filled.find("{0}") == std::string::npos &&
                       filled.find("{1}") == std::string::npos,
                   "every text fills all of its placeholders");
        }
        expect(core::ui_text(UiMessage::status_line_column, language).size() <= 64U,
               "the caret text fits the fixed buffer of the status item");
        expect(
            !core::display_language_choice_detail(DisplayLanguageChoice::system, language).empty(),
            "the Windows note exists in every column");
    }
    expect(core::binary_size_text(std::size_t{64} * 1024U * 1024U) == "64 MiB" &&
               core::binary_size_text(std::size_t{1024} * 1024U * 1024U) == "1 GiB" &&
               core::binary_size_text(std::size_t{16} * 1024U) == "16 KiB" &&
               core::binary_size_text(1000U) == "1000 bytes",
           "binary sizes use the largest dividing unit");
    expect(core::grouped_decimal_text(100000) == "100,000" &&
               core::grouped_decimal_text(999) == "999" &&
               core::grouped_decimal_text(1234567) == "1,234,567",
           "grouped decimals put a comma every three digits");
}

// 面で fill の候補を選んだ後も言語を保つ（V-1601 の M1）: en で `set fontsize=` を選び、入力を空に
// して `?` を打つと F1 の一覧は英語の名前。
void verify_filled_palette_keeps_language()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(ex_message(controller, "set language=en"));
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{":fz"});
    auto frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.command_line.value_or(core::InputLineView{}).text == ":set fontsize=",
           "Enter on the fill candidate stages set fontsize=");
    for (std::size_t erased = 0; erased < std::string_view{":set fontsize="}.size(); ++erased)
    {
        applied(controller, app::EditCommand{core::CommandEdit::backspace});
    }
    applied(controller, app::CommandText{"?"});
    frame = controller.frame();
    expect(frame.command_palette.has_value() && !frame.command_palette.value().rows.empty() &&
               frame.command_palette.value().rows.at(0).label.text() ==
                   core::ui_text(UiMessage::operation_open_file_name, DisplayLanguage::english),
           "the palette keeps the display language after a fill candidate");
}

// 英語で F1 の一覧を名前と短い名前で絞る（V-1601 の L11）。
void verify_english_operation_filter()
{
    const auto names = [](std::string_view query)
    {
        std::vector<std::string> shown;
        for (const auto &choice :
             core::operation_choices(query, core::EditMode::ordinary, DisplayLanguage::english))
        {
            shown.emplace_back(choice.label.text());
        }
        return shown;
    };
    const auto saved = names("save");
    expect(saved.size() >= 2 && saved.at(0) == "Save" && saved.at(1) == "Save As",
           "the English names filter the F1 list (save gives Save and Save As first)");
    const auto help = names(
        core::ui_text(UiMessage::operation_list_operations_short_name, DisplayLanguage::english));
    expect(!help.empty() && help.front() == core::ui_text(UiMessage::operation_list_operations_name,
                                                          DisplayLanguage::english),
           "the English short name finds its operation");
    expect(names("ほぞん").empty(), "the Japanese readings are not searched in English");
}

// 列の無い希望値（ko）は英語に解決し、別の `:set` を保存しても設定から消えない（V-1601 の L11）。
void verify_columnless_choice_is_kept()
{
    Editing editing{SettingsReading{settings_with(DisplayLanguageChoice::korean)},
                    DisplayLanguage::japanese};
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    expect(controller.display_language() == DisplayLanguage::english &&
               ex_message(controller, "set language?") == "language=ko (en)",
           "ko resolves to English and the question names both");
    static_cast<void>(ex_message(controller, "set fontsize=18"));
    expect(editing.settings()
                   .written()
                   .value_or(settings_with(DisplayLanguageChoice::japanese))
                   .ui_language == DisplayLanguageChoice::korean,
           "saving another setting keeps ui_language=ko");
}
} // namespace

void verify_ui_language_scope()
{
    verify_startup_resolution();
    verify_set_language_values();
    verify_set_language_answers();
    verify_language_list();
    verify_frame_follows_language();
    verify_notices_follow_language();
    verify_switch_keeps_editing();
    verify_quiet_observation();
    verify_filled_columns();
    verify_filled_palette_keeps_language();
    verify_english_operation_filter();
    verify_columnless_choice_is_kept();
}
} // namespace nenenib::tests
