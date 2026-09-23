// scope `--user-theme-values` の単体テスト（ADR 0042 決定 2）。
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "DisplayText.hpp"
#include "OwnedThemeSource.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "ThemeDocument.hpp"
#include "ThemeName.hpp"

#include <string>

namespace nenenib::tests
{
namespace
{
using nenenib::core::BuiltinTheme;
using nenenib::core::DisplayText;
using nenenib::core::theme_of;

namespace core = nenenib::core;
} // namespace

void verify_user_theme_values()
{
    for (const auto text : {"a", "my-theme", "my_theme", "theme-2026"})
    {
        expect(core::ThemeName::parse(text).has_value(), "valid user theme identifier");
    }
    expect(core::ThemeName::parse("my_theme").value() == core::ThemeName::parse("my-theme").value(),
           "theme name has one canonical spelling");
    for (const auto text :
         {"", "1theme", "Theme", "-theme", "a/b", "a\\b", "a..b", "a:b", "a b", "界", "a\n"})
    {
        expect(!core::ThemeName::parse(text), "invalid or path-like theme names rejected");
    }
    expect(core::ThemeName::parse(std::string(64, 'a')).has_value(), "maximum theme name accepted");
    expect(!core::ThemeName::parse(std::string(65, 'a')), "oversized theme name rejected");
    const auto &theme = core::theme_of(core::BuiltinTheme::dracula);
    core::ThemeDocument document{core::ThemeName::parse("my-theme").value(), theme.appearance,
                                 theme.ui, theme.body,
                                 core::OwnedThemeSource{core::DisplayText::parse("作者").value(),
                                                        core::DisplayText::parse("MIT").value(),
                                                        core::DisplayText::parse("local").value()}};
    const auto copy = document;
    document.name = core::ThemeName::parse("changed").value();
    document.source.author = core::DisplayText::parse("changed").value();
    const auto view = core::theme_view(copy);
    expect(view.name == "my-theme" && view.source.author == "作者",
           "copied ThemeDocument owns strings independently");
    expect(view.source.license == "MIT" && view.source.url == "local",
           "Theme view borrows all attribution fields");
    expect(view.ui.background == theme.ui.background && view.body.keyword == theme.body.keyword,
           "Theme view preserves color values");
}
} // namespace nenenib::tests
