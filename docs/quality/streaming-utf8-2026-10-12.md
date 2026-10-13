# 分割UTF-8検証の準備記録

2026-10-12 / Issue #452 / [ADR 0122](../adr/0122-utf8-validation-retains-only-the-unfinished-sequence.md)。

## 2026-10-12の準備時点

**試験はhide指示で保留。実装と契約試験コードの準備段階で、技術受理していない。** Debug/Release build、clang-tidy、ASan/UBSan、単体試験、symbols、conformance、実機性能、CIは未実行。成功件数や性能値はまだない。draftのままmainへ統合しない。

## 変更と確認対象

- `Utf8Validation.hpp`に不変の進捗を追加。`empty` / `with_bytes` / `finish`で断片とEOFを区別し、最大3 bytesの未完列だけを所有する。
- `Utf8.cpp`の既存走査を分割に対応させ、一括`validate_utf8`も同じ走査へ接続。文字集合の表・復号を二重化せず、従来のASCII連続走査を保つ。累積byte数は加算前にoverflowを検査する。
- `Utf8ValidationTests.cpp`に既知の文字列の全2/3分割、拒否、EOF、借用元の変更、進捗の分岐、未完列の後の別文字を追加。`Scopes.hpp` / `CoreTests.cpp` / `CMakeLists.txt`から既存`--utf8`と既定実行へ接続する。
- ADR索引とcurrentへ試験保留中であることを記録。FilePort、DetectedTextの所有契約、保存schema、64MiB上限は変更しない。

## 再開後に必要な最小の確認

| 対象 | 理由・予定する確認 |
| --- | --- |
| 変更target | 新しい型/署名と実装をcompileし、既存のDebug/tidy/ASan/UBSan条件で拒否がないか確認する |
| `build/nib_tests.exe --utf8` | 新しい分割契約と、一括検証・表示文字列・UTF-16という直接の呼出し境界 |
| `build/nib_tests.exe --file-text` | BOM/文字コードの判定、所有本文、controllerの読込を通じた不正入力の拒否 |
| `eng/symbols.py` / `eng/conformance.py` | coreへOS/CPU/並行性の依存を持ち込まず、型・可視性・正典のtargetへの接続を保つ |
| 直接のUTF-8/読込の性能 | 一括の走査も変わるため必要。既存のprobe/ベンチと固定した比較条件で実施し、#446とは別の対象版として記録する |

試験の保留が解除されてから対象版・入力・回数と実機の時間帯を固定する。無関係なVim全件や同じ検証の工程ごとの繰り返しは行わない。旧validatorの成功結果を変更版の成功として再利用しない。

## 実行した差分確認

`eng/toolchain.ps1`で固定したclang-format 19.1.5を使い、変更C++6ファイル（Utf8.cpp/.hpp、Utf8Validation.hpp、Utf8ValidationTests.cpp、CoreTests.cpp、Scopes.hpp）を整形した。`clang-format --dry-run --Werror <同6ファイル>`と`git diff --check`は成功。これは構文・型・静的解析・動作の成功を意味しない。コミット時の空白確認も同じ差分を対象に行う。

## 自己レビューと限界

ARC-001・CPP-014に従って、UTF-8の意味を既存`Utf8.cpp`へ集約する。ARC-003/007・CPP-013/018の区画は変えない。ARC-005/008・CPP-003/007では進捗の不変性と未完列の所有だけを保証対象にし、本文を所有しない進捗を検証済み本文の代用にしない。ARC-010・CPP-004では未完・拒否・長すぎる入力を結果と明示的なEOFで扱う。

保持するcode point数は受理したbyte数以下。`incoming > max - received`のguardで加算overflowを防ぐ設計だが、`size_t`上限まで積む実行確認はしていない。試験専用の内部状態setterは追加しない。性能影響、コンパイルと静的解析の成立、2GiBでの成立はいずれも未確認。

適用規則はADRに列挙したFR-008/013、ARC/CPPとQLT-001/008/012/013/014。Waivers: none。

## 保存先

作業木は`D:/NeNeNib/worktrees/452-streaming-utf8`、準備出力は`D:/NeNeNib/outputs/streaming-utf8-20261012`。未検証・未統合のため保持し、受理後に恒久収載と安全確認を行って整理する。mainと公開版は変更しない。

## 2026-10-13再開時の条件

00:50 JST、hideが他の作業を減らして試験を再開。#446の受理済みmain `e5f8e709eef61d88ebcdbd943d93e837ad0d348f`を取り込んでからDebugの対象targetをbuildする。改行索引は前後双方に含め、UTF-8変更の比較へ混ぜない。出力は`D:/NeNeNib/outputs/streaming-utf8-resume-20261013/`、恒久先は`D:/NeNeNib/evidence/streaming-utf8-452/`。

初回buildは`expected<optional<size_t>>`からの同じoptionalの確認と取得をclang-tidyが対応付けられず、`bugprone-unchecked-optional-access`を2箇所で検出した。optionalを一度だけ局所const値へ取り出し、その値を確認してから読む形へ修正する。診断の抑制や設定変更はしない。初回ログを`build.log`、修正後を`build-optional-fixed.log`へ分ける。

### 性能比較の事前計画

- 変更前は上記main e5f8e70、変更後は対象契約が成功したclean commitで固定し、実行前に`perf-plan.json`へfull SHAと両exe SHA-256を記録する。両版のharness/入力/flagsを揃える。
- `nib_perf_probes`は固定toolchainと通常CMake Release、target指定/並列2（ADR0082）。`eng/compare-probes.py`の`utf8-validate-ascii-16mib`、`utf8-validate-japanese-6mib`をこの順で各1回。既定のABBA×3 blocks、各processに区間外warmup1と20 samples、timeout60秒。全240値/各版120値とchecksum/入力hashを保存し、外れ値を除かない。独自の合否閾値を作らず、悪化が見えた場合は原因を調べ、同じ候補を合格まで測り直さない。
- 製品は`eng/build-release.ps1 -Ref <変更後commit>`で作り、`eng/measure-speed.py --check --bench open-large-file-16mib --repetitions 5 --executable <exe>`を新規専用profileで1回。既存QLT-014の基準/閾値を維持する。#446のopen成功値は旧validatorなので変更後の合格には代用しない。
- buildと測定を重ねない。ホストの独占利用は保証せず、他作業を減らしたという条件と限界を残す。今回は一括走査と直接の読込への影響を確認する。分割FilePortは未実装であり、2GiBの完成読込や非同期応答性を測ったとは扱わない。

### 対象契約・静的検査

| コマンド | 結果 | 直接確認した退行 |
|---|---|---|
| `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` / `cmake --build build --target nib_tests NeNeNib --parallel 4` | optional局所値の修正後に成功。clang-cl19.1.5/clang-tidy/ASan/UBSan | 変更型と実装、既存利用targetとの結線と安全性 |
| `build/nib_tests.exe --utf8` | 13,921 checks成功 | 全2/3分割と1-byte断片、拒否/EOF、不変性/所有、ASCII境界、一括の直接caller |
| `build/nib_tests.exe --file-text` | 307 checks成功 | BOM/文字コード判定、所有本文、読込の直接caller |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries/0違反 | 外部OS/CPU/並行性の混入 |
| `python eng/conformance.py --build-dir build` | 0違反 | CMake File APIで実依存とソース所有、新しい型/文書の規約 |
| `clang-format --dry-run --Werror src/core/Utf8.cpp` | 成功 | 修正した本体の整形。未変更の他5ファイルは準備時の結果を再利用 |

合計14,228 checks。最初の2scope実行は不要な`ASAN_OPTIONS=detect_leaks=0`をプロセスへ渡してしまったため採用せず、overrideを外した通常のsanitizer環境で両scopeを一度確認し直した。初回ログも保持し、採用記録は`utf8-default.log` / `file-text-default.log` / `contracts-default-result.json`。以後は関連実装・入力/環境が不変なら再実行しない。全件回帰・coverage・Vim oracleは選択していない。

### 初回の局所性能比較と修正方針

変更前e5f8e70、変更後abb38e0の通常Releaseで、事前固定した2入力をABBA×3/各20標本で測定。両入力とも全240標本を観測し、入力/出力checksum/compiler/toolchainが一致した。

| 入力 | before中央値（µs） | after中央値（µs） | 対応after/before比の中央値 |
|---|---:|---:|---:|
| ASCII 16,800,000 bytes | 1095.0 | 1109.0 | 1.015476 |
| 日本語 6,219,000 bytes | 5552.0 | 7038.5 | 1.268325 |

日本語の全6 processで変更後の中央値が増加しているため、この候補は受理しない。正式QLT-014とは別の局所比較だが、約27%の増加を無視して統合しない。`ascii.json` / `japanese.json`と各`.runs`、両exeは保持する。未実行のGUI openは修正後へ移し、この候補を同じ条件で測り直さない。

Release objectの`llvm-objdump --disassemble --demangle --no-show-raw-insn`を比較すると、旧版は2/3/4-byte列の継続byte検査を展開しているが、候補は`min(length, remaining)`で上限が変わるループを毎文字回している。これは原因候補であり、この観測だけで27%の全量を説明したとは扱わない。末尾が未完かを先に分け、継続byte検査は同じprivate helperで共有して、完成した符号列の固定上限を保つ方針で修正する。入力の長さ判定・拒否・復号・未完列の意味は維持し、第二のvalidatorを作らない。変更後は両契約scopeと変更objectの境界検査を確認し、同じbefore/同じ回数の新しい比較を一度行う。

27cc383では14,228 checksとsymbols/conformanceが再び成功。しかし固定比較のASCIIは1112.5→1225.0 µs/対応比1.102459、日本語は5485.0→10541.5 µs/1.923092となった。`scan`が展開されず文字ごとの関数呼出しになっていることを生成コードでも確認したため、この案も不採用。`*-tail.json`と実行ファイルを保持する。

次は同じ`scan`の長さ1〜4のループ内で、各byteを読む直前に残長を確認する。未完と完成で呼び口を分けず、追加したhelperは撤去する。途中の不正byteの即時拒否とEOFの意味を保ちながら、可変上限のループと追加の呼出しを避ける。コンパイラ専用属性やflags、基準値には頼らず、対象契約と同じ固定比較で確認する。

### 2026-10-13の終了点

01:36 JST、hideの「区切りのいいところで日報と引き継ぎ書を書いて今日は終わり」に従い、上記3案目の短い対象検証で終了する。追加のRelease build・性能測定・GUI open・Ready/mergeは行わない。

3案目は`cmake --build build --target nib_tests NeNeNib --parallel 4`が4 stepsで成功。通常のDebug/tidy/ASan/UBSan条件で`--utf8`13,921、`--file-text`307の計14,228 checksが成功し、`symbols.py --build-dir build --require core application`は2 libraries/0違反、`conformance.py --build-dir build`は0違反。`clang-format --dry-run --Werror src/core/Utf8.cpp`も成功。ログは`build-byte-guard.log`、`utf8-byte-guard.log`、`file-text-byte-guard.log`、`symbols-byte-guard.log`、`conformance-byte-guard.log`、`contracts-byte-guard-result.json`。ソース変更に対する確認であり、文書追記・pushで繰り返さない。

**現在の機能契約は成功したが、3案目の性能は未測定で、技術受理していない。** 初回・2案目の性能悪化は未解消として扱う。GUI openも未実行。次回は新しいclean commitのprobeを通常Releaseで作り、同じbefore e5f8e70、同じ2入力/ABBA×3/20標本を事前固定して比較する。良好な結果を得るための同一候補の再試行はしない。

`build/release-abb38e0`は2案目のprobe用へ再configure済み。中の`nib_perf_probes.exe`は27cc383（SHA-256 `846537a479f35c530d6910342b4187d4131c04029689a2a813eb6cc868f08e93`）、`NeNeNib.exe`は初回abb38e0（`32c0bfaa261bff4bdab814ff553f61327e7cfc9466ad0442994f4f5bfc47566a`）であり、3案目の製品として起動しない。初回exe/configは出力の`initial-abb38e0/`へ分離保存済み。終了時のログ・全測定値・各exe・版対応は恒久先の`checkpoint-20261013/`へ収載する。

未受理のため、作業木`452-streaming-utf8`、比較元`452-utf8-before-e5f8e70`と準備/再開出力は保持する。次回はhideの再開指示と実機測定時間帯を確認する。日付・担当変更だけでは続きを起動しない。保存schemaと公開beta.3は不変。Waivers: none。

### 2026-10-13夜の再開 — Release probeと拒否時点の契約

clean版`6c2c13f8b4d721d2f56a4fb3f1fdb08752b05c87`のまま固定`eng/toolchain.ps1`を通し、通常CMake Releaseの専用`build/release-6c2c13f-evening`を新規configure。`cmake --build build/release-6c2c13f-evening --target nib_perf_probes --parallel 2`は135 stepsで成功。製品Releaseとは呼ばず、製品exeのbuild/起動はしていない。before e5f8e70のprobe SHA-256は`80411092cd16f21ec44b0f0699e97e6ae4f07b5495e34e9a1f7e67e244d893b9`、afterは`fc758760b92d6b50c00850fa249174424aff3eb3e1d411f19b108c2186f9a29e`。

出力`D:/NeNeNib/outputs/streaming-utf8-evening-20261013/perf-plan-byte-guard.json`へ両commit/exe/flags/tool/input hashとASCII→日本語、ABBA×3/20 samples/warmup1/timeout60を実測前に固定した。両版harnessとtool hash、既存2入力、Utf8/ProbeMain/ProbeWorkloadの正規化compile flagsが一致。最新`checkpoint-20261013/checkpoint.json`のsourceHashes全456件も実ファイルと一致し、初回の`verified-sources.json`は最新契約の根拠に使わない。生成コードではscanの展開を確認したが、性能成功とは扱わない。

独立レビューでは実装欠陥を指摘せず、`validated_parts`だけでは拒否時点を区別できない契約の不足を確認。`tests/unit/Utf8ValidationTests.cpp`だけにE0 80、ED A0、F4 90 80の途中追加成功、EOF拒否、最後の80追加時の`invalid_utf8`と失敗後の旧pending/base不変を18 checks追加した。固定clang-formatの整形/検査、既存Debugの`cmake --build build --target nib_tests --parallel 2`（変更objectとlinkの2 steps）、通常ASan/UBSan条件の`build/nib_tests.exe --utf8`は13,939 checksで成功。`git diff --check`も成功。ASAN_OPTIONS等overrideなし。

製品source・関連依存・harness・入力は変更せず、追加テストと2文書だけの変更。既存`--file-text`307 checks、symbols/conformanceの成功を再利用し、他scopeや製品を再実行しない。固定probeも6c2c13f版として保持し、テストのcommitに誤って付け替えず再configureしない。原ログ、固定plan、スクリプト、生成コード、両probe/configとcommit/source対応の恒久先は`D:/NeNeNib/evidence/streaming-utf8-452/checkpoint-evening-20261013/`。**性能比較・GUI openは測定時間帯の回答待ちで未実行、PR453はdraft・技術受理未了。** 初回/2案目の失敗値を残し、未完了worktree/outputsは削除しない。Waivers: none。
