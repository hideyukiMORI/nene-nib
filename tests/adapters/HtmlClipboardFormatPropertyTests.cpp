// scope `--cfhtml-property` の性質の試験（#917・ADR 0157 の決定 4・ADR 0042）。
// クリップボードの "HTML Format"（CF_HTML）から断片を切り出す `html_fragment_of` は、バイト列を
// 受ける純粋な口なので、実機のクリップボードに触れずに呼ぶ（ARC-003 / ARC-007）。
// 固定 seed の決定的な生成器で、正しい見本（見出し 5 行 + StartFragment / EndFragment の注釈）と、
// 位置を選んで書いた見出し（範囲の内・外・逆順・巨大・幅の違い・行の順の入れ替え・BOM・LF）と、
// 見本への変異（見出しの値の置換・欠け・入れ替え・巨大な数・負の数・数でない文字・BOM の付け外し・
// 切り詰め・1 バイト置換・NUL・CRLF と LF の混ぜ・`<` の挿入）を作り、次を確かめる。
//   1. 落ちない（ASan / UBSan）・止まらない。例外は捕まえないので、出ればこの実行が落ちる。
//   2. 結果は断片（std::string_view）か値なし（閉じた失敗）。
//   3. 断片は入力の内（data() の範囲が入力の中）。見出しから素朴に計算した期待と一致する。
//      規則は実装の正本（src/adapters/win32/HtmlClipboardFormat.cpp）を読んで写したもの:
//      見出しは最初の `<` より前（無ければ全体）・鍵はその中の最初の出現・直後の 10 進の数字を
//      最大 18 桁読む（符号と空白は数でない）・両方あり 0 ≤ start ≤ end ≤ size ならその範囲、
//      それ以外は値なし。StartHTML / EndHTML は読まない（落ちる先は全体ではなく値なし）。
//   4. 同じ入力で同じ結果（決定性）。生成器も同じ seed で同じ入力を作る。
// 回数は Debug（ASan）で 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして回す（commit
// しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型・時刻・環境変数は使わない。
#include "HtmlClipboardFormat.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace
{
namespace adapters = nenenib::adapters::win32;
using Random = std::mt19937_64;
using Positions = std::pair<std::size_t, std::size_t>;
using Known = std::pair<std::string, Positions>;
using Mutation = void (*)(Random &, std::string &);
using namespace std::string_view_literals;

constexpr std::size_t rounds = 1U;
constexpr std::size_t known_inputs = 1500U * rounds;
constexpr std::size_t mutated_inputs = 3000U * rounds;
constexpr std::string_view holds{};
constexpr std::string_view start_key = "StartFragment:";
constexpr std::string_view end_key = "EndFragment:";
// 実装の正本が読む桁の上限（HtmlClipboardFormat.cpp の digit_limit）。
constexpr std::size_t digit_limit = 18U;
// どの入力より大きい位置（入力は数 KiB に収まる）。
constexpr std::size_t outside = 1'000'000U;
constexpr std::string_view before = "<html><body>\r\n<!--StartFragment-->";
constexpr std::string_view after = "<!--EndFragment-->\r\n</body></html>";

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

// --- 素朴な期待（実装を呼ばずに見出しから計算し直す）---

[[nodiscard]] std::string_view naive_header(std::string_view bytes)
{
    for (std::size_t at = 0; at < bytes.size(); ++at)
    {
        if (bytes[at] == '<')
        {
            return bytes.substr(0, at);
        }
    }
    return bytes;
}

[[nodiscard]] std::optional<std::size_t> naive_key(std::string_view header, std::string_view key)
{
    for (std::size_t at = 0; at + key.size() <= header.size(); ++at)
    {
        if (header.substr(at, key.size()) == key)
        {
            return at + key.size();
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> naive_number(std::string_view header, std::string_view key)
{
    const auto from = naive_key(header, key);
    if (!from.has_value())
    {
        return std::nullopt;
    }
    std::size_t value = 0;
    std::size_t digits = 0;
    for (std::size_t at = from.value(); at < header.size() && digits < digit_limit; ++at)
    {
        const char character = header[at];
        if (character < '0' || character > '9')
        {
            break;
        }
        value = (value * 10U) + static_cast<std::size_t>(character - '0');
        ++digits;
    }
    return digits == 0U ? std::nullopt : std::optional<std::size_t>{value};
}

[[nodiscard]] std::optional<Positions> within_input(Positions chosen, std::size_t size)
{
    if (chosen.first > chosen.second || chosen.second > size)
    {
        return std::nullopt;
    }
    return chosen;
}

[[nodiscard]] std::optional<Positions> naive_positions(std::string_view bytes)
{
    const std::string_view header = naive_header(bytes);
    const auto begin = naive_number(header, start_key);
    const auto end = naive_number(header, end_key);
    if (!begin.has_value() || !end.has_value())
    {
        return std::nullopt;
    }
    return within_input({begin.value(), end.value()}, bytes.size());
}

// --- 結果の照合 ---

[[nodiscard]] std::uintptr_t address(const char *pointer)
{
    return std::bit_cast<std::uintptr_t>(pointer);
}

[[nodiscard]] bool inside(std::string_view bytes, std::string_view fragment)
{
    const std::uintptr_t first = address(bytes.data());
    const std::uintptr_t at = address(fragment.data());
    return at >= first && at - first <= bytes.size() &&
           fragment.size() <= bytes.size() - (at - first);
}

[[nodiscard]] bool same(const std::optional<std::string_view> &left,
                        const std::optional<std::string_view> &right)
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    return !left.has_value() || (left.value().data() == right.value().data() &&
                                 left.value().size() == right.value().size());
}

[[nodiscard]] bool spans(std::string_view bytes, std::string_view fragment, Positions expected)
{
    const std::string_view wanted = bytes.substr(expected.first, expected.second - expected.first);
    return fragment.data() == wanted.data() && fragment.size() == wanted.size();
}

// 破れの理由を返す（空なら守られている）。expected は見出しから計算した期待。
[[nodiscard]] std::string_view against(std::string_view bytes,
                                       const std::optional<Positions> &expected)
{
    const auto first = adapters::html_fragment_of(bytes);
    if (!same(first, adapters::html_fragment_of(bytes)))
    {
        return "the same input gave different fragments";
    }
    if (!first.has_value())
    {
        return expected.has_value() ? "refused a valid header" : holds;
    }
    if (!inside(bytes, first.value()))
    {
        return "the fragment lies outside the input";
    }
    if (!expected.has_value())
    {
        return "accepted an invalid header";
    }
    return spans(bytes, first.value(), expected.value()) ? holds
                                                         : "the fragment differs from the header";
}

[[nodiscard]] std::string_view naive_break(std::string_view bytes)
{
    return against(bytes, naive_positions(bytes));
}

// --- 破れの報告（縮めて 16 進で出す）---

void print_bytes(std::string_view input)
{
    const std::size_t shown = input.size() < 200U ? input.size() : 200U;
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

[[nodiscard]] bool shrunk(std::string &input, std::size_t chunk, std::string_view reason)
{
    for (std::size_t at = 0; at + chunk <= input.size(); at += chunk)
    {
        std::string candidate = input;
        candidate.erase(at, chunk);
        if (naive_break(candidate) == reason)
        {
            input = std::move(candidate);
            return true;
        }
    }
    return false;
}

void report(const char *family, std::string input, std::string_view reason)
{
    const std::size_t original = input.size();
    std::size_t chunk = original / 2U > 0U ? original / 2U : 1U;
    while (chunk > 0U && naive_break(input) == reason)
    {
        chunk = shrunk(input, chunk, reason) ? (chunk < input.size() ? chunk : input.size())
                                             : chunk / 2U;
    }
    std::fprintf(stderr, "  %s: %.*s (%zu bytes, minimized to %zu):", family,
                 static_cast<int>(reason.size()), reason.data(), original, input.size());
    print_bytes(input);
}

// --- 生成器 ---

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return bound == 0U ? 0U : static_cast<std::size_t>(random() % bound);
}

template <std::size_t Count>
[[nodiscard]] std::string_view pick(Random &random, const std::array<std::string_view, Count> &from)
{
    return from.at(below(random, Count));
}

// 断片の中身（多バイト・NUL・`<` の後ろの鍵・CRLF）。
constexpr std::array<std::string_view, 8> fragment_texts{{
    "",
    "<b>x</b>",
    "<p>日本語</p>",
    "<p>StartFragment:0000000001 EndFragment:0000000002</p>",
    std::string_view("<i>\0</i>", 8),
    "<table><tr><td>a\r\nb</td></tr></table>",
    "\xF0\x9F\x98\x80 emoji \xC3",
    "<!--EndFragment-->",
}};

// StartHTML / EndHTML の値（実装は読まないので何でもよい）。
constexpr std::array<std::string_view, 6> html_values{
    {"0000000000", "x", "-1", "99999999999999999999999", "", "0000000105"}};

[[nodiscard]] std::string number_text(Random &random, std::size_t value)
{
    const std::size_t shape = below(random, 3U);
    if (shape == 0U)
    {
        return std::format("{:010}", value);
    }
    return shape == 1U ? std::format("{}", value) : std::format("{:018}", value);
}

// 行の順を入れ替える（std::shuffle は分布の型を使うので剰余の Fisher-Yates で）。
void shuffle_lines(Random &random, std::array<std::string, 5> &lines)
{
    for (std::size_t index = lines.size() - 1U; index > 0U; --index)
    {
        std::swap(lines.at(index), lines.at(below(random, index + 1U)));
    }
}

[[nodiscard]] std::string header_of(Random &random, std::array<std::string, 5> lines)
{
    if (below(random, 4U) == 0U)
    {
        shuffle_lines(random, lines);
    }
    const bool lf = below(random, 4U) == 0U;
    std::string header = below(random, 6U) == 0U ? "\xEF\xBB\xBF" : "";
    for (const std::string &line : lines)
    {
        header += line;
        header += lf ? "\n" : "\r\n";
    }
    return header;
}

// 正しい見本（cf_html と同じ形・位置は 10 桁のゼロ埋め・CRLF）。位置は UTF-8 のバイト位置。
[[nodiscard]] Known sample(std::string_view fragment)
{
    constexpr std::string_view shape = "Version:0.9\r\nStartHTML:{:010}\r\nEndHTML:{:010}\r\n"
                                       "StartFragment:{:010}\r\nEndFragment:{:010}\r\n";
    const std::size_t header = std::format(shape, 0, 0, 0, 0).size();
    const std::size_t start = header + before.size();
    const std::size_t end = start + fragment.size();
    std::string bytes = std::format(shape, header, end + after.size(), start, end);
    bytes += before;
    bytes += fragment;
    bytes += after;
    return {std::move(bytes), {start, end}};
}

// 位置を選ぶ: 範囲の内・逆順・範囲の外・巨大・空の範囲。
[[nodiscard]] Positions chosen_positions(Random &random)
{
    const std::size_t first = below(random, 300U);
    const std::size_t second = below(random, 300U);
    const std::size_t low = first < second ? first : second;
    const std::size_t high = first < second ? second : first;
    const std::array<Positions, 5> shapes{{
        {low, high},
        {high + 1U, low},
        {first, outside + second},
        {999'999'999'999'999'000U + first, 999'999'999'999'999'999U},
        {first, first},
    }};
    return shapes.at(below(random, shapes.size()));
}

// 位置を選んで書いた見出しと、選んだ位置から計算した期待（解析を通さない）。
[[nodiscard]] Known known(Random &random)
{
    const Positions chosen = chosen_positions(random);
    std::array<std::string, 5> lines{
        std::string{"Version:0.9"},
        std::format("StartHTML:{}", pick(random, html_values)),
        std::format("EndHTML:{}", pick(random, html_values)),
        std::format("StartFragment:{}", number_text(random, chosen.first)),
        std::format("EndFragment:{}", number_text(random, chosen.second)),
    };
    std::string bytes = header_of(random, std::move(lines));
    bytes += before;
    bytes += pick(random, fragment_texts);
    bytes += after;
    return {std::move(bytes), chosen};
}

// 見出しの値に置く文字（数でない・負・符号・空白・巨大・ゼロ埋めの 19 桁以上）。
constexpr std::array<std::string_view, 14> values{{
    "",
    "x",
    "-5",
    "+5",
    " 5",
    "5x",
    "18446744073709551615",
    "184467440737095516160",
    "00000000000000000000000000005",
    "4294967296",
    "0",
    "0000000000",
    "999999999999999999",
    std::string_view("\0"
                     "5",
                     2),
}};

constexpr std::array<std::string_view, 5> keys{
    {"Version:", "StartHTML:", "EndHTML:", "StartFragment:", "EndFragment:"}};

constexpr std::array<std::string_view, 12> inserted{{
    "<",
    "\r\n",
    "\n",
    "\r",
    "\xEF\xBB\xBF",
    std::string_view("\0", 1),
    "StartFragment:",
    "EndFragment:",
    "StartFragment:0000000000\r\n",
    "EndFragment:4294967296\r\n",
    "\xC3",
    "<!--",
}};

[[nodiscard]] bool is_digit(char character) noexcept
{
    return character >= '0' && character <= '9';
}

// 鍵の 1 つを選び、直後の数字の並びを別の値に置き換える（鍵が無ければ何もしない）。
void replace_value(Random &random, std::string &text)
{
    const std::string_view key = pick(random, keys);
    const std::size_t found = text.find(key);
    if (found == std::string::npos)
    {
        return;
    }
    const std::size_t from = found + key.size();
    std::size_t to = from;
    while (to < text.size() && is_digit(text[to]))
    {
        ++to;
    }
    text.replace(from, to - from, pick(random, values));
}

[[nodiscard]] std::size_t line_start(Random &random, const std::string &text)
{
    const std::size_t at = below(random, text.size());
    const std::size_t found = text.rfind('\n', at);
    return found == std::string::npos ? 0U : found + 1U;
}

[[nodiscard]] std::size_t line_end(const std::string &text, std::size_t start)
{
    const std::size_t found = text.find('\n', start);
    return found == std::string::npos ? text.size() : found + 1U;
}

void drop_line(Random &random, std::string &text)
{
    const std::size_t start = line_start(random, text);
    text.erase(start, line_end(text, start) - start);
}

void duplicate_line(Random &random, std::string &text)
{
    const std::size_t start = line_start(random, text);
    text.insert(start, text.substr(start, line_end(text, start) - start));
}

// StartFragment と EndFragment の数を入れ替える（逆順の範囲）。
void swap_values(Random &random, std::string &text)
{
    const std::size_t start = text.find(start_key);
    const std::size_t end = text.find(end_key);
    if (start == std::string::npos || end == std::string::npos || start > end)
    {
        return;
    }
    const std::size_t first = start + start_key.size();
    const std::size_t second = end + end_key.size();
    const std::size_t width = below(random, 2U) == 0U ? 10U : 1U;
    std::string left = text.substr(first, width);
    std::string right = text.substr(second, width);
    text.replace(second, width, left);
    text.replace(first, width, right);
}

void toggle_bom(Random & /*random*/, std::string &text)
{
    constexpr std::string_view bom = "\xEF\xBB\xBF";
    if (text.starts_with(bom))
    {
        text.erase(0, bom.size());
        return;
    }
    text.insert(0, bom);
}

void mix_newlines(Random &random, std::string &text)
{
    const std::size_t found = text.find('\n', below(random, text.size()));
    if (found == std::string::npos)
    {
        return;
    }
    const bool crlf = found > 0U && text[found - 1U] == '\r';
    if (crlf)
    {
        text.erase(found - 1U, 1U);
        return;
    }
    text.replace(found, 1U, below(random, 2U) == 0U ? "\r\n" : "\r");
}

void replace_byte(Random &random, std::string &text)
{
    text.at(below(random, text.size())) = static_cast<char>(below(random, 256U));
}

void erase_bytes(Random &random, std::string &text)
{
    text.erase(below(random, text.size()), 1U + below(random, 8U));
}

void insert_bytes(Random &random, std::string &text)
{
    text.insert(below(random, text.size() + 1U), pick(random, inserted));
}

void truncate_bytes(Random &random, std::string &text)
{
    text.resize(below(random, text.size()));
}

constexpr std::array<Mutation, 11> mutations{{replace_value, drop_line, duplicate_line, swap_values,
                                              toggle_bom, mix_newlines, replace_byte, erase_bytes,
                                              insert_bytes, truncate_bytes, replace_value}};

[[nodiscard]] std::string mutated(Random &random)
{
    std::string text =
        below(random, 2U) == 0U ? sample(pick(random, fragment_texts)).first : known(random).first;
    const std::size_t count = 1U + below(random, 3U);
    for (std::size_t step = 0; step < count && !text.empty(); ++step)
    {
        mutations.at(below(random, mutations.size()))(random, text);
    }
    return text;
}

// --- 性質 ---

void verify_generators_are_deterministic()
{
    Random left{9171U};
    Random right{9171U};
    bool equal = true;
    for (std::size_t round = 0; round < 32U; ++round)
    {
        equal = equal && mutated(left) == mutated(right) && known(left) == known(right);
    }
    expect(equal, "the generators give the same inputs for the same seed");
}

void verify_samples()
{
    bool held = true;
    for (const std::string_view text : fragment_texts)
    {
        const Known made = sample(text);
        const auto fragment = adapters::html_fragment_of(made.first);
        held = held && fragment.has_value() && fragment.value() == text &&
               against(made.first, made.second).empty() && naive_break(made.first).empty();
    }
    expect(held, "well-formed samples give their fragment");
}

// 選んだ位置から計算した期待と、素朴な解析の期待の両方と照らす（素朴な解析そのものも確かめる）。
void verify_known_headers()
{
    Random random{9172U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < known_inputs; ++index)
    {
        const Known made = known(random);
        std::string_view reason = against(made.first, within_input(made.second, made.first.size()));
        reason = reason.empty() ? naive_break(made.first) : reason;
        if (!reason.empty() && broken++ == 0U)
        {
            report("known headers", made.first, reason);
        }
    }
    expect(broken == 0U, "headers with chosen positions give exactly that range or nothing");
}

void verify_mutated_inputs()
{
    Random random{9173U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < mutated_inputs; ++index)
    {
        const std::string input = mutated(random);
        const std::string_view reason = naive_break(input);
        if (!reason.empty() && broken++ == 0U)
        {
            report("mutated inputs", input, reason);
        }
    }
    expect(broken == 0U, "mutated inputs give a fragment inside the input or nothing");
}

[[nodiscard]] bool refused(std::string_view bytes)
{
    return !adapters::html_fragment_of(bytes).has_value();
}

[[nodiscard]] bool gives(std::string_view bytes, std::string_view fragment)
{
    const auto found = adapters::html_fragment_of(bytes);
    return found.has_value() && found.value() == fragment && inside(bytes, found.value());
}

// 回帰と観測の case（見つけた破れと、実装の規則の端）。
void verify_edges()
{
    expect(refused("") && refused("<") && refused("StartFragment:") &&
               refused("StartFragment:0\r\nEndFragment:"),
           "empty input and missing numbers are refused");
    expect(gives("StartFragment:0\r\nEndFragment:0\r\n", "") &&
               gives("EndFragment:5\nStartFragment:2\nabcdef", "dFr"),
           "keys in any order and LF give the byte range");
    expect(refused("StartFragment:-1\r\nEndFragment:2\r\n<b>") &&
               refused("StartFragment: 1\r\nEndFragment:2\r\n<b>") &&
               refused("StartFragment:+1\r\nEndFragment:2\r\n<b>"),
           "signs and spaces are not numbers");
    expect(refused("StartFragment:3\r\nEndFragment:2\r\n<b>") &&
               refused("StartFragment:0\r\nEndFragment:4294967296\r\n<b>"),
           "reversed and outside ranges are refused");
    expect(refused("Version:0.9\r\n<StartFragment:0\r\nEndFragment:1\r\n"),
           "keys after the first '<' are not read");
    expect(refused("StartFragment:\0"
                   "0\r\nEndFragment:1\r\n"sv),
           "a NUL before the digits is not a number");
    // StartHTML / EndHTML は読まない: 断片の位置が妥当でなければ全体にも HTML にも落ちない。
    expect(refused("Version:0.9\r\nStartHTML:0000000000\r\nEndHTML:0000000050\r\n"
                   "StartFragment:x\r\nEndFragment:0000000001\r\n<html></html>"),
           "an invalid fragment does not fall back to StartHTML / EndHTML");
    // 観測（#917 の走りで見つけた規則の端）: 数字は 18 桁で読み切り、19 桁目からは捨てる。
    // ゼロ埋めの 22 桁の 5 は 0 と読まれる（範囲は入力の内に収まる）。直すかは別 Issue。
    expect(gives("StartFragment:0000000000000000000005\r\nEndFragment:0000000000000000000009"
                 "\r\nabc",
                 ""),
           "digits past the 18th are dropped (observed rule, the range stays inside)");
}
} // namespace

int main(int argc, char **argv)
{
    const std::string_view scope = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && scope != "--cfhtml-property"))
    {
        std::fprintf(stderr, "Unknown CF_HTML property test scope\n");
        return 1;
    }
    verify_generators_are_deterministic();
    verify_samples();
    verify_known_headers();
    verify_mutated_inputs();
    verify_edges();
    std::printf("CF_HTML properties: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
