// Issue #1020（親 #994）: adapters の試験が起こす替え玉の言語サーバー（ADR 0160 の決定 9）。
// 標準入力の `Content-Length` の枠を読み、決まった答えを標準出力へ枠で返す。CTest
// には登録しない。initialize には同じ id で capabilities を返し、shutdown には result: null
// を返し、exit で 0 で終わる。initialized と textDocument/did* などほかの本文には黙る。
// 時計を読まない。引数: --echo（受けた本文をそのまま返す）・--double（--echo の答えを 2
// 通まとめて 1 回で書く）・--die-after <N>（N 通目を受けて答えた後に 3 で終わる）・--big
// <bytes>（起きてすぐ bytes の本文を 1 通書く）・--hang（標準入力が閉じても終わらない。stop が
// Job Object で殺すことの試験）・--deaf（標準入力を読まずに止まる。出ている書きの取消の試験）。
#include <windows.h>

#include <algorithm>
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

void answer(std::string_view body)
{
    const std::string id(request_id(body));
    if (calls(body, "initialize"))
    {
        write_all(frame(R"({"jsonrpc":"2.0","id":)" + id +
                        R"(,"result":{"capabilities":{"positionEncoding":"utf-8"}}})"));
    }
    else if (calls(body, "shutdown"))
    {
        write_all(frame(R"({"jsonrpc":"2.0","id":)" + id + R"(,"result":null})"));
    }
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

std::optional<std::size_t> number_after(Arguments arguments, std::string_view name)
{
    for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
    {
        if (name == arguments[index])
        {
            const std::string_view value = arguments[index + 1];
            std::size_t number = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
            return result.ec == std::errc{} ? std::optional<std::size_t>{number} : std::nullopt;
        }
    }
    return std::nullopt;
}

void respond(Arguments arguments, std::string_view body)
{
    if (!has(arguments, "--echo"))
    {
        answer(body);
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
    const auto big = number_after(arguments, "--big");
    if (big.has_value())
    {
        write_all(frame(std::string(big.value(), 'x')));
    }
    const bool deaf = has(arguments, "--deaf");
    const int code = deaf ? 0 : serve(arguments);
    if (deaf || has(arguments, "--hang"))
    {
        WaitForSingleObject(GetCurrentProcess(), INFINITE);
    }
    return code;
}
