# OVERLAPPED readの取消と終端回収の限定実験 — #467 / ADR0126

## 対象と検証範囲

[ADR0126](../adr/0126-overlapped-cancellation-drains-before-release.md)に基づき、単一readの発行/取消/回収と資源の寿命を確かめる。実装は隔離branchに置き、mainへは設計と実結果だけを収載する。製品のworker/port/本文経路は変更しない。

| 確認 | この差分で壊れ得る境界 |
| --- | --- |
| 専用targetの固定Debug/tidy/ASan/UBSan build | 非移動storage、強/弱参照、結果bytesの所有、同じ状態機械の型とメモリ境界 |
| 決定的なscript契約 | 発行競合での取消取りこぼし、公開前後の採否、取消返値と実OS結果の混同、早過ぎる解放、二重操作 |
| 実pipeのpending→exact取消→終端回収 | 制御threadから既存worker上の未完readへ取消が届かず停止する、終端前に資源を失う |
| 実pipeの事前送信→read | 即時TRUEの取得量/内容を、現行ReadFileの指示に沿う候補で取得できない |
| 変更C++のformatとFileAPI conformance | OS/同期処理がadapter外へ漏れる、正典moduleと規約から外れる |
| 独立レビュー | control mutexとOS呼出しの範囲、実行/cancel中の保持、失敗cleanup、未観測の経路を受理する誤り |

script/nativeは同じ遷移を駆動する。新たなthread実装、時計、Sleep、競合の偶然性、成功までの再試行は使わない。専用CTestのTIMEOUTは停止検出のためであり、終了時間の受理値ではない。mainの文書差分には製品試験/CIを加えず、関連入力不変の成功結果を再利用する（QLT-001/012）。Waivers: none。

## 起点とレビュー

起点はmain `fba966b6ce36b4c4aa577f61348bbe32cfe215f3`。設計を先に`3bd0e1b`へcommitしてから、`test/467-overlapped-cancel-probe`を分岐した。ADRの独立レビューでは、発行中のsticky latch、公開の境界、取消呼出し中の局所pin、native失敗時の回収に阻害指摘はなかった。

既存Win32Workerは、次のjobが来るまで直前closureを保持する。実験ではjob開始時にcaptureを局所所有へmoveし、完了後もworker/control/結果を残したままstorageが解放されることを観測した。このために製品workerを変更していない。

作業先は`D:/NeNeNib/worktrees/467-overlapped-cancel-probe`、文書統合先は`D:/NeNeNib/worktrees/467-overlapped-cancel-contract`。原コマンド/ログは`D:/NeNeNib/outputs/overlapped-cancel-contract-20261013/probe`、恒久先は`D:/NeNeNib/evidence/overlapped-cancel-467`。整理後は恒久先を参照する。

## 実結果

試作は[commit 277e4d543ef731deb98fdda7ae9adc07183d1701](https://github.com/hideyukiMORI/nene-nib/commit/277e4d543ef731deb98fdda7ae9adc07183d1701)へ保存した。検証時の30ファイルのSHA-256を`outputs/probe/result.json`へ記録し、commit直前に全件一致を確認した。EXE SHA-256は`4972f1397be7fd8401997546c894fc00fe1d5573554431c18fee5981ae503d0b`。以下のログは恒久先の`outputs/probe/`を基準とする。

| コマンド・条件 | 結果と原ログ |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S <probe> -B <probe>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | 固定clang-cl 19.1.5、FileAPI query作成後にconfigure成功。`build-probe.ps1`、`configure.log` |
| `cmake --build build --target nib_overlapped_cancel_probe --parallel 2` | 初回と増分の試験側unchecked optionalを局所値で修正。最後の増分2 stepsで成功。`build.log`、`build-incremental.log`、`build-retry2.log`。flags/規約の変更なし |
| `ctest --test-dir build -R '^nib_overlapped_cancel$' --timeout 60 --output-on-failure` | 1/1成功。`ctest-run1.log` |
| `ctest --test-dir build -R '^nib_overlapped_cancel$' -V --timeout 60` | **138 checks / 0 failures**。`ctest-verbose.log`。成功後にログ取得だけを目的として再実行したため、QLT-001/012の再利用方針から外れた。両ログを保持し、この追加実行を必要な検証と正当化しない |
| 変更C++の`clang-format --dry-run --Werror` | 成功。対象は上記resultのCMakeを除く29ファイル、コマンドはbuild script |
| `python eng/conformance.py --build-dir build` | **0 violations**。`conformance.log` |
| `git diff --check` / `git diff --cached --check`、差分と独立レビュー | 空白成功。未完errorのfail-fast、コピーしたticketのcollect拒否、要求ID非再使用、native異常経路の回収を補完し、再レビューの未解決指摘なし |

実pipeの未完readはReadFileが997（ERROR_IO_PENDING）、exact CancelIoExが1回、終端は995（ERROR_OPERATION_ABORTED）。別の事前送信pipeではReadFileがTRUE、非待機queryが1回、3bytesと内容の一致を観測した。worker stopも同じ取消口で終端まで回収できた。scriptでは、issuing中not_found後の再要求、取消API失敗後のdrain、正常完了と取消採否の区別、公開前後の取消、取消APIの局所pinが戻るまでの保持、idle worker中の解放を確認した。

### ASan診断の留保 — #469

verboseログに`interception_win: unhandled instruction`が1件ある。EXE/PDBの静的照合で8-byte列はEXE中1箇所、RVA 0x1080d4、UCRT `frexp+4`と一致し、推定ロード基点も64KiB境界に整合する。[LLVM 19.1.5の実装](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.5/compiler-rt/lib/interception/interception_win.cpp)では命令長を解釈できないとこの非致命的診断を出す。`frexp`のinterception設定失敗との関連が強いが、ログに実ロード基点/呼出しstack/hookの最終結果がなく、ASanの全検査範囲は未確認である。

`asan-interception-static-investigation.md`、`asan-static-byte-search.log`、`asan-static-symbols.log`に照合方法と限界を保存した。静的調査でprobeを再実行していない。他のsanitizer runtime errorは印字されていないが、**無警告・ASan全面成立とはしない**。変更前にも存在した証拠はなく、無関係な既存不具合と断定せず[Issue #469](https://github.com/hideyukiMORI/nene-nib/issues/469)へ切り分けを残した。小さい機能/寿命の観測と、この検査範囲の留保を分けて受理する。

## 結果を広げない範囲

scriptで決定した競合と、local named pipeの一つのOS境界だけを対象とする。即時TRUEの公式資料差はADR0126に残し、実観測しても全file systemやdriverへ一般化しない。

キューでまだ始まっていないjobのshutdown通知、worker公平性、受付数/完了bytesのメモリ予算、製品temp/ディスク予算/残存回収、disk/任意driverの取消支持、ReadFile自体のblock、100ms、2GiB、性能、GUI、UTF-8/索引との結合、全TextBuffer caller移行、保存版/undo分岐はこの結果に含まれない。64MiB上限、同期の製品I/O、保存schema、公開beta.3は変えない。

試作ソース/CMakeと実行物は実験branch/commitと恒久証拠に保存し、mainへ取り込まない。公開/統合/整理の実状態は恒久先`closeout.json`、収載対応は`manifest.json`へ記録する。製品向け機械強制は追加しない。
