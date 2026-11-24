// scope `--lsp-formatting` の契約（#1993・ADR 0191 の決定 7）。言語サーバーの整形: 操作「文書を
// 整形」「選択範囲を整形」（鍵なし）。core の読みと 1 区間の組み立て、controller の道（要求の形・
// 1 つの undo の単位・頼んだ後に変わった文書・能力の無いサーバー）を替え玉 ScriptedLanguageServer
// で確かめる。
#include "CaretMotion.hpp"
#include "CommandText.hpp"
#include "ComposeText.hpp"
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
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenCompletion.hpp"
#include "OpenContextMenu.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "OperationBindings.hpp"
#include "OperationTexts.hpp"
#include "PaneFrame.hpp"
#include "PlaceCaret.hpp"
#include "QueryLanguage.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
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

// 性質の 1 回分の編集（元の本文の上の [始まり, 終わり) と新しい本文・始まりの順）。
using PlannedEdit = std::pair<std::pair<std::size_t, std::size_t>, std::string>;

// 重ならない編集の列。始まりは前の編集の終わりより後ろ（同じ位置の挿入どうしの順は試さない・
// verify_span が縛る）。最後の改行には触れない（eol の文書の最後の改行の扱いは replace の 1
// か所）。
[[nodiscard]] std::vector<PlannedEdit> random_edits(std::size_t &state, const std::string &text)
{
    std::vector<std::size_t> marks;
    const std::size_t count = 2 * (1 + (next_random(state) % 4));
    for (std::size_t index = 0; index < count; ++index)
    {
        marks.push_back(next_random(state) % (text.size() - 1));
    }
    std::ranges::sort(marks);
    std::vector<PlannedEdit> edits;
    for (std::size_t index = 0; index < count; index += 2)
    {
        std::string added = random_text(state, next_random(state) % 4, 5);
        if (edits.empty() || marks.at(index) > edits.back().first.second)
        {
            edits.emplace_back(std::pair{marks.at(index), marks.at(index + 1)}, std::move(added));
        }
    }
    return edits;
}

// 答えの JSON。届く順は固定の種で混ぜる。
[[nodiscard]] std::string shuffled_json(std::size_t &state, const std::string &text,
                                        std::vector<PlannedEdit> edits)
{
    for (std::size_t index = edits.size(); index > 1; --index)
    {
        std::swap(edits.at(index - 1), edits.at(next_random(state) % index));
    }
    std::string json = "[";
    for (const auto &[range, added] : edits)
    {
        json += (json.size() == 1 ? "" : ",") + std::string{R"({"newText":")"} + added +
                R"(","range":{"start":)" + position_json(text, range.first) + R"(,"end":)" +
                position_json(text, range.second) + "}}";
    }
    return json + "]";
}

// 参照の実装: 後ろから 1 つずつ当てた本文。
[[nodiscard]] std::string applied_back_to_front(std::string text,
                                                const std::vector<PlannedEdit> &edits)
{
    for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit)
    {
        text.replace(edit->first.first, edit->first.second - edit->first.first, edit->second);
    }
    return text;
}

// 参照の実装: 当てた後のキャレット（手前で終わる編集は長さの差だけ・中なら新しい本文の終わり）。
[[nodiscard]] std::size_t caret_after(std::size_t caret, const std::vector<PlannedEdit> &edits)
{
    std::size_t moved = caret;
    for (const auto &[range, added] : edits)
    {
        if (range.first >= caret)
        {
            break;
        }
        if (range.second > caret)
        {
            return moved - (caret - range.first) + added.size();
        }
        moved = moved + added.size() - (range.second - range.first);
    }
    return moved;
}

// ASCII の本文の上の frame のキャレットのバイト位置。
[[nodiscard]] std::size_t caret_offset(std::string_view text, const app::EditorFrame &frame)
{
    std::size_t at = 0;
    for (std::size_t line = 1; line < frame.caret.position.line.value; ++line)
    {
        at = text.find('\n', at) + 1;
    }
    return at + frame.caret.position.column.value - 1;
}

// text を開いて `initialize` に答える（文書の整形だけを持つ席）。
void opened(Editing &editing, const std::string &text)
{
    editing.files().hold_at(std::string(source_path), Bytes{text});
    static_cast<void>(editing.controller().apply(app::VisibleLines{80}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2,"documentFormattingProvider":true}}})"}));
}

// 行頭のバイト位置（1 始まりの行）。
[[nodiscard]] std::size_t line_start_of(std::string_view text, std::size_t line)
{
    std::size_t at = 0;
    for (std::size_t index = 1; index < line; ++index)
    {
        at = text.find('\n', at) + 1;
    }
    return at;
}

// 編集が触れない行（行の始まりから行末の改行まで、どの編集の区間とも重ならない）を 1 つ選ぶ。
// 無ければ 0。
[[nodiscard]] std::size_t untouched_line(std::size_t &state, std::string_view text,
                                         const std::vector<PlannedEdit> &edits)
{
    const auto lines = static_cast<std::size_t>(std::ranges::count(text, '\n'));
    const std::size_t first = 1 + (next_random(state) % lines);
    for (std::size_t step = 0; step < lines; ++step)
    {
        const std::size_t line = 1 + ((first - 1 + step) % lines);
        const std::size_t start = line_start_of(text, line);
        const std::size_t end = text.find('\n', start);
        const bool touched =
            std::ranges::any_of(edits, [start, end](const PlannedEdit &edit)
                                { return edit.first.second >= start && edit.first.first <= end; });
        if (!touched)
        {
            return line;
        }
    }
    return 0;
}

// 参照の実装: 編集が触れない行の整形の後の行（手前の編集の改行の差だけずれる）。
[[nodiscard]] std::size_t line_after(std::string_view text, std::size_t line,
                                     const std::vector<PlannedEdit> &edits)
{
    const std::size_t start = line_start_of(text, line);
    std::size_t moved = line;
    for (const auto &[range, added] : edits)
    {
        if (range.second < start)
        {
            const std::string_view removed = text.substr(range.first, range.second - range.first);
            moved = moved + static_cast<std::size_t>(std::ranges::count(added, '\n')) -
                    static_cast<std::size_t>(std::ranges::count(removed, '\n'));
        }
    }
    return moved;
}

// `'a` で跳んだ行（マークが消えていれば跳ばずに今の行）。
[[nodiscard]] std::size_t mark_line(Editing &editing)
{
    vim_replay(editing.controller(), "'a");
    return editing.controller().frame().caret.position.line.value;
}

// マークの行が、畳んだ 1 区間（最初の編集の始まりから最後の編集の終わり）の内側にあるか。畳んだ
// 1 区間で直すと消えるか端へ寄る行（3 回目のレビュー F2）。
[[nodiscard]] bool inside_folded_span(std::string_view text, std::size_t line,
                                      const std::vector<PlannedEdit> &edits)
{
    if (edits.empty())
    {
        return false;
    }
    const std::size_t start = line_start_of(text, line);
    const std::size_t end = text.find('\n', start);
    const std::size_t low =
        std::ranges::min(edits, {}, [](const PlannedEdit &edit) { return edit.first.first; })
            .first.first;
    const std::size_t high =
        std::ranges::max(edits, {}, [](const PlannedEdit &edit) { return edit.first.second; })
            .first.second;
    return start > low && end < high;
}

// Vim へ入って line 行目に `ma` を置き、通常モードへ戻る。
void mark_line_a(Editing &editing, std::size_t line)
{
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), std::to_string(line) + "Gma");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::ordinary});
}

// 通常モードで line 行目の行の中の桁から整形の答えを当て、Vim の `u` と `Ctrl-r` を打つ。編集が
// 触れない行に `ma` を置き、整形・`u`・`Ctrl-r` の後の `'a` の行を参照と比べる（2 回目のレビュー
// P1・F6）。マークを置けたら marks.first を 1 増やし、畳んだ 1 区間の内側なら marks.second も 1
// 増やす（3 回目のレビュー F2）。
void check_property_case(std::size_t &state, const std::string &text, std::size_t line,
                         std::pair<std::size_t, std::size_t> &marks)
{
    const std::vector<PlannedEdit> edits = random_edits(state, text);
    const std::string json = shuffled_json(state, text, edits);
    const std::string expected = applied_back_to_front(text, edits);
    const std::size_t start = line_start_of(text, line);
    const std::size_t length = text.find('\n', start) - start;
    const std::size_t column = 1 + (next_random(state) % std::max<std::size_t>(length, 1));
    const std::size_t mark = untouched_line(state, text, edits);
    Editing editing(lsp_on());
    opened(editing, text);
    if (mark != 0)
    {
        mark_line_a(editing, mark);
        ++marks.first;
        marks.second += inside_folded_span(text, mark, edits) ? 1U : 0U;
    }
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{line}, core::Column{column}},
                            core::SelectionAnchoring::collapse});
    const auto caret = editing.controller().frame().caret.position;
    const std::size_t before = caret_offset(text, editing.controller().frame());
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    const auto done = answered(editing, formatting_method, json);
    const std::string formatted = body(editing);
    const bool unchanged = expected == text;
    expect(formatted == expected,
           "the formatted text is the edits applied one by one from the back in any order");
    expect(unchanged || caret_offset(formatted, done) == caret_after(before, edits),
           "the caret after the formatting is where the reference puts it");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "u");
    expect(unchanged || body(editing) == text,
           "one u brings back the text from before the formatting");
    expect(unchanged || editing.controller().frame().caret.position == caret,
           "one u brings back the caret from before the formatting");
    if (mark == 0 || unchanged)
    {
        return;
    }
    expect(mark_line(editing) == mark, "after u a mark on an untouched line is on its line again");
    vim_replay(editing.controller(), "<C-r>");
    expect(body(editing) == expected, "Ctrl-r applies the formatting again");
    expect(mark_line(editing) == line_after(text, mark, edits),
           "after Ctrl-r a mark on an untouched line is where the formatting moved it");
}

// 性質（ADR 0191 の強制）: 重ならない編集の列をどの順で届けても、整形の結果は後ろから 1 つずつ
// 当てた本文と同じで、当てた直後のキャレットは参照の写しと同じで、Vim の `u` 1 回で元の本文と
// 当てる前のキャレットへ戻る。編集が触れない行のマークは `u` で元の行へ、`Ctrl-r` で参照の行へ。
void verify_undo_property()
{
    std::size_t state = 1993;
    std::pair<std::size_t, std::size_t> marks{0, 0};
    for (std::size_t round = 0; round < 24; ++round)
    {
        const std::string text =
            random_text(state, 20 + (next_random(state) % 40), property_letters.size()) + "\n";
        const auto lines = static_cast<std::size_t>(std::ranges::count(text, '\n'));
        check_property_case(state, text, 1 + (next_random(state) % lines), marks);
    }
    expect(marks.first >= 8, "the property puts a mark on an untouched line in many rounds");
    expect(marks.second >= 1,
           "the property puts a mark between edits, inside the folded span, in some rounds");
}

// 整形して保存したバイト列（本文の形と改行をそのまま見る）。
[[nodiscard]] std::string formatted_bytes(const std::string &text, std::string_view json)
{
    Editing editing(lsp_on());
    opened(editing, text);
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(answered(editing, formatting_method, json));
    static_cast<void>(editing.controller().apply_frame(
        app::SaveDocument{core::FilePath::parse(source_path).value(), core::TextEncoding::utf8}));
    return editing.files().written();
}

// 1 件ずつ: 代理対・CRLF の文書に LF の newText（R2）・文書の端を超える range・改行の無い最後の行。
void verify_edges()
{
    expect(formatted_bytes("a\xF0\x9F\x98\x80"
                           "b {\n",
                           "[" + edit_json(0, 3, 4, "c") + "]") == "a\xF0\x9F\x98\x80"
                                                                   "c {\n",
           "a position after a surrogate pair counts two UTF-16 units");
    expect(
        formatted_bytes(
            "int main() {\r\n   return 0;\r\n}\r\n",
            R"([{"newText":"\n  ","range":{"start":{"line":0,"character":12},"end":{"line":1,"character":3}}}])") ==
            "int main() {\r\n  return 0;\r\n}\r\n",
        "a LF in newText enters a CRLF document as CRLF like a paste");
    expect(
        formatted_bytes(
            "a\nb\n",
            R"([{"newText":"","range":{"start":{"line":1,"character":1},"end":{"line":5,"character":0}}}])") ==
            "a\nb",
        "a range past the last line ends at the end of the document");
    expect(formatted_bytes("int x;  ", "[" + edit_json(0, 6, 8, "") + "]") == "int x;",
           "an edit on the last line without a newline is applied");
}

// 離れた 2 つの編集（2 行目と 9 行目）の答え。畳んだ 1 区間は 3〜8 行目を覆う。
[[nodiscard]] std::string apart_edits()
{
    return "[" + edit_json(1, 10, 10, " ") + "," + edit_json(8, 2, 2, " ") + "]";
}

[[nodiscard]] const app::PaneFrame *other_pane(const app::EditorFrame &frame)
{
    return frame.other.has_value() ? std::get_if<app::PaneFrame>(&frame.other.value()) : nullptr;
}

// もう 1 つの枠のキャレットが line 行目で、先頭行が top か。
[[nodiscard]] bool other_at(const app::EditorFrame &frame, std::size_t line, std::size_t column,
                            std::size_t top)
{
    const auto *other = other_pane(frame);
    return other != nullptr && other->caret.position.line.value == line &&
           other->caret.position.column.value == column && other->first_visible.value == top;
}

// 5 行目のキャレットと 3〜5 行目の先頭行を持つもう 1 つの枠を作り、操作中の枠は先頭へ戻す。
// もう 1 つの枠の先頭行を返す（0 なら作れなかった）。
[[nodiscard]] std::size_t split_on_line_five(Editing &editing)
{
    static_cast<void>(editing.controller().apply(app::VisibleLines{3}));
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{5}, core::Column{2}},
                            core::SelectionAnchoring::collapse});
    static_cast<void>(editing.controller().apply(app::SplitPane{core::SplitLayout::vertical}));
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{1}},
                            core::SelectionAnchoring::collapse});
    const app::EditorFrame frame = editing.controller().frame();
    const auto *other = other_pane(frame);
    return other == nullptr ? 0 : other->first_visible.value;
}

// 離れた 2 つの編集の間の行のマークともう 1 つの枠（畳んだ 1 区間では消える行・端へ寄る位置）は、
// 整形・`u`・`Ctrl-r` のどれでも同じ行に残る（#1997 のレビュー R1・2 回目のレビュー P1・F6）。
void verify_marks_between_edits()
{
    Editing apart(lsp_on());
    started(apart, both_providers);
    applied(apart.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(apart.controller(), "5Gmc");
    const std::size_t top = split_on_line_five(apart);
    expect(top > 2 && top <= 5 && other_at(apart.controller().frame(), 5, 2, top),
           "the other pane starts inside the lines the two edits span");
    static_cast<void>(asked(apart, core::LanguageQueryKind::format_document));
    const auto done = answered(apart, formatting_method, apart_edits());
    expect(other_at(done, 5, 2, top), "the other pane keeps its caret and top line");
    vim_replay(apart.controller(), "gg'c");
    expect(apart.controller().frame().caret.position.line.value == 5,
           "a mark between two edits stays on its line");
    vim_replay(apart.controller(), "u");
    expect(body(apart) == source_text && other_at(apart.controller().frame(), 5, 2, top),
           "u keeps the caret and top line of the other pane");
    vim_replay(apart.controller(), "gg'c");
    expect(apart.controller().frame().caret.position.line.value == 5,
           "after u the mark between two edits stays on its line");
    vim_replay(apart.controller(), "<C-r>");
    expect(other_at(apart.controller().frame(), 5, 2, top),
           "Ctrl-r keeps the caret and top line of the other pane");
    vim_replay(apart.controller(), "gg'c");
    expect(apart.controller().frame().caret.position.line.value == 5,
           "after Ctrl-r the mark between two edits stays on its line");
}

// 通常モードの Ctrl+Z / Ctrl+Y も、もう 1 つの枠を編集ごとに直す（2 回目のレビュー P1）。
void verify_other_pane_on_ctrl_z()
{
    Editing plain(lsp_on());
    started(plain, both_providers);
    applied(plain.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(plain.controller(), "5Gmc");
    applied(plain.controller(), app::SelectEditMode{core::EditMode::ordinary});
    const std::size_t top = split_on_line_five(plain);
    static_cast<void>(asked(plain, core::LanguageQueryKind::format_document));
    static_cast<void>(answered(plain, formatting_method, apart_edits()));
    const auto undone =
        plain.controller().apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(body(plain) == source_text && other_at(undone, 5, 2, top),
           "Ctrl+Z keeps the caret and top line of the other pane");
    const auto redone =
        plain.controller().apply_frame(app::HistoryAction{core::HistoryDirection::redo});
    expect(other_at(redone, 5, 2, top), "Ctrl+Y keeps the caret and top line of the other pane");
    applied(plain.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(plain.controller(), "gg'c");
    expect(plain.controller().frame().caret.position.line.value == 5,
           "after Ctrl+Z and Ctrl+Y the mark between two edits stays on its line");
}

// 整形を `u` で戻した後に同じ履歴の位置へ別の単位が積まれても、脇の表の照合が外れて畳んだ 1 区間で
// 直し、例外を投げない（3 回目のレビュー P2 の a）。
void verify_unit_replaced_after_undo()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "5Gmc");
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(answered(editing, formatting_method, apart_edits()));
    vim_replay(editing.controller(), "u");
    vim_replay(editing.controller(), "ggix<Esc>");
    expect(body(editing) == "x" + std::string(source_text),
           "an edit after u takes the place of the formatting in the history");
    vim_replay(editing.controller(), "u");
    expect(body(editing) == source_text, "u takes back the edit that replaced the formatting");
    vim_replay(editing.controller(), "<C-r>");
    expect(body(editing) == "x" + std::string(source_text),
           "Ctrl-r brings back the edit, not the formatting");
    vim_replay(editing.controller(), "gg'c");
    expect(editing.controller().frame().caret.position.line.value == 5,
           "the mark stays on its line through the replaced unit");
}

// 整形した文書と同じ履歴の位置に別のタブの単位があっても、そのタブの `u` は脇の表を引かない
// （3 回目のレビュー P2 の b）。戻って整形したタブの `u` は編集ごとに直す。
void verify_unit_in_other_tab()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "5Gmc");
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(answered(editing, formatting_method, apart_edits()));
    applied(editing.controller(), app::NewTab{});
    vim_replay(editing.controller(), "iab<Esc>");
    vim_replay(editing.controller(), "u");
    expect(body(editing).empty(), "u in the other tab takes back its own edit");
    applied(editing.controller(), app::SwitchTab{0});
    vim_replay(editing.controller(), "u");
    expect(body(editing) == source_text, "u in the formatted tab takes back the formatting");
    vim_replay(editing.controller(), "gg'c");
    expect(editing.controller().frame().caret.position.line.value == 5,
           "after u in the formatted tab the mark between two edits stays on its line");
}

// マーク（区間の中と下の行）ともう 1 つの枠のキャレットは、編集ごとに直る（#1997 のレビュー R1）。
void verify_marks_and_other_pane()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "7Gma9Gmb7G4l");
    static_cast<void>(editing.controller().apply(app::SplitPane{core::SplitLayout::vertical}));
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    const auto done = answered(editing, formatting_method, clangd_edits);
    const auto *other =
        done.other.has_value() ? std::get_if<app::PaneFrame>(&done.other.value()) : nullptr;
    expect(body(editing) == formatted_text && other != nullptr &&
               other->caret.position.line.value == 7 && other->caret.position.column.value == 7,
           "the caret of the other pane stays on its character");
    vim_replay(editing.controller(), "gg'a");
    expect(editing.controller().frame().caret.position.line.value == 7,
           "a mark on a line inside the edits stays on its line");
    vim_replay(editing.controller(), "gg'b");
    expect(editing.controller().frame().caret.position.line.value == 8,
           "a mark below the edits moves up with the joined line");

    verify_marks_between_edits();
    verify_other_pane_on_ctrl_z();
    verify_unit_replaced_after_undo();
    verify_unit_in_other_tab();
}

// 答えが当たらずに「入力の途中」と知らせ、本文が変わっていないか。
[[nodiscard]] bool refused_as_busy(Editing &editing, const app::EditorFrame &late)
{
    return message_of(late) == notice(editing, core::UiMessage::notice_formatting_busy) &&
           body(editing) == source_text;
}

// Vim で頼んだ後に keys を流してから答えが届くと、当てずに「入力の途中」と知らせる。
void expect_busy_after_keys(std::string_view keys, const char *what)
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    vim_replay(editing.controller(), keys);
    const auto late = answered(editing, formatting_method, clangd_edits);
    expect(refused_as_busy(editing, late), what);
}

// 通常モードで頼んだ後に intent を送ってから答えが届くと、当てずに「入力の途中」と知らせる。
void expect_busy_after_intent(const app::EditorIntent &intent, const char *what)
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{9}, core::Column{5}},
                            core::SelectionAnchoring::collapse});
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(editing.controller().apply_frame(intent));
    const auto late = answered(editing, formatting_method, clangd_edits);
    expect(refused_as_busy(editing, late), what);
}

// ADR 0191 の強制の行に書いた「入力の途中」の条件を 1 つずつ（#1997 の 2 回目のレビュー F1〜F3）。
// Vim が次の鍵を待つ間（回数・レジスタ・1 鍵の待ち・INSERT の `Ctrl-o`・`map` の左辺の途中・
// イージーモーションの印）・INSERT を組み立てている間（`3i`・矩形の `I`・打つ前の `i`）・IME・
// 右クリックのメニュー・設定の画面・補完の一覧。
void verify_busy_states()
{
    expect_busy_after_keys("3", "an answer that arrives while a count is typed is not applied");
    expect_busy_after_keys("\"a", "an answer that arrives after a register name is not applied");
    expect_busy_after_keys("f",
                           "an answer that arrives while f waits for a character is not applied");
    expect_busy_after_keys("i<C-o>", "an answer that arrives after INSERT Ctrl-o is not applied");
    expect_busy_after_keys("3i", "an answer that arrives in INSERT with a count is not applied");
    expect_busy_after_keys("<C-v>jI", "an answer that arrives in a block INSERT is not applied");
    expect_busy_after_keys("i", "an answer that arrives in INSERT before typing is not applied");

    Editing clicked(lsp_on());
    started(clicked, both_providers);
    applied(clicked.controller(), app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(asked(clicked, core::LanguageQueryKind::format_document));
    vim_replay(clicked.controller(), "i");
    applied(clicked.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{3}, core::Column{2}},
                            core::SelectionAnchoring::collapse});
    expect(clicked.controller().vim_state().mode == core::VimMode::insert &&
               refused_as_busy(clicked, answered(clicked, formatting_method, clangd_edits)),
           "an answer that arrives in INSERT after a click is not applied (3rd review P1)");

    Editing mapped(lsp_on());
    started(mapped, both_providers);
    applied(mapped.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(mapped.controller(), ":");
    static_cast<void>(mapped.controller().apply_frame(app::CommandText{"nnoremap Qx dd"}));
    static_cast<void>(mapped.controller().apply_frame(app::SubmitCommand{}));
    static_cast<void>(asked(mapped, core::LanguageQueryKind::format_document));
    vim_replay(mapped.controller(), "Q");
    expect(refused_as_busy(mapped, answered(mapped, formatting_method, clangd_edits)),
           "an answer that arrives in the middle of a map left side is not applied");

    Editing labelled(lsp_on());
    started(labelled, both_providers);
    applied(labelled.controller(), app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(asked(labelled, core::LanguageQueryKind::format_document));
    for (const char32_t code : std::u32string_view{U"\\\\w"})
    {
        static_cast<void>(labelled.controller().apply_frame(
            app::VimKeyPress{core::VimKey{core::VimCharacter{code}}}));
    }
    expect(labelled.controller().vim_state().easy_motion.has_value() &&
               refused_as_busy(labelled, answered(labelled, formatting_method, clangd_edits)),
           "an answer that arrives while easy motion labels are shown is not applied");

    expect_busy_after_intent(app::ComposeText{composed_of("\xE3\x81\x82", {}, 0)},
                             "an answer that arrives during an IME composition is not applied");
    expect_busy_after_intent(
        app::OpenContextMenu{10, 40, std::nullopt},
        "an answer that arrives while the context menu is open is not applied");
    expect_busy_after_intent(
        app::OpenSettings{},
        "an answer that arrives while the settings screen is open is not applied");
    expect_busy_after_intent(
        app::OpenCompletion{},
        "an answer that arrives while the completion list is open is not applied");
}

// 録画中（`qa`）は待ちでないので当たり、整形は録画に入らない（`@a` で整形しない・2 回目のレビュー
// F3）。
void verify_recording_applies()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "qa");
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    const auto done = answered(editing, formatting_method, clangd_edits);
    expect(body(editing) == formatted_text && message_of(done).empty(),
           "an answer that arrives while a macro is recorded is applied");
    vim_replay(editing.controller(), "qu@a");
    expect(body(editing) == source_text, "the formatting is not in the recorded macro");
}

// Vim の INSERT で頼んだ整形は、サーバーへ頼まずに 1 行で断る（#1997 の 2 回目のレビュー F1）。
// NORMAL に戻れば頼む。
void verify_insert_refusal()
{
    for (const auto kind :
         {core::LanguageQueryKind::format_document, core::LanguageQueryKind::format_selection})
    {
        Editing editing(lsp_on());
        started(editing, both_providers);
        applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
        vim_replay(editing.controller(), "i");
        const std::size_t before = editing.language_server().sent().size();
        const auto refused = asked(editing, kind);
        expect(editing.language_server().sent().size() == before &&
                   message_of(refused) ==
                       notice(editing, core::UiMessage::notice_formatting_in_insert) &&
                   editing.controller().vim_state().mode == core::VimMode::insert,
               "formatting asked in INSERT is refused before the server is asked");
        vim_replay(editing.controller(), "<Esc>");
        static_cast<void>(asked(editing, kind));
        expect(editing.language_server().sent().size() == before + 1,
               "formatting asked after Esc goes to the server");
    }
}

// 頼んだ後のタブの切り替え（R4）・入力の途中に届いた答え（L5）・didChange と未保存の印（L8）。
void verify_switch_busy_and_sync()
{
    Editing tab(lsp_on());
    started(tab, both_providers);
    static_cast<void>(asked(tab, core::LanguageQueryKind::format_document));
    applied(tab.controller(), app::NewTab{});
    const auto switched = answered(tab, formatting_method, clangd_edits);
    applied(tab.controller(), app::SwitchTab{0});
    expect(message_of(switched) == notice(tab, core::UiMessage::notice_formatting_stale) &&
               body(tab) == source_text,
           "an answer that arrives after a tab switch is not applied and says so");

    Editing vim(lsp_on());
    started(vim, both_providers);
    applied(vim.controller(), app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(asked(vim, core::LanguageQueryKind::format_document));
    vim_replay(vim.controller(), "d");
    const auto pending = answered(vim, formatting_method, clangd_edits);
    expect(message_of(pending) == notice(vim, core::UiMessage::notice_formatting_busy) &&
               body(vim) == source_text,
           "an answer that arrives while an operator waits is not applied and says so");
    vim_replay(vim.controller(), "<Esc>");
    static_cast<void>(asked(vim, core::LanguageQueryKind::format_document));
    vim_replay(vim.controller(), ":");
    const auto typing = answered(vim, formatting_method, clangd_edits);
    expect(message_of(typing) == notice(vim, core::UiMessage::notice_formatting_busy) &&
               body(vim) == source_text,
           "an answer that arrives while the : line is open is not applied");

    Editing palette(lsp_on());
    started(palette, both_providers);
    static_cast<void>(asked(palette, core::LanguageQueryKind::format_document));
    static_cast<void>(palette.controller().apply(app::OpenCommandPalette{}));
    const auto listed = answered(palette, formatting_method, clangd_edits);
    expect(message_of(listed) == notice(palette, core::UiMessage::notice_formatting_busy) &&
               listed.command_palette.has_value(),
           "an answer that arrives while the Ctrl+P list is open is not applied");

    Editing sync(lsp_on());
    started(sync, both_providers);
    static_cast<void>(asked(sync, core::LanguageQueryKind::format_document));
    const std::size_t changes =
        sync.language_server().notifications("textDocument/didChange").size();
    const auto done = answered(sync, formatting_method, clangd_edits);
    expect(sync.language_server().notifications("textDocument/didChange").size() == changes + 1,
           "applying the formatting sends exactly one didChange");
    expect(done.document.save_state == core::SaveState::modified,
           "applying the formatting marks the document unsaved");
}

// Ctrl+Tab の歩きの途中に届いた整形の答えは当てない（入力の途中と同じ・#2011 のレビュー R1）。
void verify_tab_walk_busy()
{
    Editing editing(lsp_on());
    started(editing, both_providers);
    applied(editing.controller(), app::NewTab{});
    applied(editing.controller(), app::SwitchTab{0});
    static_cast<void>(asked(editing, core::LanguageQueryKind::format_document));
    static_cast<void>(editing.controller().apply(app::WalkRecentTab{core::TabStep::next}));
    static_cast<void>(editing.controller().apply(app::WalkRecentTab{core::TabStep::next}));
    const auto frame = answered(editing, formatting_method, clangd_edits);
    expect(message_of(frame) == notice(editing, core::UiMessage::notice_formatting_busy) &&
               frame.active_tab == 0 && body(editing) == source_text,
           "a formatting answer during a Ctrl+Tab walk is not applied");
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
    verify_edges();
    verify_marks_and_other_pane();
    verify_switch_busy_and_sync();
    verify_busy_states();
    verify_recording_applies();
    verify_insert_refusal();
    verify_tab_walk_busy();
}
} // namespace nenenib::tests
