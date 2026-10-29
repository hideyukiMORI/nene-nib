#include "VimStepDetail.hpp"

#include "VimRegisterText.hpp"

#include <cstddef>
#include <optional>
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
using detail::failed;
using detail::finished_input_wait;
using detail::line_of;
using detail::macro_name_of;
using detail::macro_record_action;
using detail::macro_recorded;
using detail::register_read;
using detail::register_selection_of;
using detail::repeated_change;
using detail::replayed_macro;
using detail::resolved_count;
using detail::started_macro;
using detail::started_prefix;
using detail::vim_recorded;
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
        return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::refused);
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
    if (view.source == VimKeySource::replayed)
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
// vim_keys_of_text で鍵列にして回数ぶん繋いだ VimReplay を返し、controller が `.` と同じ経路で
// 1 鍵ずつ流す。名前が無い・未使用か空のレジスタはビープして何もしない。
[[nodiscard]] VimStep replayed_macro(const VimState &state, const VimKey &key)
{
    VimState next = finished_input_wait(state);
    // `@` の鍵は `.` の記録に残さない（`.` と同じく再生された鍵が記録し直す・決定 5）。
    next.recording = std::nullopt;
    const auto selection = replayed_selection(state, key);
    if (!selection.has_value())
    {
        return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::refused);
    }
    next.last_macro = selection.value().name;
    const VimRegister source = register_read(state, selection);
    const std::vector<VimKey> keys = source.kind == VimRegisterKind::uninitialized
                                         ? std::vector<VimKey>{}
                                         : vim_keys_of_text(source.text);
    const std::size_t count = resolved_count(state);
    if (keys.empty())
    {
        return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::refused);
    }
    if (count > std::vector<VimKey>{}.max_size() / keys.size())
    {
        return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::too_large);
    }
    std::vector<VimKey> replayed;
    replayed.reserve(keys.size() * count);
    for (std::size_t round = 0; round < count; ++round)
    {
        replayed.insert(replayed.end(), keys.begin(), keys.end());
    }
    return VimStep{std::move(next), VimReplay{std::move(replayed), std::nullopt}};
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
// 桁で覚えた矩形は選び直した両端の桁がそのまま幅になるので、欲しい列は空のままでよい。
[[nodiscard]] std::optional<VimWantedColumn> replayed_wanted(const VimVisualExtent &extent) noexcept
{
    const auto *block = std::get_if<VimBlockExtent>(&extent);
    if (block == nullptr || block->wish == VimColumnWish::at_column)
    {
        return std::nullopt;
    }
    return VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
}

// VISUAL の記録の再生（ADR 0033 の決定 4・6）。step が記録の種類の VISUAL へ切り替えて大きさを
// 効果に載せ、controller が選び直してから鍵を流す。回数は使わない（固定 Vim も `2.` で大きさを
// 変えない・実測）。組み立て中の記録に同じ大きさを先に置くので、再生した鍵はこの大きさをその
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
    return VimStep{std::move(next), VimReplay{numbered_advanced(record.keys.owned_keys()), extent}};
}
} // namespace

namespace detail
{
// `.`。直前の変更が無ければ何も起きない。回数は `.` に付いた回数が優先で、無ければ記録の回数。
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
    const std::optional<VimCount> count = state.count.has_value() ? state.count : record.count;
    return VimStep{
        vim_resting_from(state, state.unnamed_register),
        VimReplay{replayed_keys(count, numbered_advanced(record.keys.owned_keys())), std::nullopt}};
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

// INSERT で記録を `i` から取り直す鍵（決定 4）。ADR 0028 の決定 3 が挿入の反復を切るのと
// 同じ境界で、Vim も移動のあとの入力を新しい挿入として扱う（Home / End / 矢印で実測）。
[[nodiscard]] bool restarts_insert(VimKey key) noexcept
{
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return false;
    }
    switch (std::get<VimSpecialKey>(key))
    {
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
        return true;
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
    case VimSpecialKey::control_r:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_w:
        return false;
    }
    std::unreachable();
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
    record.count = combined_count(before);
    return appended(std::move(record), key);
}

// INSERT の記録（決定 4）。Esc で 1 つの変更として確定し、移動の鍵は `i` から取り直す。
// VISUAL から入った挿入は記録を持たないので、確定もしない（決定 5）。
[[nodiscard]] VimState insert_recording(const VimState &before, VimState next, VimKey key)
{
    if (!before.recording.has_value())
    {
        return next;
    }
    if (next.mode == VimMode::normal)
    {
        next.last_change = appended(before.recording.value(), key);
        return next;
    }
    if (restarts_insert(key))
    {
        // 取り直した記録は VISUAL の大きさを持たない（固定 Vim も `vlcZ<Home>Y<Esc>.` を
        // ただの挿入として繰り返す・Issue #91 で実測）。
        next.recording = VimRepeatRecord{
            std::nullopt, VimRecordedKeys::from({}).appended(VimKey{VimCharacter{U'i'}}),
            std::nullopt};
        return next;
    }
    next.recording = appended(before.recording.value(), key);
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
        return VimLineExtent{lines};
    case VimMode::visual_block:
        return VimBlockExtent{lines,
                              state.wanted_column.has_value() ? state.wanted_column.value().wish
                                                              : VimColumnWish::at_column,
                              block_of(state, view).width};
    case VimMode::visual:
        return character_extent_of(view.text, state, view.selection, lines);
    }
    std::unreachable();
}

// VISUAL の記録（ADR 0033 の決定 2・3）。移動・選択の伸縮・`o`・種類の切替・yank は記録を
// 始めず、直前の変更も変えない。変更を起こす鍵（`d x c r`）から記録が始まり、そのときの選択の
// 大きさを載せる。`r` は次の文字まで、`c` は INSERT の鍵を Esc まで（INSERT は insert_recording
// が続ける）。大きさは記録を始めた 1 回だけ測るので、`.` が種を置いた再生では元の大きさが残る。
[[nodiscard]] VimStep visual_recorded(const VimState &before, const VimEditorView &view,
                                      VimStep step, VimKey key)
{
    // `"a` の後ろも次の鍵待ちと同じく記録を続ける（ADR 0048 の決定 10）。
    const bool waiting =
        step.next.input_wait.has_value() || step.next.selected_register.has_value();
    if (!before.recording.has_value() && !changes_text(step.effect) && !waiting)
    {
        return step;
    }
    // 記録を始める鍵の前に打った回数（`Vj2>` の 2）は鍵の頭に残す。再生は選び直した VISUAL へ
    // 同じ回数を流す（`>` `<` の回数は字下げの回数・Vim の redo も回数を持つ・ADR 0015 の決定
    // 11）。 回数を使わない VISUAL の鍵（d x c y r ~ u U）は流しても同じ。
    const std::vector<VimKey> counted =
        before.count.has_value() ? count_keys(before.count.value()) : std::vector<VimKey>{};
    VimRepeatRecord record = before.recording.value_or(VimRepeatRecord{
        std::nullopt, VimRecordedKeys::from(counted), visual_extent_of(before, view)});
    record = appended(std::move(record), key);
    if (waiting || step.next.mode == VimMode::insert)
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
        step.next = insert_recording(before, std::move(step.next), key);
        return step;
    case VimMode::normal:
        step.next = normal_recorded(before, std::move(step.next), step.effect, key);
        return step;
    }
    std::unreachable();
}

// ------------------------------------------ マクロの録画（ADR 0046 の決定 2）

// 録画中は vim_step に届いた鍵をそのまま積む。積まないのは、録画を始める鍵と止める `q`
// （どちらも step の前後で録画が立っていない）、再生の中の鍵（打った鍵ではない）、入力行を
// 開くだけの `/` `?` `:`（確定した検索は VimSearchPattern の 1 鍵として積まれる・Ex は後続）。
// 録画は実行の経路を変えない。
[[nodiscard]] VimStep macro_recorded(const VimState &before, const VimEditorView &view,
                                     VimStep step, VimKey key)
{
    if (view.source == VimKeySource::replayed || !before.macro_recording.has_value() ||
        !step.next.macro_recording.has_value() ||
        std::holds_alternative<VimOpenSearch>(step.effect) ||
        std::holds_alternative<VimOpenCommandLine>(step.effect))
    {
        return step;
    }
    auto &recording = step.next.macro_recording.value();
    recording.keys = recording.keys.appended(replayable(std::move(key)));
    return step;
}
} // namespace detail
} // namespace nenenib::core
