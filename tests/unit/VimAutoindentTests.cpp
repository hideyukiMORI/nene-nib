// scope `--vim-autoindent` の単体テスト（#1086・ADR 0012 / 0028 / 0170 の追補）。Vim の
// `autoindent` の振る舞いの正本は fixture（接頭辞 `autoindent-`・Vim 9.1 の oracle）で、ここは
// oracle で測れない側を守る。
// 1. オフ（既定）の道は効果の形が今のまま（INSERT の改行は VimNewLine、`o` は改行 1 つの
//    VimInsertAt、`cc` は行の頭から消す）で、印（VimState.auto_indented）を立てない。オンは
//    字下げを運ぶ形になり、印を立てる。
// 2. CRLF の文書（oracle は LF の文書しか測れない）で、改行と回数の流し直しが文書の改行で入る。
// 3. `:set autoindent` の形（`ai` `noai` `ai?` `ai!` `invai`）は保存する設定の欄を変え、問いは
//    設定を載せない。controller の設定から engine の view へ写る。
// 4. 打った鍵（窓の道）でも :normal! の道と同じ本文（fixture の代表の数件）。
#include "Appearance.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimInsertAt.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNewLine.hpp"
#include "VimNoEffect.hpp"
#include "VimRemoveRange.hpp"
#include "VimReplaceRange.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
using core::Appearance;

constexpr std::string_view indented = "    foo\nbar";

// view から鍵の列を engine に流した最後の step。view の本文とキャレットは鍵の間で動かさない
// （1 鍵か、効果を返す前に保留だけを立てる鍵の列で使う）。
[[nodiscard]] core::VimStep stepped(core::VimState state, const core::VimEditorView &view,
                                    std::string_view keys)
{
    core::VimStep step{state, core::VimNoEffect{}};
    for (const core::VimKey &key : vim_keys_of(keys))
    {
        step = core::vim_step(state, view, key);
        state = step.next;
    }
    return step;
}

// selection は呼び手の式の終わりまで生きる（view は借用する）。
[[nodiscard]] core::VimEditorView view_at(const core::TextBuffer &text,
                                          const core::Selection &selection, bool autoindent)
{
    core::VimEditorView view{text, selection, std::nullopt};
    view.autoindent = autoindent;
    return view;
}

[[nodiscard]] core::VimState inserting(bool auto_indented)
{
    core::VimState state = empty_vim_state();
    state.mode = core::VimMode::insert;
    state.auto_indented = auto_indented;
    return state;
}

constexpr core::Offset line_start{0};
constexpr core::Offset line_end{7};
constexpr core::Offset in_word{5};

void verify_off_path()
{
    const auto text = core::TextBuffer::from_utf8(std::string(indented)).value();
    const auto enter =
        stepped(inserting(false), view_at(text, core::collapsed_at(line_end), false), "<CR>");
    expect(std::holds_alternative<core::VimNewLine>(enter.effect) && !enter.next.auto_indented,
           "with autoindent off the INSERT Enter is the plain newline and sets no mark");
    const auto opened =
        stepped(empty_vim_state(), view_at(text, core::collapsed_at(line_start), false), "o");
    const auto *const at = std::get_if<core::VimInsertAt>(&opened.effect);
    expect(at != nullptr && at->utf8 == "\n" && !opened.next.auto_indented,
           "with autoindent off o inserts one newline");
    const auto changed =
        stepped(empty_vim_state(), view_at(text, core::collapsed_at(in_word), false), "cc");
    const auto *const removed = std::get_if<core::VimRemoveRange>(&changed.effect);
    expect(removed != nullptr && removed->range.begin == core::Offset{0} &&
               !changed.next.auto_indented,
           "with autoindent off cc removes from the line start");
    const auto blank = core::TextBuffer::from_utf8("    foo\n\nbar").value();
    const auto escaped = stepped(
        inserting(false), view_at(blank, core::collapsed_at(core::Offset{8}), false), "<Esc>");
    expect(std::holds_alternative<core::VimMoveTo>(escaped.effect),
           "with autoindent off the Esc on an empty line only moves");
}

void verify_on_path()
{
    const auto text = core::TextBuffer::from_utf8(std::string(indented)).value();
    const auto enter =
        stepped(inserting(false), view_at(text, core::collapsed_at(line_end), true), "<CR>");
    const auto *const replaced = std::get_if<core::VimReplaceRange>(&enter.effect);
    expect(replaced != nullptr && replaced->utf8 == "\n    " && enter.next.auto_indented,
           "with autoindent on the INSERT Enter carries the indent and sets the mark");
    const auto opened =
        stepped(empty_vim_state(), view_at(text, core::collapsed_at(line_start), true), "o");
    const auto *const at = std::get_if<core::VimInsertAt>(&opened.effect);
    expect(at != nullptr && at->utf8 == "\n    " && opened.next.auto_indented,
           "with autoindent on o copies the indent");
    const auto changed =
        stepped(empty_vim_state(), view_at(text, core::collapsed_at(in_word), true), "cc");
    const auto *const kept = std::get_if<core::VimRemoveRange>(&changed.effect);
    expect(kept != nullptr && kept->range.begin == core::Offset{4} && changed.next.auto_indented,
           "with autoindent on cc keeps the indent");
    const auto typed_mark =
        stepped(inserting(true), view_at(text, core::collapsed_at(line_end), true), "x");
    expect(!typed_mark.next.auto_indented, "a typed character clears the mark");
    const auto blank = core::TextBuffer::from_utf8("    foo\n    \nbar").value();
    const auto trimmed = stepped(
        inserting(true), view_at(blank, core::collapsed_at(core::Offset{12}), true), "<Esc>");
    const auto *const trim = std::get_if<core::VimReplaceRange>(&trimmed.effect);
    expect(trim != nullptr && trim->range.begin == core::Offset{8} &&
               trim->range.end == core::Offset{12} && trim->utf8.empty(),
           "Esc with the mark removes the indent of the blank line");
}

// CRLF の文書の本文（oracle は LF しか測れない）。
[[nodiscard]] std::string crlf_after(std::string_view keys)
{
    Editing editing;
    open_vim_document(editing, "    foo\r\nbar");
    static_cast<void>(run_ex(editing.controller(), "set autoindent"));
    vim_replay(editing.controller(), keys);
    return editing.controller().text().text();
}

void verify_crlf()
{
    expect(crlf_after("A<CR>x<Esc>") == "    foo\r\n    x\r\nbar",
           "the autoindent newline uses the CRLF of the document");
    expect(crlf_after("A<CR><CR>x<Esc>") == "    foo\r\n\r\n    x\r\nbar",
           "the second Enter cuts the indent of the CRLF line");
    expect(crlf_after("o<Esc>") == "    foo\r\n\r\nbar", "Esc removes the indent in CRLF");
    expect(crlf_after("3ox<CR>y<Esc>") ==
               "    foo\r\n    x\r\n    y\r\n    x\r\n    y\r\n    x\r\n    y\r\nbar",
           "the counted replay of o with Enter keeps CRLF and the indent");
    expect(crlf_after("3o<Esc>") == "    foo\r\n\r\n\r\n\r\nbar",
           "the counted replay of an empty o removes every indent in CRLF");
}

// Ex の答えが載せた設定の autoindent（設定を載せなければ空）。
[[nodiscard]] std::optional<bool>
carried_indent(const std::expected<core::ExResult, core::ExEvaluationFailure> &result)
{
    if (!result.has_value() || !result.value().settings.has_value())
    {
        return std::nullopt;
    }
    return result.value().settings.value_or(core::default_editor_settings()).autoindent;
}

void verify_ex_forms()
{
    const auto defaults = core::default_editor_settings();
    expect(!defaults.autoindent, "autoindent is off by default like Vim");
    for (const auto text : {"set ai", "set autoindent"})
    {
        const auto on = core::evaluate_ex(text, defaults, Appearance::dark);
        expect(carried_indent(on) == std::optional<bool>{true} &&
                   on.value().message.text() == "autoindent=on",
               "set ai turns autoindent on through the saved settings");
    }
    auto indenting = defaults;
    indenting.autoindent = true;
    for (const auto text : {"set noai", "set noautoindent", "set ai!", "set invai"})
    {
        const auto off = core::evaluate_ex(text, indenting, Appearance::dark);
        expect(carried_indent(off) == std::optional<bool>{false},
               "noai, ai! and invai turn it off from on");
    }
    const auto toggled = core::evaluate_ex("set ai!", defaults, Appearance::dark);
    expect(carried_indent(toggled) == std::optional<bool>{true}, "ai! turns it on from off");
    const auto asked = core::evaluate_ex("set ai?", indenting, Appearance::dark);
    expect(asked.has_value() && !asked.value().settings.has_value() &&
               asked.value().message.text() == "autoindent=on",
           "ai? tells the value without carrying settings");
    expect(!core::evaluate_ex("set ai=1", defaults, Appearance::dark).has_value() &&
               !core::evaluate_ex("set aix", defaults, Appearance::dark).has_value(),
           "other spellings of autoindent are refused");
}

void verify_controller_setting()
{
    Editing editing;
    open_vim_document(editing, std::string(indented));
    static_cast<void>(run_ex(editing.controller(), "set ai"));
    expect(editing.controller().frame().settings.autoindent, "set ai reaches the settings");
    vim_replay(editing.controller(), "ox<Esc>");
    expect(whole_vim_body(editing.controller()) == "    foo\n    x\nbar",
           "the controller lends the setting to the engine");
    static_cast<void>(run_ex(editing.controller(), "set noai"));
    vim_replay(editing.controller(), "ox<Esc>");
    expect(whole_vim_body(editing.controller()) == "    foo\n    x\nx\nbar",
           "set noai stops copying the indent");
}

// 打った鍵（窓の道）の本文。
[[nodiscard]] std::string typed(std::string_view text, std::string_view keys)
{
    Editing editing;
    open_vim_document(editing, std::string(text));
    static_cast<void>(run_ex(editing.controller(), "set autoindent"));
    vim_replay(editing.controller(), keys);
    return whole_vim_body(editing.controller());
}

void verify_typed_keys()
{
    using Row = std::tuple<std::string_view, std::string_view, std::string_view>;
    // `<NL>`（Ctrl-J）は窓の道で打てない鍵なので fixture ではなくここで守る（Vim は `<CR>`
    // と同じ）。
    for (const auto &[text, keys, body] : std::array<Row, 7>{{
             {indented, "A<NL>x<Esc>", "    foo\n    x\nbar"},
             {indented, "A<CR><CR>x<Esc>", "    foo\n\n    x\nbar"},
             {indented, "3o<Esc>", "    foo\n\n\n\nbar"},
             {indented, "o<BS><Esc>", "    foo\n\nbar"},
             {indented, "ccx<C-u>z<Esc>", "    z\nbar"},
             {indented, "A<CR><Up>x<Esc>", "    xfoo\n\nbar"},
             {indented, "o<C-o>0y<Esc>", "    foo\ny\nbar"},
         }})
    {
        expect(typed(text, keys) == body,
               (std::string("typed keys match the oracle for ") + std::string(keys)).c_str());
    }
}
} // namespace

void verify_vim_autoindent_scope()
{
    verify_off_path();
    verify_on_path();
    verify_crlf();
    verify_ex_forms();
    verify_controller_setting();
    verify_typed_keys();
}
} // namespace nenenib::tests
