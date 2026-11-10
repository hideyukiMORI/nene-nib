// scope `--vim-characters` の契約のうち、engine を通らない意図の後の Vim のキャレット（#1309・
// ADR 0053 の決定 5・6 と追補 #1309）。Vim モードでも、Delete / Backspace の
// DeleteText・InsertText・ NewLine・貼り付けと切り取り・Ctrl+Z / Ctrl+Y の HistoryAction・SelectAll
// は engine の鍵を通らずに 本文とキャレットを動かす。どの意図の後もキャレットは Vim の 1
// 文字の境（結合文字の手前でない）に あり、NORMAL
// では行の最後の文字を越えない。手書きの表（意図ごとの 1 行）と、固定 seed の小さな 性質（Vim
// の鍵と engine を通らない意図を混ぜた列）で守る。
#include "CaretMotion.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "CommitText.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "Offset.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectAll.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimTestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

using Random = std::mt19937_64;
// 1 歩の動作: engine を通らない意図か、VimKeyPress で送る Vim の鍵の記法。
using Action = std::variant<app::EditorIntent, std::string>;
using Step = std::pair<std::string, Action>;
using Steps = std::vector<Step>;
using Reason = std::string_view;

// Debug で 1 秒前後（#1309）。50 倍の走り（2,400 組）は手元で回し、commit しない。
constexpr std::size_t property_cases = 48U;
constexpr std::size_t step_floor = 10U;
constexpr std::size_t step_spread = 21U;

// 結合文字つきの 1 文字（e + 鋭アクセント・か + 結合濁点・\u2764 +
// VS16）と行頭に置ける孤立した結合文字。
constexpr std::array<std::string_view, 9> body_tokens{
    "ab", "Hello", " ", "\t", "日本", "e\u0301", "か\u3099", "\u2764\uFE0F", "\u0301"};
constexpr std::array<std::string_view, 5> typed_texts{"x", "\u0301", "e\u0301", "あ", "\t"};
constexpr std::array<std::string_view, 4> clipboard_texts{"\u0301", "e\u0301", "x\n", "か\u3099"};
// Vim の鍵の語。移動・モードの出入り・NORMAL の編集を混ぜる。
constexpr std::array<std::string_view, 24> vim_words{
    "h", "l", "$", "0",     "j",     "k", "x", "X",     "i", "a",  "A", "I",
    "o", "v", "V", "<C-v>", "<Esc>", "p", "u", "<C-r>", "w", "rz", "J", "e"};
constexpr std::array<core::CaretMotion, 4> motions{
    core::CaretMotion::previous_character, core::CaretMotion::next_character,
    core::CaretMotion::line_start, core::CaretMotion::line_end};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

// 本文の line 行目（1 始まり）の内容。frame の行は改行を含まない。
[[nodiscard]] std::string_view line_of(const std::string &body, std::size_t line)
{
    std::size_t start = 0;
    for (std::size_t index = 1; index < line; ++index)
    {
        start = body.find('\n', start) + 1;
    }
    const std::size_t stop = std::min(body.find('\n', start), body.size());
    return std::string_view(body).substr(start, stop - start);
}

// 今の frame のキャレットが Vim の 1 文字の境にあり、NORMAL で行の最後の文字を越えないか。
[[nodiscard]] Reason caret_breaks(EditorController &controller)
{
    const auto frame = controller.frame();
    const std::string text = vim_body(frame);
    const std::string_view line = line_of(text, frame.caret.position.line.value);
    const std::size_t byte = vim_byte_column(text, frame.caret.position) - 1U;
    if (core::vim_character_continues(line, core::Offset{byte}))
    {
        return "the caret is inside a character";
    }
    if (controller.vim_state().mode == core::VimMode::normal && !line.empty() &&
        byte >= line.size())
    {
        return "the NORMAL caret is past the last character";
    }
    return Reason{};
}

void act(EditorController &controller, const Action &action)
{
    const auto *keys = std::get_if<std::string>(&action);
    if (keys != nullptr)
    {
        // 窓と同じに鍵を 1 つずつ入口の意図で送る（press_vim_key は入口を通らない）。
        for (const auto &key : vim_keys_of(*keys))
        {
            static_cast<void>(applied(controller, app::VimKeyPress{key}));
        }
        return;
    }
    static_cast<void>(applied(controller, std::get<app::EditorIntent>(action)));
}

// 本文を Vim モードで開き、歩ごとにキャレットを見る。最初の破れを返す。
[[nodiscard]] Reason run(std::string body, std::string clipboard, const Steps &steps)
{
    Editing editing;
    editing.clipboard().hold(std::move(clipboard));
    open_vim_document(editing, std::move(body));
    EditorController &controller = editing.controller();
    for (const auto &step : steps)
    {
        act(controller, step.second);
        const Reason reason = caret_breaks(controller);
        if (!reason.empty())
        {
            return reason;
        }
    }
    return Reason{};
}

[[nodiscard]] Step keys(std::string_view typed)
{
    return Step{std::string(typed), Action{std::string(typed)}};
}

[[nodiscard]] Step intent(std::string label, app::EditorIntent value)
{
    return Step{std::move(label), Action{std::move(value)}};
}

[[nodiscard]] Step forward()
{
    return intent("delete", app::DeleteText{core::DeleteDirection::forward});
}

[[nodiscard]] Step backward()
{
    return intent("backspace", app::DeleteText{core::DeleteDirection::backward});
}

[[nodiscard]] Step history(core::HistoryDirection direction)
{
    return intent(direction == core::HistoryDirection::undo ? "undo" : "redo",
                  app::HistoryAction{direction});
}

[[nodiscard]] Step clipboard(app::ClipboardOperation operation, const char *label)
{
    return intent(label, app::ClipboardAction{operation});
}

// 意図の表（#1309 の 1）。各行は Vim の鍵で位置とモードを作ってから engine を通らない意図を送る。
// 本文 "Helloé"（é は e + U+0301）の `$` は基底 e の上。行頭の孤立した結合文字は 1 文字。
// 名前・本文・位置とモードを作る鍵・送る意図。
using KnownRow = std::tuple<const char *, std::string_view, std::string_view, Steps>;

// 本文の 3 つ: 結合文字の行（LF と CRLF）と、行頭の孤立した結合文字。
constexpr std::string_view marked_body = "Helloe\u0301\n # h";
constexpr std::string_view crlf_body = "Helloe\u0301\r\n # h";
constexpr std::string_view lone_body = "\u0301a\n";

// Delete・Backspace・INSERT の左右。
[[nodiscard]] std::vector<KnownRow> deletion_rows()
{
    return {
        {"NORMAL $ then Delete (the #1309 case)", crlf_body, "$", {forward()}},
        {"NORMAL $ then Delete on an LF document", marked_body, "$", {forward()}},
        {"NORMAL $ then Backspace", marked_body, "$", {backward()}},
        {"NORMAL 0 then Delete", marked_body, "0", {forward()}},
        {"INSERT before the marked character then Delete", marked_body, "$i", {forward()}},
        {"INSERT after the marked character then Backspace", marked_body, "$a", {backward()}},
        {"VISUAL on the marked character then Delete", marked_body, "$v", {forward()}},
        {"INSERT after the marked character then a code point to the left",
         marked_body,
         "$a",
         {intent("left", app::MoveCaret{core::CaretMotion::previous_character,
                                        core::SelectionAnchoring::collapse})}},
    };
}

// InsertText・NewLine・Ctrl+Z / Ctrl+Y。
[[nodiscard]] std::vector<KnownRow> insertion_rows()
{
    return {
        {"NORMAL $ then InsertText", marked_body, "$", {intent("insert x", app::InsertText{"x"})}},
        {"NORMAL before a lone mark then InsertText",
         lone_body,
         "",
         {intent("insert x", app::InsertText{"x"})}},
        {"INSERT before a lone mark then InsertText",
         lone_body,
         "i",
         {intent("insert x", app::InsertText{"x"})}},
        {"NORMAL $ then NewLine", marked_body, "$", {intent("newline", app::NewLine{})}},
        {"NORMAL $x then Ctrl+Z", marked_body, "$x", {history(core::HistoryDirection::undo)}},
        {"NORMAL $xu then Ctrl+Y", marked_body, "$xu", {history(core::HistoryDirection::redo)}},
        {"NORMAL $ Delete then Ctrl+Z",
         marked_body,
         "$",
         {forward(), history(core::HistoryDirection::undo)}},
    };
}

// SelectAll・貼り付けと切り取り・IME・クリック。
[[nodiscard]] std::vector<KnownRow> selection_rows()
{
    return {
        {"NORMAL SelectAll", marked_body, "", {intent("select_all", app::SelectAll{})}},
        {"INSERT SelectAll", marked_body, "i", {intent("select_all", app::SelectAll{})}},
        {"INSERT before the marked character then paste",
         marked_body,
         "$i",
         {clipboard(app::ClipboardOperation::paste, "paste")}},
        {"INSERT before a lone mark then paste",
         lone_body,
         "i",
         {clipboard(app::ClipboardOperation::paste, "paste")}},
        {"VISUAL on the marked character then cut",
         marked_body,
         "$v",
         {clipboard(app::ClipboardOperation::cut, "cut")}},
        {"VISUAL before the marked character then cut",
         marked_body,
         "$hv",
         {clipboard(app::ClipboardOperation::cut, "cut")}},
        {"NORMAL IME commit is ignored",
         marked_body,
         "$",
         {intent("commit", app::CommitText{"x"})}},
        {"NORMAL click on the mark",
         marked_body,
         "",
         {intent("click", app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{7}},
                                          core::SelectionAnchoring::collapse})}},
    };
}

void verify_known_rows()
{
    std::vector<KnownRow> rows = deletion_rows();
    std::ranges::copy(insertion_rows(), std::back_inserter(rows));
    std::ranges::copy(selection_rows(), std::back_inserter(rows));
    expect(rows.size() == 23U, "the table holds 23 intents outside the engine (#1309)");
    for (const auto &[name, body, setup, sent] : rows)
    {
        Steps steps{keys(setup)};
        std::ranges::copy(sent, std::back_inserter(steps));
        const Reason reason = run(std::string(body), std::string("\u0301"), steps);
        if (!reason.empty())
        {
            std::fprintf(stderr, "  %s: %.*s\n", name, static_cast<int>(reason.size()),
                         reason.data());
        }
        expect(reason.empty(), "an intent outside the engine keeps the Vim caret on a character "
                               "boundary and NORMAL on the last character (#1309)");
    }
}

// ---------------------------------------------------------------- 性質（固定 seed）

[[nodiscard]] std::string random_body(Random &random)
{
    const std::size_t lines = 1U + below(random, 4U);
    std::string body;
    for (std::size_t line = 0; line < lines; ++line)
    {
        if (line > 0U)
        {
            body += "\n";
        }
        const std::size_t tokens = below(random, 5U);
        for (std::size_t token = 0; token < tokens; ++token)
        {
            body += body_tokens.at(below(random, body_tokens.size()));
        }
    }
    return body;
}

[[nodiscard]] Step random_intent(Random &random)
{
    const std::size_t kind = below(random, 11U);
    const std::string_view text = typed_texts.at(below(random, typed_texts.size()));
    const std::size_t motion = below(random, motions.size());
    const std::size_t line = 1U + below(random, 4U);
    const std::size_t column = 1U + below(random, 8U);
    const std::array<Step, 11> made{
        forward(),
        backward(),
        intent("insert " + std::string(text), app::InsertText{std::string(text)}),
        intent("newline", app::NewLine{}),
        clipboard(app::ClipboardOperation::paste, "paste"),
        clipboard(app::ClipboardOperation::cut, "cut"),
        history(core::HistoryDirection::undo),
        history(core::HistoryDirection::redo),
        intent("select_all", app::SelectAll{}),
        intent("move " + std::to_string(motion),
               app::MoveCaret{motions.at(motion), core::SelectionAnchoring::collapse}),
        intent("click " + std::to_string(line) + ":" + std::to_string(column),
               app::PlaceCaret{core::TextPosition{core::LineNumber{line}, core::Column{column}},
                               core::SelectionAnchoring::collapse}),
    };
    return made.at(kind);
}

[[nodiscard]] Steps random_steps(Random &random)
{
    const std::size_t count = step_floor + below(random, step_spread);
    Steps steps;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (below(random, 2U) == 0U)
        {
            steps.push_back(keys(vim_words.at(below(random, vim_words.size()))));
        }
        else
        {
            steps.push_back(random_intent(random));
        }
    }
    return steps;
}

// 破れた組を縮める: 歩を 1 つずつ外し、破れが残るなら外したままにする。
void report(const std::string &body, const std::string &held, Steps steps)
{
    for (std::size_t index = steps.size(); index-- > 0;)
    {
        Steps smaller = steps;
        smaller.erase(std::next(smaller.begin(), static_cast<std::ptrdiff_t>(index)));
        if (!run(body, held, smaller).empty())
        {
            steps = std::move(smaller);
        }
    }
    const Reason reason = run(body, held, steps);
    std::fprintf(stderr, "  vim external caret: %.*s\n    body: \"%s\"\n    steps:",
                 static_cast<int>(reason.size()), reason.data(), body.c_str());
    for (const auto &step : steps)
    {
        std::fprintf(stderr, " [%s]", step.first.c_str());
    }
    std::fprintf(stderr, "\n");
}

void verify_property()
{
    Random random{13090U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < property_cases; ++index)
    {
        const std::string body = random_body(random);
        const std::string held(clipboard_texts.at(below(random, clipboard_texts.size())));
        const Steps steps = random_steps(random);
        if (!run(body, held, steps).empty() && broken++ == 0U)
        {
            report(body, held, steps);
        }
    }
    expect(broken == 0U, "every Vim key and every intent outside the engine leaves the caret on a "
                         "character boundary (#1309 property)");
}
} // namespace

void verify_vim_external_caret()
{
    verify_known_rows();
    verify_property();
}
} // namespace nenenib::tests
