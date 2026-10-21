// scope `--syntax` の単体テスト（ADR 0042 決定 2・#816・ADR 0156 の決定 7〜10・14）。字句器を
// 持つ言語の文書で見えている行に色の区間が付くこと、持たない言語では区間が空で字句器も覚えも
// 触らないこと、見えている最初の行の 200 行前からの遡りと覚えから続ける道、改訂・タブ・枠で
// 作り直すこと、巨大な行の窓・IME の差し込み行・格子が色を持たないこと、役割から色への写しが
// core の 1 本であること。
#include "ColorSpan.hpp"
#include "ComposeText.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "GiantLine.hpp"
#include "InsertText.hpp"
#include "LineView.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PlaceCaret.hpp"
#include "RgbColor.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchTab.hpp"
#include "SyntaxColor.hpp"
#include "SyntaxLines.hpp"
#include "SyntaxPalette.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::SyntaxRole;

constexpr std::size_t shown_lines = 10;
constexpr std::string_view code_path = "C:\\work\\a.cpp";
constexpr std::string_view other_code_path = "C:\\work\\b.cpp";
constexpr std::string_view text_path = "C:\\work\\a.txt";
constexpr std::string_view sample = "int f(void) {\n    // c\n    return \"s\";\n}\n";
constexpr core::TextViewGeometry grid_geometry{85, 20, 96};
constexpr core::TextViewGeometry text_geometry{200, 20, 96, 10.0F};

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] std::string repeated(std::string_view text, std::size_t count)
{
    std::string result;
    for (std::size_t index = 0; index < count; ++index)
    {
        result += text;
    }
    return result;
}

// before 行の本文の次の行で `/*` を開き、閉じないまま after 行の本文が続く文書。
[[nodiscard]] std::string open_comment(std::size_t before, std::size_t after)
{
    return repeated("text\n", before) + "/*\n" + repeated("text\n", after);
}

// 151 行目で開いたコメントが続く文書（300 行目のあたりからの 200 行の遡りが届く）。
[[nodiscard]] std::string nearby_comment()
{
    return open_comment(150, 600);
}

[[nodiscard]] bool has_span(const app::LineView &line, std::size_t begin, std::size_t end,
                            SyntaxRole role)
{
    const core::ColorSpan wanted{core::Column{begin}, core::Column{end}, role};
    return std::ranges::find(line.colors, wanted) != line.colors.end();
}

[[nodiscard]] bool all_comment(const app::LineView &line)
{
    return line.colors.size() == 1 && line.colors.front().role == SyntaxRole::comment;
}

[[nodiscard]] bool no_colors(const app::EditorFrame &frame)
{
    return std::ranges::all_of(frame.lines,
                               [](const app::LineView &line) { return line.colors.empty(); });
}

[[nodiscard]] const app::PaneFrame *pane_of(const app::EditorFrame &frame)
{
    return frame.other.has_value() ? std::get_if<app::PaneFrame>(&frame.other.value()) : nullptr;
}

void open(Editing &editing, std::string_view path, std::string text)
{
    editing.files().hold_at(std::string(path), Bytes{std::move(text)});
    static_cast<void>(editing.controller().apply(app::OpenDocument{path_of(path)}));
}

// frame を作らずに縦へ送る（覚えを作らない）。
void scrolled(app::EditorController &controller, std::int32_t lines)
{
    static_cast<void>(controller.apply(app::ScrollLines{lines}));
}

// 決定 7・10: `.cpp` の見えている行に本文の桁の区間が付く。CRLF の `\r` は区間に入らない。
void verify_colored_lines()
{
    for (const std::string_view newline : {"\n", "\r\n"})
    {
        std::string text(sample);
        if (newline == "\r\n")
        {
            text = "int f(void) {\r\n    // c\r\n    return \"s\";\r\n}\r\n";
        }
        Editing editing;
        static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
        open(editing, code_path, text);
        const auto frame = editing.controller().frame();
        expect(frame.lines.size() == 5, "the sample shows its five lines");
        const auto &first = frame.lines.at(0);
        expect(has_span(first, 1, 4, SyntaxRole::type) &&
                   has_span(first, 5, 6, SyntaxRole::function) &&
                   has_span(first, 7, 11, SyntaxRole::type),
               "int is a type, f( is a function and void is a type in the frame");
        expect(has_span(frame.lines.at(1), 5, 9, SyntaxRole::comment),
               "// c is a comment up to the end of the line without the line break");
        expect(has_span(frame.lines.at(2), 5, 11, SyntaxRole::keyword) &&
                   has_span(frame.lines.at(2), 12, 15, SyntaxRole::string),
               "return is a keyword and \"s\" is a string");
        expect(frame.lines.at(4).colors.empty(), "the empty last line has no span");
        expect(frame.syntax_lexed == 5, "the first lines are lexed once each without backfill");
    }
}

// 決定 14: 字句器を持たない言語では区間が空で、字句器に 1 行も通さず、覚えにも触れない
// （.txt を見ている間も .cpp の覚えは残っていて、戻ると遡らずに続く）。
void verify_plain_untouched()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    const auto code = controller.frame();
    expect(code.first_visible.value == 300 && code.syntax_lexed == 200 + shown_lines,
           "the first frame of the code backfills 200 lines and lexes the visible ones");
    open(editing, text_path, std::string(sample));
    const auto plain = controller.frame();
    expect(no_colors(plain) && plain.syntax_lexed == 0 && !plain.lines.empty(),
           "a .txt document has no spans and lexes no line");
    static_cast<void>(controller.frame());
    const auto back = controller.apply_frame(app::SwitchTab{0});
    expect(back.first_visible.value == 300 && back.syntax_lexed == shown_lines &&
               all_comment(back.lines.front()),
           "coming back continues from the cache the .txt frames left untouched");
}

// 決定 8: 見えている最初の行の 200 行前から遡る。201 行前で開いたコメントは見えない（限界）。
void verify_backfill_reach()
{
    for (const auto &[scroll, comment] :
         std::array<std::pair<std::int32_t, bool>, 2>{{{200, true}, {201, false}}})
    {
        Editing editing;
        auto &controller = editing.controller();
        static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
        open(editing, code_path, open_comment(0, 600));
        scrolled(controller, scroll);
        const auto frame = controller.frame();
        const bool shown = frame.first_visible.value == static_cast<std::size_t>(scroll) + 1;
        expect(shown && frame.syntax_lexed == 200 + shown_lines,
               "a fresh frame backfills exactly 200 lines");
        expect(comment ? all_comment(frame.lines.front()) : frame.lines.front().colors.empty(),
               comment ? "a comment opened 200 lines above the first visible line is seen"
                       : "a comment opened 201 lines above is not seen without a cache");
    }
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, open_comment(0, 600));
    static_cast<void>(controller.frame());
    scrolled(controller, 400);
    const auto far = controller.frame();
    expect(all_comment(far.lines.front()) && far.syntax_lexed == 400 + shown_lines,
           "a cache within 4,096 lines carries the comment past the 200 line reach");
}

// 決定 9: スクロールで覚えから続け、上へ戻ると作り直し、編集（改訂）で作り直す。
void verify_cache_follows()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    expect(controller.frame().syntax_lexed == 200 + shown_lines, "the first frame backfills");
    expect(controller.frame().syntax_lexed == shown_lines,
           "the same frame again starts at the cached line");
    scrolled(controller, 10);
    const auto down = controller.frame();
    expect(down.first_visible.value == 310 && down.syntax_lexed == 10 + shown_lines,
           "scrolling down continues from the cached line");
    scrolled(controller, -5);
    const auto up = controller.frame();
    expect(up.first_visible.value == 305 && up.syntax_lexed == 200 + shown_lines,
           "scrolling up above the cached line backfills again");
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{306}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    expect(controller.frame().syntax_lexed == shown_lines,
           "moving the caret without an edit keeps the cache");
    static_cast<void>(controller.apply(app::InsertText{"x"}));
    const auto edited = controller.frame();
    expect(edited.first_visible.value == 305 && edited.syntax_lexed == 200 + shown_lines &&
               all_comment(edited.lines.front()),
           "an edit changes the revision and backfills again");
}

// 決定 9: タブの切り替えで作り直し、もう 1 つの枠は別の覚えで別の文書の色を持つ。
void verify_tabs_and_panes()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    static_cast<void>(controller.frame());
    open(editing, other_code_path, nearby_comment());
    scrolled(controller, 299);
    expect(controller.frame().syntax_lexed == 200 + shown_lines,
           "another tab does not reuse the first tab's cache");
    const auto switched = controller.apply_frame(app::SwitchTab{0});
    expect(switched.syntax_lexed == 200 + shown_lines,
           "switching tabs rebuilds since the cache now belongs to the other tab");
    static_cast<void>(controller.apply(app::SplitPane{core::SplitLayout::vertical}));
    static_cast<void>(controller.apply(app::SwitchTab{1}));
    static_cast<void>(controller.frame());
    const auto split = controller.frame();
    const auto *other = pane_of(split);
    expect(other != nullptr && !other->lines.empty() && all_comment(other->lines.front()),
           "the other pane colors its own document");
    expect(split.syntax_lexed == shown_lines,
           "the active pane keeps its cache while the other pane draws another document");
    open(editing, text_path, std::string(sample));
    const auto mixed = controller.frame();
    const auto *code = pane_of(mixed);
    expect(no_colors(mixed) && code != nullptr && all_comment(code->lines.front()),
           "a .txt pane has no spans next to a colored .cpp pane");
}

// 決定 9: `:set filetype=` で言語が変わると覚えを作り直し、字句器の無い言語では色が消える。
void verify_language_switch()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    static_cast<void>(controller.frame());
    expect(controller.frame().syntax_lexed == shown_lines, "the cpp frame keeps its cache");
    const auto plain = run_ex(controller, "set ft=text");
    expect(no_colors(plain) && plain.syntax_lexed == 0,
           "setting filetype=text drops the colors and lexes nothing");
    const auto back = run_ex(controller, "set ft=c");
    expect(back.syntax_lexed == 200 + shown_lines && all_comment(back.lines.front()),
           "setting filetype=c rebuilds since the cache was made for cpp");
}

// 決定 9: 無題の空のタブへ開くと、同じ退避番号で改訂番号 0 のまま本文が替わるので覚えを捨てる。
void verify_opened_into_untitled()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(run_ex(controller, "set ft=cpp"));
    expect(controller.frame().syntax_lexed == 1, "the untitled cpp tab lexes its one line");
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    const auto opened = controller.frame();
    expect(opened.first_visible.value == 300 && opened.syntax_lexed == 200 + shown_lines &&
               all_comment(opened.lines.front()),
           "the first frame after opening into the untitled tab does not reuse its cache");
}

// 決定 7: 巨大な行の窓・IME の差し込み行・格子は色を持たない。
void verify_uncolored_rows()
{
    Editing giant;
    auto &controller = giant.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines, text_geometry}));
    open(giant, code_path, "int a;\n" + repeated("x", core::giant_line_threshold + 1) + "\nint b;");
    static_cast<void>(controller.apply(app::SelectDisplay{core::DisplayForm::text}));
    const auto windowed = controller.frame();
    expect(windowed.lines.size() == 3 && windowed.lines.at(1).window.active &&
               windowed.lines.at(1).colors.empty() &&
               has_span(windowed.lines.at(2), 1, 4, SyntaxRole::type),
           "a giant line's window has no spans and the next line is lexed from plain");

    Editing composing;
    static_cast<void>(composing.controller().apply(app::VisibleLines{shown_lines}));
    open(composing, code_path, std::string(sample));
    const auto composed =
        composing.controller().apply_frame(app::ComposeText{composed_of("にほん", {}, 9)});
    expect(composed.lines.at(0).composed.has_value() && composed.lines.at(0).colors.empty() &&
               has_span(composed.lines.at(1), 5, 9, SyntaxRole::comment),
           "the composed line has no spans and the others keep theirs");

    Editing grid;
    static_cast<void>(grid.controller().apply(app::VisibleLines{shown_lines, grid_geometry}));
    open(grid, code_path, std::string(sample));
    const auto gridded = grid.controller().apply_frame(app::SelectDisplay{core::DisplayForm::grid});
    expect(gridded.lines.empty() && !gridded.grid.empty() && gridded.syntax_lexed == 0,
           "the grid frame lexes nothing");
}

// 決定 10: 役割から色への写しは core の syntax_color の 1 本で、各役割は同じ名前の欄。
void verify_syntax_color()
{
    const auto shade = [](std::uint8_t value) { return core::RgbColor{value, value, value}; };
    const core::SyntaxPalette palette{
        shade(1), shade(2),  shade(3),  shade(4),  shade(5),  shade(6),  shade(7),  shade(8),
        shade(9), shade(10), shade(11), shade(12), shade(13), shade(14), shade(15), shade(16)};
    constexpr std::array<std::pair<SyntaxRole, std::uint8_t>, 10> rows{{
        {SyntaxRole::comment, 7},
        {SyntaxRole::keyword, 8},
        {SyntaxRole::string, 9},
        {SyntaxRole::number, 10},
        {SyntaxRole::type, 11},
        {SyntaxRole::function, 12},
        {SyntaxRole::constant, 13},
        {SyntaxRole::operators, 14},
        {SyntaxRole::error, 15},
        {SyntaxRole::warning, 16},
    }};
    for (const auto &[role, value] : rows)
    {
        expect(core::syntax_color(palette, role) == shade(value),
               "each role takes the body token of the same name");
    }
    Editing editing;
    const auto frame = editing.controller().frame();
    const auto theme = core::selected_theme(frame.settings, frame.appearance);
    expect(frame.body.comment == theme.body.comment && frame.body.keyword == theme.body.keyword &&
               frame.body.warning == theme.body.warning,
           "the frame carries the theme's body palette");
}
} // namespace

void verify_syntax_scope()
{
    verify_colored_lines();
    verify_plain_untouched();
    verify_backfill_reach();
    verify_cache_follows();
    verify_tabs_and_panes();
    verify_language_switch();
    verify_opened_into_untitled();
    verify_uncolored_rows();
    verify_syntax_color();
}
} // namespace nenenib::tests
