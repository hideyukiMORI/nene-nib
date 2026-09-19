#pragma once

#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"
#include "Theme.hpp"
#include "ThemeDerivation.hpp"
#include "ThemeSource.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

// 組み込み 9 テーマの唯一の表（ADR 0017 の決定 6）。名前の表も同じ配列で、Ex の補完（C3）・
// Ctrl+P・設定の検証（C2）はここを引く（ARC-001）。
// 本文トークンは各テーマが公開している役割の割り当てから写し、割り当ての無い役割は同じテーマの
// 近い色で埋めて行のコメントに「埋めた」と書く。テーマの実装コードは写さない（決定 2）。
namespace nenenib::core
{
// ---------------------------------------------------------------- 出典（決定 2）

inline constexpr ThemeSource nib_source{
    .author = "NeNe Nib", .license = "MIT", .url = "https://github.com/hideyukiMORI/nene-nib"};

inline constexpr ThemeSource solarized_source{.author = "Ethan Schoonover",
                                              .license = "MIT",
                                              .url = "https://ethanschoonover.com/solarized/"};

inline constexpr ThemeSource monokai_source{
    .author = "Wimer Hazenberg",
    .license = "colors used; the name is attributed to the original author",
    .url = "https://github.com/textmate/monokai.tmbundle/blob/master/Themes/Monokai.tmTheme"};

inline constexpr ThemeSource dracula_source{.author = "Zeno Rocha and contributors",
                                            .license = "MIT",
                                            .url = "https://spec.draculatheme.com/"};

inline constexpr ThemeSource one_dark_source{
    .author = "Atom", .license = "MIT", .url = "https://github.com/atom/one-dark-syntax"};

inline constexpr ThemeSource owl_source{.author = "Sarah Drasner",
                                        .license = "MIT",
                                        .url = "https://github.com/sdras/night-owl-vscode-theme"};

// ---------------------------------------------------------------- 本文トークン（決定 1）

// 採用案の 2 つ。UI は採用案の表そのもので、本文は Ubuntu の端末が使う Tango の配色
// （tango.freedesktop.org の Icon Theme Guidelines の色表）から採った。採用案の前景 #EEEEEC は
// すでに Tango の Aluminium light である。本文の 16 個は新しい値なので C3 の絵で hide が見る。
inline constexpr SyntaxPalette ubuntu_aubergine_body{
    .foreground = RgbColor{0xEE, 0xEE, 0xEC},   // 採用案の text（Tango の白）
    .background = RgbColor{0x30, 0x0A, 0x24},   // 施主決定 D11 の茄子色
    .cursor = RgbColor{0xE9, 0x54, 0x20},       // Ubuntu 橙（採用案の accent）
    .selection = RgbColor{0xE9, 0x54, 0x20},    // 採用案の selection の色
    .current_line = RgbColor{0x3E, 0x1A, 0x32}, // 採用案の current_line
    .line_number = RgbColor{0x7A, 0x66, 0x75},  // 採用案の gutter
    .comment = RgbColor{0xB8, 0xA9, 0xB3},      // 採用案の muted
    .keyword = RgbColor{0xFC, 0xAF, 0x3E},      // Tango の Orange light
    .string = RgbColor{0x8A, 0xE2, 0x34},       // Tango の Chameleon light
    .number = RgbColor{0xAD, 0x7F, 0xA8},       // Tango の Plum light
    .type = RgbColor{0xFC, 0xE9, 0x4F},         // Tango の Butter light
    .function = RgbColor{0x72, 0x9F, 0xCF},     // Tango の Sky Blue light
    .constant = RgbColor{0xAD, 0x7F, 0xA8},     // Tango の Plum light
    .operators = RgbColor{0x34, 0xE2, 0xE2},    // Tango を端末が足した水色の明るい方
    .error = RgbColor{0xEF, 0x29, 0x29},        // Tango の Scarlet Red light
    .warning = RgbColor{0xFC, 0xAF, 0x3E},      // Tango の Orange light（埋めた）
};

inline constexpr SyntaxPalette neutral_light_body{
    .foreground = RgbColor{0x1B, 0x1F, 0x24},   // 採用案の text
    .background = RgbColor{0xF4, 0xF5, 0xF7},   // 採用案の background
    .cursor = RgbColor{0xE9, 0x54, 0x20},       // Ubuntu 橙（採用案の accent）
    .selection = RgbColor{0xE9, 0x54, 0x20},    // 採用案の selection の色
    .current_line = RgbColor{0xE6, 0xE8, 0xEC}, // 採用案の current_line
    .line_number = RgbColor{0x9A, 0xA3, 0xAD},  // 採用案の gutter
    .comment = RgbColor{0x5C, 0x65, 0x70},      // 採用案の muted
    .keyword = RgbColor{0xCE, 0x5C, 0x00},      // Tango の Orange dark
    .string = RgbColor{0x4E, 0x9A, 0x06},       // Tango の Chameleon dark
    .number = RgbColor{0x75, 0x50, 0x7B},       // Tango の Plum medium
    .type = RgbColor{0xC4, 0xA0, 0x00},         // Tango の Butter dark
    .function = RgbColor{0x34, 0x65, 0xA4},     // Tango の Sky Blue medium
    .constant = RgbColor{0x75, 0x50, 0x7B},     // Tango の Plum medium
    .operators = RgbColor{0x06, 0x98, 0x9A},    // Tango を端末が足した水色の暗い方
    .error = RgbColor{0xCC, 0x00, 0x00},        // Tango の Scarlet Red medium
    .warning = RgbColor{0xCE, 0x5C, 0x00},      // Tango の Orange dark（埋めた）
};

// Solarized。base と accent の 16 色と usage 表は出典のとおり。構文の役割は出典の vim 配色の
// 割り当て（Comment = base01 / Constant = cyan / Identifier = blue / Statement = green /
// Type = yellow / Special・Error = red）から写した。
inline constexpr SyntaxPalette solarized_dark_body{
    .foreground = RgbColor{0x83, 0x94, 0x96},   // base0（usage 表の body text）
    .background = RgbColor{0x00, 0x2B, 0x36},   // base03（usage 表の background）
    .cursor = RgbColor{0x83, 0x94, 0x96},       // base0（vim の Cursor の地色）
    .selection = RgbColor{0x07, 0x36, 0x42},    // base02（usage 表の background highlights）
    .current_line = RgbColor{0x07, 0x36, 0x42}, // base02（vim の CursorLine の地色）
    .line_number = RgbColor{0x58, 0x6E, 0x75},  // base01（vim の LineNr）
    .comment = RgbColor{0x58, 0x6E, 0x75},      // base01（usage 表の comments）
    .keyword = RgbColor{0x85, 0x99, 0x00},      // green（Statement）
    .string = RgbColor{0x2A, 0xA1, 0x98},       // cyan（Constant）
    .number = RgbColor{0x2A, 0xA1, 0x98},       // cyan（Constant）
    .type = RgbColor{0xB5, 0x89, 0x00},         // yellow（Type）
    .function = RgbColor{0x26, 0x8B, 0xD2},     // blue（Identifier）
    .constant = RgbColor{0x2A, 0xA1, 0x98},     // cyan（Constant）
    .operators = RgbColor{0x85, 0x99, 0x00},    // green（Operator は Statement へ・埋めた）
    .error = RgbColor{0xDC, 0x32, 0x2F},        // red（Error）
    .warning = RgbColor{0xCB, 0x4B, 0x16},      // orange（埋めた）
};

// ライトは出典の base の入れ替え（base03↔base3・base02↔base2・base01↔base1・base00↔base0）。
// usage 表の body text は base00 だが base3 との比が 4.13:1 で、ADR 0017 の決定 7 の 4.5 を
// 満たさない。強調用の base01（5.0:1）を本文の前景にしてある（Issue #52 の判断・報告済み）。
inline constexpr SyntaxPalette solarized_light_body{
    .foreground = RgbColor{0x58, 0x6E, 0x75},   // base01（base00 は 4.13:1 で落ちる）
    .background = RgbColor{0xFD, 0xF6, 0xE3},   // base3（usage 表の background）
    .cursor = RgbColor{0x65, 0x7B, 0x83},       // base00（vim の Cursor の地色）
    .selection = RgbColor{0xEE, 0xE8, 0xD5},    // base2（usage 表の background highlights）
    .current_line = RgbColor{0xEE, 0xE8, 0xD5}, // base2（vim の CursorLine の地色）
    .line_number = RgbColor{0x93, 0xA1, 0xA1},  // base1（vim の LineNr）
    .comment = RgbColor{0x93, 0xA1, 0xA1},      // base1（usage 表の comments）
    .keyword = RgbColor{0x85, 0x99, 0x00},      // green（Statement）
    .string = RgbColor{0x2A, 0xA1, 0x98},       // cyan（Constant）
    .number = RgbColor{0x2A, 0xA1, 0x98},       // cyan（Constant）
    .type = RgbColor{0xB5, 0x89, 0x00},         // yellow（Type）
    .function = RgbColor{0x26, 0x8B, 0xD2},     // blue（Identifier）
    .constant = RgbColor{0x2A, 0xA1, 0x98},     // cyan（Constant）
    .operators = RgbColor{0x85, 0x99, 0x00},    // green（Operator は Statement へ・埋めた）
    .error = RgbColor{0xDC, 0x32, 0x2F},        // red（Error）
    .warning = RgbColor{0xCB, 0x4B, 0x16},      // orange（埋めた）
};

// Monokai。出典の原作 tmTheme の global（background / foreground / caret / selection /
// lineHighlight）と scope ごとの割り当てから写した。
inline constexpr SyntaxPalette monokai_body{
    .foreground = RgbColor{0xF8, 0xF8, 0xF2},   // foreground
    .background = RgbColor{0x27, 0x28, 0x22},   // background
    .cursor = RgbColor{0xF8, 0xF8, 0xF0},       // caret
    .selection = RgbColor{0x49, 0x48, 0x3E},    // selection
    .current_line = RgbColor{0x49, 0x48, 0x3E}, // lineHighlight
    .line_number = RgbColor{0x75, 0x71, 0x5E},  // comment の色（行番号の指定が無い・埋めた）
    .comment = RgbColor{0x75, 0x71, 0x5E},      // comment
    .keyword = RgbColor{0xF9, 0x26, 0x72},      // keyword / storage
    .string = RgbColor{0xE6, 0xDB, 0x74},       // string
    .number = RgbColor{0xAE, 0x81, 0xFF},       // constant.numeric
    .type = RgbColor{0x66, 0xD9, 0xEF},         // storage.type / support.type
    .function = RgbColor{0xA6, 0xE2, 0x2E},     // entity.name.function
    .constant = RgbColor{0xAE, 0x81, 0xFF},     // constant.language
    .operators = RgbColor{0xF9, 0x26, 0x72},    // keyword の色（指定が無い・埋めた）
    .error = RgbColor{0xF9, 0x26, 0x72},        // invalid の地色
    .warning = RgbColor{0xFD, 0x97, 0x1F},      // variable.parameter の橙（埋めた）
};

// Dracula。出典の spec の Color Palette と役割（Keyword = Pink / String = Yellow /
// Constant = Purple / Types・ClassName = Cyan / FunctionNames = Green）から写した。
inline constexpr SyntaxPalette dracula_body{
    .foreground = RgbColor{0xF8, 0xF8, 0xF2},   // Foreground
    .background = RgbColor{0x28, 0x2A, 0x36},   // Background
    .cursor = RgbColor{0xF8, 0xF8, 0xF2},       // Foreground（カーソルの指定が無い・埋めた）
    .selection = RgbColor{0x44, 0x47, 0x5A},    // Selection
    .current_line = RgbColor{0x44, 0x47, 0x5A}, // Selection と同じ（指定が無い・埋めた）
    .line_number = RgbColor{0x62, 0x72, 0xA4},  // Comment（行番号の指定が無い・埋めた）
    .comment = RgbColor{0x62, 0x72, 0xA4},      // Comment
    .keyword = RgbColor{0xFF, 0x79, 0xC6},      // Keyword（Pink）
    .string = RgbColor{0xF1, 0xFA, 0x8C},       // String（Yellow）
    .number = RgbColor{0xBD, 0x93, 0xF9},       // Constant（Purple）
    .type = RgbColor{0x8B, 0xE9, 0xFD},         // Types / ClassName（Cyan）
    .function = RgbColor{0x50, 0xFA, 0x7B},     // FunctionNames（Green）
    .constant = RgbColor{0xBD, 0x93, 0xF9},     // Constant（Purple）
    .operators = RgbColor{0xFF, 0x79, 0xC6},    // SeparatorsReferencesOrAccessors（Pink）
    .error = RgbColor{0xFF, 0x55, 0x55},        // Invalid の地色（Red）
    .warning = RgbColor{0xFF, 0xB8, 0x6C},      // Orange（埋めた）
};

// One Dark。出典の colors.less の HSL を sRGB に直した値と、syntax-variables.less /
// syntax/_base.less の割り当てから写した。
inline constexpr SyntaxPalette one_dark_body{
    .foreground = RgbColor{0xAB, 0xB2, 0xBF},   // mono-1（syntax-fg）
    .background = RgbColor{0x28, 0x2C, 0x34},   // syntax-bg
    .cursor = RgbColor{0x52, 0x8B, 0xFF},       // syntax-accent（syntax-cursor-color）
    .selection = RgbColor{0x3E, 0x44, 0x51},    // lighten(syntax-bg, 10%)
    .current_line = RgbColor{0x2C, 0x32, 0x3C}, // syntax-cursor-line を背景に重ねた値（埋めた）
    .line_number = RgbColor{0x63, 0x6D, 0x83},  // syntax-gutter（darken(syntax-fg, 26%)）
    .comment = RgbColor{0x5C, 0x63, 0x70},      // mono-3
    .keyword = RgbColor{0xC6, 0x78, 0xDD},      // hue-3
    .string = RgbColor{0x98, 0xC3, 0x79},       // hue-4
    .number = RgbColor{0xD1, 0x9A, 0x66},       // hue-6（constant.numeric）
    .type = RgbColor{0xE5, 0xC0, 0x7B},         // hue-6-2（entity.name.type）
    .function = RgbColor{0x61, 0xAF, 0xEF},     // hue-2（syntax-color-function）
    .constant = RgbColor{0xD1, 0x9A, 0x66},     // hue-6（syntax-color-constant）
    .operators = RgbColor{0xAB, 0xB2, 0xBF},    // mono-1（keyword.operator）
    .error = RgbColor{0xE0, 0x52, 0x52},        // syntax-color-removed（illegal の地色）
    .warning = RgbColor{0xE0, 0xC2, 0x85},      // syntax-color-modified（deprecated の地色）
};

// Night Owl。出典の VS Code テーマの colors と tokenColors から写した
// （entity.name.function は後から上書きされる #82AAFF が効く）。
inline constexpr SyntaxPalette night_owl_body{
    .foreground = RgbColor{0xD6, 0xDE, 0xEB},   // editor.foreground
    .background = RgbColor{0x01, 0x16, 0x27},   // editor.background
    .cursor = RgbColor{0x80, 0xA4, 0xC2},       // editorCursor.foreground
    .selection = RgbColor{0x1D, 0x3B, 0x53},    // editor.selectionBackground
    .current_line = RgbColor{0x07, 0x24, 0x35}, // lineHighlight を背景に重ねた値（埋めた）
    .line_number = RgbColor{0x4B, 0x64, 0x79},  // editorLineNumber.foreground
    .comment = RgbColor{0x63, 0x77, 0x77},      // comment
    .keyword = RgbColor{0xC7, 0x92, 0xEA},      // keyword / storage
    .string = RgbColor{0xEC, 0xC4, 0x8D},       // string
    .number = RgbColor{0xF7, 0x8C, 0x6C},       // constant.numeric
    .type = RgbColor{0xFF, 0xCB, 0x8B},         // entity.name.class
    .function = RgbColor{0x82, 0xAA, 0xFF},     // entity.name.function
    .constant = RgbColor{0x82, 0xAA, 0xFF},     // constant.language
    .operators = RgbColor{0x7F, 0xDB, 0xCA},    // keyword.operator
    .error = RgbColor{0xEF, 0x53, 0x50},        // editorError.foreground
    .warning = RgbColor{0xB3, 0x95, 0x54},      // editorWarning.foreground
};

// Light Owl（Night Owl Light）。同じ出典のライト版から写した。
inline constexpr SyntaxPalette night_owl_light_body{
    .foreground = RgbColor{0x40, 0x3F, 0x53},   // editor.foreground
    .background = RgbColor{0xFB, 0xFB, 0xFB},   // editor.background
    .cursor = RgbColor{0x90, 0xA7, 0xB2},       // editorCursor.foreground
    .selection = RgbColor{0xE0, 0xE0, 0xE0},    // editor.selectionBackground
    .current_line = RgbColor{0xF0, 0xF0, 0xF0}, // editor.lineHighlightBackground
    .line_number = RgbColor{0x90, 0xA7, 0xB2},  // editorLineNumber.foreground
    .comment = RgbColor{0x98, 0x9F, 0xB1},      // comment
    .keyword = RgbColor{0x99, 0x4C, 0xC3},      // keyword / storage
    .string = RgbColor{0x48, 0x76, 0xD6},       // string
    .number = RgbColor{0xAA, 0x09, 0x82},       // constant.numeric
    .type = RgbColor{0x11, 0x11, 0x11},         // entity.name.class
    .function = RgbColor{0x48, 0x76, 0xD6},     // entity.name.function
    .constant = RgbColor{0x48, 0x76, 0xD6},     // constant.language
    .operators = RgbColor{0x0C, 0x96, 0x9B},    // keyword.operator
    .error = RgbColor{0xE6, 0x4D, 0x49},        // editorError.foreground
    .warning = RgbColor{0xDA, 0xAA, 0x01},      // editorWarning.foreground
};

// ---------------------------------------------------------------- アクセント（決定 4）

// derive_ui に渡す各テーマの代表色。出典の中で「そのテーマだと分かる」常用色を 1 つ選ぶ。
inline constexpr RgbColor solarized_accent{0x26, 0x8B, 0xD2}; // blue（Identifier の色）
inline constexpr RgbColor monokai_accent{0xA6, 0xE2, 0x2E};   // green（関数名の色）
inline constexpr RgbColor dracula_accent{0xBD, 0x93, 0xF9};   // purple（Dracula の看板色）
inline constexpr RgbColor one_dark_accent{0x61, 0xAF, 0xEF};  // hue-2 blue（関数名の色）
inline constexpr RgbColor night_owl_accent{0x82, 0xAA, 0xFF}; // blue（関数名の色）
inline constexpr RgbColor light_owl_accent{0x48, 0x76, 0xD6}; // blue（関数名の色）

// ---------------------------------------------------------------- 9 テーマの表（決定 6）

// 並びは BuiltinTheme の並びそのもの（下の static_assert が対応を機械で留める）。
inline constexpr std::array<Theme, 9> builtin_themes{{
    {.name = "ubuntu-aubergine",
     .appearance = Appearance::dark,
     .ui = ubuntu_aubergine_palette,
     .body = ubuntu_aubergine_body,
     .source = nib_source},
    {.name = "neutral-light",
     .appearance = Appearance::light,
     .ui = neutral_light_palette,
     .body = neutral_light_body,
     .source = nib_source},
    {.name = "solarized-dark",
     .appearance = Appearance::dark,
     .ui = derive_ui(solarized_dark_body.background, solarized_dark_body.foreground,
                     solarized_accent, Appearance::dark),
     .body = solarized_dark_body,
     .source = solarized_source},
    {.name = "solarized-light",
     .appearance = Appearance::light,
     .ui = derive_ui(solarized_light_body.background, solarized_light_body.foreground,
                     solarized_accent, Appearance::light),
     .body = solarized_light_body,
     .source = solarized_source},
    {.name = "monokai",
     .appearance = Appearance::dark,
     .ui = derive_ui(monokai_body.background, monokai_body.foreground, monokai_accent,
                     Appearance::dark),
     .body = monokai_body,
     .source = monokai_source},
    {.name = "dracula",
     .appearance = Appearance::dark,
     .ui = derive_ui(dracula_body.background, dracula_body.foreground, dracula_accent,
                     Appearance::dark),
     .body = dracula_body,
     .source = dracula_source},
    {.name = "one-dark",
     .appearance = Appearance::dark,
     .ui = derive_ui(one_dark_body.background, one_dark_body.foreground, one_dark_accent,
                     Appearance::dark),
     .body = one_dark_body,
     .source = one_dark_source},
    {.name = "night-owl",
     .appearance = Appearance::dark,
     .ui = derive_ui(night_owl_body.background, night_owl_body.foreground, night_owl_accent,
                     Appearance::dark),
     .body = night_owl_body,
     .source = owl_source},
    {.name = "night-owl-light",
     .appearance = Appearance::light,
     .ui = derive_ui(night_owl_light_body.background, night_owl_light_body.foreground,
                     light_owl_accent, Appearance::light),
     .body = night_owl_light_body,
     .source = owl_source},
}};

// 表と enum は同じ添字で引く（ARC-001）。
[[nodiscard]] constexpr const Theme &theme_of(BuiltinTheme theme) noexcept
{
    return builtin_themes[static_cast<std::size_t>(theme)];
}

// OS の外観から既定テーマへ写す唯一の経路（ADR 0020）。
[[nodiscard]] constexpr BuiltinTheme theme_for(Appearance appearance) noexcept
{
    switch (appearance)
    {
    case Appearance::light:
        return BuiltinTheme::neutral_light;
    case Appearance::dark:
        return BuiltinTheme::ubuntu_aubergine;
    }
    std::unreachable();
}

static_assert(theme_of(BuiltinTheme::ubuntu_aubergine).name == "ubuntu-aubergine");
static_assert(theme_of(BuiltinTheme::neutral_light).name == "neutral-light");
static_assert(theme_of(BuiltinTheme::solarized_dark).name == "solarized-dark");
static_assert(theme_of(BuiltinTheme::solarized_light).name == "solarized-light");
static_assert(theme_of(BuiltinTheme::monokai).name == "monokai");
static_assert(theme_of(BuiltinTheme::dracula).name == "dracula");
static_assert(theme_of(BuiltinTheme::one_dark).name == "one-dark");
static_assert(theme_of(BuiltinTheme::night_owl).name == "night-owl");
static_assert(theme_of(BuiltinTheme::night_owl_light).name == "night-owl-light");

// 名前は小文字ハイフンで、`_` も同じ名前として受ける（決定 6）。正規化はここ 1 か所。
// 大文字小文字は区別する（名前は小文字だけ）。
[[nodiscard]] constexpr bool same_theme_name(std::string_view candidate,
                                             std::string_view name) noexcept
{
    if (candidate.size() != name.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        const char letter = candidate[index] == '_' ? '-' : candidate[index];
        if (letter != name[index])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr std::optional<BuiltinTheme> theme_named(std::string_view name) noexcept
{
    for (std::size_t index = 0; index < builtin_themes.size(); ++index)
    {
        if (same_theme_name(name, builtin_themes[index].name))
        {
            return static_cast<BuiltinTheme>(index);
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
