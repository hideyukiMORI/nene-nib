// scope `--lsp-messages` の契約（#994・ADR 0160 の決定 5・ADR 0042）。core の JSON-RPC の対応
// （lsp_incoming と本文を作る 3 本）と、LSP の Position ↔ 本文のバイト位置の写し（utf-8 /
// utf-16）を 当てる。位置は ASCII・日本語・絵文字（代理対）・CRLF
// の文書・空行・行の外で往復を見る。
#include "JsonNull.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "LspError.hpp"
#include "LspId.hpp"
#include "LspIncoming.hpp"
#include "LspMessageFailure.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "LspRequest.hpp"
#include "LspResponse.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
using core::JsonNull;
using core::JsonObject;
using core::JsonValue;
using core::LspId;
using core::LspIncoming;
using core::LspMessageFailure;
using core::LspPosition;
using core::LspPositionEncoding;
using core::Offset;
using Incoming = std::expected<LspIncoming, LspMessageFailure>;
using Refused = std::pair<std::string_view, LspMessageFailure>;
// 本文・Position・符号化と、写したバイト位置（値なしは本文の外）。
using Placed = std::pair<std::pair<std::string_view, LspPosition>,
                         std::pair<LspPositionEncoding, std::optional<std::size_t>>>;

constexpr auto utf8 = LspPositionEncoding::utf8;
constexpr auto utf16 = LspPositionEncoding::utf16;

// 断る 1 通と閉じた失敗（LspMessageFailure.hpp の一覧のどれにも 1 つ以上）。
constexpr std::array<Refused, 10> refused{{
    {R"([{"jsonrpc":"2.0","id":1,"result":null}])", LspMessageFailure::not_object},
    {R"("text")", LspMessageFailure::not_object},
    {R"({"id":1,"result":null})", LspMessageFailure::not_json_rpc},
    {R"({"jsonrpc":"1.0","id":1,"result":null})", LspMessageFailure::not_json_rpc},
    {R"({"jsonrpc":"2.0","id":1,"method":7})", LspMessageFailure::invalid_method},
    {R"({"jsonrpc":"2.0","id":null,"result":1})", LspMessageFailure::invalid_id},
    {R"({"jsonrpc":"2.0","id":1.5,"result":1})", LspMessageFailure::invalid_id},
    {R"({"jsonrpc":"2.0","result":1})", LspMessageFailure::missing_id},
    {R"({"jsonrpc":"2.0","id":3,"error":{"message":"x"}})", LspMessageFailure::invalid_error},
    {R"({"jsonrpc":"2.0","id":3,"error":"bad"})", LspMessageFailure::invalid_error},
}};

// 写しの表。日本語は 1 文字が UTF-8 で 3 バイト・UTF-16 で 1 単位、絵文字は 4 バイト・2 単位。
constexpr std::array<Placed, 26> placed{{
    {{"abc\ndef", {1, 2}}, {utf8, 6}},    {{"abc\ndef", {1, 2}}, {utf16, 6}},
    {{"abc\ndef", {0, 99}}, {utf16, 3}},  {{"abc\ndef", {2, 0}}, {utf16, std::nullopt}},
    {{"日本語\nx", {0, 1}}, {utf16, 3}},  {{"日本語\nx", {0, 3}}, {utf8, 3}},
    {{"日本語\nx", {0, 4}}, {utf8, 6}},   {{"日本語\nx", {0, 10}}, {utf16, 9}},
    {{"日本語\nx", {1, 1}}, {utf16, 11}}, {{"a😀b", {0, 1}}, {utf16, 1}},
    {{"a😀b", {0, 2}}, {utf16, 5}},       {{"a😀b", {0, 3}}, {utf16, 5}},
    {{"a😀b", {0, 4}}, {utf16, 6}},       {{"a😀b", {0, 2}}, {utf8, 5}},
    {{"a😀b", {0, 5}}, {utf8, 5}},        {{"ab\r\ncd", {0, 5}}, {utf16, 2}},
    {{"ab\r\ncd", {1, 0}}, {utf16, 4}},   {{"ab\r\ncd", {1, 2}}, {utf8, 6}},
    {{"\n\n", {1, 0}}, {utf16, 1}},       {{"\n\n", {2, 0}}, {utf16, 2}},
    {{"\n\n", {2, 5}}, {utf8, 2}},        {{"\n\n", {3, 0}}, {utf16, std::nullopt}},
    {{"", {0, 0}}, {utf16, 0}},           {{"", {0, 3}}, {utf8, 0}},
    {{"", {1, 0}}, {utf8, std::nullopt}}, {{"x\n", {1, 0}}, {utf16, 2}},
}};

[[nodiscard]] Incoming incoming_of(std::string_view body)
{
    const auto parsed = core::parse_json(body);
    expect(parsed.has_value(), "the message body is valid JSON");
    return core::lsp_incoming(parsed.value_or(JsonValue{JsonNull{}}));
}

void verify_responses()
{
    const Incoming done = incoming_of(R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{}}})");
    const auto *answer = done.has_value() ? std::get_if<core::LspResponse>(&done.value()) : nullptr;
    expect(answer != nullptr && answer->id == LspId{std::int64_t{1}}, "a response keeps its id");
    expect(answer != nullptr && answer->result.has_value() &&
               std::holds_alternative<JsonObject>(answer->result.value().value),
           "a response keeps its result");
    const Incoming named = incoming_of(R"({"jsonrpc":"2.0","id":"a-7","result":null})");
    const auto *by_name =
        named.has_value() ? std::get_if<core::LspResponse>(&named.value()) : nullptr;
    expect(by_name != nullptr && by_name->id == LspId{std::string{"a-7"}},
           "a response with a string id keeps it");
    const Incoming bare = incoming_of(R"({"jsonrpc":"2.0","id":2})");
    const auto *empty = bare.has_value() ? std::get_if<core::LspResponse>(&bare.value()) : nullptr;
    expect(empty != nullptr && empty->result.has_value() &&
               empty->result.value() == JsonValue{JsonNull{}},
           "a response without result reads as a null result");
    const Incoming failed =
        incoming_of(R"({"jsonrpc":"2.0","id":3,"error":{"code":-32600,"message":"bad","data":1}})");
    const auto *error =
        failed.has_value() ? std::get_if<core::LspResponse>(&failed.value()) : nullptr;
    expect(error != nullptr && !error->result.has_value() && error->result.error().code == -32600 &&
               error->result.error().message == "bad",
           "an error response keeps its code and message");
}

void verify_calls()
{
    const Incoming note = incoming_of(
        R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics","params":{"uri":"file:///a"}})");
    const auto *notification =
        note.has_value() ? std::get_if<core::LspNotification>(&note.value()) : nullptr;
    expect(notification != nullptr && notification->method == "textDocument/publishDiagnostics" &&
               std::holds_alternative<JsonObject>(notification->params.value),
           "a message with a method and no id is a notification");
    const Incoming bare = incoming_of(R"({"jsonrpc":"2.0","method":"exit"})");
    const auto *exit =
        bare.has_value() ? std::get_if<core::LspNotification>(&bare.value()) : nullptr;
    expect(exit != nullptr && exit->params == JsonValue{JsonNull{}},
           "a notification without params reads null params");
    const Incoming asked = incoming_of(
        R"({"jsonrpc":"2.0","id":"t1","method":"window/workDoneProgress/create","params":{"token":"x"}})");
    const auto *request =
        asked.has_value() ? std::get_if<core::LspRequest>(&asked.value()) : nullptr;
    expect(request != nullptr && request->id == LspId{std::string{"t1"}} &&
               request->method == "window/workDoneProgress/create",
           "a message with a method and an id is a request");
    expect(
        core::lsp_method_not_found_body(LspId{std::string{"t1"}}) ==
            R"({"jsonrpc":"2.0","id":"t1","error":{"code":-32601,"message":"Method not found"}})",
        "a server request is refused with MethodNotFound echoing its id");
    expect(core::lsp_method_not_found_body(LspId{std::int64_t{9}}) ==
               R"({"jsonrpc":"2.0","id":9,"error":{"code":-32601,"message":"Method not found"}})",
           "MethodNotFound echoes a numeric id");
    for (const auto &[body, failure] : refused)
    {
        const Incoming refusal = incoming_of(body);
        expect(!refusal.has_value() && refusal.error() == failure,
               "a message outside JSON-RPC 2.0 is refused with its closed failure");
    }
}

void verify_bodies()
{
    const auto params = core::parse_json(R"({"processId":null,"rootUri":"file:///c%3A/w"})");
    const std::string initialize =
        core::lsp_request_body(1, "initialize", params.value_or(JsonValue{JsonNull{}}));
    expect(
        initialize ==
            R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":"file:///c%3A/w"}})",
        "a request body carries jsonrpc, id, method and params in order");
    expect(core::lsp_request_body(2, "shutdown", JsonValue{JsonNull{}}) ==
               R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})",
           "a request without params omits them");
    expect(core::lsp_notification_body("exit", JsonValue{JsonNull{}}) ==
               R"({"jsonrpc":"2.0","method":"exit"})",
           "a notification body has no id");
    const Incoming back = incoming_of(initialize);
    const auto *request = back.has_value() ? std::get_if<core::LspRequest>(&back.value()) : nullptr;
    expect(request != nullptr && request->id == LspId{std::int64_t{1}} &&
               request->params == params.value_or(JsonValue{JsonNull{}}),
           "a written request reads back as the same request");
}

void verify_positions()
{
    for (const auto &[where, how] : placed)
    {
        const auto &[text, position] = where;
        const auto &[encoding, offset] = how;
        const std::optional<Offset> found =
            core::lsp_offset_of(buffer_of(text), position, encoding);
        const std::optional<std::size_t> bytes =
            found.has_value() ? std::optional<std::size_t>{found.value().value} : std::nullopt;
        expect(bytes == offset, "an LSP position maps to its byte offset");
    }
}

// at を Position へ写して戻した位置。CRLF の `\r` と `\n` の間は行末（`\r` の前）へ寄る。
[[nodiscard]] bool round_trips(const core::TextBuffer &buffer, std::string_view text,
                               std::size_t at)
{
    const bool inside_crlf = at > 0 && at < text.size() && text[at - 1] == '\r' && text[at] == '\n';
    const std::size_t expected = inside_crlf ? at - 1 : at;
    bool all = true;
    for (const LspPositionEncoding encoding : {utf8, utf16})
    {
        const LspPosition position = core::lsp_position_of(buffer, Offset{at}, encoding);
        const std::optional<Offset> back = core::lsp_offset_of(buffer, position, encoding);
        all = back.has_value() && back.value().value == expected && all;
    }
    return all;
}

// 本文のどの境目も Position へ写して戻すと同じ境目（両方の符号化）。
void verify_round_trips()
{
    constexpr std::array<std::string_view, 5> texts{{
        "abc\ndef\n",
        "日本語\n\nx😀y\n",
        "a\r\n😀\r\n\r\nb",
        "\n",
        "e\xCC\x81\t😀😀",
    }};
    bool all = true;
    for (const std::string_view text : texts)
    {
        const core::TextBuffer buffer = buffer_of(text);
        std::size_t at = 0;
        while (at < text.size())
        {
            all = round_trips(buffer, text, at) && all;
            at = core::next_code_point(text, Offset{at}).value;
        }
        all = round_trips(buffer, text, text.size()) && all;
    }
    expect(all, "every boundary maps to a position and back to itself");
    const LspPosition emoji = core::lsp_position_of(buffer_of("a😀b"), Offset{5}, utf16);
    expect(emoji == LspPosition{0, 3}, "UTF-16 counts the emoji as two units");
    const LspPosition crlf = core::lsp_position_of(buffer_of("ab\r\ncd"), Offset{3}, utf16);
    expect(crlf == LspPosition{0, 2}, "an offset inside CRLF maps to the end of its line");
    const LspPosition past = core::lsp_position_of(buffer_of("ab\ncd"), Offset{99}, utf8);
    expect(past == LspPosition{1, 2}, "an offset past the text maps to its end");
}
} // namespace

void verify_lsp_messages_scope()
{
    verify_responses();
    verify_calls();
    verify_bodies();
    verify_positions();
    verify_round_trips();
}
} // namespace nenenib::tests
