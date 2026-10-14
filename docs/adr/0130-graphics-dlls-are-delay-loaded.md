# ADR 0130 — 描画と文字組みの DLL は遅延読み込みにする

- 状態: 提案（設計席 2026-10-15。実装と性能の受理は #479 で）
- 日付: 2026-10-15
- Issue: #479
- 規則: ARC-007、QLT-010 / QLT-013 / QLT-014、ADR 0003 / 0013

## 文脈

窓は device の生成より先に見せる（ADR 0013）。しかし exe は d3d11 / d2d1 / dcomp / dxgi / DWrite を静的に import するので、loader は窓を見せる前にこの 5 つを読み込んで初期化する。実験ブランチ `experiment/startup-probe`（15c5f2c）で 5 つを `/DELAYLOAD:` にすると、process 生成から timing の bind までの origin が 15.2 → 13.5 ms、startup-window-shown が 30.1 → 26.6 ms に縮み、startup-first-frame は揺れの範囲（190.0 → 186.6 ms）だった。同じ実験で ShowWindow の約 9 ms は WM_ACTIVATE の既定処理（IME の文脈作成）で製品側では縮まず、Mica の属性の順序・`WS_EX_NOREDIRECTIONBITMAP`・dark mode は要因でないことも確かめた。

## 決定

1. **`NeNeNib` target だけに `/DELAYLOAD:` を付ける。** 対象は `d3d11.dll` `d2d1.dll` `dcomp.dll` `dxgi.dll` `DWrite.dll` の 5 つで、`delayimp` を link する。置き場は `CMakeLists.txt` の `NeNeNib` target の 1 か所（`target_link_options` と `target_link_libraries`）。警告集合（`eng/targets.cmake`）は触らない。試験の target と probe には付けない（計測の対象は製品の起動だけ）。
2. **`ole32` `imm32` `dwmapi` `shell32` `advapi32` は遅延にしない。** 起動の道（CoInitializeEx・Mica の属性・IME の文脈）で使う。
3. **失敗の時点は最初の呼び出しに移る。** 5 つは Windows 10/11 の inbox DLL で、無い環境は製品の前提の外。遅延読み込みの失敗は delayimp の既定（構造化例外）に任せ、製品にフックを足さない。
4. **受理は正式ゲート。** `llvm-readobj --coff-imports` で 5 つが DelayImport にあることを記録し、8 本で startup-window-shown が縮み、ほかに悪化が無いことを示す。基準値は変えない（縮んだ分は余裕として残す）。

## 却下した選択肢

- device の生成を別スレッドで先に始める: ADR 0013 の設計変更で、スレッドは `Win32Worker` の 1 本だけ（ARC-007）。別 Issue。
- 起動で使う DLL まで遅延にする: 起動の道で呼ぶので縮まず、失敗の時点だけが遅くなる。
- `LoadLibrary` を自分で書く: 第 2 の経路。linker の仕組みで足りる。

## 検証

- Release の import を `llvm-readobj --coff-imports` で確かめる。Debug の対象 build が通ること（試験の target は変えない）。
- 正式な `eng/measure-speed.py --check` 8 本。

## 限界

縮むのは 2〜4 ms で、起動の大半（D3D11CreateDevice 約 150 ms）はそのまま。
