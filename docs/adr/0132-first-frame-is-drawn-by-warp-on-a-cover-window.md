# ADR 0132 — 起動の最初の frame は WARP で覆いの子窓に描き、本物の device が出来たら本窓に差し替える

- 状態: 提案（設計席 2026-10-15。実装と性能の受理は #485 で）
- 日付: 2026-10-15
- Issue: #485
- 規則: ARC-001 / ARC-003 / ARC-007 / ARC-012、CPP-002 / CPP-009 / CPP-013 / CPP-017、QLT-010 / QLT-013 / QLT-014、ADR 0004 / 0007 / 0013 / 0062 / 0131

## 文脈

ADR 0131 の後も起動の最初の frame は約 178 ms で、そのうち約 150 ms は NVIDIA の `D3D11CreateDevice`（ドライバの初期化）を待つ時間。16 MiB を開くのも同じ約 174 ms。ADR 0013 は「WARP を既定にする」を描画がソフトウェアになる理由で却下し、「段階的に描く」を描画の経路が 2 本になる理由で却下した。

実験ブランチ `experiment/warp-first-frame`（2026-10-15）の実測:

- WARP の device は約 9 ms で作れる（HARDWARE は約 150 ms・2 回目は 1.4 ms）。HARDWARE の生成中に始めた WARP の生成はその終わりまで待たされる（python で再現: 単独 9.7 ms、HARDWARE の 20 ms 後に始めると 137〜142 ms）。WARP を先に始めれば 9 ms で終わり、HARDWARE はほぼ遅れない。
- 最初の frame を WARP で出し、HARDWARE が出来た時点（約 170 ms）で差し替えると startup-first-frame 177.65 → 40.6 ms、open-large-file-16mib 174.2 → 52.1 ms。
- 本窓の HWND に WARP の swap chain を一度でも結ぶと、差し替えた後も単打が 0.63 → 1.25 ms に悪化したまま残る（frame の内訳: Present +0.47 ms・D2D の描画 +0.14 ms、controller と latency の待ちは同じ）。作り直し（device lost と同じ形）だけを強制しても 0.627 ms で無害。process の中に WARP の物は残っていないので、HWND に最初に結んだ swap chain がソフトウェアの adapter だったことを DWM / DXGI 側が覚えている形と見る。
- WARP の frame を「本窓の client を覆う子窓」に出し、本窓には HARDWARE だけを結ぶと、単打 0.638 ms（ADR 0131 と同じ）のまま、起動 41.3 ms・open 53.9 ms。差し替え後の絵は本窓だけで出したものと 0 画素差。WARP の絵と HARDWARE の絵は 768×438 のうち 286 画素（字の縁）が違い、目では見えない。

## 決定

1. **箱 `DeviceWarmup` は 2 段で渡す。** worker の仕事 `make()` は、まず WARP の device を作って event A を立て、次に HARDWARE の device（失敗なら 2 つ目の WARP）と DXGI factory を作って event B を立てる。`take_provisional()` は A を待って WARP を 1 回だけ渡し、`take()` は B を待って本物を 1 回だけ渡す（どちらも 2 回目は空）。stop が立っていればどちらも作らず event だけ立てる。WARP を先に作るのは、HARDWARE の生成中に始めた WARP が待たされるため。`ready()` は B を timeout 0 で見る（待たない）。
2. **最初の renderer は WARP で、本窓の client を覆う子窓に結ぶ。** `EditorWindow::start_rendering()` は、箱が B をまだ立てていなければ（`ready()` が偽）、子窓 `CoverWindow`（`WS_CHILD | WS_VISIBLE`・`WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT`・独自の class・`WM_NCHITTEST` は `HTTRANSPARENT`・`WM_PAINT` は `ValidateRect` だけ・本窓の client と同じ大きさ）を作り、`take_provisional()` の WARP で swap chain・DirectComposition の target・D2D の context を**子窓の HWND** に作って、最初の frame をそこへ出す。本窓の HWND に WARP の swap chain や DComp の target を結ばない（上の実測の理由）。B が立っていれば（device lost の作り直し・遅い起動）、今日どおり本窓に本物で作る。
3. **差し替えは、本窓に本物の renderer を作って同じ frame を Present・Commit してから、子窓を壊す。** `EditorWindow::adopt_device()` は、renderer が provisional のときだけ、`take()` の device で本窓に ADR 0131 の経路どおり renderer を作り（swap chain → DComp → D2D → text formats）、`draw_frame()` で同じ frame を出した後に、WARP の renderer を解放して子窓を `DestroyWindow` する。節目 `device_adopted` を足し、起動の正典順（`STARTUP_MILESTONES`）には入れない（`input_received` と同じく区間の外）。`device_created` は UI が最初の device（WARP）を手にした位置のまま。
4. **合図は `pump_messages` が event B を待つ。** `src/app/Main.cpp` の `pump_messages` は、窓の renderer が provisional の間だけ `MsgWaitForMultipleObjectsEx`（event B と `QS_ALLINPUT`）で待ち、B が立てば `window.adopt_device()` を呼んでから届いていたメッセージを `PeekMessageW` で処理する。provisional でなくなれば今日の `GetMessageW` のループに戻り、二度と入らない。スレッド・タイマー・窓メッセージの番号を足さない。
5. **子窓は差し替えの前の `WM_SIZE` と DPI の変更に追う。** `WM_SIZE` は子窓を client と同じ大きさに動かしてから renderer の `resize`、DPI は renderer の `set_dpi` を今日どおり呼ぶ。鍵は本窓に届く（焦点は本窓。子窓を作った直後に `SetFocus(本窓)` を戻す）。マウスは `HTTRANSPARENT` と `WS_EX_TRANSPARENT` で本窓へ落ちる（ADR 0013 の決定 5 のとおり本文のクリックは最初の frame の後に効く）。
6. **device lost の作り直しは古い renderer を先に解放する。** `present()` の device lost の経路は `renderer_.reset()` してから `start_rendering()`（同じ HWND に DComp の target を 2 つ作らない）。箱は空なので今日どおり自分で作る（ADR 0131 の決定 3）。
7. **差し替えの前に窓が閉じたとき・stop のとき。** 子窓は本窓と一緒に壊れ、renderer はその前に解放する。stop が立っていれば `take()` は空で、provisional の renderer は本窓に移らずそのまま終わる。
8. **受理は正式ゲート 8 本と施主の試用。** startup-first-frame と open-large-file-16mib が縮み、`key-to-frame-single` を含むほかに悪化が無いこと。基準値は変えない（縮んだ分は余裕のまま）。試用では差し替えの瞬間（起動の約 170 ms 後）に絵が揺れないことを見る。

## 却下した選択肢

- 本窓に WARP を結び、本物が出来たら swap chain だけ差し替える（変種 B）／renderer ごと作り直す（変種 A）: どちらも動くが、差し替え後の単打が 1.25 ms に悪化したまま残る（上の実測）。
- WARP を UI スレッドで窓の前に作る: 動くが、窓が見えるまでが約 2 ms 遅れる（31.9 → 34.0）。worker で WARP → HARDWARE の順に作れば UI は待たない。
- WARP を既定にする: ADR 0013 のとおり描画がソフトウェアになる。本決定は最初の 1 frame だけ。
- 差し替えを `WM_PAINT` や次の鍵まで遅らせる: 使う人が触るまで WARP のまま（単打は WARP でも 1 ms 台だが GPU を使わない）。event を待つ pump なら約 170 ms で切り替わる。
- 子窓でなく別の top-level の窓に出す: Z 順・活性化・タスクバーに影響する。子窓は本窓の client の中だけ。

## 検証

- 窓なしの契約: 箱の 2 段（A の後に B・stop・2 回目は空）を `--device-warmup` に足す。`--background-work` と `verify_quiet_paths` の port の回数は変わらない。
- 窓ありの契約（`nib_window_tests`）: 起動の順（`device_created` → `frame_presented` → `device_adopted`）と、差し替えの後に鍵が本窓で描かれること。
- symbols（core / application は不変）・conformance・format。
- 速さ: 正式ゲート 8 本（`key-to-frame-single` が ADR 0131 と同等であること）。差し替え前後の PNG の差。

## 限界

最初の frame は WARP の絵で、HARDWARE の絵と字の縁が数百画素ぶん違う。差し替えの瞬間に目で見える差は実測で無いが、試用で確かめる。
差し替えまでの約 170 ms の間の鍵は WARP で描く（単打は約 1 ms）。子窓は差し替えまでしか無く、通常の経路（resize・DPI・device lost・終了）は差し替え後は今日のまま。
GPU が無い環境では 2 つ目の WARP を本窓に結ぶので、今日と同じソフトウェア描画になる（最初の frame だけ子窓に出る）。
HWND に最初に結んだ swap chain の種類が後の Present の経路を決める、という DWM / DXGI 側の挙動は実測に基づく推定で、文書の根拠は無い。
