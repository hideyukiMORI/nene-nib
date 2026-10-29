#pragma once

#include "Offset.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimMarks.hpp"
#include "VimRepeatFailure.hpp"
#include "VimRowRequest.hpp"
#include "VimSearchNotice.hpp"
#include "VimState.hpp"
#include "VimWantedColumn.hpp"

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
    // `"+` `"*` へ書いた本文と、`:set clipboard=unnamed` でレジスタを選ばずに無名へ書いた本文
    // （ADR 0051 の決定 6・追補（#848）の決定 C）。engine の値のまま（本文は LF・種類つき）で、
    // controller が文書の改行に直して ClipboardPort へ出す。状態の欄には置かない。
    std::optional<VimRegister> clipboard = std::nullopt;
    // u と Ctrl-r の戻り先（ADR 0052 の決定 7）。Vim のオペレータが範囲の先頭へ動いてから保存する
    // のと同じに、controller は効果を写す前にキャレット（選択は畳む）をここへ置く。空はいまの
    // キャレットのまま。書くのはオペレータと VISUAL の d x c r（決定 8〜10）。
    std::optional<Offset> restore = std::nullopt;
    // 必要な行の文字組みを値で要求する。同じ状態と鍵で解決し直すまで効果は適用しない。
    std::optional<LineNumber> layout_request = std::nullopt;
    // 行の中の画面行を値で要求する（ADR 0144 の決定 2）。layout_request と同じ往復で、controller が
    // VimRowSpan を VimLayoutView.spans に積んで同じ鍵を流し直す。ほかの要求と同時には立てない。
    std::optional<VimRowRequest> row_request = std::nullopt;
    // 表示領域を値で要求する（ADR 0129 の決定 1）。立てた step は効果なし・状態そのままで、
    // controller が viewport を渡して同じ鍵を流し直す。layout_request と同時には立てない。
    bool viewport_request = false;
    // マークの新しい値（ADR 0152 の決定 2）。`m` と飛ぶ鍵（前の位置）が置く。controller は効果を
    // 写す前に文書へ置き、写した編集が行をずらしたら marks_adjusted で直す（決定 6）。
    std::optional<VimMarks> marks = std::nullopt;
    // INSERT を抜けた位置（`'^`・ADR 0152 の追補・#1039）。効果を写した後のキャレットから同じ行を
    // 右へ何バイトかで表す（Esc が 1 文字左へ戻した分・戻らなければ 0）。回数の繰り返しを入れた
    // 後ろにもなる（Vim の b_last_insert は繰り返しの後のカーソル・実測）。写した編集の後の位置
    // なので marks には入れず、controller が効果を写した後のキャレットから文書のマークへ置く
    // （CRLF の文書で入れた本文の改行が長くなっても同じ行の中の差は変わらない）。
    std::optional<std::size_t> insert_left = std::nullopt;
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

// タブを切り替えたあとの状態（ADR 0056 の決定 4）。割り込みと同じく組み立て中の入力を捨て、
// モードは NORMAL へ戻し、文書ごとの値（欲しい列と 'scroll'）は相手の文書の値に置き換える。
// レジスタ・マクロ（録画中の録画も）・直前の検索と文字検索・`.` の直前の変更・hlsearch と
// incsearch は窓全体で 1 つなので保つ。何を捨てるかを決めるのは engine の側である（ARC-004）。
[[nodiscard]] VimState vim_switched_document(const VimState &state,
                                             std::optional<VimWantedColumn> wanted_column,
                                             std::optional<VimCount> scroll_lines);

// 名前つきレジスタへ本文を置いたあとの状態（ADR 0048 の決定 6・`:let @a = "…"` に当たる）。
// 名前の a〜z は置き換え、A〜Z は録画の追記と同じ規則で繋ぎ（本文の末尾の改行の手前・種類は
// 保つ）、`0`〜`9` と `-` は置き換え（ADR 0050 の決定 7）、ほかの名前は状態を変えない。
[[nodiscard]] VimState vim_register_stored(const VimState &state, char name,
                                           const VimRegister &value);

// 鍵 key を流す直前に OS のクリップボードを読むか（ADR 0051 の決定 3 と追補（#848）の決定 D）。
// `"+` `"*` を選んでいるか、`@` の次の名前を待っている（`q` の待ちは含めない）とき真で、
// vim_reads_unnamed_clipboard が真のときも真。controller は vim_step を呼ぶ直前にこれを見て、
// 真なら 1 回読んで写しを置く。何が読む状態かを決めるのは engine の側である（ARC-004）。
[[nodiscard]] bool vim_reads_clipboard(const VimState &state, const VimKey &key) noexcept;

// 読んだ本文を無名へ置く側か（追補（#848）の決定 D）。`:set clipboard=unnamed` で、レジスタを
// 選ばず・オペレータも次の鍵の待ちも無い NORMAL の `p` `P`（回数の後でも）のとき真。
// 真なら controller は vim_unnamed_loaded、偽なら vim_clipboard_loaded で置く（2
// つの側は重ならない）。
[[nodiscard]] bool vim_reads_unnamed_clipboard(const VimState &state, const VimKey &key) noexcept;

// `"+` `"*` の写しを置いたあとの状態（ADR 0051 の決定 3）。写しを置くだけで、ほかは変えない。
// 写しを置く口はこの 1 本（vim_register_stored は `+` `*` を受けない）。
[[nodiscard]] VimState vim_clipboard_loaded(const VimState &state, VimRegister value);

// OS の本文で無名レジスタの写しを置き換えたあとの状態（追補（#848）の決定 D・G）。読んだ本文が
// 今の無名の OS
// の形（vim_clipboard_text_of）と同じなら、自分が書いた本文として無名を種類ごと残す。`"0`〜`"9`
// `"-` 名前つき・`last_macro` は変えない。読めなかった・空の本文は空のレジスタとして置く。
[[nodiscard]] VimState vim_unnamed_loaded(const VimState &state, VimRegister value);

// 検索の入力行が開いているあいだの回数（`2/be` の 2・`d2/` なら積）。確定の鍵が engine の中で
// 使う回数と同じ値で、incsearch の preview と Ctrl-G / Ctrl-T が同じ回数で探すために読む
// （ADR 0043 の決定 1・2）。
[[nodiscard]] std::size_t vim_search_count(const VimState &state) noexcept;

// 表示領域を読む口の 1 本（ADR 0129 の決定 1）。画面を読む鍵（vim_reads_viewport）は入口で
// 無ければ viewport_request を立てて返るので、読む所へ来るときは値がある。無い値は読まれない。
[[nodiscard]] VimViewport vim_viewport_of(const VimEditorView &view) noexcept;

// Ex の行番号だけの命令 `:N` `:$` の着地（ADR 0022 の追補・Issue #847）。鍵の `{N}G` と同じ
// 着地の 1 本（document_last）を通るので、最後の行への切り詰め・最初の非空白・欲しい列は `G` と
// 同じに決まる。Vim 9.1 の `:N` は jump ではないので、`G` と違って `''` に前の位置を置かない。
// line が空なら最後の行（回数なしの `G`）、0 は 1 行目。
// 鍵ではないので `.` とマクロの記録には積まない。controller は鍵と同じ写しの道で結果を写す。
[[nodiscard]] VimStep vim_line_jumped(const VimState &state, const VimEditorView &view,
                                      std::optional<std::size_t> line);

// Vim エンジンの鍵の唯一の入口（ARC-001）。純関数で、時刻・OS・スレッドを持たない（ARC-007）。
// 本文・選択・表示領域を 1 回だけ借用する（ADR 0019 の決定 1）。NORMAL / INSERT は
// selection の caret だけを読み、VISUAL は anchor も範囲の片端として読む。
[[nodiscard]] VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key);
} // namespace nenenib::core
