# 私有退避の封印・範囲取得・解放の限定実験 — #465 / ADR0125

## 対象と選択した検証

[ADR0125](../adr/0125-source-resource-lifetime-is-probed-before-product-ports.md)に従い、mainへ未使用の製品portを追加する前に、隔離ブランチでOS資源の寿命を確かめる。元本文としてのUTF-8受理、UI、非同期、速度、2GiB対応はこの結果に含めない。

| 確認 | この実験で確認する失敗 |
| --- | --- |
| 専用targetの固定Debug/tidy/ASan/UBSan build | move-only所有、故障注入、span/stringの寿命が型・既存規約・メモリ境界に反する |
| 同期資源の小さい契約 | 未完成bytesの公開、元pathとの取り違え、受付済みreadの早過ぎる解放、read/write失敗時の資源残存、二重終了 |
| 変更C++のformatとconformance | OS依存がadapter外へ漏れる、型・module境界・禁止記法が正典から外れる |
| コードと独立レビュー | sealed後の書込経路、可変bytesの別名、完了値の不要なOS資源保持、未試験の状態遷移 |

readの全量判定は既存`CompleteFileRead.hpp`を使う。実験専用targetを通常のmoduleとflagsへ接続し、規約の除外・閾値変更・抑制は追加しない。mainへ統合する差分は本ADR・実験記録・進捗文書だけなので、製品試験/CIを加えない。規則の詳細はADR0125、検証範囲と再利用はQLT-001/012。Waivers: none。

## 実行記録

起点はmain `957d74b15cd781bee8e238de0164f43cb704f647`。ADRを先にcommit `022f45f7ec70efdbce6dc575f1ed5510971a606a`へ固定してから、`test/465-source-resource-probe`を分岐した。終了状態の操作表は実行前のレビューで具体化した。

実装場所は`D:/NeNeNib/worktrees/465-source-resource-probe`、文書統合場所は`D:/NeNeNib/worktrees/465-source-resource-contract`。原ログとコマンドは`D:/NeNeNib/outputs/source-resource-contract-20261013/probe`、恒久先は`D:/NeNeNib/evidence/source-resource-465`。整理後は恒久先を参照する。

試作は[commit 73b0b74f6069c247b61df52f886a66078a09d48a](https://github.com/hideyukiMORI/nene-nib/commit/73b0b74f6069c247b61df52f886a66078a09d48a)へ保存した。検証時の未commit差分18ファイルは`probe/result.json`のSHAで特定し、commit直前にも全て一致を確認した。exe SHA-256は`bd51bb889ec5f49c7e7e8a59a37c740fd46d9104011f8f8e5af8534be9d6646c`。

| コマンド・条件 | 結果・原ログ |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S <probe tree> -B <probe tree>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | 固定clang-cl 19.1.5で成功。事前にFileAPI queryを作成。`probe/build-probe.ps1`、`probe/configure.log` |
| `cmake --build <probe tree>/build --target nib_source_resource_probe --parallel 2` | 初回は試験の認知的複雑さ12 > 10と入れ子4 > 3で失敗。補助処理を抽出した増分2 stepsで成功。`probe/build.log`、`probe/build-contracts-fixed.ps1`、`probe/build-contracts-fixed.log` |
| `./nib_source_resource_probe.exe`（cwd=`build`） | **228 checks、0 failures**。初回実行の一回だけ。Debug ASan/UBSan、ASAN_OPTIONS/UBSAN_OPTIONSとも未設定。`probe/probe.log`、`probe/result.json` |
| `clang-format --dry-run --Werror` + 新規C++16ファイル | 成功。対象列と終了値は`probe/result.json`、コマンドはbuild script。無出力なのでformatの架空ログを作らない |
| `python eng/conformance.py --build-dir build` | **0 violations**。`probe/conformance.log` |
| `git diff --check`、stage後の`git diff --cached --check`、差分と独立レビュー | 空白成功。実装の意味欠陥なし。終了遷移とmove代入の試験不足を補完し、追加差分の再レビューで未解決指摘なし |

独立レビューでは、最初のADRに残っていた二重操作/move元の結果を表へ固定し、move代入で置換される側の資源解放試験を追加した。rootの確認で、既存spool衝突も存在だけでなく非空の元内容が変わらないことまで確認した。生成途中/引渡し前の取消、範囲read失敗/取消/破棄、元path差し替え、旧結果生存中の新tokenも実行した。

最後にinput・2個のspool・空fixtureフォルダを試験自身が削除し、`build/source-resource-probe`の不在を確認した。初回のbuild失敗も恒久保存する。実験branchと文書branchの保存先・統合・整理の実結果は恒久先`closeout.json`、収載対応は`manifest.json`が示す。

## 結果の意味と未確認事項

これは上限4MiB・step/range上限1MiBの同期資源実験であり、通常fixtureは64bytes以下。宣言長上限の拒否は小さい実ファイルとサイズ注入で確認し、大容量データを生成しない。

`count > 1MiB`の拒否ケースはfixtureの長さ境界も超えるため、count上限の分岐だけを独立に実行した証拠ではない。4MiBのコピー成功や負荷下の寿命も未確認。同期の小さい資源境界が成立したことを、これらの受理へ広げない。

製品の64MiB上限・同期I/O・保存schema・公開beta.3は変えない。試作コードとCMake差分は実験branch/commitと恒久証拠に保存するだけで、mainへ取り込まない。成立した契約は後続ADRの根拠にし、製品向け機械強制をactiveと記載しない。

実行中OS I/Oの取消、worker公平性、要求数と完了bytesの予算、製品temp配置/容量予約/残存回収、CloseHandle失敗、電源断、あらゆるfile systemのsnapshot、UTF-8/索引の証拠、全TextBuffer caller移行、保存版/undo分岐、100ms/2GiB/性能は未確認。関連入力が不変の成功結果は、commit・push・文書追記・統合のために再実行しない。
