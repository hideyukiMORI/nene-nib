# 改行の索引の細身化と上限の 1 本化 — Issue #792

[ADR 0121](../adr/0121-newline-index-is-bounded-by-source-bytes.md) の追補「2026-10-22: 塊ごとの疎 / 密の 2 表現」の実測の記録。巨大ファイルの段 1（親 #791・[計画](../plans/2026-10-21-large-file.md) §3.1・D63）。

## payload（索引を作る間に増えた確保の byte 数）

probe `nib_perf_probes` の `newline-index-payload-*-16mib`。`NewlineIndex::from_text` の前後で確保の byte 数を数える（Release は probe の exe の `operator new` / `delete` を置き換えて `_msize`、Debug は ASan の確保の統計・どちらも同じ値）。変更前は `origin/main`（`3dd862f`・索引は `6abb470` と同じ）に同じ probe の 9 ファイルだけを写した別の作業木の Release、変更後はこの branch の Release。どちらも warmup 1 + 3 回で値は毎回同じ。

| 本文（16 MiB） | inputBytes | 変更前 | 変更後 | 本文に対する比（前 → 後） |
|---|---:|---:|---:|---|
| 全 LF（`newline-index-payload-all-lf-16mib`） | 16,777,216 | 3,407,989 | 2,197,326 | 0.203 → 0.131 |
| 1 行 60 byte の英文（`newline-index-payload-prose-16mib`） | 16,777,200 | 3,407,949 | 583,894 | 0.203 → 0.035 |
| 1 行 16 MiB（`newline-index-payload-one-line-16mib`） | 16,777,216 | 3,407,989 | 24,654 | 0.203 → 0.0015 |

- 変更前は密度に依らず 1 byte に 1 bit（12.5%）・512 byte ごとの累積（1.56%）・512 byte ごとの要約（6.25%）で 0.203 倍。
- 変更後の全 LF は 256 塊がすべて密（8 KiB の bit と 256 byte の区切りの累積と塊ごとの 96 byte）。英文は 1 塊に約 1,092 個の LF で疎（2 byte × 個数）。1 行は塊ごとの固定の 96 byte（累積 8・要約 32・`std::variant` 56）だけ。
- 目標「典型の本文で 5% 以下・最悪でも今以下」（決定 1）を満たす。2 GiB の英文なら約 71 MiB の計算。

## 速さ

席は測らない（共通の約束）。`lengths` の両端の走査（最大 2 × 64 KiB）の費用は `newline-index-lengths-all-lf-16mib`（全 LF・両端が塊の途中の窓を 64 回）で設計席が測る。`G` `gg`・16 MiB open・16 MiB 連打は 8 本のゲートで示す。

## 対象検証（Debug・clang-cl 19.1.5・clang-tidy・ASan/UBSan）

| 対象 | 起こり得る退行 | 結果 |
|---|---|---|
| 索引の契約 `--newline-index` | rank / select の境・疎と密の隣り合い・4,095 / 4,096 / 4,097 個・64 KiB の境の LF・200 KiB の 1 行・NUL / CRLF・窓の要約・打鍵で疎から密へ変わる add の chunk と変わる前の値 | 965,762 checks 成功（変更前の同じ scope は 28,134 checks・実行時間は前後とも約 44 s） |
| 本文・表示・Vim の直接の利用側 | piece の切断と行位置・折り返し・行の移動・格子・仮想桁 | `--buffer-range` 289・`--wrapped-view` 42・`--vim-line-jumps` 999・`--virtual-columns` 85・`--grid-line` 113 checks 成功 |
| 上限の契約 | 開く上限の値・64 MiB + 1 は `too_large`・退避の読み戻しの上限 | `--application` 22,741・`--file-text` 402・`--autosave` 86 checks 成功・`nib_autosave_tests` 58 checks 0 failures |
| probe | 表の行と期待値のずれ | 追加 4 本と `ProbeWorkload` の 40 本を Debug で 1 回ずつ流して成功 |
| 境界・規約・整形 | core の外のシンボル（`__isa_available`）・規約・整形 | `python eng/symbols.py --build-dir build --require core application` 3 libraries 0 violation・`python eng/conformance.py`（`--build-dir build` も）0 violation・変更 C++ の `clang-format --dry-run --Werror`・`git diff --check` 成功 |

最初の build は `append` の lambda の入れ子 4（上限 3）・probe の `throw`（例外を使わない構成）・ASan の `operator new` との重複で 3 回止まり、それぞれ直した。最初の symbols / conformance は CMake File API の query が無く成立せず、`eng/check.ps1` と同じ query を作って configure し直した。
