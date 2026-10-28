// scope `--lsp-frames` の契約（#994・ADR 0160 の決定 3・ADR 0042）。core の lsp_frames に 1 通・
// 2 通・途中・見出しだけ・上限・ごみを当て、adapter と同じ「consumed だけ捨てて続きを繋ぐ」読み方で
// どこで切って渡しても同じ本文の列になることを確かめる。
#include "LspFrameFailure.hpp"
#include "LspFrames.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::LspFrameFailure;
using core::LspFrames;
using Framed = std::expected<LspFrames, LspFrameFailure>;
using Refused = std::pair<std::string_view, LspFrameFailure>;

constexpr std::string_view one = "Content-Length: 5\r\n\r\nhello";
constexpr std::string_view two = "Content-Length: 2\r\n\r\n{}"
                                 "content-length:3\r\nContent-Type: application/vscode-jsonrpc; "
                                 "charset=utf-8\r\n\r\n[1]";

// 断る見出しと閉じた失敗（LspFrameFailure.hpp の一覧のどれにも 1 つ以上）。
constexpr std::array<Refused, 12> refused{{
    {"Content-Length : 5\r\n\r\nhello", LspFrameFailure::missing_content_length},
    {"Content-Length: +5\r\n\r\nhello", LspFrameFailure::invalid_content_length},
    {"hello\r\n\r\n", LspFrameFailure::malformed_header},
    {"\r\n\r\n", LspFrameFailure::malformed_header},
    {": 5\r\n\r\nhello", LspFrameFailure::malformed_header},
    {"Content-Type: x\r\n\r\n", LspFrameFailure::missing_content_length},
    {"Content-Length: abc\r\n\r\n", LspFrameFailure::invalid_content_length},
    {"Content-Length: -1\r\n\r\n", LspFrameFailure::invalid_content_length},
    {"Content-Length:\r\n\r\n", LspFrameFailure::invalid_content_length},
    {"Content-Length: 1\r\nContent-Length: 1\r\n\r\nx", LspFrameFailure::invalid_content_length},
    {"Content-Length: 16777217\r\n\r\n", LspFrameFailure::body_too_long},
    {"Content-Length: 99999999999999999999999\r\n\r\n", LspFrameFailure::body_too_long},
}};

[[nodiscard]] std::vector<std::string> bodies_of(const Framed &framed)
{
    std::vector<std::string> bodies;
    if (framed.has_value())
    {
        for (const std::string_view body : framed.value().bodies)
        {
            bodies.emplace_back(body);
        }
    }
    return bodies;
}

void verify_whole_frames()
{
    const Framed single = core::lsp_frames(one);
    expect(bodies_of(single) == std::vector<std::string>{"hello"}, "one frame yields its body");
    expect(single.has_value() && single.value().consumed == one.size(),
           "one whole frame is consumed");
    const Framed pair = core::lsp_frames(two);
    expect(
        bodies_of(pair) == std::vector<std::string>{"{}", "[1]"},
        "two frames yield both bodies; the name is case-insensitive and Content-Type is skipped");
    expect(pair.has_value() && pair.value().consumed == two.size(),
           "two whole frames are consumed");
    const Framed empty = core::lsp_frames("Content-Length: 0\r\n\r\n");
    expect(bodies_of(empty) == std::vector<std::string>{""}, "a zero-length body is a frame");
    const Framed padded = core::lsp_frames("Content-Length: \t 2 \r\n\r\nok");
    expect(bodies_of(padded) == std::vector<std::string>{"ok"},
           "blanks around the length are ignored");
}

void verify_partial_frames()
{
    const Framed nothing = core::lsp_frames("");
    expect(nothing.has_value() && nothing.value().bodies.empty() && nothing.value().consumed == 0,
           "no bytes yield no frame");
    const Framed header = core::lsp_frames("Content-Len");
    expect(header.has_value() && header.value().bodies.empty() && header.value().consumed == 0,
           "a partial header waits for more bytes");
    const Framed header_only = core::lsp_frames("Content-Length: 5\r\n\r\n");
    expect(header_only.has_value() && header_only.value().bodies.empty() &&
               header_only.value().consumed == 0,
           "a header without its body waits for the body");
    const std::string first_and_part = std::string{one} + "Content-Length: 4\r\n\r\nab";
    const Framed part = core::lsp_frames(first_and_part);
    expect(bodies_of(part) == std::vector<std::string>{"hello"},
           "a whole frame before a partial one is returned");
    expect(part.has_value() && part.value().consumed == one.size(),
           "consumed stops at the end of the last whole frame");
}

void verify_limits()
{
    const Framed largest = core::lsp_frames("Content-Length: 16777216\r\n\r\n");
    expect(largest.has_value() && largest.value().bodies.empty(),
           "a 16 MiB body is accepted and waited for");
    const std::string endless(core::lsp_header_limit + 1U, 'x');
    const Framed garbage = core::lsp_frames(endless);
    expect(!garbage.has_value() && garbage.error() == LspFrameFailure::header_too_long,
           "a header without its blank line past 8 KiB is refused");
    const Framed bare_lf = core::lsp_frames("Content-Length: 5\n\nhello");
    expect(bare_lf.has_value() && bare_lf.value().bodies.empty() && bare_lf.value().consumed == 0,
           "a header ended by LF alone is not a frame and waits for CRLF");
    const std::string lf_garbage =
        "Content-Length: 5\n\n" + std::string(core::lsp_header_limit, 'x');
    const Framed lf_refused = core::lsp_frames(lf_garbage);
    expect(!lf_refused.has_value() && lf_refused.error() == LspFrameFailure::header_too_long,
           "LF-only headers are refused once they pass 8 KiB");
    const std::string waiting(core::lsp_header_limit, 'x');
    expect(core::lsp_frames(waiting).has_value(), "a header within 8 KiB still waits");
    for (const auto &[bytes, failure] : refused)
    {
        const Framed framed = core::lsp_frames(bytes);
        expect(!framed.has_value() && framed.error() == failure,
               "a broken header is refused with its closed failure");
    }
    const std::string after_good = std::string{one} + "garbage\r\n\r\n";
    const Framed late = core::lsp_frames(after_good);
    expect(!late.has_value() && late.error() == LspFrameFailure::malformed_header,
           "garbage after a whole frame is refused");
}

// adapter の読み方: 揃った枠を取り、consumed だけ捨て、残りに続きを繋ぐ。
[[nodiscard]] std::vector<std::string> read_in_two(std::string_view bytes, std::size_t split)
{
    std::vector<std::string> bodies;
    std::string pending{bytes.substr(0, split)};
    for (const std::string_view more : {std::string_view{}, bytes.substr(split)})
    {
        pending.append(more);
        const Framed framed = core::lsp_frames(pending);
        const std::vector<std::string> found = bodies_of(framed);
        bodies.insert(bodies.end(), found.begin(), found.end());
        pending.erase(0, framed.has_value() ? framed.value().consumed : pending.size());
    }
    return bodies;
}

void verify_every_split()
{
    const std::string stream = std::string{two} + std::string{one};
    const std::vector<std::string> expected{"{}", "[1]", "hello"};
    bool all = true;
    for (std::size_t split = 0; split <= stream.size(); ++split)
    {
        all = read_in_two(stream, split) == expected && all;
    }
    expect(all, "reading the stream split at any byte yields the same bodies");
}
} // namespace

void verify_lsp_frames_scope()
{
    verify_whole_frames();
    verify_partial_frames();
    verify_limits();
    verify_every_split();
}
} // namespace nenenib::tests
