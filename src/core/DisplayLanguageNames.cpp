#include "DisplayLanguageNames.hpp"

#include "ResolvedLanguage.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// 名前を引く順（希望値の全 7 値）。
constexpr std::array<DisplayLanguageChoice, 7> all_display_language_choices{
    DisplayLanguageChoice::system,  DisplayLanguageChoice::japanese,
    DisplayLanguageChoice::english, DisplayLanguageChoice::simplified_chinese,
    DisplayLanguageChoice::korean,  DisplayLanguageChoice::spanish,
    DisplayLanguageChoice::german};
} // namespace

std::string_view display_language_choice_name(DisplayLanguageChoice choice) noexcept
{
    switch (choice)
    {
    case DisplayLanguageChoice::system:
        return "system";
    case DisplayLanguageChoice::japanese:
        return "ja";
    case DisplayLanguageChoice::english:
        return "en";
    case DisplayLanguageChoice::simplified_chinese:
        return "zh-hans";
    case DisplayLanguageChoice::korean:
        return "ko";
    case DisplayLanguageChoice::spanish:
        return "es";
    case DisplayLanguageChoice::german:
        return "de";
    }
    std::unreachable();
}

std::optional<DisplayLanguageChoice> display_language_choice_named(std::string_view name) noexcept
{
    for (const DisplayLanguageChoice choice : all_display_language_choices)
    {
        if (display_language_choice_name(choice) == name)
        {
            return choice;
        }
    }
    return std::nullopt;
}

std::string_view display_language_code(DisplayLanguage language) noexcept
{
    switch (language)
    {
    case DisplayLanguage::japanese:
        return display_language_choice_name(DisplayLanguageChoice::japanese);
    case DisplayLanguage::english:
        return display_language_choice_name(DisplayLanguageChoice::english);
    case DisplayLanguage::simplified_chinese:
        return display_language_choice_name(DisplayLanguageChoice::simplified_chinese);
    }
    std::unreachable();
}

std::string_view display_language_endonym(DisplayLanguage language) noexcept
{
    switch (language)
    {
    case DisplayLanguage::japanese:
        return "日本語";
    case DisplayLanguage::english:
        return "English";
    case DisplayLanguage::simplified_chinese:
        return "简体中文";
    }
    std::unreachable();
}

std::string_view display_language_choice_detail(DisplayLanguageChoice choice,
                                                DisplayLanguage language) noexcept
{
    if (choice == DisplayLanguageChoice::system)
    {
        return ui_text(UiMessage::language_choice_system, language);
    }
    // 列のある言語はそのまま写る（observed は system のときだけ読まれる）。
    return display_language_endonym(resolved_language(choice, std::nullopt));
}
} // namespace nenenib::core
