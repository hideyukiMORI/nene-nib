#include "ProbeSelection.hpp"

#include "Utf16.hpp"
#include "Win32TimingAdapter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
// `--expectations` が流さない workload と理由（#1604）。CTest の nib_probe_expectations を Debug
// （ASan 付き）で 60 秒に収めるため、Debug の exe で warmup 1 + sample 1 の 1 回が 2 秒を超えた
// workload を名指しで外す（理由の秒は 2026-11-13 の作業木の実行の壁時計で、判定には使わない）。
// 外した workload の期待は `eng/check.ps1 -Full` の `eng/probe-smoke.ps1` が見る（ADR 0082 の #690
// の追補）。workload を足すときは、Debug で 2 秒を超えるならここに 1 行足す。
using SkippedWorkload = std::pair<std::string_view, std::string_view>;
constexpr std::array<SkippedWorkload, 73> skipped_workloads{{
    {"controller-open-utf8-16mib", "Debug 3.5 s"},
    {"buffer-from-utf8-16mib", "Debug 3.3 s"},
    {"controller-insert-200-after-delete-16mib", "Debug 7.9 s"},
    {"utf8-validate-ascii-16mib", "Debug 3.3 s"},
    {"controller-vim-insert-200-register-16mib", "Debug 7.0 s"},
    {"controller-vim-record-insert-2000", "Debug 5.5 s"},
    {"buffer-line-text-scattered-crlf-4096", "Debug 5.5 s"},
    {"buffer-line-text-scattered-lf-4096", "Debug 6.6 s"},
    {"buffer-position-scattered-utf8-57344", "Debug 6.0 s"},
    {"codepage-to-utf8-cp932-japanese-16mib", "Debug 7.7 s"},
    {"utf16-to-utf8-japanese-8m-units", "Debug 9.1 s"},
    {"utf16-to-utf8-ascii-8m-units", "Debug 5.7 s"},
    {"utf16-to-utf8-supplementary-8m-units", "Debug 7.4 s"},
    {"controller-save-utf8-16mib", "Debug 3.8 s"},
    {"controller-save-utf8-bom-16mib", "Debug 3.7 s"},
    {"buffer-erase-scattered-head-4096", "Debug 6.3 s"},
    {"buffer-erase-scattered-middle-4096", "Debug 6.7 s"},
    {"buffer-erase-scattered-tail-4096", "Debug 5.9 s"},
    {"buffer-erase-scattered-all-4096", "Debug 5.4 s"},
    {"buffer-offset-scattered-middle-57344", "Debug 6.4 s"},
    {"command-input-palette-fill-long-1024", "Debug 3.5 s"},
    {"palette-choice-builtin-all-512", "Debug 4.6 s"},
    {"palette-choice-custom-all-128", "Debug 4.1 s"},
    {"palette-choice-miss-1024", "Debug 2.8 s"},
    {"palette-choice-short-1024", "Debug 3.1 s"},
    {"palette-choice-fallback-long-512", "Debug 3.1 s"},
    {"text-object-tag-unclosed-4096", "Debug 2.1 s"},
    {"markdown-parse-mixed-16mib", "Debug 20.4 s"},
    {"markdown-parse-mixed-4mib", "Debug 5.0 s"},
    {"newline-index-payload-all-lf-16mib", "Debug 5.0 s"},
    {"newline-index-lengths-all-lf-16mib", "Debug 5.7 s"},
    {"vim-block-insert-giant-1line", "Debug 2.8 s"},
    {"vim-block-replace-giant-1line", "Debug 2.6 s"},
    {"vim-block-put-giant-1line", "Debug 2.6 s"},
    {"vim-block-repeat-giant-1line", "Debug 2.6 s"},
    {"vim-insert-ctrl-w-giant-1line", "Debug 2.6 s"},
    {"vim-count-h-giant-1line", "Debug 3.4 s"},
    {"vim-count-l-giant-1line", "Debug 3.5 s"},
    {"ordinary-word-boundary-giant-1line", "Debug 2.6 s"},
    {"text-object-iw-solid-1line-16mib", "Debug 127.3 s"},
    {"text-object-aw-solid-1line-16mib", "Debug 132.0 s"},
    {"text-object-iw-spaced-1line-16mib", "Debug 2.7 s"},
    {"text-object-aw-spaced-1line-16mib", "Debug 3.3 s"},
    {"text-object-iquote-1line-16mib", "Debug 9.7 s"},
    {"text-object-iparen-1line-16mib", "Debug 16.1 s"},
    {"vim-lh-after-1000-edits", "Debug 7.3 s"},
    {"vim-lh-after-4000-edits", "Debug 23.7 s"},
    {"vim-lh-after-16000-edits", "Debug 125.3 s"},
    {"vim-x-after-1000-edits", "Debug 12.5 s"},
    {"vim-x-after-4000-edits", "Debug 36.0 s"},
    {"vim-x-after-16000-edits", "Debug 109.1 s"},
    {"piece-read-after-4000-pieces", "Debug 2.4 s"},
    {"piece-read-after-16000-pieces", "Debug 32.8 s"},
    {"piece-replace-after-4000-pieces", "Debug 2.6 s"},
    {"piece-replace-after-16000-pieces", "Debug 33.2 s"},
    {"replace-all-100000-after-1-pieces", "Debug 7.1 s"},
    {"replace-all-100000-after-8000-pieces", "Debug 15.9 s"},
    {"frame-after-1-pieces", "Debug 3.7 s"},
    {"frame-after-16000-pieces", "Debug 108.0 s"},
    {"vim-substitute-percent-g-100k-lines", "Debug 10.7 s"},
    {"vim-substitute-no-match-100k-lines", "Debug 4.1 s"},
    {"pattern-cost-star-tail-256-16000-chars-line", "Debug 3.8 s"},
    {"pattern-cost-star-tail-1024-16000-chars-line", "Debug 13.2 s"},
    {"pattern-cost-quantity-1000-16000-chars-line", "Debug 13.5 s"},
    {"pattern-cost-star-tail-64-2000-lines-40-chars", "Debug 4.8 s"},
    {"pattern-cost-star-tail-256-2000-lines-40-chars", "Debug 17.1 s"},
    {"pattern-cost-star-tail-1024-2000-lines-40-chars", "Debug 66.5 s"},
    {"pattern-cost-quantity-1000-2000-lines-40-chars", "Debug 66.7 s"},
    {"pattern-group-bounded-miss-4096", "Debug 31.5 s"},
    {"vim-shift-n-1mib-line-end", "Debug 2.7 s"},
    {"vim-gn-1mib-line-end", "Debug 3.3 s"},
    {"vim-dgn-1mib-line-end", "Debug 4.7 s"},
    {"vim-dg-shift-n-1mib-line-end", "Debug 4.3 s"},
}};

[[nodiscard]] bool skipped(std::string_view name) noexcept
{
    return std::ranges::any_of(skipped_workloads,
                               [name](const SkippedWorkload &row) { return row.first == name; });
}

// 外す表の名前が表から消えた・理由が空の行の数（表が黙って古くならないように落とす）。
[[nodiscard]] std::size_t unknown_skips()
{
    std::size_t unknown = 0;
    for (const auto &[name, reason] : skipped_workloads)
    {
        if (!nenenib::tests::performance::workload_of(name).has_value() || reason.empty())
        {
            ++unknown;
            std::cout << "unknown skipped workload " << name << '\n';
        }
    }
    return unknown;
}

// 全 workload の warmup の 1 回だけを流し、結果が表の期待と合うかを見る（#1604）。節目の置き場を
// bind しないので時刻を書かず、回数も時間も比べない。合わない workload を 1 行ずつ書き、1 つでも
// あれば 1 を返す。
[[nodiscard]] int checked_expectations()
{
    namespace probe = nenenib::tests::performance;
    nenenib::adapters::win32::Win32TimingAdapter timing;
    std::size_t checked = 0;
    std::size_t differing = unknown_skips();
    for (const std::string_view name : probe::workload_names())
    {
        if (skipped(name))
        {
            std::cout << "skip " << name << '\n';
            continue;
        }
        const auto workload = probe::workload_of(name);
        ++checked;
        if (!workload.has_value() ||
            !probe::run_workload(workload.value(), probe::input_of(workload.value()), timing)
                 .has_value())
        {
            ++differing;
            std::cout << "differs " << name << '\n';
        }
    }
    std::cout << "Probe expectations: " << checked << " workloads checked, " << differing
              << " differ; " << skipped_workloads.size() << " skipped\n";
    return differing == 0 ? 0 : 1;
}
[[nodiscard]] std::optional<std::size_t> iterations_of(std::string_view text) noexcept
{
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 ||
        value > 2048)
    {
        return std::nullopt;
    }
    return value;
}

void report(std::string_view workload, std::size_t iterations, std::string_view input,
            std::uint64_t checksum)
{
    std::cout << "{\"schema\":1,\"workload\":\"" << workload << "\",\"iterations\":" << iterations
              << ",\"warmup\":1,\"inputBytes\":" << input.size()
              << ",\"inputHashAlgorithm\":\"fnv1a64\",\"inputHash\":\""
              << nenenib::tests::performance::checksum_of(input) << "\",\"outputChecksum\":\""
              << checksum << "\",\"commit\":\"" << NIB_PROBE_COMMIT
              << "\",\"dirty\":" << NIB_PROBE_DIRTY << ",\"compilerVersion\":\""
              << NIB_PROBE_COMPILER << "\",\"configuration\":\"" << NIB_PROBE_CONFIGURATION
              << "\",\"toolchainSha256\":\"" << NIB_PROBE_TOOLCHAIN_HASH << "\"}\n";
}
} // namespace

// argv: workload / iterations / private timing output. No window, clock or file API here.
int main(int argc, char **argv)
{
    namespace probe = nenenib::tests::performance;
    if (argc == 2 && std::string_view{argv[1]} == "--expectations")
    {
        return checked_expectations();
    }
    if (argc != 4)
    {
        std::cerr << "Probe: specify workload, iterations (1..2048), timing output\n";
        return 2;
    }
    const auto workload = probe::workload_of(argv[1]);
    const auto iterations = iterations_of(argv[2]);
    const auto path = nenenib::core::to_utf16(argv[3]);
    if (!workload.has_value() || !iterations.has_value() || !path.has_value() ||
        path.value().empty())
    {
        std::cerr << "Probe: invalid workload, iterations or UTF-8 output path\n";
        return 2;
    }
    const std::string input = probe::input_of(workload.value());
    nenenib::adapters::win32::Win32TimingAdapter timing;
    const auto warmup = probe::run_workload(workload.value(), input, timing);
    if (!warmup.has_value())
    {
        std::cerr << "Probe: warmup result differs from the fixed workload\n";
        return 1;
    }
    timing.bind(path.value());
    for (std::size_t iteration = 0; iteration < iterations.value(); ++iteration)
    {
        const auto result = probe::run_workload(workload.value(), input, timing);
        if (!result.has_value() || result.value() != warmup.value())
        {
            std::cerr << "Probe: observed result differs from warmup\n";
            return 1;
        }
    }
    report(argv[1], iterations.value(), input, warmup.value());
    return 0;
}
