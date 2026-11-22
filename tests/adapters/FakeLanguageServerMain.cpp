// Issue #1020（親 #994）: adapters の試験が起こす替え玉の言語サーバー（ADR 0160 の決定 9）。
// 標準入力の `Content-Length` の枠を読み、決まった答えを標準出力へ枠で返す。CTest
// には登録しない。initialize には同じ id で capabilities を返し、shutdown には result: null
// を返し、exit で 0 で終わる。initialized と textDocument/did* などほかの本文には黙る。
// 時計を読まない。引数: --echo（受けた本文をそのまま返す）・--double（--echo の答えを 2
// 通まとめて 1 回で書く）・--die-after <N>（N 通目を受けて答えた後に 3 で終わる）・--big
// <bytes>（起きてすぐ bytes の本文を 1 通書く）・--endless-header <bytes>（起きてすぐ空行の無い
// bytes の見出しを書く。見出しの長さの超えの試験）・--hang（標準入力が閉じても終わらない。stop が
// Job Object で殺すことの試験）・--deaf（標準入力を読まずに止まる。出ている書きの取消の試験）。
// ベンチの引数（#995 の L1d・ADR 0161 の決定 7）: --diagnostics <N>（textDocument/didOpen と
// didChange を受けるたびに、同じ uri と version で N 件の publishDiagnostics を 1 通すぐ書く。
// 1 行目から順に 1 行 1 件・range は各行の 0〜4・severity は error と warning を交互・message は
// 「fake <番号>」。ほかの振る舞いの引数と併せると何も読まずに 4 で終わる）・--silent（did*
// に黙る。引数なしと同じで、ベンチの 設定の行に意図を書くための名前）。
// 答えの引数（#1825・ADR 0190 の決定 4・11）: initialize の答えは
// completionProvider（triggerCharacters は "."）・hoverProvider・definitionProvider
// を持つ。--completion <N>（textDocument/completion に N 件の CompletionList・label
// は「fake<番号>」）・--hover（textDocument/hover に markdown の
// MarkupContent・囲みの行つき）・--definition <path>（textDocument/definition に path の Location
// 1 つ・range は 0 行目の 0〜4）。引数の無い要求には result: null で答える。
// 記録と進み具合の引数（#1942・ADR 0160 の 2026-11-22 の追補）: --stderr <text>（起きてすぐ標準
// エラーへ text と改行を書く）・--progress（initialize に答えた後、`window/workDoneProgress/create`
// の要求（id "p1"・token "fake"）と `$/progress` の begin（題 "indexing"・0%）と
// `window/logMessage`（type 4・"fake log"）を書く）。
#include <windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{
using Arguments = std::span<char *const>;

constexpr std::string_view header_end = "\r\n\r\n";
constexpr std::string_view length_name = "Content-Length:";
constexpr int died_exit_code = 3;
// --diagnostics をほかの振る舞いの引数と併せたとき（#1217 のレビュー R1-1）。読まずに終わる。
constexpr int mixed_exit_code = 4;
// --diagnostics と併せない振る舞いの引数（引数の表）。
constexpr std::array<std::string_view, 8> other_behaviours{
    "--echo", "--double", "--die-after",      "--big",
    "--hang", "--deaf",   "--endless-header", "--silent"};

// 標準入力から読んだがまだ枠に切っていないバイト列。
std::string &unread()
{
    static std::string bytes;
    return bytes;
}

bool read_more()
{
    char chunk[4096];
    DWORD transferred = 0;
    if (ReadFile(GetStdHandle(STD_INPUT_HANDLE), chunk, sizeof(chunk), &transferred, nullptr) ==
            0 ||
        transferred == 0)
    {
        return false;
    }
    unread().append(chunk, transferred);
    return true;
}

std::optional<std::size_t> declared_length(std::string_view headers)
{
    const auto at = headers.find(length_name);
    if (at == std::string_view::npos)
    {
        return std::nullopt;
    }
    auto value = headers.substr(at + length_name.size());
    value.remove_prefix((std::min)(value.find_first_not_of(' '), value.size()));
    std::size_t length = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), length);
    return result.ec == std::errc{} ? std::optional<std::size_t>{length} : std::nullopt;
}

// 次の 1 通の本文。標準入力が閉じたら nullopt。
std::optional<std::string> read_message()
{
    while (unread().find(header_end) == std::string::npos)
    {
        if (!read_more())
        {
            return std::nullopt;
        }
    }
    const auto end = unread().find(header_end);
    const auto length = declared_length(std::string_view(unread()).substr(0, end));
    const auto start = end + header_end.size();
    while (length.has_value() && unread().size() - start < length.value())
    {
        if (!read_more())
        {
            return std::nullopt;
        }
    }
    if (!length.has_value())
    {
        return std::nullopt;
    }
    std::string body = unread().substr(start, length.value());
    unread().erase(0, start + length.value());
    return body;
}

std::string frame(std::string_view body)
{
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
}

void write_all(std::string_view bytes)
{
    while (!bytes.empty())
    {
        DWORD transferred = 0;
        const auto size = static_cast<DWORD>((std::min<std::size_t>)(bytes.size(), 1U << 20U));
        if (WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), bytes.data(), size, &transferred, nullptr) ==
            0)
        {
            ExitProcess(died_exit_code);
        }
        bytes.remove_prefix(transferred);
    }
}

// `"id":` の後ろの値をそのまま（数でも文字列でも）。
std::string_view request_id(std::string_view body)
{
    const auto at = body.find("\"id\":");
    if (at == std::string_view::npos)
    {
        return "null";
    }
    const auto value = body.substr(at + 5);
    return value.substr(0, value.find_first_of(",}"));
}

bool calls(std::string_view body, std::string_view method)
{
    return body.find("\"method\":\"" + std::string(method) + "\"") != std::string_view::npos;
}

// 本文の中の `"<name>":` の後ろの値をそのまま（文字列は引用符ごと・数はそのまま）。本文の text は
// JSON の文字列で `"` が `\"` になっているので、鍵の形は本物の欄にだけ当たる。
std::string_view member_value(std::string_view body, std::string_view name)
{
    const std::string key = "\"" + std::string(name) + "\":";
    const auto at = body.find(key);
    if (at == std::string_view::npos)
    {
        return "null";
    }
    const auto value = body.substr(at + key.size());
    if (value.starts_with('"'))
    {
        return value.substr(0, value.find('"', 1) + 1);
    }
    return value.substr(0, value.find_first_of(",}"));
}

// 1 件の診断。line 行目の 0〜4・偶数番は error（1）・奇数番は warning（2）。
std::string fake_diagnostic(std::size_t line)
{
    const std::string at = std::to_string(line);
    return R"({"range":{"start":{"line":)" + at + R"(,"character":0},"end":{"line":)" + at +
           R"(,"character":4}},"severity":)" + (line % 2 == 0 ? "1" : "2") +
           R"(,"message":"fake )" + at + R"("})";
}

// didOpen と didChange に、同じ uri と version で count 件の publishDiagnostics を 1 通返す。
void publish_diagnostics(std::string_view body, std::size_t count)
{
    if (!calls(body, "textDocument/didOpen") && !calls(body, "textDocument/didChange"))
    {
        return;
    }
    std::string diagnostics;
    for (std::size_t line = 0; line < count; ++line)
    {
        if (line != 0)
        {
            diagnostics += ',';
        }
        diagnostics += fake_diagnostic(line);
    }
    constexpr std::string_view head =
        R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics","params":{"uri":)";
    std::string published(head);
    published += member_value(body, "uri");
    published += R"(,"version":)";
    published += member_value(body, "version");
    published += R"(,"diagnostics":[)";
    published += diagnostics;
    published += "]}}";
    write_all(frame(published));
}

bool has(Arguments arguments, std::string_view name)
{
    for (const char *argument : arguments)
    {
        if (name == argument)
        {
            return true;
        }
    }
    return false;
}

// name の次の引数（無ければ値なし）。
std::optional<std::string_view> text_after(Arguments arguments, std::string_view name)
{
    for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
    {
        if (name == arguments[index])
        {
            return std::string_view(arguments[index + 1]);
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> number_after(Arguments arguments, std::string_view name)
{
    const auto value = text_after(arguments, name);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    std::size_t number = 0;
    const auto result =
        std::from_chars(value.value().data(), value.value().data() + value.value().size(), number);
    return result.ec == std::errc{} ? std::optional<std::size_t>{number} : std::nullopt;
}

// --completion N の CompletionList。
std::string completion_result(std::size_t count)
{
    std::string items;
    for (std::size_t index = 0; index < count; ++index)
    {
        items += index == 0 ? "" : ",";
        items += R"({"label":"fake)" + std::to_string(index) + R"("})";
    }
    return R"({"isIncomplete":false,"items":[)" + items + "]}";
}

// --definition <path> の Location（区切りの \ を / にした素朴な file: の URI）。
std::string definition_result(std::string_view path)
{
    std::string uri = "file:///";
    for (const char character : path)
    {
        uri.push_back(character == '\\' ? '/' : character);
    }
    return R"({"uri":")" + uri +
           R"(","range":{"start":{"line":0,"character":0},"end":{"line":0,"character":4}}})";
}

// 文脈つきの要求の答えの result（引数が無ければ null）。
std::string request_result(Arguments arguments, std::string_view body)
{
    const auto completion = number_after(arguments, "--completion");
    if (calls(body, "textDocument/completion") && completion.has_value())
    {
        return completion_result(completion.value());
    }
    if (calls(body, "textDocument/hover") && has(arguments, "--hover"))
    {
        return R"({"contents":{"kind":"markdown","value":"```cpp\nint fake()\n```\nfake hover"}})";
    }
    const auto definition = text_after(arguments, "--definition");
    if (calls(body, "textDocument/definition") && definition.has_value())
    {
        return definition_result(definition.value());
    }
    return "null";
}

// --progress の 3 通（initialize の答えの後）。
void write_progress()
{
    write_all(frame(
        R"({"jsonrpc":"2.0","id":"p1","method":"window/workDoneProgress/create","params":{"token":"fake"}})"));
    write_all(frame(
        R"({"jsonrpc":"2.0","method":"$/progress","params":{"token":"fake","value":{"kind":"begin","title":"indexing","percentage":0}}})"));
    write_all(frame(
        R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"type":4,"message":"fake log"}})"));
}

void answer(Arguments arguments, std::string_view body)
{
    const std::string id(request_id(body));
    if (calls(body, "initialize"))
    {
        write_all(frame(
            R"({"jsonrpc":"2.0","id":)" + id +
            R"(,"result":{"capabilities":{"positionEncoding":"utf-8","textDocumentSync":2,"completionProvider":{"triggerCharacters":["."]},"hoverProvider":true,"definitionProvider":true}}})"));
        if (has(arguments, "--progress"))
        {
            write_progress();
        }
    }
    else if (calls(body, "shutdown"))
    {
        write_all(frame(R"({"jsonrpc":"2.0","id":)" + id + R"(,"result":null})"));
    }
    else if (calls(body, "textDocument/completion") || calls(body, "textDocument/hover") ||
             calls(body, "textDocument/definition"))
    {
        write_all(frame(R"({"jsonrpc":"2.0","id":)" + id + R"(,"result":)" +
                        request_result(arguments, body) + "}"));
    }
}

void respond(Arguments arguments, std::string_view body)
{
    if (!has(arguments, "--echo"))
    {
        answer(arguments, body);
        const auto diagnostics = number_after(arguments, "--diagnostics");
        if (diagnostics.has_value())
        {
            publish_diagnostics(body, diagnostics.value());
        }
        return;
    }
    const std::string once = frame(body);
    write_all(has(arguments, "--double") ? once + once : once);
}

int serve(Arguments arguments)
{
    const auto die_after = number_after(arguments, "--die-after");
    for (std::size_t count = 1;; ++count)
    {
        const auto body = read_message();
        if (!body.has_value())
        {
            return 0;
        }
        if (calls(body.value(), "exit"))
        {
            return 0;
        }
        respond(arguments, body.value());
        if (die_after.has_value() && count == die_after.value())
        {
            return died_exit_code;
        }
    }
}
} // namespace

int main(int argc, char **argv)
{
    const Arguments arguments(argv, static_cast<std::size_t>(argc));
    if (has(arguments, "--diagnostics") &&
        std::ranges::any_of(other_behaviours,
                            [&](std::string_view name) { return has(arguments, name); }))
    {
        return mixed_exit_code;
    }
    const auto error_text = text_after(arguments, "--stderr");
    if (error_text.has_value())
    {
        const std::string line = std::string(error_text.value()) + "\n";
        DWORD transferred = 0;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), line.data(), static_cast<DWORD>(line.size()),
                  &transferred, nullptr);
    }
    const auto big = number_after(arguments, "--big");
    if (big.has_value())
    {
        write_all(frame(std::string(big.value(), 'x')));
    }
    const auto endless = number_after(arguments, "--endless-header");
    if (endless.has_value())
    {
        write_all(std::string(endless.value(), 'h'));
    }
    const bool deaf = has(arguments, "--deaf");
    const int code = deaf ? 0 : serve(arguments);
    if (deaf || has(arguments, "--hang"))
    {
        WaitForSingleObject(GetCurrentProcess(), INFINITE);
    }
    return code;
}
