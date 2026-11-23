// Issue #1019 の T1a（ADR 0123 の追補 2026-11-13 の決定 1〜5）。表示言語の文の表と読む口:
// 表の網羅と置き場の一致・ui_text_filled の埋め方・resolved_language の全組み合わせ・日本語の列が
// 今の製品の文（既存の関数の戻り値）と同じこと・状態の既定と with_display_language・frame。
#include "Appearance.hpp"
#include "CommandChoice.hpp"
#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorOperation.hpp"
#include "EditorState.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"
#include "ModeLabel.hpp"
#include "OperationTexts.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "ResolvedLanguage.hpp"
#include "SaveQuestionText.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "StatusItems.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "UiTextsEnglish.hpp"
#include "UiTextsJapanese.hpp"
#include "UiTextsSimplifiedChinese.hpp"
#include "VimMode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

constexpr std::array<core::DisplayLanguage, core::display_language_count> languages{
    core::DisplayLanguage::japanese, core::DisplayLanguage::english,
    core::DisplayLanguage::simplified_chinese};

// 文の置き場 {0}〜{9} の集合（ビット i が {i}）。
[[nodiscard]] unsigned placeholder_bits(std::string_view text)
{
    unsigned bits = 0;
    for (std::size_t at = 0; at + 2 < text.size(); ++at)
    {
        if (text[at] == '{' && text[at + 2] == '}' && text[at + 1] >= '0' && text[at + 1] <= '9')
        {
            bits |= 1U << static_cast<unsigned>(text[at + 1] - '0');
        }
    }
    return bits;
}

void verify_columns()
{
    expect(core::ui_message_count == core::ui_texts_japanese.size() &&
               core::ui_message_count == core::ui_texts_english.size() &&
               core::ui_message_count == core::ui_texts_simplified_chinese.size(),
           "every language column has one row per UiMessage");
    std::size_t placed = 0;
    for (std::size_t index = 0; index < core::ui_message_count; ++index)
    {
        const auto message = static_cast<core::UiMessage>(index);
        const unsigned bits =
            placeholder_bits(core::ui_text(message, core::DisplayLanguage::japanese));
        placed += bits != 0 ? 1U : 0U;
        for (const core::DisplayLanguage language : languages)
        {
            const std::string_view text = core::ui_text(message, language);
            expect(!text.empty() && placeholder_bits(text) == bits,
                   "every text is non-empty and has the Japanese placeholders");
        }
    }
    expect(core::ui_message_count == 399,
           "the table has 399 messages (T1a 197, T1b 4, the About operation 2 of D90, the "
           "project search mark 1 of #1003, the project search face 8 of #1763, the "
           "easymotion prompt of ADR 0163, the 5 context menu operations 10 of #1051, the finder "
           "preview 4 of #1785, the preview failures 3 more of #1793, the packaged "
           "registration notice 1 of #1809, the completion operation 2 and notice 1 of "
           "#1833, the language queries 9 of #1834, the settings panel 54 of #1828, its screen "
           "4 of #1836, the screen fixes 8 of #1836 and the tab menu 5 operations 10 and "
           "notices 3 of #1790, the link card 17 of #1869, its heading notice 1 and the "
           "language server start notices 3 of #1897, the diagnostics 4 operations 8 and "
           "the mark and notices 3 of #1939, the 3 pane size operations 6 of #1941, the "
           "language server rows 12 of #1945 less the command line note 1, the language "
           "server log 4 of #1942, the caret motion rows 4 of #1933, the references "
           "operation 2 and the mark and notices 4 of #1947, the symbols operation 2 and "
           "the mark and notices 3 of #1955, the symbols cut notice, the 3 empty lines and "
           "the references subject of #1958, the signature operation 2 and notices 2 of "
           "#1959)");
    expect(placed == 49, "49 messages take placeholders (T1a 26, the search progress 3, the "
                         "preview line 1, the definition count 1, the settings summary 4, "
                         "the settings no-match line 1, the link card hints 2 and the language "
                         "server start notices 3, the language server rows 2, the language "
                         "server starting 1, the references count 1 and the symbols count, "
                         "the 2 empty lines and the subject of #1958)");
    expect(core::ui_text(core::UiMessage::tab_untitled, core::DisplayLanguage::english) ==
               "Untitled",
           "the English column is read for english");
    expect(core::ui_text(core::UiMessage::dialog_save_button_discard,
                         core::DisplayLanguage::english) == "Do&n't Save",
           "English buttons use the Windows access keys");
    expect(
        core::ui_text(core::UiMessage::tab_untitled, core::DisplayLanguage::simplified_chinese) ==
            core::ui_texts_simplified_chinese.at(std::to_underlying(core::UiMessage::tab_untitled))
                .text,
        "the Chinese column is read for simplified_chinese");
}

void verify_filling()
{
    constexpr auto line = core::UiMessage::status_line_column;
    constexpr auto ja = core::DisplayLanguage::japanese;
    expect(core::ui_text_filled(line, ja, {"12", "34"}) == "行 12, 桁 34",
           "both placeholders are filled in order");
    expect(core::ui_text_filled(line, core::DisplayLanguage::english, {"12", "34"}) ==
               "Ln 12, Col 34",
           "the English sentence decides its own wording");
    expect(core::ui_text_filled(line, ja, {"12"}) == "行 12, 桁 {1}",
           "a placeholder without a value stays as it is");
    expect(core::ui_text_filled(line, ja, {}) == "行 {0}, 桁 {1}",
           "no values leave every placeholder");
    expect(core::ui_text_filled(line, ja, {"1", "2", "3"}) == "行 1, 桁 2",
           "extra values are dropped");
    expect(core::placeholders_filled("{0}-{1}-{0}", {"a", "b"}) == "a-b-a",
           "the same placeholder is filled every time");
    expect(core::placeholders_filled("{1} {0}", {"a", "b"}) == "b a",
           "placeholders follow their numbers, not their order");
    expect(core::placeholders_filled("{", {"a"}) == "{", "a lone brace stays");
    expect(core::placeholders_filled("a{0", {"x"}) == "a{0", "an unclosed placeholder stays");
    expect(core::placeholders_filled("{x}{}{0}", {"v"}) == "{x}{}v",
           "only a brace with one digit is a placeholder");
    expect(core::placeholders_filled("{0}", {"{1}"}) == "{1}",
           "filled values are not filled again");
    expect(core::placeholders_filled("", {"a"}).empty(), "an empty text stays empty");
}

void verify_resolution()
{
    using Choice = core::DisplayLanguageChoice;
    using Language = core::DisplayLanguage;
    constexpr std::array<std::optional<Language>, 4> observed{
        std::nullopt, Language::japanese, Language::english, Language::simplified_chinese};
    constexpr std::array<std::pair<Choice, std::optional<Language>>, 7> fixed{{
        {Choice::system, std::nullopt},
        {Choice::japanese, Language::japanese},
        {Choice::english, Language::english},
        {Choice::simplified_chinese, Language::simplified_chinese},
        {Choice::korean, Language::english},
        {Choice::spanish, Language::english},
        {Choice::german, Language::english},
    }};
    for (const auto &[choice, expected] : fixed)
    {
        for (const auto &seen : observed)
        {
            const Language wanted =
                expected.has_value() ? expected.value() : seen.value_or(Language::english);
            expect(core::resolved_language(choice, seen) == wanted,
                   "resolved_language follows the choice and reads the OS only for system");
        }
    }
}

[[nodiscard]] std::string_view japanese(core::UiMessage message)
{
    return core::ui_text(message, core::DisplayLanguage::japanese);
}

void verify_japanese_save_and_palette()
{
    using Text = core::SaveQuestionText;
    using Message = core::UiMessage;
    constexpr std::array<std::pair<Text, Message>, 5> questions{{
        {Text::unsaved_question, Message::dialog_save_question},
        {Text::rendering_lost_question, Message::dialog_save_question_rendering_lost},
        {Text::save_button, Message::dialog_save_button_save},
        {Text::discard_button, Message::dialog_save_button_discard},
        {Text::cancel_button, Message::dialog_save_button_cancel},
    }};
    for (const auto &[text, message] : questions)
    {
        expect(core::save_question_text(text, core::DisplayLanguage::japanese) == japanese(message),
               "the Japanese save question is the product text");
    }
    using Origin = core::PaletteOrigin;
    constexpr std::array<std::pair<Origin, Message>, 7> origins{{
        {Origin::tab, Message::palette_origin_tab},
        {Origin::bookmarked_tab, Message::palette_origin_bookmarked_tab},
        {Origin::bookmark, Message::label_bookmarks},
        {Origin::history, Message::label_history},
        {Origin::folder, Message::palette_origin_folder},
        {Origin::document_format, Message::palette_origin_document_format},
        {Origin::project_hit, Message::palette_mark_project},
    }};
    for (const auto &[origin, message] : origins)
    {
        expect(core::palette_origin_label(origin, nenenib::core::DisplayLanguage::japanese) ==
                   japanese(message),
               "the Japanese palette origin is the product text");
    }
    constexpr std::array<Message, 7> marks{
        Message::palette_mark_tabs,     Message::label_bookmarks,
        Message::label_history,         Message::palette_mark_folder,
        Message::palette_mark_settings, Message::palette_mark_operations,
        Message::palette_mark_project};
    for (std::size_t index = 0; index < marks.size(); ++index)
    {
        expect(core::palette_marks.at(index).name == marks.at(index),
               "the palette mark names its text in the table");
    }
}

void verify_japanese_status_and_operations()
{
    expect(core::mode_label(core::EditMode::ordinary, core::VimMode::normal, false,
                            nenenib::core::DisplayLanguage::japanese) ==
               japanese(core::UiMessage::status_mode_basic),
           "the Japanese basic mode label is the product text");
    expect(core::tab_title_for(std::nullopt, core::SaveState::saved,
                               nenenib::core::DisplayLanguage::japanese)
                   .text() == japanese(core::UiMessage::tab_untitled),
           "the Japanese untitled tab is the product text");
    const auto items = core::status_items_for(
        nenenib::core::caret_status_item(core::TextPosition{core::LineNumber{12}, core::Column{34}},
                                         nenenib::core::DisplayLanguage::japanese),
        core::TextEncoding::utf8, core::LineEnding::lf, core::Language::plain);
    expect(items.at(0).text() == core::ui_text_filled(core::UiMessage::status_line_column,
                                                      core::DisplayLanguage::japanese,
                                                      {"12", "34"}),
           "the filled Japanese caret position is the product text");
    // 操作の表は文の名前を持つ（T1b・決定 6）。38 操作は名前・説明・短い名前の順に enum の頭に並ぶ
    // （T1b の表示言語の操作も T1f で同じ群へ移した）。
    std::size_t index = 0;
    for (const core::OperationText &row : core::operation_texts)
    {
        expect(row.name == static_cast<core::UiMessage>(index) &&
                   row.description == static_cast<core::UiMessage>(index + 1),
               "the operation rows name their texts in the order of the table");
        index += 2;
        if (row.short_name.has_value())
        {
            expect(row.short_name.value() == static_cast<core::UiMessage>(index),
                   "the operation short name follows its description");
            ++index;
        }
    }
    expect(index == std::to_underlying(core::UiMessage::app_error_window_class),
           "the operation rows come first in the order of operation_texts");
}

void verify_state_and_frame()
{
    const auto state = app::EditorState::create(core::Appearance::dark, core::EditMode::ordinary);
    expect(state.display_language() == core::DisplayLanguage::japanese,
           "the state starts with Japanese (the current display)");
    const auto english = state.with_display_language(core::DisplayLanguage::english);
    expect(english.display_language() == core::DisplayLanguage::english &&
               state.display_language() == core::DisplayLanguage::japanese,
           "with_display_language returns a new state and keeps the old one");
    expect(english.with_mode(core::EditMode::vim).display_language() ==
               core::DisplayLanguage::english,
           "other with_* keep the display language");
    Editing editor;
    expect(editor.controller().frame().language == core::DisplayLanguage::japanese,
           "the frame carries the state's display language");
}

// T1d（#1595）: 中国語の列に英語の写しが残っていない。どの行も ASCII
// の外の字を含み、英語の行と違う。
void verify_chinese_translated()
{
    std::size_t untranslated = 0;
    for (std::size_t index = 0; index < core::ui_message_count; ++index)
    {
        const auto message = static_cast<core::UiMessage>(index);
        const std::string_view chinese =
            core::ui_text(message, core::DisplayLanguage::simplified_chinese);
        const bool ascii_only = std::ranges::all_of(
            chinese, [](char unit) { return static_cast<unsigned char>(unit) < 0x80U; });
        const bool copied = chinese == core::ui_text(message, core::DisplayLanguage::english);
        untranslated += ascii_only || copied ? 1U : 0U;
    }
    expect(untranslated == 0, "every Chinese text is translated (no ASCII-only or English copy)");
    expect(core::ui_text(core::UiMessage::tab_untitled,
                         core::DisplayLanguage::simplified_chinese) == "无标题",
           "the Chinese untitled tab is the Notepad wording");
}
} // namespace

void verify_ui_texts_scope()
{
    verify_columns();
    verify_filling();
    verify_resolution();
    verify_japanese_save_and_palette();
    verify_japanese_status_and_operations();
    verify_state_and_frame();
    verify_chinese_translated();
}
} // namespace nenenib::tests
