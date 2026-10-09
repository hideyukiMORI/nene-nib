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
