// settings の `lsp.<language>.command` の行の codec の試験（ADR 0170 の決定 3・#1044・#1897）。
// NibAdapterTests.cpp の長さの上限（CNF-014）のため分けた（#1956 の main の取り込み）。ファイルに
// 触れない。
#include "SettingsLspCommandTests.hpp"

#include "EditorSettings.hpp"
#include "SettingsCodec.hpp"
#include "SettingsFailure.hpp"
#include "SettingsVersion.hpp"
#include "SettingsVersionSixTests.hpp"

#include <format>
#include <string>
#include <string_view>

namespace nenenib::tests::adapters
{
namespace
{
using nenenib::adapters::win32::decode_settings;
using nenenib::adapters::win32::encode_settings;
using nenenib::adapters::win32::SettingsVersion;
using nenenib::application::SettingsFailure;
using nenenib::core::default_editor_settings;

// 表に無い名前の行（後の版が足す言語）は v5 と v6 では形だけ見て持ち、書き戻す（ADR 0170 の
// 2026-11-20 の追補・#1897）。v4 は 1.0.0 と同じく拒む（D91）。
void verify_unknown_command_names(Expect expect, std::string_view released_v4_settings)
{
    const auto plain = encode_settings(default_editor_settings());
    const std::string later = plain + "lsp.nim.command=C:\\nim\\nimlangserver.exe\n"
                                      "lsp.c++_x-1.command=y\n"
                                      "lsp.abcdefghijklmnopqrstuvwxyz012345.command=z\n";
    const auto kept = decode_settings(later, SettingsVersion::v6);
    expect(
        kept && kept.value().lsp_commands.size() == 3 &&
            encode_settings(kept.value())
                .ends_with("lsp.abcdefghijklmnopqrstuvwxyz012345.command=z\n"
                           "lsp.c++_x-1.command=y\nlsp.nim.command=C:\\nim\\nimlangserver.exe\n"),
        "v6 keeps command rows for languages it does not know and writes them back");
    expect(!decode_settings(plain + "lsp.nim.command=x\nlsp.nim.command=y\n", SettingsVersion::v6),
           "a second row for an unknown name is refused too");
    // Java と Svelte は 1.0.0 の後に表へ足した言語（ADR 0154 の 2026-11-20 の追補）。v4 の読みは
    // 1.0.0 の 26 言語のまま緩めない。
    for (const std::string_view name : {"nim", "java", "svelte"})
    {
        const auto v4 = decode_settings(
            std::format("{}lsp.{}.command=x\n", released_v4_settings, name), SettingsVersion::v4);
        expect(!v4 && v4.error() == SettingsFailure::malformed,
               "v4 still refuses a command row for a language 1.0.0 does not know");
    }
    expect(decode_settings(std::format("{}lsp.java.command=x\n", released_v5_settings),
                           SettingsVersion::v5)
                   .has_value() &&
               decode_settings(plain + "lsp.java.command=x\n", SettingsVersion::v6).has_value(),
           "v5 and v6 read a command row for a language added after 1.0.0");
}
} // namespace

// `lsp.<language>.command` の行（ADR 0170 の決定 3・#1044）。0
// 行以上・言語は表の名前だけ・同じ言語は 1 行・値は空でない。書くときは features
// と検索の大小の後ろに言語の名前の順。
void verify_settings_lsp_commands(Expect expect, std::string_view released_v4_settings,
                                  std::string_view v3_settings)
{
    auto settings = default_editor_settings();
    settings.lsp_commands = {{"python", "C:\\py\\pylsp.exe"},
                             {"cpp", "C:\\llvm\\clangd.exe --log=error"}};
    const auto bytes = encode_settings(settings);
    expect(bytes.ends_with("features=\nignorecase=off\nsmartcase=off\nrelativenumber=off\n"
                           "scrolloff=0\nexplorer_width=\nautoindent=off\n"
                           "finder_preview=auto\ncaret_motion=off\ncaret_motion_time=120\n"
                           "indent_guide_style=rainbow\nindent_guide_colors=4\n"
                           "indent_guide_active=on\nindent_guide_width=4\n"
                           "lsp.cpp.command=C:\\llvm\\clangd.exe --log=error\n"
                           "lsp.python.command=C:\\py\\pylsp.exe\n"),
           "commands follow the fixed keys in language name order");
    const auto decoded = decode_settings(bytes, SettingsVersion::v6);
    expect(decoded && decoded.value().lsp_commands.size() == 2 &&
               decoded.value().lsp_commands.front().first == "cpp" &&
               encode_settings(decoded.value()) == bytes,
           "commands round trip in language name order");
    const auto plain = encode_settings(default_editor_settings());
    const auto none = decode_settings(plain, SettingsVersion::v6);
    expect(none && none.value().lsp_commands.empty(), "zero command lines are accepted");
    for (const auto line :
         {"lsp.cpp.command=\n", "lsp..command=x\n", "lsp.CPP.command=x\n", "lsp.cpp.args=x\n",
          "lsp.a b.command=x\n", "lsp.j\xc3\xa1va.command=x\n",
          "lsp.abcdefghijklmnopqrstuvwxyz0123456.command=x\n"})
    {
        const auto result = decode_settings(plain + line, SettingsVersion::v6);
        expect(!result && result.error() == SettingsFailure::malformed,
               "misshapen names, empty values and other lsp keys are refused");
    }
    expect(!decode_settings(plain + "lsp.cpp.command=a\nlsp.cpp.command=b\n", SettingsVersion::v6),
           "a second line for the same language is refused");
    verify_unknown_command_names(expect, released_v4_settings);
    const auto old =
        decode_settings(std::string(v3_settings) + "lsp.cpp.command=x\n", SettingsVersion::v3);
    expect(!old && old.error() == SettingsFailure::malformed, "v3 rejects command lines");
    auto other = settings;
    other.lsp_commands.pop_back();
    expect(!core::same_settings(other, settings), "commands take part in the settings comparison");
}
} // namespace nenenib::tests::adapters
