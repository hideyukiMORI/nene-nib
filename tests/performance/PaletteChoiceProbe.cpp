#include "PaletteChoiceProbe.hpp"
#include "CommandChoice.hpp"
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
constexpr std::array<std::string_view, 9> builtin_names{
    "ubuntu-aubergine", "neutral-light", "solarized-dark", "solarized-light", "monokai",
    "dracula",          "one-dark",      "night-owl",      "night-owl-light"};
constexpr std::array<std::string_view, 256> custom_names{
    "user-0000", "user-0001", "user-0002", "user-0003", "user-0004", "user-0005", "user-0006",
    "user-0007", "user-0008", "user-0009", "user-0010", "user-0011", "user-0012", "user-0013",
    "user-0014", "user-0015", "user-0016", "user-0017", "user-0018", "user-0019", "user-0020",
    "user-0021", "user-0022", "user-0023", "user-0024", "user-0025", "user-0026", "user-0027",
    "user-0028", "user-0029", "user-0030", "user-0031", "user-0032", "user-0033", "user-0034",
    "user-0035", "user-0036", "user-0037", "user-0038", "user-0039", "user-0040", "user-0041",
    "user-0042", "user-0043", "user-0044", "user-0045", "user-0046", "user-0047", "user-0048",
    "user-0049", "user-0050", "user-0051", "user-0052", "user-0053", "user-0054", "user-0055",
    "user-0056", "user-0057", "user-0058", "user-0059", "user-0060", "user-0061", "user-0062",
    "user-0063", "user-0064", "user-0065", "user-0066", "user-0067", "user-0068", "user-0069",
    "user-0070", "user-0071", "user-0072", "user-0073", "user-0074", "user-0075", "user-0076",
    "user-0077", "user-0078", "user-0079", "user-0080", "user-0081", "user-0082", "user-0083",
    "user-0084", "user-0085", "user-0086", "user-0087", "user-0088", "user-0089", "user-0090",
    "user-0091", "user-0092", "user-0093", "user-0094", "user-0095", "user-0096", "user-0097",
    "user-0098", "user-0099", "user-0100", "user-0101", "user-0102", "user-0103", "user-0104",
    "user-0105", "user-0106", "user-0107", "user-0108", "user-0109", "user-0110", "user-0111",
    "user-0112", "user-0113", "user-0114", "user-0115", "user-0116", "user-0117", "user-0118",
    "user-0119", "user-0120", "user-0121", "user-0122", "user-0123", "user-0124", "user-0125",
    "user-0126", "user-0127", "user-0128", "user-0129", "user-0130", "user-0131", "user-0132",
    "user-0133", "user-0134", "user-0135", "user-0136", "user-0137", "user-0138", "user-0139",
    "user-0140", "user-0141", "user-0142", "user-0143", "user-0144", "user-0145", "user-0146",
    "user-0147", "user-0148", "user-0149", "user-0150", "user-0151", "user-0152", "user-0153",
    "user-0154", "user-0155", "user-0156", "user-0157", "user-0158", "user-0159", "user-0160",
    "user-0161", "user-0162", "user-0163", "user-0164", "user-0165", "user-0166", "user-0167",
    "user-0168", "user-0169", "user-0170", "user-0171", "user-0172", "user-0173", "user-0174",
    "user-0175", "user-0176", "user-0177", "user-0178", "user-0179", "user-0180", "user-0181",
    "user-0182", "user-0183", "user-0184", "user-0185", "user-0186", "user-0187", "user-0188",
    "user-0189", "user-0190", "user-0191", "user-0192", "user-0193", "user-0194", "user-0195",
    "user-0196", "user-0197", "user-0198", "user-0199", "user-0200", "user-0201", "user-0202",
    "user-0203", "user-0204", "user-0205", "user-0206", "user-0207", "user-0208", "user-0209",
    "user-0210", "user-0211", "user-0212", "user-0213", "user-0214", "user-0215", "user-0216",
    "user-0217", "user-0218", "user-0219", "user-0220", "user-0221", "user-0222", "user-0223",
    "user-0224", "user-0225", "user-0226", "user-0227", "user-0228", "user-0229", "user-0230",
    "user-0231", "user-0232", "user-0233", "user-0234", "user-0235", "user-0236", "user-0237",
    "user-0238", "user-0239", "user-0240", "user-0241", "user-0242", "user-0243", "user-0244",
    "user-0245", "user-0246", "user-0247", "user-0248", "user-0249", "user-0250", "user-0251",
    "user-0252", "user-0253", "user-0254", "user-0255"};
constexpr std::array<PaletteChoiceExpected, 36> expected_0{{
    {"ls", "ls", 2U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"wq", "wq", 2U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"xit", "xit", 3U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"edit", "edit", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"exit", "exit", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"quit", "quit", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"tabs", "tabs", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"files", "files", 5U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"quit!", "quit!", 5U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"write", "write", 5U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"buffer", "buffer", 6U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"saveas", "saveas", 6U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"tabnew", "tabnew", 6U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"buffers", "buffers", 7U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"tabnext", "tabnext", 7U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"tabclose", "tabclose", 8U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"set guide", "set guide", 9U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"colorscheme", "colorscheme", 11U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set noguide", "set noguide", 11U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"tabprevious", "tabprevious", 11U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set guifont=", "set guifont=", 12U, core::CommandChoiceKind::fill, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"set hlsearch", "set hlsearch", 12U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set fontsize=", "set fontsize=", 13U, core::CommandChoiceKind::fill, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set incsearch", "set incsearch", 13U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set nohlsearch", "set nohlsearch", 14U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set noincsearch", "set noincsearch", 15U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"colorscheme system", "colorscheme system", 18U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme dracula", "colorscheme dracula", 19U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme monokai", "colorscheme monokai", 19U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme one-dark", "colorscheme one-dark", 20U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme night-owl", "colorscheme night-owl", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme neutral-light", "colorscheme neutral-light", 25U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme solarized-dark", "colorscheme solarized-dark", 26U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme night-owl-light", "colorscheme night-owl-light", 27U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme solarized-light", "colorscheme solarized-light", 27U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme ubuntu-aubergine", "colorscheme ubuntu-aubergine", 28U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 292> expected_1{{
    {"ls", "ls", 2U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"wq", "wq", 2U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"xit", "xit", 3U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"edit", "edit", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"exit", "exit", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"quit", "quit", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"tabs", "tabs", 4U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt,
     ""},
    {"files", "files", 5U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"quit!", "quit!", 5U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"write", "write", 5U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"buffer", "buffer", 6U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"saveas", "saveas", 6U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"tabnew", "tabnew", 6U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"buffers", "buffers", 7U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"tabnext", "tabnext", 7U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"tabclose", "tabclose", 8U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"set guide", "set guide", 9U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"colorscheme", "colorscheme", 11U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set noguide", "set noguide", 11U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"tabprevious", "tabprevious", 11U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set guifont=", "set guifont=", 12U, core::CommandChoiceKind::fill, std::nullopt, std::nullopt,
     std::nullopt, ""},
    {"set hlsearch", "set hlsearch", 12U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set fontsize=", "set fontsize=", 13U, core::CommandChoiceKind::fill, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set incsearch", "set incsearch", 13U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set nohlsearch", "set nohlsearch", 14U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"set noincsearch", "set noincsearch", 15U, core::CommandChoiceKind::execute, std::nullopt,
     std::nullopt, std::nullopt, ""},
    {"colorscheme system", "colorscheme system", 18U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme dracula", "colorscheme dracula", 19U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme monokai", "colorscheme monokai", 19U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme one-dark", "colorscheme one-dark", 20U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme night-owl", "colorscheme night-owl", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0000", "colorscheme user-0000", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0001", "colorscheme user-0001", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0002", "colorscheme user-0002", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0003", "colorscheme user-0003", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0004", "colorscheme user-0004", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0005", "colorscheme user-0005", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0006", "colorscheme user-0006", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0007", "colorscheme user-0007", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0008", "colorscheme user-0008", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0009", "colorscheme user-0009", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0010", "colorscheme user-0010", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0011", "colorscheme user-0011", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0012", "colorscheme user-0012", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0013", "colorscheme user-0013", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0014", "colorscheme user-0014", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0015", "colorscheme user-0015", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0016", "colorscheme user-0016", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0017", "colorscheme user-0017", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0018", "colorscheme user-0018", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0019", "colorscheme user-0019", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0020", "colorscheme user-0020", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0021", "colorscheme user-0021", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0022", "colorscheme user-0022", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0023", "colorscheme user-0023", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0024", "colorscheme user-0024", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0025", "colorscheme user-0025", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0026", "colorscheme user-0026", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0027", "colorscheme user-0027", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0028", "colorscheme user-0028", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0029", "colorscheme user-0029", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0030", "colorscheme user-0030", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0031", "colorscheme user-0031", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0032", "colorscheme user-0032", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0033", "colorscheme user-0033", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0034", "colorscheme user-0034", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0035", "colorscheme user-0035", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0036", "colorscheme user-0036", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0037", "colorscheme user-0037", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0038", "colorscheme user-0038", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0039", "colorscheme user-0039", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0040", "colorscheme user-0040", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0041", "colorscheme user-0041", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0042", "colorscheme user-0042", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0043", "colorscheme user-0043", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0044", "colorscheme user-0044", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0045", "colorscheme user-0045", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0046", "colorscheme user-0046", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0047", "colorscheme user-0047", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0048", "colorscheme user-0048", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0049", "colorscheme user-0049", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0050", "colorscheme user-0050", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0051", "colorscheme user-0051", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0052", "colorscheme user-0052", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0053", "colorscheme user-0053", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0054", "colorscheme user-0054", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0055", "colorscheme user-0055", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0056", "colorscheme user-0056", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0057", "colorscheme user-0057", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0058", "colorscheme user-0058", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0059", "colorscheme user-0059", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0060", "colorscheme user-0060", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0061", "colorscheme user-0061", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0062", "colorscheme user-0062", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0063", "colorscheme user-0063", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0064", "colorscheme user-0064", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0065", "colorscheme user-0065", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0066", "colorscheme user-0066", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0067", "colorscheme user-0067", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0068", "colorscheme user-0068", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0069", "colorscheme user-0069", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0070", "colorscheme user-0070", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0071", "colorscheme user-0071", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0072", "colorscheme user-0072", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0073", "colorscheme user-0073", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0074", "colorscheme user-0074", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0075", "colorscheme user-0075", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0076", "colorscheme user-0076", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0077", "colorscheme user-0077", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0078", "colorscheme user-0078", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0079", "colorscheme user-0079", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0080", "colorscheme user-0080", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0081", "colorscheme user-0081", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0082", "colorscheme user-0082", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0083", "colorscheme user-0083", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0084", "colorscheme user-0084", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0085", "colorscheme user-0085", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0086", "colorscheme user-0086", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0087", "colorscheme user-0087", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0088", "colorscheme user-0088", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0089", "colorscheme user-0089", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0090", "colorscheme user-0090", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0091", "colorscheme user-0091", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0092", "colorscheme user-0092", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0093", "colorscheme user-0093", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0094", "colorscheme user-0094", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0095", "colorscheme user-0095", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0096", "colorscheme user-0096", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0097", "colorscheme user-0097", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0098", "colorscheme user-0098", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0099", "colorscheme user-0099", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0100", "colorscheme user-0100", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0101", "colorscheme user-0101", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0102", "colorscheme user-0102", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0103", "colorscheme user-0103", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0104", "colorscheme user-0104", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0105", "colorscheme user-0105", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0106", "colorscheme user-0106", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0107", "colorscheme user-0107", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0108", "colorscheme user-0108", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0109", "colorscheme user-0109", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0110", "colorscheme user-0110", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0111", "colorscheme user-0111", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0112", "colorscheme user-0112", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0113", "colorscheme user-0113", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0114", "colorscheme user-0114", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0115", "colorscheme user-0115", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0116", "colorscheme user-0116", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0117", "colorscheme user-0117", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0118", "colorscheme user-0118", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0119", "colorscheme user-0119", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0120", "colorscheme user-0120", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0121", "colorscheme user-0121", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0122", "colorscheme user-0122", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0123", "colorscheme user-0123", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0124", "colorscheme user-0124", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0125", "colorscheme user-0125", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0126", "colorscheme user-0126", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0127", "colorscheme user-0127", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0128", "colorscheme user-0128", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0129", "colorscheme user-0129", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0130", "colorscheme user-0130", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0131", "colorscheme user-0131", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0132", "colorscheme user-0132", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0133", "colorscheme user-0133", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0134", "colorscheme user-0134", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0135", "colorscheme user-0135", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0136", "colorscheme user-0136", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0137", "colorscheme user-0137", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0138", "colorscheme user-0138", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0139", "colorscheme user-0139", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0140", "colorscheme user-0140", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0141", "colorscheme user-0141", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0142", "colorscheme user-0142", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0143", "colorscheme user-0143", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0144", "colorscheme user-0144", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0145", "colorscheme user-0145", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0146", "colorscheme user-0146", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0147", "colorscheme user-0147", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0148", "colorscheme user-0148", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0149", "colorscheme user-0149", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0150", "colorscheme user-0150", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0151", "colorscheme user-0151", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0152", "colorscheme user-0152", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0153", "colorscheme user-0153", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0154", "colorscheme user-0154", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0155", "colorscheme user-0155", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0156", "colorscheme user-0156", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0157", "colorscheme user-0157", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0158", "colorscheme user-0158", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0159", "colorscheme user-0159", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0160", "colorscheme user-0160", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0161", "colorscheme user-0161", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0162", "colorscheme user-0162", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0163", "colorscheme user-0163", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0164", "colorscheme user-0164", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0165", "colorscheme user-0165", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0166", "colorscheme user-0166", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0167", "colorscheme user-0167", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0168", "colorscheme user-0168", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0169", "colorscheme user-0169", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0170", "colorscheme user-0170", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0171", "colorscheme user-0171", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0172", "colorscheme user-0172", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0173", "colorscheme user-0173", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0174", "colorscheme user-0174", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0175", "colorscheme user-0175", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0176", "colorscheme user-0176", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0177", "colorscheme user-0177", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0178", "colorscheme user-0178", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0179", "colorscheme user-0179", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0180", "colorscheme user-0180", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0181", "colorscheme user-0181", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0182", "colorscheme user-0182", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0183", "colorscheme user-0183", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0184", "colorscheme user-0184", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0185", "colorscheme user-0185", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0186", "colorscheme user-0186", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0187", "colorscheme user-0187", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0188", "colorscheme user-0188", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0189", "colorscheme user-0189", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0190", "colorscheme user-0190", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0191", "colorscheme user-0191", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0192", "colorscheme user-0192", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0193", "colorscheme user-0193", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0194", "colorscheme user-0194", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0195", "colorscheme user-0195", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0196", "colorscheme user-0196", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0197", "colorscheme user-0197", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0198", "colorscheme user-0198", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0199", "colorscheme user-0199", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0200", "colorscheme user-0200", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0201", "colorscheme user-0201", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0202", "colorscheme user-0202", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0203", "colorscheme user-0203", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0204", "colorscheme user-0204", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0205", "colorscheme user-0205", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0206", "colorscheme user-0206", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0207", "colorscheme user-0207", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0208", "colorscheme user-0208", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0209", "colorscheme user-0209", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0210", "colorscheme user-0210", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0211", "colorscheme user-0211", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0212", "colorscheme user-0212", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0213", "colorscheme user-0213", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0214", "colorscheme user-0214", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0215", "colorscheme user-0215", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0216", "colorscheme user-0216", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0217", "colorscheme user-0217", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0218", "colorscheme user-0218", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0219", "colorscheme user-0219", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0220", "colorscheme user-0220", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0221", "colorscheme user-0221", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0222", "colorscheme user-0222", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0223", "colorscheme user-0223", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0224", "colorscheme user-0224", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0225", "colorscheme user-0225", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0226", "colorscheme user-0226", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0227", "colorscheme user-0227", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0228", "colorscheme user-0228", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0229", "colorscheme user-0229", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0230", "colorscheme user-0230", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0231", "colorscheme user-0231", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0232", "colorscheme user-0232", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0233", "colorscheme user-0233", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0234", "colorscheme user-0234", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0235", "colorscheme user-0235", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0236", "colorscheme user-0236", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0237", "colorscheme user-0237", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0238", "colorscheme user-0238", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0239", "colorscheme user-0239", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0240", "colorscheme user-0240", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0241", "colorscheme user-0241", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0242", "colorscheme user-0242", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0243", "colorscheme user-0243", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0244", "colorscheme user-0244", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0245", "colorscheme user-0245", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0246", "colorscheme user-0246", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0247", "colorscheme user-0247", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0248", "colorscheme user-0248", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0249", "colorscheme user-0249", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0250", "colorscheme user-0250", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0251", "colorscheme user-0251", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0252", "colorscheme user-0252", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0253", "colorscheme user-0253", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0254", "colorscheme user-0254", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme user-0255", "colorscheme user-0255", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme neutral-light", "colorscheme neutral-light", 25U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme solarized-dark", "colorscheme solarized-dark", 26U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme night-owl-light", "colorscheme night-owl-light", 27U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme solarized-light", "colorscheme solarized-light", 27U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
    {"colorscheme ubuntu-aubergine", "colorscheme ubuntu-aubergine", 28U,
     core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 0> expected_2{{}};
constexpr std::array<PaletteChoiceExpected, 1> expected_3{{
    {"set fontsize=", "set fontsize=", 13U, core::CommandChoiceKind::fill, std::nullopt,
     std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 1> expected_4{{
    {"colorscheme dracula", "colorscheme dracula", 19U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 1> expected_5{{
    {"set guifont=", "set guifont=", 12U, core::CommandChoiceKind::fill, std::nullopt, std::nullopt,
     std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 1> expected_6{{
    {"set "
     "guifont="
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日AB:h18",
     "set "
     "guifont="
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日AB:h18",
     97U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 1> expected_7{{
    {"colorscheme "
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日",
     "colorscheme "
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日",
     93U, core::CommandChoiceKind::execute, std::nullopt, std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceExpected, 1> expected_8{{
    {"colorscheme user-0123", "colorscheme user-0123", 21U, core::CommandChoiceKind::execute,
     std::nullopt, std::nullopt, std::nullopt, ""},
}};
constexpr std::array<PaletteChoiceSample, 9> samples{{
    {":", 512U, 677888U, expected_0, 0U,
     "builtin-all\n:\n512\nbuiltins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n"},
    {":", 128U, 2266624U, expected_1, 256U,
     "custom-all\n:\n128\ncustom256-invalid-color\nubuntu-aubergine\nneutral-light\nsolarized-"
     "dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nuser-"
     "0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-"
     "0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-"
     "0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-"
     "0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-"
     "0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-"
     "0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-"
     "0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-"
     "0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-"
     "0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-"
     "0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-"
     "0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-"
     "0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-"
     "0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-"
     "0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-"
     "0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-"
     "0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-"
     "0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-"
     "0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-"
     "0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-"
     "0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-"
     "0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-"
     "0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-"
     "0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-"
     "0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-"
     "0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-"
     "0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-"
     "0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-"
     "0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-"
     "0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-"
     "0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-"
     "0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-"
     "0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n"},
    {":?", 1024U, 1024U, expected_2, 0U,
     "miss\n:?\n1024\nbuiltins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n"},
    {":fz", 1024U, 41984U, expected_3, 0U,
     "short\n:fz\n1024\nbuiltins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n"},
    {":DRAC", 512U, 30208U, expected_4, 0U,
     "theme\n:DRAC\n512\nbuiltins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n"},
    {":set guifont=", 1024U, 38912U, expected_5, 0U,
     "direct-fill\n:set "
     "guifont=\n1024\nbuiltins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n"},
    {":set "
     "guifont="
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日AB:h18",
     1024U, 623616U, expected_6, 0U,
     "direct-long\n:set "
     "guifont="
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日AB:"
     "h18\n1024\nbuiltins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-"
     "light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n"},
    {":colorscheme "
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日",
     512U, 309760U, expected_7, 0U,
     "fallback-long\n:colorscheme "
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"
     "日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日\n512\nbuiltins\nubuntu"
     "-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-"
     "dark\nnight-owl\nnight-owl-light\n"},
    {":user-0123", 128U, 8320U, expected_8, 256U,
     "custom-narrow\n:user-0123\n128\ncustom256-invalid-color\nubuntu-aubergine\nneutral-"
     "light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-"
     "light\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-"
     "0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-"
     "0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-"
     "0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-"
     "0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-"
     "0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-"
     "0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-"
     "0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-"
     "0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-"
     "0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-"
     "0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-"
     "0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-"
     "0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-"
     "0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-"
     "0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-"
     "0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-"
     "0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-"
     "0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-"
     "0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-"
     "0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-"
     "0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-"
     "0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-"
     "0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-"
     "0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-"
     "0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-"
     "0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-"
     "0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-"
     "0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-"
     "0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-"
     "0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-"
     "0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-"
     "0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-"
     "0255\n"},
}};
[[nodiscard]] core::ThemeCatalog catalog_of(PaletteChoiceWorkload workload)
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
        return core::ThemeCatalog::builtins();
    case PaletteChoiceWorkload::custom_all:
    case PaletteChoiceWorkload::custom_narrow:
        break;
    }
    std::vector<core::ThemeRecord> records;
    for (const auto name : custom_names)
    {
        records.push_back({core::ThemeName::parse(name).value(),
                           std::unexpected(core::ThemeFailure::invalid_color)});
    }
    return core::ThemeCatalog::from(std::move(records)).value();
}
[[nodiscard]] bool builtin_names_equal(std::span<const std::string> names)
{
    for (std::size_t index = 0; index < builtin_names.size(); ++index)
    {
        if (names.at(index) != builtin_names.at(index))
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] bool catalog_equal(const core::ThemeCatalog &catalog,
                                 const PaletteChoiceSample &sample)
{
    const auto names = catalog.names();
    const auto records = catalog.records();
    if (records.size() != sample.catalog_records ||
        names.size() != builtin_names.size() + sample.catalog_records)
    {
        return false;
    }
    if (!builtin_names_equal(names))
    {
        return false;
    }
    if (!records.empty() && records.size() != custom_names.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const auto &record = records[index];
        if (record.name.text() != custom_names.at(index) || record.value.has_value() ||
            record.value.error() != core::ThemeFailure::invalid_color ||
            names.at(builtin_names.size() + index) != custom_names.at(index))
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
[[nodiscard]] bool choice_equal(const core::CommandChoice &choice,
                                const PaletteChoiceExpected &expected)
{
    if (choice.detail.has_value() != expected.detail.has_value())
    {
        return false;
    }
    if (choice.detail.has_value() && expected.detail.has_value() &&
        choice.detail.value().text() != expected.detail.value())
    {
        return false;
    }
    return choice.command == expected.command && choice.label.text() == expected.label &&
           choice.label.code_point_count() == expected.label_code_points &&
           choice.kind == expected.kind && choice.origin == expected.origin &&
           choice.operation == expected.operation && choice.key == expected.key;
}
[[nodiscard]] bool choices_equal(std::span<const core::CommandChoice> choices,
                                 const PaletteChoiceSample &sample)
{
    if (choices.size() != sample.expected.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < choices.size(); ++index)
    {
        if (!choice_equal(choices[index], sample.expected[index]))
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
                               const PaletteChoiceSample &sample)
{
    if (views.size() != sample.expected.size() * 2U)
    {
        return false;
    }
    for (std::size_t index = 0; index < sample.expected.size(); ++index)
    {
        if (views[index * 2U] != sample.expected[index].command ||
            views[index * 2U + 1U] != sample.expected[index].label)
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
        !choices_equal(held, sample) || !views_equal(views, sample) ||
        !choices_equal(detached.value(), sample))
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
    if (checksum != sample.checksum || !choices_equal(core::palette_choices(line), sample) ||
        !choices_equal(held, sample) || !views_equal(views, sample) ||
        !choices_equal(detached.value(), sample) || !line_equal(line, sample) ||
        original_text != sample.input || !catalog_equal(catalog, sample))
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
