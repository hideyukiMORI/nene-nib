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

## 検索preview位置の固定比較（#378・ADR0103）

旧65条件を保ち、PreviewCaretWorkloadの7条件を同じtyped表へ追加する。
preview-caret-long-30-64 / long-120-64 / short-30-64 / confirmed-30-64 / unsearched-30-64 / absent-30-64 / disabled-30-64（各後半名にもpreview-caret-接頭辞）。
入力/独立oracle/2利益・5費用は[ADR0103](../adr/0103-preview-offset-is-resolved-once-per-frame.md)を正本とする。
本文の先頭は日×1024+z+Tab+🖋+U+0001、後続119行z x、CRLF/末尾改行なし。shortだけ日×1。
長3674bytes/FNV3943064766360993406、短605bytes/FNV5659432768607348982。
const frame64回の生成/破棄/field checksumを区間内、全文/表示map/span/current/caret/入力行/設定/履歴のliteral期待値照合を前後区間外に置く。checksum30行9792/120行15552。
before28bc1ffはmain dff0d5cと同製品。after13655cbと同じharness/入力/区間/flagsを使う。新7Debug/変更前Release各1iterationは道具の動作確認で時間値を利益にしない。
固定20iterations/ABBA3/各120組を一度取得、全raw/失敗を保持。旧65/selector/template/入力の不変証拠と新enum欠落のcompiler拒否を保存。結果はgate-proofs 5-dp。


## 文書表示値の所有移管の固定比較（#380・ADR0104）

旧72条件を保持してFrameDocumentWorkloadの4条件を同じtyped表へ追加する。
frame-document-short-saved-256 / long-saved-256 / long-failed-256 / untitled-256（各名にframe-document-接頭辞）。
入力metadataは固定path31/241/241/0bytes、本文・encoding・保存/失敗値・独立期待値は[ADR0104](../adr/0104-temporary-document-view-is-moved-into-the-frame.md)を正本とする。
const frame256回の生成/破棄とdocument/status/tabs/linesの長さchecksumを区間内、準備・前後の全文/表示map/document全欄/status/tabs/delivery保有値照合を区間外に置く。
checksumは14592/82176/83200/3328。新4Debug/変更前Release各1iterationは道具確認のみ。
固定20iterations/ABBA3/120組、2利益/2費用条件で一度取得し、全raw/失敗を保持する。旧72の入力/区間/selector不変と新enum欠落の実compiler拒否を確認する。

before b6de932 / after b82b0edは同じharness/入力/区間/flagsを使用。pathのFNV1a64はshort3165114504819470625、long/failed3259355916674798828、untitled14695981039346656037。
各120組の2利益/2費用が成立。最初の環境読取だけはUTF-8/cp932で失敗し、probe試料0/原本を保全。出力先だけを変えて-X utf8で未取得系列を一度取得した。結果と全失敗はgate-proofs 5-dq、局所値を正式GUI速度へ代用しない。
