// scope `--syntax` の単体テスト（ADR 0042 決定 2・#816・ADR 0156 の決定 7〜10・14）。字句器を
// 持つ言語の文書で見えている行に色の区間が付くこと、持たない言語では区間が空で字句器も覚えも
// 触らないこと、見えている最初の行の 200 行前からの遡りと覚えから続ける道、改訂・タブ・枠で
// 作り直すこと、巨大な行の窓が色を持たないこと、役割から色への写しが core の 1 本であること。
// 格子の画面行と IME の差し込み行も同じ遡りと覚えで色を持ち、差し込みの範囲は色なしで後ろの区間が
// ずれること（#839・ADR 0156 の追補）。`:syntax on|off|?` が文書の値で、off
// の文書は字句器を呼ばないこと （#829・ADR 0156 の追補）。
#include "Appearance.hpp"
#include "ColorSpan.hpp"
#include "ComposeText.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "GiantLine.hpp"
#include "GridColorSpan.hpp"
#include "GridRowView.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "Language.hpp"
#include "Lexer.hpp"
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
#include "SelectSyntax.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchPane.hpp"
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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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
constexpr std::string_view json_path = "C:\\work\\a.json";
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

[[nodiscard]] bool has_cells(const app::GridRowView &row, std::size_t begin, std::size_t end,
                             SyntaxRole role)
{
    const app::GridColorSpan wanted{app::GridCellSpan{begin, end}, role};
    return std::ranges::find(row.colors, wanted) != row.colors.end();
}

// 格子の frame の行 line の画面行 row（無ければ nullptr）。
[[nodiscard]] const app::GridRowView *grid_row(const app::EditorFrame &frame, std::size_t line,
                                               std::size_t row)
{
    const auto found =
        std::ranges::find_if(frame.grid, [line, row](const app::GridRowView &shown)
                             { return shown.number.value == line && shown.row == row; });
    return found == frame.grid.end() ? nullptr : &*found;
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

// 決定 9: スクロールで覚えから続け、上へ戻ると作り直す。見えている行の中の編集は覚えを保つ（追補
// #827）。
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
    static_cast<void>(controller.apply(app::InsertText{"*/"}));
    const auto edited = controller.frame();
    expect(edited.first_visible.value == 305 && edited.syntax_lexed == shown_lines,
           "an edit inside the visible lines keeps the cache and lexes only the visible lines");
    expect(all_comment(edited.lines.front()) &&
               has_span(edited.lines.at(1), 1, 3, SyntaxRole::comment) &&
               edited.lines.at(2).colors.empty(),
           "the visible lines are lexed again from the kept state and the comment closes");
    const auto undone = controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(undone.first_visible.value == 305 && undone.syntax_lexed == shown_lines &&
               all_comment(undone.lines.at(2)),
           "undoing an edit inside the visible lines keeps the cache");
}

// 追補 #827: 見えている最初の行より前の編集（先頭へ行って打つ・その undo）は覚えを捨てる。
void verify_edit_above_forgets()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    static_cast<void>(controller.frame());
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(app::InsertText{"x"}));
    scrolled(controller, 299);
    const auto typed = controller.frame();
    expect(typed.first_visible.value == 300 && typed.syntax_lexed == 200 + shown_lines &&
               all_comment(typed.lines.front()),
           "typing above the cached line forgets the cache and backfills");
    static_cast<void>(controller.apply(app::HistoryAction{core::HistoryDirection::undo}));
    scrolled(controller, 299);
    const auto undone = controller.frame();
    expect(undone.first_visible.value == 300 && undone.syntax_lexed == 200 + shown_lines,
           "undoing an edit above the cached line forgets the cache too");
}

// 追補 #827 の境界: 覚えの行 L の行頭ちょうどで改行を含めて挿入する編集は保ち、L − 1 の行末の改行を
// 消して結合する削除（編集の始まりは L − 1）は捨てる。
void verify_edit_boundary()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 304);
    static_cast<void>(controller.frame());
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{305}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(app::InsertText{"*/\n"}));
    const auto inserted = controller.frame();
    expect(inserted.first_visible.value == 305 && inserted.syntax_lexed == shown_lines &&
               has_span(inserted.lines.front(), 1, 3, SyntaxRole::comment) &&
               inserted.lines.at(1).colors.empty(),
           "an edit at the very start of the cached line keeps the cache and closes the comment");
    Editing joined_editing;
    auto &joining = joined_editing.controller();
    static_cast<void>(joining.apply(app::VisibleLines{shown_lines}));
    open(joined_editing, code_path, nearby_comment());
    scrolled(joining, 304);
    static_cast<void>(joining.frame());
    static_cast<void>(
        joining.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{305}, core::Column{1}},
                                      core::SelectionAnchoring::collapse}));
    static_cast<void>(joining.apply(app::DeleteText{core::DeleteDirection::backward}));
    scrolled(joining, 1);
    const auto joined = joining.frame();
    expect(joined.first_visible.value == 305 && joined.syntax_lexed == 200 + shown_lines,
           "deleting the line break at the end of the line above the cached line forgets it");
}

// 追補 #827: 2 枠で同じ文書を見ているとき、覚えの行より前の編集は両方の覚えを捨て（もう 1 つの
// 枠も 200 行前のコメントの閉じを見る）、覚えの行より後ろの編集は両方とも保つ。
void verify_split_forgets_both()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    static_cast<void>(controller.apply(app::SplitPane{core::SplitLayout::vertical}));
    static_cast<void>(controller.frame());
    static_cast<void>(controller.apply(app::SwitchPane{}));
    scrolled(controller, 299);
    static_cast<void>(controller.frame());
    const auto both = controller.frame();
    const auto *kept = pane_of(both);
    expect(both.syntax_lexed == shown_lines && kept != nullptr && !kept->lines.empty() &&
               all_comment(kept->lines.front()) && all_comment(both.lines.front()),
           "both panes on the same document keep their caches inside the comment");
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{200}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(app::InsertText{"*/"}));
    scrolled(controller, 100);
    const auto closed = controller.frame();
    const auto *other = pane_of(closed);
    expect(closed.first_visible.value == 300 && closed.syntax_lexed == 200 + shown_lines &&
               closed.lines.front().colors.empty(),
           "the edited pane forgets its cache and sees the comment closed above");
    expect(other != nullptr && !other->lines.empty() && other->lines.front().colors.empty(),
           "the other pane on the same document forgets its cache and sees the comment closed");
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{305}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(app::InsertText{"x"}));
    expect(controller.apply_frame(app::SwitchPane{}).syntax_lexed == shown_lines,
           "an edit below the cached line keeps the other pane's cache");
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{305}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(app::InsertText{"x"}));
    expect(controller.frame().syntax_lexed == shown_lines,
           "an edit below the cached line keeps the active pane's cache");
}

// 追補 #827: 遡りの行数は言語ごとの表 1 つ。JSON は 1 行、C / C++ は 200 行、YAML は 100 行
// （#838）。
void verify_backfill_per_language()
{
    using core::Language;
    expect(core::syntax_backfill_lines(Language::plain) == 0 &&
               core::syntax_backfill_lines(Language::json) == 1 &&
               core::syntax_backfill_lines(Language::c) == 200 &&
               core::syntax_backfill_lines(Language::cpp) == 200 &&
               core::syntax_backfill_lines(Language::python) == 200 &&
               core::syntax_backfill_lines(Language::javascript) == 200 &&
               core::syntax_backfill_lines(Language::typescript) == 200 &&
               core::syntax_backfill_lines(Language::markdown) == 500 &&
               core::syntax_backfill_lines(Language::html) == 200 &&
               core::syntax_backfill_lines(Language::xml) == 200 &&
               core::syntax_backfill_lines(Language::css) == 200 &&
               core::syntax_backfill_lines(Language::toml) == 200 &&
               core::syntax_backfill_lines(Language::yaml) == 100 &&
               core::syntax_backfill_lines(Language::gdscript) == 200 &&
               core::syntax_backfill_lines(Language::gdresource) == 200 &&
               core::syntax_backfill_lines(Language::dosini) == 1 &&
               core::syntax_backfill_lines(Language::cs) == 200 &&
               core::syntax_backfill_lines(Language::rust) == 200 &&
               core::syntax_backfill_lines(Language::go) == 200 &&
               core::syntax_backfill_lines(Language::php) == 200 &&
               core::syntax_backfill_lines(Language::sql) == 200 &&
               core::syntax_backfill_lines(Language::lua) == 200 &&
               core::syntax_backfill_lines(Language::dockerfile) == 50 &&
               core::syntax_backfill_lines(Language::powershell) == 200 &&
               core::syntax_backfill_lines(Language::shell) == 200 &&
               core::syntax_backfill_lines(Language::cmake) == 200,
           "the backfill table has one value per language");
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    open(editing, json_path, repeated("1,\n", 600));
    scrolled(controller, 299);
    const auto json = controller.frame();
    expect(json.first_visible.value == 300 && json.syntax_lexed == 1 + shown_lines,
           "a JSON document backfills one line");
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

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string();
}

[[nodiscard]] std::string asked_syntax(app::EditorController &controller)
{
    return message_of(run_ex(controller, "syntax?"));
}

// 追補: Ex の評価。`:sy[ntax] on|off|?` と引数なしは設定に載らない要求で、ほかの引数は E474、
// 範囲の形は Not supported。補完の候補は `syntax on` と `syntax off`。
void verify_syntax_evaluation()
{
    const auto settings = core::default_editor_settings();
    const auto system = core::Appearance::light;
    for (const auto &[text, enabled] :
         std::array<std::pair<std::string_view, std::optional<bool>>, 6>{{{"syntax on", true},
                                                                          {"syntax off", false},
                                                                          {"sy off", false},
                                                                          {"syntax?", std::nullopt},
                                                                          {"syntax", std::nullopt},
                                                                          {"syn ?", std::nullopt}}})
    {
        const auto result = core::evaluate_ex(text, settings, system);
        const auto request = result.has_value() ? result.value().syntax : std::nullopt;
        expect(request.has_value() && request.value().enabled == enabled &&
                   !result.value().settings.has_value(),
               "syntax on|off|? is a document request outside the settings");
    }
    for (const std::string_view text : {"syntax bogus", "syntax enable", "syntax on off"})
    {
        const auto result = core::evaluate_ex(text, settings, system);
        expect(!result.has_value() &&
                   result.error() == core::ExEvaluationFailure{core::ExFailure::invalid_argument},
               "an unknown syntax argument is E474");
    }
    const auto ranged = core::evaluate_ex("2syntax on", settings, system);
    expect(!ranged.has_value() &&
               ranged.error() == core::ExEvaluationFailure{core::ExFailure::unsupported_argument},
           "a range before syntax is not supported");
    const auto completions = core::command_completions("synt");
    expect(completions == std::vector<std::string>{"syntax on", "syntax off"},
           "syntax completes to syntax on and syntax off");
}

// 追補: `.cpp` の色が `:syntax off` で消えて字句器を 1 行も通さず、`:syntax on` で 200 行の遡りから
// 戻る。`:syntax?` は今の値の 1 行。未知の引数は E474 で値を変えない。
void verify_syntax_toggle()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    open(editing, code_path, nearby_comment());
    scrolled(controller, 299);
    expect(all_comment(controller.frame().lines.front()), "the cpp document is colored");
    expect(asked_syntax(controller) == "syntax=on", "a document starts with syntax on");
    const auto off = run_ex(controller, "syntax off");
    expect(message_of(off) == "syntax=off" && no_colors(off) && off.syntax_lexed == 0,
           ":syntax off drops the colors and lexes nothing");
    expect(controller.frame().syntax_lexed == 0, "later frames of an off document lex nothing");
    expect(asked_syntax(controller) == "syntax=off" &&
               message_of(run_ex(controller, "syntax")) == "syntax=off",
           ":syntax? and a bare :syntax tell the value");
    expect(message_of(run_ex(controller, "syntax bogus")) == "E474: Invalid argument" &&
               asked_syntax(controller) == "syntax=off",
           "E474 leaves the value as it was");
    const auto on = run_ex(controller, "syntax on");
    expect(message_of(on) == "syntax=on" && on.syntax_lexed == 200 + shown_lines &&
               all_comment(on.lines.front()),
           ":syntax on backfills again and the colors come back");
    expect(controller.frame().syntax_lexed == shown_lines, "the rebuilt cache is kept");
    static_cast<void>(controller.apply(app::SelectSyntax{true}));
    expect(controller.frame().syntax_lexed == shown_lines,
           "selecting the same value keeps the cache");
}

// 追補: 値は文書ごとで、タブの切り替えに付いていく。新しいタブと開いた文書は on。
void verify_syntax_per_document()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    open(editing, code_path, std::string(sample));
    static_cast<void>(run_ex(controller, "syntax off"));
    open(editing, other_code_path, std::string(sample));
    const auto other = controller.frame();
    expect(asked_syntax(controller) == "syntax=on" && !no_colors(other),
           "an opened document is on even when another tab is off");
    const auto first = controller.apply_frame(app::SwitchTab{0});
    expect(no_colors(first) && first.syntax_lexed == 0 && asked_syntax(controller) == "syntax=off",
           "the off value follows its tab");
    const auto second = controller.apply_frame(app::SwitchTab{1});
    expect(!no_colors(second) && asked_syntax(controller) == "syntax=on",
           "the on value follows its tab");
    static_cast<void>(controller.apply(app::NewTab{}));
    expect(asked_syntax(controller) == "syntax=on", "a new tab is on");
    static_cast<void>(controller.apply(app::SwitchTab{0}));
    open(editing, text_path, std::string(sample));
    static_cast<void>(controller.apply(app::SwitchTab{0}));
    expect(asked_syntax(controller) == "syntax=off",
           "opening a document in another tab leaves the off tab alone");
}

// 決定 7: 巨大な行の窓は色を持たない。
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
}

// 本文の 1 行目「int f(void) {」の 3 桁目（int の中）にキャレットを置いて utf8 を変換中にする。
[[nodiscard]] app::EditorFrame composed_in_int(app::EditorController &controller, std::string utf8)
{
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{3}},
                                         core::SelectionAnchoring::collapse}));
    const auto cursor = utf8.size();
    return controller.apply_frame(app::ComposeText{composed_of(std::move(utf8), {}, cursor)});
}

// #839: IME の差し込み行は、差し込みの手前の区間はそのまま、後ろは差し込んだ code point の数（3）
// だけずれ、差し込みをまたぐ int は 2 つに割れて差し込みの範囲は色なし。
void verify_composed_colors()
{
    Editing composing;
    static_cast<void>(composing.controller().apply(app::VisibleLines{shown_lines}));
    open(composing, code_path, std::string(sample));
    const auto composed = composed_in_int(composing.controller(), "にほん");
    const auto &line = composed.lines.at(0);
    const bool clear = std::ranges::none_of(line.colors, [](const core::ColorSpan &span)
                                            { return span.begin.value < 6 && span.end.value > 3; });
    expect(line.composed.has_value() && clear && has_span(line, 1, 3, SyntaxRole::type) &&
               has_span(line, 6, 7, SyntaxRole::type) &&
               has_span(line, 8, 9, SyntaxRole::function) &&
               has_span(line, 10, 14, SyntaxRole::type),
           "the composed line shifts the spans after the insertion and leaves it uncolored");
    expect(has_span(composed.lines.at(1), 5, 9, SyntaxRole::comment),
           "the other lines keep their spans");
}

// #839（レビュー I-2）: サロゲート対 1 つ（U+20B9F・UTF-16 で 2 単位）を変換中にしても、区間は
// code point の数（1）だけずれ、差し込みの範囲（4 桁目）は色なし。
void verify_composed_surrogate_colors()
{
    Editing composing;
    static_cast<void>(composing.controller().apply(app::VisibleLines{shown_lines}));
    open(composing, code_path, std::string(sample));
    const auto composed = composed_in_int(composing.controller(), "\xF0\xA0\xAE\x9F");
    const auto &line = composed.lines.at(0);
    const bool clear = std::ranges::none_of(line.colors, [](const core::ColorSpan &span)
                                            { return span.begin.value < 4 && span.end.value > 3; });
    expect(line.composed.has_value() && line.composed.value().inserted_columns == 1 && clear &&
               has_span(line, 1, 3, SyntaxRole::type) && has_span(line, 4, 5, SyntaxRole::type) &&
               has_span(line, 6, 7, SyntaxRole::function) &&
               has_span(line, 8, 12, SyntaxRole::type),
           "a composed surrogate pair shifts the later spans by one code point");
}

// #839（レビュー I-1）: Tab（8 升）と全角を含む行を幅 8 升の格子で折り返す。「\tx "日本語";」は
// 1 行目が Tab、2 行目が「x "日本」（語は升 7 に収まらず次の画面行へ送る）、3 行目が「語";」。
// 文字列の区間は 2 行目の升 [2, 7) と 3 行目の升 [0, 3) に切れ、送りの空き（2 行目の升
// 7）は含まない。
void verify_grid_wide_tab_colors()
{
    Editing grid;
    auto &controller = grid.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines, grid_geometry}));
    open(grid, code_path, "\tx \"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\";\n");
    const auto gridded = controller.apply_frame(app::SelectDisplay{core::DisplayForm::grid});
    const auto *tab = grid_row(gridded, 1, 0);
    const auto *second = grid_row(gridded, 1, 1);
    const auto *third = grid_row(gridded, 1, 2);
    expect(tab != nullptr && tab->colors.empty() && second != nullptr && third != nullptr,
           "the Tab takes the first screen row and the line folds twice");
    expect(second != nullptr && has_cells(*second, 2, 7, SyntaxRole::string),
           "the string's second-row cells start after x and stop before the pushed gap");
    expect(third != nullptr && has_cells(*third, 0, 3, SyntaxRole::string),
           "the pushed wide character and the closing quote color the third row from cell 0");
}

// #839（レビュー I-1）: 格子でも巨大な行は色を持たず、手前の行は色を持つ。
void verify_grid_giant_uncolored()
{
    Editing grid;
    auto &controller = grid.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines, grid_geometry}));
    open(grid, code_path, "int a;\n" + repeated("x", core::giant_line_threshold + 1) + "\nint b;");
    const auto gridded = controller.apply_frame(app::SelectDisplay{core::DisplayForm::grid});
    const auto *first = grid_row(gridded, 1, 0);
    const bool giant_rows = std::ranges::any_of(gridded.grid, [](const app::GridRowView &row)
                                                { return row.number.value == 2; });
    const bool giant_plain =
        std::ranges::all_of(gridded.grid, [](const app::GridRowView &row)
                            { return row.number.value != 2 || row.colors.empty(); });
    expect(first != nullptr && has_cells(*first, 0, 3, SyntaxRole::type) && giant_rows &&
               giant_plain,
           "the giant line's grid rows have no spans while the line above keeps its spans");
}

// #839: 格子の文書（幅 8 升）も text と同じ字句器を通り、画面行ごとに升の区間が付く。折り返した行の
// 2 行目の区間は 2 行目の升で、変換中の文字列（6 升）を差し込んだ升は色なしで後ろがずれる。
void verify_grid_colors()
{
    Editing grid;
    auto &controller = grid.controller();
    static_cast<void>(controller.apply(app::VisibleLines{shown_lines, grid_geometry}));
    open(grid, code_path, std::string(sample));
    const auto gridded = controller.apply_frame(app::SelectDisplay{core::DisplayForm::grid});
    const auto *first = grid_row(gridded, 1, 0);
    const auto *folded = grid_row(gridded, 1, 1);
    const auto *returned = grid_row(gridded, 3, 1);
    expect(gridded.lines.empty() && first != nullptr && folded != nullptr && returned != nullptr,
           "the grid frame shows the folded rows of the sample");
    expect(first != nullptr && has_cells(*first, 0, 3, SyntaxRole::type) &&
               has_cells(*first, 4, 5, SyntaxRole::function) &&
               has_cells(*first, 6, 8, SyntaxRole::type),
           "the first screen row of int f(vo takes the cells of its spans");
    expect(folded != nullptr && has_cells(*folded, 0, 2, SyntaxRole::type),
           "the folded row continues void from its own first cell");
    expect(returned != nullptr && has_cells(*returned, 0, 2, SyntaxRole::keyword) &&
               has_cells(*returned, 3, 6, SyntaxRole::string),
           "the second row of the return line has the rest of return and the string");
    expect(gridded.syntax_lexed == 5, "the grid frame lexes the five lines once each");

    const auto composed = composed_in_int(controller, "にほん");
    const auto *inserted = grid_row(composed, 1, 0);
    const bool clear = inserted != nullptr &&
                       std::ranges::none_of(inserted->colors, [](const app::GridColorSpan &span)
                                            { return span.cells.begin < 8 && span.cells.end > 2; });
    expect(inserted != nullptr && inserted->composed_at.has_value() && clear &&
               has_cells(*inserted, 0, 2, SyntaxRole::type) &&
               has_cells(*inserted, 8, 9, SyntaxRole::type) &&
               has_cells(*inserted, 10, 11, SyntaxRole::function) &&
               has_cells(*inserted, 12, 14, SyntaxRole::type),
           "the grid row with the composition splits int and shifts the later cells by six");
}

// 決定 21（#861）: 覚えの無い PHP の文書は、遡りが 1 行目から始まれば `?>` の外（HTML・区間
// なし）から、途中から始まれば PHP の中から読む。`echo` は中でだけ keyword。
void verify_php_start_state()
{
    for (const auto &[scroll, inside] :
         std::array<std::pair<std::int32_t, bool>, 3>{{{0, false}, {100, false}, {250, true}}})
    {
        Editing editing;
        auto &controller = editing.controller();
        static_cast<void>(controller.apply(app::VisibleLines{shown_lines}));
        open(editing, "C:\\work\\a.php", repeated("echo 1;\n", 300));
        scrolled(controller, scroll);
        const auto frame = controller.frame();
        const auto &first = frame.lines.front();
        expect(frame.first_visible.value == static_cast<std::size_t>(scroll) + 1,
               "the PHP document shows the scrolled line first");
        expect(inside ? has_span(first, 1, 5, SyntaxRole::keyword) : first.colors.empty(),
               inside ? "a backfill starting in the middle reads the lines inside PHP"
                      : "a backfill starting at the first line reads the lines outside PHP");
    }
    expect(core::lex_start_state(core::Language::php, true) == core::LexState{} &&
               core::lex_start_state(core::Language::php, false) != core::LexState{} &&
               core::lex_start_state(core::Language::cpp, false) == core::LexState{},
           "only PHP starts inside when the backfill begins in the middle");
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
    verify_edit_above_forgets();
    verify_edit_boundary();
    verify_split_forgets_both();
    verify_backfill_per_language();
    verify_php_start_state();
    verify_tabs_and_panes();
    verify_language_switch();
    verify_opened_into_untitled();
    verify_uncolored_rows();
    verify_composed_colors();
    verify_grid_colors();
    verify_composed_surrogate_colors();
    verify_grid_wide_tab_colors();
    verify_grid_giant_uncolored();
    verify_syntax_color();
    verify_syntax_evaluation();
    verify_syntax_toggle();
    verify_syntax_per_document();
}
} // namespace nenenib::tests
