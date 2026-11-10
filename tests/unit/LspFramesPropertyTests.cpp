// scope `--lsp-frames-property` の性質の試験（#1291・ADR 0160 の決定 3 / 5・ADR 0042）。
// 固定 seed の決定的な生成器で作った枠の列（見出しの書き方の揺れ・本文の任意のバイト・見出しの
// 変異・本文の切り詰め）を core の lsp_frames に渡し、次を確かめる。
//   1. 一括で渡した結果（本文の列・consumed・閉じた失敗）が、試験の中に書いた素朴な模型（遅い
//      別実装・ヘッダの注記の規則の写し）と同じ。
//   2. adapter の Win32LanguageServer::deliver と同じ手順（繋ぐ → 切る → consumed だけ捨てる・失敗
//      したら止める）で任意の位置の chunk に割って流すと、成功の列は一括と同じ本文の列と残りに
//      なる。失敗する列は失敗の種類が一括と同じで、届いた本文は一括の失敗より前の本文の頭。
//   3. 変異 1 つだけの枠を先頭に置いた列は、決めてある閉じた失敗（LspFrameFailure の注記）になる。
//      CR だけ / LF だけの見出しは空行が来ないので、上限（8 KiB）までは待ち、超えたら
//      header_too_long。見出しの長さが上限ちょうどは受け、1 バイト超えると断る。
//   4. lsp_framed で包んだ本文の列は同じ列に戻る（往復）。
//   5. JSON-RPC の 1 通を本文にした列を分割して流し、parse_json → lsp_incoming で読んだ型の列が
//      生成した型の列と一致する。1 か所だけ壊した 1 通は決めてある LspMessageFailure になる。
// Content-Length の値が数字の列で桁あふれし、さらに後ろに数字でない文字が続く形（from_chars の
// 都合で body_too_long になる）は、規則の文が決めていないので生成しない。
// 回数は Debug（ASan）で scope 全体が 1 秒程度に終わる値。生成器は std::mt19937_64 の剰余だけを
// 使い、分布の型は使わない。時計と環境変数を読まない。
#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "JsonWrite.hpp"
#include "LspError.hpp"
#include "LspFrameFailure.hpp"
#include "LspFrames.hpp"
#include "LspId.hpp"
#include "LspIncoming.hpp"
#include "LspMessageFailure.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspRequest.hpp"
#include "LspResponse.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::JsonArray;
using core::JsonNull;
using core::JsonObject;
using core::JsonValue;
using core::LspFrameFailure;
using core::LspId;
using core::LspIncoming;
using core::LspMessageFailure;
using Random = std::mt19937_64;
using Bodies = std::vector<std::string>;
// 本文の列・consumed・失敗（値なしは成功）。失敗のときの本文の列は失敗した枠より前の本文。
using Outcome = std::tuple<Bodies, std::size_t, std::optional<LspFrameFailure>>;
// 生成した列のバイト列と、模型を使わずに分かる本文の列（変異の無い列だけが値を持つ）。
using Stream = std::pair<std::string, std::optional<Bodies>>;
using Incoming = std::expected<LspIncoming, LspMessageFailure>;
// 1 通の本文と、読んだときに期待する型か閉じた失敗。
using Message = std::pair<std::string, Incoming>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t stream_cases = 300U * rounds;
constexpr std::size_t mutated_cases = 300U * rounds;
constexpr std::size_t single_cases = 200U * rounds;
constexpr std::size_t message_cases = 200U * rounds;
constexpr std::size_t limit_cases = 6U * rounds;
constexpr std::size_t splits_per_stream = 3U;
constexpr std::size_t largest_body = 96U;
constexpr std::size_t most_frames = 6U;
constexpr std::int64_t exact_limit = (std::int64_t{1} << 53) - 1;

constexpr std::string_view header_end = "\r\n\r\n";
constexpr std::string_view line_end = "\r\n";

// 本文に入れる断片（見出しに似た列・空行・NUL・不正な UTF-8 も本文としては素通り）。
constexpr std::array<std::string_view, 12> body_pieces{{
    "a",
    "{\"x\":1}",
    "\r\n",
    "\r\n\r\n",
    "Content-Length: 5",
    ":",
    "\n",
    "\r",
    std::string_view{"\0", 1},
    "\xFF",
    "\xE3\x81\x82",
    " ",
}};

// 正しい Content-Length の行の書き方（名前の大小・空白と Tab・先頭の 0）。%N が本文の長さ。
constexpr std::array<std::string_view, 6> length_lines{{
    "Content-Length: %N",
    "content-length:%N",
    "CONTENT-LENGTH: \t%N\t ",
    "Content-Length:  000%N",
    "cOnTeNt-LeNgTh:%N  ",
    "Content-Length:\t%N",
}};

// 読み飛ばす見出しの行（値に `:` を含むものも）。
constexpr std::array<std::string_view, 4> other_lines{{
    "Content-Type: application/vscode-jsonrpc; charset=utf-8",
    "X-Extra: a: b: c",
    "x:",
    "Content-Lengthy: 12",
}};

// 変異の種類。
enum class Mutation : std::uint8_t
{
    missing_length,
    duplicate_length,
    negative_length,
    not_a_number,
    huge_length,
    overflowing_length,
    no_colon,
    empty_name,
    lf_only,
    cr_only,
    truncated_body,
    longer_length,
    shorter_length
};

constexpr std::array<Mutation, 13> mutations{{
    Mutation::missing_length,
    Mutation::duplicate_length,
    Mutation::negative_length,
    Mutation::not_a_number,
    Mutation::huge_length,
    Mutation::overflowing_length,
    Mutation::no_colon,
    Mutation::empty_name,
    Mutation::lf_only,
    Mutation::cr_only,
    Mutation::truncated_body,
    Mutation::longer_length,
    Mutation::shorter_length,
}};

[[nodiscard]] std::size_t below(Random &random, std::size_t limit)
{
    return static_cast<std::size_t>(random() % limit);
}

[[nodiscard]] std::string random_body(Random &random)
{
    std::string body;
    const std::size_t pieces = below(random, 12U);
    for (std::size_t index = 0; index < pieces && body.size() < largest_body; ++index)
    {
        body += body_pieces.at(below(random, body_pieces.size()));
    }
    return body;
}

[[nodiscard]] std::string replaced_number(std::string_view line, std::string_view number)
{
    std::string text{line};
    const std::size_t at = text.find("%N");
    text.replace(at, 2, number);
    return text;
}

[[nodiscard]] std::string length_line(Random &random, std::string_view number)
{
    return replaced_number(length_lines.at(below(random, length_lines.size())), number);
}

[[nodiscard]] std::string joined(const std::vector<std::string> &lines, std::string_view between)
{
    std::string text;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        text += index == 0 ? std::string{} : std::string{between};
        text += lines.at(index);
    }
    return text;
}

// Content-Length の行 length の前後に読み飛ばす行を 0〜2 つ置いた見出しの行の列。
[[nodiscard]] std::vector<std::string> header_around(Random &random, std::string length)
{
    std::vector<std::string> lines;
    const std::size_t before = below(random, 3U);
    for (std::size_t index = 0; index < before; ++index)
    {
        lines.emplace_back(other_lines.at(below(random, other_lines.size())));
    }
    lines.push_back(std::move(length));
    if (below(random, 3U) == 0)
    {
        lines.emplace_back(other_lines.at(below(random, other_lines.size())));
    }
    return lines;
}

[[nodiscard]] std::string valid_frame(Random &random, std::string_view body)
{
    const std::string length = length_line(random, std::to_string(body.size()));
    return joined(header_around(random, length), line_end) + std::string{header_end} +
           std::string{body};
}

// Content-Length の値を value にした見出しの行の列。
[[nodiscard]] std::vector<std::string> with_length_value(Random &random, std::string_view value)
{
    return header_around(random, "Content-Length: " + std::string{value});
}

[[nodiscard]] std::string framed_lines(const std::vector<std::string> &lines, std::string_view body)
{
    return joined(lines, line_end) + std::string{header_end} + std::string{body};
}

[[nodiscard]] std::string without_length(Random &random, std::string_view body)
{
    std::vector<std::string> lines{std::string{other_lines.at(below(random, 2U))}};
    if (below(random, 2U) == 0)
    {
        lines.emplace_back(other_lines.at(2U + below(random, 2U)));
    }
    return framed_lines(lines, body);
}

[[nodiscard]] std::string with_separator(std::string_view body, std::string_view separator)
{
    const std::string number = std::to_string(body.size());
    return "Content-Type: x" + std::string{separator} + "Content-Length: " + number +
           std::string{separator} + std::string{separator} + std::string{body};
}

[[nodiscard]] std::string mutated_length(Random &random, Mutation mutation, std::string_view body)
{
    const std::size_t size = body.size();
    switch (mutation)
    {
    case Mutation::negative_length:
        return framed_lines(with_length_value(random, "-" + std::to_string(size)), body);
    case Mutation::not_a_number:
        return framed_lines(with_length_value(random, below(random, 2U) == 0 ? "1.5" : "0x10 "),
                            body);
    case Mutation::huge_length:
        return framed_lines(with_length_value(random, std::to_string(core::lsp_body_limit + 1U)),
                            body);
    case Mutation::overflowing_length:
        return framed_lines(with_length_value(random, "184467440737095516160"), body);
    case Mutation::longer_length:
        return framed_lines(
            with_length_value(random, std::to_string(size + 1U + below(random, 9U))), body);
    case Mutation::shorter_length:
        return framed_lines(with_length_value(random, std::to_string(size / 2U)), body);
    case Mutation::missing_length:
    case Mutation::duplicate_length:
    case Mutation::no_colon:
    case Mutation::empty_name:
    case Mutation::lf_only:
    case Mutation::cr_only:
    case Mutation::truncated_body:
        return valid_frame(random, body);
    }
    std::unreachable();
}

[[nodiscard]] std::string mutated_frame(Random &random, Mutation mutation, std::string_view body)
{
    const std::string number = std::to_string(body.size());
    switch (mutation)
    {
    case Mutation::missing_length:
        return without_length(random, body);
    case Mutation::duplicate_length:
        return framed_lines({length_line(random, number), length_line(random, number)}, body);
    case Mutation::no_colon:
        return framed_lines({"Garbage line", length_line(random, number)}, body);
    case Mutation::empty_name:
        return framed_lines({length_line(random, number), ": value"}, body);
    case Mutation::lf_only:
        return with_separator(body, "\n");
    case Mutation::cr_only:
        return with_separator(body, "\r");
    case Mutation::truncated_body:
    {
        const std::string whole = valid_frame(random, body);
        return whole.substr(0, whole.size() - 1U);
    }
    case Mutation::negative_length:
    case Mutation::not_a_number:
    case Mutation::huge_length:
    case Mutation::overflowing_length:
    case Mutation::longer_length:
    case Mutation::shorter_length:
        return mutated_length(random, mutation, body);
    }
    std::unreachable();
}

// 正しい枠だけの列（本文の列は模型を使わずに分かる）。末尾を切り詰めることもある。
[[nodiscard]] Stream valid_stream(Random &random)
{
    Bodies bodies;
    std::string bytes;
    const std::size_t frames = below(random, most_frames + 1U);
    for (std::size_t index = 0; index < frames; ++index)
    {
        bodies.push_back(random_body(random));
        bytes += valid_frame(random, bodies.back());
    }
    Stream stream{std::move(bytes), std::move(bodies)};
    if (frames > 0 && below(random, 3U) == 0)
    {
        const std::string tail = valid_frame(random, random_body(random));
        stream.first += tail.substr(0, below(random, tail.size()));
    }
    return stream;
}

// 正しい枠の間に変異した枠を 1〜2 つ混ぜた列（本文の列は模型が決める）。
[[nodiscard]] Stream mutated_stream(Random &random)
{
    Stream stream{std::string{}, std::nullopt};
    const std::size_t frames = 1U + below(random, most_frames);
    for (std::size_t index = 0; index < frames; ++index)
    {
        const std::string body = random_body(random);
        const bool mutate = below(random, 3U) == 0;
        stream.first +=
            mutate ? mutated_frame(random, mutations.at(below(random, mutations.size())), body)
                   : valid_frame(random, body);
    }
    return stream;
}

// --- 素朴な模型（ヘッダの注記の規則を 1 バイトずつ歩いて写す） ---

[[nodiscard]] std::optional<std::size_t> naive_find(std::string_view bytes, std::string_view mark,
                                                    std::size_t from)
{
    for (std::size_t at = from; at + mark.size() <= bytes.size(); ++at)
    {
        if (bytes.substr(at, mark.size()) == mark)
        {
            return at;
        }
    }
    return std::nullopt;
}

[[nodiscard]] char lower(char byte)
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

[[nodiscard]] bool names_length(std::string_view name)
{
    constexpr std::string_view wanted = "content-length";
    if (name.size() != wanted.size())
    {
        return false;
    }
    bool same = true;
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        same = same && lower(name.at(index)) == wanted.at(index);
    }
    return same;
}

[[nodiscard]] std::string_view stripped(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
    {
        value.remove_suffix(1);
    }
    return value;
}

using Length = std::expected<std::size_t, LspFrameFailure>;

// 数字だけの空でない列なら値（上限を超えたら body_too_long）、ほかは invalid_content_length。
[[nodiscard]] Length naive_length(std::string_view value)
{
    const std::string_view digits = stripped(value);
    if (digits.empty())
    {
        return std::unexpected(LspFrameFailure::invalid_content_length);
    }
    std::size_t length = 0;
    bool over = false;
    for (const char digit : digits)
    {
        if (digit < '0' || digit > '9')
        {
            return std::unexpected(LspFrameFailure::invalid_content_length);
        }
        over = over || length > core::lsp_body_limit;
        length = over ? length : length * 10U + static_cast<std::size_t>(digit - '0');
    }
    if (over || length > core::lsp_body_limit)
    {
        return std::unexpected(LspFrameFailure::body_too_long);
    }
    return length;
}

// 1 行を読む。Content-Length なら seen に値を置く。
[[nodiscard]] std::optional<LspFrameFailure> naive_line(std::string_view line,
                                                        std::optional<std::size_t> &seen)
{
    const std::optional<std::size_t> colon = naive_find(line, ":", 0);
    if (!colon.has_value() || colon.value() == 0)
    {
        return LspFrameFailure::malformed_header;
    }
    if (!names_length(line.substr(0, colon.value())))
    {
        return std::nullopt;
    }
    if (seen.has_value())
    {
        return LspFrameFailure::invalid_content_length;
    }
    const Length length = naive_length(line.substr(colon.value() + 1U));
    if (!length.has_value())
    {
        return length.error();
    }
    seen = length.value();
    return std::nullopt;
}

[[nodiscard]] Length naive_header(std::string_view header)
{
    std::optional<std::size_t> seen;
    std::size_t at = 0;
    bool more = true;
    while (more)
    {
        const std::optional<std::size_t> end = naive_find(header, line_end, at);
        const std::size_t stop = end.value_or(header.size());
        const std::optional<LspFrameFailure> failure =
            naive_line(header.substr(at, stop - at), seen);
        if (failure.has_value())
        {
            return std::unexpected(failure.value());
        }
        more = end.has_value();
        at = stop + line_end.size();
    }
    if (!seen.has_value())
    {
        return std::unexpected(LspFrameFailure::missing_content_length);
    }
    return seen.value();
}

// at からの 1 枠。値なしは「まだ揃っていない」、本文の終わりの位置を返す。
using NaiveStep = std::expected<std::optional<std::size_t>, LspFrameFailure>;

[[nodiscard]] NaiveStep naive_frame(std::string_view bytes, std::size_t at)
{
    const std::optional<std::size_t> end = naive_find(bytes, header_end, at);
    if (!end.has_value())
    {
        // 空行がまだ無い。届いた分＋少なくとも 1 バイトが見出しになる。
        if (bytes.size() - at + 1U > core::lsp_header_limit + header_end.size())
        {
            return std::unexpected(LspFrameFailure::header_too_long);
        }
        return std::nullopt;
    }
    if (end.value() - at > core::lsp_header_limit)
    {
        return std::unexpected(LspFrameFailure::header_too_long);
    }
    const Length length = naive_header(bytes.substr(at, end.value() - at));
    if (!length.has_value())
    {
        return std::unexpected(length.error());
    }
    const std::size_t begin = end.value() + header_end.size();
    if (bytes.size() - begin < length.value())
    {
        return std::nullopt;
    }
    return begin + length.value();
}

[[nodiscard]] Outcome naive_frames(std::string_view bytes)
{
    Outcome outcome{Bodies{}, 0U, std::nullopt};
    auto &[bodies, consumed, failure] = outcome;
    while (consumed < bytes.size())
    {
        const NaiveStep step = naive_frame(bytes, consumed);
        if (!step.has_value())
        {
            failure = step.error();
            return outcome;
        }
        if (!step.value().has_value())
        {
            return outcome;
        }
        const std::size_t finish = step.value().value_or(consumed);
        const std::optional<std::size_t> end = naive_find(bytes, header_end, consumed);
        const std::size_t begin = end.value_or(0U) + header_end.size();
        bodies.emplace_back(bytes.substr(begin, finish - begin));
        consumed = finish;
    }
    return outcome;
}

// --- 製品の答えを Outcome に揃える ---

[[nodiscard]] Outcome whole_frames(std::string_view bytes)
{
    const auto frames = core::lsp_frames(bytes);
    if (!frames.has_value())
    {
        return Outcome{Bodies{}, 0U, frames.error()};
    }
    Bodies bodies;
    for (const std::string_view body : frames.value().bodies)
    {
        bodies.emplace_back(body);
    }
    return Outcome{std::move(bodies), frames.value().consumed, std::nullopt};
}

// 一括の答えと模型の答えが同じ（失敗のとき製品は本文を返さないので、失敗の種類だけ比べる）。
[[nodiscard]] bool same_as_model(const Outcome &whole, const Outcome &model)
{
    if (std::get<2>(whole).has_value() || std::get<2>(model).has_value())
    {
        return std::get<2>(whole) == std::get<2>(model);
    }
    return whole == model;
}

// chunk の切れ目（昇順・重複あり）。0 はすべてを 1 度に、ほかは任意の位置。
[[nodiscard]] std::vector<std::size_t> cut_points(Random &random, std::size_t size,
                                                  std::size_t kind)
{
    std::vector<std::size_t> cuts;
    if (kind == 1U && size <= 256U)
    {
        for (std::size_t at = 1; at < size; ++at)
        {
            cuts.push_back(at);
        }
        return cuts;
    }
    const std::size_t count = kind == 0 ? 0U : below(random, 12U);
    for (std::size_t index = 0; index < count && size > 0; ++index)
    {
        cuts.push_back(below(random, size + 1U));
    }
    std::ranges::sort(cuts);
    return cuts;
}

// adapter の deliver と同じ手順で流す。失敗した読みの中の本文は積まない。
[[nodiscard]] std::pair<Outcome, std::string> fed_in_chunks(std::string_view bytes,
                                                            const std::vector<std::size_t> &cuts)
{
    Outcome outcome{Bodies{}, 0U, std::nullopt};
    std::string pending;
    std::size_t from = 0;
    for (std::size_t index = 0; index <= cuts.size(); ++index)
    {
        const std::size_t to = index < cuts.size() ? cuts.at(index) : bytes.size();
        pending.append(bytes.substr(from, to - from));
        from = to;
        const auto frames = core::lsp_frames(pending);
        if (!frames.has_value())
        {
            std::get<2>(outcome) = frames.error();
            return {outcome, std::string{}};
        }
        for (const std::string_view body : frames.value().bodies)
        {
            std::get<0>(outcome).emplace_back(body);
        }
        std::get<1>(outcome) += frames.value().consumed;
        pending.erase(0, frames.value().consumed);
    }
    return {outcome, pending};
}

[[nodiscard]] bool is_prefix(const Bodies &head, const Bodies &whole)
{
    if (head.size() > whole.size())
    {
        return false;
    }
    bool same = true;
    for (std::size_t index = 0; index < head.size(); ++index)
    {
        same = same && head.at(index) == whole.at(index);
    }
    return same;
}

// 分割して流した答えが一括と同じ（失敗なら種類が同じで、届いた本文は模型の失敗より前の頭）。
[[nodiscard]] bool same_in_chunks(std::string_view bytes, const std::vector<std::size_t> &cuts,
                                  const Outcome &model)
{
    const auto [fed, rest] = fed_in_chunks(bytes, cuts);
    if (std::get<2>(model).has_value())
    {
        return std::get<2>(fed) == std::get<2>(model) &&
               is_prefix(std::get<0>(fed), std::get<0>(model));
    }
    return !std::get<2>(fed).has_value() && std::get<0>(fed) == std::get<0>(model) &&
           std::get<1>(fed) == std::get<1>(model) && rest == bytes.substr(std::get<1>(model));
}

void print_bytes(const char *label, std::string_view input)
{
    const std::size_t shown = input.size() < 160U ? input.size() : 160U;
    std::fprintf(stderr, "    %s (%zu bytes):", label, input.size());
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

// 1 つの列の性質 1・2（と変異の無い列では生成した本文の列との一致）。
[[nodiscard]] bool stream_holds(Random &random, const Stream &stream)
{
    const Outcome model = naive_frames(stream.first);
    const Outcome whole = whole_frames(stream.first);
    bool holds = same_as_model(whole, model);
    if (stream.second.has_value())
    {
        holds =
            holds && !std::get<2>(whole).has_value() && std::get<0>(whole) == stream.second.value();
    }
    for (std::size_t kind = 0; kind < splits_per_stream; ++kind)
    {
        holds =
            same_in_chunks(stream.first, cut_points(random, stream.first.size(), kind), model) &&
            holds;
    }
    return holds;
}

[[nodiscard]] std::size_t broken_streams(Random &random, Stream (*make)(Random &),
                                         std::size_t cases, const char *family)
{
    std::size_t broken = 0;
    for (std::size_t index = 0; index < cases; ++index)
    {
        const Stream stream = make(random);
        const bool first = broken == 0;
        const bool holds = stream_holds(random, stream);
        broken += holds ? 0U : 1U;
        if (!holds && first)
        {
            std::fprintf(stderr, "  %s: case %zu\n", family, index);
            print_bytes("stream", stream.first);
        }
    }
    return broken;
}

void verify_valid_streams()
{
    Random random{129101U};
    expect(broken_streams(random, valid_stream, stream_cases, "valid") == 0,
           "valid frame streams split anywhere give the generated bodies and the model's answer");
}

void verify_mutated_streams()
{
    Random random{129102U};
    expect(broken_streams(random, mutated_stream, mutated_cases, "mutated") == 0,
           "mutated frame streams agree with the naive model whole and split anywhere");
}

// 変異 1 つの枠を先頭に置いた列が決めてある答え（失敗の種類・待つ）になるか。
[[nodiscard]] bool single_holds(Mutation mutation, const Outcome &whole, std::size_t size)
{
    const std::optional<LspFrameFailure> failure = std::get<2>(whole);
    const bool waits =
        !failure.has_value() && std::get<0>(whole).empty() && std::get<1>(whole) == 0;
    switch (mutation)
    {
    case Mutation::missing_length:
        return failure == LspFrameFailure::missing_content_length;
    case Mutation::duplicate_length:
    case Mutation::negative_length:
    case Mutation::not_a_number:
        return failure == LspFrameFailure::invalid_content_length;
    case Mutation::huge_length:
    case Mutation::overflowing_length:
        return failure == LspFrameFailure::body_too_long;
    case Mutation::no_colon:
    case Mutation::empty_name:
        return failure == LspFrameFailure::malformed_header;
    case Mutation::lf_only:
    case Mutation::cr_only:
        return size + 1U > core::lsp_header_limit + header_end.size()
                   ? failure == LspFrameFailure::header_too_long
                   : waits;
    case Mutation::truncated_body:
    case Mutation::longer_length:
        return waits;
    case Mutation::shorter_length:
        return !failure.has_value() && std::get<0>(whole).size() == 1U;
    }
    std::unreachable();
}

void verify_single_mutations()
{
    Random random{129103U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < single_cases; ++index)
    {
        const Mutation mutation = mutations.at(index % mutations.size());
        // 本文は見出しに似た列を含まない 1 文字以上（shorter_length
        // の残りは空行の来ない見出しとして 待つ・本文の任意のバイトは列の性質が覆う）。
        const std::string body(1U + below(random, 40U), 'b');
        const std::string bytes = mutated_frame(random, mutation, body);
        if (!single_holds(mutation, whole_frames(bytes), bytes.size()))
        {
            std::fprintf(stderr, "  single: case %zu mutation %zu\n", index,
                         static_cast<std::size_t>(mutation));
            print_bytes("frame", bytes);
            ++broken;
        }
    }
    expect(broken == 0, "a single mutated frame gives the documented closed failure or waits");
}

// 見出しの長さが上限ちょうどは受け、1 バイト超えは断る。CR だけ / LF だけの見出しは上限を超える
// まで待つ。どれも分割して流しても同じ。
[[nodiscard]] std::string padded_frame(std::size_t header_size, std::string_view body)
{
    const std::string length = "Content-Length: " + std::to_string(body.size()) + "\r\n";
    const std::string name = "X-Pad: ";
    const std::size_t pad = header_size - length.size() - name.size();
    return length + name + std::string(pad, 'p') + std::string{header_end} + std::string{body};
}

void verify_header_limit()
{
    Random random{129104U};
    bool all = true;
    for (std::size_t index = 0; index < limit_cases; ++index)
    {
        const std::string body = random_body(random);
        const std::string fits = padded_frame(core::lsp_header_limit, body);
        const std::string over = padded_frame(core::lsp_header_limit + 1U, body);
        const Outcome accepted = whole_frames(fits);
        all = all && !std::get<2>(accepted).has_value() && std::get<0>(accepted) == Bodies{body};
        all = all && std::get<2>(whole_frames(over)) == LspFrameFailure::header_too_long;
        const std::string lf(core::lsp_header_limit + 3U, '\n');
        const std::string lf_over = lf + "\n\n";
        all = all && std::get<2>(whole_frames(lf)) == std::nullopt;
        all = all && std::get<2>(whole_frames(lf_over)) == LspFrameFailure::header_too_long;
        all = stream_holds(random, Stream{fits + over, std::nullopt}) && all;
        all = stream_holds(random, Stream{fits + lf_over, std::nullopt}) && all;
    }
    expect(all,
           "the header limit accepts exactly 8 KiB, refuses one byte more and waits until then");
}

void verify_framed_round_trip()
{
    Random random{129105U};
    bool all = true;
    for (std::size_t index = 0; index < single_cases; ++index)
    {
        Bodies bodies;
        std::string bytes;
        const std::size_t count = below(random, most_frames + 1U);
        for (std::size_t frame = 0; frame < count; ++frame)
        {
            bodies.push_back(random_body(random));
            bytes += core::lsp_framed(bodies.back());
        }
        const Outcome whole = whole_frames(bytes);
        all = all && whole == Outcome{bodies, bytes.size(), std::nullopt};
    }
    expect(all, "lsp_framed bodies come back as the same bodies from lsp_frames");
}

// --- JSON-RPC の 1 通 ---

[[nodiscard]] std::string random_text(Random &random)
{
    constexpr std::array<std::string_view, 8> pieces{{"a", "Z", " ", "\"", "\\", "\n", "あ", "/"}};
    std::string text;
    const std::size_t count = below(random, 6U);
    for (std::size_t index = 0; index < count; ++index)
    {
        text += pieces.at(below(random, pieces.size()));
    }
    return text;
}

[[nodiscard]] std::int64_t random_integer(Random &random)
{
    const auto magnitude =
        static_cast<std::int64_t>(random() % static_cast<std::uint64_t>(exact_limit));
    return below(random, 2U) == 0 ? magnitude : -magnitude;
}

[[nodiscard]] JsonValue random_array(Random &random, std::size_t depth);
[[nodiscard]] JsonValue random_object(Random &random, std::size_t depth);

[[nodiscard]] JsonValue random_value(Random &random, std::size_t depth)
{
    const std::size_t kinds = depth >= 2U ? 5U : 7U;
    switch (below(random, kinds))
    {
    case 0:
        return JsonValue{JsonNull{}};
    case 1:
        return JsonValue{below(random, 2U) == 0};
    case 2:
        return JsonValue{core::json_integer(random_integer(random))};
    case 3:
        return JsonValue{core::json_number(below(random, 2U) == 0 ? 0.5 : -2.25)};
    case 4:
        return JsonValue{random_text(random)};
    case 5:
        return random_array(random, depth);
    }
    return random_object(random, depth);
}

[[nodiscard]] JsonValue random_array(Random &random, std::size_t depth)
{
    JsonArray items;
    const std::size_t count = below(random, 4U);
    for (std::size_t index = 0; index < count; ++index)
    {
        items.push_back(random_value(random, depth + 1U));
    }
    return JsonValue{std::move(items)};
}

[[nodiscard]] JsonValue random_object(Random &random, std::size_t depth)
{
    JsonObject members;
    const std::size_t count = below(random, 4U);
    for (std::size_t index = 0; index < count; ++index)
    {
        members.emplace_back("k" + std::to_string(index), random_value(random, depth + 1U));
    }
    return JsonValue{std::move(members)};
}

[[nodiscard]] LspId random_id(Random &random)
{
    if (below(random, 2U) == 0)
    {
        return LspId{random_integer(random)};
    }
    return LspId{random_text(random)};
}

[[nodiscard]] JsonValue id_json(const LspId &id)
{
    if (const auto *number = std::get_if<std::int64_t>(&id))
    {
        return JsonValue{core::json_integer(*number)};
    }
    return JsonValue{std::get<std::string>(id)};
}

// members を入れ替えて（読む側は順に依らない）、知らない鍵を足すこともある 1 通。
[[nodiscard]] std::string written(Random &random, JsonObject members)
{
    if (below(random, 3U) == 0)
    {
        members.emplace_back("extra", random_value(random, 1U));
    }
    for (std::size_t index = members.size(); index > 1U; --index)
    {
        std::swap(members.at(index - 1U), members.at(below(random, index)));
    }
    return core::write_json(JsonValue{std::move(members)});
}

[[nodiscard]] JsonObject envelope()
{
    JsonObject members;
    members.emplace_back("jsonrpc", JsonValue{std::string{"2.0"}});
    return members;
}

[[nodiscard]] Message random_call(Random &random, bool request)
{
    JsonObject members = envelope();
    const std::string method = "m/" + random_text(random);
    members.emplace_back("method", JsonValue{method});
    const JsonValue params =
        below(random, 3U) == 0 ? JsonValue{JsonNull{}} : random_value(random, 1U);
    if (!std::holds_alternative<JsonNull>(params.value))
    {
        members.emplace_back("params", params);
    }
    if (!request)
    {
        return {written(random, std::move(members)),
                LspIncoming{core::LspNotification{.method = method, .params = params}}};
    }
    const LspId id = random_id(random);
    members.emplace_back("id", id_json(id));
    return {written(random, std::move(members)),
            LspIncoming{core::LspRequest{.id = id, .method = method, .params = params}}};
}

[[nodiscard]] Message random_response(Random &random)
{
    JsonObject members = envelope();
    const LspId id = random_id(random);
    members.emplace_back("id", id_json(id));
    if (below(random, 3U) == 0)
    {
        const core::LspError error{.code = random_integer(random), .message = random_text(random)};
        JsonObject body;
        body.emplace_back("code", JsonValue{core::json_integer(error.code)});
        body.emplace_back("message", JsonValue{error.message});
        members.emplace_back("error", JsonValue{std::move(body)});
        return {written(random, std::move(members)),
                LspIncoming{core::LspResponse{.id = id, .result = std::unexpected(error)}}};
    }
    const JsonValue result =
        below(random, 4U) == 0 ? JsonValue{JsonNull{}} : random_value(random, 1U);
    if (!std::holds_alternative<JsonNull>(result.value) || below(random, 2U) == 0)
    {
        members.emplace_back("result", result);
    }
    return {written(random, std::move(members)),
            LspIncoming{core::LspResponse{.id = id, .result = result}}};
}

// 1 か所だけ壊した 1 通と、決めてある失敗。
[[nodiscard]] Message broken_message(Random &random)
{
    constexpr std::array<std::pair<std::string_view, LspMessageFailure>, 10> broken{{
        {R"([{"jsonrpc":"2.0","id":1}])", LspMessageFailure::not_object},
        {R"("2.0")", LspMessageFailure::not_object},
        {R"({"jsonrpc":"1.0","id":1,"result":null})", LspMessageFailure::not_json_rpc},
        {R"({"id":1,"method":"x"})", LspMessageFailure::not_json_rpc},
        {R"({"jsonrpc":2.0,"method":"x"})", LspMessageFailure::not_json_rpc},
        {R"({"jsonrpc":"2.0","method":5})", LspMessageFailure::invalid_method},
        {R"({"jsonrpc":"2.0","method":"x","id":1.5})", LspMessageFailure::invalid_id},
        {R"({"jsonrpc":"2.0","id":true,"result":1})", LspMessageFailure::invalid_id},
        {R"({"jsonrpc":"2.0","result":1})", LspMessageFailure::missing_id},
        {R"({"jsonrpc":"2.0","id":"a","error":{"code":1}})", LspMessageFailure::invalid_error},
    }};
    const auto &[text, failure] = broken.at(below(random, broken.size()));
    return {std::string{text}, std::unexpected(failure)};
}

[[nodiscard]] Message random_message(Random &random)
{
    switch (below(random, 5U))
    {
    case 0:
        return random_call(random, false);
    case 1:
        return random_call(random, true);
    case 2:
        return broken_message(random);
    }
    return random_response(random);
}

[[nodiscard]] bool same_result(const std::expected<JsonValue, core::LspError> &left,
                               const std::expected<JsonValue, core::LspError> &right)
{
    if (left.has_value() || right.has_value())
    {
        return left.has_value() && right.has_value() && left.value() == right.value();
    }
    return left.error().code == right.error().code && left.error().message == right.error().message;
}

[[nodiscard]] bool same_incoming(const LspIncoming &left, const LspIncoming &right)
{
    if (const auto *response = std::get_if<core::LspResponse>(&left))
    {
        const auto *other = std::get_if<core::LspResponse>(&right);
        return other != nullptr && response->id == other->id &&
               same_result(response->result, other->result);
    }
    if (const auto *note = std::get_if<core::LspNotification>(&left))
    {
        const auto *other = std::get_if<core::LspNotification>(&right);
        return other != nullptr && note->method == other->method && note->params == other->params;
    }
    const auto &request = std::get<core::LspRequest>(left);
    const auto *other = std::get_if<core::LspRequest>(&right);
    return other != nullptr && request.id == other->id && request.method == other->method &&
           request.params == other->params;
}

[[nodiscard]] bool read_as(std::string_view body, const Incoming &expected)
{
    const auto parsed = core::parse_json(body);
    if (!parsed.has_value())
    {
        return false;
    }
    const Incoming read = core::lsp_incoming(parsed.value());
    if (!read.has_value() || !expected.has_value())
    {
        return !read.has_value() && !expected.has_value() && read.error() == expected.error();
    }
    return same_incoming(read.value(), expected.value());
}

// 1 列の 1 通ずつを枠にして分割して流し、届いた本文をすべて読んで生成した型の列と比べる。
[[nodiscard]] bool messages_hold(Random &random)
{
    std::vector<Message> messages;
    std::string bytes;
    const std::size_t count = 1U + below(random, most_frames);
    for (std::size_t index = 0; index < count; ++index)
    {
        messages.push_back(random_message(random));
        bytes += valid_frame(random, messages.back().first);
    }
    const auto [fed, rest] = fed_in_chunks(bytes, cut_points(random, bytes.size(), 2U));
    bool holds =
        !std::get<2>(fed).has_value() && rest.empty() && std::get<0>(fed).size() == messages.size();
    for (std::size_t index = 0; holds && index < messages.size(); ++index)
    {
        holds = read_as(std::get<0>(fed).at(index), messages.at(index).second);
    }
    return holds;
}

void verify_messages()
{
    Random random{129106U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < message_cases; ++index)
    {
        if (!messages_hold(random))
        {
            std::fprintf(stderr, "  messages: case %zu\n", index);
            ++broken;
        }
    }
    expect(broken == 0, "JSON-RPC messages framed and split anywhere read back as the generated "
                        "types or the documented closed failure");
}

// Nib が送る本文（要求・通知）を受ける側で読むと、同じ id・method・params の型になる。
void verify_sent_bodies_read_back()
{
    Random random{129107U};
    bool all = true;
    for (std::size_t index = 0; index < message_cases; ++index)
    {
        const std::int64_t id = random_integer(random);
        const std::string method = "m/" + random_text(random);
        const JsonValue params =
            below(random, 3U) == 0 ? JsonValue{JsonNull{}} : random_value(random, 1U);
        const LspIncoming request{
            core::LspRequest{.id = LspId{id}, .method = method, .params = params}};
        const LspIncoming note{core::LspNotification{.method = method, .params = params}};
        all = read_as(core::lsp_request_body(id, method, params), request) && all;
        all = read_as(core::lsp_notification_body(method, params), note) && all;
    }
    expect(all, "bodies Nib writes read back as the same request and notification");
}

void verify_generators_are_deterministic()
{
    Random first{129108U};
    Random second{129108U};
    expect(mutated_stream(first).first == mutated_stream(second).first &&
               random_message(first).first == random_message(second).first,
           "the stream and message generators are deterministic for a fixed seed");
}
} // namespace

void verify_lsp_frames_property_scope()
{
    verify_generators_are_deterministic();
    verify_valid_streams();
    verify_mutated_streams();
    verify_single_mutations();
    verify_header_limit();
    verify_framed_round_trip();
    verify_messages();
    verify_sent_bodies_read_back();
}
} // namespace nenenib::tests
