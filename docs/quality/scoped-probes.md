# 閉じた処理の前後比較（Issue #329・ADR 0082）

この道具は UI / GPU / 実ファイル I/O を含まない開発用の比較器。
正式な QLT-014 と基準値・許容は変えない。小さい差だけで候補を採用しない。

同じ #329 harness を比較する二つの commit に載せる。各作業木は変更を commit して clean にし、
`eng/toolchain.ps1` を読み込んで、Debug / Release の `nib_perf_probes` target だけを並列2で build する。
製品 Release 専用の `eng/build-release.ps1` ではこの開発 target を作らない。

```powershell
. ./eng/toolchain.ps1
cmake -S . -B build/probes-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/probes-release --target nib_perf_probes --parallel 2
```

exe が返す build 時の commit・compiler・configuration・tool-versions の SHA256 を照合するため、
ソースを変えたら clean commit の後に configure してから build する。
出力ファイルと同名の `.runs` ディレクトリは未使用のパスを指定する。以前の結果は上書きしない。

```powershell
python eng/compare-probes.py --before D:/NeNeNib/worktrees/before/build/probes-release/nib_perf_probes.exe --before-ref before-ref --before-commit <40桁SHA> --after D:/NeNeNib/worktrees/after/build/probes-release/nib_perf_probes.exe --after-ref after-ref --after-commit <40桁SHA> --workload display-line-long --iterations 20 --blocks 3 --timeout 60 --output D:/NeNeNib/outputs/comparison.json
```

場面は `controller-open-utf8-16mib` / `buffer-from-utf8-16mib` / `controller-insert-200` /
`display-line-long` / `controller-insert-200-after-delete-1mib` / `controller-insert-200-after-delete-16mib` /
`utf8-validate-ascii-16mib` / `utf8-validate-japanese-6mib`。
UTF-8検証は16800000 ASCII bytes・同数codepointsと、6219000日本語bytes・2079000codepointsの二本。
削除後の1MiBは1048572bytes、16MiBは16800000bytesなので、名称は近似で実bytesが正本。

各 process は計測前のwarmupを一回行い、その後の固定回数だけを測る。入力の実FNV-1a64を、Pythonが生成した同じ固定列のhashと照合し、その列のSHA256も記録する。
結果のchecksum、本文、行数・caretと削除履歴は計測外で照合する。controllerは既存Scripted portsを使い、毎回新しいEditingを作る。
ABBAを固定回数だけ実行し、全sample・対応するafter/before ratio・中央値・rangeを保存する。
外れ値は除かず、欠測・timeout・片側の失敗・不正marks・checksum不一致なら終了2で判定不能を記録する。
合格するまで繰り返したり、試行数を途中で変えたりしない。成功した値に性能の合格閾値を付けない。

道具の短い検証は `python -m unittest discover -s tests/conformance -p test_compare_probes.py`。
実際の自己比較は `display-line-long --iterations 1 --blocks 1` の4processだけで十分。
長い比較、実機の試用、正式な速さのゲートは親設計席で行う。

## 第2段階: 初期8本の固定比較後のVimコピー前計測（#323）

初期8本は定義・結果を保持する。ADR0082の第2段階として追加した4本だけを、同じharnessでbefore/candidateへ適用する。

| workload | 固定入力 | 区間内 |
| --- | --- | --- |
| controller-vim-insert-200-register-1mib | ASCII r 1048576bytes | VimKeyPress{x}を200回 |
| controller-vim-insert-200-register-16mib | ASCII r 16777216bytes | VimKeyPress{x}を200回 |
| controller-vim-record-insert-200 | ASCII x 200bytes | VimKeyPress{x}を200回 |
| controller-vim-record-insert-2000 | ASCII x 2000bytes | VimKeyPress{x}を2000回 |

登録は名前aの文字単位、録画はq a iを区間前に準備。区間後の保存・frame・Esc・貼付/undo・録画停止/再生で本文とcaretを確認する。準備と確認は時間に含めない。各processは従来通りwarmup1回を含み、iterationsで指定したsampleだけをmarksへ記録する。1sampleの短い動作確認は性能比較・正式速度ゲートを代替しない。

## 表示行一覧の固定比較（#367・ADR0100）

既存53条件を保持して `frame-rows-empty-64` / `frame-rows-short-64` /
`frame-rows-30-64` / `frame-rows-120-64` を末尾へ追加する。
入力・viewport・全行の独立期待値と採否条件は
[ADR0100](../adr/0100-visible-lines-reserve-only-the-existing-row-range.md)を正本とする。
各sampleは同じconst controllerのframeを64回生成し、その反復内で破棄する。
反復とlines.size/first_visible/total_lines/caret.line/caret.columnの和も区間内である。
準備・入力生成・固定literalとの全行照合は区間外で、前後双方を照合する。
結果checksumは空320、2行448、30行9792、120行15552。
display_lineやcapacityを期待値生成に使わない。

同一harnessをbeforeとafterへ使い、before製品sourceはmain decd4f1（#362受理済み、旧8252cb1と同じsrc）と一致させる。
対象Python検査は旧stage-six境界と新4入力/registry/不正名だけ。
Debugの新4条件各1iterationとRelease同一exeの各iterations1/blocks1は道具の動作確認である。
既存のwarmup/marks/metadata/欠測/timeout/許容は不変で、空inputBytes0も完全一致で照合する。
長いABBA・実機・正式速度・採否は親設計席が担当し、短い自己比較から利益を判断しない。


## 確定検索の不変共有の固定比較（#376・ADR0102）

旧57条件を保持し、SearchSnapshotWorkloadの8条件だけを同じ型付き表へ追加する。
入力/操作/oracle/3利益・5費用の固定条件は[ADR0102](../adr/0102-remembered-search-owns-one-immutable-parsed-value.md)を正本とする。
before07c346cはmain bfa1b32と同じ製品、afterc9d1e17と計測器/入力/区間は同一。
名前のframe-short/commit-short/typing-frame-short/retained-shortはa（1byte、FNV12638187200555641996）、
frame-long/repeat-long/retained-longはa×256（256bytes、FNV18242136092491110437）、unsearchedは空（FNV14695981039346656037）。
各long準備では正規SearchLineへの256byte入力の到達を区間外で検査する。
frame64は生成/破棄/field checksum、操作64/200はapplyが区間内、最後のdelivery破棄と前後の全文/行/map/span/current/caret/mode/message oracleは区間外。
新APIから期待値を生成しない。新enum case欠落の正規compiler拒否と旧57のsource不変を確認する。
Debug8条件各1iteration・Release同一exe各iterations1/blocks1は道具確認であり、時間値を利益に使わない。
20iterations/ABBA3/各120組は一度だけ取得し、全raw/失敗を保持、除外/補完/再取得しない。実施結果はgate-proofs 5-do。
