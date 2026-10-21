// Markdown プレビューの差し替えの費用の probe（#818・ADR 0155 の決定 7）。CTest に登録しない。
// 決定的に作った Markdown を parse_markdown で読み、PreviewTextLayouts ＋ PreviewBlockTops::measure
// で錨から 1 枠ぶん測る。製品の PreviewPane は通さない（nib_preview_capture と同じ土台）。
// 時刻は Win32TimingAdapter の節目だけで取る（ARC-007）: parse と measure をそれぞれ
// probe_started / probe_finished の 1 対で挟み、節目の
// JSON（<json>.timing.json）を読み戻して差を書く。
//
//   nib_preview_probe <bytes> <head|middle|tail> <json の出力先>
#include "PreviewProbeOutcome.hpp"
#include "Sha256.hpp"

#include "DevicePixels.hpp"
#include "EditorSettings.hpp"
#include "FontSize.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Milestone.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "Utf16.hpp"
#include "Win32TimingAdapter.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
using nenenib::tests::preview::PreviewProbeOutcome;

constexpr std::size_t subsections = 4U;
constexpr std::size_t paragraphs = 3U;
constexpr std::int32_t viewport_width = 900;
constexpr std::int32_t viewport_height = 640;
constexpr std::uint32_t viewport_dpi = 120;
constexpr std::size_t tail_distance = 10;
constexpr std::size_t timing_marks = 4;
constexpr std::int64_t microseconds_per_millisecond = 1000;
constexpr std::array<std::string_view, 3> anchors{"head", "middle", "tail"};

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

// tests/performance/MarkdownProbe.cpp の markdown_input と同じ規則（共有できない理由:
// preview_tests は performance_tests に依存できない・eng/architecture.json）。節を bytes まで
// 繰り返し、bytes で切って最後の改行まで。16 MiB で同じバイト列になる。
[[nodiscard]] std::string markdown_text(std::size_t bytes)
{
    std::string text;
    text.reserve(bytes + 4096U);
    for (std::size_t section = 0; text.size() < bytes; ++section)
    {
        text += std::format("# Section {}\n\n", section);
        for (std::size_t subsection = 0; subsection < subsections; ++subsection)
        {
            append_subsection(text, section, subsection);
        }
    }
    text.resize(bytes);
    text.resize(text.rfind('\n') + 1U);
    return text;
}

[[nodiscard]] std::optional<std::size_t> size_of(std::string_view text)
{
    std::size_t value = 0;
    const auto *end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end && value > 0 ? std::optional<std::size_t>{value}
                                                            : std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> anchor_of(std::string_view name)
{
    for (std::size_t index = 0; index < anchors.size(); ++index)
    {
        if (anchors[index] == name)
        {
            return index;
        }
    }
    return std::nullopt;
}

// head は 0、middle はブロックの数の半分、tail は最後の 10 個前（足りなければ 0）。
[[nodiscard]] std::size_t first_block(std::size_t anchor, std::size_t blocks)
{
    if (anchor == 0)
    {
        return 0;
    }
    if (anchor == 1)
    {
        return blocks / 2;
    }
    return blocks > tail_distance ? blocks - tail_distance : 0;
}

[[nodiscard]] float pixels_of(std::int32_t dips)
{
    return static_cast<float>(core::to_pixels(dips, viewport_dpi));
}

// 節目を積むのはこの関数の中だけ。戻るときに timing のデストラクタが節目の JSON を書く。
[[nodiscard]] std::optional<PreviewProbeOutcome>
timed_run(const std::string &text, std::size_t anchor, const std::wstring &timing_path)
{
    nenenib::adapters::win32::Win32TimingAdapter timing;
    timing.bind(timing_path);
    timing.mark(core::Milestone::probe_started);
    const auto document = core::parse_markdown(text, core::MarkdownStop::never());
    timing.mark(core::Milestone::probe_finished);
    if (!document)
    {
        return std::nullopt;
    }
    const auto blocks = document.value().blocks.size();
    const auto first = first_block(anchor, blocks);
    const win32::PreviewTextStyle style{
        std::string(core::default_editor_settings().font_family.text()),
        core::FontSize::default_points, viewport_dpi};
    win32::PreviewTextLayouts layouts;
    timing.mark(core::Milestone::probe_started);
    const auto tops = win32::PreviewBlockTops::measure(
        document.value(), layouts, style,
        win32::PreviewViewport{first, pixels_of(viewport_width), pixels_of(viewport_height)});
    timing.mark(core::Milestone::probe_finished);
    if (!tops)
    {
        return std::nullopt;
    }
    return PreviewProbeOutcome{blocks, first, tops.value().size() - tops.value().first(),
                               layouts.created()};
}

[[nodiscard]] std::optional<std::string> read_text(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// 節目の JSON の qpcMicroseconds を順に読む。
[[nodiscard]] std::vector<std::int64_t> mark_times(std::string_view report)
{
    constexpr std::string_view key = "\"qpcMicroseconds\": ";
    std::vector<std::int64_t> times;
    for (auto at = report.find(key); at != std::string_view::npos; at = report.find(key, at))
    {
        at += key.size();
        std::int64_t value = 0;
        const auto [stop, error] =
            std::from_chars(report.data() + at, report.data() + report.size(), value);
        if (error != std::errc{})
        {
            return {};
        }
        times.push_back(value);
        at = static_cast<std::size_t>(stop - report.data());
    }
    return times;
}

[[nodiscard]] std::string milliseconds_text(std::int64_t microseconds)
{
    return std::format("{}.{:03d}", microseconds / microseconds_per_millisecond,
                       microseconds % microseconds_per_millisecond);
}

[[nodiscard]] std::string report_of(const std::string &text, std::string_view anchor,
                                    const PreviewProbeOutcome &outcome,
                                    std::span<const std::int64_t> times)
{
    return std::format(
        "{{\"schema\":1,\"probe\":\"nib_preview_probe\",\"bytes\":{},\"sha256\":\"{}\","
        "\"blocks\":{},\"anchor\":\"{}\",\"first\":{},\"measured\":{},\"parseMs\":{},"
        "\"measureMs\":{},\"layoutsCreated\":{},\"viewport\":{{\"widthDip\":{},\"heightDip\":{},"
        "\"dpi\":{}}},\"commit\":\"{}\",\"dirty\":{},\"configuration\":\"{}\"}}\n",
        text.size(), nenenib::tests::preview::sha256_hex(text), outcome.blocks, anchor,
        outcome.first, outcome.measured, milliseconds_text(times[1] - times[0]),
        milliseconds_text(times[3] - times[2]), outcome.layouts_created, viewport_width,
        viewport_height, viewport_dpi, NIB_PROBE_COMMIT, NIB_PROBE_DIRTY, NIB_PROBE_CONFIGURATION);
}

[[nodiscard]] int probe(std::size_t bytes, std::size_t anchor, const std::string &output)
{
    const std::string timing_path = output + ".timing.json";
    const auto wide = core::to_utf16(timing_path);
    const std::string text = markdown_text(bytes);
    const auto outcome = wide ? timed_run(text, anchor, wide.value()) : std::nullopt;
    const auto timing = read_text(timing_path);
    const auto times = timing ? mark_times(timing.value()) : std::vector<std::int64_t>{};
    if (!outcome || times.size() != timing_marks)
    {
        std::fputs("preview probe: parsing, measuring or the timing file failed\n", stderr);
        return 1;
    }
    std::ofstream file(output, std::ios::binary);
    file << report_of(text, anchors[anchor], outcome.value(), times);
    if (!file)
    {
        std::fputs("preview probe: the output could not be written\n", stderr);
        return 1;
    }
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    const auto arguments = std::span(argv, static_cast<std::size_t>(argc));
    const auto bytes = arguments.size() == 4 ? size_of(arguments[1]) : std::nullopt;
    const auto anchor = arguments.size() == 4 ? anchor_of(arguments[2]) : std::nullopt;
    if (!bytes || !anchor)
    {
        std::fputs("usage: nib_preview_probe <bytes> <head|middle|tail> <json>\n", stderr);
        return 2;
    }
    return probe(bytes.value(), anchor.value(), arguments[3]);
}
