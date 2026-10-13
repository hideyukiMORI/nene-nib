# ADR 0126 — OVERLAPPEDの取消要求と最終完了を分けて資源を回収する

- 状態: 提案（隔離実験の契約を固定。実結果と製品採用は未確認）
- 日付: 2026-10-13
- Issue: #467
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-008 / ARC-010 / ARC-012 / CPP-003 / CPP-005 / CPP-006 / CPP-007 / CPP-008 / CPP-011 / CPP-012 / CPP-013 / CPP-016 / QLT-001 / QLT-002 / QLT-004 / QLT-008 / QLT-012 / QLT-013

## 文脈

#465/[ADR0125](0125-source-resource-lifetime-is-probed-before-product-ports.md)で同期の退避資源とI/O操作間の取消を確認した。実行中のreadへ取消を届け、最終結果を回収する契約は残る。固定1本のWin32WorkerはFIFOであり、待っているjobの後へ取消jobを足しても届かない。

取消要求とreadの完了は別の事象である。取消が成功したという返値だけでbufferやOVERLAPPEDを解放すると、まだOSが使っている資源を失う。一方、正常完了が勝ったときにOS結果をabortedへ書き換えると、観測とapplicationの採否を混同する。製品portへ公開する前に、単一readでこの境界を確かめる。

## 決定

1. **隔離した専用実験だけを作る。** `test/467-overlapped-cancel-probe`でOS処理を`src/adapters/win32`、駆動を`tests/adapters`の専用targetへ置く。既存Win32WorkerとFileHandle、通常の固定toolchain・警告・tidy・Debug ASan/UBSanを使う。mainへはADRと実結果だけを収載する。製品worker、FilePort、本文、UTF-8、保存には接続しない。新しいthread実装、隠し設定、規約の除外は追加しない（ADR0068）。
2. **1要求を安定配置した所有物へ結び付ける。** noncopy/nonmoveのoperation storageをheapに置き、read handle、専用manual-reset event、zero-initialized OVERLAPPED、64bytes以下の固定bufferを所有する。OVERLAPPEDは1回だけ使い、handle・event・bufferの位置と同一性を終端でも変更しない。資源はstorageの最後の強参照が消えるときにだけ閉じる。
3. **制御とOS資源の寿命を分ける。** mutexで守るcontrolがphase、sticky cancel latch、storageのweak参照、完了値を持つ。worker jobはstorageを強参照し、取消API呼出しはmutex内でweak参照から局所的な強参照を得て、mutexを離してからOSへ渡す。終端回収と公開が済んでも、先にpinした取消呼出しが戻るまで同じstorageを保持する。control/ticket/結果だけを残してもOS資源は延命しない。
4. **実行と回収は既存worker、取消は制御側から直接届ける。** OS呼出しと待機中にcontrol mutexを保持しない。実行中jobはstop_tokenへcallbackを登録し、明示取消と同じ制御口へ伝える。worker破棄のCancelSynchronousIoをOVERLAPPED取消の代用にしない。job開始時にcaptureの強参照を局所変数へmoveして空にし、直前closureを保持したままworkerが次jobを待っても資源を保持しない。
5. **発行中の取消を取りこぼさない。** queuedからissuingへの変更とcancel latchの確認を同じmutex内で行い、ReadFileは外で呼ぶ。issuing中cancelがERROR_NOT_FOUNDになってもlatchを残す。発行がpendingで戻った側は同じmutex内でpendingへ変更しlatchを確認、既存取消があれば外でexact CancelIoExを再要求する。pendingを見た取消側も外でexact取消を要求する。再要求は発行競合を閉じる規則であり、成功までの再試行ではない。
6. **発行・取消・完了の返値を混ぜない。** ReadFileの即時TRUE、FALSE/ERROR_IO_PENDING、FALSE/その他errorを分ける。CancelIoExの成功、ERROR_NOT_FOUND、別errorは取消APIの観測であり、どれもpendingの終了を意味しない。pendingはGetOverlappedResultで正常成功・ERROR_OPERATION_ABORTED・別の終端errorを回収する。ERROR_IO_INCOMPLETEは未完であり資源を保持する。内部のInternal/InternalHighを読んで状態を推定しない。
7. **公開を採否の境界にする。** OS終端を得たworkerが所有bytesを用意し、control mutex内でpublishedへ一度だけ変更する。そこまでに受理した取消はapplication側をcancelledにし、正常OS結果でもbytesを採用しない。公開後の取消はtoo_lateで、結果を変えない。完了値は実OS結果とapplication側のdelivered/cancelled/failedを別に持つ。成功時だけ不変の所有bytesを公開し、資源を借りるviewや生のhandle/OVERLAPPEDを公開しない。
8. **要求の同一性と操作回数を固定する。** 要求ごとに新しいcontrol identityを割り当て、pathや再使用する番号をIDにしない。実行所有者はmove-only、control ticketはcopy可能で同じ要求へ作用する。runは一回だけで重複は型付き拒否、collectは全ticketを通じて一回だけで、未完ならnot_ready、回収後ならconsumed。ticketの破棄は暗黙取消やOS closeを行わない。所有者のmove元は実行不可であり、未発行の所有者を捨てるとその保持を解く。既に走る要求をticketのmove/破棄で止めない。
9. **解放より先に終端を証明する。** timeout、取消APIのエラー、control側の解放を終端の代わりにしない。privateな有効handleと一度だけ登録したOVERLAPPEDの不変条件の下で、completion queryの成功または終端errorを使う。INVALID_HANDLE/INVALID_PARAMETER等でその不変条件が疑われる場合は通常のI/O失敗として解放を続けず、契約違反としてfail-fastする。blocking queryがINCOMPLETEを返した場合も同様。通常のdriver errorは終端値として記録する。
10. **終了時間は保証しない。** 対象は実行開始した1 jobの取消と回収である。FIFO内でまだ始まっていないjobのshutdown通知、受付上限、公平性、ReadFile自体が戻らない場合、driverが取消を支持しない場合、100msの応答は未決のまま残す。実験成功をdiskや製品の大容量対応へ広げない。

## 遷移と所有

| 状態・操作 | 結果と保持 |
| --- | --- |
| queuedでcancel | accepted、latchのみ。run時にreadを発行せずnot_issued/cancelledを公開しstorageを解放 |
| issuingでcancel | accepted、局所pinでexact取消。not_foundでもlatchを残す |
| issuing→pending | 同じlockでlatchを確認し必要なら再要求。workerは終端回収まで保持 |
| pendingでcancel | accepted、局所pinでexact取消。返値にかかわらずworkerが終端を回収 |
| 未公開で二度目のcancel | already_requested。新しい取消APIを重ねない（発行戻り時の再要求は別） |
| OS終端回収済み・未公開でcancel | accepted。OS結果はそのまま、application側はcancelled |
| publishedでcancel | too_late。latch/結果を変更せず、新しいOS取消を呼ばない |
| 公開後、取消API呼出しがまだ戻らない | worker側の保持を解いても、取消側の局所pinが同じ全資源を保持 |
| collect成功後 | 不変bytesと要求identityだけを移す。OS資源を保持しない |

queuedのpre-cancel以外は、OSの実結果を取得してから公開する。発行失敗なら未完I/Oはなく、失敗codeを保存する。即時成功でも取消latchでOS成功をabortedに変えない。生bufferを読むのはOS完了後のworkerだけとする。

## 事前に固定する確認

scripted transportとnative transportが同じ制御遷移を駆動する。scriptは発行/取消/回収の境界だけを差し替え、別の状態機械を作らない。必要な順序はsemaphoreまたは呼出し境界の固定hookで作り、時計・Sleep・偶然の競合・成功までの再試行を使わない。

| ケース | 必要な観測 |
| --- | --- |
| 即時成功、発行失敗 | 正しいOS結果と採否、成功時だけ内容を公開、発行は一回 |
| queued取消、重複run/cancel/collect | 発行なし、型付き終端、重複操作がOS呼出し/結果を増やさない |
| issuing取消→not_found→pending | latchを保持しexact再要求、終端前に資源を解放しない |
| pending取消→aborted / 正常成功 / 別error | 実OS結果を保持、application側はcancelled、成功bytesも不採用 |
| CancelIoExの通常error | エラーを保存しても終端回収を省略しない |
| 非待機queryがINCOMPLETE→終端 | 未完時にstorageが生存し、終端後だけ解放可能 |
| 終端回収後・公開前のcancel / 公開後のcancel | 前者だけ採否に作用、後者はtoo_late |
| worker終端と取消API返却の競合 | 公開/worker解放後も取消側pin中はstorage生存、返却後は解放 |
| 完了後もworker/control/結果を残す | workerがidleで次jobを待つ間にstorageのweak参照がexpiredになる |
| 実行開始済みjobでworker stop | stop callbackが同じ取消口を通り、終端回収後にjoin可能 |

本物のOSではlocal byte-mode named pipeを1組だけ作る。serverは`PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE`、`PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS`、instance数1、非継承。固定の専用名の衝突は失敗とし、他者のinstanceへ接続しない。pipeはkernel namespaceであり、D配下のdisk fileではない。

connect専用OVERLAPPED/eventでpendingを発行してから同processの主threadでwriterを開き、connectを回収する。readは別のOVERLAPPED/eventを使う。writerを開いたままpayloadを送らず非0長ReadFileを発行し、ERROR_IO_PENDINGを必須観測とする。合図を受けた主threadがexact取消を直接要求し、workerがERROR_OPERATION_ABORTEDを回収する。pendingを観測しなかったケースを取消成功として数えない。

client open失敗時も未完connectをcancel/drainしてから閉じる。read取消が失敗した異常経路はwriterを先に閉じてからreadをdrainし、実際の結果を失敗として記録する。pendingのOVERLAPPEDをstack unwindさせない。Flushは不要であり呼ばない。全handle終了でpipe instanceが消える。停止は専用CTestのTIMEOUTで検出し、timeout時の早期解放を実装しない。

## 公式資料と限定的な採用判断

[CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)は取消要求後も正常・aborted・別errorがあり得ること、完了前のOVERLAPPED再利用禁止を説明する。[Canceling Pending I/O](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)のdriver依存も残る。sticky latchと公開時の採否は、このOS境界に対する本設計の判断である。

[GetOverlappedResult](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult)の未完と待機を分け、[OVERLAPPED](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped)に従いmanual-reset eventを専用に使う。[ConnectNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe)のERROR_PIPE_CONNECTEDはpendingとは別の接続成功として扱い、connectの転送bytes数は利用しない。[CreateNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea)の共有parameter説明をfixtureへ適用する（実装はW版）。

即時TRUEには公式資料内で記述差がある。現行[ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)はOVERLAPPED使用時にbytes出力をNULLとしGetOverlappedResultで取得するよう説明する。一方、[旧pipe例](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-overlapped-i-o)と[旧KB156932](https://learn.microsoft.com/en-us/previous-versions/troubleshoot/windows/win32/asynchronous-disk-io-synchronous)は即時完了の扱いが異なる。今回は現行APIに従いNULLで発行し、TRUEならeventを待たずGetOverlappedResult(FALSE)で量を取得する候補を、64bytes以下の事前送信pipeで独立に観測する。TRUEが実際に返ったか、量と内容が一致したかを記録し、未観測を成功にしない。違いをInternal/InternalHighの直接参照で回避せず、観測結果を全file systemへ一般化しない。

## 強制と結果

実行前に契約と順序を固定した。専用targetのDebug/tidy/ASan/UBSanで所有とメモリ境界、専用CTestで上記遷移とpipe、変更C++のformatとFileAPI conformanceで規約境界を確認する。必要な独立レビューで制御mutexと資源pinの範囲を確認する。結果は追記するまで未確認であり、製品向け強制をactiveにはしない。成功結果は関連入力不変ならcommit/push/文書統合のために再実行しない。waiverなし。

作業先は`D:/NeNeNib/worktrees/467-overlapped-cancel-contract`と`D:/NeNeNib/worktrees/467-overlapped-cancel-probe`、出力は`D:/NeNeNib/outputs/overlapped-cancel-contract-20261013`、恒久先は`D:/NeNeNib/evidence/overlapped-cancel-467`。実験branchとcommitを保存し、main文書統合と証拠収載後に取込・未保存物・稼働参照を監査して追加worktreeと不要出力を整理する。
