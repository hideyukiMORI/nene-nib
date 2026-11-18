// INSERT の補完（`Ctrl-n` `Ctrl-p` `Ctrl-e` `Ctrl-y`・#1845・ADR 0169 の決定 3）。Vim の insexpand.c
// の形を engine が持つ: 選ぶたびに選んだ語を本文に入れ、輪は「候補の列 ＋ 元の接頭辞」、`Ctrl-e` は
// 元の接頭辞へ戻して閉じ、`Ctrl-y` とほかの鍵は確定して閉じる。候補は controller が置いた写し
// （VimState.completion_offer）で、engine は語の表にも port にも触れない（ADR 0051 と同じ形）。
// `.` と回数の記録には補完の鍵を残さず、確定した語を打った本文として足す（Vim の
// ins_compl_fixRedoBufForLeader・oracle で実測: `cwa<C-n><Esc>` の後の `.` は同じ語を入れる）。
#include "VimStepDetail.hpp"

#include "CompletionDirection.hpp"
#include "EditBoundary.hpp"
#include "Utf8.hpp"
#include "VimChange.hpp"
#include "VimChangeMark.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimCompletionOffer.hpp"
#include "VimInsertCompletion.hpp"
#include "VimLineMarks.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
using Words = std::shared_ptr<const std::vector<std::string>>;

// 輪の中の位置 selected の本文（候補の語か、列の端の元の接頭辞）。
[[nodiscard]] std::string_view chosen_text(const VimInsertCompletion &completion) noexcept
{
    return completion.selected < completion.words->size()
               ? std::string_view{completion.words->at(completion.selected)}
               : std::string_view{completion.original};
}

// 元の接頭辞と選んだ語の頭の同じバイト数を、元の接頭辞の Vim の 1 文字の頭へ切り下げたもの（Vim の
// ins_compl_fixRedoBufForLeader の mb_head_off）。
[[nodiscard]] std::size_t shared_head(std::string_view original, std::string_view word) noexcept
{
    // 1 バイトずつ比べる（std::mismatch は MSVC STL の純関数を core の外へ出す・ARC-003）。
    const std::size_t shortest = std::min(original.size(), word.size());
    std::size_t common = 0;
    while (common < shortest && original.at(common) == word.at(common))
    {
        ++common;
    }
    if (common == 0 || common >= original.size())
    {
        return common;
    }
    const Offset at{common};
    return is_boundary(original, at) && !vim_character_continues(original, at)
               ? common
               : vim_character_start(original, at).value;
}

// 補完の鍵（`Ctrl-n` `Ctrl-p` `Ctrl-e` `Ctrl-y`）か。開いている補完はこれ以外の鍵で確定する。
[[nodiscard]] bool is_completion_key(const VimKey &key) noexcept
{
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    if (special == nullptr)
    {
        return false;
    }
    switch (*special)
    {
    case VimSpecialKey::control_n:
    case VimSpecialKey::control_p:
    case VimSpecialKey::control_e:
    case VimSpecialKey::control_y:
        return true;
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::control_r:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_w:
    case VimSpecialKey::control_a:
    case VimSpecialKey::control_x:
    case VimSpecialKey::control_o:
    case VimSpecialKey::delete_key:
        return false;
    }
    std::unreachable();
}

// 補完を始める鍵の向き（`Ctrl-n` は前方・`Ctrl-p` は後方）。ほかの鍵は空。
[[nodiscard]] std::optional<CompletionDirection> direction_of(const VimKey &key) noexcept
{
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    if (special == nullptr)
    {
        return std::nullopt;
    }
    if (*special == VimSpecialKey::control_n)
    {
        return CompletionDirection::forward;
    }
    return *special == VimSpecialKey::control_p ? std::optional{CompletionDirection::backward}
                                                : std::nullopt;
}

// begin からキャレットまでを text に置き換える効果。INSERT の 1 単位に吸収する（ADR 0015）。
[[nodiscard]] VimEffect replaced_with(Offset begin, Offset caret, std::string_view text)
{
    return VimReplaceRange{OffsetRange{begin, caret}, std::string{text},
                           Offset{begin.value + text.size()}, EditBoundary::absorb,
                           VimLineMarks::shifted};
}

// 選ぶ位置を動かした補完と、選んだ本文を入れる効果。
[[nodiscard]] VimStep selected_step(const VimState &state, const VimEditorView &view,
                                    VimInsertCompletion completion)
{
    const Offset begin = completion.begin;
    VimState next = state;
    next.completion_offer = nullptr;
    next.insert_completion = std::make_shared<const VimInsertCompletion>(std::move(completion));
    VimEffect effect =
        replaced_with(begin, view.selection.caret, chosen_text(*next.insert_completion));
    return VimStep{std::move(next), std::move(effect)};
}

// 補完を始める。候補の写しが無い・語の頭がキャレットより後ろなら何もしない（Vim の
// 「Pattern not found」・残りの鍵は捨てない・実測 `A<C-n>z` は z を打つ）。写しの列が空のとき
// （controller が言語サーバーの答えだけを待つとき・ADR 0190 の決定 5）は、本文に触れずに元の
// 接頭辞を選んだ形で開く（答えは vim_completion_reoffered が足す）。
[[nodiscard]] VimStep started_completion(const VimState &state, const VimEditorView &view,
                                         CompletionDirection direction)
{
    const auto &offer = state.completion_offer;
    const Offset caret = view.selection.caret;
    VimState next = state;
    next.completion_offer = nullptr;
    if (offer == nullptr || offer->words == nullptr || caret.value < offer->begin.value)
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    VimInsertCompletion completion{offer->words, view.text.text_range(offer->begin, caret),
                                   offer->begin, 0, direction};
    if (offer->words->empty())
    {
        next.insert_completion = std::make_shared<const VimInsertCompletion>(std::move(completion));
        return VimStep{std::move(next), VimNoEffect{}};
    }
    return selected_step(state, view, std::move(completion));
}
} // namespace

namespace detail
{
bool completion_ends(const VimState &state, const VimKey &key) noexcept
{
    return state.insert_completion != nullptr && !is_completion_key(key);
}

std::vector<VimKey> completion_typed_keys(const VimInsertCompletion &completion)
{
    std::vector<VimKey> keys;
    if (completion.selected >= completion.words->size())
    {
        return keys;
    }
    const std::string_view original = completion.original;
    const std::string_view word = completion.words->at(completion.selected);
    const std::size_t common = shared_head(original, word);
    for (Offset at{common}; at.value < original.size(); at = vim_character_end(original, at))
    {
        keys.emplace_back(VimSpecialKey::backspace);
    }
    for (Offset at{common}; at.value < word.size(); at = next_code_point(word, at))
    {
        keys.emplace_back(VimCharacter{code_point_at(word, at)});
    }
    return keys;
}

VimState completion_settled(const VimState &state)
{
    VimState next = state;
    next.completion_offer = nullptr;
    next.insert_completion = nullptr;
    if (state.insert_completion == nullptr)
    {
        return next;
    }
    for (const VimKey &key : completion_typed_keys(*state.insert_completion))
    {
        if (next.recording.has_value())
        {
            next.recording.value().keys = next.recording.value().keys.appended(key);
        }
        if (next.insert_repeat.has_value())
        {
            next.insert_repeat.value().keys = next.insert_repeat.value().keys.appended(key);
        }
    }
    return next;
}

VimStep completion_stepped(const VimState &state, const VimEditorView &view,
                           CompletionDirection direction)
{
    if (state.insert_completion == nullptr)
    {
        return started_completion(state, view, direction);
    }
    VimInsertCompletion completion = *state.insert_completion;
    // 輪は「候補の列 ＋ 元の接頭辞」。列の向きと同じ鍵は次へ、逆の鍵は前へ回る。
    const std::size_t ring = completion.words->size() + single_step;
    const std::size_t advance = direction == completion.direction ? single_step : ring - single_step;
    completion.selected = (completion.selected + advance) % ring;
    return selected_step(state, view, std::move(completion));
}

VimStep completion_cancelled(const VimState &state, const VimEditorView &view)
{
    if (state.insert_completion == nullptr)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const VimInsertCompletion &completion = *state.insert_completion;
    VimState next = state;
    next.insert_completion = nullptr;
    return VimStep{std::move(next),
                   replaced_with(completion.begin, view.selection.caret, completion.original)};
}

VimStep completion_accepted(const VimState &state)
{
    if (state.insert_completion == nullptr)
    {
        return VimStep{state, VimNoEffect{}};
    }
    // 記録は vim_recorded が鍵の前の状態から作り直すので、ここは補完を閉じて回数の鍵の列に足す。
    return VimStep{completion_settled(state), VimNoEffect{}};
}

VimStep completion_keyed(const VimState &state, const VimEditorView &view, VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::control_n:
        return completion_stepped(state, view, CompletionDirection::forward);
    case VimSpecialKey::control_p:
        return completion_stepped(state, view, CompletionDirection::backward);
    case VimSpecialKey::control_e:
        return completion_cancelled(state, view);
    case VimSpecialKey::control_y:
        return completion_accepted(state);
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::control_r:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_w:
    case VimSpecialKey::control_a:
    case VimSpecialKey::control_x:
    case VimSpecialKey::control_o:
    case VimSpecialKey::delete_key:
        return VimStep{state, VimNoEffect{}};
    }
    std::unreachable();
}

std::optional<VimRepeatRecord> completion_recording(const VimState &before,
                                                    const VimRepeatRecord &record,
                                                    const VimKey &key)
{
    if (!is_completion_key(key))
    {
        return std::nullopt;
    }
    if (direction_of(key).has_value())
    {
        return record;
    }
    if (before.insert_completion == nullptr)
    {
        return std::nullopt;
    }
    VimRepeatRecord kept = record;
    if (std::get<VimSpecialKey>(key) == VimSpecialKey::control_y)
    {
        for (const VimKey &typed : completion_typed_keys(*before.insert_completion))
        {
            kept.keys = kept.keys.appended(typed);
        }
    }
    return kept;
}
} // namespace detail

std::optional<CompletionDirection> vim_starts_completion(const VimState &state,
                                                         const VimKey &key) noexcept
{
    if (state.mode != VimMode::insert || state.input_wait.has_value() ||
        state.insert_completion != nullptr)
    {
        return std::nullopt;
    }
    return direction_of(key);
}

VimState vim_completion_offered(const VimState &state, VimCompletionOffer offer)
{
    VimState next = state;
    next.completion_offer = std::make_shared<const VimCompletionOffer>(std::move(offer));
    return next;
}

VimState vim_completion_reoffered(const VimState &state, Words words)
{
    if (state.insert_completion == nullptr || words == nullptr)
    {
        return state;
    }
    VimInsertCompletion completion = *state.insert_completion;
    const std::string_view chosen = chosen_text(*state.insert_completion);
    std::size_t selected = words->size();
    if (completion.selected < completion.words->size())
    {
        const auto found = std::ranges::find(*words, chosen);
        if (found == words->end())
        {
            return state;
        }
        selected = static_cast<std::size_t>(found - words->begin());
    }
    completion.words = std::move(words);
    completion.selected = selected;
    VimState next = state;
    next.insert_completion = std::make_shared<const VimInsertCompletion>(std::move(completion));
    return next;
}

VimStep vim_completion_chosen(const VimState &state, const VimEditorView &view, std::size_t index)
{
    if (state.insert_completion == nullptr || index >= state.insert_completion->words->size())
    {
        return VimStep{state, VimNoEffect{}};
    }
    VimInsertCompletion completion = *state.insert_completion;
    completion.selected = index;
    const TextPosition at = view.text.position_of(completion.begin);
    VimStep step = selected_step(state, view, std::move(completion));
    step.next = detail::completion_settled(step.next);
    step.change = VimChangeMark{VimChange::changed, at};
    return step;
}

VimState vim_completion_settled(const VimState &state)
{
    if (state.insert_completion == nullptr && state.completion_offer == nullptr)
    {
        return state;
    }
    return detail::completion_settled(state);
}
} // namespace nenenib::core
