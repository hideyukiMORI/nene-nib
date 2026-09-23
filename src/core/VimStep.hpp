#pragma once

#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimRepeatFailure.hpp"
#include "VimSearchNotice.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 1 つの鍵の結果。次の状態と、application が本文へ写す効果 1 つ（ADR 0012 の決定 2）。
struct VimStep
{
    VimState next;
    VimEffect effect;
    // 検索が残した報せ（ADR 0032 の決定 5）。折り返しは効果と同時に出るので効果の選択肢では
    // なく、1 打鍵の結果に添える値である。controller が command_message へ写す。
    std::optional<VimSearchNotice> notice = std::nullopt;
    // 鍵が閉じた失敗（ビープ）で終わったか（ADR 0046 の決定 3）。再生（`.` と `@`）の中なら
    // controller が残りの鍵を捨てる。打った鍵では何もしない（本文も状態も効果のとおり）。
    std::optional<VimRepeatFailure> failure = std::nullopt;
};

// 入力行の取消のあとの状態（ADR 0032 の決定 1）。保留中のオペレータと回数と組み立て中の
// 鍵を捨て、モードと直前の変更は保つ。何を捨てるかを決めるのは engine の側である（ARC-004）。
[[nodiscard]] VimState vim_cancelled_input(const VimState &state);

// 外から割り込まれたあとの状態（Issue #92）。クリック・Ctrl+Z / Ctrl+Y・全選択・engine を
// 通らない編集のあとでは、組み立て中の入力（INSERT の入力記録・組み立て中の `.` の記録・
// 保留オペレータ・回数・次キー待ち）はどれも engine の外の出来事を織り込めないので捨てる。
// モード・直前の変更・検索と文字検索の記憶は保つ。何が割り込みかを決めるのは呼ぶ側だが、
// 何を捨てるかを決めるのは engine の側である（ARC-004）。
[[nodiscard]] VimState vim_interrupted(const VimState &state);

// マクロのレジスタへ鍵の列を置いたあとの状態（ADR 0046 の決定 6・`:let @a = "…"` に当たる）。
// 名前の a〜z は置き換え、A〜Z は追記、ほかの名前は状態を変えない。
[[nodiscard]] VimState vim_macro_stored(const VimState &state, char32_t name,
                                        const std::vector<VimKey> &keys);

// 検索の入力行が開いているあいだの回数（`2/be` の 2・`d2/` なら積）。確定の鍵が engine の中で
// 使う回数と同じ値で、incsearch の preview と Ctrl-G / Ctrl-T が同じ回数で探すために読む
// （ADR 0043 の決定 1・2）。
[[nodiscard]] std::size_t vim_search_count(const VimState &state) noexcept;

// Vim エンジンの唯一の入口（ARC-001）。純関数で、時刻・OS・スレッドを持たない（ARC-007）。
// 本文・選択・表示領域を 1 回だけ借用する（ADR 0019 の決定 1）。NORMAL / INSERT は
// selection の caret だけを読み、VISUAL は anchor も範囲の片端として読む。
[[nodiscard]] VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key);
} // namespace nenenib::core
