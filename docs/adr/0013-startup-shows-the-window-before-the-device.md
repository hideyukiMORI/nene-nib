# ADR 0013 — 起動は窓を先に見せてから D3D の device を作り、「窓が見えるまで」を第 2 の起動の値として基準値に載せる

- 状態: 受理
- 日付: 2026-09-17
- Issue: #24
- 影響する規則: ARC-004 / ARC-007 / ARC-011 / CPP-002 / CPP-017 / QLT-013 / QLT-014

## 文脈

Issue #19 で起動の内訳を測った: 実機（RTX 3090 / 120 DPI・Release）の起動 184 ms のうち **`D3D11CreateDevice(D3D_DRIVER_TYPE_HARDWARE)` が 159.6 ms（87%）**。CI（WARP）は同じ区間が 3.6 ms。
Issue #24 の段 1 で呼び方を変える 4 実験を測った（`docs/quality/speed-reference.md`「`D3D11CreateDevice` の実験」）: 機能レベルの限定 +2.8 ms・adapter の明示 +5.6 ms・`SINGLETHREADED` -0.8 ms・連続 2 回目 +0.5 ms。
どれも対照の 5 回の幅の中で、**30 ms 以上縮む実験は無い**。WARP を強制すると 9.5 ms になるので、159 ms はまるごと NVIDIA のユーザーモードドライバ（`nvwgf2umx.dll` 86 MiB ほか）の初期化であり、
連続 2 回でも動かないので OS のファイルキャッシュも効かない＝**プロセスごとに毎回払う固定費**。呼び方を変える道は閉じている。

いまの `EditorWindow::initialize` は 窓の生成 → Mica → **device / swap chain / DirectComposition / D2D / DirectWrite → 最初の `Present`** → `ShowWindow` の順で、利用者は 184 ms のあいだ何も見ない。
施主の優先順位は速さが 1 番で、「起動が速い」と利用者が感じるのは窓が出るまでの時間である。

## 決定

**`ShowWindow` を device の生成より前に出す。Mica の背景だけの窓が約 25 ms で見え、本文の最初のフレームは device ができてから（約 185 ms）描く。
`startup-first-frame`（プロセス生成 → 最初の `frame_presented`）の定義と基準値は変えず、`window_shown` の節目を足して「プロセス生成 → 窓が見えるまで」を第 5 のベンチ `startup-window-shown` として基準値に載せる。**

1. **順番**: `RegisterClassExW` → `CreateWindowExW` → `place_at_screen_centre` → `apply_backdrop`（ダーク・Mica）→ **`ShowWindow(SW_SHOW)` → `timing_.mark(window_shown)`** → `start_rendering`（device → swap chain → DirectComposition → D2D → DirectWrite → 最初の `draw_frame`）→ `update_title` → `announce`。
   `WS_EX_NOREDIRECTIONBITMAP` の窓は再描画面を持たず、DirectComposition の内容が来るまでクライアント領域は DWM の Mica の背景だけが見える（実機で確かめる。**黒や白が見えるなら、この決定は却下して案 C（受け入れる）に戻す**。判定は下の「強制」）
2. **`core::Milestone` に `window_shown`** を足す（起動の正典順で `backdrop_applied` の次・`device_created` の前）。`eng/measure-speed.py` の `STARTUP_MILESTONES` と `NibTests.cpp` の表に同じ綴りで足す。閉じた enum なので写し先が増えたらコンパイルが落ちる（CPP-002）
3. **第 5 のベンチ `startup-window-shown`**: プロセス生成 → 最初の `window_shown`（`processCreationToOriginMs` ＋ `window_shown` の `qpcMicroseconds`）。`BENCHES` に足し、`compare` が既存と同じ許容（25%・下限 2 ms）で見る。**基準値は施主の実機の 1 回目の値を `--adopt --bench startup-window-shown` で足す。** 既存 4 本の中央値は動かさない（`--adopt` に `--bench <name>` を足して 1 本だけ書けるようにする。全部を書き直す既存の `--adopt` はそのまま）
4. **`startup-first-frame` は変えない**（定義も基準値も）。順番を変えた結果として最初の `Present` が遅くなれば、既存の許容 25% で落ちる。これがこの決定の安全弁。速くなれば記録だけして基準値は次の ADR で
5. **renderer が無い間の窓の振る舞い**: `WM_PAINT` は `ValidateRect` して何も描かない（`present` は `renderer_ == nullptr` で返る・既存）。`WM_SIZE` は既存の門で返る。**本文のクリック（`column_at`）は renderer が無ければ捨てる**（門を足す）。鍵は意図として controller に入り、最初のフレームで描かれる。`announce`（開けなかった理由の 1 行）は最初のフレームの後（既存）
6. **見えるまでの絵**: Mica（ダークなら茄子色に透ける）の面だけ。タイトルバーのタブ・本文・ステータスバーは最初のフレームで一度に出る。段階的に描かない（中途半端な絵を 160 ms 見せるより、背景 → 完成の 2 段のほうが静か）。ライトでは Mica の明るい面

## 強制

- QLT-014: `startup-window-shown` を施主の実機の基準値との比較でゲートが落とす — **active**（施主の実機。CI は記録だけ・既存と同じ）。`startup-first-frame` の許容を守ることが「順番を変えても最初のフレームが遅くならない」の機械強制
- QLT-013: 実機で「窓が見えてから本文が出るまで」のクライアント領域が黒や白でなく Mica の面であることを `eng/verify-window.py` の節（起動直後の画素の列を取り、`window_shown` と `frame_presented` の間の画素が背景トークンの色と 1 段の差に収まる）で記録する。取れなければ `docs/quality/gate-proofs.md` 5-h に手動確認 — **active**（記録の場所として）
- CPP-002: `Milestone` の `switch`・`STARTUP_MILESTONES`・テストの表 — **active**（既存）
- **却下の条件**（この ADR を「却下 → 案 C」に書き換える）: 窓が見えてから最初のフレームまでのクライアント領域が黒か白、または `startup-first-frame` が許容を越える

## 結果

得られるもの: 窓が約 25 ms で見える（体感の起動）。159 ms のドライバ初期化はそのまま払うが、利用者はそれを「窓が出た後の本文の遅れ」として見る。`startup-window-shown` が基準値に載り、窓の生成より前に重い処理を足す退行をゲートが落とす。
失うもの: 「起動 → 最初の描画」の値そのものは縮まない。窓が出てから本文が出るまでの 160 ms の Mica だけの面を利用者が見る。速さの基準値の鍵が 5 つになる（ADR 0011 の決定 3 の「4 つ」を更新）。
正直に: この決定で速くなるのは体感だけで、`startup-first-frame` の数字は変わらない（変わったら記録する）。DWM が Mica を描くまでの時間（DWM 側の合成の遅れ）は `window_shown`（`ShowWindow` が返った直後）には入らず、本当に画素が光る時刻は測らない（ADR 0011 の決定 8 と同じ立場）。device の生成を worker に出して窓の生成やファイルの読み込みと重ねる案 B は、重ねられるのが最大 60 ms 程度で 159 ms は消えず、ADR 0004 の枠（worker はポートの実装）に device を置く形が要るので、この ADR では採らない（16 MiB の読み込みが重くなったら再検討）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `D3D11CreateDevice` の呼び方を変える（機能レベル・adapter・`SINGLETHREADED`） | Issue #24 段 1 で 4 実験とも ±6 ms（`speed-reference.md`）。費用はドライバの初期化そのもの |
| 案 B: device の生成を adapters の worker に出して窓・ファイル読み込みと重ねる | 重ねられるのは 25〜60 ms で 159 ms は消えない。renderer（ui/win32）が worker の作った device を受け取るポートが要り、ADR 0004 の「worker はポートの実装」に device という OS の資源を載せる形になる。得るものに対して形が重い |
| 案 C: 受け入れる（ドライバの固定費として記録し、起動は体感で勝負しない） | 窓を先に見せる順番の変更は既存の門（`renderer_ == nullptr`）でほぼ済み、費用が小さい。Mica だけの面が黒や白になる環境が見つかったら、この案に戻す（上の却下の条件） |
| WARP を既定にする | 9.5 ms で起動するが描画がソフトウェアになり、1 打鍵と 16 MiB の速さを捨てる。SPECIFICATION 第 9 節の「GPU が無い環境の fallback」のまま |
| `startup-first-frame` の定義を「窓が見えるまで」に変える | 基準値の意味が変わり、最初のフレームの退行が見えなくなる。第 2 の値として足す |
| 段階的に描く（タイトルバーだけ先に GDI で） | 描画の経路が 2 本になる（ARC-001 / ADR 0007 の Direct2D 1 本） |

## 関連

ADR 0004（UI スレッド＋1 本の worker）・ADR 0007（Direct2D 1 本）・ADR 0008（Mica・`WS_EX_NOREDIRECTIONBITMAP`）・ADR 0011（節目・ベンチ・基準値・`Present` まで測る）・Issue #19 / #24・`docs/quality/speed-reference.md`。
