#include "CompletionPrefixProbe.hpp"
#include "CommandLine.hpp"
#include "CompletionPrefixSample.hpp"
#include "ExCompletionProbe.hpp"
#include "Milestone.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
namespace nenenib::tests::performance
{
namespace
{
constexpr std::array<std::string_view, 9> builtin_names{
    "ubuntu-aubergine", "neutral-light", "solarized-dark", "solarized-light", "monokai",
    "dracula",          "one-dark",      "night-owl",      "night-owl-light"};
constexpr std::array<std::string_view, 0> expected_0{};
constexpr std::array<std::string_view, 1> expected_1{"colorscheme dracula"};
constexpr std::array<std::string_view, 1> expected_2{"colorscheme dracula"};
constexpr std::array<std::string_view, 1> expected_3{"set fontsize="};
constexpr std::array<std::string_view, 1> expected_4{"set fontsize="};
constexpr std::array<std::string_view, 28> expected_5{"colorscheme",
                                                      "set fontsize=",
                                                      "set guifont=",
                                                      "set incsearch",
                                                      "set noincsearch",
                                                      "set hlsearch",
                                                      "set nohlsearch",
                                                      "set guide",
                                                      "set noguide",
                                                      "tabs",
                                                      "tabnew",
                                                      "tabnext",
                                                      "tabprevious",
                                                      "tabclose",
                                                      "set wrap",
                                                      "set nowrap",
                                                      "edit",
                                                      "buffer",
                                                      "ls",
                                                      "files",
                                                      "buffers",
                                                      "write",
                                                      "quit",
                                                      "quit!",
                                                      "wq",
                                                      "xit",
                                                      "exit",
                                                      "saveas"};
constexpr std::array<std::string_view, 28> expected_6{"colorscheme",
                                                      "set fontsize=",
                                                      "set guifont=",
                                                      "set incsearch",
                                                      "set noincsearch",
                                                      "set hlsearch",
                                                      "set nohlsearch",
                                                      "set guide",
                                                      "set noguide",
                                                      "tabs",
                                                      "tabnew",
                                                      "tabnext",
                                                      "tabprevious",
                                                      "tabclose",
                                                      "set wrap",
                                                      "set nowrap",
                                                      "edit",
                                                      "buffer",
                                                      "ls",
                                                      "files",
                                                      "buffers",
                                                      "write",
                                                      "quit",
                                                      "quit!",
                                                      "wq",
                                                      "xit",
                                                      "exit",
                                                      "saveas"};
constexpr std::array<CompletionPrefixSample, 7> samples{{
    {"edit "
     "C:\\nib-"
     "probe\\日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日Z",
     "edit "
     "C:\\nib-"
     "probe\\日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日Z",
     256U, std::nullopt, expected_0,
     "mode=raw\ninput=edit "
     "C:\\nib-"
     "probe\\日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日Z\nrounds="
     "4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n",
     4096U},
    {"colorscheme drac", "colorscheme drac", 16U, std::nullopt, expected_1,
     "mode=raw\ninput=colorscheme "
     "drac\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-"
     "dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n",
     86016U},
    {"colorscheme drac", "colorscheme dracula", 19U, 0U, expected_2,
     "mode=tab-next\ninput=colorscheme "
     "drac\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-"
     "dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n",
     86016U},
    {"set f", "set f", 5U, std::nullopt, expected_3,
     "mode=raw\ninput=set "
     "f\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n",
     61440U},
    {"set f", "set fontsize=", 13U, 0U, expected_4,
     "mode=tab-next\ninput=set "
     "f\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n",
     61440U},
    {"", "", 0U, std::nullopt, expected_5,
     "mode=raw\ninput=\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-"
     "dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n",
     1007616U},
    {"", "colorscheme", 11U, 0U, expected_6,
     "mode=tab-next\ninput=\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-"
     "light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-"
     "light\n",
     1007616U},
}};
[[nodiscard]] bool catalog_equal(const core::ThemeCatalog &catalog)
{
    return catalog.records().empty() && std::ranges::equal(catalog.names(), builtin_names);
}
[[nodiscard]] core::CommandLine prepared_line(CompletionPrefixWorkload workload,
                                              const core::CommandLine &raw)
{
    switch (workload)
    {
    case CompletionPrefixWorkload::long_miss:
    case CompletionPrefixWorkload::theme_raw:
    case CompletionPrefixWorkload::short_raw:
    case CompletionPrefixWorkload::empty_raw:
        return raw;
    case CompletionPrefixWorkload::theme_seeded:
    case CompletionPrefixWorkload::short_seeded:
    case CompletionPrefixWorkload::empty_seeded:
        return raw.edited(core::CommandEdit::complete_next);
    }
    std::unreachable();
}
[[nodiscard]] bool raw_equal(const core::CommandLine &raw, const CompletionPrefixSample &sample)
{
    return raw.text() == sample.input && raw.caret().value == sample.input.size() &&
           !raw.completion_index().has_value() && catalog_equal(raw.catalog());
}
[[nodiscard]] bool prepared_equal(const core::CommandLine &line,
                                  const CompletionPrefixSample &sample)
{
    return line.text() == sample.text && line.caret().value == sample.caret &&
           line.completion_index() == sample.completion_index && catalog_equal(line.catalog());
}
[[nodiscard]] bool result_equal(std::span<const std::string> result,
                                const CompletionPrefixSample &sample)
{
    return std::ranges::equal(result, sample.expected);
}
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
pure_probe(CompletionPrefixWorkload workload, const CompletionPrefixSample &sample,
           application::TimingPort &timing)
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto names = catalog.names();
    const auto inserted = core::CommandLine::empty(catalog).inserted(sample.input);
    if (!inserted.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &raw = inserted.value();
    const auto line = prepared_line(workload, raw);
    const auto raw_text = raw.text();
    const auto prepared_text = line.text();
    const auto held = line.completions();
    std::vector<std::string_view> views;
    for (const auto &candidate : held)
    {
        views.emplace_back(candidate);
    }
    if (!raw_equal(raw, sample) || !prepared_equal(line, sample) || !catalog_equal(catalog) ||
        !result_equal(held, sample) || !std::ranges::equal(views, sample.expected))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < 4096U; ++index)
    {
        const auto result = line.completions();
        checksum += completion_checksum(result);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || !result_equal(line.completions(), sample) ||
        !result_equal(held, sample) || !std::ranges::equal(views, sample.expected) ||
        !raw_equal(raw, sample) || !prepared_equal(line, sample) || raw_text != sample.input ||
        prepared_text != sample.text || names != catalog.names() || !catalog_equal(catalog))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace
std::string completion_prefix_input(CompletionPrefixWorkload workload)
{
    return std::string(samples.at(static_cast<std::size_t>(workload)).metadata);
}
std::expected<std::uint64_t, ProbeFailure>
probed_completion_prefix(CompletionPrefixWorkload workload, const std::string &input,
                         application::TimingPort &timing)
{
    const auto &sample = samples.at(static_cast<std::size_t>(workload));
    if (input != sample.metadata)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return pure_probe(workload, sample, timing);
}
} // namespace nenenib::tests::performance
