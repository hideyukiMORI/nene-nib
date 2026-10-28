# 品質向上の日（2 日目）— 2026-10-29（設計席 Fable・hide 不在）

hide の指示（朝）「これから 1 日、外回りでデスクにいないので他のセッションは Folio サナ以外は立ち上げない。自由に試験や計測を繰り返して、品質が上がるように調査対応をして欲しい。必要なら web 検索やマルチエージェント・オーケストレーションを自由に使っていい。止めるまで続けて最善を尽くして。試したことや変更したこと等は全てドキュメントとして記録を残して」。前日の同じ日は [quality-day-2026-10-28.md](quality-day-2026-10-28.md)。

設計・判断は Fable の設計席、実装・調査・レビューは Opus の背景の席（ADR 0038 / 0039・依頼書は `D:\NeNeNib\briefs\20261029\`・席の共通の約束は同じ場所の `_common.md`）。merge の裁量（hide の指示 2026-10-15）: 試験・道具・文書・製品の破れの直し・hide が「よし」と言った機能は、独立レビューと CI の後に main に入れる。速さに関わるものは D41。基準値（2026-10-18 採用）は変えない。

## 0. 朝の状態と計画

- origin/main `0703602d`（#1059 まで）。本体の checkout は 33 本遅れていて、前夜の文書 PR と同じ内容の未 commit の文書（29 ファイル・origin/main と同一）が残っていたので `git stash push -u`（捨てない）で退かせて fast-forward した（stash `20261029-duplicate-docs-identical-to-origin-main`・中身は全部 main にあるので後で drop してよい）。open の PR は #593（hold）だけ。作業木 27 本（前日までの実験のまま）。
- `D:\NeNeNib\worktrees\main-replayer` に前夜の試験の残骸 `nib-adapter-files/`（15 KiB・adapters の試験の一時ファイル）が未追跡で残り、`build-release.ps1` が dirty と見るので `D:\NeNeNib\outputs\20261029\main-replayer-nib-adapter-files-moved\` へ移した（捨てていない）。adapters の試験が作業木の直下に一時ファイルを残す件は後で確かめる（§5）。

計画（レーン）:

| レーン | 内容 | 根拠 |
|---|---|---|
| D 速さ | 分割・開く道の port・設定 v4 の後の main を **朝一番の静かな機械で** 8 本のゲート（beta.8 の前に必須・引き継ぎ） | 前日は未計測 |
| A 再現度 | Vim の差分 fuzz の続き seed 21〜30（3,000 件・`--exclude` で `''` と `` ` ` ``・`--known` 781 / 985 / 980）を調査席で回し、新規の種類を仕分ける | 前日 6,900 件で 12 種 → 全部直した。例に基づく fixture の外に差が残る |
| F 機能（hide「よし」の列） | 引き継ぎの順: L0c（LSP の配線）・E3（エクスプローラーの application）・L2b（worker で読む adapter）・Vim の定番 #1037〜#1040 を並列 8 席 | 前日の引き継ぎの再開手順 1 |
| G 設計 | D71 / D72 の ADR（検索・置換・行へ移動 / `ClockPort` / `map` / 印刷）と追補（Ctrl+ホイール・文字コードと改行・右クリック・全面プレビュー）を CI 待ちに書く | 前日の引き継ぎ「先に ADR」 |
| T 言語 | T1a（`ui_texts` の表と移し替え・CNF-016）は F の群が merge した後に 1 席（全ファイルの日本語のリテラルを触るので、同時に走らせると衝突が全部に出る） | 設計席の判断 |

## 1. 速さのゲート（朝・main `0703602d`・Release `D:/NeNeNib/worktrees/main-replayer/build/release-0703602/NeNeNib.exe`・sha256 `9111070C…`）

負荷 14 / 8 / 2%（Folio の codex は軽い）。`python eng/measure-speed.py --check --repetitions 5` → **8 benches checked, 0 regression(s), 0 unmeasurable**（ログ `D:/NeNeNib/outputs/20261029/gate-main-0703602d.log`）。

| ベンチ | 中央値 ms | 前日 `0070859c` |
|---|---|---|
| startup-first-frame | 48.669 | 41.261 |
| startup-window-shown | 36.131 | 31.813 |
| key-to-frame-single | 0.461 | 0.580 |
| key-to-frame-single-long-line | 1.017 | 1.132 |

（残り 4 本はログに。起動の 2 本は許容の内で、朝の 1 回目の計測は OS の暖まりで数 ms 揺れる。1 打鍵は分割の後も速い。）分割（#1034 #1053）・開く道の port（#1054）・設定 v4（#1055）は速さに退行なし → beta.8 の前提の 1 つが済んだ。

## 2. 席と結果（随時）

| # | レーン | 席 | 状態 |
|---|---|---|---|
| #994 L0c | F | L0c | 走行中 |
| #997 E3 | F | E3 | 走行中 |
| #993 L2b | F | L2b | 着手前に設計の穴で正しく止まった（`SealedBytes::filled` は 1 回の同期の fill で封印するので、予約を worker の塊の仕事にまたがって持ち回れない）→ 設計席が **A**（core に `BytesReservation`・塊ごとの検証は予約が抱え封印に判定を添える・ADR 0141 追補 8〜11）を決めて同じ席に送った → 再開 |
| #1037 | F | V1 | 走行中 |
| #1038 | F | V2 | 走行中 |
| #1039 | F | V3 | 走行中 |
| #1040 | F | V4 | 走行中 |
| #954 fuzz 21〜30 | A | FZ | 走行中（速さの計測の終わりを待ってから走る約束） |

## 3. 設計（CI 待ちに書いたもの）

- ADR 0174（通常モードの検索・置換・行へ移動は Ex の入力行と Vim の照合器を共用・#1046）・ADR 0175（`ClockPort`・#1050）・ADR 0176（`map` は設定 v4 の行・入口の表 1 つ・#1058）・ADR 0177（印刷は port 1 本・XPS の print control・#1047）。全部 D71 / D72 の施主決定の範囲の中で、仕様の D 番号と FR を読み直してから書いた。
- ADR 0141 の追補 8〜11（`BytesReservation`・L2b の穴）。

## 4. Vim の差分 fuzz（seed 21〜30）

（席の報告の後に書く）

## 5. 今日分かったこと・次に効くこと

- 本体の checkout に前夜の文書の写しが未 commit で残ると `build-release.ps1` が断る。`git stash push -u` は捨てないので auto mode の分類器を通る（`checkout --` と `clean` は止められる）。
- adapters の試験（`nib_adapter_tests`）が作業木の直下に `nib-adapter-files/` を残す（確かめる: どの試験か・一時フォルダに書く形に直すか）。

## 6. 置き場

`D:/NeNeNib/outputs/20261029/`（計測のログ・fuzz の結果・レビュー）、`D:/NeNeNib/briefs/20261029/`（依頼書）。
