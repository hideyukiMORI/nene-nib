#include "ExCompletionProbe.hpp"
#include "AsciiCase.hpp"
#include "BuiltinThemes.hpp"
#include "ExCompletionFrameProbe.hpp"
#include "ExResult.hpp"
#include "Milestone.hpp"
#include "ThemeCatalog.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
namespace nenenib::tests::performance
{
namespace
{
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
// Ex の固定の候補（正本: src/core/ExResult.cpp の fixed_candidates・palette_names・
// document_names の並び。quit! は document_names の quit から作られる）。組み込みテーマの名前は
// core::builtin_themes から引き、ここには書かない。製品の表が変わったら、この並びを製品に合わせる
// （ずれると eng/probe-smoke.ps1 で ex-completion-* が wrong_result で落ちる）。
constexpr auto fixed_candidates = std::to_array<std::string_view>({"colorscheme",
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
                                                                   "set display=grid",
                                                                   "set display=text",
                                                                   "nohlsearch",
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
                                                                   "saveas"});
constexpr std::string_view theme_command = "colorscheme ";
const std::array<ExCompletionSample, 12> samples{
    {{"set f", 1024U, "mode=prefix\nprefix=set f\ncatalog=builtins\n"},
     {"unknown", 1024U, "mode=prefix\nprefix=unknown\ncatalog=builtins\n"},
     {"colorscheme user-0123", 128U,
      "mode=prefix\nprefix=colorscheme "
      "user-0123\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-"
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
     {"", 1024U, "mode=prefix\nprefix=\ncatalog=builtins\n"},
     {"set ", 1024U, "mode=prefix\nprefix=set \ncatalog=builtins\n"},
     {"colorscheme ", 1024U, "mode=prefix\nprefix=colorscheme \ncatalog=builtins\n"},
     {"", 1024U, "mode=all\nprefix=\ncatalog=builtins\n"},
     {"colorscheme ", 128U,
      "mode=prefix\nprefix=colorscheme "
      "\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-"
      "0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-"
      "0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-"
      "0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-"
      "0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-"
      "0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-"
      "0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-"
      "0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-"
      "0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-"
      "0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-"
      "0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-"
      "0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-"
      "0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-"
      "0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-"
      "0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-"
      "0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-"
      "0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-"
      "0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-"
      "0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-"
      "0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-"
      "0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-"
      "0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-"
      "0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-"
      "0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-"
      "0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-"
      "0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-"
      "0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-"
      "0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-"
      "0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-"
      "0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-"
      "0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-"
      "0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-"
      "0252\nuser-0253\nuser-0254\nuser-0255\n"},
     {"", 128U,
      "mode=all\nprefix=\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-"
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
     {"set f", 64U,
      "mode=frame-ex\nprefix=set f\ncatalog=builtins\nbody=a x "
      "a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n"},
     {"", 64U,
      "mode=frame-ex\nprefix=\ncatalog=builtins\nbody=a x "
      "a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n"},
     {":fz", 64U,
      "mode=frame-palette\nprefix=:fz\ncatalog=builtins\nbody=a x "
      "a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n"}}};
[[nodiscard]] bool uses_custom_catalog(ExCompletionWorkload workload)
{
    switch (workload)
    {
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
    case ExCompletionWorkload::frame_palette:
        return false;
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::custom_wide:
    case ExCompletionWorkload::all_custom:
        return true;
    }
    std::unreachable();
}
[[nodiscard]] bool lists_all(ExCompletionWorkload workload)
{
    switch (workload)
    {
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::all_custom:
        return true;
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::custom_wide:
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
    case ExCompletionWorkload::frame_palette:
        return false;
    }
    std::unreachable();
}
[[nodiscard]] core::ThemeCatalog catalog_of(ExCompletionWorkload workload)
{
    if (!uses_custom_catalog(workload))
    {
        return core::ThemeCatalog::builtins();
    }
    std::vector<core::ThemeRecord> records;
    for (const auto name : custom_names)
    {
        records.push_back({core::ThemeName::parse(name).value(),
                           std::unexpected(core::ThemeFailure::invalid_color)});
    }
    return core::ThemeCatalog::from(std::move(records)).value();
}
[[nodiscard]] std::vector<std::string> catalog_names(ExCompletionWorkload workload)
{
    if (uses_custom_catalog(workload))
    {
        return theme_names_with(custom_names);
    }
    return builtin_theme_names();
}
[[nodiscard]] bool catalog_equal(const core::ThemeCatalog &catalog)
{
    const auto names = catalog.names();
    const auto records = catalog.records();
    if (names != (records.empty() ? builtin_theme_names() : theme_names_with(custom_names)))
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
            record.value.error() != core::ThemeFailure::invalid_color)
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] std::vector<std::string> generated(ExCompletionWorkload workload,
                                                 const ExCompletionSample &sample,
                                                 const core::ThemeCatalog &catalog)
{
    switch (workload)
    {
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::all_custom:
        return core::ex_command_candidates(catalog);
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::custom_wide:
        return core::command_completions(sample.prefix, catalog);
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
    case ExCompletionWorkload::frame_palette:
        break;
    }
    std::unreachable();
}
[[nodiscard]] bool held_equal(std::span<const std::string_view> views,
                              std::span<const std::string> expected)
{
    return std::ranges::equal(views, expected);
}
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
pure_probe(ExCompletionWorkload workload, const ExCompletionSample &sample,
           application::TimingPort &timing)
{
    const auto catalog = catalog_of(workload);
    const auto names = catalog.names();
    const std::string prefix(sample.prefix);
    const auto expected = expected_completions(workload);
    const auto held = generated(workload, sample, catalog);
    std::vector<std::string_view> views;
    for (const auto &candidate : held)
    {
        views.emplace_back(candidate);
    }
    if (!catalog_equal(catalog) || !completions_equal(held, expected) ||
        !held_equal(views, expected))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < sample.rounds; ++index)
    {
        const auto result = generated(workload, sample, catalog);
        checksum += completion_checksum(result);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.rounds * completion_checksum(expected) ||
        !completions_equal(generated(workload, sample, catalog), expected) ||
        !completions_equal(held, expected) || !held_equal(views, expected) ||
        names != catalog.names() || !catalog_equal(catalog) || prefix != sample.prefix)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
// テーマの候補（ExResult.cpp の append_theme_candidates）。命令名（colorscheme ）は区別し、引数は
// ASCII の大文字小文字を無視して比べる（#696）。並びはカタログの並びで、最後に system。
void append_expected_themes(std::vector<std::string> &expected,
                            std::optional<std::string_view> prefix,
                            std::span<const std::string> theme_names)
{
    const auto argument_matches = [prefix](std::string_view argument)
    {
        return !prefix.has_value() || core::ascii_prefix_ignoring_case(
                                          argument, prefix.value().substr(theme_command.size()));
    };
    for (const auto &name : theme_names)
    {
        if (argument_matches(name))
        {
            expected.push_back(std::string(theme_command) + name);
        }
    }
    if (argument_matches("system"))
    {
        expected.push_back(std::string(theme_command) + "system");
    }
}
} // namespace
std::vector<std::string> builtin_theme_names()
{
    return theme_names_with({});
}
std::vector<std::string> theme_names_with(std::span<const std::string_view> custom)
{
    std::vector<std::string> names;
    names.reserve(core::builtin_themes.size() + custom.size());
    for (const auto &theme : core::builtin_themes)
    {
        names.emplace_back(theme.name);
    }
    names.insert(names.end(), custom.begin(), custom.end());
    std::ranges::sort(names, core::ascii_folded_before);
    return names;
}
std::vector<std::string> expected_ex_completions(std::optional<std::string_view> prefix,
                                                 std::span<const std::string> theme_names)
{
    const auto matches = [prefix](std::string_view candidate)
    { return !prefix.has_value() || candidate.starts_with(prefix.value()); };
    std::vector<std::string> expected;
    for (const auto candidate : fixed_candidates)
    {
        if (matches(candidate))
        {
            expected.emplace_back(candidate);
        }
    }
    // テーマの候補は全件の一覧か「colorscheme 」で始まる入力のときだけ（ExResult.cpp の
    // append_theme_candidates）。並びはカタログの並びで、最後に system。
    if (prefix.has_value() && !prefix.value().starts_with(theme_command))
    {
        return expected;
    }
    append_expected_themes(expected, prefix, theme_names);
    return expected;
}
const ExCompletionSample &ex_completion_sample(ExCompletionWorkload workload)
{
    return samples.at(static_cast<std::size_t>(workload));
}
std::vector<std::string> expected_completions(ExCompletionWorkload workload)
{
    const auto &sample = ex_completion_sample(workload);
    const auto names = catalog_names(workload);
    if (lists_all(workload))
    {
        return expected_ex_completions(std::nullopt, names);
    }
    return expected_ex_completions(sample.prefix, names);
}
std::string ex_completion_input(ExCompletionWorkload workload)
{
    return std::string(ex_completion_sample(workload).metadata);
}
std::uint64_t completion_checksum(std::span<const std::string> candidates)
{
    std::uint64_t checksum = 1U + candidates.size();
    for (const auto &candidate : candidates)
    {
        checksum += candidate.size();
    }
    return checksum;
}
bool completions_equal(std::span<const std::string> candidates,
                       std::span<const std::string> expected)
{
    return std::ranges::equal(candidates, expected);
}
std::expected<std::uint64_t, ProbeFailure> probed_ex_completion(ExCompletionWorkload workload,
                                                                const std::string &input,
                                                                application::TimingPort &timing)
{
    const auto &sample = ex_completion_sample(workload);
    if (input != sample.metadata)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    switch (workload)
    {
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::custom_wide:
    case ExCompletionWorkload::all_custom:
        return pure_probe(workload, sample, timing);
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
    case ExCompletionWorkload::frame_palette:
        return probed_ex_completion_frame(workload, sample, timing);
    }
    std::unreachable();
}
std::span<const std::string_view> custom_theme_names()
{
    return custom_names;
}
} // namespace nenenib::tests::performance
