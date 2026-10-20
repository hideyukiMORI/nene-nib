#pragma once

#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemePalettes.hpp"
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

// 組み込み 125 テーマの唯一の表（ADR 0017 の決定 6）。名前の表も同じ配列で、Ex の補完（C3）・
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

// ---------------------------------------------------------------- 125 テーマの表（決定 6）

// 並びは BuiltinTheme の並びそのもの（下の static_assert が対応を機械で留める）。
inline constexpr std::array<Theme, 125> builtin_themes{{
    {.name = "ubuntu-aubergine",
     .appearance = Appearance::dark,
     .ui = with_gutter(ubuntu_aubergine_palette, RgbColor{0x27, 0x08, 0x1D},
                       RgbColor{0x8B, 0x79, 0x86}),
     .body = ubuntu_aubergine_body,
     .source = nib_source},
    {.name = "neutral-light",
     .appearance = Appearance::light,
     .ui =
         with_gutter(neutral_light_palette, RgbColor{0xE6, 0xE9, 0xEE}, RgbColor{0x63, 0x69, 0x6F}),
     .body = neutral_light_body,
     .source = nib_source},
    {.name = "solarized-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(solarized_dark_body.background, solarized_dark_body.foreground,
                                 solarized_accent, Appearance::dark),
                       RgbColor{0x00, 0x23, 0x2C}, RgbColor{0x77, 0x89, 0x8F}),
     .body = solarized_dark_body,
     .source = solarized_source},
    {.name = "solarized-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(solarized_light_body.background, solarized_light_body.foreground,
                                 solarized_accent, Appearance::light),
                       RgbColor{0xFD, 0xFA, 0xEF}, RgbColor{0x6A, 0x74, 0x74}),
     .body = solarized_light_body,
     .source = solarized_source},
    {.name = "monokai",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(monokai_body.background, monokai_body.foreground, monokai_accent,
                                 Appearance::dark),
                       RgbColor{0x20, 0x20, 0x1C}, RgbColor{0x8B, 0x87, 0x77}),
     .body = monokai_body,
     .source = monokai_source},
    {.name = "dracula",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(dracula_body.background, dracula_body.foreground, dracula_accent,
                                 Appearance::dark),
                       RgbColor{0x20, 0x22, 0x2C}, RgbColor{0x7B, 0x88, 0xB2}),
     .body = dracula_body,
     .source = dracula_source},
    {.name = "one-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(one_dark_body.background, one_dark_body.foreground,
                                 one_dark_accent, Appearance::dark),
                       RgbColor{0x20, 0x24, 0x2A}, RgbColor{0x83, 0x8B, 0x9D}),
     .body = one_dark_body,
     .source = one_dark_source},
    {.name = "night-owl",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(night_owl_body.background, night_owl_body.foreground,
                                 night_owl_accent, Appearance::dark),
                       RgbColor{0x01, 0x12, 0x20}, RgbColor{0x6B, 0x7F, 0x91}),
     .body = night_owl_body,
     .source = owl_source},
    {.name = "night-owl-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(night_owl_light_body.background, night_owl_light_body.foreground,
                                 light_owl_accent, Appearance::light),
                       RgbColor{0xFC, 0xFC, 0xFC}, RgbColor{0x67, 0x77, 0x7F}),
     .body = night_owl_light_body,
     .source = owl_source},
    {.name = "blue",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(blue_body.background, blue_body.foreground,
                                 RgbColor{0xBC, 0xBC, 0xBC}, Appearance::dark),
                       RgbColor{0x00, 0x00, 0x6C}, blue_body.line_number),
     .body = blue_body,
     .source = {.author = "Original author Steven Vertigan <steven@vertigan.wattle.id.au>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/blue.vim"}},
    {.name = "catppuccin",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(catppuccin_body.background, catppuccin_body.foreground,
                                 RgbColor{0xF2, 0xCD, 0xCD}, Appearance::dark),
                       RgbColor{0x18, 0x18, 0x25}, catppuccin_body.line_number),
     .body = catppuccin_body,
     .source = {.author = "The Catppuccin Community <catppuccin.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/catppuccin.vim"}},
    {.name = "darkblue",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(darkblue_body.background, darkblue_body.foreground,
                                 RgbColor{0x90, 0xFF, 0xF0}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x4D}, darkblue_body.line_number),
     .body = darkblue_body,
     .source = {.author = "Original author Bohdan Vlasyuk <bohdan@vstu.edu.ua>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/darkblue.vim"}},
    {.name = "default",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(vim_default_body.background, vim_default_body.foreground,
                                 RgbColor{0x40, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x10, 0x10, 0x10}, vim_default_body.line_number),
     .body = vim_default_body,
     .source = {.author = "Vim contributors",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/default.vim"}},
    {.name = "delek",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(delek_body.background, delek_body.foreground,
                                 RgbColor{0x00, 0x8B, 0x8B}, Appearance::light),
                       RgbColor{0xF0, 0xF0, 0xF0}, delek_body.line_number),
     .body = delek_body,
     .source = {.author = "Original author David Schweikert <david@schweikert.ch>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/delek.vim"}},
    {.name = "desert",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(desert_body.background, desert_body.foreground,
                                 RgbColor{0x89, 0xFB, 0x98}, Appearance::dark),
                       RgbColor{0x29, 0x29, 0x29}, desert_body.line_number),
     .body = desert_body,
     .source = {.author = "Original author Hans Fugal <hans@fugal.net>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/desert.vim"}},
    {.name = "elflord",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(elflord_body.background, elflord_body.foreground,
                                 RgbColor{0x40, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, elflord_body.line_number),
     .body = elflord_body,
     .source = {.author = "original author Ron Aaron <ron@ronware.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/elflord.vim"}},
    {.name = "evening",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(evening_body.background, evening_body.foreground,
                                 RgbColor{0x00, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x29, 0x29, 0x29}, evening_body.line_number),
     .body = evening_body,
     .source = {.author = "Original author Bram Moolenaar <Bram@vim.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/evening.vim"}},
    {.name = "habamax",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(habamax_body.background, habamax_body.foreground,
                                 RgbColor{0x87, 0xAF, 0xAF}, Appearance::dark),
                       RgbColor{0x17, 0x17, 0x17}, habamax_body.line_number),
     .body = habamax_body,
     .source = {.author = "Maxim Kim <habamax@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/habamax.vim"}},
    {.name = "industry",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(industry_body.background, industry_body.foreground,
                                 RgbColor{0xFF, 0x00, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, industry_body.line_number),
     .body = industry_body,
     .source = {.author = "Original author Shian Lee.",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/industry.vim"}},
    {.name = "koehler",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(koehler_body.background, koehler_body.foreground,
                                 RgbColor{0x40, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, koehler_body.line_number),
     .body = koehler_body,
     .source = {.author = "original author Ron Aaron <ron@ronware.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/koehler.vim"}},
    {.name = "lunaperche",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(lunaperche_body.background, lunaperche_body.foreground,
                                 RgbColor{0xE4, 0xE4, 0xE4}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, lunaperche_body.line_number),
     .body = lunaperche_body,
     .source = {.author = "Maxim Kim <habamax@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/lunaperche.vim"}},
    {.name = "morning",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(morning_body.background, morning_body.foreground,
                                 RgbColor{0x00, 0x87, 0x87}, Appearance::light),
                       RgbColor{0xE5, 0xE5, 0xE5}, morning_body.line_number),
     .body = morning_body,
     .source = {.author = "Original author Bram Moolenaar <Bram@vim.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/morning.vim"}},
    {.name = "murphy",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(murphy_body.background, murphy_body.foreground,
                                 RgbColor{0x00, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, murphy_body.line_number),
     .body = murphy_body,
     .source = {.author = "Original author Ron Aaron <ron@ronware.org>.",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/murphy.vim"}},
    {.name = "novum",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(novum_body.background, novum_body.foreground,
                                 RgbColor{0xA6, 0xDB, 0xFF}, Appearance::dark),
                       RgbColor{0x10, 0x12, 0x16}, novum_body.line_number),
     .body = novum_body,
     .source = {.author = "Charan Mandati <charancuz008@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/novum.vim"}},
    {.name = "pablo",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(pablo_body.background, pablo_body.foreground,
                                 RgbColor{0x00, 0xC0, 0xC0}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, pablo_body.line_number),
     .body = pablo_body,
     .source = {.author = "Ron Aaron <ron@ronware.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/pablo.vim"}},
    {.name = "peachpuff",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(peachpuff_body.background, peachpuff_body.foreground,
                                 RgbColor{0x00, 0x8B, 0x8B}, Appearance::light),
                       RgbColor{0xFF, 0xDC, 0xBD}, peachpuff_body.line_number),
     .body = peachpuff_body,
     .source = {.author = "Original author David Ne\\v{c}as (Yeti) <yeti@physics.muni.cz>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/peachpuff.vim"}},
    {.name = "quiet",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(quiet_body.background, quiet_body.foreground,
                                 RgbColor{0xDA, 0xDA, 0xDA}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, quiet_body.line_number),
     .body = quiet_body,
     .source = {.author = "Maxence Weynans <neutaaaaan@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/quiet.vim"}},
    {.name = "retrobox",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(retrobox_body.background, retrobox_body.foreground,
                                 RgbColor{0x83, 0xA5, 0x98}, Appearance::dark),
                       RgbColor{0x17, 0x17, 0x17}, retrobox_body.line_number),
     .body = retrobox_body,
     .source = {.author = "Maxim Kim <habamax@gmail.com>, ported from gruvbox8 of Lifepillar "
                          "<lifepillar@lifepillar.me>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/retrobox.vim"}},
    {.name = "ron",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(ron_body.background, ron_body.foreground,
                                 RgbColor{0x00, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, ron_body.line_number),
     .body = ron_body,
     .source = {.author = "original author Ron Aaron <ron@ronware.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/ron.vim"}},
    {.name = "shine",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(shine_body.background, shine_body.foreground,
                                 RgbColor{0x00, 0x8B, 0x8B}, Appearance::light),
                       RgbColor{0xF0, 0xF0, 0xF0}, shine_body.line_number),
     .body = shine_body,
     .source = {.author = "Original author is Yasuhiro Matsumoto <mattn.jp@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/shine.vim"}},
    {.name = "slate",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(slate_body.background, slate_body.foreground,
                                 RgbColor{0xFF, 0x87, 0x87}, Appearance::dark),
                       RgbColor{0x1F, 0x1F, 0x1F}, slate_body.line_number),
     .body = slate_body,
     .source = {.author = "Original maintainer Ralph Amissah <ralph@amissah.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/slate.vim"}},
    {.name = "sorbet",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(sorbet_body.background, sorbet_body.foreground,
                                 RgbColor{0x87, 0xD7, 0x5F}, Appearance::dark),
                       RgbColor{0x12, 0x14, 0x1B}, sorbet_body.line_number),
     .body = sorbet_body,
     .source = {.author = "Maxence Weynans <neutaaaaan@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/sorbet.vim"}},
    {.name = "torte",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(torte_body.background, torte_body.foreground,
                                 RgbColor{0x40, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, torte_body.line_number),
     .body = torte_body,
     .source = {.author = "Original maintainer Thorsten Maerz <info@netztorte.de>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/torte.vim"}},
    {.name = "unokai",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(unokai_body.background, unokai_body.foreground,
                                 RgbColor{0x66, 0xD9, 0xEF}, Appearance::dark),
                       RgbColor{0x20, 0x21, 0x1C}, unokai_body.line_number),
     .body = unokai_body,
     .source = {.author = "k-37 <60838818+k-37@users.noreply.github.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/unokai.vim"}},
    {.name = "wildcharm",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(wildcharm_body.background, wildcharm_body.foreground,
                                 RgbColor{0xFF, 0x87, 0xFF}, Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, wildcharm_body.line_number),
     .body = wildcharm_body,
     .source = {.author = "Maxim Kim <habamax@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/wildcharm.vim"}},
    {.name = "zaibatsu",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(zaibatsu_body.background, zaibatsu_body.foreground,
                                 RgbColor{0x87, 0xFF, 0xFF}, Appearance::dark),
                       RgbColor{0x1E, 0x11, 0x33}, zaibatsu_body.line_number),
     .body = zaibatsu_body,
     .source = {.author = "Romain Lafourcade <romainlafourcade@gmail.com>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/zaibatsu.vim"}},
    {.name = "zellner",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(zellner_body.background, zellner_body.foreground,
                                 RgbColor{0x00, 0x00, 0xFF}, Appearance::light),
                       RgbColor{0xF0, 0xF0, 0xF0}, zellner_body.line_number),
     .body = zellner_body,
     .source = {.author = "Original author Ron Aaron <ron@ronware.org>",
                .license = "Vim license; palette adaptation",
                .url = "https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/"
                       "runtime/colors/zellner.vim"}},
    {.name = "abyss",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(abyss_body.background, abyss_body.foreground,
                                 RgbColor{0x59, 0x6F, 0x99}, Appearance::dark),
                       RgbColor{0x06, 0x06, 0x21}, abyss_body.line_number),
     .body = abyss_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-abyss/themes/abyss-color-theme.json"}},
    {.name = "vscode-dark-2026",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(vscode_dark_2026_body.background, vscode_dark_2026_body.foreground,
                                 RgbColor{0x2D, 0x6E, 0x8A}, Appearance::dark),
                       RgbColor{0x1C, 0x1D, 0x1E}, vscode_dark_2026_body.line_number),
     .body = vscode_dark_2026_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/2026-dark.json"}},
    {.name = "vscode-light-2026",
     .appearance = Appearance::light,
     .ui =
         with_gutter(derive_ui(vscode_light_2026_body.background, vscode_light_2026_body.foreground,
                               RgbColor{0x00, 0x69, 0xCC}, Appearance::light),
                     RgbColor{0xF2, 0xF2, 0xF2}, vscode_light_2026_body.line_number),
     .body = vscode_light_2026_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/2026-light.json"}},
    {.name = "vscode-dark-modern",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(vscode_dark_modern_body.background,
                                 vscode_dark_modern_body.foreground, RgbColor{0x00, 0x78, 0xD4},
                                 Appearance::dark),
                       RgbColor{0x18, 0x18, 0x18}, vscode_dark_modern_body.line_number),
     .body = vscode_dark_modern_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/dark_modern.json"}},
    {.name = "vscode-dark-plus",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(vscode_dark_plus_body.background, vscode_dark_plus_body.foreground,
                                 RgbColor{0xDC, 0xDC, 0xAA}, Appearance::dark),
                       RgbColor{0x18, 0x18, 0x18}, vscode_dark_plus_body.line_number),
     .body = vscode_dark_plus_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/dark_plus.json"}},
    {.name = "vscode-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(vscode_dark_body.background, vscode_dark_body.foreground,
                                 RgbColor{0x56, 0x9C, 0xD6}, Appearance::dark),
                       RgbColor{0x18, 0x18, 0x18}, vscode_dark_body.line_number),
     .body = vscode_dark_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/dark_vs.json"}},
    {.name = "vscode-high-contrast",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(vscode_high_contrast_body.background,
                                 vscode_high_contrast_body.foreground, RgbColor{0xDC, 0xDC, 0xAA},
                                 Appearance::dark),
                       RgbColor{0x11, 0x11, 0x11}, vscode_high_contrast_body.line_number),
     .body = vscode_high_contrast_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/hc_black.json"}},
    {.name = "vscode-high-contrast-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(vscode_high_contrast_light_body.background,
                                 vscode_high_contrast_light_body.foreground,
                                 RgbColor{0x5E, 0x2C, 0xBC}, Appearance::light),
                       RgbColor{0xEE, 0xEE, 0xEE}, vscode_high_contrast_light_body.line_number),
     .body = vscode_high_contrast_light_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/hc_light.json"}},
    {.name = "vscode-light-modern",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(vscode_light_modern_body.background,
                                 vscode_light_modern_body.foreground, RgbColor{0x00, 0x5F, 0xB8},
                                 Appearance::light),
                       RgbColor{0xF8, 0xF8, 0xF8}, vscode_light_modern_body.line_number),
     .body = vscode_light_modern_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/light_modern.json"}},
    {.name = "vscode-light-plus",
     .appearance = Appearance::light,
     .ui =
         with_gutter(derive_ui(vscode_light_plus_body.background, vscode_light_plus_body.foreground,
                               RgbColor{0x79, 0x5E, 0x26}, Appearance::light),
                     RgbColor{0xF0, 0xF0, 0xF0}, vscode_light_plus_body.line_number),
     .body = vscode_light_plus_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/light_plus.json"}},
    {.name = "vscode-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(vscode_light_body.background, vscode_light_body.foreground,
                                 RgbColor{0x00, 0x00, 0xFF}, Appearance::light),
                       RgbColor{0xF0, 0xF0, 0xF0}, vscode_light_body.line_number),
     .body = vscode_light_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-defaults/themes/light_vs.json"}},
    {.name = "kimbie-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(kimbie_dark_body.background, kimbie_dark_body.foreground,
                                 RgbColor{0xA5, 0x7A, 0x4C}, Appearance::dark),
                       RgbColor{0x36, 0x27, 0x12}, kimbie_dark_body.line_number),
     .body = kimbie_dark_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-kimbie-dark/themes/kimbie-dark-color-theme.json"}},
    {.name = "monokai-dimmed",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(monokai_dimmed_body.background, monokai_dimmed_body.foreground,
                                 RgbColor{0x36, 0x55, 0xB5}, Appearance::dark),
                       RgbColor{0x27, 0x27, 0x27}, monokai_dimmed_body.line_number),
     .body = monokai_dimmed_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-monokai-dimmed/themes/dimmed-monokai-color-theme.json"}},
    {.name = "quiet-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(quiet_light_body.background, quiet_light_body.foreground,
                                 RgbColor{0x97, 0x69, 0xDC}, Appearance::light),
                       RgbColor{0xF2, 0xF2, 0xF2}, quiet_light_body.line_number),
     .body = quiet_light_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-quietlight/themes/quietlight-color-theme.json"}},
    {.name = "red",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(red_body.background, red_body.foreground,
                                 RgbColor{0xBD, 0x44, 0x44}, Appearance::dark),
                       RgbColor{0x33, 0x00, 0x00}, red_body.line_number),
     .body = red_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-red/themes/Red-color-theme.json"}},
    {.name = "tomorrow-night-blue",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(tomorrow_night_blue_body.background,
                                 tomorrow_night_blue_body.foreground, RgbColor{0xBB, 0xDA, 0xFF},
                                 Appearance::dark),
                       RgbColor{0x00, 0x1C, 0x40}, tomorrow_night_blue_body.line_number),
     .body = tomorrow_night_blue_body,
     .source =
         {.author = "Microsoft and theme contributors",
          .license = "MIT",
          .url =
              "https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/"
              "extensions/theme-tomorrow-night-blue/themes/tomorrow-night-blue-color-theme.json"}},
    {.name = "monalisa",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(monalisa_body.background, monalisa_body.foreground,
                                 RgbColor{0xF6, 0xD6, 0x66}, Appearance::dark),
                       RgbColor{0x20, 0x15, 0x19}, monalisa_body.line_number),
     .body = monalisa_body,
     .source = {.author = "Patrick Dewey and contributors",
                .license = "MIT",
                .url = "https://github.com/ptdewey/monalisa-nvim/blob/"
                       "2ffe6db37fcad17da0d210f5b3a357712d0b8a2b/fnl/monalisa/init.fnl"}},
    {.name = "catppuccin-latte",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(catppuccin_latte_body.background, catppuccin_latte_body.foreground,
                                 RgbColor{0x1E, 0x66, 0xF5}, Appearance::light),
                       RgbColor{0xE6, 0xE9, 0xEF}, catppuccin_latte_body.line_number),
     .body = catppuccin_latte_body,
     .source = {.author = "Catppuccin contributors",
                .license = "MIT",
                .url = "https://github.com/catppuccin/palette/blob/"
                       "07d02aa110ef9eb7e7427afca5c73ba9cf7f8ebd/palette.json"}},
    {.name = "catppuccin-frappe",
     .appearance = Appearance::dark,
     .ui =
         with_gutter(derive_ui(catppuccin_frappe_body.background, catppuccin_frappe_body.foreground,
                               RgbColor{0x8C, 0xAA, 0xEE}, Appearance::dark),
                     RgbColor{0x29, 0x2C, 0x3C}, catppuccin_frappe_body.line_number),
     .body = catppuccin_frappe_body,
     .source = {.author = "Catppuccin contributors",
                .license = "MIT",
                .url = "https://github.com/catppuccin/palette/blob/"
                       "07d02aa110ef9eb7e7427afca5c73ba9cf7f8ebd/palette.json"}},
    {.name = "catppuccin-macchiato",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(catppuccin_macchiato_body.background,
                                 catppuccin_macchiato_body.foreground, RgbColor{0x8A, 0xAD, 0xF4},
                                 Appearance::dark),
                       RgbColor{0x1E, 0x20, 0x30}, catppuccin_macchiato_body.line_number),
     .body = catppuccin_macchiato_body,
     .source = {.author = "Catppuccin contributors",
                .license = "MIT",
                .url = "https://github.com/catppuccin/palette/blob/"
                       "07d02aa110ef9eb7e7427afca5c73ba9cf7f8ebd/palette.json"}},
    {.name = "nord",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(nord_body.background, nord_body.foreground,
                                 RgbColor{0x88, 0xC0, 0xD0}, Appearance::dark),
                       RgbColor{0x3B, 0x42, 0x52}, nord_body.line_number),
     .body = nord_body,
     .source = {.author = "Arctic Ice Studio and Sven Greb",
                .license = "MIT",
                .url = "https://github.com/nordtheme/nord/blob/"
                       "1cef71605416a222e57225b544540ce0fcec18d4/src/nord.scss"}},
    {.name = "gruvbox",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(gruvbox_body.background, gruvbox_body.foreground,
                                 RgbColor{0xB8, 0xBB, 0x26}, Appearance::dark),
                       RgbColor{0x1D, 0x20, 0x21}, gruvbox_body.line_number),
     .body = gruvbox_body,
     .source = {.author = "Pavel Pertsev",
                .license = "MIT",
                .url = "https://github.com/morhetz/gruvbox/blob/"
                       "ef8864bb42bf244f0295d1c5a403b27e3d139695/colors/gruvbox.vim"}},
    {.name = "gruvbox-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(gruvbox_light_body.background, gruvbox_light_body.foreground,
                                 RgbColor{0x79, 0x74, 0x0E}, Appearance::light),
                       RgbColor{0xEB, 0xDB, 0xB2}, gruvbox_light_body.line_number),
     .body = gruvbox_light_body,
     .source = {.author = "Pavel Pertsev",
                .license = "MIT",
                .url = "https://github.com/morhetz/gruvbox/blob/"
                       "ef8864bb42bf244f0295d1c5a403b27e3d139695/colors/gruvbox.vim"}},
    {.name = "tokyo-night",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(tokyo_night_body.background, tokyo_night_body.foreground,
                                 RgbColor{0x7A, 0xA2, 0xF7}, Appearance::dark),
                       RgbColor{0x16, 0x16, 0x1E}, tokyo_night_body.line_number),
     .body = tokyo_night_body,
     .source = {.author = "Folke Lemaitre and Tokyo Night contributors",
                .license = "MIT",
                .url = "https://github.com/folke/tokyonight.nvim/blob/"
                       "cdc07ac78467a233fd62c493de29a17e0cf2b2b6/lua/tokyonight/colors/night.lua"}},
    {.name = "tokyo-night-storm",
     .appearance = Appearance::dark,
     .ui =
         with_gutter(derive_ui(tokyo_night_storm_body.background, tokyo_night_storm_body.foreground,
                               RgbColor{0x7A, 0xA2, 0xF7}, Appearance::dark),
                     RgbColor{0x1F, 0x23, 0x35}, tokyo_night_storm_body.line_number),
     .body = tokyo_night_storm_body,
     .source = {.author = "Folke Lemaitre and Tokyo Night contributors",
                .license = "MIT",
                .url = "https://github.com/folke/tokyonight.nvim/blob/"
                       "cdc07ac78467a233fd62c493de29a17e0cf2b2b6/lua/tokyonight/colors/storm.lua"}},
    {.name = "tokyo-night-moon",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(tokyo_night_moon_body.background, tokyo_night_moon_body.foreground,
                                 RgbColor{0x82, 0xAA, 0xFF}, Appearance::dark),
                       RgbColor{0x1E, 0x20, 0x30}, tokyo_night_moon_body.line_number),
     .body = tokyo_night_moon_body,
     .source = {.author = "Folke Lemaitre and Tokyo Night contributors",
                .license = "MIT",
                .url = "https://github.com/folke/tokyonight.nvim/blob/"
                       "cdc07ac78467a233fd62c493de29a17e0cf2b2b6/lua/tokyonight/colors/moon.lua"}},
    {.name = "tokyo-night-light",
     .appearance = Appearance::light,
     .ui =
         with_gutter(derive_ui(tokyo_night_light_body.background, tokyo_night_light_body.foreground,
                               RgbColor{0xCE, 0xD0, 0xD7}, Appearance::light),
                     RgbColor{0xD6, 0xD8, 0xDF}, tokyo_night_light_body.line_number),
     .body = tokyo_night_light_body,
     .source = {.author = "Enkia",
                .license = "MIT",
                .url = "https://github.com/enkia/tokyo-night-vscode-theme/blob/"
                       "7c0f11eaef322f293621ca7befe462214b7ea468/themes/"
                       "tokyo-night-light-color-theme.json"}},
    {.name = "github-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(github_dark_body.background, github_dark_body.foreground,
                                 RgbColor{0xD2, 0xA8, 0xFF}, Appearance::dark),
                       RgbColor{0x01, 0x04, 0x09}, github_dark_body.line_number),
     .body = github_dark_body,
     .source = {.author = "GitHub and Primer contributors",
                .license = "MIT",
                .url = "https://github.com/primer/github-vscode-theme/blob/"
                       "cd78e5e4e7bcf132a6f428ae0f32264bb1b729cf/src/theme.js"}},
    {.name = "github-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(github_light_body.background, github_light_body.foreground,
                                 RgbColor{0x82, 0x50, 0xDF}, Appearance::light),
                       RgbColor{0xF6, 0xF8, 0xFA}, github_light_body.line_number),
     .body = github_light_body,
     .source = {.author = "GitHub and Primer contributors",
                .license = "MIT",
                .url = "https://github.com/primer/github-vscode-theme/blob/"
                       "cd78e5e4e7bcf132a6f428ae0f32264bb1b729cf/src/theme.js"}},
    {.name = "github-dark-dimmed",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(github_dark_dimmed_body.background,
                                 github_dark_dimmed_body.foreground, RgbColor{0xDC, 0xBD, 0xFB},
                                 Appearance::dark),
                       RgbColor{0x1C, 0x21, 0x28}, github_dark_dimmed_body.line_number),
     .body = github_dark_dimmed_body,
     .source = {.author = "GitHub and Primer contributors",
                .license = "MIT",
                .url = "https://github.com/primer/github-vscode-theme/blob/"
                       "cd78e5e4e7bcf132a6f428ae0f32264bb1b729cf/src/theme.js"}},
    {.name = "one-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(one_light_body.background, one_light_body.foreground,
                                 RgbColor{0x40, 0x78, 0xF2}, Appearance::light),
                       RgbColor{0xF0, 0xF0, 0xF1}, one_light_body.line_number),
     .body = one_light_body,
     .source = {.author = "Atom",
                .license = "MIT",
                .url = "https://github.com/atom/one-light-syntax/blob/"
                       "d84579027410c576086dfca14d934c4bd74b0438/styles/colors.less"}},
    {.name = "intellij-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(intellij_light_body.background, intellij_light_body.foreground,
                                 RgbColor{0x00, 0x00, 0x00}, Appearance::light),
                       RgbColor{0xF0, 0xF0, 0xF0}, intellij_light_body.line_number),
     .body = intellij_light_body,
     .source = {.author = "JetBrains",
                .license = "Apache-2.0; palette adaptation",
                .url = "https://github.com/JetBrains/intellij-community/blob/"
                       "8886d47ff4b7a538583b793ba29e9cc642ae01d9/platform/platform-resources/src/"
                       "DefaultColorSchemesManager.xml"}},
    {.name = "darcula",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(darcula_body.background, darcula_body.foreground,
                                 RgbColor{0xFF, 0xC6, 0x6D}, Appearance::dark),
                       RgbColor{0x31, 0x33, 0x35}, darcula_body.line_number),
     .body = darcula_body,
     .source = {.author = "JetBrains",
                .license = "Apache-2.0; palette adaptation",
                .url = "https://github.com/JetBrains/intellij-community/blob/"
                       "8886d47ff4b7a538583b793ba29e9cc642ae01d9/platform/platform-resources/src/"
                       "DefaultColorSchemesManager.xml"}},
    {.name = "rose-pine",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(rose_pine_body.background, rose_pine_body.foreground,
                                 RgbColor{0xEB, 0xBC, 0xBA}, Appearance::dark),
                       RgbColor{0x1F, 0x1D, 0x2E}, rose_pine_body.line_number),
     .body = rose_pine_body,
     .source = {.author = "Rosé Pine contributors",
                .license = "MIT",
                .url = "https://github.com/rose-pine/palette/blob/"
                       "92af52b465ab6e47437aca223c9b8d3009a2023b/palette.json"}},
    {.name = "rose-pine-moon",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(rose_pine_moon_body.background, rose_pine_moon_body.foreground,
                                 RgbColor{0xEA, 0x9A, 0x97}, Appearance::dark),
                       RgbColor{0x2A, 0x27, 0x3F}, rose_pine_moon_body.line_number),
     .body = rose_pine_moon_body,
     .source = {.author = "Rosé Pine contributors",
                .license = "MIT",
                .url = "https://github.com/rose-pine/palette/blob/"
                       "92af52b465ab6e47437aca223c9b8d3009a2023b/palette.json"}},
    {.name = "rose-pine-dawn",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(rose_pine_dawn_body.background, rose_pine_dawn_body.foreground,
                                 RgbColor{0xD7, 0x82, 0x7E}, Appearance::light),
                       RgbColor{0xFF, 0xFA, 0xF3}, rose_pine_dawn_body.line_number),
     .body = rose_pine_dawn_body,
     .source = {.author = "Rosé Pine contributors",
                .license = "MIT",
                .url = "https://github.com/rose-pine/palette/blob/"
                       "92af52b465ab6e47437aca223c9b8d3009a2023b/palette.json"}},
    {.name = "everforest-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(everforest_dark_body.background, everforest_dark_body.foreground,
                                 RgbColor{0xA7, 0xC0, 0x80}, Appearance::dark),
                       RgbColor{0x23, 0x2A, 0x2E}, everforest_dark_body.line_number),
     .body = everforest_dark_body,
     .source = {.author = "Sainnhe Park",
                .license = "MIT",
                .url = "https://github.com/sainnhe/everforest/blob/"
                       "85a86eb62409e3ec88713bff3d1b9d7374e112e4/autoload/everforest.vim"}},
    {.name = "everforest-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(everforest_light_body.background, everforest_light_body.foreground,
                                 RgbColor{0x8D, 0xA1, 0x01}, Appearance::light),
                       RgbColor{0xEF, 0xEB, 0xD4}, everforest_light_body.line_number),
     .body = everforest_light_body,
     .source = {.author = "Sainnhe Park",
                .license = "MIT",
                .url = "https://github.com/sainnhe/everforest/blob/"
                       "85a86eb62409e3ec88713bff3d1b9d7374e112e4/autoload/everforest.vim"}},
    {.name = "kanagawa",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(kanagawa_body.background, kanagawa_body.foreground,
                                 RgbColor{0x7E, 0x9C, 0xD8}, Appearance::dark),
                       RgbColor{0x16, 0x16, 0x1D}, kanagawa_body.line_number),
     .body = kanagawa_body,
     .source = {.author = "Rebelot and Kanagawa contributors",
                .license = "MIT",
                .url = "https://github.com/rebelot/kanagawa.nvim/blob/"
                       "bb85e4bfc8d89b0e62c8fa53ccdd13d12e2f77b3/lua/kanagawa/colors.lua"}},
    {.name = "kanagawa-dragon",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(kanagawa_dragon_body.background, kanagawa_dragon_body.foreground,
                                 RgbColor{0x8B, 0xA4, 0xB0}, Appearance::dark),
                       RgbColor{0x12, 0x12, 0x0F}, kanagawa_dragon_body.line_number),
     .body = kanagawa_dragon_body,
     .source = {.author = "Rebelot and Kanagawa contributors",
                .license = "MIT",
                .url = "https://github.com/rebelot/kanagawa.nvim/blob/"
                       "bb85e4bfc8d89b0e62c8fa53ccdd13d12e2f77b3/lua/kanagawa/colors.lua"}},
    {.name = "kanagawa-lotus",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(kanagawa_lotus_body.background, kanagawa_lotus_body.foreground,
                                 RgbColor{0x4D, 0x69, 0x9B}, Appearance::light),
                       RgbColor{0xE5, 0xDD, 0xB0}, kanagawa_lotus_body.line_number),
     .body = kanagawa_lotus_body,
     .source = {.author = "Rebelot and Kanagawa contributors",
                .license = "MIT",
                .url = "https://github.com/rebelot/kanagawa.nvim/blob/"
                       "bb85e4bfc8d89b0e62c8fa53ccdd13d12e2f77b3/lua/kanagawa/colors.lua"}},
    {.name = "material-oceanic",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(material_oceanic_body.background, material_oceanic_body.foreground,
                                 RgbColor{0x82, 0xAA, 0xFF}, Appearance::dark),
                       RgbColor{0x1E, 0x27, 0x2C}, material_oceanic_body.line_number),
     .body = material_oceanic_body,
     .source = {.author = "Material Theme / Mallowigi and Equinusocio",
                .license = "published palette colors; names attributed to their authors",
                .url = "https://material-theme.com/docs/reference/color-palette/"}},
    {.name = "material-darker",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(material_darker_body.background, material_darker_body.foreground,
                                 RgbColor{0x82, 0xAA, 0xFF}, Appearance::dark),
                       RgbColor{0x1A, 0x1A, 0x1A}, material_darker_body.line_number),
     .body = material_darker_body,
     .source = {.author = "Material Theme / Mallowigi and Equinusocio",
                .license = "published palette colors; names attributed to their authors",
                .url = "https://material-theme.com/docs/reference/color-palette/"}},
    {.name = "material-lighter",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(material_lighter_body.background, material_lighter_body.foreground,
                                 RgbColor{0x61, 0x82, 0xB8}, Appearance::light),
                       RgbColor{0xFF, 0xFF, 0xFF}, material_lighter_body.line_number),
     .body = material_lighter_body,
     .source = {.author = "Material Theme / Mallowigi and Equinusocio",
                .license = "published palette colors; names attributed to their authors",
                .url = "https://material-theme.com/docs/reference/color-palette/"}},
    {.name = "material-palenight",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(material_palenight_body.background,
                                 material_palenight_body.foreground, RgbColor{0x82, 0xAA, 0xFF},
                                 Appearance::dark),
                       RgbColor{0x20, 0x23, 0x31}, material_palenight_body.line_number),
     .body = material_palenight_body,
     .source = {.author = "Material Theme / Mallowigi and Equinusocio",
                .license = "published palette colors; names attributed to their authors",
                .url = "https://material-theme.com/docs/reference/color-palette/"}},
    {.name = "material-deep-ocean",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(material_deep_ocean_body.background,
                                 material_deep_ocean_body.foreground, RgbColor{0x82, 0xAA, 0xFF},
                                 Appearance::dark),
                       RgbColor{0x19, 0x1A, 0x26}, material_deep_ocean_body.line_number),
     .body = material_deep_ocean_body,
     .source = {.author = "Material Theme / Mallowigi and Equinusocio",
                .license = "published palette colors; names attributed to their authors",
                .url = "https://material-theme.com/docs/reference/color-palette/"}},
    {.name = "ayu-dark",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(ayu_dark_body.background, ayu_dark_body.foreground,
                                 RgbColor{0xE6, 0xB4, 0x50}, Appearance::dark),
                       RgbColor{0x0D, 0x10, 0x17}, ayu_dark_body.line_number),
     .body = ayu_dark_body,
     .source = {.author = "Ayu Theme and Yihui Hu",
                .license = "MIT",
                .url = "https://github.com/ayu-theme/vscode-ayu/blob/"
                       "2c6feb7215bf7ca6c8c9a51066f255025df3a5b9/ayu-dark.json"}},
    {.name = "ayu-mirage",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(ayu_mirage_body.background, ayu_mirage_body.foreground,
                                 RgbColor{0xFF, 0xCC, 0x66}, Appearance::dark),
                       RgbColor{0x1F, 0x24, 0x30}, ayu_mirage_body.line_number),
     .body = ayu_mirage_body,
     .source = {.author = "Ayu Theme and Yihui Hu",
                .license = "MIT",
                .url = "https://github.com/ayu-theme/vscode-ayu/blob/"
                       "2c6feb7215bf7ca6c8c9a51066f255025df3a5b9/ayu-mirage.json"}},
    {.name = "ayu-light",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(ayu_light_body.background, ayu_light_body.foreground,
                                 RgbColor{0xF2, 0x97, 0x18}, Appearance::light),
                       RgbColor{0xF8, 0xF9, 0xFA}, ayu_light_body.line_number),
     .body = ayu_light_body,
     .source = {.author = "Ayu Theme and Yihui Hu",
                .license = "MIT",
                .url = "https://github.com/ayu-theme/vscode-ayu/blob/"
                       "2c6feb7215bf7ca6c8c9a51066f255025df3a5b9/ayu-light.json"}},
    {.name = "cobalt2",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(cobalt2_body.background, cobalt2_body.foreground,
                                 RgbColor{0x0D, 0x3A, 0x58}, Appearance::dark),
                       RgbColor{0x16, 0x2F, 0x42}, cobalt2_body.line_number),
     .body = cobalt2_body,
     .source = {.author = "Wes Bos",
                .license = "MIT",
                .url = "https://github.com/wesbos/cobalt2-vscode/blob/"
                       "c4e9574372b85afad1682ed0fdd1ac0411c62512/theme/cobalt2.json"}},
    {.name = "synthwave-84",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(synthwave_84_body.background, synthwave_84_body.foreground,
                                 RgbColor{0x1F, 0x21, 0x2B}, Appearance::dark),
                       RgbColor{0x24, 0x1B, 0x2F}, synthwave_84_body.line_number),
     .body = synthwave_84_body,
     .source = {.author = "Robb Owen",
                .license = "MIT",
                .url =
                    "https://github.com/robb0wen/synthwave-vscode/blob/"
                    "ecfa2fe1279f7233663fa3f98a96e6756000567b/themes/synthwave-color-theme.json"}},
    {.name = "alloy-core",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(alloy_core_body.background, alloy_core_body.foreground,
                                 RgbColor{0xE4, 0xAF, 0x55}, Appearance::dark),
                       RgbColor{0x10, 0x0E, 0x14}, alloy_core_body.line_number),
     .body = alloy_core_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "verdant-rage",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(verdant_rage_body.background, verdant_rage_body.foreground,
                                 RgbColor{0x9A, 0xCE, 0x6A}, Appearance::dark),
                       RgbColor{0x20, 0x31, 0x29}, verdant_rage_body.line_number),
     .body = verdant_rage_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "scarlet-web",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(scarlet_web_body.background, scarlet_web_body.foreground,
                                 RgbColor{0xEA, 0x71, 0x6D}, Appearance::dark),
                       RgbColor{0x0A, 0x13, 0x25}, scarlet_web_body.line_number),
     .body = scarlet_web_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "relic-sand",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(relic_sand_body.background, relic_sand_body.foreground,
                                 RgbColor{0x94, 0x50, 0x25}, Appearance::light),
                       RgbColor{0xFF, 0xF4, 0xDB}, relic_sand_body.line_number),
     .body = relic_sand_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "soap-rebellion",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(soap_rebellion_body.background, soap_rebellion_body.foreground,
                                 RgbColor{0xF3, 0x9B, 0xAD}, Appearance::dark),
                       RgbColor{0x12, 0x1B, 0x1A}, soap_rebellion_body.line_number),
     .body = soap_rebellion_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "code-rain",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(code_rain_body.background, code_rain_body.foreground,
                                 RgbColor{0x75, 0xE5, 0x9A}, Appearance::dark),
                       RgbColor{0x10, 0x23, 0x19}, code_rain_body.line_number),
     .body = code_rain_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "arcane-library",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(arcane_library_body.background, arcane_library_body.foreground,
                                 RgbColor{0xD8, 0xB7, 0x78}, Appearance::dark),
                       RgbColor{0x0F, 0x13, 0x25}, arcane_library_body.line_number),
     .body = arcane_library_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-colorschemes.md"}},
    {.name = "royal-kinetic",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(royal_kinetic_body.background, royal_kinetic_body.foreground,
                                 RgbColor{0xB5, 0x82, 0xF5}, Appearance::dark),
                       RgbColor{0x1D, 0x16, 0x2D}, royal_kinetic_body.line_number),
     .body = royal_kinetic_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "arena-sunset",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(arena_sunset_body.background, arena_sunset_body.foreground,
                                 RgbColor{0xF0, 0x90, 0x68}, Appearance::dark),
                       RgbColor{0x10, 0x23, 0x1F}, arena_sunset_body.line_number),
     .body = arena_sunset_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "cosmic-mixtape",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(cosmic_mixtape_body.background, cosmic_mixtape_body.foreground,
                                 RgbColor{0xE6, 0x8B, 0xCD}, Appearance::dark),
                       RgbColor{0x1A, 0x10, 0x25}, cosmic_mixtape_body.line_number),
     .body = cosmic_mixtape_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "prism-sanctum",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(prism_sanctum_body.background, prism_sanctum_body.foreground,
                                 RgbColor{0xED, 0xA6, 0x57}, Appearance::dark),
                       RgbColor{0x0B, 0x1C, 0x1D}, prism_sanctum_body.line_number),
     .body = prism_sanctum_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "winter-sentinel",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(winter_sentinel_body.background, winter_sentinel_body.foreground,
                                 RgbColor{0x8F, 0xB9, 0xCB}, Appearance::dark),
                       RgbColor{0x0D, 0x19, 0x25}, winter_sentinel_body.line_number),
     .body = winter_sentinel_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "infinite-dusk",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(infinite_dusk_body.background, infinite_dusk_body.foreground,
                                 RgbColor{0xDD, 0xA2, 0x86}, Appearance::dark),
                       RgbColor{0x20, 0x13, 0x27}, infinite_dusk_body.line_number),
     .body = infinite_dusk_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "stellar-pilot",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(stellar_pilot_body.background, stellar_pilot_body.foreground,
                                 RgbColor{0xD6, 0xB7, 0x66}, Appearance::dark),
                       RgbColor{0x0D, 0x14, 0x28}, stellar_pilot_body.line_number),
     .body = stellar_pilot_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "quantum-honey",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(quantum_honey_body.background, quantum_honey_body.foreground,
                                 RgbColor{0xE4, 0xB6, 0x51}, Appearance::dark),
                       RgbColor{0x1D, 0x19, 0x12}, quantum_honey_body.line_number),
     .body = quantum_honey_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "crimson-mercenary",
     .appearance = Appearance::dark,
     .ui =
         with_gutter(derive_ui(crimson_mercenary_body.background, crimson_mercenary_body.foreground,
                               RgbColor{0xE5, 0x7D, 0x85}, Appearance::dark),
                     RgbColor{0x19, 0x0D, 0x12}, crimson_mercenary_body.line_number),
     .body = crimson_mercenary_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "last-outlaw",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(last_outlaw_body.background, last_outlaw_body.foreground,
                                 RgbColor{0xCA, 0xA0, 0x6A}, Appearance::dark),
                       RgbColor{0x1D, 0x17, 0x12}, last_outlaw_body.line_number),
     .body = last_outlaw_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "ivory-agent",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(ivory_agent_body.background, ivory_agent_body.foreground,
                                 RgbColor{0xA7, 0x38, 0x37}, Appearance::light),
                       RgbColor{0xE3, 0xE2, 0xDC}, ivory_agent_body.line_number),
     .body = ivory_agent_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "celestial-brass",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(celestial_brass_body.background, celestial_brass_body.foreground,
                                 RgbColor{0x9B, 0x78, 0x36}, Appearance::light),
                       RgbColor{0xDD, 0xD8, 0xC8}, celestial_brass_body.line_number),
     .body = celestial_brass_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "amber-haze",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(amber_haze_body.background, amber_haze_body.foreground,
                                 RgbColor{0xEA, 0xA2, 0x5C}, Appearance::dark),
                       RgbColor{0x21, 0x13, 0x0F}, amber_haze_body.line_number),
     .body = amber_haze_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "desert-oracle",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(desert_oracle_body.background, desert_oracle_body.foreground,
                                 RgbColor{0x9D, 0x70, 0x2F}, Appearance::light),
                       RgbColor{0xD9, 0xC9, 0xAC}, desert_oracle_body.line_number),
     .body = desert_oracle_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "chrome-wasteland",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(chrome_wasteland_body.background, chrome_wasteland_body.foreground,
                                 RgbColor{0xE6, 0xAC, 0x72}, Appearance::dark),
                       RgbColor{0x19, 0x26, 0x2E}, chrome_wasteland_body.line_number),
     .body = chrome_wasteland_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "neon-grid",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(neon_grid_body.background, neon_grid_body.foreground,
                                 RgbColor{0x6E, 0xDD, 0xEC}, Appearance::dark),
                       RgbColor{0x0B, 0x2B, 0x37}, neon_grid_body.line_number),
     .body = neon_grid_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "biolume-forest",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(biolume_forest_body.background, biolume_forest_body.foreground,
                                 RgbColor{0x69, 0xCD, 0xD9}, Appearance::dark),
                       RgbColor{0x07, 0x16, 0x2B}, biolume_forest_body.line_number),
     .body = biolume_forest_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "pastel-concierge",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(pastel_concierge_body.background, pastel_concierge_body.foreground,
                                 RgbColor{0xA3, 0x4D, 0x70}, Appearance::light),
                       RgbColor{0xE9, 0xD2, 0xD7}, pastel_concierge_body.line_number),
     .body = pastel_concierge_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "twilight-jazz",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(twilight_jazz_body.background, twilight_jazz_body.foreground,
                                 RgbColor{0xE4, 0xC8, 0x6B}, Appearance::dark),
                       RgbColor{0x14, 0x17, 0x33}, twilight_jazz_body.line_number),
     .body = twilight_jazz_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "noir-vigil",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(noir_vigil_body.background, noir_vigil_body.foreground,
                                 RgbColor{0xD5, 0x79, 0x72}, Appearance::dark),
                       RgbColor{0x12, 0x0F, 0x11}, noir_vigil_body.line_number),
     .body = noir_vigil_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "carnival-ashes",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(carnival_ashes_body.background, carnival_ashes_body.foreground,
                                 RgbColor{0xC9, 0xAC, 0x62}, Appearance::dark),
                       RgbColor{0x14, 0x24, 0x1C}, carnival_ashes_body.line_number),
     .body = carnival_ashes_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "orbital-frost",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(orbital_frost_body.background, orbital_frost_body.foreground,
                                 RgbColor{0x52, 0x73, 0x8B}, Appearance::light),
                       RgbColor{0xD7, 0xE0, 0xE7}, orbital_frost_body.line_number),
     .body = orbital_frost_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "folded-city",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(folded_city_body.background, folded_city_body.foreground,
                                 RgbColor{0x91, 0xB6, 0xC8}, Appearance::dark),
                       RgbColor{0x16, 0x21, 0x2B}, folded_city_body.line_number),
     .body = folded_city_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "midnight-drive",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(midnight_drive_body.background, midnight_drive_body.foreground,
                                 RgbColor{0xBC, 0xD8, 0xC9}, Appearance::dark),
                       RgbColor{0x0B, 0x25, 0x2E}, midnight_drive_body.line_number),
     .body = midnight_drive_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "editorial-cafe",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(editorial_cafe_body.background, editorial_cafe_body.foreground,
                                 RgbColor{0x4F, 0x7D, 0x75}, Appearance::light),
                       RgbColor{0xCD, 0xDD, 0xD5}, editorial_cafe_body.line_number),
     .body = editorial_cafe_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "striped-haunt",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(striped_haunt_body.background, striped_haunt_body.foreground,
                                 RgbColor{0x77, 0x53, 0x83}, Appearance::light),
                       RgbColor{0xD9, 0xD5, 0xDB}, striped_haunt_body.line_number),
     .body = striped_haunt_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "curious-garden",
     .appearance = Appearance::light,
     .ui = with_gutter(derive_ui(curious_garden_body.background, curious_garden_body.foreground,
                                 RgbColor{0x52, 0x7F, 0x70}, Appearance::light),
                       RgbColor{0xD0, 0xDF, 0xC8}, curious_garden_body.line_number),
     .body = curious_garden_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "tidebound-gold",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(tidebound_gold_body.background, tidebound_gold_body.foreground,
                                 RgbColor{0xD8, 0xB5, 0x6F}, Appearance::dark),
                       RgbColor{0x1B, 0x1F, 0x18}, tidebound_gold_body.line_number),
     .body = tidebound_gold_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "velvet-cabaret",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(velvet_cabaret_body.background, velvet_cabaret_body.foreground,
                                 RgbColor{0xE2, 0xB3, 0x75}, Appearance::dark),
                       RgbColor{0x24, 0x0E, 0x16}, velvet_cabaret_body.line_number),
     .body = velvet_cabaret_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
    {.name = "lagoon-whisper",
     .appearance = Appearance::dark,
     .ui = with_gutter(derive_ui(lagoon_whisper_body.background, lagoon_whisper_body.foreground,
                                 RgbColor{0x8B, 0xC6, 0xBC}, Appearance::dark),
                       RgbColor{0x09, 0x26, 0x2C}, lagoon_whisper_body.line_number),
     .body = lagoon_whisper_body,
     .source = {.author = "NeNe Nib",
                .license = "MIT; original palette",
                .url = "https://github.com/hideyukiMORI/nene-nib/blob/main/docs/design/"
                       "2026-10-12-cinema-collection.md"}},
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

static_assert(theme_of(BuiltinTheme::blue).name == "blue");
static_assert(theme_of(BuiltinTheme::catppuccin).name == "catppuccin");
static_assert(theme_of(BuiltinTheme::darkblue).name == "darkblue");
static_assert(theme_of(BuiltinTheme::vim_default).name == "default");
static_assert(theme_of(BuiltinTheme::delek).name == "delek");
static_assert(theme_of(BuiltinTheme::desert).name == "desert");
static_assert(theme_of(BuiltinTheme::elflord).name == "elflord");
static_assert(theme_of(BuiltinTheme::evening).name == "evening");
static_assert(theme_of(BuiltinTheme::habamax).name == "habamax");
static_assert(theme_of(BuiltinTheme::industry).name == "industry");
static_assert(theme_of(BuiltinTheme::koehler).name == "koehler");
static_assert(theme_of(BuiltinTheme::lunaperche).name == "lunaperche");
static_assert(theme_of(BuiltinTheme::morning).name == "morning");
static_assert(theme_of(BuiltinTheme::murphy).name == "murphy");
static_assert(theme_of(BuiltinTheme::novum).name == "novum");
static_assert(theme_of(BuiltinTheme::pablo).name == "pablo");
static_assert(theme_of(BuiltinTheme::peachpuff).name == "peachpuff");
static_assert(theme_of(BuiltinTheme::quiet).name == "quiet");
static_assert(theme_of(BuiltinTheme::retrobox).name == "retrobox");
static_assert(theme_of(BuiltinTheme::ron).name == "ron");
static_assert(theme_of(BuiltinTheme::shine).name == "shine");
static_assert(theme_of(BuiltinTheme::slate).name == "slate");
static_assert(theme_of(BuiltinTheme::sorbet).name == "sorbet");
static_assert(theme_of(BuiltinTheme::torte).name == "torte");
static_assert(theme_of(BuiltinTheme::unokai).name == "unokai");
static_assert(theme_of(BuiltinTheme::wildcharm).name == "wildcharm");
static_assert(theme_of(BuiltinTheme::zaibatsu).name == "zaibatsu");
static_assert(theme_of(BuiltinTheme::zellner).name == "zellner");
static_assert(theme_of(BuiltinTheme::abyss).name == "abyss");
static_assert(theme_of(BuiltinTheme::vscode_dark_2026).name == "vscode-dark-2026");
static_assert(theme_of(BuiltinTheme::vscode_light_2026).name == "vscode-light-2026");
static_assert(theme_of(BuiltinTheme::vscode_dark_modern).name == "vscode-dark-modern");
static_assert(theme_of(BuiltinTheme::vscode_dark_plus).name == "vscode-dark-plus");
static_assert(theme_of(BuiltinTheme::vscode_dark).name == "vscode-dark");
static_assert(theme_of(BuiltinTheme::vscode_high_contrast).name == "vscode-high-contrast");
static_assert(theme_of(BuiltinTheme::vscode_high_contrast_light).name ==
              "vscode-high-contrast-light");
static_assert(theme_of(BuiltinTheme::vscode_light_modern).name == "vscode-light-modern");
static_assert(theme_of(BuiltinTheme::vscode_light_plus).name == "vscode-light-plus");
static_assert(theme_of(BuiltinTheme::vscode_light).name == "vscode-light");
static_assert(theme_of(BuiltinTheme::kimbie_dark).name == "kimbie-dark");
static_assert(theme_of(BuiltinTheme::monokai_dimmed).name == "monokai-dimmed");
static_assert(theme_of(BuiltinTheme::quiet_light).name == "quiet-light");
static_assert(theme_of(BuiltinTheme::red).name == "red");
static_assert(theme_of(BuiltinTheme::tomorrow_night_blue).name == "tomorrow-night-blue");
static_assert(theme_of(BuiltinTheme::monalisa).name == "monalisa");
static_assert(theme_of(BuiltinTheme::catppuccin_latte).name == "catppuccin-latte");
static_assert(theme_of(BuiltinTheme::catppuccin_frappe).name == "catppuccin-frappe");
static_assert(theme_of(BuiltinTheme::catppuccin_macchiato).name == "catppuccin-macchiato");
static_assert(theme_of(BuiltinTheme::nord).name == "nord");
static_assert(theme_of(BuiltinTheme::gruvbox).name == "gruvbox");
static_assert(theme_of(BuiltinTheme::gruvbox_light).name == "gruvbox-light");
static_assert(theme_of(BuiltinTheme::tokyo_night).name == "tokyo-night");
static_assert(theme_of(BuiltinTheme::tokyo_night_storm).name == "tokyo-night-storm");
static_assert(theme_of(BuiltinTheme::tokyo_night_moon).name == "tokyo-night-moon");
static_assert(theme_of(BuiltinTheme::tokyo_night_light).name == "tokyo-night-light");
static_assert(theme_of(BuiltinTheme::github_dark).name == "github-dark");
static_assert(theme_of(BuiltinTheme::github_light).name == "github-light");
static_assert(theme_of(BuiltinTheme::github_dark_dimmed).name == "github-dark-dimmed");
static_assert(theme_of(BuiltinTheme::one_light).name == "one-light");
static_assert(theme_of(BuiltinTheme::intellij_light).name == "intellij-light");
static_assert(theme_of(BuiltinTheme::darcula).name == "darcula");
static_assert(theme_of(BuiltinTheme::rose_pine).name == "rose-pine");
static_assert(theme_of(BuiltinTheme::rose_pine_moon).name == "rose-pine-moon");
static_assert(theme_of(BuiltinTheme::rose_pine_dawn).name == "rose-pine-dawn");
static_assert(theme_of(BuiltinTheme::everforest_dark).name == "everforest-dark");
static_assert(theme_of(BuiltinTheme::everforest_light).name == "everforest-light");
static_assert(theme_of(BuiltinTheme::kanagawa).name == "kanagawa");
static_assert(theme_of(BuiltinTheme::kanagawa_dragon).name == "kanagawa-dragon");
static_assert(theme_of(BuiltinTheme::kanagawa_lotus).name == "kanagawa-lotus");
static_assert(theme_of(BuiltinTheme::material_oceanic).name == "material-oceanic");
static_assert(theme_of(BuiltinTheme::material_darker).name == "material-darker");
static_assert(theme_of(BuiltinTheme::material_lighter).name == "material-lighter");
static_assert(theme_of(BuiltinTheme::material_palenight).name == "material-palenight");
static_assert(theme_of(BuiltinTheme::material_deep_ocean).name == "material-deep-ocean");
static_assert(theme_of(BuiltinTheme::ayu_dark).name == "ayu-dark");
static_assert(theme_of(BuiltinTheme::ayu_mirage).name == "ayu-mirage");
static_assert(theme_of(BuiltinTheme::ayu_light).name == "ayu-light");
static_assert(theme_of(BuiltinTheme::cobalt2).name == "cobalt2");
static_assert(theme_of(BuiltinTheme::synthwave_84).name == "synthwave-84");
static_assert(theme_of(BuiltinTheme::alloy_core).name == "alloy-core");
static_assert(theme_of(BuiltinTheme::verdant_rage).name == "verdant-rage");
static_assert(theme_of(BuiltinTheme::scarlet_web).name == "scarlet-web");
static_assert(theme_of(BuiltinTheme::relic_sand).name == "relic-sand");
static_assert(theme_of(BuiltinTheme::soap_rebellion).name == "soap-rebellion");
static_assert(theme_of(BuiltinTheme::code_rain).name == "code-rain");
static_assert(theme_of(BuiltinTheme::arcane_library).name == "arcane-library");
static_assert(theme_of(BuiltinTheme::royal_kinetic).name == "royal-kinetic");
static_assert(theme_of(BuiltinTheme::arena_sunset).name == "arena-sunset");
static_assert(theme_of(BuiltinTheme::cosmic_mixtape).name == "cosmic-mixtape");
static_assert(theme_of(BuiltinTheme::prism_sanctum).name == "prism-sanctum");
static_assert(theme_of(BuiltinTheme::winter_sentinel).name == "winter-sentinel");
static_assert(theme_of(BuiltinTheme::infinite_dusk).name == "infinite-dusk");
static_assert(theme_of(BuiltinTheme::stellar_pilot).name == "stellar-pilot");
static_assert(theme_of(BuiltinTheme::quantum_honey).name == "quantum-honey");
static_assert(theme_of(BuiltinTheme::crimson_mercenary).name == "crimson-mercenary");
static_assert(theme_of(BuiltinTheme::last_outlaw).name == "last-outlaw");
static_assert(theme_of(BuiltinTheme::ivory_agent).name == "ivory-agent");
static_assert(theme_of(BuiltinTheme::celestial_brass).name == "celestial-brass");
static_assert(theme_of(BuiltinTheme::amber_haze).name == "amber-haze");
static_assert(theme_of(BuiltinTheme::desert_oracle).name == "desert-oracle");
static_assert(theme_of(BuiltinTheme::chrome_wasteland).name == "chrome-wasteland");
static_assert(theme_of(BuiltinTheme::neon_grid).name == "neon-grid");
static_assert(theme_of(BuiltinTheme::biolume_forest).name == "biolume-forest");
static_assert(theme_of(BuiltinTheme::pastel_concierge).name == "pastel-concierge");
static_assert(theme_of(BuiltinTheme::twilight_jazz).name == "twilight-jazz");
static_assert(theme_of(BuiltinTheme::noir_vigil).name == "noir-vigil");
static_assert(theme_of(BuiltinTheme::carnival_ashes).name == "carnival-ashes");
static_assert(theme_of(BuiltinTheme::orbital_frost).name == "orbital-frost");
static_assert(theme_of(BuiltinTheme::folded_city).name == "folded-city");
static_assert(theme_of(BuiltinTheme::midnight_drive).name == "midnight-drive");
static_assert(theme_of(BuiltinTheme::editorial_cafe).name == "editorial-cafe");
static_assert(theme_of(BuiltinTheme::striped_haunt).name == "striped-haunt");
static_assert(theme_of(BuiltinTheme::curious_garden).name == "curious-garden");
static_assert(theme_of(BuiltinTheme::tidebound_gold).name == "tidebound-gold");
static_assert(theme_of(BuiltinTheme::velvet_cabaret).name == "velvet-cabaret");
static_assert(theme_of(BuiltinTheme::lagoon_whisper).name == "lagoon-whisper");

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
