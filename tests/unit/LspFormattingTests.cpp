// scope `--lsp-formatting` の契約（#1993・ADR 0191 の決定 7）。言語サーバーの整形: 操作「文書を
// 整形」「選択範囲を整形」（鍵なし）。core の読みと 1 区間の組み立て、controller の道（要求の形・
// 1 つの undo の単位・頼んだ後に変わった文書・能力の無いサーバー）を替え玉 ScriptedLanguageServer
// で確かめる。
#include "CaretMotion.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FormattedSpan.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "LanguageQueryKind.hpp"
#include "LineNumber.hpp"
#include "LspAnswerFailure.hpp"
#include "LspFormatting.hpp"
#include "LspPositionEncoding.hpp"
#include "LspTextEdit.hpp"
#include "MoveCaret.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationTexts.hpp"
#include "PlaceCaret.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimMode.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::Offset;

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
// 実物の clangd（Build Tools の LLVM・2026-11-22・D:/NeNeNib/scripts/20261122/lsp-s6 の probe）に
// 整形を頼んだ本文と、その答え（formatting と rangeFormatting は同じ 2 つの編集を返した）。
constexpr std::string_view source_text = "int add(int a, int b) { return a + b; }\n"
                                         "struct Box {\n"
                                         "  int width;\n"
                                         "  int area() const { return width * width; }\n"
                                         "};\n"
                                         "int main() {\n"
                                         "   int x = add(1,\n"
                                         "  2);\n"
                                         "  return add(x, x);\n"
                                         "}\n";
constexpr std::string_view formatted_text = "int add(int a, int b) { return a + b; }\n"
                                            "struct Box {\n"
                                            "  int width;\n"
                                            "  int area() const { return width * width; }\n"
                                            "};\n"
                                            "int main() {\n"
                                            "  int x = add(1, 2);\n"
                                            "  return add(x, x);\n"
                                            "}\n";
constexpr std::string_view clangd_edits =
    R"([{"newText":"\n  ","range":{"end":{"character":3,"line":6},"start":{"character":12,"line":5}}},)"
    R"({"newText":" ","range":{"end":{"character":2,"line":7},"start":{"character":17,"line":6}}}])";
constexpr std::string_view formatting_method = "textDocument/formatting";
constexpr std::string_view range_method = "textDocument/rangeFormatting";
constexpr std::string_view both_providers =
    R"({"textDocumentSync":2,"documentFormattingProvider":true,"documentRangeFormattingProvider":true})";

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

[[nodiscard]] bool caret_on(const app::EditorFrame &frame, std::size_t line, std::size_t column)
{
    return frame.caret.position == core::TextPosition{core::LineNumber{line}, core::Column{column}};
}

[[nodiscard]] SettingsReading lsp_on()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

app::EditorFrame delivered(Editing &editing, app::ServerEvent event)
{
    editing.language_server().serve(std::move(event));
    return editing.controller().apply_frame(app::WorkCompleted{});
}

// main.cpp を開いて capabilities で `initialize` に答える。
void started(Editing &editing, std::string_view capabilities)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                           std::string{capabilities} + "}}"}));
}

app::EditorFrame asked(Editing &editing, core::LanguageQueryKind kind)
{
    return editing.controller().apply_frame(app::QueryLanguage{kind});
}

app::EditorFrame answered(Editing &editing, std::string_view method, std::string_view result)
{
    const auto ids = editing.language_server().requests(method);
    editing.language_server().answer(ids.empty() ? -1 : ids.back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] std::string notice(Editing &editing, core::UiMessage message)
{
    return std::string(core::ui_text(message, editing.controller().display_language()));
}

[[nodiscard]] std::string body(Editing &editing)
{
    return whole_vim_body(editing.controller());
}

// 本文の上の 1 区間（`TextEdit` の JSON・行と桁は 0 始まり）。
[[nodiscard]] std::string edit_json(std::size_t line, std::size_t start, std::size_t end,
                                    std::string_view text)
{
    const std::string at = R"({"line":)" + std::to_string(line) + R"(,"character":)";
    return R"({"newText":")" + std::string{text} + R"(","range":{"start":)" + at +
           std::to_string(start) + R"(},"end":)" + at + std::to_string(end) + "}}}";
}

[[nodiscard]] std::vector<core::LspTextEdit> read(std::string_view json)
{
    const auto parsed = core::parse_json(json);
    if (!parsed.has_value())
    {
        return {};
    }
    return core::lsp_text_edits(parsed.value()).value_or(std::vector<core::LspTextEdit>{});
}

[[nodiscard]] std::optional<core::FormattedSpan> span_of(std::string_view text,
                                                         std::string_view json, Offset caret)
{
    const auto edits = read(json);
    return core::formatted_span(buffer_of(text), edits, core::LspPositionEncoding::utf16, caret);
}

[[nodiscard]] std::string applied_span(std::string_view text, const core::FormattedSpan &span)
{
    std::string whole{text};
    whole.replace(span.range.begin.value, span.range.end.value - span.range.begin.value, span.text);
    return whole;
}

// ---------------------------------------------------------------- core の読みと組み立て

void verify_tables()
{
    for (const core::EditorOperation operation :
         {core::EditorOperation::format_document, core::EditorOperation::format_selection})
    {
        expect(std::ranges::none_of(
                   core::operation_bindings, [operation](const core::OperationBinding &binding)
                   { return binding.operation == operation && binding.chord.has_value(); }) &&
                   std::ranges::count_if(core::operation_bindings,
                                         [operation](const core::OperationBinding &binding)
                                         { return binding.operation == operation; }) == 1,
               "formatting has one row and no key (F1 list only, ADR 0078 keeps Alt keys out)");
    }
}

void verify_reading()
{
    const auto edits = read(clangd_edits);
    expect(edits.size() == 2 && edits.at(0).new_text == "\n  " &&
               edits.at(0).range.start.line == 5 && edits.at(0).range.start.character == 12 &&
               edits.at(1).new_text == " " && edits.at(1).range.end.line == 7,
           "the clangd answer reads as two text edits in the order they came");
    const auto null = core::parse_json("null");
    const auto object = core::parse_json("{}");
    const auto broken = core::parse_json(R"([{"newText":"x"}])");
    expect(null.has_value() && core::lsp_text_edits(null.value()).has_value() &&
               core::lsp_text_edits(null.value()).value().empty() && object.has_value() &&
               core::lsp_text_edits(object.value()).error() ==
                   core::LspAnswerFailure::invalid_result &&
               broken.has_value() &&
               core::lsp_text_edits(broken.value()).error() ==
                   core::LspAnswerFailure::invalid_result,
           "null is no edits, another shape and an edit without a range fail the whole answer");
}

void verify_span()
{
    // 当てる前のキャレットは "  2);" の 2（7 行目・バイトの桁 2）。
    const Offset caret{source_text.find("  2);") + 2};
    const auto span = span_of(source_text, clangd_edits, caret);
    expect(span.has_value() && applied_span(source_text, span.value()) == formatted_text &&
               span.value().range.begin.value == source_text.find("{\n   int") + 1 &&
               span.value().range.end.value == caret.value,
           "the edits become one span from the first start to the last end");
    expect(span.has_value() && span.value().caret.value == formatted_text.find("2);"),
           "the caret after the edits keeps its character");
    // 届いた順が逆でも同じ（始まりの順に並べる）。同じ位置の挿入は届いた順。
    const std::string reversed = "[" + edit_json(0, 3, 3, "b") + "," + edit_json(0, 1, 1, "x") +
                                 "," + edit_json(0, 1, 1, "y") + "]";
    const auto sorted = span_of("abcd", reversed, Offset{0});
    expect(sorted.has_value() && applied_span("abcd", sorted.value()) == "axybcbd" &&
               sorted.value().range.begin.value == 1 && sorted.value().range.end.value == 3,
           "edits are applied by their start and inserts at one place keep their order");
    // キャレットの位置への挿入はキャレットの後ろに入り、編集の中のキャレットは新しい本文の終わりへ。
    const auto inserted = span_of("abcd", "[" + edit_json(0, 2, 2, "__") + "]", Offset{2});
    const auto inside = span_of("a    b", "[" + edit_json(0, 1, 5, " ") + "]", Offset{3});
    expect(inserted.has_value() && inserted.value().caret.value == 2 && inside.has_value() &&
               inside.value().caret.value == 2,
           "an insert at the caret goes after it and a caret inside an edit goes to its end");
    const auto overlapping = span_of(
        "abcd", "[" + edit_json(0, 0, 2, "") + "," + edit_json(0, 1, 3, "") + "]", Offset{0});
    const auto unplaced = span_of("abcd", "[" + edit_json(0, 3, 1, "") + "]", Offset{0});
    expect(!overlapping.has_value() && !unplaced.has_value() &&
               !span_of("abcd", "[]", Offset{0}).has_value(),
           "overlapping edits, a range that does not map and no edits make no span");
}

// ---------------------------------------------------------------- controller の道

// 文書を整形: 要求の形（固定の tabSize 8・insertSpaces false）・本文・キャレット・1 回の Ctrl+Z で
// 元の本文へ戻る。
void verify_document_formatting()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{8}, core::Column{3}},
                            core::SelectionAnchoring::collapse});
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    const auto sent = editing.language_server().notifications(formatting_method);
    expect(sent.size() == 1 && contains(sent.front(), R"("tabSize":8)") &&
               contains(sent.front(), R"("insertSpaces":false)") &&
               contains(sent.front(), R"("uri":"file:///C:/work/main.cpp")") &&
               !contains(sent.front(), R"("range")"),
           "format document sends the document and the fixed options of Nib");
    const auto done = answered(editing, formatting_method, clangd_edits);
    expect(body(editing) == formatted_text && caret_on(done, 7, 18) && message_of(done).empty(),
           "the answer is applied and the caret stays on its character");
    applied(editing.controller(), app::HistoryAction{core::HistoryDirection::undo});
    expect(body(editing) == source_text, "one Ctrl+Z brings the whole text back");
    applied(editing.controller(), app::HistoryAction{core::HistoryDirection::redo});
    expect(body(editing) == formatted_text, "Ctrl+Y applies the formatting again");
}

// Vim の `u` は 1 回で元の本文と当てる前のキャレットへ戻る（ADR 0191 の強制）。
void verify_vim_undo()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "8G0l");
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(answered(editing, formatting_method, clangd_edits));
    expect(body(editing) == formatted_text && caret_on(editing.controller().frame(), 7, 18),
           "Vim applies the same answer");
    vim_replay(editing.controller(), "u");
    expect(body(editing) == source_text && caret_on(editing.controller().frame(), 8, 2),
           "one u brings back the text and the caret from before the formatting");
}

// 選択範囲を整形: 範囲を送る。選択が空ならキャレットの行。Vim の VISUAL は NORMAL へ戻る。
void verify_range_formatting()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{6}, core::Column{1}},
                            core::SelectionAnchoring::collapse});
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::extend});
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::extend});
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_selection));
    const auto sent = editing.language_server().notifications(range_method);
    expect(
        sent.size() == 1 &&
            contains(
                sent.front(),
                R"("range":{"start":{"line":5,"character":0},"end":{"line":7,"character":0}})") &&
            contains(sent.front(), R"("tabSize":8)"),
        "format selection sends the selected range");
    static_cast<void>(answered(editing, range_method, clangd_edits));
    expect(body(editing) == formatted_text, "the range answer is applied the same way");

    Editing line(lsp_on());
    started(line, both_providers);
    applied(line.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{7}, core::Column{5}},
                            core::SelectionAnchoring::collapse});
    static_cast<void>(asked(line, core::LanguageQueryKind::format_selection));
    const auto caret_line = line.language_server().notifications(range_method);
    expect(caret_line.size() == 1 &&
               contains(
                   caret_line.front(),
                   R"("range":{"start":{"line":6,"character":0},"end":{"line":6,"character":17}})"),
           "with nothing selected the caret line is formatted");

    Editing vim(lsp_on());
    started(vim, both_providers);
    applied(vim.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(vim.controller(), "6GVj");
    static_cast<void>(asked(vim, core::LanguageQueryKind::format_selection));
    const auto visual = vim.language_server().notifications(range_method);
    expect(visual.size() == 1 &&
               contains(
                   visual.front(),
                   R"("range":{"start":{"line":5,"character":0},"end":{"line":6,"character":17}})"),
           "Vim VISUAL LINE sends its lines");
    const auto done = answered(vim, range_method, clangd_edits);
    expect(body(vim) == formatted_text && done.vim_mode == core::VimMode::normal,
           "applying leaves VISUAL for NORMAL");
}

// 頼んだ後に本文が変わった・読めない答え・空の答え・今と同じ結果。
void verify_stale_and_unreadable()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    applied(editing.controller(), app::InsertText{"x"});
    const std::string typed = body(editing);
    const auto stale = answered(editing, formatting_method, clangd_edits);
    expect(body(editing) == typed &&
               message_of(stale) == notice(editing, core::UiMessage::notice_formatting_stale),
           "an answer for an older revision is not applied and says so");

    Editing broken(lsp_on());
    started(broken, both_providers);
    static_cast<void>(asked(broken, core::LanguageQueryKind::format_document));
    const auto unreadable = answered(broken, formatting_method, R"([{"newText":"x"}])");
    expect(body(broken) == source_text &&
               message_of(unreadable) ==
                   notice(broken, core::UiMessage::notice_formatting_unreadable),
           "an answer that cannot be read is not applied and says so");
    static_cast<void>(asked(broken, core::LanguageQueryKind::format_document));
    const auto overlapping =
        answered(broken, formatting_method,
                 "[" + edit_json(0, 0, 4, "") + "," + edit_json(0, 2, 6, "") + "]");
    expect(body(broken) == source_text &&
               message_of(overlapping) ==
                   notice(broken, core::UiMessage::notice_formatting_unreadable),
           "overlapping edits are not applied");

    Editing quiet(lsp_on());
    started(quiet, both_providers);
    static_cast<void>(asked(quiet, core::LanguageQueryKind::format_document));
    const auto empty = answered(quiet, formatting_method, "[]");
    static_cast<void>(asked(quiet, core::LanguageQueryKind::format_document));
    const auto same = answered(quiet, formatting_method, "[" + edit_json(1, 0, 6, "struct") + "]");
    applied(quiet.controller(), app::HistoryAction{core::HistoryDirection::undo});
    expect(message_of(empty).empty() && message_of(same).empty() && body(quiet) == source_text,
           "an empty answer and an answer with the same text push no undo unit");
}

// 能力の無いサーバーには頼まず、動いていなければ今の 1 行。
void verify_unsupported()
{
    Editing plain(lsp_on());
    started(plain, R"({"textDocumentSync":2,"documentFormattingProvider":true})");
    const std::size_t before = plain.language_server().sent().size();
    const auto range = asked(plain, core::LanguageQueryKind::format_selection);
    expect(plain.language_server().sent().size() == before &&
               message_of(range) ==
                   notice(plain, core::UiMessage::notice_range_formatting_not_supported),
           "a server without documentRangeFormattingProvider is not asked and says so");
    Editing none(lsp_on());
    started(none, R"({"textDocumentSync":2})");
    const auto document = asked(none, core::LanguageQueryKind::format_document);
    expect(message_of(document) == notice(none, core::UiMessage::notice_formatting_not_supported),
           "a server without documentFormattingProvider says so");
    Editing off(
        SettingsReading{std::optional<core::EditorSettings>{core::default_editor_settings()}});
    off.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    applied(off.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    const auto stopped = asked(off, core::LanguageQueryKind::format_document);
    expect(off.language_server().sent().empty() &&
               message_of(stopped) ==
                   notice(off, core::UiMessage::notice_language_server_not_running),
           "with lsp off nothing is sent and the not running line is shown");
}
// 固定の種の線形合同（時刻を読まない）。
[[nodiscard]] std::size_t next_random(std::size_t &state)
{
    state = (state * 6364136223846793005U) + 1442695040888963407U;
    return static_cast<std::size_t>(state >> 33U);
}

// ASCII の本文のバイト位置 → `{"line":L,"character":C}`（UTF-16 の単位はバイトと同じ）。
[[nodiscard]] std::string position_json(std::string_view text, std::size_t at)
{
    const std::size_t line = static_cast<std::size_t>(
        std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(at), '\n'));
    const std::size_t start = text.rfind('\n', at == 0 ? 0 : at - 1);
    const std::size_t column = at == 0 || start == std::string_view::npos ? at : at - start - 1;
    return R"({"line":)" + std::to_string(line) + R"(,"character":)" + std::to_string(column) + "}";
}

constexpr std::string_view property_letters = "ab {}\n\n x";

// 長さ length の ASCII の文字列（letters の先頭 choices 文字から選ぶ）。
[[nodiscard]] std::string random_text(std::size_t &state, std::size_t length, std::size_t choices)
{
    std::string text;
    for (std::size_t index = 0; index < length; ++index)
    {
        text += property_letters.at(next_random(state) % choices);
    }
    return text;
}

// 性質の 1 回分: 答えの JSON と、後ろから 1 つずつ当てた本文。
using PropertyCase = std::pair<std::string, std::string>;

// 最後の改行には触れない（eol の文書の最後の改行の扱いは replace の 1 か所が持つ）。
[[nodiscard]] PropertyCase random_case(std::size_t &state, const std::string &text)
{
    std::vector<std::size_t> marks;
    const std::size_t count = 2 * (1 + (next_random(state) % 4));
    for (std::size_t index = 0; index < count; ++index)
    {
        marks.push_back(next_random(state) % (text.size() - 1));
    }
    std::ranges::sort(marks);
    std::string json = "[";
    std::vector<std::string> inserted;
    for (std::size_t index = 0; index < count; index += 2)
    {
        inserted.push_back(random_text(state, next_random(state) % 4, 5));
        json += (index == 0 ? "" : ",") + std::string{R"({"newText":")"} + inserted.back() +
                R"(","range":{"start":)" + position_json(text, marks.at(index)) + R"(,"end":)" +
                position_json(text, marks.at(index + 1)) + "}}";
    }
    std::string expected = text;
    for (std::size_t index = inserted.size(); index-- > 0;)
    {
        expected.replace(marks.at(2 * index), marks.at((2 * index) + 1) - marks.at(2 * index),
                         inserted.at(index));
    }
    return {json + "]", expected};
}

// Vim で text を開き、line 行目で整形の答え json を当て、`u` を打つ。
void check_property_case(const std::string &text, const PropertyCase &formatting, std::size_t line)
{
    Editing editing(lsp_on());
    editing.files().hold_at(std::string(source_path), Bytes{text});
    static_cast<void>(editing.controller().apply(app::VisibleLines{80}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2,"documentFormattingProvider":true}}})"}));
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), std::to_string(line) + "G");
    const auto caret = editing.controller().frame().caret.position;
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(answered(editing, formatting_method, formatting.first));
    const std::string formatted = body(editing);
    vim_replay(editing.controller(), "u");
    const bool unchanged = formatting.second == text;
    expect(formatted == formatting.second,
           "the formatted text is the edits applied one by one from the back");
    expect(unchanged || body(editing) == text,
           "one u brings back the text from before the formatting");
    expect(unchanged || editing.controller().frame().caret.position == caret,
           "one u brings back the caret from before the formatting");
}

// 性質（ADR 0191 の強制）: 重ならない編集の列をどう組んでも、整形の結果は後ろから 1 つずつ当てた
// 本文と同じで、Vim の `u` 1 回で元の本文と当てる前のキャレットへ戻る。
void verify_undo_property()
{
    std::size_t state = 1993;
    for (std::size_t round = 0; round < 24; ++round)
    {
        const std::string text =
            random_text(state, 20 + (next_random(state) % 40), property_letters.size()) + "\n";
        const PropertyCase formatting = random_case(state, text);
        check_property_case(text, formatting, 1 + (next_random(state) % 6));
    }
}
} // namespace

void verify_lsp_formatting_scope()
{
    verify_tables();
    verify_reading();
    verify_span();
    verify_document_formatting();
    verify_vim_undo();
    verify_range_formatting();
    verify_stale_and_unreadable();
    verify_unsupported();
    verify_undo_property();
}
} // namespace nenenib::tests
