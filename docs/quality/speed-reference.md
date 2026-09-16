# 速さの記録 — 施主の実機の基準値（QLT-014 / ADR 0011）

> Status: 記録 / 最終実測 2026-09-16（Issue #16・起動の内訳は Issue #19）。正本の値は `eng/perf-reference.json`（`eng/measure-speed.py --adopt` が書く）。ここは環境・手順・ばらつきの記録。
> 中央値を上げる・許容退行を広げるのは ADR の判断であって、落ちたときの修理ではない（QLT-014）。

## 環境

| 項目 | 値 |
| --- | --- |
| 機械の指紋 | `bc8a356f37c68491` |
| CPU | Intel(R) Core(TM) i9-10850K CPU @ 3.60GHz |
| GPU | NVIDIA GeForce RTX 3090 |
| DPI | 120 |
| OS | Windows 11 Pro build 26200・ダーク |
| exe | `build-release/NeNeNib.exe`（Release・`/MT`・サニタイザなし。**Debug の exe は ASan / UBSan の数字になる**） |
| 計測 | `python eng/measure-speed.py --record`（各ベンチ 5 回・中央値）。節目は exe が `--measure <out.json>` で書く |

## 採用した値（2026-09-16・1 回目の `--record`）

| ベンチ | 何を測るか | 中央値 | 最小 | 最大 | 許容上限（25%・下限 2 ms） |
| --- | --- | --- | --- | --- | --- |
| startup-first-frame | プロセス生成 → 最初の `frame_presented` | 191.5 ms | 185.4 | 197.6 | 239.4 ms |
| key-to-frame-single | 1 つの `WM_CHAR` の `input_received` → 次の `frame_presented` | 0.906 ms | 0.888 | 1.056 | 2.906 ms |
| key-to-frame-burst-200 | 200 の `WM_CHAR` をまとめて post → 最後の `frame_presented` | 2.695 ms | 2.324 | 2.754 | 4.695 ms |
| open-large-file-16mib | 16,800,000 バイト・200,000 行の UTF-8 CRLF を起動引数で開く → 最初の `frame_presented` | 249.8 ms | 234.9 | 250.5 | 312.2 ms |

同日の 2 回目の `--record`（採用しない。中央値の一致を見るため）: 198.7 / 0.999 / 2.450 / 263.0 ms。2 回目は 200 打鍵の 5 回のうち 1 回が 3 度測り直しても「まとめて」届かず 384.8 ms として残った（中央値には効かない）。

## 起動の内訳（Issue #19）

`core::Milestone` に起動の節目を 8 つ足し、`eng/measure-speed.py --record` が区間ごとの中央値を出す。
最初の区間 `origin` だけは節目ではなく、`Win32TimingAdapter` が JSON に書く `processCreationToOriginMs`
（プロセス生成 → `bind`）である。区間名は「その区間の終わりに打つ節目の名前」で、値は 5 回の中央値。

実測: 2026-09-16・同じ機械（指紋 `bc8a356f37c68491`・RTX 3090 / 120 DPI）・Release の exe・`--record` 1 回。
この回のベンチの中央値は 184.2 / 0.914 / 2.504 / 235.9 ms（採用値は上の表のまま。**基準値は変えていない**）。

### startup-first-frame（中央値 184.2 ms）

| 区間 | 中央値 | 最小 | 最大 | その区間が含むもの | CI |
| --- | --- | --- | --- | --- | --- |
| `origin` | 11.6 ms | 11.0 | 17.5 | プロセス生成 → `bind`（loader・CRT・`CoInitializeEx`・引数の取り出し） | 9.4 ms |
| `document_opened` | 0.0 ms | 0.0 | 0.1 | adapters の構築・`EditorController` の生成（起動引数のファイルは無い） | 0.0 ms |
| `window_created` | 2.8 ms | 2.8 | 3.4 | `RegisterClassExW`・`CreateWindowExW`（`WM_NCCALCSIZE` 等） | 1.1 ms |
| `backdrop_applied` | 0.6 ms | 0.6 | 1.1 | `GetDpiForWindow`・`SetWindowPos` で中央寄せ・`controller_.frame()`・`DwmSetWindowAttribute` × 2 | 0.6 ms |
| `device_created` | **159.6 ms** | 156.9 | 190.4 | `D3D11CreateDevice(HARDWARE)`・`As(dxgi_device_)` | 3.6 ms |
| `swap_chain_created` | 1.6 ms | 1.5 | 1.8 | `CreateDXGIFactory2`・`CreateSwapChainForComposition`・待機可能オブジェクト | 0.8 ms |
| `composition_bound` | 0.8 ms | 0.7 | 0.9 | `DCompositionCreateDevice`・target・visual・`Commit` | 0.4 ms |
| `context_created` | 0.6 ms | 0.6 | 0.8 | `D2D1CreateFactory`・D2D device / context・ターゲットの結び付け | 0.4 ms |
| `text_formats_created` | 0.8 ms | 0.8 | 0.9 | `DWriteCreateFactory`・`GetSystemFontCollection` × 2・`CreateTextFormat` × 6 | 0.6 ms |
| `frame_presented` | 4.7 ms | 4.1 | 5.6 | `VisibleLines` の適用・最初の layout・描画・`Present` が返るまで | 3.8 ms |

### open-large-file-16mib（中央値 235.9 ms）

| 区間 | 中央値 | 最小 | 最大 | startup との違い | CI |
| --- | --- | --- | --- | --- | --- |
| `origin` | 12.5 ms | 12.1 | 13.6 | 同じ | 9.4 ms |
| `document_opened` | 47.5 ms | 46.9 | 74.3 | ここだけが違う。16.0 MiB・20 万行の読み込み・符号の判別・復号・piece table の構築 | 60.0 ms |
| `window_created` | 3.4 ms | 3.1 | 5.0 | 同じ | 1.6 ms |
| `backdrop_applied` | 0.8 ms | 0.7 | 1.8 | 同じ | 0.6 ms |
| `device_created` | **161.4 ms** | 157.3 | 223.3 | 同じ | 4.1 ms |
| `swap_chain_created` | 1.6 ms | 1.5 | 1.8 | 同じ | 0.8 ms |
| `composition_bound` | 0.7 ms | 0.7 | 0.9 | 同じ | 0.4 ms |
| `context_created` | 0.7 ms | 0.6 | 0.8 | 同じ | 0.4 ms |
| `text_formats_created` | 0.8 ms | 0.8 | 1.0 | 同じ | 0.6 ms |
| `frame_presented` | 6.4 ms | 5.8 | 8.5 | 20 万行のうち見える行だけを描くので startup とほぼ同じ | 5.8 ms |

### 170 ms はどこに乗っているか

**`device_created` の 159.6 ms（起動 184.2 ms の 87%）である。** この区間には `D3D11CreateDevice(D3D_DRIVER_TYPE_HARDWARE)` と
`device_.As(&dxgi_device_)` しか無い。16 MiB でも同じ 161.4 ms が乗り、CI（WARP / 96 DPI）の起動が全部で 20.4 ms だったことと合わせて、
**実機と CI の約 170 ms の差は NVIDIA のユーザーモードドライバを読み込む `D3D11CreateDevice` そのもの**だと読める（設計の予想どおり）。

予想が外れた所も記録する。

- `GetSystemFontCollection`（実機はフォントが多い）は `text_formats_created` の 0.8 ms に収まっていて、疑う余地が無い
- DirectComposition は 0.8 ms、Mica の `DwmSetWindowAttribute` × 2 を含む `backdrop_applied` は 0.6 ms。どちらも 1 ms 未満
- プロセス生成 → `wWinMain` の loader（`origin`）は 11.6 ms で、3 番目に大きいが桁が違う
- 16 MiB の読み込み（`document_opened` 47.5 ms）は 2 番目に大きい。CI の 16 MiB が 70.1 ms だったことと矛盾しない

CI（PR #23・run 35104611209・AMD EPYC 7763 / Hyper-V Video＝WARP / 96 DPI）の列も同じ形で取った。CI の起動 21.5 ms に対して `device_created` は 3.6 ms、実機は 159.6 ms。**差 156 ms がそのまま実機と CI の差（約 170 ms）を説明する。** CI の 16 MiB の読み込みは 60.0 ms で実機（47.5 ms）より遅く、ここは CPU の差。CI の起動は 5 回のうち 1 回目だけ 303.5 ms（exe の初回読み込み）で、中央値は動かない。

**直すのは別 Issue（ADR は直すときに起こす。番号は受理順）。** この Issue は測って記録するところまでで、経路は変えていない。
遅延できそうな候補は 2 つだけ挙げておく（設計はしない）: ①窓を見せてから device を作る（最初のフレームより前に `ShowWindow` する順に変える）、
②`D3D11CreateDevice` を adapters の worker で先に走らせる（ADR 0004 の「UI スレッド＋1 本」の枠内に収まるか要検討）。

記録した JSON: `out/speed/2026-09-16T13-40-14Z.json`（git 対象外）。

## `WM_PAINT` への集約の効果（Debug の exe・同じ機械・前後比較）

| | 200 打鍵の合計 | 1 打鍵 |
| --- | --- | --- |
| 前（意図ごとに `Present`） | 6631 ms（3 回: 6624〜6646） | 3.53 ms |
| 後（`InvalidateRect` → `WM_PAINT` で 1 回） | 29.9 ms | 3.55 ms |

まとめて来た入力は 1 フレームで描け、1 打鍵の遅延は変わらない（ADR 0011 の決定 6）。Release では 200 打鍵 2.7 ms・1 打鍵 0.91 ms。

## Debug と Release の差（同じベンチ・同じ機械）

| ベンチ | Debug（ASan / UBSan） | Release |
| --- | --- | --- |
| startup-first-frame | 213.7 ms | 191.5 ms |
| key-to-frame-single | 3.55 ms | 0.91 ms |
| key-to-frame-burst-200 | 29.9 ms | 2.70 ms |
| open-large-file-16mib | 4217.5 ms | 249.8 ms |

## 計測器の癖（製品コードは計測のために変えていない）

- 隠れた窓は合成器が提示を間引き、1 打鍵に 200 ms 超の外れ値が出る。窓を最前面・前景にしてから測る
- 窓を出して最初に提示する 1 枚は交換鎖の暖機で数十 vsync 遅れる。測らない暖機の 1 打鍵を先に打つ
- Release の editor は Python の `PostMessageW` より速くキューを空にするので、「200 打鍵をまとめて post」が成立しない回がある。窓のスレッドを止めてから post し、節目の到着幅が 50 ms を越えた試行は最大 3 回測り直す。3 回とも駄目なら最後の値をそのまま記録して 1 行出す（中央値が守る）
- 測るのは `Present` が返るまでで、画面が光るまでではない（waitable swap chain・最大遅延 1 なので最大 1 vsync 後）

## 所要時間

`--check` は実機で約 33 秒（4 ベンチ × 5 回の起動）。フルゲート全体は Release の差分ビルドを含めて 2 分 19 秒（Debug を測っていたときより短い）。

## CI

CI の機械には指紋の一致する基準値が無いので記録だけで終了 0（`Speed: no reference for this machine; recorded only`）。窓が作れなければ `window unavailable` を記録して終了 0。

PR #18（run 35002057290）で GitHub の Windows ランナーでも窓が作れてベンチが動いた。指紋 `e7a87d5b6ac1e14b`（AMD EPYC 7763 / Microsoft Hyper-V Video＝WARP / 96 DPI）:

| ベンチ | 中央値 | 5 回の幅 |
| --- | --- | --- |
| startup-first-frame | 20.4 ms | 20.0〜258.9（1 回目だけ遅い） |
| key-to-frame-single | 1.344 ms | 1.304〜1.995 |
| key-to-frame-burst-200 | 2.700 ms | 2.564〜7.982 |
| open-large-file-16mib | 70.1 ms | 69.9〜75.6 |

**実機（RTX 3090）との差は起動と 16 MiB のどちらも約 170 ms。** ファイルの読み込みではなく、実機の最初のフレームに GPU / DWM 側の固定費が乗っていると読める。内訳は「起動の内訳（Issue #19）」の節で測った（`device_created` に 159.6 ms）。CI の基準値は同じ CI 機の値が数回たまってから別の Issue で決める（ADR 0006 の決定 1・ADR 0011 の決定 4）。
