# 速さの記録 — 施主の実機の基準値（QLT-014 / ADR 0011）

> Status: 記録 / 最終実測 2026-09-17（Issue #16・起動の内訳は Issue #19・窓を先に見せる Issue #24）。正本の値は `eng/perf-reference.json`（`eng/measure-speed.py --adopt` が書く）。ここは環境・手順・ばらつきの記録。
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
| startup-window-shown | プロセス生成 → 最初の `window_shown`（2026-09-17・Issue #24 の段 2 で足した 5 本目の鍵） | 34.9 ms | 32.2 | 38.0 | 43.7 ms |

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

## `D3D11CreateDevice` の実験（Issue #24・段 1）

**30 ms 以上縮んだ実験は無い。** 実験 a・b・c・d はどれも対照の ±6 ms に収まり（ばらつきの幅の中）、
`device_created` の 159 ms を `D3D11CreateDevice` の呼び方で削る道は見つからなかった。
削れたのは WARP を強制した補足 e だけで、これは NVIDIA のユーザーモードドライバを読まないという意味であって、直し方ではない。

実測: 2026-09-17・同じ機械（指紋 `bc8a356f37c68491`・RTX 3090 / 120 DPI・ドライバ 32.0.16.1088）・Release の exe。
各実験は `src/ui/win32/Direct2DRenderer.cpp` の `create_device` を一時的に変え、
`cmake --build build-release --target NeNeNib` の後に `python eng/measure-speed.py --record` を 1 回（5 回の中央値）。
6 本を続けて測ったので、機械の状態は 6 本で同じである。**基準値・`eng/perf-reference.json`・ゲートは変えていない。**

| 実験 | 変えたこと | `device_created` 中央値 | 最小 | 最大 | 起動全体の中央値 | 対照との差 |
| --- | --- | --- | --- | --- | --- | --- |
| 0（対照） | 変えない | 159.0 ms | 155.5 | 173.1 | 185.8 ms | — |
| a | `D3D_FEATURE_LEVEL` の配列 `{11_1, 11_0}` を渡す（いまは `nullptr, 0`） | 161.8 ms | 156.3 | 184.9 | 192.4 ms | **+2.8 ms** |
| b | `CreateDXGIFactory2` → `EnumAdapters1(0)` の adapter を `D3D_DRIVER_TYPE_UNKNOWN` に渡す | 164.6 ms | 156.0 | 166.7 | 188.8 ms | **+5.6 ms** |
| c | flags に `D3D11_CREATE_DEVICE_SINGLETHREADED` を足す | 158.2 ms | 155.5 | 162.0 | 183.6 ms | **-0.8 ms** |
| d | 変えずに `--record` を連続 2 回走らせた 2 回目 | 159.5 ms | 156.6 | 171.6 | 184.8 ms | **+0.5 ms** |
| e（補足） | `D3D_DRIVER_TYPE_WARP` を強制（製品には入れない） | 9.5 ms | 9.1 | 11.7 | 37.6 ms | **-149.5 ms** |

所見:

- **a（機能レベルの配列）**: 縮まない。むしろ 2.8 ms 増えたが、対照の最大 173.1 ms との幅を見れば差とは言えない。既定の全機能レベルの試行は費用ではない
- **b（adapter を明示）**: 縮まない。この列の `device_created` には `CreateDXGIFactory2` と `EnumAdapters1` も入るので +5.6 ms のうち数 ms はそれ自身の費用である。`D3D_DRIVER_TYPE_HARDWARE` の内部の adapter 列挙は重複の費用になっていない
- **c（SINGLETHREADED）**: 縮まない（-0.8 ms）。ロックの初期化は費用ではない。なお製品に入れるなら ADR 0004 の「UI スレッド＋1 本」との整合を別に見る必要があり、0.8 ms のために足す理由は無い
- **d（連続 2 回）**: 1 回目 159.0 ms → 2 回目 159.5 ms で動かない。**ドライバ DLL のファイルキャッシュは効いていない**。5 回の起動を 2 回繰り返しても値が同じなので、159 ms はコールドスタートの読み込み費用ではなく、毎回必ず払う初期化である
- **e（WARP）**: 9.5 ms。CI（Hyper-V Video＝WARP）の 3.6 ms と桁が合う（CI は 96 DPI・別 CPU）。起動全体も 37.6 ms まで落ちる。**159 ms はまるごと NVIDIA のユーザーモードドライバの初期化である**ことが実機でも確かめられた

`device_created` の区間で読まれているもの（`tasklist /m nv*`。ADR 0011 の決定 8 により Process Monitor / ETW は使わない）:
`nvwgf2umx.dll`（D3D11 のユーザーモードドライバ・**86.3 MiB**）・`nvldumdx.dll`・`nvgpucomp64.dll`・`NvMemMapStoragex.dll`・`nvppex.dll`。
86 MiB の DLL の読み込みと初期化が d でキャッシュに効かない以上、**呼び方を変える道は閉じている**と読める。残るのは「いつ払うか」（順番・並行）を変える道だけである。

記録した JSON（git 対象外）:

| 実験 | パス |
| --- | --- |
| 0（対照） | `out/speed/2026-09-16T15-32-16Z.json` |
| d（連続 2 回目） | `out/speed/2026-09-16T15-33-07Z.json` |
| a | `out/speed/2026-09-16T15-34-10Z.json` |
| b | `out/speed/2026-09-16T15-35-15Z.json` |
| c | `out/speed/2026-09-16T15-36-11Z.json` |
| e | `out/speed/2026-09-16T15-37-11Z.json` |

（ファイル名の刻は機械の時計。実験は 0 → d → a → b → c → e の順に続けて走らせた。）

製品コードは実験のあと `git checkout -- src/` で戻し、`build-release` を戻した src で build し直した。この節の変更は docs だけである。

## 窓を先に見せる（Issue #24・段 2・ADR 0013）

`ShowWindow(SW_SHOW)` を `start_rendering`（device の生成）より**前**に出し、`core::Milestone::window_shown` を打つ順に変えた。
**窓は 30〜35 ms で見える**（採用した基準値は 34.9 ms）。`startup-first-frame` の定義と基準値は変えていない（ADR 0013 の決定 4）。

実測: 2026-09-17・同じ機械（指紋 `bc8a356f37c68491`・RTX 3090 / 120 DPI）・Release の exe・`--record` 1 回（各 5 回の中央値）。
記録した JSON: `out/speed/2026-09-16T15-51-02Z.json`（git 対象外。名前の刻は UTC）。

| ベンチ | 中央値 | 最小 | 最大 | 基準値 | 許容上限 | 判定 |
| --- | --- | --- | --- | --- | --- | --- |
| startup-first-frame | 212.9 ms | 200.1 | 226.0 | 191.5 ms | 239.4 ms | 許容内（+21.4 ms・+11%） |
| **startup-window-shown** | **34.9 ms** | 32.2 | 38.0 | 採用（この回） | 43.7 ms | 新規 |
| key-to-frame-single | 0.889 ms | 0.821 | 0.978 | 0.906 ms | 2.906 ms | 許容内 |
| key-to-frame-burst-200 | 2.348 ms | 2.270 | 3.120 | 2.695 ms | 4.695 ms | 許容内 |
| open-large-file-16mib | 268.3 ms | 258.6 | 296.7 | 249.8 ms | 312.2 ms | 許容内 |

5 回の `startup-window-shown`: 38.04 / 32.20 / **34.93** / 34.43 / 37.62 ms（太字が中央値）。
`eng/measure-speed.py --adopt --bench startup-window-shown` で施主の実機にこの 1 本だけを足した。**既存 4 本の中央値は動かしていない。**

採用のあとに `--check` を 3 回走らせた。3 回とも `Speed: 5 benches checked, 0 regression(s)`。

| `--check` | 機械の状態 | startup-first-frame | startup-window-shown | 1 打鍵 | 200 打鍵 | 16 MiB |
| --- | --- | --- | --- | --- | --- | --- |
| 1 回目（採用直後のフルゲート） | build の直後で忙しい | 217.6 ms | 38.2 ms | 0.981 | 2.493 | 283.3 |
| 2 回目 | 静か | **195.9 ms** | **30.3 ms** | 1.023 | 2.425 | 251.5 |
| 3 回目（最後のフルゲート） | 静か | 201.6 ms | 32.5 ms | 0.845 | 2.443 | 261.8 |

**採用した 34.9 ms は忙しい機械で測った値で、静かな機械では 30.3〜32.5 ms。** 許容上限 43.7 ms との余裕は
他の 4 本より狭い（忙しい回には 50.1 ms の試行が 1 つ出た）。基準値を動かすのは ADR の判断なので、ここでは記録だけする。

### 順番の前後の内訳（`startup-first-frame`・中央値 ms）

| 区間 | 前（段 1 の対照・2026-09-17） | 後（この回） | 読み |
| --- | --- | --- | --- |
| `origin` | 11.6 | 12.4 | 同じ |
| `document_opened` | 0.0 | 0.0 | 同じ |
| `window_created` | 2.8 | 3.9 | 同じ（幅の中） |
| `backdrop_applied` | 0.6 | 0.8 | 同じ |
| **`window_shown`** | —（最後に居た） | **16.9** | `ShowWindow(SW_SHOW)` が返るまで。ここまでで窓が見える |
| `device_created` | 159.6 | 166.0 | `D3D11CreateDevice`。順番を変えても縮まない（段 1 のとおり） |
| `swap_chain_created` | 1.6 | 1.5 | 同じ |
| `composition_bound` | 0.8 | 0.8 | 同じ |
| `context_created` | 0.6 | 0.7 | 同じ |
| `text_formats_created` | 0.8 | 0.8 | 同じ |
| `frame_presented` | 4.7 | 4.5 | 同じ |

`open-large-file-16mib` も同じ形: `origin` 15.6 | `document_opened` 51.5 | `window_created` 3.5 | `backdrop_applied` 1.0 |
**`window_shown` 17.3** | `device_created` 164.9 | `swap_chain_created` 1.5 | `composition_bound` 0.8 | `context_created` 0.7 |
`text_formats_created` 0.9 | `frame_presented` 6.0。

正直に書く: `startup-first-frame` は**増えた**（許容内なのでゲートは通る）。
増分は「`ShowWindow` が返るまで」の 15.3〜19.6 ms で、これは順番を変える前は最初のフレームより**後**に払っていた費用が
測る区間の中に入ってきたものである。`device_created` は 156.0〜172.1 ms で、段 1 の対照の幅（155.5〜173.1 ms）の中にある。
上の `--record` の 212.9 ms と最初のフルゲートの 217.6 ms は build の直後の忙しい機械の値で、
静かな機械では **195.9〜201.6 ms**（基準値 191.5 ms に対して +4〜10 ms）である。
**窓が見えるまでは 185.8 ms → 30〜35 ms**（体感の起動は 5 分の 1 以下）。基準値を動かすのは次の ADR の仕事で、ここでは記録だけする。

### 窓が見えてから最初のフレームまでの画素（ADR 0013 の強制・却下の条件）

`eng/verify-window.py` の `verify_first_paint` が、`--measure` 付きで起動した exe のクライアント領域の中央を
`GetPixel` で約 15 ms ごとに読み、本文の背景が出るまでの列を記録する（`out/window-verification/look-slice-results.json` の `firstPaint`）。

実測: 2026-09-17・同じ機械・**Debug の exe**（`build/NeNeNib.exe`。この検査の既定）・OS はダーク。
経過は `start()` が窓を見つけた時刻からで、約 15 ms ごとに 1 点（`Sleep` の粒度）。

| 経過 | 画素 (R,G,B) | 何が見えているか |
| --- | --- | --- |
| 0 / 15 / 31 / 46 / 62 / 78 / 93 / 109 / 125 / 140 / 156 / 171 / 187 ms | (32, 32, 32) | DWM の Mica の面だけ（13 回とも同じ） |
| 203 ms | (48, 10, 36) | 最初のフレーム＝本文の背景（茄子色 `#300A24`・D11） |

- **黒 (0,0,0) は 1 度も出ない。白 (255,255,255) も 1 度も出ない** → ADR 0013 の却下の条件に当たらない
- 最初のフレームの背景との最大の差は **22**（`FIRST_PAINT_STEP` の 32 以内・「1 段の差」に収まる）
- この回の節目（節目は最初の読みだけを見る）: プロセス生成 → `window_shown` **45.3 ms**、
  `window_shown` → 最初の `frame_presented` **204.2 ms**（Debug の exe なので Release の 15 / 156 ms より遅い）
- 画は `out/window-verification/first-paint.bmp`（git 対象外）
- この節は**画面中央に他の窓が被っていると測れない**（被った画素は `ours` が偽になり判定に使わない）。
  走らせる前に画面の中央を空けておく。覆われていたら「背景が中央に届かなかった」と言って落ちる（黒や白と間違えない）

見えているのはダークの Mica の面で、本文の茄子色そのものではない（DWM が壁紙を暗く畳んだ灰）。
利用者が見るのは「暗い面 → 本文」の 2 段で、黒や白の閃きは無い。

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
