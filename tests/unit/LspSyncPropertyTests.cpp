// scope `--lsp-sync-property` の性質の試験（#1897・ADR 0161 の決定 1 と 2026-11-20 の追補）。
// 固定 seed の決定的な生成器で作った編集の列（打鍵・Backspace と Delete・語の削除・キャレットの
// 移動・CRLF と代理対と結合文字を含む貼り付け・undo / redo・Vim の鍵とマクロと `.`）を替え玉の
// サーバーに繋いだ controller に流し、送った `didOpen` の本文に送った `didChange` の列を順に当てた
// 本文が、controller の今の本文と同じことを確かめる。当てる側の位置の写しは core の lsp_offset_of
// の 1 本（サーバーの側の読み方の模型）。同期の種類は incremental（utf-16 と utf-8）と full。
// 生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。時計と環境変数を読まない。
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DeleteUnit.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "LineNumber.hpp"
#include "LspJsonRead.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
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

constexpr std::string_view source_path = "C:\\work\\sync.cpp";
constexpr std::string_view first_text = "int a;\r\nあ😀b\n\tc\u0301d\n";
constexpr std::size_t steps = 160;
constexpr std::array<std::uint64_t, 4> seeds{11, 29, 47, 83};
// キャレットを置く範囲（行と桁は 1 始まり・本文の外は controller が寄せる）。
constexpr std::uint64_t caret_lines = 6;
constexpr std::uint64_t caret_columns = 8;

// 打つ・貼る本文（CRLF・代理対・結合文字・Tab・複数行）。
constexpr std::array<std::string_view, 10> pieces{"x",      "あ",      "😀", "\r\n",       "\n",
                                                  "ab\ncd", "e\u0301", "\t", "1\r\n2\r\n", "🎉z"};
// Vim の鍵の列（削除・貼り付け・行の操作・undo / redo・`.`・マクロの録画と再生）。
constexpr std::array<std::string_view, 16> vim_moves{
    "x", "dd", "p",        "P",   "u", "<C-r>", ".",  "j",
    "k", "l",  "o😀<Esc>", "yyp", "J", "qaxjq", "@a", "A\r\n<Esc>"};

[[nodiscard]] SettingsReading lsp_settings()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", R"("C:\LLVM\bin\clangd.exe")"}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

[[nodiscard]] std::string whole(const core::TextBuffer &text)
{
    return text.text_range(core::Offset{0}, core::Offset{text.size_bytes()});
}

// 1 つの `TextDocumentContentChangeEvent` を模型の本文に当てる（range が無ければ全文）。
[[nodiscard]] core::TextBuffer changed(const core::TextBuffer &text, const core::JsonObject &change,
                                       core::LspPositionEncoding encoding)
{
    const auto *inserted = core::json_text_member(change, "text");
    const auto *range = core::json_object_member(change, "range");
    if (inserted == nullptr)
    {
        return text;
    }
    if (range == nullptr)
    {
        return core::TextBuffer::from_utf8(*inserted).value_or(text);
    }
    const auto start = core::lsp_position_member(*range, "start");
    const auto end = core::lsp_position_member(*range, "end");
    const auto begin =
        start.has_value() ? core::lsp_offset_of(text, start.value(), encoding) : std::nullopt;
    const auto stop =
        end.has_value() ? core::lsp_offset_of(text, end.value(), encoding) : std::nullopt;
    if (!begin.has_value() || !stop.has_value())
    {
        return text;
    }
    return text.replaced(begin.value(), stop.value(), *inserted);
}

// 届いた 1 通の object の鍵 key の object と、`method` の文字列（無ければ nullptr）。
[[nodiscard]] const std::string *method_of(const core::JsonObject *message)
{
    return message != nullptr ? core::json_text_member(*message, "method") : nullptr;
}

// didOpen の params → 模型の本文（全文）。
[[nodiscard]] core::TextBuffer opened(const core::JsonObject &params)
{
    const auto *item = core::json_object_member(params, "textDocument");
    const auto *text = item != nullptr ? core::json_text_member(*item, "text") : nullptr;
    return core::TextBuffer::from_utf8(text != nullptr ? *text : "").value();
}

// didChange の params の `contentChanges` を順に当てる。
[[nodiscard]] core::TextBuffer changed_all(core::TextBuffer model, const core::JsonObject &params,
                                           core::LspPositionEncoding encoding)
{
    const auto changes = core::json_member(params, "contentChanges");
    const auto *list =
        changes.has_value() ? std::get_if<core::JsonArray>(&changes.value().get().value) : nullptr;
    for (const core::JsonValue &entry : list != nullptr ? *list : core::JsonArray{})
    {
        const auto *change = core::json_object_of(entry);
        model = change != nullptr ? changed(model, *change, encoding) : model;
    }
    return model;
}

// 送った 1 通を模型に当てる（didOpen は全文で置き、didChange は順に当て、ほかは何もしない）。
[[nodiscard]] core::TextBuffer stepped(core::TextBuffer model, const std::string &body,
                                       core::LspPositionEncoding encoding)
{
    const auto value = core::parse_json(body);
    const auto *message = value.has_value() ? core::json_object_of(value.value()) : nullptr;
    const auto *method = method_of(message);
    const auto *params =
        message != nullptr ? core::json_object_member(*message, "params") : nullptr;
    if (method == nullptr || params == nullptr)
    {
        return model;
    }
    if (*method == "textDocument/didOpen")
    {
        return opened(*params);
    }
    return *method == "textDocument/didChange" ? changed_all(std::move(model), *params, encoding)
                                               : model;
}

// 送った列（didOpen で始め、didChange を順に当てる）から、サーバーが持つ本文を作り直す。
[[nodiscard]] std::string replayed(const std::vector<std::string> &sent,
                                   core::LspPositionEncoding encoding)
{
    core::TextBuffer model = core::TextBuffer::empty();
    for (const std::string &body : sent)
    {
        model = stepped(std::move(model), body, encoding);
    }
    return whole(model);
}

// 開いて `initialize` に capabilities で答え、`initialized` まで進める。
void started(Editing &editing, std::string_view capabilities)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string(first_text)});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    editing.language_server().serve(app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    editing.language_server().answer(1, std::string{R"({"capabilities":)"} +
                                            std::string{capabilities} + "}");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// 通常モードの 1 歩（打鍵・削除・移動・貼り付け・undo / redo）。
void normal_step(Editing &editing, std::mt19937_64 &random)
{
    auto &controller = editing.controller();
    const std::string_view piece = pieces.at(random() % pieces.size());
    switch (random() % 8)
    {
    case 0:
    case 1:
        applied(controller, app::InsertText{std::string(piece)});
        return;
    case 2:
        applied(controller,
                app::DeleteText{core::DeleteDirection::backward, core::DeleteUnit::character});
        return;
    case 3:
        applied(controller, app::DeleteText{core::DeleteDirection::forward,
                                            random() % 2 == 0 ? core::DeleteUnit::character
                                                              : core::DeleteUnit::word});
        return;
    case 4:
        applied(controller, app::HistoryAction{core::HistoryDirection::undo});
        return;
    case 5:
        applied(controller, app::HistoryAction{core::HistoryDirection::redo});
        return;
    case 6:
        editing.clipboard().hold(Content{std::string(piece) + std::string(piece)});
        applied(controller, app::ClipboardAction{app::ClipboardOperation::paste});
        return;
    default:
        applied(controller,
                app::PlaceCaret{core::TextPosition{core::LineNumber{1 + (random() % caret_lines)},
                                                   core::Column{1 + (random() % caret_columns)}},
                                core::SelectionAnchoring::collapse});
        return;
    }
}

void verify_normal_runs(std::string_view capabilities, core::LspPositionEncoding encoding,
                        const char *what)
{
    for (const std::uint64_t seed : seeds)
    {
        Editing editing(lsp_settings());
        started(editing, capabilities);
        std::mt19937_64 random(seed);
        for (std::size_t step = 0; step < steps; ++step)
        {
            normal_step(editing, random);
        }
        const std::string sent = replayed(editing.language_server().sent(), encoding);
        expect(sent == whole(editing.controller().text()) && sent != first_text &&
                   editing.language_server().notifications("textDocument/didChange").size() >
                       steps / 4,
               what);
    }
}

// Vim の 1 歩（鍵の列を 1 つ）。
void vim_step(Editing &editing, std::mt19937_64 &random)
{
    for (const auto &key : vim_keys_of(vim_moves.at(random() % vim_moves.size())))
    {
        applied(editing.controller(), app::VimKeyPress{key});
    }
}

void verify_vim_runs(std::string_view capabilities, core::LspPositionEncoding encoding,
                     const char *what)
{
    for (const std::uint64_t seed : seeds)
    {
        Editing editing(lsp_settings());
        applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
        started(editing, capabilities);
        std::mt19937_64 random(seed);
        for (std::size_t step = 0; step < steps; ++step)
        {
            vim_step(editing, random);
        }
        const std::string sent = replayed(editing.language_server().sent(), encoding);
        expect(sent == whole(editing.controller().text()) && sent != first_text &&
                   editing.language_server().notifications("textDocument/didChange").size() >
                       steps / 4,
               what);
    }
}
} // namespace

void verify_lsp_sync_property_scope()
{
    constexpr std::string_view incremental = R"({"textDocumentSync":2})";
    constexpr std::string_view incremental_utf8 =
        R"({"positionEncoding":"utf-8","textDocumentSync":{"openClose":true,"change":2}})";
    constexpr std::string_view full = R"({"textDocumentSync":{"change":1}})";
    verify_normal_runs(incremental, core::LspPositionEncoding::utf16,
                       "incremental utf-16 changes replay to the editor's text (normal mode)");
    verify_normal_runs(incremental_utf8, core::LspPositionEncoding::utf8,
                       "incremental utf-8 changes replay to the editor's text (normal mode)");
    verify_normal_runs(full, core::LspPositionEncoding::utf16,
                       "full changes replay to the editor's text (normal mode)");
    verify_vim_runs(incremental, core::LspPositionEncoding::utf16,
                    "incremental utf-16 changes replay to the editor's text (Vim keys and macros)");
    verify_vim_runs(incremental_utf8, core::LspPositionEncoding::utf8,
                    "incremental utf-8 changes replay to the editor's text (Vim keys and macros)");
}
} // namespace nenenib::tests
