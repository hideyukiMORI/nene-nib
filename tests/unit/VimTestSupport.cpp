// Vim の単体テストが共有する足場の定義（ADR 0042 決定 1）。
#include "VimTestSupport.hpp"
#include "CancelCommand.hpp"
#include "Column.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "ScriptedFiles.hpp"
#include "ScrollLines.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "StoreVimMacro.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimCount.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixture.hpp"
#include "../vim/VimMacroFixture.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorFrame;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::ScrollLines;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::append_utf8;
using nenenib::core::code_point_at;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::next_code_point;
using nenenib::core::Offset;
using nenenib::core::SelectionAnchoring;
using nenenib::core::TextBuffer;
using nenenib::core::TextPosition;
using nenenib::core::VimCharacter;
using nenenib::core::VimCharacterSearchKind;
using nenenib::core::VimKey;
using nenenib::core::VimPrefix;
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimState;

[[nodiscard]] std::optional<VimKeyName> vim_key_name_at(std::string_view keys, std::size_t index)
{
    for (const VimKeyName name : vim_key_names)
    {
        if (keys.substr(index).starts_with(name.text))
        {
            return name;
        }
    }
    return std::nullopt;
}

// 入力行が開いているあいだの鍵の写し先。窓と同じ約束の 2 つめの端（ARC-012・ADR 0032 の決定 1）。
// fixture の `/foo<CR>` は Vim では 1 つの命令なので、再生もこの 1 本を通る。
void command_key(EditorController &controller, const VimKey &key)
{
    if (const auto *special = std::get_if<VimSpecialKey>(&key))
    {
        if (*special == VimSpecialKey::enter)
        {
            static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
            return;
        }
        if (*special == VimSpecialKey::escape)
        {
            static_cast<void>(controller.apply(nenenib::application::CancelCommand{}));
            return;
        }
        if (*special == VimSpecialKey::backspace)
        {
            static_cast<void>(controller.apply(
                nenenib::application::EditCommand{nenenib::core::CommandEdit::backspace}));
        }
        return;
    }
    if (const auto *character = std::get_if<VimCharacter>(&key))
    {
        std::string utf8;
        nenenib::core::append_utf8(utf8, character->code);
        static_cast<void>(controller.apply(nenenib::application::CommandText{utf8}));
    }
}

[[nodiscard]] TextPosition vim_fixture_position(const VimFixture &fixture)
{
    const auto text = TextBuffer::from_utf8(fixture.text);
    expect(text.has_value(), "the fixture input is valid UTF-8");
    if (!text.has_value() || !fixture.viewport.has_value())
    {
        return TextPosition{LineNumber{1}, Column{1}};
    }
    const auto &viewport = fixture.viewport.value();
    const LineNumber line{static_cast<std::size_t>(viewport.line)};
    const Offset at{text.value().line_start(line).value +
                    static_cast<std::size_t>(viewport.column - 1)};
    return text.value().position_of(at);
}
} // namespace

[[nodiscard]] std::vector<VimKey> vim_keys_of(std::string_view keys)
{
    std::vector<VimKey> result;
    std::size_t index = 0;
    while (index < keys.size())
    {
        const auto name = vim_key_name_at(keys, index);
        if (name.has_value())
        {
            result.emplace_back(name.value().key);
            index += name.value().text.size();
            continue;
        }
        result.emplace_back(VimCharacter{code_point_at(keys, Offset{index})});
        index = next_code_point(keys, Offset{index}).value;
    }
    return result;
}

// 表示値だけから本文を組み立てる（ARC-011）。行でつなぐので Vim の getline(1, '$') と同じ形。
[[nodiscard]] std::string vim_body(const EditorFrame &frame)
{
    std::string joined;
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        if (index > 0)
        {
            joined += "\n";
        }
        joined += frame.lines.at(index).text;
    }
    return joined;
}

void vim_replay(EditorController &controller, std::string_view keys)
{
    for (const VimKey &key : vim_keys_of(keys))
    {
        if (controller.command_line_active())
        {
            command_key(controller, key);
            continue;
        }
        static_cast<void>(controller.apply(VimKeyPress{key}));
    }
}

[[nodiscard]] VimState empty_vim_state()
{
    return nenenib::core::vim_resting_state(
        VimRegister{std::string{}, VimRegisterKind::uninitialized});
}

[[nodiscard]] bool last_search_is(const VimState &state, VimCharacterSearchKind kind,
                                  char32_t target) noexcept
{
    return state.last_character_search.has_value() &&
           state.last_character_search.value().kind == kind &&
           state.last_character_search.value().target == target;
}

[[nodiscard]] bool waits_for_character(const VimState &state, VimCharacterSearchKind kind) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<VimCharacterSearchKind>(state.input_wait.value()) &&
           std::get<VimCharacterSearchKind>(state.input_wait.value()) == kind;
}

[[nodiscard]] bool waits_for_prefix(const VimState &state, VimPrefix prefix) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<VimPrefix>(state.input_wait.value()) &&
           std::get<VimPrefix>(state.input_wait.value()) == prefix;
}

void arrange_vim_viewport(EditorController &controller, const VimFixture &fixture)
{
    if (!fixture.viewport.has_value())
    {
        return;
    }
    const auto &viewport = fixture.viewport.value();
    static_cast<void>(controller.apply(VisibleLines{viewport.visible_lines}));
    static_cast<void>(
        controller.apply(PlaceCaret{vim_fixture_position(fixture), SelectionAnchoring::collapse}));
    const std::int64_t current = static_cast<std::int64_t>(controller.frame().first_visible.value);
    const std::int64_t requested = static_cast<std::int64_t>(viewport.first_visible);
    static_cast<void>(
        controller.apply(ScrollLines{static_cast<std::int32_t>(requested - current)}));
}

// fixture の `register`（ADR 0046 の決定 6・oracle の `let @a = "…"`）。記法の鍵を VimKey の列へ
// 写すのは録画と同じ 1 本にしたいので、同じ本文を開いた別の editor で `q{name}` のあとに鍵を打ち、
// 録画中の鍵の列をそのまま取って、再生する editor のレジスタへ StoreVimMacro で置く。
// 別の editor で打つので、再生する側の本文・レジスタ・直前の変更には何も残らない。
void store_vim_fixture_macro(EditorController &controller, const VimFixture &fixture)
{
    if (!fixture.macro.has_value())
    {
        return;
    }
    const VimMacroFixture &macro = fixture.macro.value();
    Editing scratch;
    open_vim_document(scratch, std::string(fixture.text));
    applied(scratch.controller(), VimKeyPress{VimKey{VimCharacter{U'q'}}});
    applied(scratch.controller(),
            VimKeyPress{VimKey{VimCharacter{static_cast<char32_t>(macro.name)}}});
    vim_replay(scratch.controller(), macro.keys);
    const auto &recording = scratch.controller().vim_state().macro_recording;
    expect(recording.has_value(),
           (std::string(fixture.name) + ": the register was recorded").c_str());
    if (!recording.has_value())
    {
        return;
    }
    applied(controller, nenenib::application::StoreVimMacro{static_cast<char32_t>(macro.name),
                                                            recording.value().keys});
}

[[nodiscard]] std::string whole_vim_body(EditorController &controller)
{
    static_cast<void>(controller.apply(VisibleLines{controller.frame().total_lines}));
    const std::int64_t first = static_cast<std::int64_t>(controller.frame().first_visible.value);
    static_cast<void>(controller.apply(ScrollLines{static_cast<std::int32_t>(1 - first)}));
    return vim_body(controller.frame());
}

// 無名レジスタの種類を Vim の getregtype の言葉で言う。一度も使っていないレジスタは空（ADR 0015）。
// 矩形は Ctrl-V（0x16）に幅の 10 進が続く（ADR 0035 の決定 10・Vim 9.1 で実測）。
[[nodiscard]] std::string vim_register_kind(const VimRegister &value)
{
    switch (value.kind)
    {
    case VimRegisterKind::uninitialized:
        return "";
    case VimRegisterKind::characters:
        return "v";
    case VimRegisterKind::lines:
        return "V";
    case VimRegisterKind::block:
        // 8 進のエスケープは 3 桁で止まるので、続く幅の数字と混ざらない（"\x16" は混ざる）。
        return "\026" + std::to_string(value.width.has_value() ? value.width.value().columns : 0);
    }
    std::unreachable();
}

void open_vim_document(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), VisibleLines{vim_visible_lines});
    applied(editing.controller(), OpenDocument{sample_path()});
    applied(editing.controller(), SelectEditMode{EditMode::vim});
}

// 記録は回数 1 つと、回数の桁を除いた鍵の列（決定 1・2）。表示値に出ないのでここで直接見る。
[[nodiscard]] bool dot_record_is(const VimState &state,
                                 const std::optional<nenenib::core::VimCount> &count,
                                 std::string_view keys)
{
    if (!state.last_change.has_value())
    {
        return false;
    }
    const auto &record = state.last_change.value();
    return record.count == count && record.keys == vim_keys_of(keys);
}

// 期待値は固定 Vim 9.1 の 218 ケース（out/issue100-oracle）から取った。fixture にできない
// 取消・未対応構文・報せの文言はここが正本の検査である（ADR 0032 の補足）。
[[nodiscard]] bool caret_at(const EditorFrame &frame, std::size_t line, std::size_t column)
{
    return frame.caret.position.line == LineNumber{line} &&
           frame.caret.position.column == Column{column};
}

[[nodiscard]] std::vector<MatchSpan> frame_matches(const EditorFrame &frame, std::size_t index)
{
    std::vector<MatchSpan> spans;
    for (const auto &span : frame.lines.at(index).matches)
    {
        spans.emplace_back(span.begin.value, span.end.value);
    }
    return spans;
}

[[nodiscard]] bool current_match_is(const EditorFrame &frame, std::size_t index,
                                    std::optional<MatchSpan> expected)
{
    const auto &current = frame.lines.at(index).current_match;
    if (!current.has_value())
    {
        return !expected.has_value();
    }
    return expected == std::optional<MatchSpan>{
                           MatchSpan{current.value().begin.value, current.value().end.value}};
}
} // namespace nenenib::tests
