// scope `--lsp-sync-property` の性質の試験（#1897・ADR 0161 の決定 1 と 2026-11-20 の追補）。
// 固定 seed の決定的な生成器で作った編集の列（打鍵・Backspace と Delete・語の削除・キャレットの
// 移動・CRLF と代理対と結合文字を含む貼り付け・undo / redo・Vim の鍵とマクロと `.`）を替え玉の
// サーバーに繋いだ controller に流し、送った `didOpen` の本文に送った `didChange` の列を順に当てた
// 本文が、controller の今の本文と同じこと・版が狭義に増えることを確かめる。当てる側（サーバーの
// 模型）は製品の core の写しを使わない素朴な別実装（#1907 のレビュー R1）。同期の種類は
// incremental（utf-16 と utf-8）と full。
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

#include <algorithm>
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

// サーバーの側の模型（#1907 のレビュー R1）。製品の core の写し（lsp_offset_of・TextBuffer の行の
// 索引・Utf16）を使わず、本文を std::string のまま持ち、行は `\n` で割り（内容は `\r\n` の `\r` を
// 含まない）、桁は UTF-8 の先頭バイトから文字の長さを読んで素朴に数える。公開 aggregate。
struct ServerModel
{
    std::string text;
    std::optional<std::uint64_t> version;
    bool versions_rose = true;
};

// UTF-8 の先頭バイト → 文字のバイト数（正しい UTF-8 だけが届く前提）。
[[nodiscard]] std::size_t utf8_length(unsigned char lead) noexcept
{
    constexpr unsigned char two = 0xC0;
    constexpr unsigned char three = 0xE0;
    constexpr unsigned char four = 0xF0;
    return lead < two ? 1 : lead < three ? 2 : lead < four ? 3 : 4;
}

// 行 line の内容の [begin, end)（行が本文の外なら本文の末尾の空の区間）。
[[nodiscard]] std::pair<std::size_t, std::size_t> line_span(std::string_view text, std::size_t line)
{
    std::size_t begin = 0;
    for (std::size_t passed = 0; passed < line; ++passed)
    {
        const std::size_t newline = text.find('\n', begin);
        if (newline == std::string_view::npos)
        {
            return {text.size(), text.size()};
        }
        begin = newline + 1;
    }
    std::size_t end = std::min(text.find('\n', begin), text.size());
    end = end > begin && text[end - 1] == '\r' ? end - 1 : end;
    return {begin, end};
}

// Position → バイト位置（桁が行の内容の外なら行末・LSP の既定）。
[[nodiscard]] std::size_t model_offset(std::string_view text, std::size_t line,
                                       std::size_t character, core::LspPositionEncoding encoding)
{
    const auto [begin, end] = line_span(text, line);
    std::size_t at = begin;
    std::size_t units = 0;
    constexpr std::size_t pair_bytes = 4;
    while (at < end && units < character)
    {
        const std::size_t length = utf8_length(static_cast<unsigned char>(text[at]));
        units += encoding == core::LspPositionEncoding::utf8 ? length
                 : length == pair_bytes                      ? 2
                                                             : 1;
        at += length;
    }
    return std::min(at, end);
}

// range の端 key の Position（line と character）をバイト位置へ。
[[nodiscard]] std::optional<std::size_t> model_end(std::string_view text,
                                                   const core::JsonObject &range,
                                                   std::string_view key,
                                                   core::LspPositionEncoding encoding)
{
    const auto *position = core::json_object_member(range, key);
    const auto line =
        position != nullptr ? core::json_count_member(*position, "line") : std::nullopt;
    const auto character =
        position != nullptr ? core::json_count_member(*position, "character") : std::nullopt;
    if (!line.has_value() || !character.has_value())
    {
        return std::nullopt;
    }
    return model_offset(text, line.value(), character.value(), encoding);
}

// 1 つの `TextDocumentContentChangeEvent` を模型の本文に当てる（range が無ければ全文）。
void changed(std::string &text, const core::JsonObject &change, core::LspPositionEncoding encoding)
{
    const auto *inserted = core::json_text_member(change, "text");
    const auto *range = core::json_object_member(change, "range");
    if (inserted == nullptr)
    {
        return;
    }
    if (range == nullptr)
    {
        text = *inserted;
        return;
    }
    const auto begin = model_end(text, *range, "start", encoding);
    const auto end = model_end(text, *range, "end", encoding);
    if (begin.has_value() && end.has_value() && begin.value() <= end.value())
    {
        text.replace(begin.value(), end.value() - begin.value(), *inserted);
    }
}

[[nodiscard]] const std::string *method_of(const core::JsonObject *message)
{
    return message != nullptr ? core::json_text_member(*message, "method") : nullptr;
}

// params の `textDocument` の `version`（無ければ値なし）。
[[nodiscard]] std::optional<std::uint64_t> version_of(const core::JsonObject &params)
{
    const auto *item = core::json_object_member(params, "textDocument");
    return item != nullptr ? core::json_count_member(*item, "version") : std::nullopt;
}

// didOpen の params → 模型の本文と版。
void opened(ServerModel &model, const core::JsonObject &params)
{
    const auto *item = core::json_object_member(params, "textDocument");
    const auto *text = item != nullptr ? core::json_text_member(*item, "text") : nullptr;
    model.text = text != nullptr ? *text : "";
    model.version = version_of(params);
}

// didChange の params: 版が狭義に増えることを見て、`contentChanges` を順に当てる。
void changed_all(ServerModel &model, const core::JsonObject &params,
                 core::LspPositionEncoding encoding)
{
    const auto version = version_of(params);
    model.versions_rose = model.versions_rose && version.has_value() && model.version.has_value() &&
                          version.value_or(0) > model.version.value_or(0);
    model.version = version;
    const auto changes = core::json_member(params, "contentChanges");
    const auto *list =
        changes.has_value() ? std::get_if<core::JsonArray>(&changes.value().get().value) : nullptr;
    for (const core::JsonValue &entry : list != nullptr ? *list : core::JsonArray{})
    {
        if (const auto *change = core::json_object_of(entry))
        {
            changed(model.text, *change, encoding);
        }
    }
}

// 送った 1 通を模型に当てる（didOpen は全文で置き、didChange は順に当て、ほかは何もしない）。
void stepped(ServerModel &model, const std::string &body, core::LspPositionEncoding encoding)
{
    const auto value = core::parse_json(body);
    const auto *message = value.has_value() ? core::json_object_of(value.value()) : nullptr;
    const auto *method = method_of(message);
    const auto *params =
        message != nullptr ? core::json_object_member(*message, "params") : nullptr;
    if (method == nullptr || params == nullptr)
    {
        return;
    }
    if (*method == "textDocument/didOpen")
    {
        opened(model, *params);
    }
    else if (*method == "textDocument/didChange")
    {
        changed_all(model, *params, encoding);
    }
}

// 送った列（didOpen で始め、didChange を順に当てる）から、サーバーが持つ本文と版を作り直す。
[[nodiscard]] ServerModel replayed(const std::vector<std::string> &sent,
                                   core::LspPositionEncoding encoding)
{
    ServerModel model;
    for (const std::string &body : sent)
    {
        stepped(model, body, encoding);
    }
    return model;
}

// 模型の本文が controller の本文と同じで、版が増え続け、実際に変更が送られたか。
[[nodiscard]] bool replays_to_text(Editing &editing, core::LspPositionEncoding encoding)
{
    const ServerModel model = replayed(editing.language_server().sent(), encoding);
    return model.text == whole(editing.controller().text()) && model.versions_rose &&
           model.text != first_text &&
           editing.language_server().notifications("textDocument/didChange").size() > steps / 4;
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
        expect(replays_to_text(editing, encoding), what);
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
        expect(replays_to_text(editing, encoding), what);
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
