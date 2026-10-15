# ADR 0131 — D3D11 の device は worker で先に作り始め、窓を見せてから受け取る

- 状態: 提案（設計席 2026-10-15。実装と性能の受理は #481 で）
- 日付: 2026-10-15
- Issue: #481
- 規則: ARC-003 / ARC-007 / ARC-012、CPP-003 / CPP-009 / CPP-017、QLT-010 / QLT-013 / QLT-014、ADR 0004 / 0013 / 0062

## 文脈

起動の最初の frame は約 190 ms で、`D3D11CreateDevice`（NVIDIA）が約 150 ms を占める。ADR 0013 は窓を device より先に見せたが、device の生成は UI スレッドが窓を見せた後に始まるので、process の最初から device が要るまでの約 20〜30 ms（loader・adapters・起動引数のファイルの読み込み・窓の生成と表示）は device の生成と重なっていない。実験ブランチ `experiment/device-on-worker`（2461701）で生成を `Win32Worker` へ先に投げると、交互 5 周の中央値で startup-first-frame 194.9 → 179.9 ms、open-large-file-16mib 207.7 → 178.7 ms、startup-window-shown は揺れの範囲（35.6 → 36.2）。worker は 0.1 ms で起き、worker 上の生成は UI より 4〜5 ms 長い。

## 決定

1. **device の箱は ui/win32 の `DeviceWarmup` で、合成ルートが所有する。** `D3D11CreateDevice`（今日と同じ flags・feature levels・WARP への切り替え）と `CreateDXGIFactory2` を行う `make()` と、結果を 1 回だけ渡す `take()` を持つ。`make()` は worker の仕事の中で呼ばれ、結果（`ComPtr` 3 つと `HRESULT`）を置いてから Win32 の event を立てる。`take()` は event を待ってから読む（書いてから立てる・待ってから読むので、`<atomic>` は要らない・CNF-009）。箱は値を 1 回しか渡さず、2 回目の `take()` は空を返す。
2. **投げるのは `run()` の 1 か所、`Win32Worker` の宣言の直後（`EditorController` の構築＝起動引数の読み込みより前）。** 載せる仕事は、受け取った stop の印を `warmup.make(stop.stop_requested())` と箱へ渡すだけ（`<stop_token>` を include できるのは `src/adapters/win32` だけ・CPP-013 なので、箱は印を `bool` で受ける）。箱は worker より前に宣言し、worker より後に壊れる（仕事が箱を借りる）。stop が立っていれば `make()` は device を作らず、event だけを立てる（`take()` が待ち続けない）。
3. **renderer は箱から受け取り、無ければ今日どおり自分で作る。** `Direct2DRenderer::create_device()` の先頭で `take()` し、値があればそれを使う。device lost の作り直し（ADR 0007）は今日の経路のまま自分で作る。節目 `device_created` は UI スレッドが device を手にした位置のまま（ADR 0013 の決定 2〜4 は変えない。順番は変えず、生成を始める時点だけ前へ）。
4. **スレッドは `Win32Worker` の 1 本のまま（ADR 0004 / 0062）。** 新しいスレッド・グローバル変数・lint の除外を持ち込まない。worker の仕事が起動のたびに 1 つ増え、その間（約 150 ms）に投げられた同じフォルダの読み込みは後ろに並ぶ。Ctrl+P を起動直後 150 ms 以内に開いた場合だけ候補が遅れ、契約 `--background-work` と `verify_quiet_paths` の port の回数は変わらない。
5. **受理は正式ゲート 8 本。** startup-first-frame と open-large-file-16mib が縮み、ほかに悪化が無いこと。基準値は変えない。

## 却下した選択肢

- 専用スレッドで device を作る: スレッドは 1 本（ARC-007 / ADR 0004）。
- UI スレッドで窓の前に device を作る: ADR 0013 が却下した順（窓が見えるまでが 150 ms 遅れる）。
- device の生成そのものを速くする: ドライバの初期化で、呼び方では縮まない（ADR 0013）。
- 箱を adapters に置く: D3D / DXGI は ui/win32 の持ち物で、adapters は COM の描画を知らない。箱は同期の event を持つが、スレッドは持たない。

## 検証

- 対象: ui の窓なしの契約（renderer の生成・device lost の作り直し）、`--background-work`、adapters の worker の試験（時計を読まない・セマフォで待つ）。symbols（core / application は不変）・conformance。
- 速さ: 正式ゲート 8 本。

## 限界

縮むのは UI が device を取りに来るまでの仕事の長さの分（空の文書で約 15 ms・16 MiB で約 30 ms）。device の 150 ms そのものは残る。
worker のスレッドは最初の仕事で起きる（ADR 0062）ので、起動のたびに起きるようになる。スレッドを起こせない（`std::jthread` が投げる）極めて稀な環境では、これまで最初の Ctrl+P で起きていた失敗が起動時に起きる。`take()` は event を待つだけで時計を読まず、仕事が走らないまま worker が壊れる経路（窓が無い）では `take()` は呼ばれない。
