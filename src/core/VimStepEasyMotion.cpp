#include "VimStepDetail.hpp"

#include "EasyMotion.hpp"
#include "EasyMotionCancel.hpp"
#include "EasyMotionJump.hpp"
#include "EasyMotionKind.hpp"
#include "EasyMotionNarrowed.hpp"
#include "EasyMotionOutcome.hpp"
#include "EasyMotionState.hpp"
#include "VimCharacter.hpp"
#include "VimInputWait.hpp"
#include "VimKeyTable.hpp"
#include "VimNoEffect.hpp"
#include "VimPrefix.hpp"
#include "VimRepeatFailure.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// イージーモーションの鍵の道（ADR 0163 の決定 1・3〜5）。`\` `\` の接頭辞・f F s の文字の待ち・
// 印の鍵を engine の 1 鍵として受ける。跳びは exact_jump（`` ` `` と同じ着地・`''` を置く）で、
// 移動なので `.`・レジスタ・undo・VimStep.change は動かない。印が無いときの打鍵の道は通らない
// （`\` の 1 回目は鍵の表を 1 回引くだけ・決定 7）。
namespace nenenib::core
{
using detail::escaped_or_refused;
using detail::exact_jump;
using detail::failed;
using detail::finished_input_wait;
using detail::started_prefix;

namespace
{
constexpr char32_t backslash = 0x5C;

// f F s は跳び先を決める文字を 1 つ待つ（決定 4）。
[[nodiscard]] bool takes_character(EasyMotionKind kind) noexcept
{
    switch (kind)
    {
    case EasyMotionKind::character_after:
    case EasyMotionKind::character_before:
    case EasyMotionKind::character_around:
        return true;
    case EasyMotionKind::word_start_after:
    case EasyMotionKind::word_start_before:
    case EasyMotionKind::word_end_after:
    case EasyMotionKind::line_below:
    case EasyMotionKind::line_above:
    case EasyMotionKind::word_start_around:
        return false;
    }
    std::unreachable();
}

// 見えている跳び先を作る（決定 3）。表示領域が無ければ状態をそのままに求め、controller が同じ鍵を
// 流し直す（ADR 0129 の決定 1）。0 個は鳴らし、1 個は印を出さずに跳び、2 個以上は印を出して待つ。
[[nodiscard]] VimStep begun(const VimState &state, const VimEditorView &view, EasyMotionKind kind,
                            std::optional<char32_t> character)
{
    if (!view.viewport.has_value())
    {
        VimStep step{state, VimNoEffect{}};
        step.viewport_request = true;
        return step;
    }
    std::vector<Offset> targets = easymotion_targets(view, kind, character);
    VimState next = finished_input_wait(state);
    if (targets.empty())
    {
        return failed(VimStep{std::move(next), VimNoEffect{}}, VimRepeatFailure::not_found);
    }
    if (targets.size() == 1)
    {
        return exact_jump(next, view, targets.front());
    }
    std::vector<std::string> labels = easymotion_labels(targets.size());
    next.input_wait = VimInputWait{kind};
    next.easy_motion = EasyMotionState{kind, std::move(targets), std::move(labels), std::string{}};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep answered(const VimState &state, const VimEditorView &, const VimKey &,
                               const EasyMotionNarrowed &narrowed)
{
    VimState next = state;
    next.easy_motion = narrowed.state;
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep answered(const VimState &state, const VimEditorView &view, const VimKey &,
                               const EasyMotionJump &jump)
{
    return exact_jump(finished_input_wait(state), view, jump.target);
}

[[nodiscard]] VimStep answered(const VimState &state, const VimEditorView &, const VimKey &key,
                               const EasyMotionCancel &)
{
    return escaped_or_refused(state, key);
}

// 印が出ているあいだの鍵（決定 5）。
[[nodiscard]] VimStep labeled(const VimState &state, const VimEditorView &view,
                              const EasyMotionState &shown, const VimKey &key)
{
    const EasyMotionOutcome outcome = easymotion_step(shown, key);
    return std::visit([&](const auto &value) { return answered(state, view, key, value); },
                      outcome);
}

// f F s の文字の待ち（決定 4）。controller の入力行が確定した最初の 1 文字を VimCharacter で流す。
[[nodiscard]] VimStep character_awaited(const VimState &state, const VimEditorView &view,
                                        EasyMotionKind kind, const VimKey &key)
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr)
    {
        return escaped_or_refused(state, key);
    }
    return begun(state, view, kind, character->code);
}

[[nodiscard]] std::optional<EasyMotionKind> kind_of(const VimKey &key) noexcept
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr)
    {
        return std::nullopt;
    }
    return vim_easymotion_kind_for(character->code);
}
} // namespace

namespace detail
{
// `\` の次の鍵（決定 1）。`\` なら動作を待ち、回数は捨てる。ほかの鍵は鳴らして保留を捨てる（Esc は
// 鳴らさない）。
[[nodiscard]] VimStep leader_step(const VimState &state, const VimKey &key)
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr || character->code != backslash)
    {
        return escaped_or_refused(state, key);
    }
    VimState next = state;
    next.count = std::nullopt;
    return started_prefix(next, VimPrefix::leader_leader);
}

// `\` `\` の次の鍵（決定 1・2）。表の動作なら跳び先を作るか文字を待ち、表に無い鍵は鳴らして保留を
// 捨てる。
[[nodiscard]] VimStep leader_leader_step(const VimState &state, const VimEditorView &view,
                                         const VimKey &key)
{
    const auto kind = kind_of(key);
    if (!kind.has_value())
    {
        return escaped_or_refused(state, key);
    }
    if (!takes_character(kind.value()))
    {
        return begun(state, view, kind.value(), std::nullopt);
    }
    VimState next = state;
    next.input_wait = VimInputWait{kind.value()};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep easymotion_awaited(const VimState &state, const VimEditorView &view,
                                         EasyMotionKind kind, const VimKey &key)
{
    if (state.easy_motion.has_value())
    {
        return labeled(state, view, state.easy_motion.value(), key);
    }
    return character_awaited(state, view, kind, key);
}
} // namespace detail
} // namespace nenenib::core
