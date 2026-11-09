// scope `--vim-view` の単体テスト（#1041・ADR 0019 / ADR 0008 の追補）。`scrolloff` の余白と
// `relativenumber` の行番号の文字は画面の値なので oracle では測れず、ここで契約にする。`scrolloff`
// の H M L と Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u の着地（#1099）も窓の高さが要るのでここ。
#include "Column.hpp"
#include "DisplayForm.hpp"
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
#include "ScrollLines.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorFrame;
using nenenib::application::InsertText;
using nenenib::application::PlaceCaret;
using nenenib::application::ScrollLines;
using nenenib::application::SelectDisplay;
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

// 打ち切り（2 × 10 + 1 = 21 行）の手前の跳び（51 行目が先頭の窓から 33G・18 行上）も中央へ寄る。
// 閾値 (10 - 2) / 2 = 4 行より近い跳び（48G・3 行上）は寄せずに最小だけ送る（#1100）。
void verify_far_jump_up_below_limit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    prepared(controller, "0");
    const auto bottom = pressed(controller, U"G");
    expect(bottom.first_visible == LineNumber{51}, "#1100 G fills the window up to the last line");
    const auto up = pressed(controller, U"33G");
    expect(up.caret.position.line == LineNumber{33} && up.first_visible == LineNumber{29},
           "#1100 a jump up short of the distance limit still centers the caret");
    Editing near_editing;
    EditorController &near = near_editing.controller();
    prepared(near, "0");
    static_cast<void>(pressed(near, U"G"));
    const auto stepped = pressed(near, U"48G");
    expect(stepped.caret.position.line == LineNumber{48} && stepped.first_visible == LineNumber{48},
           "#1100 a jump up closer than the threshold scrolls only as far as needed");
}

// so=2 の遠い上への跳びも中央（先頭 26）へ寄り、キャレットは上の余白 2 行の内側に来る。
// 仮の 1 行目で切れていたときは余白の縁（先頭 28）で止まった（#1100）。
void verify_far_jump_up_with_margin()
{
    Editing editing;
    EditorController &controller = editing.controller();
    prepared(controller, "2");
    static_cast<void>(pressed(controller, U"G"));
    const auto up = pressed(controller, U"30G");
    const auto row = up.caret.position.line.value - up.first_visible.value;
    expect(up.caret.position.line == LineNumber{30} && up.first_visible == LineNumber{26} &&
               row > 2,
           "#1100 a far jump up with scrolloff=2 centers the caret inside the margin");
}

// 一続きの鍵（press_vim_keys・fixture の再生の口）は各鍵の後に追従する（Vim の update_topline・
// #1154）。窓は viewport の fixture と同じ幅 0（文字組みの問いが無く、途中の解決が起きない）。
// `G30G` は G で先頭 51 の窓を経てから遠い上への跳びで中央へ寄り（先頭 26）、1 鍵ずつ打ったとき
// と同じ。最後に 1 回だけ解決すると 1 行目の窓から下へ最小に送って先頭 21 になった。
void verify_key_sequence_follows_each_key()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{shown_lines});
    std::string text;
    for (std::size_t line = 1; line < document_lines; ++line)
    {
        text += "line\n";
    }
    text += "line";
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{0}}, SelectionAnchoring::collapse}));
    expect(controller.frame().first_visible == LineNumber{1}, "#1154 the window starts at line 1");
    std::vector<VimKey> keys;
    for (const char32_t key : std::u32string_view{U"G30G"})
    {
        keys.emplace_back(VimCharacter{key});
    }
    const auto sequence = controller.press_vim_keys(keys);
    expect(sequence.caret.position.line == LineNumber{30} &&
               sequence.first_visible == LineNumber{26},
           "#1154 a key sequence follows the caret after each key, through the window of G");
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

// Vim の cursor_correct の着地（#1099・ADR 0019 の追補）。Vim 9.1.0004 を `:set lines=12`（窓 11
// 行）で 100 行の本文に当てた実測（D:/NeNeNib/scripts/1099/gen.py）。行は (余白, 前の先頭, 前の行,
// 鍵, 後の 先頭, 後の行)。前の窓は余白 0 の `{先頭}Gzt{行}G` で作ってから余白を置く。
using Landing =
    std::tuple<std::size_t, std::size_t, std::size_t, std::string_view, std::size_t, std::size_t>;
constexpr std::array<Landing, 28> measured_landings{{
    // 余白が画面の半分を越えると、窓の途中では両端から詰めた範囲が潰れて残った 1
    // 行に着き、窓の先頭が
    // 1 行目なら下の余白は 画面 / 2 で切り詰める。回数つきの Ctrl-f は途中の周で末尾の FAIL
    // に当たる。
    {6, 30, 35, "H", 30, 35},       {6, 30, 35, "L", 30, 35},     {999, 30, 35, "L", 30, 35},
    {6, 1, 1, "L", 1, 6},           {999, 1, 1, "L", 1, 6},       {3, 85, 90, "2<C-f>", 97, 100},
    {3, 1, 1, "H", 1, 1},           {3, 1, 1, "L", 1, 8},         {3, 1, 1, "<C-f>", 10, 13},
    {3, 1, 1, "<C-d>", 6, 9},       {3, 1, 5, "<C-u>", 1, 1},     {3, 27, 30, "2L", 27, 34},
    {3, 30, 35, "H", 30, 33},       {3, 30, 35, "2H", 30, 33},    {3, 30, 35, "5H", 30, 34},
    {3, 30, 35, "M", 30, 35},       {3, 30, 35, "L", 30, 37},     {3, 27, 30, "<C-b>", 18, 25},
    {3, 30, 35, "<C-f>", 39, 42},   {3, 30, 35, "<C-d>", 35, 40}, {3, 90, 100, "<C-u>", 85, 92},
    {3, 90, 100, "H", 90, 93},      {3, 90, 95, "L", 90, 100},    {3, 90, 95, "<C-f>", 97, 100},
    {3, 85, 90, "<C-f>", 94, 97},   {5, 1, 1, "L", 1, 6},         {5, 85, 90, "<C-f>", 94, 99},
    {999, 90, 95, "<C-f>", 90, 95},
}};

constexpr std::size_t measured_window = 11;
constexpr std::size_t measured_document = 100;

// 11 行の窓（幅 0 は文字組みの無い道・400 は折り返しの道）に 100 行の本文を置き、余白 0 で窓を
// 作ってから `:set so=<margin>` を打つ。
void placed_window(EditorController &controller, std::int32_t width, const Landing &row)
{
    applied(controller,
            VisibleLines{measured_window, nenenib::core::TextViewGeometry{width, 20, 96}});
    std::string text;
    for (std::size_t line = 1; line <= measured_document; ++line)
    {
        text += "line" + std::to_string(line) + (line < measured_document ? "\n" : "");
    }
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(run_ex(controller, "set so=0"));
    vim_replay(controller, "gg" + std::to_string(std::get<1>(row)) + "Gzt" +
                               std::to_string(std::get<2>(row)) + "G");
    static_cast<void>(run_ex(controller, "set so=" + std::to_string(std::get<0>(row))));
}

void verify_measured_landings(std::int32_t width)
{
    for (const Landing &row : measured_landings)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        placed_window(controller, width, row);
        vim_replay(controller, std::get<3>(row));
        const auto frame = controller.frame();
        const std::string description =
            "#1099 so=" + std::to_string(std::get<0>(row)) + " " + std::string(std::get<3>(row)) +
            " from line " + std::to_string(std::get<2>(row)) + " lands where Vim 9.1 does (width " +
            std::to_string(width) + ")";
        expect(frame.first_visible == LineNumber{std::get<4>(row)} &&
                   frame.caret.position.line == LineNumber{std::get<5>(row)},
               description.c_str());
    }
}

// キャレットの行だけを確かめる実測（窓の先頭は追従の切り詰めの差で Vim と違う・ADR 0019 の
// 追補 5 の (a)）。so=6 の回数つきの Ctrl-f は 2 周目が末尾の FAIL で、Vim のキャレットは 99。
constexpr std::array<Landing, 1> measured_caret_lines{{
    {6, 85, 90, "2<C-f>", 0, 99},
}};

void verify_measured_caret_lines(std::int32_t width)
{
    for (const Landing &row : measured_caret_lines)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        placed_window(controller, width, row);
        vim_replay(controller, std::get<3>(row));
        const std::string description =
            "#1099 so=" + std::to_string(std::get<0>(row)) + " " + std::string(std::get<3>(row)) +
            " puts the caret on Vim's line (width " + std::to_string(width) + ")";
        expect(controller.frame().caret.position.line == LineNumber{std::get<5>(row)},
               description.c_str());
    }
}

// オペレータの範囲は寄せない（Vim の nv_scroll は op_type が NOP のときだけ cursor_correct
// を呼ぶ）。 VISUAL の H は寄せる。行の数は Vim 9.1 の実測。
void verify_operator_ranges_are_not_corrected(std::int32_t width)
{
    Editing removing;
    EditorController &controller = removing.controller();
    placed_window(controller, width, Landing{3, 27, 30, "", 0, 0});
    vim_replay(controller, "dL");
    const auto removed = whole_vim_body(controller);
    expect(std::ranges::count(removed, '\n') + 1 == 92 &&
               controller.frame().caret.position.line == LineNumber{30},
           "#1099 dL with so=3 removes lines 30 to 37 (L itself is not corrected)");
    Editing visual;
    EditorController &selected = visual.controller();
    placed_window(selected, width, Landing{3, 30, 35, "", 0, 0});
    vim_replay(selected, "vHd");
    const auto cut = whole_vim_body(selected);
    expect(std::ranges::count(cut, '\n') + 1 == 98 &&
               selected.frame().caret.position.line == LineNumber{33},
           "#1099 VISUAL H with so=3 lands 3 lines below the top before d");
}

// 高さの違う行を挟む窓の cursor_correct（#1141・ADR 0019 の追補）。Vim
// は余白を詰めるときに詰めた行の 画面行の数（plines）を足す。Vim 9.1.0004 を `:set lines=12
// columns=40`（窓 11 行・40 桁）で、100 行の 本文の 1 行か 2 行を 190 字の `w`（5
// 画面行）にして測った（D:/NeNeNib/scripts/1141/gen.py・ 前の窓が Vim と同じになる 540
// 件のうち、直す前に 43 件が違った）。行は (高い行, 2 本目の高い行 （0 は無し）, 余白, 前の先頭,
// 前の行, 鍵, 後の先頭, 後の行)。幅 400 の文字組みの替え玉は 40 升。
using TallLanding = std::tuple<std::size_t, std::size_t, std::size_t, std::size_t, std::size_t,
                               std::string_view, std::size_t, std::size_t>;
constexpr std::array<TallLanding, 49> tall_landings{{
    {30, 0, 2, 30, 32, "H", 30, 31},      {30, 0, 2, 30, 32, "M", 30, 31},
    {30, 0, 2, 30, 32, "2H", 30, 31},     {30, 0, 2, 30, 33, "H", 30, 31},
    {30, 0, 2, 30, 33, "M", 30, 31},      {30, 0, 2, 30, 33, "2H", 30, 31},
    {30, 0, 3, 30, 32, "H", 30, 31},      {30, 0, 3, 30, 32, "M", 30, 31},
    {30, 0, 3, 30, 32, "2H", 30, 31},     {30, 0, 3, 30, 32, "<C-b>", 25, 29},
    {30, 0, 3, 30, 33, "H", 30, 31},      {30, 0, 3, 30, 33, "M", 30, 31},
    {30, 0, 3, 30, 33, "2H", 30, 31},     {30, 0, 3, 30, 33, "<C-b>", 25, 29},
    {36, 0, 2, 30, 32, "L", 30, 35},      {36, 0, 2, 30, 32, "M", 30, 35},
    {36, 0, 2, 30, 32, "2L", 30, 35},     {36, 0, 2, 30, 33, "L", 30, 35},
    {36, 0, 2, 30, 33, "M", 30, 35},      {36, 0, 2, 30, 33, "2L", 30, 35},
    {36, 0, 3, 30, 33, "L", 30, 35},      {36, 0, 3, 30, 33, "M", 30, 35},
    {36, 0, 3, 30, 33, "2L", 30, 35},     {36, 0, 3, 30, 33, "<C-f>", 35, 37},
    {36, 0, 3, 26, 30, "<C-d>", 31, 35},  {31, 0, 2, 26, 30, "<C-f>", 31, 32},
    {31, 0, 3, 30, 32, "H", 30, 32},      {31, 0, 3, 30, 32, "M", 30, 32},
    {31, 0, 3, 30, 32, "2H", 30, 32},     {31, 0, 3, 30, 32, "<C-d>", 31, 33},
    {31, 0, 3, 30, 33, "H", 30, 32},      {31, 0, 3, 30, 33, "M", 30, 32},
    {31, 0, 3, 30, 33, "2H", 30, 32},     {31, 0, 3, 26, 30, "L", 26, 30},
    {31, 0, 3, 26, 30, "M", 26, 30},      {31, 0, 3, 26, 30, "2L", 26, 30},
    {31, 0, 3, 26, 30, "<C-f>", 31, 32},  {30, 31, 2, 26, 30, "L", 26, 29},
    {30, 31, 2, 26, 30, "M", 26, 29},     {30, 31, 2, 26, 30, "2L", 26, 29},
    {30, 31, 2, 26, 30, "<C-f>", 31, 32}, {2, 0, 2, 1, 1, "<C-d>", 2, 3},
    {2, 0, 3, 1, 1, "<C-d>", 2, 3},       {30, 0, 2, 1, 1, "L", 1, 9},
    {2, 0, 3, 1, 1, "L", 1, 4},           {36, 0, 2, 26, 30, "L", 26, 33},
    {33, 0, 3, 26, 30, "H", 26, 29},      {30, 0, 2, 26, 30, "H", 26, 28},
    {30, 0, 3, 90, 95, "<C-u>", 85, 90},
}};

// 高い行が窓の下で収まりきらないときの `M`（#1177）。Vim の nv_scroll は半分を
// `(窓の高さ - w_empty_rows + 1) / 2` で数え、収まりきらない行の残りの段は w_empty_rows に入る。
// 同じ実測（D:/NeNeNib/scripts/1141/gen.py）の、前の窓が Vim と同じで直す前に違った 4 件。
constexpr std::array<TallLanding, 4> tall_middles{{
    {36, 0, 2, 26, 30, "M", 26, 30},
    {36, 0, 3, 26, 30, "M", 26, 30},
    {36, 0, 2, 29, 30, "M", 28, 31},
    {33, 0, 2, 26, 30, "M", 26, 29},
}};

void placed_tall_window(EditorController &controller, const TallLanding &row)
{
    applied(controller,
            VisibleLines{measured_window, nenenib::core::TextViewGeometry{400, 20, 96}});
    std::string text;
    for (std::size_t line = 1; line <= measured_document; ++line)
    {
        const bool tall = line == std::get<0>(row) || line == std::get<1>(row);
        text += (tall ? std::string(190, 'w') : "line" + std::to_string(line)) +
                (line < measured_document ? "\n" : "");
    }
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(run_ex(controller, "set so=0"));
    vim_replay(controller, "gg" + std::to_string(std::get<3>(row)) + "Gzt" +
                               std::to_string(std::get<4>(row)) + "G");
    static_cast<void>(run_ex(controller, "set so=" + std::to_string(std::get<2>(row))));
}

void verify_tall_rows(std::span<const TallLanding> rows, std::string_view issue)
{
    for (const TallLanding &row : rows)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        placed_tall_window(controller, row);
        vim_replay(controller, std::get<5>(row));
        const auto frame = controller.frame();
        const std::string description =
            std::string(issue) + " so=" + std::to_string(std::get<2>(row)) + " " +
            std::string(std::get<5>(row)) + " from line " + std::to_string(std::get<4>(row)) +
            " with the 5-row line " + std::to_string(std::get<0>(row)) +
            " lands where Vim 9.1 does";
        expect(frame.first_visible == LineNumber{std::get<6>(row)} &&
                   frame.caret.position.line == LineNumber{std::get<7>(row)},
               description.c_str());
    }
}

void verify_tall_landings()
{
    verify_tall_rows(tall_landings, "#1141");
    verify_tall_rows(tall_middles, "#1177");
}

// Vim モードの追従は Vim の update_topline（#1178・ADR 0019 の追補）。`:set so=` の後の窓と、
// そこからの鍵の後の窓とキャレットを、#1141 と同じ置き方（窓 11 行・40 升・100 行・5 画面行の
// 行）で Vim 9.1.0004 の実測（D:/NeNeNib/scripts/1141/gen.py の 882 件）から選んだ。行は (高い行,
// 2 本目の高い行（0 は無し）, 余白, 前の先頭, 前の行, 鍵, `:set so=` の後の先頭, 鍵の後の先頭,
// 鍵の後の行)。上側の scroll_cursor_top（先頭 30・行 30・so=2 は 28）・中央寄せ・下側の
// scroll_cursor_bot（先頭 90・行 95）・高い行を挟む窓・動かない窓を含む。後ろの 5 行は跳びの後の
// 窓（D:/NeNeNib/scripts/1178/vim/gen.py で同じ置き方の Vim 9.1.0004 を測った）: 下側の
// scroll_cursor_halfway（遠い下への `70G` と高い行を越える `41G`）と、scroll_cursor_bot の送りが
// 窓以上になって上を先に足す halfway（prefer_above）に入る `41G` `38G` `8j`。
using ToplineRow = std::tuple<std::size_t, std::size_t, std::size_t, std::size_t, std::size_t,
                              std::string_view, std::size_t, std::size_t, std::size_t>;
constexpr std::array<ToplineRow, 18> topline_rows{{
    {30, 0, 2, 30, 30, "H", 28, 28, 30},
    {30, 0, 3, 26, 30, "L", 27, 27, 30},
    {30, 0, 3, 29, 30, "<C-d>", 27, 30, 33},
    {36, 0, 2, 29, 30, "L", 28, 28, 33},
    {30, 31, 2, 29, 30, "<C-f>", 29, 32, 34},
    {30, 31, 3, 30, 33, "2H", 31, 31, 32},
    {95, 0, 2, 90, 95, "<C-u>", 91, 86, 90},
    {95, 0, 3, 90, 95, "H", 92, 92, 95},
    {2, 0, 3, 30, 32, "<C-b>", 29, 20, 27},
    {33, 0, 2, 26, 30, "2L", 26, 26, 30},
    {36, 0, 3, 30, 32, "L", 29, 29, 32},
    {31, 0, 2, 30, 30, "<C-d>", 28, 31, 33},
    {30, 0, 2, 26, 30, "H", 26, 26, 28},
    {0, 0, 0, 1, 1, "70G", 1, 65, 70},
    {36, 0, 0, 1, 1, "41G", 1, 37, 41},
    {40, 0, 0, 26, 30, "41G", 26, 40, 41},
    {37, 38, 2, 26, 30, "38G", 26, 37, 38},
    {36, 31, 0, 26, 30, "8j", 26, 36, 38},
}};

void verify_topline_updated()
{
    for (const ToplineRow &row : topline_rows)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        placed_tall_window(controller,
                           TallLanding{std::get<0>(row), std::get<1>(row), std::get<2>(row),
                                       std::get<3>(row), std::get<4>(row), std::get<5>(row), 0, 0});
        const std::string window = "#1178 so=" + std::to_string(std::get<2>(row)) + " top " +
                                   std::to_string(std::get<3>(row)) + " line " +
                                   std::to_string(std::get<4>(row)) + " with the 5-row line " +
                                   std::to_string(std::get<0>(row));
        const auto placed = controller.frame();
        expect(placed.first_visible == LineNumber{std::get<6>(row)} &&
                   placed.caret.position.line == LineNumber{std::get<4>(row)},
               (window + " moves the window where Vim 9.1 update_topline does").c_str());
        vim_replay(controller, std::get<5>(row));
        const auto moved = controller.frame();
        expect(moved.first_visible == LineNumber{std::get<7>(row)} &&
                   moved.caret.position.line == LineNumber{std::get<8>(row)},
               (window + " then " + std::string(std::get<5>(row)) + " lands where Vim 9.1 does")
                   .c_str());
    }
}

// 行単位の削除で本文が短くなり、先頭が本文の外に残ったときの追従（#1230・ADR 0019 の追補
// 2026-11-04）。Vim の w_topline は行数で切られず、n = 先頭 + 余白 - キャレットが halfheight
// 以上なら中央へ寄せる。行は (行数, 空の行（0 は無し）, 窓, 余白, 前の先頭, 前の行, 鍵,
// 鍵の後の先頭, 鍵の後の行)。`GdH` 形（消した後は先頭 40・行数 39・窓 8・so=2 → 32）と `cip`
// 形（先頭 13・行数 9・窓 12・so=3 → 1）で、どちらも fixture viewport-after-linewise-delete- の
// Vim 9.1 の実測と同じ窓。
using ShrunkRow = std::tuple<std::size_t, std::size_t, std::size_t, std::size_t, std::size_t,
                             std::size_t, std::string_view, std::size_t, std::size_t>;
constexpr std::array<ShrunkRow, 2> shrunk_rows{{
    {47, 0, 8, 2, 40, 47, "dH", 32, 39},
    {30, 8, 12, 3, 13, 20, "cip<Esc>", 1, 9},
}};

// 行数の行を "line<N>" で並べ、空の行の番号だけ空にする。
[[nodiscard]] std::string shrunk_text(const ShrunkRow &row)
{
    std::string text;
    for (std::size_t line = 1; line <= std::get<0>(row); ++line)
    {
        text += (line == std::get<1>(row) ? std::string{} : "line" + std::to_string(line)) +
                (line < std::get<0>(row) ? "\n" : "");
    }
    return text;
}

void verify_topline_after_shrinking()
{
    for (const ShrunkRow &row : shrunk_rows)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        applied(controller,
                VisibleLines{std::get<2>(row), nenenib::core::TextViewGeometry{400, 20, 96}});
        static_cast<void>(controller.apply(InsertText{shrunk_text(row)}));
        applied(controller, SelectEditMode{EditMode::vim});
        static_cast<void>(run_ex(controller, "set so=0"));
        vim_replay(controller, "gg" + std::to_string(std::get<4>(row)) + "Gzt" +
                                   std::to_string(std::get<5>(row)) + "G");
        static_cast<void>(run_ex(controller, "set so=" + std::to_string(std::get<3>(row))));
        const std::string window = "#1230 so=" + std::to_string(std::get<3>(row)) + " window " +
                                   std::to_string(std::get<2>(row)) + " top " +
                                   std::to_string(std::get<4>(row)) + " line " +
                                   std::to_string(std::get<5>(row));
        const auto placed = controller.frame();
        expect(placed.first_visible == LineNumber{std::get<4>(row)} &&
                   placed.caret.position.line == LineNumber{std::get<5>(row)},
               (window + " is placed before the keys").c_str());
        vim_replay(controller, std::get<6>(row));
        const auto moved = controller.frame();
        expect(moved.first_visible == LineNumber{std::get<7>(row)} &&
                   moved.caret.position.line == LineNumber{std::get<8>(row)},
               (window + " then " + std::string(std::get<6>(row)) +
                " recenters from the top left past the last line as Vim 9.1 does")
                   .c_str());
    }
}

// 窓より高い行（窓 11 行に 600 字 = 15 画面行の行 30）では、Vim の答えの窓（先頭 30 の段 0）で
// キャレットの画面行が見えなくなると画面行の追従へ戻る（#1178・ADR 0019 の追補の決定 3）。`j` で
// その行に入ると先頭 30 の段 0、`12gj` で 12 段目へ下りると窓は段の途中から始まり、キャレットの段が
// 見えている。
void verify_taller_than_window()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller,
            VisibleLines{measured_window, nenenib::core::TextViewGeometry{400, 20, 96}});
    std::string text;
    for (std::size_t line = 1; line <= measured_document; ++line)
    {
        text += (line == 30 ? std::string(600, 'w') : "line" + std::to_string(line)) +
                (line < measured_document ? "\n" : "");
    }
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "gg25Gzt");
    vim_replay(controller, "5j");
    const auto entered = controller.frame();
    expect(entered.first_visible == LineNumber{30} && !entered.lines.empty() &&
               entered.lines.front().screen_row == 0 &&
               entered.caret.position.line == LineNumber{30},
           "#1178 j onto a line taller than the window puts that line at the top (Vim halfway)");
    vim_replay(controller, "12gj");
    const auto deep = controller.frame();
    const auto first_row = deep.lines.empty() ? std::int64_t{0} : -deep.lines.front().screen_row;
    const auto caret_row = static_cast<std::int64_t>(deep.caret.position.column.value / 40);
    expect(deep.first_visible == LineNumber{30} && first_row > 0 && caret_row >= first_row &&
               caret_row - first_row < static_cast<std::int64_t>(measured_window),
           "#1178 12gj inside the tall line falls back to the screen-row follow and keeps the "
           "caret row visible");
}

// 追従が文字組みするのは描く行と core が問うた行だけ（#1178 の独立レビュー R4-1 / R4-2・ADR
// 0128）。 どの行も 3 画面行（120 字）で窓 11 行には 4 行しか描かない文書で、`gg`・窓の下端での `j`
// の連打・ INSERT の打鍵の間に新しく作られた layout の数は、frame
// が初めて描いた行の数と打った文字の数を
// 越えない（窓の外を論理行の固定幅で先読みすると越える）。本文は寸法が届く前に入れて、まだどの行も
// 文字組みしていない所から数える。
// 鍵を打って frame を描き、初めて描いた行の数を返す。
[[nodiscard]] std::size_t newly_drawn(EditorController &controller, std::set<std::size_t> &drawn,
                                      std::string_view keys)
{
    vim_replay(controller, keys);
    std::size_t newly = 0;
    for (const auto &line : controller.frame().lines)
    {
        newly += drawn.insert(line.number.value).second ? 1 : 0;
    }
    return newly;
}

void verify_follow_reads_drawn_lines()
{
    Editing editing;
    EditorController &controller = editing.controller();
    std::string text;
    for (std::size_t line = 1; line <= measured_document; ++line)
    {
        text += "line" + std::to_string(line) + std::string(110, 'w') +
                (line < measured_document ? "\n" : "");
    }
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    applied(controller,
            VisibleLines{measured_window, nenenib::core::TextViewGeometry{400, 20, 96}});
    std::set<std::size_t> drawn;
    static_cast<void>(newly_drawn(controller, drawn, ""));
    const std::size_t before = editing.text_layouts().made();
    std::size_t newly = newly_drawn(controller, drawn, "gg");
    for (std::size_t press = 0; press < 20; ++press)
    {
        newly += newly_drawn(controller, drawn, "j");
    }
    newly += newly_drawn(controller, drawn, "ixyz");
    expect(controller.frame().first_visible.value > 1 &&
               editing.text_layouts().made() - before <= newly + 3,
           "#1178 the Vim follow lays out only the drawn lines and the lines it asks for");
}

// `so` が窓の高さより大きい（`so=999`）ときの窓の外への跳び（#1214・ADR 0019 の追補 #1178 の
// 決定 5 の (e)）。Vim の `update_topline` は `check_top_offset` が真でも偽でも
// `scroll_cursor_halfway` に行くので、追従はキャレットの上の行を歩いて測らない。どの行も 5
// 画面行（全角 198 字と 4 桁の番号・80 升）の 1000 行の文書の 100 行目から `500j` を 1
// 回打つ間に新しく作られる layout は、窓 1 つ分（40 本） 以下（直す前は上の行を 1 本ずつ測って 172
// 本）。着地は Vim の halfway（先頭 597）。
void verify_far_jump_with_large_margin()
{
    constexpr std::size_t window = 40;
    Editing editing;
    EditorController &controller = editing.controller();
    std::string text;
    std::string line;
    for (std::size_t column = 0; column < 198; ++column)
    {
        line += "あ";
    }
    // 行ごとに違う文字列（替え玉は同じ文字列の layout を 1 本と数える）で、どれも 400 升。
    for (std::size_t number = 1; number <= 1000; ++number)
    {
        text += line + std::to_string(10000 + number).substr(1) + (number < 1000 ? "\n" : "");
    }
    static_cast<void>(controller.apply(InsertText{text}));
    applied(controller, SelectEditMode{EditMode::vim});
    applied(controller, VisibleLines{window, nenenib::core::TextViewGeometry{800, 20, 96}});
    static_cast<void>(run_ex(controller, "set so=999"));
    vim_replay(controller, "100G");
    static_cast<void>(controller.frame());
    const std::size_t before = editing.text_layouts().made();
    vim_replay(controller, "500j");
    const auto jumped = controller.frame();
    const std::size_t made = editing.text_layouts().made() - before;
    expect(jumped.first_visible == LineNumber{597} && jumped.caret.position.line == LineNumber{600},
           "#1214 a far jump down with so=999 centers the caret as Vim's scroll_cursor_halfway");
    expect(made <= window, ("#1214 a far jump down with so=999 lays out at most one window of "
                            "lines (made " +
                            std::to_string(made) + ")")
                               .c_str());
}

// 「通常 | Vim」の切り替えは窓を動かさない（追従を頼まない）。so=3 の Vim の update_topline なら
// 先頭 30・行 30 の窓を上げるが、切り替えた時点の先頭を保つ。
void verify_mode_switch_keeps_window()
{
    Editing editing;
    EditorController &controller = editing.controller();
    placed_tall_window(controller, TallLanding{36, 0, 3, 1, 1, "", 0, 0});
    applied(controller, SelectEditMode{EditMode::ordinary});
    applied(controller, ScrollLines{29});
    const auto scrolled = controller.frame();
    applied(controller, SelectEditMode{EditMode::vim});
    const auto entered = controller.frame();
    expect(entered.first_visible == scrolled.first_visible &&
               entered.first_visible == LineNumber{30} &&
               entered.caret.position.line == LineNumber{30},
           "#1178 switching to Vim keeps the window where update_topline with so=3 would move it");
}

void verify_scrolloff_cursor_correct()
{
    verify_tall_landings();
    for (const std::int32_t width : {0, 400})
    {
        verify_measured_landings(width);
        verify_measured_caret_lines(width);
        verify_operator_ranges_are_not_corrected(width);
    }
}
// 格子の画面行の単位の半画面と 1 画面も同じ寄せ（#1142・ADR 0019 の追補 7）。Vim に格子は
// 無いので、論理行の道の規則を画面行に読み替えた形を固定する。8 升の幅・見えている画面行は 10 で、
// 200 字の 1 行は 25 画面行。キャレットは 0 画面行の升 2（3 桁目）から始まり、画面行 r の同じ升は
// 3 + 8r 桁目。
void open_grid_window(EditorController &controller, std::string_view margin)
{
    applied(controller, VisibleLines{shown_lines, nenenib::core::TextViewGeometry{85, 20, 96}});
    applied(controller, InsertText{std::string(200, 'a')});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{3}}, SelectionAnchoring::collapse});
    applied(controller, SelectDisplay{nenenib::core::DisplayForm::grid});
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(run_ex(controller, "set so=" + std::string(margin)));
}

// 最上段の画面行とキャレットの桁。
[[nodiscard]] std::pair<std::size_t, std::size_t> grid_top_and_column(EditorController &controller)
{
    const auto frame = controller.frame();
    const std::size_t top = frame.grid.empty() ? 999 : frame.grid.front().row;
    return {top, frame.caret.position.column.value};
}

using GridPlace = std::pair<std::size_t, std::size_t>;

// 読み替えの 2 つの端（R5-1）。下の余白: 末尾の見える窓（下の余白 0）で最後から 2 番目の画面行に
// いたキャレットは、Ctrl-b の後の末尾の見えない窓では下に 3 画面行を残すように上へ寄る（Ctrl-d 4 回
// で先頭 15・キャレット 23、Ctrl-b で先頭 5・キャレット 13 → 11）。上の余白 0: 余白 0 で置いた
// キャレットから so=3 の Ctrl-u で最上段が文書の最初の画面行に戻ると、上へは寄せない。
void verify_grid_rows_edges()
{
    Editing lower;
    EditorController &below = lower.controller();
    open_grid_window(below, "3");
    vim_replay(below, "<C-d><C-d><C-d><C-d>");
    expect(grid_top_and_column(below) == GridPlace{15, 187},
           "#1142 grid Ctrl-d to the end leaves no lower margin");
    vim_replay(below, "<C-b>");
    expect(grid_top_and_column(below) == GridPlace{5, 91},
           "#1142 grid Ctrl-b keeps 3 screen rows below the caret when the end is hidden");
    Editing upper;
    EditorController &above = upper.controller();
    open_grid_window(above, "0");
    vim_replay(above, "<C-d>gj");
    static_cast<void>(run_ex(above, "set so=3"));
    vim_replay(above, "<C-u>");
    expect(grid_top_and_column(above) == GridPlace{0, 11},
           "#1142 grid Ctrl-u back to the first screen row has no upper margin");
}

void verify_grid_rows_cursor_correct()
{
    Editing paged;
    EditorController &forward = paged.controller();
    open_grid_window(forward, "3");
    vim_replay(forward, "<C-f>");
    expect(grid_top_and_column(forward) == GridPlace{10, 107},
           "#1142 grid Ctrl-f with so=3 puts the caret 3 screen rows below the new top");
    vim_replay(forward, "<C-b>");
    expect(grid_top_and_column(forward) == GridPlace{0, 27},
           "#1142 grid Ctrl-b back to the first screen row keeps the caret (no upper margin)");
    Editing ended;
    EditorController &end = ended.controller();
    open_grid_window(end, "3");
    vim_replay(end, "2<C-f>");
    expect(grid_top_and_column(end) == GridPlace{20, 187},
           "#1142 grid 2 Ctrl-f to the end keeps the upper margin and drops the lower one");
    Editing halved;
    EditorController &half = halved.controller();
    open_grid_window(half, "3");
    vim_replay(half, "<C-d>");
    expect(grid_top_and_column(half) == GridPlace{5, 67},
           "#1142 grid Ctrl-d with so=3 puts the caret 3 screen rows below the new top");
    vim_replay(half, "<C-u>");
    expect(grid_top_and_column(half) == GridPlace{0, 27},
           "#1142 grid Ctrl-u back to the first screen row moves the caret by half a screen only");
    Editing plain;
    EditorController &unmargined = plain.controller();
    open_grid_window(unmargined, "0");
    vim_replay(unmargined, "<C-f>");
    expect(grid_top_and_column(unmargined) == GridPlace{10, 83},
           "#1142 grid Ctrl-f with so=0 is unchanged (the caret on the new top row)");
    verify_grid_rows_edges();
}

// INSERT の PgUp / PgDn が端で送れないとき、Vim の ins_pageup / ins_pagedown は onepage の FAIL で
// ビープだけを返し start_arrow を呼ばない。回数の反復も `.` の記録も切らない（#1149・ADR 0028 の
// 追補）。窓の高さが要るので oracle では測れず、ここで契約にする。
void verify_insert_page_edges()
{
    Editing top;
    EditorController &at_top = top.controller();
    prepared(at_top, "0");
    vim_replay(at_top, "2ix<PageUp>y<Esc>j0.");
    expect(whole_vim_body(at_top).starts_with("xyxyline\nxyxyline\nline\n") &&
               at_top.frame().first_visible == LineNumber{1},
           "#1149 PgUp at the first page keeps the count and the record of the insert");
    Editing bottom;
    EditorController &at_bottom = bottom.controller();
    prepared(at_bottom, "0");
    vim_replay(at_bottom, "G<C-f>");
    expect(at_bottom.frame().first_visible == LineNumber{document_lines},
           "#1149 Ctrl-f from the last page puts the last line at the top");
    vim_replay(at_bottom, "Ax<PageDown>y<Esc>k.");
    expect(whole_vim_body(at_bottom).ends_with("\nline\nlinexy\nlinexy"),
           "#1149 PgDn at the last line keeps the record of the insert");
    // 対照: 末尾の窓からの PgUp は先頭の行が動くので、回数の反復を切る（x は 1 回だけ）。
    Editing moving;
    EditorController &moved = moving.controller();
    prepared(moved, "0");
    vim_replay(moved, "G");
    const LineNumber before = moved.frame().first_visible;
    vim_replay(moved, "2ix<PageUp>y<Esc>");
    const auto body = whole_vim_body(moved);
    expect(body.ends_with("\nxline") && std::ranges::count(body, 'x') == 1 &&
               std::ranges::count(body, 'y') == 1 && moved.frame().first_visible != before,
           "#1149 a moving PgUp cancels the count of the insert");
}
} // namespace

void verify_vim_view_scope()
{
    verify_scrolloff_follow();
    verify_scrolloff_alignment();
    verify_scrolloff_center();
    verify_far_jump_up_centers();
    verify_far_jump_up_below_limit();
    verify_far_jump_up_with_margin();
    verify_key_sequence_follows_each_key();
    verify_gutter_labels();
    verify_scrolloff_wrapped_rows();
    verify_ordinary_mode_options();
    verify_scrolloff_cursor_correct();
    verify_grid_rows_cursor_correct();
    verify_insert_page_edges();
    verify_topline_updated();
    verify_topline_after_shrinking();
    verify_mode_switch_keeps_window();
    verify_taller_than_window();
    verify_follow_reads_drawn_lines();
    verify_far_jump_with_large_margin();
}
} // namespace nenenib::tests
