# ADR 0125 — 退避資源の寿命を製品portへ入れる前に小さく確かめる

- 状態: 受理（同期の小さい資源寿命を228契約で確認。製品APIの採用は未了）
- 日付: 2026-10-13
- Issue: #465
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-008 / ARC-010 / ARC-012 / CPP-003 / CPP-005 / CPP-006 / CPP-007 / CPP-008 / CPP-011 / CPP-012 / CPP-013 / CPP-016 / QLT-001 / QLT-002 / QLT-004 / QLT-008 / QLT-012 / QLT-013

## 文脈

#450の[所有設計](../design/2026-10-12-large-file-source-ownership.md)は私有退避ファイルを候補とした。#463/ADR0124で読込の全量成功を固定したが、未完成の退避の取消と、範囲readが残る間の解放は未確認。ここを一足飛びに製品のFilePortやTextBufferへ接続すると、未使用の公開APIと二つの本文経路を残す。

Win32Workerは固定1本・FIFOで、一つのjobが戻るまで次へ進まない。現行Folderの世代取消とworker破棄時のCancelSynchronousIoは、元本文の個別要求を中断し資源を回収する契約ではない。まず資源寿命を同期の小さい実験で切り出す。

## 決定

1. **実験コードを製品mainへ入れない。** `test/465-source-resource-probe` の隔離ブランチで、OS処理を `src/adapters/win32`、駆動を既存 `adapter_tests` の専用targetへ置く。固定toolchain・通常の警告/tidy/ASan/UBSanと規約を使う。mainへは本ADR・実結果・未確認事項を収載する（ADR0068の限定実験方式）。隠し設定、FilePort/EditorPortsの未使用API、新しいmoduleを作らない。
2. **対象は不透明bytesの資源であり、検証済み本文ではない。** BOM/NUL/不正UTF-8もそのままコピーする。文字コード判定、TextBuffer、索引、Vim、保存へ接続しない。#452の分割UTF-8の採用を先取りせず、別のvalidatorも作らない。将来、元本文へ採用するには既存validatorと索引を同じ取得元へ結び付けた証拠が必要になる。
3. **元ファイルと退避を一度だけ開く。** 元は `GENERIC_READ / FILE_SHARE_READ / OPEN_EXISTING`、退避は `GENERIC_READ | GENERIC_WRITE / 共有0 / CREATE_NEW / FILE_FLAG_DELETE_ON_CLOSE`、いずれも非継承。元のhandleをコピー完了まで保持し、退避は同じhandleで読み戻す。再open・複製・継承で寿命を増やさない。同名の既存退避があれば失敗し、そのファイルを削除しない。
4. **コピーは有界の同期stepにする。** 一回のstepが扱うbytesは最大1MiB、実験の元ファイル上限は4MiB、通常fixtureは64bytes以下。初めの宣言長まで取得し、読み込みの完了判定は `CompleteFileRead.hpp` の一つを使う。要求したwriteの完了長が一致しなければ失敗とし、部分writeの自動再試行をしない。源/退避の位置、宣言長、コピー済み長の所有者は生成taskだけである。
5. **公開前に封印し、公開後の書込口を持たない。** 宣言長全体のreadとwriteが成功し、取消されていないtaskだけが封印資源を引き渡せる。初期化の途中、失敗、取消からは封印値を作れない。封印はOS権限の剥奪ではなく、adapterの型と状態による書込経路の閉鎖である。外へhandle・退避path・可変bufferを貸さない。
6. **元本文IDはpathや数値の履歴位置で代用しない。** 試作では資源ごとに新しい不透明な不変tokenを割り当て、封印資源・受付済みread・完了値が同じtokenを持つ。古い完了値が生存する間に同じpathから別資源を作ってもtokenは異なる。token自身はOS資源を持たず、コピー/破棄にI/Oを隠さない。プロセスを跨ぐIDや製品の公開型を決めたものではない。
7. **範囲要求の受付で資源を保持する。** 範囲は `offset <= length` かつ `count <= length - offset` で検証し、1MiBを超える取得を拒否する。EOFの0長は有効。受け付けた要求はID・範囲・資源参照を所有し、一度だけ実行するか取り消す。probeでは単一スレッド上で順に実行し、同じhandleのfile pointerを並行更新しない。
8. **releaseは新規受付を閉じる。** release後の新規readを拒否し、所有者の資源参照を手放す。既に受け付けた要求は保持を続け、完了・取消・破棄で最後の参照を手放す。完了値はID・範囲・所有する不変bytesであり、資源そのものを保持しない。lvalueの読取viewだけを貸し、可変列やrvalueのviewを返さない。退避削除後も成功値のbytesは変わらない。
9. **今回の取消はI/O操作間の明示状態遷移だけ。** 生成中または引渡し前の取消は、新しいstepを始めず未公開資源を閉じる。受付済み範囲要求も実行前なら取り消して保持を解く。封印値を渡した後の生成taskの取消で、渡した資源を暗黙に撤回しない。進行中のOS呼出しを中断した、キュー公平性を満たした、100msで戻ったとは宣言しない。
10. **退避を永続回復データにしない。** この実験では、完了済みwriteの同一handleでの読戻しを確認し、封印のためのFlushは行わない。通常/失敗/取消時に最後のhandleを閉じ、DELETE_ON_CLOSEの削除を確認する。電源断・OSクラッシュ・媒体障害・CloseHandle失敗まで回収成功とは保証しない。製品の配置先、容量予約、残存回収は次のADRで定め、今回のfixture cleanupを製品の回収方針に流用しない。

## 所有と遷移

| 段階 | 所有するもの | 終了/取消/解放 |
| --- | --- | --- |
| 生成task | 元handle、未公開退避handle、宣言長、コピー位置、小buffer | 失敗/取消で全資源を閉じる。未完値を渡さない |
| 引渡し待ち | 完成した退避と新しいtoken | 引渡し前取消で破棄。引渡しは一回だけ |
| 封印資源 | token、長さ、退避の所有参照 | releaseで受付を閉じ、所有参照を解く |
| 受付済み範囲要求 | 同じtoken、範囲、退避の保持参照 | 完了/取消/破棄で保持を解く。release後も既存要求は実行可能 |
| 完了値 | token、範囲、防御コピーした不変bytes | 退避をpinしない。別資源のtokenと一致しない |

資源の参照管理とOS処理はadapter内だけで行う。共有するのは封印後のOS資源の寿命であり、applicationの可変本文を共有することではない。生成task・封印資源の所有者・範囲要求はmove-onlyとし、IDと完了値だけを不変の値としてコピーできる。move元は資源を持たない閉鎖状態になる。

| 操作対象と状態 | 操作の結果 |
| --- | --- |
| 生成taskが作成中 | `take`は`not_ready`で状態を維持。`cancel`で全資源を解放して取消状態へ |
| 生成taskが引渡し待ち | `step`はI/Oなしで完成を返す。`take`は一回だけ成功。`cancel`は未公開退避を破棄 |
| 生成taskが取消済み | `step`/`take`は`cancelled`、二重`cancel`は無作用 |
| 生成taskが失敗済み・引渡し済み・move元 | `step`/`take`は`unavailable`、`cancel`は無作用 |
| 封印所有者がrelease済み・move元 | 新規`request`は`unavailable`、`release`は無作用 |
| 範囲要求が未実行 | `run`は成功/失敗のどちらでも一回で消費し、保持を解く。`cancel`は保持を解いて取消状態へ |
| 範囲要求が取消済み | `run`は`cancelled`、二重`cancel`は無作用 |
| 範囲要求が実行済み・move元 | `run`は`consumed`、`cancel`は無作用 |

move代入で置き換えられた側が持っていた資源も解放する。公開済みの資源や受付済みの別要求を、元の生成taskや所有者への終了操作で取り消さない。

## 事前に固定する小さい検証

| 入力・故障 | 必要な観測 |
| --- | --- |
| 空、NUL/BOM/不正UTF-8を含むbytes、1/複数step | 宣言長と取得範囲が一致、内容を解釈せず保持、未完成時は引渡し不可 |
| 短いread→残り成功、初回/途中read失敗、0進捗 | 既存CompleteFileReadで完了または失敗。未完成資源/bytesを成功公開しない |
| 初回/途中write失敗、短いwrite | 未公開退避を閉じる。元ファイルを変更しない |
| 同名spoolが既存、上限超、作成失敗 | 他者のspoolを変更/削除しない。取得前に拒否できる上限を先に検査する |
| 生成途中/引渡し前にcancel | 以後のstepと引渡しを拒否し、未公開退避を削除 |
| 封印後に元pathを置換/削除し同pathを再度コピー | 旧資源のbytesが変わらず、新資源のtokenは旧完了と異なる |
| 境界外/過大range、EOFの0長 | 型付き拒否または正しい空結果。加算overflowを起こさない |
| range受付→release→range実行 | 新規受付拒否、既存要求の成功まで資源保持、完了後に退避削除、結果bytes保持 |
| range受付→release→要求cancel/破棄 | 最後の保持が解けて退避削除。取消値を成功値にしない |
| 範囲read失敗、二重実行、二重release | 部分値を返さず、保持漏れ/二重closeを起こさない |

外部ディスクを満杯にしたり、他プロセスを強制終了したりせず、故障は限定したI/O境界へ注入する。fixtureの固定パスはD:/NeNeNib配下の当該作業だけに置く。正常なcleanupと、想定外に残った資源の所在を記録する。

## OSの根拠と次段階の条件

共有条件、CREATE_NEW、非継承、DELETE_ON_CLOSEは[CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)に従う。共有0でも同じread/write handleで読み戻せるという使い方と、型による封印は本設計の判断である。プロセス終了時のhandle closeは[Terminating a Process](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process)が説明するが、電源断の残存なしへ一般化しない。

[File Caching](https://learn.microsoft.com/en-us/windows/win32/fileio/file-caching)と[FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)を踏まえ、同handleの通常readbackと媒体への耐久性を分ける。Flushをしない選択は一時資源の要件に基づき、成功readbackで耐久性を証明しない。[ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)の短い成功/EOFはADR0124の契約で扱う。短いwrite拒否は防御契約であり、通常diskで必ず起きる事象とは主張しない。

次の非同期設計では[CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)の取消要求と最終完了を分け、正常完了との競合も回収する。OVERLAPPED、buffer、handleは[GetOverlappedResult](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult)等で終端結果を得るまで保持する。取消jobを、完了待ちで止まっている同じFIFOの後ろへ積むだけでは中断できない。

製品採用前に、(1) 完了結果/使用中rangeを含むメモリ予算と受付上限、(2) ワーカーの有界な仕事と個別I/O取消/回収、(3) tempの配置・ディスク予算・残存回収、(4) 正典のUTF-8/索引との結合、(5) TextBufferと全callerの一括移行、(6) 保存版/undo分岐の同一性を定める。実験成功を2GiB対応やFR-013のメモリマップ方針変更へ読み替えない。

## 強制と結果

隔離した[試作commit 73b0b74](https://github.com/hideyukiMORI/nene-nib/commit/73b0b74f6069c247b61df52f886a66078a09d48a)で228契約が成功した。Debug/tidy/ASan/UBSan、変更C++のformat、conformanceも成功。独立レビューで終了状態を表へ固定し、move代入時に置換先が以前持っていた資源を解放する試験を補った。同期の小さい資源寿命は成立と判断し、製品portへ先に入れず次の取消/予算/本文取得設計の根拠にする。

上限超の元サイズは小ファイルへ宣言長を注入して確認した。1MiB超rangeの拒否試験は小fixtureの長さ境界にも当たるため、count上限だけの分岐を独立して実行した証拠ではない。全量コピー/範囲取得の成功と失敗、資源削除、元path変更後のbytes/ID、moveと二重操作の実結果・SHA・失敗ログは[品質記録](../quality/source-resource-2026-10-13.md)を参照する。実験コードはmainへ収載せず、製品に対する機械強制は追加しない。waiverなし。
