# 大容量本文を同じ経路へ接続する移行表

2026-10-13 / #468 / [ADR0127](../adr/0127-large-file-integration-reserves-prepares-and-commits.md)。main `fba966b`の実コードを読み取った設計記録。製品移行と下記試験は未実施。#452の性能受理や#467の取消実験結果を先取りしない。

## byte取得の入口と直接caller

行数/byte長/索引だけの位置計算を、本文byteを使う関数から分ける。`line_end`もCR判定で本文を読むため、名前だけでmetadata関数とみなさない。以下の行番号は調査時点の目印で、移行時は関数名と実差分で照合する。

| 役割 | 現行のファイル・関数 | 取得/所有と必要な移行 |
| --- | --- | --- |
| 本文の正本 | [TextBuffer](../../src/core/TextBuffer.cpp) `segments/text/text_range/line_text/line_end/offset_of/position_of`（288〜423行） | 全piece借用/全文copy/全論理行copy、CR byte、行頭からのUTF-8走査。`line_start/line_terminator_end`の索引だけの答えと分離し、byteが足りなければneedを返す |
| 表示・wrap・hit test | [VisualTextView](../../src/application/VisualTextView.cpp) 52/56/115/134行、[EditorView](../../src/application/EditorView.cpp) 56/78/114行、[EditorController](../../src/application/EditorController.cpp) 3264/3301/3391行 | raw行と表示行の二重所有、折り返しの複数行走査。UIはTextBufferを直接読まず所有LineView等を受ける現在の境界を維持 |
| status/session | `EditorController` 514/530/649/1612/3391行 | caretの行内文字数/offset変換にbyte取得が必要。session記録を隠れた同期readにしない |
| 通常移動 | [CaretMove](../../src/core/CaretMove.cpp) 59〜141行、[OrdinaryCharacterBoundary](../../src/core/OrdinaryCharacterBoundary.cpp) 19/25/32行、`EditorController` 1113/1128/1147行 | 文字の小窓と全行のword/column走査。結合列は複数窓へ跨ぐため固定4bytesで常に完了とはしない |
| 通常編集/undo | `EditorController::replace/undo_edit/redo_edit` 1049/1156/1283/1304/1320行 | removed全範囲、履歴と本文の候補、確定後のcaret追従。必要な旧本文を準備してから一度commit |
| Vim位置/仮想桁 | [VimCaret](../../src/core/VimCaret.cpp) 43/72/86/104/115行、[VirtualColumn](../../src/core/VirtualColumn.cpp) 97/175行、[VimScreenGeometry](../../src/core/VimScreenGeometry.cpp) 135/169/220/337行 | 64bytesから倍増する窓、first_non_blank、全行の仮想桁。走査済み位置/結合状態を保持し有界stepで継続 |
| Vim word/text object | [VimWordMotion](../../src/core/VimWordMotion.cpp) 90/119行、[VimTextObjectRange](../../src/core/VimTextObjectRange.cpp) 44/55/67/595/824/863/901行 | 行を乗り換えるscannerと前後の複数行走査。文書端まで到達し得るので、window不足を動作失敗へ変えない |
| Vim visual/block | [VimVisualRange](../../src/core/VimVisualRange.cpp)、[VimVisualReselect](../../src/core/VimVisualReselect.cpp)、[VimBlockRange](../../src/core/VimBlockRange.cpp) | 各行の全文、矩形幅、yankの結合結果。複数の置換を同じ準備値にまとめ、途中行で失敗しても半分だけ編集しない |
| Vim engine | [VimStep](../../src/core/VimStep.cpp) `indent/operator/yank/block/replace/search-under-caret`（288〜3842行の直接読取） | 全行/選択全範囲を利用。step.nextをeffectsより先に本状態へ採用する現在の順序をprepared transitionへ変更 |
| 検索/ハイライト | [VimSearch](../../src/core/VimSearch.cpp) 123/159行、`EditorController` 360/403/1660/1667/3279行 | count回、全行、折返し、全一致列。need/failure/not_foundを分け、走査と結果数を予算内で継続 |
| macro/dot | `EditorController::perform(VimReplay)/merge_replayed_edits` 2467〜2550行、`apply_block` 2421行 | before/afterの全文copyと覆うEditの作成を除く。確定した編集列を合成し、既存の一回undoと意味上の失敗時prefix保持を守る |
| clipboard | `EditorController` 1224/1247/1275/1491/1506行 | selection/block、改行変換の追加copy。cutは旧本文/候補を準備してから外部write、成功後はI/O不要のcommit |
| 履歴 | [EditHistory](../../src/core/EditHistory.cpp) `pushed/undo/redo/applied/absorbed` | removed/inserted防御copy、coalesce/absorbの一時列、返値copy。保存revisionと予算を同じ確定に結び付ける |
| 保存 | `EditorController::write_utf8_document/write_document/save_document` 2620/2630/2798行、[FilePort](../../src/application/FilePort.hpp)、[Win32FileAdapter](../../src/adapters/win32/Win32FileAdapter.cpp) | UTF-8の同期借用とShift_JIS全文変換。凍結した版から有界供給し、既存の全write/Flush/配置の意味へ一括移行 |

取得量は文書端で終わっても、定数の上限があるとは限らない。長行/大きいselection/全件match/長い結合列を、小ファイルの成功で有界と宣言しない。8MiB長行と最大4KiB局所編集は既存試作計画の別条件で確認する。

## 操作の準備値が含むもの

開始時に文書incarnation/要求/元本文/本文revisionと窓共通Vim/register/録画/repeatの編集世代を固定し、必要な範囲と走査位置、開始時selection/mode、取得したclipboard snapshotを保持する。`ready`は候補本文、履歴、selection、document保存地点、Vim/register/録画/repeat、必要なcaret補正と外部出力予定を持つ。欠けたbyteを要求する段階では、これらを本状態へ採用しない。対象が非activeならbytes回収までにとどめ、activeかつ依存状態が一致するときだけcommitする。別タブで共通状態が変われば古い候補は明示取消にする。

`EditorController::apply/begin_intent`のnoticeやtab歩き等の開始効果と、編集のcommitを分ける。`step_vim`が先に置いている`step.next`、history seal、restore caretを準備段階へ移す。外部出力を伴う場合は、操作ごとに一回性の段階を持ち、再開でclipboardを重複出力しない。`CommitText`は受領済み文字列を保ち、OS確定を取り消したふりをしない。

表示は最後の成功したframeを保持し、古い要求の結果で現在のselectionを上書きしない。本文commit後のDirectWrite失敗は表示の失敗として再描画へ分ける。本文のI/O失敗とlayout失敗を同じrollbackへ混ぜない。

## 測定前に成立させる小さい契約

| 契約群 | 固定する入力/順序と観測 |
| --- | --- |
| 予約 | limit=0、上限一致、1超過、整数最大値近傍、slot不足、bytes不足。全予約か全拒否で、失敗後の台帳不変 |
| 移管と解放 | queued→pending→未collect→利用中→解放、queued取消、pending取消drain、collect後もcancel pin中。最後の利用前に返却せず、二重collect/discard/releaseで増えない |
| 複製と共有 | 同じ割当ての2参照はpayloadを一度計上、実copyは同居量を予約。cache evictionで使用中rangeを追い出さない |
| temp | 名前衝突、生存leaseあり、壊れたmarker、unknown file、reparse/領域外、既知の孤児、保存候補。自分の終了済み退避だけ回収、元/他者/保存結果不明は保持 |
| 通常編集 | removedが複数rangeへ跨ぎ、最初成功→次失敗。本文/selection/history/revision不変。全成功は一度だけcommit |
| Vim/矩形 | need中にstep.next/register/録画/restore caretを本状態へ適用しない。後半行失敗で部分編集なし |
| 古い結果 | 待機→別タブ、Bでregister更新→同revision/selection/modeのAへ戻る、close→同path再open、本文版変更、要求取消→正常OS完了。共通世代も照合し、古い候補でBの効果を上書きしない |
| clipboard | 1回入力snapshot、cut準備失敗ならwriteなし、write失敗なら本文保持、再開でwrite/編集を重複しない |
| 検索 | 断片境界の一致、need→再開、read失敗とnot_foundの区別、折返し一周の重複なし、結果予算超時の明示結果 |
| macro/dot | 成功key→need→再開→意味上の失敗。成功prefixだけ保持し後続を捨て、外部効果を再生し直さない。離れた2編集と入れ子でも一回undo。既存net-zero集約では始点revision/保存表示へ戻り、完了した外部効果は残す |
| 受領済みIME | 通常/Vimそれぞれでneed/失敗/取消を挟み、未commit文字を空成功として失わない。既存の文字ごとのVim意味も維持 |
| 保存版 | 保存A開始→undo→別入力Bで同position→A完了はBがmodified。別にA保存開始→B編集→A完了→undo Aはsaved。undo/redo、coalesce/absorb、seal、非空/net-zero集約、再生内保存、同path再open、retain_document、古い配置完了の遅着を分ける |
| 凍結と保存 | 保存snapshot取得→同じ旧tailへの新入力を試みる。snapshotのbytes/索引は不変、新入力は別chunk。BOMは一度だけ |
| 配置 | read/write/Flush失敗、配置前取消、placing後取消→成功、配置error/結果不明、保存先世代変更。実際のdisk結果と本文の保存印を別に照合 |

上記は未実行の受け入れ計画である。実ディスクを満杯にする故障、利用者データの置換、別プロセスの強制終了は使わず、I/O境界へ失敗を注入する。nativeの確認は隔離した小さいfixtureだけで行い、実装時に直接影響する群を選ぶ。全Vim/全GUI/全性能を毎工程で回さない。

## 実装順と製品統合の境界

以下の2と3は**同じ製品候補内の実装順**であり、別々のmain統合単位ではない。本文revisionを使うcaller、履歴のbefore/after、保存のrange供給と版管理がすべて揃うまでmainへ統合しない。途中候補は専用branch内に留める。

1. **接続前の資源台帳と退避回収。** #465/#467の観測に続き、slot/bytes/volumeの予約・移管・最後の解放・所有確認を小さい隔離実験で成立させる。実験を製品の別経路として残さない。OS結果が不明な保存tempの保全はこの段階で削除方針から除く。
2. **本文取得の一括移行。** 正典TextBufferのoriginal表現、全byte取得口、上表の全caller、型付きport、prepared transition、Vim/検索のcontinuation、macro集約を同じ候補で更新する。小さい既存入力も同じモデルを通し、旧同期validator/本文経路を並走させない。関連機能の意味契約と故障群を選んで確認する。
3. **保存版の一括移行。** Documentのsaved_position、EditHistoryのbefore/after、active/parkedのsave表示、保存開始/完了、AddChunk凍結、同じFilePort保存を一体で変更する。本文revisionを先に未使用fieldとしてmainへ入れない。単位2の本文モデルと整合させる。
4. **実容量と応答の受理。** 上の機能/故障契約が揃った候補で、#440の固定計画と#452の受理条件へ戻る。2GiB/512MiB/100msと長行を測り、結果から製品上限を判断する。測定待ちの設計/小契約と、性能受理を混同しない。

この表が一括移行の境界であり、今この文書だけで製品変更を完了としない。段階を越えた二重APIが必要なら、隠し設定で逃げず、期限/撤去証明付きwaiverを先に判断する。Waivers none。
