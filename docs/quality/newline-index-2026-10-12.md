# 改行索引の容量制限 — Issue #446

## 変更と確認する退行

段階1A・[ADR 0121](../adr/0121-newline-index-is-bounded-by-source-bytes.md)。originalとaddの改行ごとのOffset列を、sourceの1 byteにつき1 bitと512 byteごとの累積数に置き換える。pieceのordinal窓、先端だけがaddを伸ばす所有規則、公開TextBufferの意味は維持する。

2 GiBでは索引の要素領域は計算上288 MiB。全LFの旧16 GiBから密度への依存をなくす一方、128 byteごとのLFでは旧128 MiBから増える。本文とallocatorを含むprivate bytesの実測値ではなく、2 GiB対応の成立判定ではない。

## 対象検証

| 対象 | この変更で起こり得る退行 | コマンド・結果 |
|---|---|---|
| core/applicationと直接利用targetのDebug | 型/所有と複雑度の違反、範囲外アクセス、未定義動作 | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build --target nib_tests NeNeNib --parallel 4`。成功（clang-cl 19.1.5・clang-tidy・ASan/UBSan） |
| 索引の契約 | bit/集計境界、空/密/疎、CRLF/NUL、EOF、共有add追記と分岐、chunk境界の行位置 | `build/nib_tests.exe --newline-index`。27,839 checks成功 |
| 本文 | piece切断と行位置、旧値、CRLF、UTF-8と範囲の退行 | `build/nib_tests.exe --buffer-range`。289 checks成功 |
| 折り返し表示 | 行番号/行先頭を読む表示と位置解決の退行 | `build/nib_tests.exe --wrapped-view`。35 checks成功 |
| Vim行ジャンプ | 行の選択・先頭/末尾移動の直接利用側の退行 | `build/nib_tests.exe --vim-line-jumps`。999 checks成功 |
| 境界/規約/整形 | 中核からのCPU判別等の外部依存、規約逸脱、編集行の空白 | `python eng/symbols.py --build-dir build --require core application`、`python eng/conformance.py --build-dir build`、変更C++の`clang-format --dry-run --Werror`、`git diff --check`。symbols 2 libraries/0違反、整形8ファイルと空白成功、規約はFile API補完後に0違反 |

初回のNewlineIndex objectの`llvm-nm --undefined-only`で`__isa_available`を検出。固定MSVC STLのbit関数がCPU機能を参照するため、許可リストを変更せず標準関数をconstexpr表の生成へ限定した。変更後のライブラリは2本とも違反0。最初のbuildは追加テストの入れ子深さ4（上限3）で停止し、データ構築を整理して修正版buildを成功させた。最初のsymbols/conformanceはCMake File APIのquery未作成で成立せず、`eng/check.ps1`と同じ`build/.cmake/api/v1/query/codemodel-v2`を作って同じDebug構成を再生成した。失敗記録も保持する。

## 実機速度の事前条件

実装と対象契約がそろったclean commitから`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`でReleaseを作り、commit/exe SHA-256を測定前に記録する。実行はADR 0114の時間調整後。専用profileと入力はD側worktreeの`out/speed`に置き、通常の設定や開いている文書を使わない。

既存`eng/measure-speed.py --check --bench <name> --repetitions 5 --executable <exe>`を次の固定順で各1回実行する。道具が既に定める暖機・欠測検出/上限付き再試行・入力を維持し、合格するまでコマンドを繰り返さない。欠測・失敗・外れ値を含む全出力とJSONを保存する。

1. `key-to-frame-single`: 空の本文への入力。共通の試行が送る200打鍵も既存どおり。
2. `key-to-frame-single-long-line`: 6,219,000 bytes・日本語長行1000行。入力SHA-256は道具の`ef2d1511a696984dcd4184498a852d944a8ca366bf847f440d1e46b73b32a6c5`と照合する。
3. `key-to-frame-burst-200-16mib`: 16,800,000 bytes・CRLF200,000行への200打鍵。索引の切断/追記と行位置の費用を確認する。
4. `open-large-file-16mib`: 同じ本文を開いて最初の描画まで。索引構築の費用を確認する。

比較先は追跡済み`eng/perf-reference.json`の同じ機械指紋。既存25%/2ms床の判定を維持し、基準値の採用/緩和は行わない。各5試行の記録値と欠測を示し、小差から改善を断定しない。起動専用・パレット・全件回帰は変更の直接影響を確認する対象へ含めない。

## 再利用と限界

検証はこの変更への初回。成功後は実装・テスト・関連依存・必要環境が同一なら、文書追加/commit/push/review/mergeによる再実行をしない。保存schema・基準値・閾値・許可シンボル・既存Vim fixtureを変更しない。Waivers: none。64 MiB読込上限、同期I/O、本文の所有コピー、2 GiB A/B試験は後続。

対象契約の合計は29,162 checks。ログの恒久収載先は`D:/NeNeNib/evidence/newline-index-446/`（統合時に原本とSHAを照合する）。

## 初回未測と測定器修正後の条件

2026-10-12 20:18 JSTにhideが実機測定を許可し、他エージェントの並行作業で負荷が安定しない可能性を伝えた。固定4コマンドの初回は全て`window unavailable; recorded only`となり、速度の値は得られていない。終了0を性能合格に数えない。測定用窓が可視・応答中でも既存の通常版が先に見つかる、同class先頭1窓だけの選択が原因（[修正記録 #448](window-owner-2026-10-12.md)）。

#448 / PR449 / main843822aの修正を取り込み、同じ4項目を同じ順/各5回で1組測る。変更したのはdriverの窓所有選択だけ。初回ログ/入力/profileを`out/speed-unobserved`へ保持し、新しい専用profileで開始する。旧試行の失敗を消す再試行ではなく、計測器修正後の最初の測定。製品commit2b4de9652a76a118d7fd04f73caf07fb9f4a1f21のRelease（SHA-256 `0d74775a7e1f14fa41e827cfabb14147169673ccda9745e75bd2b03aef4b8bf9`、1,529,344 bytes）と対象9ファイルは不変。対象テストは再利用する。測定前のdriver/参照値/設定/ReleaseのSHAは`perf-preflight-fixed.json`へ記録する。並行作業ありの条件を結果にも併記する。

## 修正後の実機結果と保留

2026-10-12 20:32〜20:36 JST、`measurementCommit`は6387764、上記の同じReleaseを使用。4コマンドを固定順で実行したが、各5回を完了する前に全コマンドが例外終了した。

| bench | 結果 |
|---|---|
| key-to-frame-single | 200打鍵の到着幅811.0 msの試行を既存の規定で無効化。その後、終了待ち15秒と後片付け5秒でTimeoutExpired、exit 1 |
| key-to-frame-single-long-line | 終了待ち15秒と後片付け5秒でTimeoutExpired、exit 1 |
| key-to-frame-burst-200-16mib | 終了待ち15秒と後片付け5秒でTimeoutExpired、exit 1 |
| open-large-file-16mib | 終了待ち15秒と後片付け5秒でTimeoutExpired、exit 1 |

窓所有選択の修正後は測定対象へ到達したが、集約JSONと正式な中央値を得ていない。初回と修正後のログ、残存raw marks、Release metadata、全入力SHAを保持する。並行作業ありの条件であり、負荷・I/O・製品側の原因は未分離。終了timeoutを性能退行の数値や合格値へ読み替えず、同条件での追加測定はしない。20:36 JST時点で残っていたNeNeNibは既存通常版PID21624だけで、測定用プロセスは残存しなかった。

PR447はdraft・main未統合で保持。次は落ち着いた時間帯で、終了待ちを含めて測定が成立するか確認する。必要なら同じ環境の変更前後を固定した計画で切り分ける。対象契約29,162 checksは成功のまま再利用できるが、速度の採用条件は未達成。未完了の`D:/NeNeNib/worktrees/446-newline-index`と`D:/NeNeNib/outputs/newline-index-20261012/`はRelease・入力・未成立ログを再確認するため保持する。

## 2026-10-13再開・固定5試行の結果

00:50 JSTにhideが他の作業を減らして試験を再開。ホストの独占利用や無負荷は保証しない。過去の失敗出力を`out/speed-timeout-20261012`へ保存してから新しい専用profile（既定wrap on）を作り、事前の4コマンドを各1回/5試行で実行した。製品Releaseは同じ2b4de96、測定開始時のHEADはf25f8c3。機械指紋bc8a356f37c68491、120 dpi。全20試行で欠測0、終了timeoutなし。

| bench | 全5試行の記録値（ms） | 中央値（ms） | 既存基準に対する結果 |
|---|---|---:|---|
| key-to-frame-single | 0.613, 0.540, 0.586, 0.632, 0.654 | 0.613 | pass、上限2.906 |
| key-to-frame-single-long-line | 6.500, 6.341, 5.113, 6.706, 6.323 | 6.341 | pass、上限7.431 |
| key-to-frame-burst-200-16mib | 18.577, 15.543, 15.255, 15.301, 15.468 | 15.468 | fail、基準7.411の25%増まで（9.26375） |
| open-large-file-16mib | 217.923, 213.292, 209.4842, 210.9568, 215.4961 | 213.292 | pass、上限312.229125 |

連打だけは変更前にも同じ失敗があるか確認する必要があるため、候補の再試行は追加せず、比較計画を`before-comparison-plan.txt`へ先に記録した。直前の製品commit `7b117e21b88d3c15df1ccea7327674fdd78a590c`を正典の`eng/build-release.ps1 -Ref <commit>`でbuild。Release SHA-256は`e1dda045d341784f8e46647105476a3695202c0374a249145aef35832c115750`、1,522,176 bytes。build完了後に候補の出力を`out/speed-index-20261013`へ保全し、新しい専用profile/同じdriver/入力/設定/回数で**連打だけ**を5試行測定した。

変更前は15.349, 14.943, 17.925, 15.050, 15.633 ms、中央値15.349 ms、欠測0で同じ速度基準を超過（exit 1）。候補との差は0.119 ms/約0.8%、範囲は重なる。候補5試行→build→変更前5試行の順でABBAではなく、小差の原因や回帰ゼロを証明する比較ではない。ただし基準超過が索引変更前にも存在することは確認できた。[#460](https://github.com/hideyukiMORI/nene-nib/issues/460)へ全値/条件/限界を分離し、QLT-012に沿って本変更を進める。4条件すべての速度合格とは報告しない。#460の原因修正や基準値の更新は本件に含めない。

openの内訳中央値はorigin 12.0、document_opened 12.4、device_created 156.5、frame_presented 4.4 ms。連打のkeysArrivalSpan中央値は候補14.842 ms。本文処理だけの所要時間と画面到達までの値を同一視しない。

### 記録・再利用・受理範囲

`D:/NeNeNib/evidence/newline-index-446/resume-20261013/`へ、再開ログ/preflight、before/candidateの全JSON・残存raw marks・入力・profile・両Release、旧失敗ログを収載した。75ファイル/95,162,688 bytesを元ファイルとSHA-256照合済み（`manifest.json`）。driverは`c992c7a7fa89a00c6aa8931d9a579562d36c9debba73817b83409cc32193aa02`、measure-speedは`5bdf7bcdfc41da51bfc3d3e715aeabefb4ea259947e4c0d63bde626b64a8fe08`。測定器は各trialのraw marksを上書きするため、5試行を保持する正式JSONと最後のraw marksを区別する。

対象9ファイルのSHAを`verified-sources.json`と照合し全件一致。2b4de96以降の製品src/単体tests/CMake/toolchain/targets/tool-versionsは不変。main取り込みも設計/報告文書のみであり、29,162 checks・Debug解析・symbols/conformance・今回の性能値を再利用する。境界修正を含む製品変更は再開時に行っていない。自己レビューでsource単位の所有、旧pieceのordinal上限、rank/select境界、正典一本化、外部CPU依存の不在を確認した。

受理対象は索引表現とその直接利用。2GiBの実容量/全LF構築時間、元本文の所有と読込上限/非同期化、既存の連打基準超過は未解決。Waivers: none。
