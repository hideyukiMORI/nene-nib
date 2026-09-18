# ADR 0011 — 速さの縦切り: `TimingPort` の節目・ベンチ 3 本と機械ごとの基準値・`WM_PAINT` で 1 フレームにまとめる描画

- 状態: 受理
- 日付: 2026-09-16
- Issue: #16
- 影響する規則: ARC-003 / ARC-006 / ARC-007 / ARC-010 / ARC-011 / CPP-002 / CPP-005 / CPP-013 / CPP-017 / QLT-013 / QLT-014

## 文脈

ADR 0006 の決定 1 は「ベンチ 3 本を計測スクリプトで測り、`eng/perf-reference.json` の基準値からの退行でゲートを落とす。目標値は初版を実測してから決める」とし、
QLT-014 は planned のままである。Issue #7 の実測では、1 打鍵ごとに `Present(1, 0)` で vsync を待つため、まとめて post した 200 回の Enter が 6 秒で 181 行しか進まなかった。
Vim エンジンと IME の縦切りに入る前に、いまの速さを測って基準値を置き、退行が見える状態にする必要がある。

制約: 現在時刻を持てるのは `src/adapters/win32` だけ（ARC-007）。窓（ui/win32）も application も時計を読めない。CI の機械は揺れる（ADR 0006）。

## 決定

**速さは、窓が打つ「節目」を adapters の時計で刻み、`eng/measure-speed.py` が exe を起動して 3 本のベンチを 5 回ずつ測り、機械の指紋ごとの基準値と比べてゲートを落とす。描画は `WM_PAINT` で 1 フレームに 1 回にまとめる。**

1. **節目は `core::Milestone` の閉じた enum**（`input_received` / `frame_presented`。最初の `frame_presented` が「最初の描画」）。application が `TimingPort`（`void mark(core::Milestone) noexcept` ただ 1 つ）を宣言し、
   窓は節目を打つだけで時刻を知らない（ARC-007 / ARC-011）。`input_received` は `WM_CHAR` / `WM_KEYDOWN` の受信、`frame_presented` は `Present` が返った直後
2. **実装は `Win32TimingAdapter`（adapters/win32）だけ**が `QueryPerformanceCounter` と `GetProcessTimes`（プロセス生成時刻）を読む。合成ルートが `--measure <out.json>` を受け取ったときだけ記録し、
   終了時に JSON（プロセス生成 → 最初の `frame_presented` の ms、節目の列と µs）を書く。無指定なら記録しない実装（同じ型の「記録しない」状態。ポートの実装は 1 つ。`bind(path)` を呼ぶまで何も積まない。
   `mark` を本物の `noexcept` にするため `bind` で 4096 件ぶん確保し、以後は割り当てない。溢れた節目は捨てる）
3. **ベンチ 3 本**（ADR 0006 の決定 1 の名前を保つ）:
   ① `startup-first-frame`: プロセス生成時刻 → 最初の `frame_presented`
   ② `key-to-frame`: 1 打鍵の `input_received` → 次の `frame_presented`（中央値）と、200 打鍵をまとめて post したときの最初の `input_received` → 最後の `frame_presented`（合計）
   ③ `open-large-file`: 16 MiB・20 万行の UTF-8 CRLF を起動引数で開いて最初の `frame_presented` まで（**1 GB はメモリマップの縦切りで置き換える**。名前はそのままで、大きさは基準値に記録する）
   各 5 回の中央値を値とし、5 回のばらつき（最小・最大）も記録する。基準値の鍵は `startup-first-frame` / `key-to-frame-single` / `key-to-frame-burst-200` / `open-large-file-16mib` の 4 つ（③は 200,000 行 × 84 バイト＝16.02 MiB）。
   **測るのは Release 構成の exe**（`build-release/NeNeNib.exe`）。Debug 構成は ASan / UBSan の計装（ADR 0003）で数字がサニタイザのものになる。`verify-window.py` は今までどおり Debug の exe で動かす
4. **基準値は `eng/perf-reference.json` に機械の指紋ごと**（CPU 名・GPU アダプタの説明・DPI の組）。項目ごとに中央値と許容退行（% と絶対値の下限 ms。小さな値の揺れを % だけで見ない）。
   `--adopt` が現在の機械の値を書き、`--check` が比べる。**指紋の一致する基準値が無い機械では値を記録して終了 0**（CI を含む。CI の基準値は同じ CI 機での値が数回たまってから別の Issue で決める）
5. **ゲート**: `eng/check.ps1` の CTest の後に Release 構成の `NeNeNib` target だけを build し（tidy はそのまま掛かる。tidy を外す option は作らない）、`python eng/measure-speed.py --check`。施主の実機では基準値との比較で落ちる。**基準値を緩める（中央値を上げる・許容退行を広げる）のは ADR**（QLT-014）。
   許容退行は 25%・絶対値の下限 2 ms（施主の実機の 2 回 × 5 回のばらつきを見て確定。中央値どうしの差は最大 5%、5 回の幅は ±7% に収まり、1 ms 未満の項目は下限 2 ms が守る）
6. **描画は `WM_PAINT` で 1 回**: 意図を適用したら `InvalidateRect(nullptr, FALSE)` だけ行い、`WM_PAINT` で `controller_.frame()` を描いて `Present`。Windows は入力メッセージが残っている間 `WM_PAINT` を出さないので、
   まとめて来た入力は 1 フレームで描ける。1 打鍵のときは入力の直後に `WM_PAINT` が来るので遅延は変わらない（ベンチ②で確かめる）。`WM_SIZE` / `WM_DPICHANGED` / 外観の変更も同じ経路
   （`WM_DPICHANGED` の後に再描画していなかった穴もここで塞がる）。`WM_PAINT` では `BeginPaint` / `EndPaint` ではなく `ValidateRect` を使う（device lost で `present` → `abandon` → `DestroyWindow` が `BeginPaint` と `EndPaint` の間で走るのを避ける）
7. **計測の窓の駆動は `eng/verify-window.py` と同じ道具**（起動・`PostMessageW`・窓の検出）を共有モジュール `eng/window_driver.py` に出して使う。第 2 の駆動器を書かない（ARC-001 / ARC-012）。
   反例（QLT-014）は `--check --reference <path> --values <path>` の経路で「基準値の複製を 1 本だけ厳しくすると終了 1」を示し、証明用ツリーで exe も窓も要らない
8. **測るのは `Present` が返るまで**で、実際のスキャンアウト（光るまで）ではない。waitable swap chain（最大遅延 1）なので、画面に出るのは最大 1 vsync 後。ETW / PresentMon は使わない

## 強制

- QLT-014: **施主の実機（指紋の一致する機械）で active**。`check.ps1` の `measure-speed.py --check` が基準値との比較で落ちる。反例は「基準値の複製を 1 本だけ厳しくして `--check` が終了 1」。**CI は [ADR 0016](0016-ci-speed-reference-per-host-fingerprint.md) で指紋 1 つ（`e7a87d5b6ac1e14b`・AMD EPYC 7763）が active**（他の指紋の host は記録だけで、そのことを 1 行で言う。打鍵 2 本は floor 2 ms に飲まれて判定にならない）
- ARC-007 / CPP-013: `src/core` / `src/application` から `QueryPerformanceCounter` / `GetProcessTimes` / `_Xtime_get_ticks` が出ないことを `eng/symbols.py` が見る — **active**（既存）
- CPP-002: `Milestone` の `switch` に `default` を書かない — **active**（既存の clang-tidy）
- QLT-013: 実機の値は `docs/quality/speed-reference.md` に環境つきで記録する — **active**（記録の場所として）

## 結果

- 得られるもの: いまの速さの数字。退行が施主の実機で機械的に見える。まとめて来た入力が 1 フレームで描ける。Vim・IME・シンタックスハイライトの縦切りが「遅くなったか」を測れる
- 失うもの: CI での退行判定はまだ無い（記録だけ）。ベンチと Release の build はローカルのゲートを約 1〜2 分延ばす（`--check` だけで約 65 秒: 4 ベンチ × 5 回の起動）。③は 16 MiB であって 1 GB ではない
- 正直に記録しておくこと: 計測は `Present` が返るまでで光るまでではない。5 回の中央値は同じ機械でも負荷で揺れるので、許容退行はばらつきを見て決める。
  CI の Windows ランナー（AMD EPYC 7763 / Hyper-V Video＝WARP / 96 DPI）でも窓が作れてベンチが記録された（PR #18・run 35002057290: 起動 20.4 ms・1 打鍵 1.344 ms・200 打鍵 2.700 ms・16 MiB 70.1 ms）。
  実機との差は起動と 16 MiB のどちらも約 170 ms で、実機の最初のフレームには GPU / DWM 側の固定費が乗っている疑い（Issue #19 で内訳を測る）。
  施主の実機の最初の値（Release・2026-09-16・`docs/quality/speed-reference.md`）: 起動 → 最初の描画 191.5 ms、1 打鍵 0.906 ms、200 打鍵 2.695 ms、16 MiB を開く 249.8 ms。
  `WM_PAINT` への集約で 200 打鍵は Debug 計測で 6631 ms → 29.9 ms（1 打鍵は 3.53 → 3.55 ms で変わらない）。
  Debug（ASan / UBSan）の exe で測ると 16 MiB が 4.2 秒になる。最初の実装は Debug を測っていて、設計レビューで Release に直した。
  Release で測ると計測器の 3 つの癖が出た: 隠れた窓は合成器が提示を間引く（前景にして測る）、最初の 1 枚は交換鎖の暖機で遅い（暖機の 1 打鍵を捨てる）、
  Python の `PostMessageW` より editor がキューを空にするのが速く「まとめて post」が成立しない（窓のスレッドを止めてから post し、節目の到着幅が 50 ms を越えた試行は測り直す）。製品コードは計測のために変えていない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 窓が `QueryPerformanceCounter` を直接読む | ARC-007 に反する。時計は adapters の区画にだけ在る |
| 外部から画面の変化を撮って測る（`verify-window.py` の撮影） | 50 ms 刻みの粗さで、キー → 画面の ms を測れない。節目を打つほうが正確 |
| ETW / PresentMon で光るまでを測る | 外部の道具に依存し、CI に置けない。`Present` までで退行は十分見える |
| CI で絶対値の目標を課す | ADR 0006 が却下済み。共有ランナーの揺れで偽の失敗が出る |
| CI の基準値をこの縦切りで決める | 値が 1 回しか無い。数回たまってから決める |
| 1 打鍵ごとに `Present` する今の形を保つ | 200 打鍵が 6 秒。`WM_PAINT` のまとめで 1 フレームに収まる |
| タイマーで描画をまとめる（例: 8 ms） | 1 打鍵の遅延がタイマー分だけ悪くなる。`WM_PAINT` の優先度で足りる |
| 基準値を 1 つにして機械を問わない | 実機と CI で数倍違う。指紋ごとに持つ |

## 関連

- [ADR 0006](0006-speed-gate-simd-and-table-driven-dispatch.md)・[ADR 0004](0004-ui-thread-plus-one-worker.md)・[ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)
- SPECIFICATION 第 3 節・FR-015・QLT-014
