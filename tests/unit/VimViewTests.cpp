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
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorFrame;
using nenenib::application::InsertText;
using nenenib::application::PlaceCaret;
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

void verify_scrolloff_cursor_correct()
{
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
    verify_gutter_labels();
    verify_scrolloff_wrapped_rows();
    verify_ordinary_mode_options();
    verify_scrolloff_cursor_correct();
    verify_grid_rows_cursor_correct();
}
} // namespace nenenib::tests
