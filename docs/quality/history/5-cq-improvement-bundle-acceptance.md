# 5-cq — 改善一式の統合の受理（Issue #291・ADR 0075・施主決定 D41）

出典: docs/quality/gate-proofs.md の節 5-cq（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-06 夜、設計席が日報と引き継ぎから再開し、止まっていた理由を保存済みの記録から計算し直した。hide が「昨夜の試用は問題なかった」「統合の決め方はおすすめで進める」「速さの検査は準備ができ次第回してよい」と答えた（D41）。
**5-ck〜5-co の `hold` は、その実験の事前条件に対する結果としてそのまま残す。** この節は main へ入れるかを ADR 0075 の 4 つで決めた記録で、過去の判定の上書きではない。

### 揺れの条件の読み直し（新しい測定なし）

`docs/quality/render-capsule-2026-10-05.json` の `summary` の 12 指標（3 本文 × O / A × 1 打鍵 / 200 文字）を、`plan.record.stability` の式のまま計算し直した。

| 本文・版・指標 | 6 本の値 ms（測った順） | 許容 ms | 前後半の差 ms | 判定 |
| --- | --- | ---: | ---: | --- |
| 長行・O・1 打鍵 | 28.020 / 28.645 / 32.491 / 27.459 / 29.337 / 28.423 | 2.8534 | 0.222 | 条件内 |
| 長行・A・1 打鍵 | 7.633 / 8.107 / 6.906 / 6.833 / 7.990 / 5.690 | 0.72695 | 0.800 | 外れ |
| ほかの 10 指標（空文書・16 MiB 短行の O / A、長行の 200 文字） | JSON のとおり | — | 0.002〜1.808 | すべて条件内 |

A の 6 値は単調ではなく、3 本ずつに分けるどの分け方でも許容を越える（値の散らばりが約 1 ms、許容が 0.73 ms）。O は 5 ms 散っても許容 2.85 ms に収まる。条件が値に比例して縮むために、速くなった版だけが外れた。
差 21.26 ms は散らばりの 20 倍を越え、6 組すべてで短縮。別の固定実験（各 240 打鍵・`tail`）では A の観測最大 7.978 ms、O は 240 件すべてが 16.7 ms 超。
空文書と 16 MiB 短行は 8 指標すべてが条件内で、A の中央値は O 以下（0.7395 → 0.7355 / 3.2275 → 3.1135 / 1.2650 → 1.2430 / 8.6080 → 7.1165 ms）。
O は `0dfad49`（文字組みとステータスの再利用まで）で main ではない。main からの悪化が無いことは、次の正式なゲートで示す（ADR 0075 決定 4）。

### 正式な速さのゲート（QLT-014）

確認する退行: 描画の経路と無効化のまとめ方を変えたので、既存の 7 本（起動・1 打鍵・200 打鍵・16 MiB・Ctrl+P の 5000 件）が基準値から悪化していないこと。
対象: Release `f42a95d`（`D:/NeNeNib/worktrees/291-display-width-capsule-20261005/build/release-f42a95d/NeNeNib.exe`・SHA-256 `44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900` を実行の直前に照合）。
`git diff --exit-code f42a95d HEAD -- src tests eng CMakeLists.txt` と `git diff --exit-code main HEAD -- eng` はどちらも終了 0（製品は試用版と同じ・計測器と基準値は main と同じ）。
実行の直前にビルドとテストのプロセスが 0 件・CPU の負荷 8% を確かめ、hide の了承の後に 1 回だけ回した。

`python eng/measure-speed.py --check --executable <上の exe>` → **7 benches checked, 0 regression(s), 0 unmeasurable**・終了 0。記録 `out/speed/2026-10-06T14-00-58Z.json`・ログ `out/291-rina-speed-f42a95d.log`。指紋 `bc8a356f37c68491`。

| ベンチ | 基準値 ms | 今回の中央値 ms（最小〜最大） |
| --- | ---: | --- |
| startup-first-frame | 191.488 | 201.062（195.290〜245.217） |
| startup-window-shown | 34.933 | 35.306（31.309〜45.079） |
| key-to-frame-single | 0.906 | 0.642（0.492〜0.654） |
| key-to-frame-burst-200 | 2.695 | 3.120（2.714〜3.175） |
| open-large-file-16mib | 249.783 | 244.790（241.787〜257.211） |
| key-to-frame-burst-200-16mib | 7.411 | 5.580（4.315〜6.215） |
| key-to-frame-palette-5000 | 2.630 | 2.070（2.045〜2.359） |

基準値と許容（25% / 床 2 ms）は変えていない。採用（`--adopt`）もしていない。Ctrl+P の 5 枚の PNG のうち `palette-2026-10-06T14-00-27Z-31360.png` を設計席が見て、暖機の `f` と件数 **1 / 5000** を確かめた。
起動の 1 本目 245.217 ms は 5 本の最大で、中央値は許容内。原因は調べていない。

### 再利用（QLT-012）

表示 22 場面の 0 画素差・IME の変換と確定・sanitizer の契約 604 checks・字形 180 画素対・`--display-line` 1114301 checks・`--vim-virtual-column` 424 checks・`protected-diff`（fixture 1853 → 1853）は 5-cm の成功をそのまま使う。製品の入力が `f42a95d` から変わっていないので実行し直していない。

### 独立レビュー

書いた席（Codex）とは別の実装席（Opus）が、結論を渡されずに `git diff main...f42a95d -- src tests CMakeLists.txt`（18 ファイル・+663 / −21）を読んだ。読むだけで、ビルド・実行はしていない。報告は `out/reports/review-291.md`、依頼書は `D:/NeNeNib/briefs/review-291-rina-20261006.md`。

- **統合を止める: 0 件。** 保持する 4 つ（本文の layout の現在と直前・ステータスの 7 枠・字体選択の 128 件・字形）を、フォント・DPI・テーマ・device lost・幅・IME・選択と検索のそれぞれで追い、古い結果が出る道・解放済みを触る道・借用が保持より長生きする道は見つからなかった。無効化をまとめる変更で `paint_pending_` が true のまま固まる道も無い。規則 ID の違反と色のリテラルも無い。
- 統合の後で直せる 4 件: waiver の期限 2026-11-04（→ #299）・字体選択の保持の取り付けの失敗で窓が終わる・起動中のフォントの追加が表示中の行に反映されない・保持の契約の試験がリポジトリに無い（→ #300）。
- 所見 7 件のうち、字形の経路の前提（96 DPI・恒等変換・整数の原点）を機械が守っていない点は #300 に入れた。

設計席は L2 の該当箇所（`Direct2DRenderer.cpp` の `create_body_formats`）を読み、書式を作れなかったときの既存の失敗（`RenderFailure::directwrite`）と同じ扱いであること・Windows 11 専用（D1）で `IDWriteTextFormat2` が必ずあることから、統合を止めないと判断した。製品を `f42a95d` から動かすと表示・IME・速さの成功を使えなくなるので、直すのは #300 で行う。

### Debug 構成のビルドとシンボル（レビューの所見 O6）

確認する退行: 新しい翻訳単位（`BodyGlyphCollector.cpp`・`FontFallbackCache.cpp`・`FontFallbackKey.cpp`）が Debug（ASan / UBSan・clang-tidy）で通ること、core に足した索引が core の外へ許可の無いシンボルを出さないこと（ARC-003 / ARC-007）、実際のビルドグラフの依存方向（ARC-002）。5-cm は Release と `nib_tests` の 2 scope だけだった。

`. ./eng/toolchain.ps1` の後、`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build`（全 target・80 手順・clang-tidy 込み）は終了 0。新しい 3 つの翻訳単位と `Direct2DRenderer.cpp`・`EditorWindow.cpp`・`VirtualColumn.cpp` を Debug で作り直し、`NeNeNib.exe` と `nib_tests.exe` を含む全 exe がリンクした（`out/291-rina-debug-build.log`）。
`python eng/conformance.py --build-dir build` → `Conformance: 0 violation(s)`。`python eng/symbols.py --build-dir build --require core application` → `Symbols: 2 libraries checked, 0 violation(s)`。文書を足した後の `python eng/conformance.py` も 0 violation(s)。
CTest は回していない（`--display-line` と `--vim-virtual-column` は同じソースの成功を再利用・ほかの scope は差分が触れない）。

### 4 つの条件の結果

| ADR 0075 の条件 | 結果 |
| --- | --- |
| 独立レビュー | 統合を止める所見 0 件（`out/reports/review-291.md`） |
| 正式な速さのゲート | 7 benches checked・0 regression・0 unmeasurable |
| 必須 check | Ready の後の run（PR #293） |
| 施主の実機の試用 | 2026-10-06・Release `f42a95d`・「問題なかった」 |

統合の後に残す仕事: #298 長い行のベンチ・#299 SDK の固定署名の恒久規則（期限 2026-11-04）・#300 保持の契約の試験と取り付けの失敗。split の採用は保留のまま。PR #296 / #297 は #293 に含まれるので統合せずに閉じる。
適用: #291 / ADR 0075 / D41 / QLT-001 / QLT-010 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。製品コード・保存 schema・基準値・許容の変更なし。Waivers: WVR-0001 / WVR-0002（変更なし）。
