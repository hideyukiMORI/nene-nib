# 起動の最初の frame は WARP で覆いの子窓に描き、本物の device が出来たら本窓に差し替える — Issue #485（ADR 0132）

## 変更と確認する退行

`DeviceWarmup` は worker の仕事で WARP → HARDWARE の順に device を作り、event A / B で 2 段に渡す。`EditorWindow::start_rendering()` は B がまだなら本窓の client を覆う子窓 `CoverWindow` に WARP の renderer を結んで最初の frame を出し、`pump_messages`（`MsgWaitForMultipleObjectsEx`）が B を見て `adopt_device()` → 本窓に本物の renderer → 同じ frame を Present・Commit → `WaitForCommitCompletion` → `DwmFlush` → 節目 `device_adopted` → 仮の renderer を解放 → 子窓を壊す。本窓の HWND に WARP の swap chain を一度も結ばない（結ぶと差し替え後も単打が 1.25 ms のまま残る・実験 `experiment/warp-first-frame` 4d2aa02 の D1〜D3）。

確認する退行: 差し替え後の単打（本窓の Present の経路）、差し替えの瞬間に Mica や黒が見えないこと、鍵とマウスが本窓に届くこと、device lost の作り直し、箱の 2 段の順（書いてから立てる・待ってから読む）、起動 3 本以外の速さ。

## 対象検証（worktree `D:/NeNeNib/worktrees/485-warp-first-frame`・実装 bdc2152 → レビュー反映 598bff5 → 標本点 3560e5e → `DwmFlush` e870d0f）

| 対象 | 退行 | コマンド・結果 |
|---|---|---|
| ui / app の Debug | 型・複雑度・未定義動作 | `cmake --build build --target NeNeNib nib_tests nib_window_tests --parallel 8`（clang-cl 19.1.5・tidy・ASan/UBSan）成功（各 commit） |
| 箱の 2 段 | A の後に B・stop は両方空・2 回目は空・WARP の adapter の判定 | `build\nib_window_tests.exe --device-warmup` 13 checks（全体 3,264） |
| application・裏の仕事 | 意図と port の回数が不変 | `build\nib_tests.exe --application` 16,021、`--background-work` 89、既定 1,173,768 checks |
| 境界・規約・整形 | 外部依存・規約逸脱・空白 | `eng/symbols.py --require core application` 0、`eng/conformance.py` 0、`clang-format --dry-run --Werror` 0、`git diff --check` 0、`py_compile` OK |

独立レビュー（`out/reports/review-485.md`・bdc2152）: コードの不具合なし。R1（ADR の検証の節が窓ありの契約を実在より強く書く・ADR 0001）と R2（`verify-window.py` の検査が差し替えを assert しない・QLT-013）は 598bff5 で反映、I1（子窓を壊す前に DWM の合成を待つ）は設計として受けて `WaitForCommitCompletion`（598bff5）と `DwmFlush`（e870d0f）を入れた。R3 / R4 / I2 は文書。

## 窓ありの検査（`eng/verify-window.py`・施主の了承のもと 2026-10-15 21:20〜21:35・窓を最前面に出す）

- `--capture D:/NeNeNib/evidence/485-keys --keys ihello<Esc>`（598bff5）: `coverGoneBeforeKeys` true・`deviceAdoptedMs` 150.0・`inputsBeforeAdoption` 0・鍵は本窓に描かれた（`after.png`）。
- first-paint の節（差し替えの標本・最初の frame の後から子窓が消えた後まで 10 ms ごと）: 3560e5e では子窓が消えた直後の 1 標本（125 ms）が (32,32,32)＝Mica のダークの地で、`WaitForCommitCompletion` だけでは本窓の最初の frame が DWM に取り込まれる前に子窓が消えていた。判定も各 channel の差 2 以内に締めた。e870d0f（`DwmFlush` 追加）で 3 回: 標本 10 / 12 / 11、全標本 (48,10,36)、空の面 0、子窓は 110 / 125 / 94 ms で消えて `coverGoneAfterAdoption` true、窓まで 51.3 / 35.2 / 35.0 ms、最初の frame はその 8.1 / 7.7 / 8.9 ms 後。ログ `D:/NeNeNib/outputs/perf-day-20261015/verify-485-first-paint*.log`、呼び出し `D:/NeNeNib/scripts/verify-485-first-paint.py`（first-paint の節だけを呼ぶ）。
- `verify()` の look の節は client の中央が空の文書の案内（#304）にかかって落ちる。#481 の exe でも同じ値で落ちる既存の失敗で、Issue #486 に分けた（QLT-012）。

## 正式な速さのゲート（QLT-014・e870d0f の Release・負荷 2〜5%・ビルドなし）

`python eng/measure-speed.py --check --executable build/release-e870d0f/NeNeNib.exe --repetitions 5` を 1 回（598bff5 でも 1 回・同等）。

| bench | #481 f6843aa | #485 e870d0f | 基準（上限） |
|---|---:|---:|---:|
| startup-first-frame | 177.65 | **40.513** | 191.488（239.4） |
| startup-window-shown | 31.887 | 33.234 | 34.933（43.7） |
| key-to-frame-single | 0.636 | 0.574 | 0.906（2.906） |
| key-to-frame-burst-200 | 11.206 | 10.798 | 2.695（3.369）既存超過 |
| open-large-file-16mib | 174.2 | **52.224** | 249.783（312.2） |
| key-to-frame-burst-200-16mib | 14.558 | 14.001 | 7.411（9.264）既存超過 |
| key-to-frame-palette-5000 | 1.878 | 1.935 | 2.630（4.630） |
| key-to-frame-single-long-line | 6.226 | 5.085 | 5.431（7.431） |

連打 2 本の超過は base（#481）に #474 が無いための既存の超過（main 89c582d も 11.266 / 14.661）で、#474 を重ねた版では 1.9 / 1.8 ms（`gate-stack-af9ea72-note.md`）。基準値・許容は変えない。breakdown: `device_created` 1.0 ms（WARP）、`frame_presented` 3.1 ms。JSON `D:/NeNeNib/outputs/perf-day-20261015/gate-485-e870d0f-speed.json`。

## 再利用と限界

最初の frame は WARP の絵で、本物の絵と字の縁が数百画素ぶん違う（目では見えない・実験で 286 画素）。差し替えまでの約 150 ms の鍵は WARP で描く。`key-to-frame-single` が本物の device で測れているのは measure-speed の待ち（1.1 s）が差し替えより長いため。窓ありの検査は `check.ps1` と CI に入らず、施主の了承のもとで手で回す。成功後は実装・テスト・関連依存が同一なら再実行しない。Waivers: none。
