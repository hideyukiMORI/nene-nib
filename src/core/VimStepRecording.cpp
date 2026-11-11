#include "VimStepDetail.hpp"

#include "Utf8.hpp"
#include "VimCount.hpp"
#include "VimRegisterSelection.hpp"
#include "VimRegisterText.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
using detail::appended_recording;
using detail::block_of;
using detail::cancelled;
using detail::combined_count;
using detail::counted;
using detail::counts_as_digit;
using detail::decimal_base;
using detail::drops_count_digit;
using detail::erased_nothing;
using detail::failed;
using detail::finished_input_wait;
using detail::line_of;
using detail::macro_name_of;
using detail::macro_record_action;
using detail::macro_recorded;
using detail::moved_nothing;
using detail::register_read;
using detail::register_selection_of;
using detail::repeated_change;
using detail::replayed_macro;
using detail::resolved_count;
using detail::restarts_insert;
using detail::single_step;
using detail::started_macro;
using detail::started_prefix;
using detail::vim_recorded;
using detail::visual_operated_mode;
using detail::wanted_column_of;

namespace detail
{
// ---------------------------------------------------------------- マクロ（ADR 0046）

// 名前の鍵（`q` と `@` の次の 1 鍵）の文字。文字でない鍵は空。名前になるかは呼ぶ側が決める。
[[nodiscard]] std::optional<char32_t> macro_name_of(const VimKey &key) noexcept
{
    if (!std::holds_alternative<VimCharacter>(key))
    {
        return std::nullopt;
    }
    return std::get<VimCharacter>(key).code;
}

// `q{a-z}` は新しい録画、`q{A-Z}` は追記の録画（決定 2）。`q{0-9}` はその数字へ録る（追記なし・
// ADR 0050 の決定 6）。ほかの鍵（`q-` を含む）はビープして何もしない。
// モードと欲しい列はそのまま（VISUAL の `q` も同じ）。
[[nodiscard]] VimStep started_macro(const VimState &state, const VimKey &key)
{
    VimState next = finished_input_wait(state);
    const char32_t name = macro_name_of(key).value_or(char32_t{});
    if (vim_numbered_index(name).has_value())
    {
        next.macro_recording =
            VimMacroRecording{static_cast<char>(name), VimRecordedKeys::from({}), false};
        return VimStep{std::move(next), VimNoEffect{}};
    }
    const auto index = vim_register_index(name);
    if (!index.has_value())
    {
        // Esc は鳴らさずに打ち消す（`q<Esc>x` は x が効く・#1369）。
        return escaped_or_refused(state, key);
    }
    next.macro_recording =
        VimMacroRecording{static_cast<char>(U'a' + index.value()), VimRecordedKeys::from({}),
                          name >= U'A' && name <= U'Z'};
    return VimStep{std::move(next), VimNoEffect{}};
}

// 録画の追記（ADR 0048 の決定 4）。本文の末尾の改行の手前へ繋ぎ、種類は保つ（行単位なら末尾の
// `\n` の前・文字と矩形は末尾）。未使用のレジスタへの追記は新規と同じで文字単位になる。
[[nodiscard]] VimRegister appended_recording(const VimRegister &old, std::string_view text)
{
    if (old.kind == VimRegisterKind::uninitialized)
    {
        return VimRegister{std::string(text), VimRegisterKind::characters};
    }
    VimRegister next = old;
    const bool before_newline = old.kind == VimRegisterKind::lines && next.text.ends_with('\n');
    next.text.insert(before_newline ? next.text.size() - 1 : next.text.size(), text);
    return next;
}
} // namespace detail

namespace
{
// 止める `q`（ADR 0046 の決定 2・ADR 0048 の決定 6）。録った鍵を本文にして文字単位でレジスタへ
// 置き、追記なら appended_recording で繋ぐ。数字は置き換えるだけで、registers_written を通らない
// （繰り下がらない・無名は変えない・ADR 0050 の決定 6）。止める `q` 自身は積まない。
[[nodiscard]] VimStep stopped_macro(const VimState &state, const VimMacroRecording &recording)
{
    VimState next = finished_input_wait(state);
    next.macro_recording = std::nullopt;
    const std::string text = vim_register_text(recording.keys.owned_keys());
    const auto numbered = vim_numbered_index(static_cast<char32_t>(recording.name));
    if (numbered.has_value())
    {
        next.numbered.registers.at(numbered.value()) =
            VimRegisterSnapshot::from(VimRegister{text, VimRegisterKind::characters});
        return VimStep{std::move(next), VimNoEffect{}};
    }
    const auto index = vim_register_index(static_cast<char32_t>(recording.name));
    if (!index.has_value())
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    VimRegisterSnapshot &target = next.registers.registers.at(index.value());
    target = VimRegisterSnapshot::from(recording.append
                                           ? appended_recording(target.value(), text)
                                           : VimRegister{text, VimRegisterKind::characters});
    return VimStep{std::move(next), VimNoEffect{}};
}
} // namespace

namespace detail
{
// NORMAL / VISUAL の `q`（決定 2）。再生の中では何もせず次の鍵も待たない（Vim の「レジスタ
// 実行中は q は無効」）。録画中なら止め、そうでなければ名前を待つ。
[[nodiscard]] VimStep macro_record_action(const VimState &state, const VimEditorView &view)
{
    if (vim_key_replayed(view.source))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    if (state.macro_recording.has_value())
    {
        return stopped_macro(state, state.macro_recording.value());
    }
    return started_prefix(state, VimPrefix::q);
}
} // namespace detail

namespace
{
// 再生するレジスタの選択（ADR 0048 の決定 6・ADR 0050 の決定 5・6）。名前 → 選択は `"` と同じ
// register_selection_of で解く（a〜z・A〜Z・`"`・`0`〜`9`・`-`・`+` `*`・ADR 0051 の
// 決定 5）。`_` はレジスタを持たないので名前にならない。
[[nodiscard]] std::optional<VimRegisterSelection> replayed_selection(char32_t name)
{
    const auto selection = register_selection_of(name);
    if (!selection.has_value() || selection.value().target == VimRegisterTarget::black_hole)
    {
        return std::nullopt;
    }
    return selection;
}

// `@` の次の鍵が指す選択。`@@` は直前の名前で、無ければ名前を持たない（E748 相当）。
[[nodiscard]] std::optional<VimRegisterSelection> replayed_selection(const VimState &state,
                                                                     const VimKey &key)
{
    const char32_t name = macro_name_of(key).value_or(char32_t{});
    if (name != U'@')
    {
        return replayed_selection(name);
    }
    if (!state.last_macro.has_value())
    {
        return std::nullopt;
    }
    return replayed_selection(static_cast<char32_t>(state.last_macro.value()));
}
} // namespace

namespace detail
{
// `[count]@{a-z}` と `@"` と `@@`（ADR 0046 の決定 3・ADR 0048 の決定 6）。レジスタの本文を
// vim_keys_of_text で鍵列にして回数を周の数に載せた VimReplay を返し、controller が `.` と同じ
// 経路で 1 鍵ずつ・1 周ずつ流す（鍵の列を回数ぶん並べない・#1341）。名前が無い・未使用か空の
// レジスタはビープして何もしない。
[[nodiscard]] VimStep replayed_macro(const VimState &state, const VimKey &key)
{
    VimState next = finished_input_wait(state);
    // `@` の鍵は `.` の記録に残さない（`.` と同じく再生された鍵が記録し直す・決定 5）。
    next.recording = std::nullopt;
    const auto selection = replayed_selection(state, key);
    if (!selection.has_value())
    {
        // Esc は鳴らさずに打ち消す（`@<Esc>x` は x が効く・#1369）。`.` の記録は vim_recorded が
        // 決め直すので、上の記録を消した next と同じ答えになる。
        return escaped_or_refused(state, key);
    }
    next.last_macro = selection.value().name;
    const VimRegister source = register_read(state, selection);
    std::vector<VimKey> keys = source.kind == VimRegisterKind::uninitialized
                                   ? std::vector<VimKey>{}
                                   : vim_keys_of_text(source.text);
    if (keys.empty())
    {
        return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::refused);
    }
    return VimStep{std::move(next), VimReplay{std::move(keys), std::nullopt, VimReplayKind::macro,
                                              resolved_count(state)}};
}
} // namespace detail

namespace
{
// ---------------------------------------------------------------- `.`（ADR 0030）

// 記録の回数を 10 進の桁に戻す。表が数字を回数として読み直すので、再生に特別な経路は要らない。
[[nodiscard]] std::vector<VimKey> count_keys(VimCount count)
{
    std::vector<VimKey> reversed;
    for (std::size_t rest = count.value; rest > 0; rest /= decimal_base)
    {
        const auto digit = static_cast<char32_t>(U'0' + rest % decimal_base);
        reversed.emplace_back(VimCharacter{digit});
    }
    return std::vector<VimKey>{reversed.rbegin(), reversed.rend()};
}

// 再生する鍵の列（決定 6）。回数の桁を先頭に展開するだけで、鍵の意味は engine の表が決める。
[[nodiscard]] std::vector<VimKey> replayed_keys(const std::optional<VimCount> &count,
                                                const std::vector<VimKey> &keys)
{
    std::vector<VimKey> result =
        count.has_value() ? count_keys(count.value()) : std::vector<VimKey>{};
    result.insert(result.end(), keys.begin(), keys.end());
    return result;
}

// `"{name}.` の再生の回数（ADR 0030 の追補 #1337）。Vim 9.1 の nv_dot は名前を消さないので、
// normal_cmd は名前が残るあいだ opcount（`.` の回数）を持ち越し、start_redo が積んだ同じ回数の
// 桁に掛ける。`:help .` の文には無い振る舞いで、実測のまま写す。2 乗をやめるならここだけを
// 変える。積は normal_cmd と同じく vim_count_limit に貼り付ける。
[[nodiscard]] VimCount register_dot_count(VimCount count) noexcept
{
    if (count.value != 0 && count.value > vim_count_limit / count.value)
    {
        return VimCount{vim_count_limit};
    }
    return VimCount{count.value * count.value};
}

// 再生の回数（決定 6）。`.` に回数が無ければ記録の回数、あれば `.` の回数（#1319 の積）で
// 置き換え、`"{name}` が残っていれば register_dot_count を通す（#1337）。
[[nodiscard]] std::optional<VimCount> replayed_count(const VimState &state,
                                                     const VimRepeatRecord &record) noexcept
{
    if (!state.count.has_value())
    {
        return record.count;
    }
    if (!state.selected_register.has_value())
    {
        return state.count;
    }
    return register_dot_count(state.count.value());
}

// `"{name}.` の名前を再生の鍵の頭に置く（ADR 0030 の追補 #1337）。記録の頭が `"` なら
// start_redo の積む記録の名前が後から選び直すので置かない（番号送りもその名前の上で進む）。
// 再生した鍵は名前ごと記録し直されるので、続く `.` も同じ名前へ書く。
[[nodiscard]] std::vector<VimKey> register_prefixed(const VimState &state, std::vector<VimKey> keys)
{
    const VimKey quote{VimCharacter{U'"'}};
    if (!state.selected_register.has_value() || (!keys.empty() && keys.front() == quote))
    {
        return keys;
    }
    const VimRegisterSelection selection = state.selected_register.value();
    const char32_t name = selection.append ? static_cast<char32_t>(selection.name) - (U'a' - U'A')
                                           : static_cast<char32_t>(selection.name);
    keys.insert(keys.begin(), {quote, VimKey{VimCharacter{name}}});
    return keys;
}

// `.` の番号送り（ADR 0050 の決定 8）。記録の先頭に並ぶ `"{名前}` の組の最後の組の名前が
// `1`〜`8` なら 1 つ進めた鍵列を返す。`9` `0` `-` と名前つき、先頭の組でない `"1`（INSERT で
// 打った本文）はそのまま。再生した鍵は記録し直されるので、次の `.` はさらに進む。
[[nodiscard]] std::vector<VimKey> numbered_advanced(std::vector<VimKey> keys)
{
    const auto quote = [&keys](std::size_t at)
    { return keys.at(at) == VimKey{VimCharacter{U'"'}}; };
    std::optional<std::size_t> last_name;
    for (std::size_t at = 0; at + 1 < keys.size() && quote(at); at += 2)
    {
        last_name = at + 1;
    }
    if (!last_name.has_value())
    {
        return keys;
    }
    auto *const name = std::get_if<VimCharacter>(&keys.at(last_name.value()));
    if (name == nullptr || name->code < U'1' || name->code > U'8')
    {
        return keys;
    }
    ++name->code;
    return keys;
}

[[nodiscard]] VimMode visual_mode_of(const VimVisualExtent &extent) noexcept
{
    if (std::holds_alternative<VimLineExtent>(extent))
    {
        return VimMode::visual_line;
    }
    if (std::holds_alternative<VimBlockExtent>(extent))
    {
        return VimMode::visual_block;
    }
    return VimMode::visual;
}

// 矩形の再生のあいだだけ立つ印（ADR 0035 の決定 7）。鍵を 1 つ食べ終わると休止の状態に
// 戻るので、`r` の次キー待ちだけを跨いで消える。
[[nodiscard]] std::optional<VimBlockExtent> replayed_extent(const VimVisualExtent &extent) noexcept
{
    const auto *block = std::get_if<VimBlockExtent>(&extent);
    if (block == nullptr)
    {
        return std::nullopt;
    }
    return *block;
}

// `$` で取った矩形は「各行の内容の終わりまで」を覚え直す（ADR 0035 の決定 7・実測）。
// 桁で覚えた矩形は選び直した両端の桁がそのまま幅になるので、欲しい列は空のままでよい。`$` で
// 選んだ行単位と文字単位も行末の印を持つ（Vim の rv_vcol == MAXCOL の coladvance・`A`
// の終わり・#1436）。
[[nodiscard]] std::optional<VimWantedColumn> replayed_wanted(const VimVisualExtent &extent) noexcept
{
    const auto *block = std::get_if<VimBlockExtent>(&extent);
    const auto *line = std::get_if<VimLineExtent>(&extent);
    const auto *character = std::get_if<VimCharacterExtent>(&extent);
    const bool at_line_end =
        (block != nullptr && block->wish == VimColumnWish::at_line_end) ||
        (line != nullptr && line->wish == VimColumnWish::at_line_end) ||
        (character != nullptr && character->wish == VimColumnWish::at_line_end);
    if (!at_line_end)
    {
        return std::nullopt;
    }
    return VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
}

// VISUAL の記録の再生（ADR 0033 の決定 4・6）。step が記録の種類の VISUAL へ切り替えて大きさを
// 効果に載せ、controller が選び直してから鍵を流す。回数は使わない（固定 Vim も `2.` で大きさを
// 変えない・実測）が、`"{name}.`
// の名前へは書く（register_prefixed・#1337）。組み立て中の記録に同じ大きさを先に置くので、再生した鍵はこの大きさをその
// まま記録し直す＝桁や行が足りずに畳まれても次の `.` は元の大きさで繰り返す（Vim も同じ・実測）。
[[nodiscard]] VimStep replayed_visual(const VimState &state, const VimRepeatRecord &record,
                                      const VimVisualExtent &extent)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = visual_mode_of(extent);
    next.wanted_column = replayed_wanted(extent);
    next.replayed_block = replayed_extent(extent);
    next.redoing_visual = true;
    next.recording = VimRepeatRecord{std::nullopt, VimRecordedKeys::from({}), extent};
    return VimStep{std::move(next),
                   VimReplay{register_prefixed(state, numbered_advanced(record.keys.owned_keys())),
                             extent, VimReplayKind::repeat, single_step}};
}
} // namespace

namespace detail
{
// `.`。直前の変更が無ければ何も起きない。回数は replayed_count、`"{name}` は register_prefixed が
// 鍵の列に載せる（vim_resting_from が選んだレジスタを捨てても名前は鍵が運ぶ・#1337）。
[[nodiscard]] VimStep repeated_change(const VimState &state)
{
    if (!state.last_change.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::refused);
    }
    const VimRepeatRecord &record = state.last_change.value();
    if (record.visual.has_value())
    {
        return replayed_visual(state, record, record.visual.value());
    }
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimReplay{replayed_keys(replayed_count(state, record),
                                           register_prefixed(
                                               state, numbered_advanced(record.keys.owned_keys()))),
                             std::nullopt, VimReplayKind::repeat, single_step}};
}
} // namespace detail

namespace
{
// ---------------------------------------------------------------- 記録（ADR 0030 の決定 2〜5）

// 本文を変える効果か（決定 3）。写し先が足りなければ std::visit がここで落ちる（CPP-002）。
[[nodiscard]] bool changes_text(const VimNoEffect &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimMoveTo &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimNavigate &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimSelect &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimUndo &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimRedo &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimOpenCommandLine &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimOpenSearch &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimReplay &) noexcept
{
    return false;
}

// `gt` `gT` は繰り返しの対象ではない（ADR 0057 の決定 2・Vim の実測）。
[[nodiscard]] bool changes_text(const VimSwitchTab &) noexcept
{
    return false;
}

// `Ctrl-w` の窓の命令も本文を変えない（ADR 0148 の決定 7）。
[[nodiscard]] bool changes_text(const VimWindowCommand &) noexcept
{
    return false;
}

[[nodiscard]] bool changes_text(const VimRemoveRange &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimRemoveLines &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimInsertString &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimNewLine &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimInsertAt &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimReplaceRange &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimRemoveBlock &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimReplaceBlock &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimInsertBlock &) noexcept
{
    return true;
}

[[nodiscard]] bool changes_text(const VimEffect &effect) noexcept
{
    return std::visit([](const auto &value) { return changes_text(value); }, effect);
}

// `.` の再生は今のキャレットから探し直すので、incsearch の起点は記録に残さない（ADR 0043 の
// 決定 3）。
[[nodiscard]] VimKey replayable(VimKey key)
{
    if (auto *search = std::get_if<VimSearchPattern>(&key))
    {
        search->from = std::nullopt;
    }
    return key;
}

[[nodiscard]] VimRepeatRecord appended(VimRepeatRecord record, VimKey key)
{
    record.keys = record.keys.appended(replayable(std::move(key)));
    return record;
}

// 命令が完了したか（決定 3）。次キー待ち・保留オペレータ・回数・選んだレジスタが空で NORMAL の
// まま。`"a` は回数の桁と違って記録に残る（ADR 0048 の決定 10）。
[[nodiscard]] bool completed(const VimState &next) noexcept
{
    return next.mode == VimMode::normal && !next.input_wait.has_value() &&
           !next.pending.has_value() && !next.count.has_value() &&
           !next.selected_register.has_value();
}

// NORMAL で `x` として効く `<Del>` か（回数の桁を消す `<Del>` と 1 鍵の待ちの `<Del>`
// を除く・#1325）。
[[nodiscard]] bool deletes_like_x(const VimState &before, const VimKey &key) noexcept
{
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    return special != nullptr && *special == VimSpecialKey::delete_key &&
           before.mode == VimMode::normal && !before.input_wait.has_value() &&
           !drops_count_digit(before, key);
}

// NORMAL の鍵を記録へ（決定 2）。回数の桁は記録せず、回数はそのときの積を 1 つだけ残す。
// 次の 1 鍵を待つ状態（`"` の名前・`r` の文字・`f F t T` の対象など）の数字は回数の桁ではなく
// 引数なので記録に残す（ADR 0050 の決定 9 を待ち全体へ・Issue #209）。オペレータ待ちは待ちでは
// ないので `d2w` の `2` は回数のまま。
// 検索の入力行を開く鍵も記録しない。記録に残るのは確定した VimSearchPattern の 1 鍵だけで、
// 再生はその鍵を同じ経路へ流すだけになる（ADR 0032 の決定 3）。
[[nodiscard]] VimRepeatRecord normal_recording(const VimState &before, const VimEffect &effect,
                                               VimKey key)
{
    VimRepeatRecord record = before.recording.value_or(VimRepeatRecord{});
    if (std::holds_alternative<VimOpenSearch>(effect))
    {
        return record;
    }
    if (!before.input_wait.has_value() && std::holds_alternative<VimCharacter>(key) &&
        counts_as_digit(before, std::get<VimCharacter>(key).code))
    {
        return record;
    }
    // 回数の桁を消す `<Del>` も回数の桁と同じく記録しない（#1325）。
    if (drops_count_digit(before, key))
    {
        return record;
    }
    // `x` として効く `<Del>` は Vim の redo に積まれた "dl" と同じく `x` で記録し、前の `"a`
    // も捨てる （nv_optrans の "dl" は名前を持たない・#1325）。`<Del>` のまま残すと `3.`
    // が前に置く回数の 桁を消してしまう。
    if (deletes_like_x(before, key))
    {
        VimRepeatRecord plain{};
        plain.count = combined_count(before);
        return appended(std::move(plain), VimKey{VimCharacter{U'x'}});
    }
    record.count = combined_count(before);
    return appended(std::move(record), key);
}

// `Ctrl-r {name}` が入れた本文を、打った文字の鍵の列にする（#1040）。改行は `<NL>` の文字で、
// INSERT では `<CR>` と同じ改行になる。ほかの制御文字も文字として入る。
[[nodiscard]] std::vector<VimKey> typed_keys_of(const VimEffect &effect)
{
    const auto *const inserted = std::get_if<VimInsertAt>(&effect);
    if (inserted == nullptr)
    {
        return {};
    }
    const std::string_view body = inserted->utf8;
    std::vector<VimKey> keys;
    for (Offset at{0}; at.value < body.size(); at = next_code_point(body, at))
    {
        keys.emplace_back(VimCharacter{code_point_at(body, at)});
    }
    return keys;
}

// 矢印・Home などで区切った後に、まだ何も打っていないか（Vim の arrow_used・#1137）。印は始めの
// 位置の moved で、#1040 の arrow_stopped と同じ合図。
[[nodiscard]] bool arrow_pending(const VimState &state) noexcept
{
    return state.insert_start.has_value() && state.insert_start.value().moved;
}

// 区切った後の挿入の記録（Vim の stop_arrow の `1i`）。取り直した記録は VISUAL の大きさを持たない
// （固定 Vim も `vlcZ<Home>Y<Esc>.` をただの挿入として繰り返す・Issue #91 で実測）。
[[nodiscard]] VimRepeatRecord restarted_insert()
{
    return VimRepeatRecord{
        std::nullopt, VimRecordedKeys::from({}).appended(VimKey{VimCharacter{U'i'}}), std::nullopt};
}

// 入力の待ち（`Ctrl-r {name}` など）の前後の記録。待ちを開く鍵は記録に残さず、区切った後なら
// 入れる本文から取り直す（`ylix<Left><C-r>"<Esc>.` は `a`・#1137）。待ちを終える鍵は入れた本文を
// 打った文字として足す。
[[nodiscard]] VimRepeatRecord waited_recording(const VimState &before, VimRepeatRecord record,
                                               const VimState &next, const VimEffect &effect)
{
    if (next.input_wait.has_value())
    {
        return arrow_pending(before) ? restarted_insert() : record;
    }
    for (VimKey typed : typed_keys_of(effect))
    {
        record = appended(std::move(record), std::move(typed));
    }
    return record;
}

// 何も消せない消す鍵と行の端で動かない矢印（ビープ）は記録を変えない（#1093・#1136）。区切った
// 後の印はビープの後も残る（insert_start_kept・#1137）。
[[nodiscard]] VimState beeped_recording(const VimState &before, VimState next)
{
    next.recording = before.recording;
    return next;
}

// 打った鍵の記録。移動の鍵は記録を区切るだけで、区切った後の最初の移動でない鍵が `i` から
// 取り直す（Vim の stop_arrow・#1137）。
[[nodiscard]] VimRepeatRecord typed_recording(const VimState &before, VimRepeatRecord record,
                                              VimKey key)
{
    if (restarts_insert(key))
    {
        return record;
    }
    return appended(arrow_pending(before) ? restarted_insert() : std::move(record), std::move(key));
}

// INSERT の記録（決定 4）。Esc で 1 つの変更として確定する。区切った後に何も打たずに Esc すれば
// 記録は区切る前のまま（`ix<Left><Esc>.` は `x` を・`vlcZ<Home><Esc>.`
// は大きさごと繰り返す・#1137）。 VISUAL から入った挿入は記録を持たないので、確定もしない（決定
// 5）。 `Ctrl-r {name}` は鍵ではなく入れた本文を記録する（Vim の redo も本文・oracle
// で実測・#1040）。
[[nodiscard]] VimState insert_recording(const VimState &before, VimState next,
                                        const VimEffect &effect, VimKey key)
{
    if (!before.recording.has_value())
    {
        return next;
    }
    if (next.input_wait.has_value() || before.input_wait.has_value())
    {
        next.recording = waited_recording(before, before.recording.value(), next, effect);
        return next;
    }
    if (erased_nothing(key, effect) || moved_nothing(key, effect))
    {
        return beeped_recording(before, std::move(next));
    }
    if (next.mode == VimMode::normal)
    {
        next.last_change = appended(before.recording.value(), key);
        return next;
    }
    next.recording = typed_recording(before, before.recording.value(), std::move(key));
    return next;
}

// `g` の次の鍵 `i`（`gi`）か。`g` の後に INSERT へ入るほかの命令（`gI` など）は記録を変えない。
[[nodiscard]] bool is_gi(const VimState &state, const VimKey &key) noexcept
{
    if (!state.input_wait.has_value())
    {
        return false;
    }
    const auto *const prefix = std::get_if<VimPrefix>(&state.input_wait.value());
    const auto *const character = std::get_if<VimCharacter>(&key);
    return prefix != nullptr && *prefix == VimPrefix::g && character != nullptr &&
           vim_g_action_for(character->code) == VimAction::insert_at_last_insert;
}

// `gi` の記録は `i`（#1039）。Vim の nv_g_cmd は命令を `i` に変えてから挿入を始めるので、`.` は
// `'^` へ行かずにキャレットの所で同じ本文を入れる（実測）。記録の末尾の `g` を `i` に置き換える。
[[nodiscard]] VimRepeatRecord insert_recorded_as_i(const VimState &before, VimRepeatRecord record)
{
    std::vector<VimKey> keys = before.recording.value_or(VimRepeatRecord{}).keys.owned_keys();
    if (!keys.empty())
    {
        keys.pop_back();
    }
    keys.emplace_back(VimCharacter{U'i'});
    record.keys = VimRecordedKeys::from(keys);
    return record;
}

// NORMAL の記録。完了した鍵だけが効果で分かれ、途中の鍵と INSERT へ入る命令は記録を続ける。
[[nodiscard]] VimState normal_recorded(const VimState &before, VimState next,
                                       const VimEffect &effect, VimKey key)
{
    VimRepeatRecord record = normal_recording(before, effect, key);
    if (next.mode != VimMode::normal)
    {
        // VISUAL への遷移は記録を捨てる（決定 5）。INSERT へ入る命令は Esc まで続ける。
        if (next.mode == VimMode::insert)
        {
            next.recording = is_gi(before, key) ? insert_recorded_as_i(before, std::move(record))
                                                : std::move(record);
        }
        return next;
    }
    if (!completed(next))
    {
        next.recording = std::move(record);
        return next;
    }
    if (changes_text(effect))
    {
        next.last_change = std::move(record);
    }
    return next;
}

// 文字単位 VISUAL の大きさ（ADR 0033 の決定 5）。`$` で選んだ範囲は curswant が行末のままなので
// 桁ではなく「行末まで」を覚える。1 行なら桁の個数、複数行なら最終行の絶対桁（どちらも実測）。
// 桁は仮想桁で、範囲の先頭は「最初の桁」・末尾は「最後の桁」で数える（Tab の始まりと終わりで
// 非対称なのは Vim も同じ・ADR 0034 の決定 4 で実測）。
[[nodiscard]] VimVisualExtent character_extent_of(const TextBuffer &text, const VimState &state,
                                                  const Selection &selection, std::size_t lines)
{
    const VimWantedColumn wanted = wanted_column_of(text, state, selection.caret);
    if (wanted.wish == VimColumnWish::at_line_end)
    {
        return VimCharacterExtent{lines, VimColumnWish::at_line_end, VirtualColumn{1}};
    }
    const OffsetRange ordered = selection_range(selection);
    const VirtualColumn end = virtual_column_end(text, ordered.end);
    const VirtualColumn column =
        lines > 1 ? end : VirtualColumn{end.value - virtual_column(text, ordered.begin).value + 1};
    return VimCharacterExtent{lines, VimColumnWish::at_column, column};
}

// VISUAL の変更の「範囲の大きさ」（決定 5）。選択と種類だけから決まり、本文は持たない。
[[nodiscard]] VimVisualExtent visual_extent_of(const VimState &state, const VimEditorView &view)
{
    const OffsetRange ordered = selection_range(view.selection);
    const std::size_t lines =
        line_of(view.text, ordered.end).value - line_of(view.text, ordered.begin).value + 1;
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        // 記録は VISUAL の鍵からしか始まらない（決定 2）。
        std::unreachable();
    case VimMode::visual_line:
        return VimLineExtent{lines, wanted_column_of(view.text, state, view.selection.caret).wish};
    case VimMode::visual_block:
        // 幅だけを引き、行ごとの形は作らない（#1198）。`.` の再生の中は記録の大きさから決め直す
        // 矩形（block_of と同じ vim_replayed_block_range）の幅だけを引く（#1237）。
        return VimBlockExtent{
            lines,
            state.wanted_column.has_value() ? state.wanted_column.value().wish
                                            : VimColumnWish::at_column,
            state.replayed_block.has_value()
                ? vim_replayed_block_width(view.text, view.selection, state.replayed_block.value())
                : vim_block_width(view.text, view.selection, state.wanted_column)};
    case VimMode::visual:
        return character_extent_of(view.text, state, view.selection, lines);
    }
    std::unreachable();
}

// VISUAL の `p` `P` か（#1418・矩形は #1461）。1 鍵の引数を待つ鍵（`r` `f` `"` の次）は除く。
[[nodiscard]] std::optional<VimPutSide> visual_put_side(const VimState &before, const VimKey &key)
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (before.input_wait.has_value() || character == nullptr)
    {
        return std::nullopt;
    }
    const auto action = vim_action_for(character->code);
    if (action == VimAction::put_after)
    {
        return VimPutSide::after;
    }
    return action == VimAction::put_before ? std::optional{VimPutSide::before} : std::nullopt;
}

// VISUAL の `p` `P` の `.`（#1418）。Vim の nv_put_opt は選択を消すために VISUAL の `d`（`P` は
// `"_d`）を do_pending_operator へ流し、その prep_redo が put の記録を上書きするので、`.` は同じ
// 大きさを選び直して消すだけ（`vp` の後の `w.` は貼らない・`vP` の後の `.` は `"1` を書かない・
// 実測）。回数と `"a` の名前は記録に載らない。大きさは記録を始めた鍵（`"a` の `"`）で測った値。
[[nodiscard]] VimStep visual_put_recorded(const VimState &before, const VimEditorView &view,
                                          VimStep step, VimPutSide side)
{
    if (!changes_text(step.effect))
    {
        return step;
    }
    const VimVisualExtent extent =
        before.recording.has_value() && before.recording.value().visual.has_value()
            ? before.recording.value().visual.value()
            : visual_extent_of(before, view);
    std::vector<VimKey> keys;
    if (side == VimPutSide::before)
    {
        keys = {VimKey{VimCharacter{U'"'}}, VimKey{VimCharacter{U'_'}}};
    }
    keys.emplace_back(VimCharacter{U'd'});
    step.next.last_change = VimRepeatRecord{std::nullopt, VimRecordedKeys::from(keys), extent};
    return step;
}

// 大きさを測る種類。大文字の鍵（`X` `D` `Y` `C` `I` `A` と `R`）は Vim の v_visop が行単位へ
// 読み替えた種類で覚える（do_pending_operator の resel_VIsual_mode・#1436）。`vlAX<Esc>j0.` は
// 1 行を選び直して打つ（2 文字ではない・実測）。
[[nodiscard]] VimState measured_state(const VimState &before, const VimKey &key)
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr || before.input_wait.has_value())
    {
        return before;
    }
    const auto action = vim_action_for(character->code);
    if (!action.has_value())
    {
        return before;
    }
    VimState measured = before;
    measured.mode = visual_operated_mode(before.mode, action.value());
    return measured;
}

// VISUAL の記録（ADR 0033 の決定 2・3）。移動・選択の伸縮・`o`・種類の切替・yank は記録を
// 始めず、直前の変更も変えない。変更を起こす鍵（`d x c r`）から記録が始まり、そのときの選択の
// 大きさを載せる。`r` は次の文字まで、`c` は INSERT の鍵を Esc まで（INSERT は insert_recording
// が続ける）。大きさは記録を始めた 1 回だけ測るので、`.` が種を置いた再生では元の大きさが残る。
[[nodiscard]] VimStep visual_recorded(const VimState &before, const VimEditorView &view,
                                      VimStep step, VimKey key)
{
    if (const auto side = visual_put_side(before, key); side.has_value())
    {
        return visual_put_recorded(before, view, std::move(step), side.value());
    }
    // `"a` の後ろも次の鍵待ちと同じく記録を続ける（ADR 0048 の決定 10）。
    const bool waiting =
        step.next.input_wait.has_value() || step.next.selected_register.has_value();
    // 矩形の `I` `A` は入るだけで本文を変えないことがあるが、INSERT の鍵を Esc まで記録する
    // （ADR 0035 の追補 #1042）。
    const bool inserting = step.next.mode == VimMode::insert;
    if (!before.recording.has_value() && !changes_text(step.effect) && !waiting && !inserting)
    {
        return step;
    }
    // 記録を始める鍵の前に打った回数（`Vj2>` の 2）は鍵の頭に残す。再生は選び直した VISUAL へ
    // 同じ回数を流す（`>` `<` の回数は字下げの回数・Vim の redo も回数を持つ・ADR 0015 の決定
    // 11）。 回数を使わない VISUAL の鍵（d x c y r ~ u U）は流しても同じ。
    const std::vector<VimKey> counted =
        before.count.has_value() ? count_keys(before.count.value()) : std::vector<VimKey>{};
    VimRepeatRecord record = before.recording.value_or(
        VimRepeatRecord{std::nullopt, VimRecordedKeys::from(counted),
                        visual_extent_of(measured_state(before, key), view)});
    record = appended(std::move(record), key);
    if (waiting || inserting)
    {
        step.next.recording = std::move(record);
        return step;
    }
    if (changes_text(step.effect))
    {
        step.next.last_change = std::move(record);
    }
    return step;
}
} // namespace

namespace detail
{
// 記録の更新はこの 1 か所だけ（ARC-001）。鍵の意味ではなく、前のモードと効果で分ける。
[[nodiscard]] VimStep vim_recorded(const VimState &before, const VimEditorView &view, VimStep step,
                                   VimKey key)
{
    step.next.last_change = before.last_change;
    // `.` 自身は記録しない。再生する鍵に `.` が入らないので再帰は深さ 1 で止まる（決定 7）。
    // VISUAL の再生は step が置いた「大きさだけの記録」を種として持ち越す（ADR 0033 の決定 4）。
    if (std::holds_alternative<VimReplay>(step.effect))
    {
        return step;
    }
    step.next.recording = std::nullopt;
    switch (before.mode)
    {
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return visual_recorded(before, view, std::move(step), key);
    case VimMode::insert:
        step.next = insert_recording(before, std::move(step.next), step.effect, key);
        return step;
    case VimMode::normal:
        step.next = normal_recorded(before, std::move(step.next), step.effect, key);
        return step;
    }
    std::unreachable();
}

// ------------------------------------------ マクロの録画（ADR 0046 の決定 2）

// 録画中は vim_step に届いた打った鍵をそのまま積む。積まないのは、録画を始める鍵と止める `q`
// （どちらも step の前後で録画が立っていない）、再生の中の鍵と `map` の右辺の鍵（打った鍵では
// ない・右辺の代わりに controller が打った左辺を vim_macro_typed で積む・#1312）、入力行を
// 開くだけの `/` `?` `:`（確定した検索は VimSearchPattern の 1 鍵として積まれる・Ex は後続）。
// 録画は実行の経路を変えない。
[[nodiscard]] VimStep macro_recorded(const VimState &before, const VimEditorView &view,
                                     VimStep step, VimKey key)
{
    if (!vim_key_recorded_in_macro(view.source) || !before.macro_recording.has_value() ||
        !step.next.macro_recording.has_value() ||
        std::holds_alternative<VimOpenSearch>(step.effect) ||
        std::holds_alternative<VimOpenCommandLine>(step.effect))
    {
        return step;
    }
    vim_macro_typed(step.next, std::span<const VimKey>{&key, 1U});
    return step;
}
} // namespace detail

// 録画の書き手の 1 本（engine の macro_recorded と controller の打った左辺が通る・#1312）。その場
// で積む（録画中の鍵ごとに VimState を move しない）。
void vim_macro_typed(VimState &state, std::span<const VimKey> keys)
{
    if (!state.macro_recording.has_value())
    {
        return;
    }
    auto &recording = state.macro_recording.value();
    for (const VimKey &key : keys)
    {
        recording.keys = recording.keys.appended(replayable(key));
    }
}
} // namespace nenenib::core
