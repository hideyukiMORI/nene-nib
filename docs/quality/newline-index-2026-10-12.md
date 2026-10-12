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
