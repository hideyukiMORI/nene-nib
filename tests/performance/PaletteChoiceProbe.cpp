#include "PaletteChoiceProbe.hpp"
#include "CommandChoice.hpp"
#include "ExCompletionProbe.hpp"
#include "Milestone.hpp"
#include "PaletteChoiceSample.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
namespace nenenib::tests::performance
{
namespace
{
constexpr std::array<PaletteChoiceSample, 9> samples{{
    {":", 512U, 0U, "builtin-all\n:\n512\nbuiltins\n"},
    {":", 128U, 256U,
     "custom-all\n:\n128\ncustom256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-"
     "0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-"
     "0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-"
     "0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-"
     "0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-"
     "0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-"
     "0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-"
     "0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-"
     "0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-"
     "0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-"
     "0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-"
     "0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-"
     "0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-"
     "0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-"
     "0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-"
     "0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-"
     "0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-"
     "0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-"
     "0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-"
     "0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-"
     "0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-"
     "0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-"
     "0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-"
     "0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-"
     "0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-"
     "0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-"
     "0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-"
     "0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-"
     "0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-"
     "0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-"
     "0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-"
     "0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-"
     "0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n"},
    {":?", 1024U, 0U, "miss\n:?\n1024\nbuiltins\n"},
    {":fz", 1024U, 0U, "short\n:fz\n1024\nbuiltins\n"},
    {":DRAC", 512U, 0U, "theme\n:DRAC\n512\nbuiltins\n"},
    {":set guifont=", 1024U, 0U, "direct-fill\n:set guifont=\n1024\nbuiltins\n"},
    {":set "
     "guifont="
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日AB:h18",
     1024U, 0U,
     "direct-long\n:set "
     "guifont="
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日AB:h18\n1024\nbuiltins\n"},
    {":colorscheme "
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日",
     512U, 0U,
     "fallback-long\n:colorscheme "
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日\n512\nbuiltins\n"},
    {":user-0123", 128U, 256U,
     "custom-narrow\n:user-0123\n128\ncustom256-invalid-color\nuser-0000\nuser-0001\nuser-"
     "0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-"
     "0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-"
     "0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-"
     "0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-"
     "0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-"
     "0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-"
     "0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-"
     "0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-"
     "0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-"
     "0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-"
     "0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-"
     "0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-"
     "0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-"
     "0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-"
     "0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-"
     "0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-"
     "0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-"
     "0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-"
     "0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-"
     "0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-"
     "0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-"
     "0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-"
     "0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-"
     "0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-"
     "0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-"
     "0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-"
     "0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-"
     "0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-"
     "0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-"
     "0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-"
     "0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-"
     "0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n"},
}};
[[nodiscard]] bool uses_custom_catalog(PaletteChoiceWorkload workload)
{
    switch (workload)
    {
    case PaletteChoiceWorkload::builtin_all:
    case PaletteChoiceWorkload::miss:
    case PaletteChoiceWorkload::short_query:
    case PaletteChoiceWorkload::theme:
    case PaletteChoiceWorkload::direct_fill:
    case PaletteChoiceWorkload::direct_long:
    case PaletteChoiceWorkload::fallback_long:
        return false;
    case PaletteChoiceWorkload::custom_all:
    case PaletteChoiceWorkload::custom_narrow:
        return true;
    }
    std::unreachable();
}
[[nodiscard]] core::ThemeCatalog catalog_of(PaletteChoiceWorkload workload)
{
    if (!uses_custom_catalog(workload))
    {
        return core::ThemeCatalog::builtins();
    }
    std::vector<core::ThemeRecord> records;
    for (const auto name : custom_theme_names())
    {
        records.push_back({core::ThemeName::parse(name).value(),
                           std::unexpected(core::ThemeFailure::invalid_color)});
    }
    return core::ThemeCatalog::from(std::move(records)).value();
}
// 空の問い合わせの並び（CommandChoice.cpp の before）: 点は実行文字列のバイト数、同点は文字列の順。
[[nodiscard]] std::vector<std::string> listed_order(std::vector<std::string> commands)
{
    std::ranges::sort(commands,
                      [](const std::string &left, const std::string &right)
                      {
                          if (left.size() != right.size())
                          {
                              return left.size() < right.size();
                          }
                          return left < right;
                      });
    return commands;
}
[[nodiscard]] std::vector<std::string> listed_all(PaletteChoiceWorkload workload)
{
    auto names = builtin_theme_names();
    if (uses_custom_catalog(workload))
    {
        const auto custom = custom_theme_names();
        names.insert(names.end(), custom.begin(), custom.end());
    }
    return listed_order(expected_ex_completions(std::nullopt, names));
}
// 期待の実行文字列。全件の 2 本は Ex の補完と同じ正本から作る。絞り込みの候補は、2026-10-19 の
// 製品（組み込み 125 テーマ・src/core/BuiltinThemes.hpp）で palette_choices が返す値（部分列の
// 当たりと点の順は CommandChoice.cpp の match_score と before）。打った値がそのまま候補になる 2 本
// （字体の直接指定と、当たらない colorscheme の値）は、行頭の「:」を除いた入力。
[[nodiscard]] std::vector<std::string> expected_commands(PaletteChoiceWorkload workload,
                                                         const PaletteChoiceSample &sample)
{
    switch (workload)
    {
    case PaletteChoiceWorkload::builtin_all:
    case PaletteChoiceWorkload::custom_all:
        return listed_all(workload);
    case PaletteChoiceWorkload::miss:
        return {};
    case PaletteChoiceWorkload::short_query:
        return {"set fontsize="};
    case PaletteChoiceWorkload::theme:
        return {"colorscheme dracula", "colorscheme desert-oracle", "colorscheme editorial-cafe"};
    case PaletteChoiceWorkload::direct_fill:
        return {"set guifont="};
    case PaletteChoiceWorkload::direct_long:
    case PaletteChoiceWorkload::fallback_long:
        return {std::string(sample.input.substr(1))};
    case PaletteChoiceWorkload::custom_narrow:
        return {"colorscheme user-0123"};
    }
    std::unreachable();
}
[[nodiscard]] std::size_t code_points_of(std::string_view text)
{
    return static_cast<std::size_t>(std::ranges::count_if(
        text, [](char unit) { return (static_cast<unsigned char>(unit) & 0xC0U) != 0x80U; }));
}
// 埋める候補は「set fontsize=」と「set guifont=」そのものだけ（CommandChoice.cpp の choice_of）。
[[nodiscard]] core::CommandChoiceKind kind_of(std::string_view command)
{
    return command == "set fontsize=" || command == "set guifont="
               ? core::CommandChoiceKind::fill
               : core::CommandChoiceKind::execute;
}
[[nodiscard]] bool catalog_equal(const core::ThemeCatalog &catalog,
                                 const PaletteChoiceSample &sample)
{
    const auto names = catalog.names();
    const auto records = catalog.records();
    const auto builtins = builtin_theme_names();
    const auto custom = custom_theme_names();
    if (records.size() != sample.catalog_records ||
        names.size() != builtins.size() + sample.catalog_records ||
        !std::ranges::equal(std::span{names}.first(builtins.size()), builtins))
    {
        return false;
    }
    if (!records.empty() && records.size() != custom.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const auto &record = records[index];
        if (record.name.text() != custom[index] || record.value.has_value() ||
            record.value.error() != core::ThemeFailure::invalid_color ||
            names.at(builtins.size() + index) != custom[index])
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] bool line_equal(const core::CommandLine &line, const PaletteChoiceSample &sample)
{
    return line.text() == sample.input && line.caret().value == sample.input.size() &&
           !line.completion_index().has_value() && catalog_equal(line.catalog(), sample);
}
[[nodiscard]] bool choice_equal(const core::CommandChoice &choice, std::string_view expected)
{
    return !choice.detail.has_value() && choice.command == expected &&
           choice.label.text() == expected &&
           choice.label.code_point_count() == code_points_of(expected) &&
           choice.kind == kind_of(expected) && !choice.origin.has_value() &&
           !choice.operation.has_value() && choice.key.empty();
}
[[nodiscard]] bool choices_equal(std::span<const core::CommandChoice> choices,
                                 std::span<const std::string> expected)
{
    if (choices.size() != expected.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < choices.size(); ++index)
    {
        if (!choice_equal(choices[index], expected[index]))
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] std::vector<std::string_view> held_views(std::span<const core::CommandChoice> choices)
{
    std::vector<std::string_view> views;
    for (const auto &choice : choices)
    {
        views.emplace_back(choice.command);
        views.emplace_back(choice.label.text());
    }
    return views;
}
[[nodiscard]] bool views_equal(std::span<const std::string_view> views,
                               std::span<const std::string> expected)
{
    if (views.size() != expected.size() * 2U)
    {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (views[index * 2U] != expected[index] || views[index * 2U + 1U] != expected[index])
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] std::uint64_t choice_checksum(std::span<const core::CommandChoice> choices)
{
    std::uint64_t checksum = 1U + choices.size();
    for (const auto &choice : choices)
    {
        checksum +=
            choice.command.size() + choice.label.text().size() + choice.label.code_point_count();
    }
    return checksum;
}
// choice_checksum と同じ足し算を期待の実行文字列から（実行文字列と表示は同じ文字列）。
[[nodiscard]] std::uint64_t expected_checksum(std::span<const std::string> expected)
{
    std::uint64_t checksum = 1U + expected.size();
    for (const auto &command : expected)
    {
        checksum += command.size() * 2U + code_points_of(command);
    }
    return checksum;
}
[[nodiscard]] std::expected<std::vector<core::CommandChoice>, ProbeFailure>
detached_choices(PaletteChoiceWorkload workload, const PaletteChoiceSample &sample)
{
    const auto local_catalog = catalog_of(workload);
    const auto inserted = core::CommandLine::empty(local_catalog).inserted(sample.input);
    if (!inserted.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return core::palette_choices(inserted.value());
}
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
pure_probe(PaletteChoiceWorkload workload, const PaletteChoiceSample &sample,
           application::TimingPort &timing)
{
    const auto catalog = catalog_of(workload);
    const auto expected = expected_commands(workload, sample);
    const auto inserted = core::CommandLine::empty(catalog).inserted(sample.input);
    if (!inserted.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &line = inserted.value();
    const auto original_text = line.text();
    const auto held = core::palette_choices(line);
    const auto views = held_views(held);
    const auto detached = detached_choices(workload, sample);
    if (!detached.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    if (!line_equal(line, sample) || !catalog_equal(catalog, sample) ||
        !choices_equal(held, expected) || !views_equal(views, expected) ||
        !choices_equal(detached.value(), expected))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < sample.rounds; ++index)
    {
        const auto result = core::palette_choices(line);
        checksum += choice_checksum(result);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.rounds * expected_checksum(expected) ||
        !choices_equal(core::palette_choices(line), expected) || !choices_equal(held, expected) ||
        !views_equal(views, expected) || !choices_equal(detached.value(), expected) ||
        !line_equal(line, sample) || original_text != sample.input ||
        !catalog_equal(catalog, sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace
std::string palette_choice_input(PaletteChoiceWorkload workload)
{
    return std::string(samples.at(static_cast<std::size_t>(workload)).metadata);
}
std::expected<std::uint64_t, ProbeFailure> probed_palette_choice(PaletteChoiceWorkload workload,
                                                                 const std::string &input,
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
