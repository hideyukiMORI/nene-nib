// 単体テストの基盤の定義（ADR 0042 決定 1）。計数は関数局所の静的値で 1 か所に持つ。
#include "TestSupport.hpp"
#include "CommandChoice.hpp"
#include "CommandLine.hpp"
#include "CommandText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DisplayText.hpp"
#include "Edit.hpp"
#include "EditHistory.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "Offset.hpp"
#include "SubmitCommand.hpp"
#include "TextBuffer.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorController;
using nenenib::application::EditorFrame;
using nenenib::application::VimKeyPress;
using nenenib::core::absorbed;
using nenenib::core::Composition;
using nenenib::core::CompositionClause;
using nenenib::core::DisplayText;
using nenenib::core::Edit;
using nenenib::core::FilePath;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;
using nenenib::core::VimCharacter;

namespace core = nenenib::core;
namespace app = nenenib::application;
} // namespace

// 可変グローバルは置けない（ARC-005 / clang-tidy）。計数は関数局所の静的値で持つ。
std::size_t &failure_count()
{
    static std::size_t failures = 0;
    return failures;
}

std::size_t &check_count()
{
    static std::size_t checks = 0;
    return checks;
}

void expect(bool condition, const char *description)
{
    ++check_count();
    if (!condition)
    {
        ++failure_count();
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

DisplayText fixed_text(std::string_view text)
{
    auto parsed = DisplayText::parse(text);
    expect(parsed.has_value(), "test fixture text must parse");
    return std::move(parsed).value();
}

TextBuffer buffer_of(std::string_view text)
{
    auto made = TextBuffer::from_utf8(text);
    expect(made.has_value(), "test fixture text must be valid UTF-8");
    return std::move(made).value();
}

// absorbed は失敗しうるので、畳めたことを言ってから中身を見る（CPP-004）。
[[nodiscard]] Edit absorbed_edit(const Edit &previous, const Edit &edit)
{
    const auto folded = absorbed(previous, edit);
    expect(folded.has_value(), "the two edits are adjacent");
    return folded.value_or(edit);
}

// 意図を 1 つ流し、見えている行を '|' でつないで返す。表示値だけで振る舞いを測る（ARC-011）。
std::string applied(EditorController &controller, const nenenib::application::EditorIntent &intent)
{
    const auto frame = controller.apply(intent);
    std::string joined;
    for (const auto &line : frame.lines)
    {
        if (!joined.empty())
        {
            joined += "|";
        }
        joined += line.text;
    }
    return joined;
}

FilePath sample_path()
{
    auto parsed = FilePath::parse("C:\\work\\note.txt");
    expect(parsed.has_value(), "the sample path parses");
    return std::move(parsed).value();
}

// 文節の列を書くのはここだけ。utf8 は「あい」「うえ」のような 3 バイトの列で数える。
[[nodiscard]] Composition composed_of(std::string utf8, std::vector<CompositionClause> clauses,
                                      std::size_t cursor)
{
    return Composition{std::move(utf8), std::move(clauses), Offset{cursor}};
}

app::EditorFrame run_ex(EditorController &controller, std::string text)
{
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U':'}}));
    static_cast<void>(controller.apply(app::CommandText{std::move(text)}));
    return controller.apply(app::SubmitCommand{});
}

std::vector<core::CommandChoice> choices_for(std::string_view query)
{
    return core::palette_choices(core::CommandLine::empty().inserted(query).value());
}
} // namespace nenenib::tests
