// fixture を窓の道でも再生する契約（#1326・ADR 0054 の追補）。fixture
// の再生（verify_vim_fixture）は 入口を通らない press_vim_keys で鍵を流すので、入口 apply
// の処理（map の入口・出口の寄せ keep_vim_character・合図の後始末）が本文やキャレットを Vim
// と変えても落ちない（PR #1323 の レビューの M1 と F2）。ここでは同じ fixture
// の鍵を、窓（src/ui/win32/EditorWindowKeys.cpp）が 送るのと同じ意図に写して 1 つずつ apply
// し、窓の描画と同じく 1 鍵ごとに frame を読む。 期待は engine
// の道の結果（本文・キャレット・無名レジスタと種類・窓の先頭と画面行・横位置・
// 半画面の行数・モード）。鍵が途中で失敗しない fixture では engine の道の 2 本（:normal! の
// press_vim_keys と打った鍵の press_vim_key）が一致し、その値は verify_vim_fixture が oracle と
// 突き合わせている。失敗で残りを捨てる fixture（#230）は、窓では捨てないので打った鍵の道と比べる。
// 窓が Vim の鍵として送らない鍵を含む fixture は飛ばし、理由ごとの件数を断言する。
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorIntent.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "PasteCommand.hpp"
#include "Scopes.hpp"
#include "SubmitCommand.hpp"
#include "TabKey.hpp"
#include "TabKeyTable.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"

#include "../vim/VimFixture.hpp"
#include "../vim/VimFixtures.hpp"
#include "../vim/VimWrapFixtures.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

// 本文・行・桁・無名レジスタの本文と種類・窓の先頭・先頭の行の画面行・横位置・半画面の行数・
// モード。
using Outcome = std::tuple<std::string, std::size_t, std::size_t, std::string, std::string,
                           std::size_t, std::int64_t, float, std::size_t, core::VimMode>;
// 窓が送る意図。値なしは窓が何も送らない鍵。失敗は飛ばす理由の番号（skip_reasons の位置）。
using WindowSend = std::expected<std::optional<app::EditorIntent>, std::size_t>;
// 再生した件数・そのうち失敗で残りを捨てる件数・理由ごとの飛ばした件数・食い違った件数。
using Tally = std::tuple<std::size_t, std::size_t, std::array<std::size_t, 2>, std::size_t>;

constexpr std::size_t untypable_control = 0;
constexpr std::size_t insert_ordinary_control = 1;
constexpr std::array<std::string_view, 2> skip_reasons{
    "a control character other than Tab (WM_CHAR drops it and no key sends it)",
    "INSERT Ctrl+V / A / X / E / Y (the window sends the OS paste, select all, cut or nothing)"};

[[nodiscard]] bool control_character(char32_t code) noexcept
{
    return code < U' ' || code == U'\x7F';
}

// 入力行が開いているあいだ（EditorWindow::press_command_key と type_text）。
[[nodiscard]] WindowSend command_line_send(const core::VimKey &key)
{
    if (const auto *const character = std::get_if<core::VimCharacter>(&key))
    {
        if (character->code == U'\t')
        {
            return app::EditorIntent{app::EditCommand{core::CommandEdit::complete_next}};
        }
        if (control_character(character->code))
        {
            return std::unexpected(untypable_control);
        }
        std::string utf8;
        core::append_utf8(utf8, character->code);
        return app::EditorIntent{app::CommandText{std::move(utf8)}};
    }
    switch (std::get<core::VimSpecialKey>(key))
    {
    case core::VimSpecialKey::enter:
        return app::EditorIntent{app::SubmitCommand{}};
    case core::VimSpecialKey::escape:
        return app::EditorIntent{app::CancelCommand{}};
    case core::VimSpecialKey::backspace:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::backspace}};
    case core::VimSpecialKey::arrow_left:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::left}};
    case core::VimSpecialKey::arrow_right:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::right}};
    case core::VimSpecialKey::home:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::home}};
    case core::VimSpecialKey::end:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::end}};
    case core::VimSpecialKey::arrow_up:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::history_previous}};
    case core::VimSpecialKey::arrow_down:
        return app::EditorIntent{app::EditCommand{core::CommandEdit::history_next}};
    case core::VimSpecialKey::control_v:
        return app::EditorIntent{app::PasteCommand{}};
    case core::VimSpecialKey::page_up:
    case core::VimSpecialKey::page_down:
    case core::VimSpecialKey::control_r:
    case core::VimSpecialKey::control_d:
    case core::VimSpecialKey::control_u:
    case core::VimSpecialKey::control_f:
    case core::VimSpecialKey::control_b:
    case core::VimSpecialKey::control_w:
    case core::VimSpecialKey::control_a:
    case core::VimSpecialKey::control_x:
    case core::VimSpecialKey::control_o:
    case core::VimSpecialKey::control_e:
    case core::VimSpecialKey::control_y:
        return std::optional<app::EditorIntent>{};
    }
    std::unreachable();
}

// 入力行が閉じた Vim モード（press_vim_key・press_control_key の vim_control_key・send_vim_redo・
// type_text）。Ctrl+V / A / X / E / Y は INSERT でだけ通常モードの表へ落ちる（vim_block_key）。
[[nodiscard]] WindowSend vim_send(const core::VimKey &key, core::VimMode mode)
{
    const app::EditorIntent press{app::VimKeyPress{key}};
    if (const auto *const character = std::get_if<core::VimCharacter>(&key))
    {
        if (character->code != U'\t' && control_character(character->code))
        {
            return std::unexpected(untypable_control);
        }
        return press;
    }
    switch (std::get<core::VimSpecialKey>(key))
    {
    case core::VimSpecialKey::control_v:
    case core::VimSpecialKey::control_a:
    case core::VimSpecialKey::control_x:
    case core::VimSpecialKey::control_e:
    case core::VimSpecialKey::control_y:
        if (mode == core::VimMode::insert)
        {
            return std::unexpected(insert_ordinary_control);
        }
        return press;
    case core::VimSpecialKey::escape:
    case core::VimSpecialKey::enter:
    case core::VimSpecialKey::backspace:
    case core::VimSpecialKey::arrow_left:
    case core::VimSpecialKey::arrow_right:
    case core::VimSpecialKey::arrow_up:
    case core::VimSpecialKey::arrow_down:
    case core::VimSpecialKey::home:
    case core::VimSpecialKey::end:
    case core::VimSpecialKey::page_up:
    case core::VimSpecialKey::page_down:
    case core::VimSpecialKey::control_r:
    case core::VimSpecialKey::control_d:
    case core::VimSpecialKey::control_u:
    case core::VimSpecialKey::control_f:
    case core::VimSpecialKey::control_b:
    case core::VimSpecialKey::control_w:
    case core::VimSpecialKey::control_o:
        return press;
    }
    std::unreachable();
}

// 窓は Vim の鍵の前に操作の表・ブックマーク・タブの鍵を引く（press_key）。fixture の鍵に当たる
// 和音が Vim モードで表に無いことを確かめ、上の写しが窓の順と同じであることを機械で守る。
void verify_window_tables_pass_vim_keys()
{
    using core::OperationKey;
    constexpr std::array<OperationKey, 7> controlled{
        OperationKey::d, OperationKey::w, OperationKey::y, OperationKey::f,
        OperationKey::o, OperationKey::e, OperationKey::v};
    bool passes =
        !core::operation_for(core::KeyChord{false, false, OperationKey::tab}, core::EditMode::vim)
             .has_value() &&
        !core::operation_for(core::KeyChord{false, false, OperationKey::space}, core::EditMode::vim)
             .has_value() &&
        !core::tab_command_for(core::TabKey::control_w, core::EditMode::vim).has_value();
    for (const OperationKey key : controlled)
    {
        passes =
            passes &&
            !core::operation_for(core::KeyChord{true, false, key}, core::EditMode::vim).has_value();
    }
    expect(passes, "the operation, bookmark and tab tables let the fixture keys through to the "
                   "Vim keys in Vim mode (#1326)");
}

[[nodiscard]] Outcome outcome_of(EditorController &controller)
{
    const auto frame = controller.frame();
    const auto &state = controller.vim_state();
    const auto &unnamed = state.unnamed_register.value();
    const std::size_t scroll =
        state.scroll_lines.has_value() ? state.scroll_lines.value().value : 0U;
    const std::int64_t row = frame.lines.empty() ? 0 : frame.lines.front().screen_row;
    Outcome result{std::string{},
                   frame.caret.position.line.value,
                   frame.caret.position.column.value,
                   unnamed.text,
                   vim_register_kind(unnamed),
                   frame.first_visible.value,
                   row,
                   frame.horizontal,
                   scroll,
                   state.mode};
    std::get<0>(result) = whole_vim_body(controller);
    return result;
}

void prepare(Editing &editing, const VimFixture &fixture)
{
    open_vim_document(editing, std::string(fixture.text));
    arrange_vim_viewport(editing.controller(), fixture);
    store_vim_fixture_macro(editing.controller(), fixture);
}

[[nodiscard]] Outcome engine_outcome(const VimFixture &fixture, bool typed)
{
    Editing editing;
    prepare(editing, fixture);
    if (typed)
    {
        vim_replay(editing.controller(), fixture.keys);
    }
    else
    {
        vim_normal(editing.controller(), fixture.keys);
    }
    return outcome_of(editing.controller());
}

// 窓と同じに鍵を 1 つずつ意図にして送り、1 鍵ごとに描く（WM_PAINT の frame）。
[[nodiscard]] std::expected<Outcome, std::size_t> window_outcome(const VimFixture &fixture)
{
    Editing editing;
    prepare(editing, fixture);
    EditorController &controller = editing.controller();
    for (const core::VimKey &key : vim_keys_of(fixture.keys))
    {
        const WindowSend send = controller.command_line_active()
                                    ? command_line_send(key)
                                    : vim_send(key, controller.vim_state().mode);
        if (!send.has_value())
        {
            return std::unexpected(send.error());
        }
        const std::optional<app::EditorIntent> &intent = send.value();
        if (intent.has_value())
        {
            static_cast<void>(controller.apply(intent.value()));
            static_cast<void>(controller.frame());
        }
    }
    return outcome_of(controller);
}

void report_difference(const VimFixture &fixture, const Outcome &window, const Outcome &engine)
{
    std::fprintf(stderr,
                 "  window path %.*s\n    window: \"%s\" %zu:%zu reg \"%s\" top %zu mode %d\n"
                 "    engine: \"%s\" %zu:%zu reg \"%s\" top %zu mode %d\n",
                 static_cast<int>(fixture.name.size()), fixture.name.data(),
                 std::get<0>(window).c_str(), std::get<1>(window), std::get<2>(window),
                 std::get<3>(window).c_str(), std::get<5>(window),
                 static_cast<int>(std::get<9>(window)), std::get<0>(engine).c_str(),
                 std::get<1>(engine), std::get<2>(engine), std::get<3>(engine).c_str(),
                 std::get<5>(engine), static_cast<int>(std::get<9>(engine)));
}

void replay_on_window(const VimFixture &fixture, Tally &tally)
{
    const auto window = window_outcome(fixture);
    if (!window.has_value())
    {
        ++std::get<2>(tally).at(window.error());
        return;
    }
    ++std::get<0>(tally);
    const Outcome typed = engine_outcome(fixture, true);
    if (typed != engine_outcome(fixture, false))
    {
        ++std::get<1>(tally);
    }
    if (window.value() != typed)
    {
        ++std::get<3>(tally);
        report_difference(fixture, window.value(), typed);
    }
}

// 並びの stride 件に 1 件（先頭から・決定的）。stride 1 は全件。
[[nodiscard]] Tally replayed_fixtures(std::span<const VimFixture> fixtures, std::size_t stride)
{
    Tally tally{};
    for (std::size_t index = 0; index < fixtures.size(); index += stride)
    {
        replay_on_window(fixtures[index], tally);
    }
    return tally;
}

// 件数は HEAD の fixture で数えた値。fixture を足したら数え直す（黙って飛ばさない）。数が
// 合わなければ数えた値と飛ばした理由を出す。
void verify_tally(const Tally &tally, const Tally &expected, const char *label)
{
    const auto &[replayed, truncated, skipped, differing] = tally;
    if (tally != expected)
    {
        std::fprintf(stderr, "  %s: replayed %zu (truncated %zu), differing %zu\n", label, replayed,
                     truncated, differing);
        for (std::size_t reason = 0; reason < skip_reasons.size(); ++reason)
        {
            std::fprintf(stderr, "    skipped %zu: %.*s\n", skipped.at(reason),
                         static_cast<int>(skip_reasons.at(reason).size()),
                         skip_reasons.at(reason).data());
        }
    }
    expect(differing == 0U, "every replayed fixture reaches the engine path's text, caret, "
                            "register, viewport and mode on the window path (#1326)");
    expect(replayed == std::get<0>(expected) && truncated == std::get<1>(expected),
           "the window path replays the counted fixtures, and the counted ones stop at a "
           "failure on the :normal! path (#1326)");
    expect(skipped == std::get<2>(expected),
           "the window path skips only the counted fixtures, for the named reasons (#1326)");
}

void verify_fixture_window_path(std::size_t stride, const Tally &main, const Tally &wrapped)
{
    verify_window_tables_pass_vim_keys();
    verify_tally(replayed_fixtures(vim_fixtures, stride), main, "fixtures");
    verify_tally(replayed_fixtures(vim_wrap_fixtures, stride), wrapped, "wrap fixtures");
}
} // namespace

// 既定実行は並びの 10 件に 1 件（先頭から 0, 10, 20, …）。
void verify_vim_fixture_window_path_sample()
{
    verify_fixture_window_path(10U, Tally{362U, 13U, {0U, 0U}, 0U}, Tally{34U, 2U, {0U, 0U}, 0U});
}

// 全件（名指しの scope `--vim-window-path-all`・nib_unit_scopes が回す）。
void verify_vim_fixture_window_path_all()
{
    verify_fixture_window_path(1U, Tally{3612U, 129U, {0U, 0U}, 0U}, Tally{332U, 8U, {0U, 0U}, 0U});
}
} // namespace nenenib::tests
