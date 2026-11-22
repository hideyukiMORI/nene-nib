// scope `--lsp-symbols` の契約（#1955・ADR 0191 の決定 5）。言語サーバーの記号の一覧: Vim の `gO`・
// 操作「記号の一覧」・Ctrl+P の一覧の記号 `%`。core の読みと表、engine の効果、controller の道を
// 替え玉 ScriptedLanguageServer で確かめる。
#include "ActivateCommandChoice.hpp"
#include "CancelCommand.hpp"
#include "CloseTab.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "DisplayText.hpp"
#include "EditCommand.hpp"
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
#include "LspSymbolList.hpp"
#include "LspSymbols.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
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
#include "WindowDeactivated.hpp"
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
    return core::lsp_document_symbols(parsed.value())
        .value_or(core::LspSymbolList{.symbols = {}, .total = 0})
        .symbols;
}

[[nodiscard]] std::size_t read_total(std::string_view json)
{
    const auto parsed = core::parse_json(json);
    return parsed.has_value() ? core::lsp_document_symbols(parsed.value())
                                    .value_or(core::LspSymbolList{.symbols = {}, .total = 0})
                                    .total
                              : 0;
}

// 名前と位置だけの記号の JSON（子は children の JSON・空なら無し）。
[[nodiscard]] std::string symbol_json(std::string_view name, std::string_view children)
{
    return std::string{R"({"name":")"} + std::string{name} +
           R"(","kind":13,"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}})" +
           (children.empty() ? std::string{} : ",\"children\":" + std::string{children}) + "}";
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
               core::lsp_document_symbols(null.value()).value().symbols.empty() &&
               object.has_value() &&
               core::lsp_document_symbols(object.value()).error() ==
                   core::LspAnswerFailure::invalid_result,
           "null is no symbols and another shape fails");
    expect(core::lsp_symbol_kind_mark(12) == "function" &&
               core::lsp_symbol_kind_mark(23) == "struct" &&
               core::lsp_symbol_kind_mark(0) == "symbol" &&
               core::lsp_symbol_kind_mark(27) == "symbol",
           "the kind mark is the LSP kind name and unknown kinds are symbol");
}

// 上限で切る・階層の途中で切る・containerName が勝つ（#1958 のレビュー R3・L1・L5）。
void verify_limits()
{
    std::string many = "[";
    for (std::size_t index = 0; index < core::lsp_symbol_limit + 5; ++index)
    {
        many +=
            std::string{index == 0 ? "" : ","} +
            R"({"name":"s","kind":13,"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})";
    }
    many += "]";
    expect(read(many).size() == core::lsp_symbol_limit &&
               read_total(many) == core::lsp_symbol_limit + 5,
           "the symbols stop at the limit and count the rest");
    // 階層の途中で切る（#1958 のレビュー R3・L5）: 親 a の子 c0.. が上限を越える。親の直後に子が
    // 文書の順に並び、上限の内側の子は親の名前を持ち、上限の後の兄弟 b は読まずに数える。
    std::string children = "[";
    for (std::size_t index = 0; index < core::lsp_symbol_limit + 2; ++index)
    {
        children +=
            std::string{index == 0 ? "" : ","} + symbol_json("c" + std::to_string(index), "");
    }
    children += "]";
    const std::string nested = "[" + symbol_json("a", children) + "," + symbol_json("b", "") + "]";
    const auto cut = read(nested);
    expect(cut.size() == core::lsp_symbol_limit && cut.front().name == "a" &&
               cut.at(1).name == "c0" && cut.at(1).parent == "a" &&
               cut.back().name == "c" + std::to_string(core::lsp_symbol_limit - 2) &&
               cut.back().parent == "a" && read_total(nested) == core::lsp_symbol_limit + 4,
           "a hierarchy cut at the limit keeps the document order and the parent names");
    // 平らな答えで containerName と階層の親が両方あるときは containerName が勝つ。
    const auto both = read(
        R"([{"name":"Outer","kind":5,"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"children":[)"
        R"({"name":"inner","kind":12,"containerName":"Named","range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}}]}])");
    expect(both.size() == 2 && both.at(1).parent == "Named",
           "containerName wins over the hierarchical parent");
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

// 手で `%` を打った一覧（開いていなければ値なし）。
[[nodiscard]] std::optional<app::CommandPaletteView> typed_percent(Editing &editing)
{
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    return editing.controller().apply_frame(app::CommandText{"%"}).command_palette;
}

// 窓が非アクティブの間に届いた答えは一覧を開かずに控え、答えを頼んだ文書を閉じたら捨てる
// （#1958 のレビュー R3・参照と同じ）。
void verify_inactive_and_closed()
{
    Editing editing(lsp_on());
    listing(editing);
    static_cast<void>(asked(editing));
    static_cast<void>(editing.controller().apply(app::WindowDeactivated{}));
    expect(!answered(editing, hierarchical).command_palette.has_value(),
           "an answer that arrives while the window is inactive opens nothing");
    const auto held = typed_percent(editing);
    expect(held.has_value() && held.value().total == 3, "the held answer shows under %");
    applied(editing.controller(), app::CancelCommand{});
    applied(editing.controller(), app::NewTab{});
    applied(editing.controller(), app::CloseTab{0});
    expect(!editing.controller().language_answers().symbols.has_value(),
           "no answer is left in the state");
    const auto forgotten = typed_percent(editing);
    expect(forgotten.has_value() && forgotten.value().total == 0,
           "closing the asking document forgets its symbols");
    applied(editing.controller(), app::CancelCommand{});
}

// 誤りの答え（`error` だけ）は「記号が見つかりません」、UTF-8
// の席の記号も同じ名前に着地する、切った 答えは 1 行知らせる（#1958 のレビュー R3・L1）。
void verify_error_bytes_and_cut()
{
    Editing failing(lsp_on());
    listing(failing);
    static_cast<void>(asked(failing));
    const auto ids = failing.language_server().requests(symbols_method);
    const auto failed = delivered(
        failing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" +
                                    std::to_string(ids.empty() ? -1 : ids.back()) +
                                    R"(,"error":{"code":-32801,"message":"content modified"}})"});
    expect(!failed.command_palette.has_value() &&
               message_of(failed) == notice(failing, core::UiMessage::notice_symbols_not_found),
           "an error answer is no symbols");

    constexpr std::string_view wide = "/*\xE3\x81\x82*/ int box;\n";
    Editing bytes(lsp_on());
    bytes.files().hold_at(std::string(source_path), Bytes{std::string(wide)});
    static_cast<void>(bytes.controller().apply(app::VisibleLines{6}));
    applied(bytes.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(bytes, app::ServerStarted{}));
    static_cast<void>(delivered(
        bytes,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","textDocumentSync":2,"documentSymbolProvider":true}}})"}));
    static_cast<void>(asked(bytes));
    static_cast<void>(answered(
        bytes,
        R"([{"name":"box","kind":13,"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":16}},"selectionRange":{"start":{"line":0,"character":12},"end":{"line":0,"character":15}}}])"));
    expect(caret_on(bytes.controller().apply_frame(app::ActivateCommandChoice{0}), 1, 11),
           "a server that counts UTF-8 bytes lands on the name");

    Editing many(lsp_on());
    listing(many);
    static_cast<void>(asked(many));
    std::string answer = "[";
    for (std::size_t index = 0; index < core::lsp_symbol_limit + 1; ++index)
    {
        answer += std::string{index == 0 ? "" : ","} + symbol_json("s", "");
    }
    answer += "]";
    const auto cut = answered(many, answer);
    expect(cut.command_palette.has_value() &&
               cut.command_palette.value().total == core::lsp_symbol_limit &&
               message_of(cut) == core::ui_text_filled(core::UiMessage::notice_symbols_first_of,
                                                       many.controller().display_language(),
                                                       {"1000", "1001"}),
           "a cut answer says how many symbols it had");
}

// `%` は今の文書の今の改訂の記号だけ（#1958 の決め 4・レビュー R2）。別のタブの答えも、頼んだ後に
// 変わった文書の答えも見せず、今の文書について頼み直す。届いた答えは開いている `%` の一覧に出る。
void verify_percent_is_the_current_document()
{
    Editing editing(lsp_on());
    editing.files().hold_at("C:\\work\\two.cpp", Bytes{std::string{"int two;\n"}});
    listing(editing);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, hierarchical));
    applied(editing.controller(), app::CancelCommand{});
    applied(editing.controller(),
            app::OpenDocument{core::FilePath::parse("C:\\work\\two.cpp").value()});
    const std::size_t before = editing.language_server().requests(symbols_method).size();
    const auto other = typed_percent(editing);
    expect(other.has_value() && other.value().total == 0 &&
               editing.language_server().requests(symbols_method).size() == before + 1,
           "% on another tab shows none of the held symbols and asks again for this document");
    const auto arrived = answered(
        editing,
        R"([{"name":"two","kind":13,"range":{"start":{"line":0,"character":4},"end":{"line":0,"character":7}}}])");
    expect(arrived.command_palette.has_value() && arrived.command_palette.value().total == 1 &&
               arrived.command_palette.value().rows.front().label.text() == "variable two",
           "the answer for this document fills the open % list");
    applied(editing.controller(), app::CancelCommand{});
    applied(editing.controller(), app::InsertText{"x"});
    const std::size_t edited = editing.language_server().requests(symbols_method).size();
    const auto stale = typed_percent(editing);
    expect(stale.has_value() && stale.value().total == 0 &&
               editing.language_server().requests(symbols_method).size() == edited + 1,
           "% after an edit asks again instead of showing the older symbols");
    applied(editing.controller(), app::CancelCommand{});
}

// `%` で頼み直している間に `%` を消して打ち直しても、届いた答え（照合は通っている）を控えておき、
// 頼み直さずに見せる（#1958 の確かめレビュー R1）。同じ一覧の中で `%` → `#` → `%`
// と戻っても当たりを 作り直さない（L4）。
void verify_percent_retyped_while_asking()
{
    Editing editing(lsp_on());
    listing(editing);
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    static_cast<void>(editing.controller().apply(app::CommandText{"%"}));
    const std::size_t asked = editing.language_server().requests(symbols_method).size();
    static_cast<void>(editing.controller().apply(app::EditCommand{core::CommandEdit::backspace}));
    static_cast<void>(answered(editing, hierarchical));
    const auto retyped = editing.controller().apply_frame(app::CommandText{"%"});
    expect(retyped.command_palette.has_value() && retyped.command_palette.value().total == 3 &&
               editing.language_server().requests(symbols_method).size() == asked,
           "an answer that arrives after % was erased shows when % is typed again");
    const std::size_t made = editing.controller().listed_hits_made();
    static_cast<void>(editing.controller().apply(app::EditCommand{core::CommandEdit::backspace}));
    static_cast<void>(editing.controller().apply(app::CommandText{"#"}));
    static_cast<void>(editing.controller().apply(app::EditCommand{core::CommandEdit::backspace}));
    static_cast<void>(editing.controller().apply(app::CommandText{"%"}));
    expect(editing.controller().listed_hits_made() == made,
           "% then # then % in one list makes the hits once");
    applied(editing.controller(), app::CancelCommand{});
}

// LSP がオフの `^` `&` `%` は一覧の中に「LSP はオフです」の 1 行。案内に `^` は出ない（#1958 の
// 決め 1・2）。頼んでいない `%` は頼み方の 1 行。
void verify_empty_lines()
{
    Editing off(
        SettingsReading{std::optional<core::EditorSettings>{core::default_editor_settings()}});
    for (const std::string_view mark : {"^", "&", "%"})
    {
        static_cast<void>(off.controller().apply(app::OpenCommandPalette{}));
        const auto frame = off.controller().apply_frame(app::CommandText{std::string{mark}});
        const auto line = frame.command_palette.has_value()
                              ? frame.command_palette.value().empty_line
                              : std::nullopt;
        expect(line.has_value() &&
                   line.value().text() ==
                       notice(off, core::UiMessage::palette_empty_language_server_off),
               "a language server source with LSP off says LSP is off");
        applied(off.controller(), app::CancelCommand{});
    }
    static_cast<void>(off.controller().apply(app::OpenCommandPalette{}));
    const auto hint = off.controller().frame().command_palette;
    expect(hint.has_value() && hint.value().hint.has_value() &&
               !contains(hint.value().hint.value().text(), "^ ") &&
               hint.value().hint_starts.size() == 7,
           "the hint with LSP off has seven groups and no diagnostics");
    applied(off.controller(), app::CancelCommand{});

    Editing able(lsp_on());
    listing(able);
    const auto unasked = typed_percent(able);
    const auto line = unasked.has_value() ? unasked.value().empty_line : std::nullopt;
    expect(
        line.has_value() &&
            line.value().text() ==
                core::ui_text_filled(
                    core::UiMessage::palette_empty_symbols, able.controller().display_language(),
                    {"gO", "F1",
                     core::ui_text(core::operation_text(core::EditorOperation::list_symbols).name,
                                   able.controller().display_language())}),
        "% before the answer says how to ask, with the keys from the tables");
    applied(able.controller(), app::CancelCommand{});
}

// LSP がオンの一覧の中の空の 1 行: 能力の無い席の `%` `&` は「答えません」、診断の無い `^` は
// 「診断はありません」（#1958 の確かめレビュー L2・L5）。
void verify_empty_lines_with_lsp_on()
{
    Editing plain(lsp_on());
    started(plain, R"({"textDocumentSync":2})");
    const auto line_of = [&plain](std::string_view mark)
    {
        static_cast<void>(plain.controller().apply(app::OpenCommandPalette{}));
        const auto frame = plain.controller().apply_frame(app::CommandText{std::string{mark}});
        applied(plain.controller(), app::CancelCommand{});
        const auto empty = frame.command_palette.has_value()
                               ? frame.command_palette.value().empty_line
                               : std::nullopt;
        return empty.has_value() ? std::string(empty.value().text()) : std::string{};
    };
    expect(line_of("%") == notice(plain, core::UiMessage::notice_symbols_not_supported) &&
               line_of("&") == notice(plain, core::UiMessage::notice_references_not_supported),
           "% and & on a server without the ability say it does not answer");
    expect(line_of("^") == notice(plain, core::UiMessage::notice_no_diagnostics),
           "^ without diagnostics says there are none");
}
} // namespace

void verify_lsp_symbols_scope()
{
    verify_tables();
    verify_reading();
    verify_limits();
    verify_engine_effects();
    verify_list_and_landing();
    verify_absent_and_stale();
    verify_vim_keys();
    verify_inactive_and_closed();
    verify_error_bytes_and_cut();
    verify_percent_is_the_current_document();
    verify_percent_retyped_while_asking();
    verify_empty_lines();
    verify_empty_lines_with_lsp_on();
}
} // namespace nenenib::tests
