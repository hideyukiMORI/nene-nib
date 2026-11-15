// バージョン情報（施主決定 D90・#1677・ADR 0078 / 0172 の追補）: 名乗りの文は core::AppIdentity の
// 1 か所で、版は project(... VERSION) の値そのもの（CMake が nib_tests にも同じ PROJECT_VERSION を
// NIB_EXPECTED_VERSION として渡す）。F1 の一覧の操作「バージョン情報」と Vim の `:version` `:ve`
// は同じ 1 行をステータスバーの知らせにし、本文・設定・port に触れない。操作は鍵を持たない。
#include "AppIdentity.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "OperationBindings.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "ShowVersion.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

constexpr std::string_view body = "abc\ndef\n";

[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

[[nodiscard]] std::string expected_about()
{
    return std::string("NeNe Nib ") + NIB_EXPECTED_VERSION +
           " · © 2026 Hideyuki Mori · github.com/hideyukiMORI/nene-nib";
}

// 版は project() の値そのもので、文は決めの形（訳さない）。
void verify_identity()
{
    expect(core::app_version() == NIB_EXPECTED_VERSION,
           "the shown version is project(... VERSION) itself");
    expect(core::app_copyright_line() == "© 2026 Hideyuki Mori" &&
               core::app_homepage == "github.com/hideyukiMORI/nene-nib" &&
               core::app_name == "NeNe Nib",
           "the signature lines are the decided text");
    expect(core::app_about_line() == expected_about(),
           "the About line joins the name, version, copyright and address with middle dots");
}

[[nodiscard]] std::expected<core::ExResult, core::ExEvaluationFailure> ex(std::string_view input)
{
    return core::evaluate_ex(input, core::default_editor_settings(), core::Appearance::dark);
}

// `:ve[rsion]` は答えの文が名乗りの 1 行で、設定も窓も変えない。引数・`!`・範囲は断る。
void verify_version_parse()
{
    for (const std::string_view input : {"version", "ve", "ver", "vers", "version  "})
    {
        const auto result = ex(input);
        expect(result && result.value().message.text() == expected_about() &&
                   !result.value().settings.has_value() && !result.value().print.has_value() &&
                   !result.value().window.has_value() && !result.value().document.has_value(),
               ":version and its abbreviations answer the About line without saving settings");
    }
    for (const std::string_view input : {"version!", "version x", "2version", "ve 1"})
    {
        const auto result = ex(input);
        expect(!result && result.error() ==
                              core::ExEvaluationFailure{core::ExFailure::unsupported_argument},
               ":version arguments, the bang and ranges are refused");
    }
    const auto alone = ex("v");
    expect(!alone || alone.value().message.text() != expected_about(), "v alone is not :version");
}

// F1 の操作は鍵なしで両方のモードに載り、意図 ShowVersion は本文を変えずに 1 行を知らせる。
// Vim の `:version` `:ve` も同じ 1 行。
void verify_version_notice()
{
    for (const auto mode : {core::EditMode::ordinary, core::EditMode::vim})
    {
        expect(core::operation_available(core::EditorOperation::show_version, mode) &&
                   !core::shown_chord(core::EditorOperation::show_version, mode).has_value(),
               "About is listed in both modes without a key");
    }
    Editing editing;
    open_vim_document(editing, std::string(body));
    for (const std::string_view keys : {":version<CR>", ":ve<CR>"})
    {
        vim_replay(editing.controller(), keys);
        expect(notice(editing.controller().frame()) == expected_about() &&
                   !editing.controller().frame().operation_request.has_value() &&
                   editing.bell().rings() == 0,
               "Vim's :version and :ve show the About line without asking the window");
    }
    applied(editing.controller(), app::SelectEditMode{core::EditMode::ordinary});
    const auto before = applied(editing.controller(), app::VisibleLines{8});
    const auto after = applied(editing.controller(), app::ShowVersion{});
    expect(notice(editing.controller().frame()) == expected_about() && after == before &&
               editing.files().written().empty(),
           "the About operation shows the line and leaves the text and the files alone");
}
} // namespace

void verify_version_info_contracts()
{
    verify_identity();
    verify_version_parse();
    verify_version_notice();
}
} // namespace nenenib::tests
