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

### 2026-10-14 — 3案目の局所比較と4案目の一回走査

hideが実機は空いていると伝えたため、保持済み3案目6c2c13fのprobeを固定計画どおりASCII→日本語、各1回のABBA×3/20 samples/warmup1/timeout60で比較した。beforeはe5f8e70。元planは変更せず、SHA-256 `c86f491d5ac003204662f8244cc9295c3c960847fc8d65bd05c6eae271488f78`を実行記録へ参照し、出力先と日付だけを更新した。

| 入力 | before中央値（µs） | after中央値（µs） | 対応after/before比の中央値 |
|---|---:|---:|---:|
| ASCII 16,800,000 bytes | 1130.0 | 1248.0 | 1.105407 |
| 日本語 6,219,000 bytes | 5572.0 | 5774.0 | 1.033577 |

両条件exit 0、全240標本/各版120標本を保存し、両commit/exe SHA・input bytes/FNV/SHA・出力checksum・dirty 0・Release・compiler19.1.5/toolchainが全trialで一致した。全6 pairの対応比中央値は双方とも1を超え、process中央値ではASCII5/6、日本語6/6が増えた。前2案より日本語の増加は縮小したが、退行が解消したとは判断しない。正式QLT-014の合否ではなく局所比較であり、外れ値を除かず、同じ候補を測り直していない。

実機はi9-10850K/20 logical processors、Windows 11 Pro 10.0.26300、HP推奨電源設定。測定直前のCPU負荷3標本は0/4/0%、build/testプロセスは不在。ほかのアプリを閉じず、専有環境とは主張しない。準備ラッパーはJSONを既定cp932で読んで失敗したが、probe起動前だった。UTF-8を明示して準備を修正し、その失敗も保存した。原ログ・全値・process/pair集計・plan・probe/configは`D:/NeNeNib/evidence/streaming-utf8-452/checkpoint-20261014/`へ54ファイルをSHA照合して収載。以前の失敗値とcheckpointも残す。

続くhideの「対応を進めて」を受け、4案目は`src/core/Utf8.cpp`で継続byteの検査と復号を一つのループへまとめる。従来の`scan`は継続byteを検査した後、`decode`で同じbyteを再読込し長さ分岐を繰り返していた。小さいprivate `decoded_continuation`を`scan`と既存`decode`で共用し、境界確認→継続byte判定→値への追加を行う。scalar範囲は符号列が完成した後だけ判定し、E0 80/ED A0/F4 90 80の途中成功・EOF拒否・完成時拒否を維持する。新しいvalidator、専用inline属性、flags、閾値は追加しない。

保持済み生成コードではbeforeのASCII内側ループと3案目はレジスタ割当以外の命令列が同じで、wordごとのstate spillは無い。入力ごとに一度だけのwrapper/state copyで約118µs増の全量を説明できない。ASCIIの原因は未確定で、この4案目もASCII改善を保証しない。ASCII走査、pending、overflow、公開API、保存schema、64MiB上限は変更しない。

現在main60c9a8dを通常mergeした。内容の衝突は無かったが、既定Merge件名がGIT-003で拒否されたため、同じmergeを規約件名でab013b7として完了し初回拒否を記録した。上流#463のCompleteFileRead.hpp/Win32FileAdapter.cpp/FilePort.hpp/NibAdapterTests.cppはmainと一致し、139契約を再利用する。新しいUtf8.cppを通る`--utf8`/`--file-text`は今回確認する。出力先は`D:/NeNeNib/outputs/streaming-utf8-fix-20261014/`。4案目の対象検証とRelease probeの準備を進める段階であり、性能・GUI open・Ready/main統合は未了。Waivers: none。

独立SOLの読み取りレビューはmain60c9a8dとのPR全体差分と4案目を確認し、対応が必要な指摘なし。レビューしたUtf8.cppのSHA-256は`e856410fcfca0468def0c68a8c4b7c0195242944b053f6f91dc3b87a728b5222`で、実ファイルとも一致した。最大3 bytesのpending、余分に写した次文字の再走査、完成後だけのscalar拒否、bytes_seen/countのoverflow不変条件、正典経路と既存契約の十分性を確認した。レビュー成功は実行契約や性能受理を代用しない。本件は分割所有の土台となるAPI/機能のリファクタリングであり、改善率を目的に候補を測り直すものではない。

4案目の対象検証は固定clang-cl19.1.5/Debug/tidy/ASan/UBSan、通常のsanitizer環境で次のとおり成功した。既存テストは分割・拒否時点・一括/表示/UTF-16・読込境界を覆うため変更していない。

| コマンド | 実結果・確認した退行 |
|---|---|
| `cmake --build build --target nib_tests NeNeNib --parallel 2` | 71 steps成功。変更したcoreと直接callerをcompile/link。上流FilePort.hppの更新に伴う依存再compileを含み、他scopeの実行ではない |
| `build/nib_tests.exe --utf8` | 13,939 checks成功。全分割、未完/拒否/EOF、所有/不変性、完成後scalar拒否、ASCII、共有復号と表示/UTF-16 |
| `build/nib_tests.exe --file-text` | 307 checks成功。BOM/文字コード判定、所有本文と読込の直接caller |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries / 0違反。中核の外部依存を確認 |
| `python eng/conformance.py --build-dir build` | 0違反。正典区画・宣言・実グラフを確認 |
| 固定`clang-format --dry-run --Werror src/core/Utf8.cpp` / `git diff --check` | 成功。変更した本体の整形と差分の空白 |

計14,246 checks。ログと実行script、検証したUtf8.cpp SHAは上記出力の`debug-build.log` / `utf8.log` / `file-text.log` / `symbols.log` / `conformance.log` / `contracts-result.json`。#463の139契約は入力不変を照合して再利用し、無関係なscope・全件回帰・Vim oracle・coverageは実行していない。実装・テスト・関連依存/必要な環境が変わらなければ、文書・commit・push・レビュー・mergeだけを理由にこの成功を繰り返さない。次はclean候補の新規専用Release probeと生成コードを準備し、同じ固定2条件を事前計画へする。4案目の性能・製品GUI open・全体受理は未了。

#### 本日の準備の終点 — 4案目は測定前で保持

実装と上記成功結果をclean `348cfb3ac5e1f00889a0ce655e07c187e8db8f6d`へ保存した。`python eng/protected-diff.py --base 60c9a8da67ce3fb0f76ef6f7368db5241b8c9093 --head HEAD`を静的モードで1回実行しexit 0。fixtures1853→1853、metadata/deleted/changed/addedは全0、保護対象ファイルは変更なし。`out/protected/348cfb3.json`の52scope件数は全て未測であり、実際に実行した13,939/307以外を成功扱いにしない。

同じclean版から通常CMake Releaseの専用`build/release-scalar-fold-20261014-348cfb3`へ`nib_perf_probes`を並列2でbuildし、135 steps成功。probe SHA-256は`4e4eec418d415f7167e47240d3fb5a28c86eb33e2ac396b3b5bee387d10e33ac`。before e5f8e70のprobeは`80411092cd16f21ec44b0f0699e97e6ae4f07b5495e34e9a1f7e67e244d893b9`のまま。生成コードは`scan`/`decoded_continuation`を展開し、文字ごとの追加callは無い。offset1/2/3のbyteを各1回読み、検査と値への追加に共用してからscalar判定を行う。残るcall relocationはpending/tailを写す`memcpy`だけで、ASCIIの8byte走査命令列も維持した。生成コードは性能成功の代用ではない。

実測前の`perf-plan-scalar-fold.json`へ両commit/exe、tool/harness/input SHA、flags、ASCII→日本語/ABBA×3/各20 samples/warmup1/timeout60を固定した。採否文言の初稿は「局所悪化が解消してから製品open」と書いていたが、**測定前に**設計席が本件のAPI/機能リファクタリングの採用条件を明確化した。局所比較は効果と分布を事実として報告し、新しい自動閾値や比が1を超えるだけで拒否するゲートを作らない。実際の増加は隠さず、設計席が大きさと原因を判断し、必要な正式QLT-014 openへ進む。採用には対象契約・独立レビュー・正式openが必要で、改善率の主張を前提にしない。既存の基準/閾値、入力、exe、flags、回数、順序は変更していない。初稿を`perf-plan-scalar-fold-initial.json`（SHA-256 `3db6c1104e6e384fd2a89ec6dafb221a215d6a944c298de2c82f2fe4021eaaa4`）へ保存し、最終planのSHAは`f57cb130c986a47c06f126b5215b9139cfbb6c535f05f450defdc514c71ef306`。両hashと理由は`plan-clarification.json`に記録した。

実測前のexe/config/tool実体9件、tool/harness75件、両入力のbytes/FNV/SHAは全一致、sourceはcleanだったが、CPU3標本が16/40/35%のためprobe起動前で止めた。読み取り診断では約8秒のCPU deltaでWindowsTerminalが全20logical換算約5.45%を占め、Chrome/Dropbox/Claude/他の実装プロセスにも活動があった。診断後の3標本は24/26/22%。この観測だけで活動の原因や他案件を特定せず、preflightの一時負荷だけとも断定しない。アプリを閉じたり、任意のプロセスを終了したりしていない。

待機中に正式openに必要な製品を先に準備する順序変更を`operational-order-adjustment.json`へ**4案目の測定0回の時点で**記録した。clean348cfb3の新規`build/release-348cfb3`で正典`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`を`CMAKE_BUILD_PARALLEL_LEVEL=2`として実行し、NeNeNib target127 steps成功、configure59.631秒/build404.221秒/total463.852秒。`NeNeNib.exe`は1,529,856 bytes、SHA-256 `82d8102968ad4280ca74afe365c0883b9a0800a83fb3e5adee5842012d983857`。正規記録は`out/release/348cfb3.json`で、製品は起動していない。過去のexe/builddirを上書きせず、probe計画も変更していない。

build完了という状態変化後に10秒idleを置いた1回の確認は22/15/9%。下がる傾向があったため、設計席の明示指示で45秒のsleep（25+20秒、間にhash/収載/CIMなし）を一度置いてから3標本を確認したが24/25/11%で、低負荷へ安定したとは判断できなかった。各確認でbuild/testプロセスは不在。元の全値と理由を個別JSONへ残し、低い値が出るまでpollすることはしない。専有環境とは主張しない。

**4案目の局所比較と正式QLT-014 openはそれぞれ0回。性能は失敗ではなく未知・未受理で、PR453はdraft・Ready/CI/main統合は未了。** 今回のhideの測定許可と実機が空いているとの申告は時点付きで記録し、次回はその時点の時間帯/負荷を確認する。実装・対象契約・独立レビュー・probe/製品Releaseは準備済みで保持する。

本節とADR状態/currentだけの後続commitは文書変更であり、src/tests/eng/CMakeとの差分0を照合してbuild/検証版348cfb3との対応を記録する。成功済み契約を再実行・再build・再configureしない。恒久先は新規`D:/NeNeNib/evidence/streaming-utf8-452/checkpoint-scalar-fold-20261014/`。現在の全原ログ、両plan、負荷確認、script、レビュー、assembly、PR本文、正確な両probe/configと製品exe/正規記録をSHA照合して収載する。初回/2案目と3案目の以前のcheckpointは参照として保持し上書きしない。未受理候補と実行物が必要なため2作業木とoutputsは削除せず、技術受理・main統合・必要反映・恒久収載後の安全監査まで残す。保存schemaの変更なし。Waivers: none。
