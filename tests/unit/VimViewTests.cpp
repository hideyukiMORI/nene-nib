// scope `--vim-view` の単体テスト（#1041・ADR 0019 / ADR 0008 の追補）。`scrolloff` の余白と
// `relativenumber` の行番号の文字は画面の値なので oracle では測れず、ここで契約にする。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "GutterAlignment.hpp"
#include "GutterLabels.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "ScriptedSettings.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorFrame;
using nenenib::application::InsertText;
using nenenib::application::PlaceCaret;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::GutterAlignment;
using nenenib::core::LineNumber;
using nenenib::core::SelectionAnchoring;
using nenenib::core::TextPosition;
using nenenib::core::VimCharacter;
using nenenib::core::VimKey;

constexpr std::size_t shown_lines = 10;
constexpr std::size_t document_lines = 60;

// 10 行の窓に 60 行の本文を置き、Vim にして `:set so=<margin>` を打ってから gg で先頭へ。
void prepared(EditorController &controller, std::string_view margin)
{
    applied(controller, VisibleLines{shown_lines, nenenib::core::TextViewGeometry{400, 20, 96}});
    std::string text;
    for (std::size_t line = 1; line < document_lines; ++line)
    {
        text += "line\n";
    }
    text += "line";
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(run_ex(controller, "set so=" + std::string(margin)));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'g'}}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'g'}}}));
}

EditorFrame pressed(EditorController &controller, std::u32string_view keys)
{
    for (const char32_t key : keys)
    {
        static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{key}}}));
    }
    return controller.frame();
}

// j で下りると、キャレットは下の余白に入る前に窓を送る。先頭では上の余白は効かない。
void verify_scrolloff_follow()
{
    Editing editing;
    EditorController &controller = editing.controller();
    prepared(controller, "3");
    const auto top = controller.frame();
    expect(top.first_visible == LineNumber{1} && top.caret.position.line == LineNumber{1},
           "#1041 the margin gives way at the first line");
    const auto six = pressed(controller, U"jjjjjj");
    expect(six.first_visible == LineNumber{1},
           "#1041 line 7 of a 10-line window is outside the lower margin of 3");
    const auto seven = pressed(controller, U"j");
    expect(seven.first_visible == LineNumber{2} && seven.caret.position.line == LineNumber{8},
           "#1041 line 8 would enter the lower margin, so the window scrolls by one");
    const auto back = pressed(controller, U"kkkk");
    expect(back.first_visible == LineNumber{1} && back.caret.position.line == LineNumber{4},
           "#1041 moving up keeps 3 lines above the caret");
}

// zt zb は余白の分だけ戻り（Vim の zt は so 行を上に残す）、zz は余白に依らない。
void verify_scrolloff_alignment()
{
    Editing editing;
    EditorController &controller = editing.controller();
    prepared(controller, "3");
    const auto top = pressed(controller, U"30Gzt");
    expect(top.caret.position.line == LineNumber{30} && top.first_visible == LineNumber{27},
           "#1041 zt leaves the 3-line margin above the caret");
    const auto bottom = pressed(controller, U"zb");
    expect(bottom.first_visible == LineNumber{24},
           "#1041 zb leaves the 3-line margin below the caret");
    Editing plain;
    EditorController &unmargined = plain.controller();
    prepared(unmargined, "0");
    const auto centered = pressed(controller, U"zz");
    const auto centered_plain = pressed(unmargined, U"30Gzz");
    expect(centered.first_visible == centered_plain.first_visible,
           "#1041 zz centers the same way with or without a margin");
    const auto plain_top = pressed(unmargined, U"zt");
    expect(plain_top.first_visible == LineNumber{30}, "#1041 scrolloff=0 keeps zt as before");
}

// 999 は常に中央（(10 - 1) / 2 = 4 行の余白に切り詰める）。
void verify_scrolloff_center()
{
    Editing editing;
    EditorController &controller = editing.controller();
    prepared(controller, "999");
    const auto moved = pressed(controller, U"jjjjjjjjjjjjjjjjjjjj");
    const auto row = moved.caret.position.line.value - moved.first_visible.value;
    expect(moved.caret.position.line == LineNumber{21} && row >= 4 && row <= 5,
           "#1041 scrolloff=999 keeps the caret in the middle of the window");
}

// 上へ遠く跳ぶ（G の後の 30G）と、Vim の scroll_cursor_halfway と同じくキャレットを中央へ寄せる
// （(10 - 1) / 2 = 4 行上が先頭）。仮の座標の 1 行目で切れて最上段に置かない（#1100）。
void verify_far_jump_up_centers()
{
    Editing editing;
    EditorController &controller = editing.controller();
    prepared(controller, "0");
    const auto bottom = pressed(controller, U"G");
    expect(bottom.caret.position.line == LineNumber{document_lines},
           "#1100 G goes to the last line");
    const auto up = pressed(controller, U"30G");
    expect(up.caret.position.line == LineNumber{30} && up.first_visible == LineNumber{26},
           "#1100 a far jump up centers the caret like Vim's scroll_cursor_halfway");
}

// 行番号の欄の文字（ADR 0008 の追補）。relativenumber はキャレットの行を絶対の番号で左寄せ、
// ほかの行を距離で右寄せにする。
void verify_gutter_labels()
{
    const auto plain = nenenib::core::gutter_label(LineNumber{12}, LineNumber{10}, false);
    expect(plain.text == "12" && plain.alignment == GutterAlignment::trailing,
           "#1041 without relativenumber the absolute number is right-aligned");
    const auto current = nenenib::core::gutter_label(LineNumber{10}, LineNumber{10}, true);
    expect(current.text == "10" && current.alignment == GutterAlignment::leading,
           "#1041 the caret line shows its absolute number on the left");
    const auto above = nenenib::core::gutter_label(LineNumber{7}, LineNumber{10}, true);
    const auto below = nenenib::core::gutter_label(LineNumber{13}, LineNumber{10}, true);
    expect(above.text == "3" && below.text == "3" && above.alignment == GutterAlignment::trailing &&
               below.alignment == GutterAlignment::trailing,
           "#1041 other lines show the distance from the caret line on the right");
}
// 折り返した行（ADR 0019 の追補の決定 2）: 余白は画面行で数える。幅 40 升の窓で 6 行目は
// 3 画面行。`so=2` の `j` は、キャレットの下に 2 画面行が残るように画面行の単位で送る。
void verify_scrolloff_wrapped_rows()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{shown_lines, nenenib::core::TextViewGeometry{400, 20, 96}});
    std::string text = "line\nline\nline\nline\nline\n" + std::string(100, 'w') + "\n";
    for (std::size_t line = 0; line < 30; ++line)
    {
        text += "line\n";
    }
    text += "line";
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(run_ex(controller, "set so=2"));
    static_cast<void>(pressed(controller, U"gg"));
    const auto seventh = pressed(controller, U"jjjjjj");
    expect(seventh.caret.position.line == LineNumber{7} && seventh.first_visible == LineNumber{2},
           "#1041 line 7 on screen row 8 scrolls one screen row to keep 2 rows below");
    const auto ninth = pressed(controller, U"jj");
    expect(ninth.caret.position.line == LineNumber{9} && ninth.first_visible == LineNumber{4},
           "#1041 the margin keeps counting screen rows past the wrapped line");
}

// 通常モードでも効く（ADR 0019 / 0008 の追補・設定は窓の値で wrap と同じくモードを見ない）。
// `so=2` のクリックは余白の内側へ送り、`rnu` の行番号は相対。
void verify_ordinary_mode_options()
{
    auto settings = nenenib::core::default_editor_settings();
    settings.scrolloff = 2;
    settings.relativenumber = true;
    Editing editing(SettingsReading{std::optional{settings}});
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{shown_lines, nenenib::core::TextViewGeometry{400, 20, 96}});
    std::string text;
    for (std::size_t line = 1; line < document_lines; ++line)
    {
        text += "line\n";
    }
    text += "line";
    static_cast<void>(controller.apply(InsertText{text}));
    const auto end = controller.frame();
    expect(end.first_visible == LineNumber{51}, "#1041 the ordinary window shows the last lines");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{52}, Column{0}}, SelectionAnchoring::collapse});
    const auto clicked = controller.frame();
    expect(clicked.caret.position.line == LineNumber{52} && clicked.first_visible == LineNumber{50},
           "#1041 a click inside the upper margin scrolls the ordinary window by the margin");
    const auto label = nenenib::core::gutter_label(LineNumber{50}, clicked.caret.position.line,
                                                   clicked.settings.relativenumber);
    expect(clicked.settings.relativenumber && label.text == "2",
           "#1041 relativenumber numbers the ordinary gutter from the caret line");
}
} // namespace

void verify_vim_view_scope()
{
    verify_scrolloff_follow();
    verify_scrolloff_alignment();
    verify_scrolloff_center();
    verify_far_jump_up_centers();
    verify_gutter_labels();
    verify_scrolloff_wrapped_rows();
    verify_ordinary_mode_options();
}
} // namespace nenenib::tests
