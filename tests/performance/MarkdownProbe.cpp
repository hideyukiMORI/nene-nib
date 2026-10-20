#include "MarkdownProbe.hpp"

#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Milestone.hpp"
#include "ProbeWorkload.hpp"

#include <cstddef>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t markdown_bytes = std::size_t{16U} << 20U;
constexpr std::size_t subsections = 4U;
constexpr std::size_t paragraphs = 3U;
// 1 節の leaf は見出し 1 と、小節ごとに見出し 1・段落 3・項目 4・番号 3・引用 1・コード 2・線 1。
constexpr std::size_t section_blocks = 1U + (subsections * 15U);

void append_subsection(std::string &text, std::size_t section, std::size_t subsection)
{
    text += std::format("## Subsection {}.{}\n\n", section, subsection);
    for (std::size_t paragraph = 0; paragraph < paragraphs; ++paragraph)
    {
        text += std::format("This is paragraph {} with *emphasis*, **strong text**, `inline code` "
                            "and a [link](https://example.com/{}/{}/{}).\n",
                            paragraph, section, subsection, paragraph);
        text += "Second line of the paragraph continues the sentence with plain words to make "
                "it longer.\n";
        text += "Third line has an autolink <https://example.org/x> and some more "
                "_underscored emphasis_ here.\n\n";
    }
    text += "- first item with **bold**\n- second item with `code`\n  - nested item with "
            "[ref](/a/b)\n- third item\n\n";
    text += "1. ordered one\n2. ordered two\n3. ordered three\n\n";
    text += "> A block quote line with *emphasis*.\n> Another quoted line.\n\n";
    text += "```c\nint main(void) {\n    return 0;\n}\n```\n\n";
    text += "    indented code line one\n    indented code line two\n\n";
    text += "---\n\n";
}
} // namespace

// md4c の probe の gen.py の (i) と同じ: 節を 16 MiB まで繰り返し、16 MiB で切って最後の改行まで。
std::string markdown_input(MarkdownWorkload workload)
{
    static_cast<void>(workload);
    std::string text;
    text.reserve(markdown_bytes + 4096U);
    for (std::size_t section = 0; text.size() < markdown_bytes; ++section)
    {
        text += std::format("# Section {}\n\n", section);
        for (std::size_t subsection = 0; subsection < subsections; ++subsection)
        {
            append_subsection(text, section, subsection);
        }
    }
    text.resize(markdown_bytes);
    text.resize(text.rfind('\n') + 1U);
    return text;
}

// 打ち切りの述語は製品の worker と同じく印を読む値を貸す（印は立たない）。
std::expected<std::uint64_t, ProbeFailure> parsed_markdown(MarkdownWorkload workload,
                                                           const std::string &input,
                                                           application::TimingPort &timing)
{
    if (input != markdown_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    bool cancelled = false;
    const auto ask = [&cancelled] { return cancelled; };
    std::cerr << "preparedBytes=" << input.size() << " preparedHash=" << checksum_of(input) << '\n';
    timing.mark(core::Milestone::probe_started);
    const auto parsed = core::parse_markdown(input, core::MarkdownStop::asking(ask));
    timing.mark(core::Milestone::probe_finished);
    if (!parsed.has_value() || parsed.value().blocks.size() < section_blocks)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = checksum_of(input) + parsed.value().blocks.size();
    for (const auto &block : parsed.value().blocks)
    {
        checksum += block.text.size() + block.spans.size() + block.source_begin.value;
    }
    std::cerr << "blocks=" << parsed.value().blocks.size() << '\n';
    return checksum;
}
} // namespace nenenib::tests::performance
