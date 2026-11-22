// scope `--lsp-symbols` の契約（#1955・ADR 0191 の決定 5）。言語サーバーの記号の一覧: Vim の `gO`・
// 操作「記号の一覧」・Ctrl+P の一覧の記号 `%`。core の読みと表、engine の効果、controller の道を
// 替え玉 ScriptedLanguageServer で確かめる。
#include "ActivateCommandChoice.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "LanguageQueryKind.hpp"
#include "LineNumber.hpp"
#include "LspAnswerFailure.hpp"
#include "LspSymbols.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationTexts.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimAction.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimKeyTable.hpp"
#include "VimLanguageQuery.hpp"
#include "VimNoEffect.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <expected>
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
constexpr std::string_view source_text = "struct Box {\n  int width;\n};\nint main() {}\n";
constexpr std::string_view symbols_method = "textDocument/documentSymbol";
// 実物の clangd（Build Tools の LLVM・2026-11-22・D:/NeNeNib/scripts/20261122/lsp-s6 の probe）の
// 階層の答えの形を、この本文の位置に合わせたもの。
constexpr std::string_view hierarchical =
    R"j([{"children":[{"detail":"int","kind":8,"name":"width","range":{"end":{"character":12,"line":1},"start":{"character":2,"line":1}},"selectionRange":{"end":{"character":11,"line":1},"start":{"character":6,"line":1}}}],)j"
    R"j("detail":"struct","kind":5,"name":"Box","range":{"end":{"character":1,"line":2},"start":{"character":0,"line":0}},"selectionRange":{"end":{"character":10,"line":0},"start":{"character":7,"line":0}}},)j"
    R"j({"detail":"int ()","kind":12,"name":"main","range":{"end":{"character":13,"line":3},"start":{"character":0,"line":3}},"selectionRange":{"end":{"character":8,"line":3},"start":{"character":4,"line":3}}}])j";
// 平らな答え（`SymbolInformation[]`）。
constexpr std::string_view flat =
    R"([{"name":"Box","kind":23,"location":{"uri":"file:///C:/work/main.cpp","range":{"start":{"line":0,"character":7},"end":{"line":0,"character":10}}}},)"
    R"({"name":"width","kind":8,"containerName":"Box","location":{"uri":"file:///C:/work/main.cpp","range":{"start":{"line":1,"character":6},"end":{"line":1,"character":11}}}}])";

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
    static_cast<void>(editing.controller().apply(app::VisibleLines{6}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                           std::string{capabilities} + "}}"}));
}

void listing(Editing &editing)
{
    started(editing, R"({"textDocumentSync":2,"documentSymbolProvider":true})");
}

app::EditorFrame asked(Editing &editing)
{
    return editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::symbols});
}

app::EditorFrame answered(Editing &editing, std::string_view result)
{
    const auto ids = editing.language_server().requests(symbols_method);
    editing.language_server().answer(ids.empty() ? -1 : ids.back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] std::string notice(Editing &editing, core::UiMessage message)
{
    return std::string(core::ui_text(message, editing.controller().display_language()));
}

[[nodiscard]] std::vector<core::LspSymbol> read(std::string_view json)
{
    const auto parsed = core::parse_json(json);
    if (!parsed.has_value())
    {
        return {};
    }
    return core::lsp_document_symbols(parsed.value()).value_or(std::vector<core::LspSymbol>{});
}

// ---------------------------------------------------------------- core の表と読み

void verify_tables()
{
    expect(core::vim_g_action_for(U'O') == core::VimAction::language_symbols,
           "gO is a row of the g table");
    const auto query = core::palette_query_of("%main");
    expect(query.scope == core::PaletteScope::symbols && query.query == "main" &&
               core::palette_input_for(core::PaletteQuery{core::PaletteScope::symbols, ""}) == "%",
           "% opens the symbols source of the Ctrl+P list");
    expect(std::ranges::none_of(core::operation_bindings,
                                [](const core::OperationBinding &binding)
                                {
                                    return binding.operation ==
                                               core::EditorOperation::list_symbols &&
                                           binding.chord.has_value();
                                }),
           "the symbol list has no key (F1 list and gO)");
    const std::vector<core::CommandChoice> entries{
        core::CommandChoice{core::DisplayText::parse("function main").value(),
                            "0",
                            core::CommandChoiceKind::land,
                            std::nullopt,
                            core::PaletteOrigin::symbol,
                            std::nullopt,
                            {}}};
    expect(core::listed_positions(entries, core::PaletteScope::files, "").empty() &&
               core::listed_positions(entries, core::PaletteScope::symbols, "") ==
                   std::vector<std::size_t>{0} &&
               core::listed_positions(entries, core::PaletteScope::references, "").empty(),
           "symbols are not file candidates and show only under %");
}

// 階層の答えは親の直後に子を親の名前つきで平らにし、跳ぶ先は selectionRange。平らな答えは
// containerName を親にする。
void verify_reading()
{
    const auto tree = read(hierarchical);
    expect(tree.size() == 3 && tree.at(0).name == "Box" && tree.at(0).parent.empty() &&
               tree.at(1).name == "width" && tree.at(1).parent == "Box" &&
               tree.at(1).at.start.character == 6 && tree.at(2).name == "main" &&
               tree.at(2).kind == 12 && tree.at(2).at.start.line == 3,
           "a hierarchical answer is flattened in document order with parent names");
    const auto listed = read(flat);
    expect(listed.size() == 2 && listed.at(0).kind == 23 && listed.at(1).parent == "Box" &&
               listed.at(1).at.start.line == 1,
           "a flat answer reads the location and the container name");
    const auto broken = read(
        R"([{"kind":12},{"name":"x"},{"name":"y","kind":13,"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}}])");
    expect(broken.size() == 1 && broken.front().name == "y",
           "a symbol without a name or a place is left out (a range alone is the place)");
    const auto null = core::parse_json("null");
    const auto object = core::parse_json("{}");
    expect(null.has_value() && core::lsp_document_symbols(null.value()).has_value() &&
               core::lsp_document_symbols(null.value()).value().empty() && object.has_value() &&
               core::lsp_document_symbols(object.value()).error() ==
                   core::LspAnswerFailure::invalid_result,
           "null is no symbols and another shape fails");
    std::string many = "[";
    for (std::size_t index = 0; index < core::lsp_symbol_limit + 5; ++index)
    {
        many +=
            std::string{index == 0 ? "" : ","} +
            R"({"name":"s","kind":13,"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})";
    }
    many += "]";
    expect(read(many).size() == core::lsp_symbol_limit, "the symbols stop at the limit");
    expect(core::lsp_symbol_kind_mark(12) == "function" &&
               core::lsp_symbol_kind_mark(23) == "struct" &&
               core::lsp_symbol_kind_mark(0) == "symbol" &&
               core::lsp_symbol_kind_mark(27) == "symbol",
           "the kind mark is the LSP kind name and unknown kinds are symbol");
}

// engine の `gO` は表に行が無かったときと同じ失敗の歩に、記号の問いの効果だけを添える。
void verify_engine_effects()
{
    const auto buffer = core::TextBuffer::from_utf8("ab\ncd").value();
    const core::VimEditorView view{buffer, core::collapsed_at(Offset{0}),
                                   core::VimViewport{core::LineNumber{1}, 8, 0}};
    const auto queried = [&](std::string_view text)
    {
        core::VimState state = empty_vim_state();
        core::VimStep step{state, core::VimNoEffect{}};
        for (const char key : text)
        {
            step = core::vim_step(state, view, core::VimCharacter{static_cast<char32_t>(key)});
            state = step.next;
        }
        const auto *found = std::get_if<core::VimLanguageQuery>(&step.effect);
        return found != nullptr && found->kind == core::LanguageQueryKind::symbols;
    };
    expect(queried("gO"), "gO asks for the symbols");
    expect(!queried("dgO") && !queried("vgO") && !queried("2gO"),
           "an operator, VISUAL and a count add nothing");
}

// ---------------------------------------------------------------- controller の道

// 頼むのは文書だけ（位置なし）。答えは `%` の一覧になり、題名は「種類の印 名前」、補足は親。選ぶと
// 跳ぶ。
void verify_list_and_landing()
{
    Editing editing(lsp_on());
    listing(editing);
    static_cast<void>(asked(editing));
    const auto &sent = editing.language_server().sent();
    expect(contains(sent.back(), R"("method":"textDocument/documentSymbol")") &&
               contains(sent.back(),
                        R"("params":{"textDocument":{"uri":"file:///C:/work/main.cpp"}})"),
           "the request names only the document");
    const auto listed = answered(editing, hierarchical);
    if (!listed.command_palette.has_value() || listed.command_palette.value().rows.size() < 3)
    {
        expect(false, "the answer opens the % list with every symbol");
        return;
    }
    const auto &rows = listed.command_palette.value().rows;
    expect(rows.at(0).label.text() == "class Box" && !rows.at(0).detail.has_value() &&
               rows.at(1).label.text() == "field width" &&
               rows.at(1).detail.value_or(core::DisplayText::parse("-").value()).text() == "Box" &&
               rows.at(2).label.text() == "function main",
           "rows are the kind mark and the name with the parent as the note");
    const auto landed = editing.controller().apply_frame(app::ActivateCommandChoice{1});
    expect(!landed.command_palette.has_value() && caret_on(landed, 2, 7),
           "choosing a symbol lands on its name");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "''");
    expect(caret_on(editing.controller().frame(), 1, 1), "'' goes back to before the jump");
}

// 能力の無いサーバー・空の答え・待っている間の答え。
void verify_absent_and_stale()
{
    Editing plain(lsp_on());
    started(plain, R"({"textDocumentSync":2})");
    const std::size_t before = plain.language_server().sent().size();
    const auto unsupported = asked(plain);
    expect(plain.language_server().sent().size() == before &&
               message_of(unsupported) ==
                   notice(plain, core::UiMessage::notice_symbols_not_supported),
           "a server without documentSymbolProvider is not asked and says so");

    Editing editing(lsp_on());
    listing(editing);
    static_cast<void>(asked(editing));
    const auto empty = answered(editing, "[]");
    expect(!empty.command_palette.has_value() &&
               message_of(empty) == notice(editing, core::UiMessage::notice_symbols_not_found),
           "an empty answer opens nothing and says so");
    static_cast<void>(asked(editing));
    applied(editing.controller(), app::InsertText{"x"});
    expect(!answered(editing, flat).command_palette.has_value(),
           "an answer for an older revision is dropped");
}

// Vim の `gO` も同じ問いで、頼めたら成功の歩（マクロの続きが走る）。
void verify_vim_keys()
{
    Editing editing(lsp_on());
    listing(editing);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    const std::size_t before = editing.language_server().requests(symbols_method).size();
    vim_replay(editing.controller(), "qagOjqgg@a");
    expect(editing.language_server().requests(symbols_method).size() == before + 2 &&
               whole_vim_body(editing.controller()) == source_text &&
               caret_on(editing.controller().frame(), 2, 1),
           "gO asks for the symbols, leaves the text and lets the macro go on");
}
} // namespace

void verify_lsp_symbols_scope()
{
    verify_tables();
    verify_reading();
    verify_engine_effects();
    verify_list_and_landing();
    verify_absent_and_stale();
    verify_vim_keys();
}
} // namespace nenenib::tests
