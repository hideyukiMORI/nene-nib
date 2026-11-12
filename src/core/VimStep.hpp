#pragma once

#include "ExSubstituteRequest.hpp"
#include "Offset.hpp"
#include "Selection.hpp"
#include "VimChangeMark.hpp"
#include "VimCommandMotion.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimLastVisual.hpp"
#include "VimMarks.hpp"
#include "VimMode.hpp"
#include "VimRepeatFailure.hpp"
#include "VimRowRequest.hpp"
#include "VimSearchNotice.hpp"
#include "VimState.hpp"
#include "VimWantedColumn.hpp"

#include <cstddef>
#include <optional>
#include <span>
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
    // VISUAL の `p` `P` の後の前の選択（`gv`・ADR 0018 の追補 #1418）。Vim の nv_put_opt は貼った
    // 後に b_visual の両端を貼った本文の両端（b_op_start・b_op_end）へ置き直す。写した編集の後の
    // 位置なので marks には入れず、controller が効果を写した後に文書の前の選択へ置く
    // （insert_left と同じ形・行のずれは掛けない）。
    std::optional<VimLastVisual> visual_after = std::nullopt;
    // 鍵が Vim の undo の単位を作ったか・本文を変えたと数えるか（ADR 0181 の決定 1・2）。空は
    // Vim が u_save を呼ばない鍵。本文を置き換える編集の効果（vim_replaces_text）を返す step は
    // 必ず changed で、vim_step の出口の 1 か所（change_marked）が立てる。編集を返さずに印だけを
    // 持つ形（U だけの saved_only と、範囲が空の U ＋ C）は、効果を作る所が立てる。controller は
    // 編集の効果が無い印から「空の単位」を履歴に積み（決定 3）、`'.` は changed のときだけ置く
    // （決定 5）。失敗（failure）と同時に立つこともある（空のレジスタの `p`・決定 7）。changed の
    // 印は Vim が changed_* に渡した位置を持つ（追補 #1374）。位置は既定で効果の置き換えの始まり
    // （change_marked）で、矩形の効果は Vim の op_* の式を作る所が書く。
    std::optional<VimChangeMark> change = std::nullopt;
    // 鍵の中で Vim の edit() が先頭を確かめたカーソル（ADR 0019 の追補 #1374）。NORMAL の `r<CR>`
    // は nv_replace が del_chars の後に改行と Esc を積んで invoke_edit を呼び、edit()
    // は改行を入れる 前に update_topline を流す（edit.c「May need to adjust w_topline to show the
    // cursor.」）。 controller は鍵の終わりの追従の前に、ここで先に 1 回追従する。ほかの鍵は空。
    std::optional<TextPosition> edit_topline = std::nullopt;
};

// 効果が本文を置き換えるか（ADR 0181 の決定 2）。範囲も本文も空の置き換え・行の無い矩形は
// 置き換えない。同じ本文への置き換え（同じ文字への `r`）は置き換える。
[[nodiscard]] bool vim_replaces_text(const VimEffect &effect) noexcept;

// 入力行の取消のあとの状態（ADR 0032 の決定 1）。保留中のオペレータと回数と組み立て中の
// 鍵を捨て、モードと直前の変更は保つ。何を捨てるかを決めるのは engine の側である（ARC-004）。
[[nodiscard]] VimState vim_cancelled_input(const VimState &state);

// 外から割り込まれたあとの状態（Issue #92）。クリック・Ctrl+Z / Ctrl+Y・全選択・engine を
// 通らない編集のあとでは、組み立て中の入力（INSERT の入力記録・組み立て中の `.` の記録・
// 保留オペレータ・回数・次キー待ち）はどれも engine の外の出来事を織り込めないので捨てる。
// モード・直前の変更・検索と文字検索の記憶は保つ。何が割り込みかを決めるのは呼ぶ側だが、
// 何を捨てるかを決めるのは engine の側である（ARC-004）。オペレータ待ちの `:` `/` の入力行を
// 確定の外で離れたときと Ex の命令が断られたときもこれで捨てる（欲しい列は保つ・ADR 0022 の
// 追補 #1549・Vim の clearop）。
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

// 録画中なら打った鍵 keys を state のマクロの録画にその場で積む（Vim の gotchars は打った鍵だけを
// 録る・ADR 0176 の追補 33・#1312）。controller が `map` の表に当たった単位の左辺を、録画が単位の
// 前に立っていて右辺の最初の鍵の後にも立っているときに積む（追補 32）。engine の録画（vim_step に
// 届いた打った鍵）も同じ 1 本を通る。録画していなければ何も変えない。
void vim_macro_typed(VimState &state, std::span<const VimKey> keys);

// 鍵 key を流す直前に OS のクリップボードを読むか（ADR 0051 の決定 3 と追補（#848）の決定 D）。
// `"+` `"*` を選んでいるか、`@` の次の名前を待っている（`q` の待ちは含めない）か、INSERT の
// `Ctrl-r` の次の鍵が `+` `*` のとき（#1040・ほかの名前では読まない）真で、
// vim_reads_unnamed_clipboard が真のときも真。controller は vim_step を呼ぶ直前にこれを見て、
// 真なら 1 回読んで写しを置く。何が読む状態かを決めるのは engine の側である（ARC-004）。
[[nodiscard]] bool vim_reads_clipboard(const VimState &state, const VimKey &key) noexcept;

// 鍵 key を engine が回数の続きの桁として読む `0` か（Vim の no_zero_mapping・ADR 0176 の追補 30・
// #1311）。NORMAL・VISUAL・オペレータ待ちで 1 鍵の引数を待たず、回数が値を持つ間の `0` だけが
// 真（回数の先頭の `0` は行頭へ・オペレータの後の回数は別に数える）。表を引く側
// （vim_map_mode_of）はこれを見て、回数の続きの `0` を表で引かない。
[[nodiscard]] bool vim_continues_count(const VimState &state, const VimKey &key) noexcept;

// 読んだ本文を無名へ置く側か（追補（#848）の決定 D）。`:set clipboard=unnamed` で、レジスタを
// 選ばず・オペレータも次の鍵の待ちも無い NORMAL と VISUAL の `p` `P`（回数の後でも・#1418・
// 矩形は #1461）のとき真。
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

// オペレータ待ちの `:` の確定（ADR 0022 の追補 #1549・Vim の nv_colon）。Ex
// の命令が動かしたキャレット までを文字単位の exclusive
// な移動として保留のオペレータを閉じる。行き先は motion.line があれば
// `:N` と同じ着地（vim_line_jumped と同じ document_last）、無ければ今のキャレット（空の範囲・`d` は
// 何も消さず `c` は INSERT へ入り `y` は空を取る・Vim の実測）。範囲は exclusive の言い換え（行頭で
// 終わる範囲は行単位か前の行の末尾まで）を検索の後ろのオペレータと同じ 1 本で通る。`.` の記録は
// `:` までの記録に motion.typed の文字と `<CR>` を足した鍵の列（Vim の repeat_cmdline）で、再生は
// 同じ入力行の口を通る。マクロの録画には積まない（#1528）。保留のオペレータが無ければ何もしない。
[[nodiscard]] VimStep vim_command_operated(const VimState &state, const VimEditorView &view,
                                           const VimCommandMotion &motion);
// Ex の `:s` の族（`:s` `:&` `:~`）と NORMAL の `&` `g&` の唯一の入口（ADR 0168 の決定
// 1〜5）。範囲を 行に解き（E16 E20 E493）、前の置換と前のパターンを引き（E33 E35）、置換の本体
// substituted の 行ごとの編集を VimReplaceBlock の 1 つの効果（undo 1
// 単位）で返す。前の置換と直前の検索を覚え、
// `''` に置換の前のキャレットを置き、キャレットは最後に置き換えた行の最初の非空白。当たりが無ければ
// E486（`e` なら何もしない）。`n` は数えるだけで報せを出す。鍵ではないので `.` とマクロの記録には
// 積まない。controller は鍵と同じ写しの道で結果を写す。
[[nodiscard]] VimStep vim_substituted(const VimState &state, const VimEditorView &view,
                                      const ExSubstituteRequest &request);

// 本文のダブルクリック・トリプルクリックの VISUAL（#1250・ADR 0056 の決定 9
// の追補）。鍵ではないので
// `.` とマクロの記録には積まない（gvim の do_mouse が Visual を直に始めるのと同じ）。待ち・回数・
// オペレータ・レジスタの名指しは捨て、INSERT は回数の反復なしで離れ（vim_interrupted と同じ
// 捨て方）、前が VISUAL なら前の選択（`gv`）を鍵と同じ 1 か所で覚える。効果は VimSelect で、
// selection は VISUAL の選択（caret は含む側の端）、mode は visual か visual_line。
[[nodiscard]] VimStep vim_pointer_selected(const VimState &state, const VimEditorView &view,
                                           Selection selection, VimMode mode);

// INSERT の `Ctrl-o` の印で INSERT を始め直す（ADR 0182 の決定 2・8）。印が無ければ何も変えない。
// controller は印が due になった鍵の効果を写した後（キャレットは NORMAL
// の寄せの後）と、印を待つ間に 開いた入力行を閉じた後（Ex
// の評価・検索の取消）とクリックの後に呼ぶ。何を始め直すか（位置の 規則・`i` と同じ入口・`.`
// の記録の印）を決めるのは engine である（ARC-004）。
// command_caret は命令の効果を写した後で NORMAL の寄せの前のキャレット（Vim は restart_edit の間
// それが行の内容の終わりにいてよい・決定 2）。engine の外の命令の後は今のキャレット。
[[nodiscard]] VimStep vim_restarted_insert(const VimState &state, const VimEditorView &view,
                                           Offset command_caret);

// INSERT の `Ctrl-o` の命令を待つ間か（印が waiting か due・ステータスの `(insert)`・ADR 0182 の
// 決定 9）。始め直した INSERT と印の無い状態は偽。
[[nodiscard]] bool vim_insert_paused(const VimState &state) noexcept;

// 命令が終わって始め直す時が来たか（印が due・決定 2）。controller は鍵の効果を写した後にこれを
// 見て vim_restarted_insert を呼ぶ（`.` の再生の鍵が残る間は呼ばない・Vim の stuff_empty()）。
[[nodiscard]] bool vim_insert_restart_due(const VimState &state) noexcept;

// Vim エンジンの鍵の唯一の入口（ARC-001）。純関数で、時刻・OS・スレッドを持たない（ARC-007）。
// 本文・選択・表示領域を 1 回だけ借用する（ADR 0019 の決定 1）。NORMAL / INSERT は
// selection の caret だけを読み、VISUAL は anchor も範囲の片端として読む。
[[nodiscard]] VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key);
} // namespace nenenib::core
