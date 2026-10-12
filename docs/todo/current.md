# いまのタスク — NeNe Nib

2026-10-12。Issueが正本。経緯は[日報](../reports/2026-10-12.md)、根拠は[gate-proofs](../quality/gate-proofs.md)、現在の区切りは[引き継ぎ](../handoffs/2026-10-12.md)を参照する。

## 運用

- サナ単体で進める。変更した機能と直接の境界を検証し、関連入力が不変の成功結果を再利用する（QLT-001/012）。
- CIはマージ直前だけ。README・文書のみならマージ時もCI不要で、差分と必要な目視で確認する（#430・ADR 0118）。
- 実機画面・性能はhideの他案件と時間帯を調整する。2026-10-12 20:47の指示で試験は保留し、独立した設計を進める。追加worktree/出力はD:/NeNeNib配下、完了後に恒久収載・監査整理。branch/commitを保持する。

## いまのIssueと次の順

| 順 | 対象 | 状態 |
|---|---|---|
| 保留 | #446 / PR447 改行索引 | 対象契約29,162 checks成功、実機性能の有効な集計が得られずdraft維持。試験はhide指示で保留。原ログとReleaseをD側に保持し、同じ契約試験は再実行しない |
| 保留 | #452 / PR453 分割UTF-8検証 | 実装・対象試験コードをdraftに保存。整形と差分確認のみ、ビルド・試験は未実施。main未統合 |
| 設計 | #450 元本文の所有と読込失敗 | [段階1Aの設計案](../design/2026-10-12-large-file-source-ownership.md)。退避の寿命、範囲取得、失敗時保持、保存版と実装順を整理。API採用・実装・試験は未了 |
| 設計 | #454 設定モーダルと日英表示 | [操作設計案](../design/2026-10-12-settings-dialog-and-language.md)。6項目、保存/取消/失敗、日英化する表示と実装順を具体化。画面選択・ADR・実装は後続。アプリ試験・ビルド・CIなしで進める文書作業 |
| 設計 | #456 設定画面と版付き言語設定 | [日英・保存失敗・最小窓の画面案](../design/2026-10-12-settings-dialog-boards.md)と[ADR 0123案](../adr/0123-ui-language-is-a-versioned-setting.md)を具体化。settings.v4は提案のみ。画像は資料の描画で製品試験ではなく、見た目の採用・実装は未了 |
| 1 | #410 / PR412 折り返し・通常ドラッグ | PR412/main9afca7fへ統合済み。対象契約/実IME/画面と、修正後のnowrap/wrap性能が成立 |
| 2 | #413 ベータ配布 | PR414/main6e1043f、[v0.1.0-beta.1](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.1)を公開。ZIP/起動/再取得照合成功。収載/整理は恒久先closeout.jsonを参照 |
| 完了 | #419 配色95種と行番号欄 | 対象検証/実画面/確認用ZIPが成立。[設計と一覧](../design/2026-10-12-colorschemes.md)、[検証](../quality/colorschemes-2026-10-12.md)。統合/整理の実状態は恒久先closeout.json |
| 完了 | #440 正式リリースの範囲と試験計画 | [段階0・事前計画](../plans/2026-10-12-release-scope-and-large-file-probe.md)。2GiBを目安に本文/I/Oの試作A→応答性B。以後は[ロードマップ](../plans/2026-10-11-release-roadmap.md)の大容量→日常編集→split→Markdown→公開 |
| 完了 | #442 UTF-8保存の断片化 | 段階1Aの最初の土台。本文断片を既存FilePortの同じ一時ファイル/置換経路へ接続。[696 checksと境界](../quality/chunked-save-2026-10-12.md)。次は元本文の所有・行索引・読込。2GiB実測・上限変更・非同期化は後続 |
| 完了 | #443 splitの新条件 | D45・[ADR 0120](../adr/0120-split-acceptance-is-relative-and-tail-bounded.md)。中央値の追加`max(1ms, 単画面の25%)`以内かつp95 16.7ms以内をhideが採用。同一/別文書を分けた新試験は未実施、旧holdを維持 |
| 完了 | #416 / PR417 ファイルのドロップとbeta.2 | main8da81e3へ受理、[beta.2](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.2)を公開/再取得照合済み。[公開記録](../quality/beta2-release-2026-10-12.md) |
| 完了 | #432 125テーマ入りbeta.3 | [beta.3](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.3)を公開/起動/再取得照合済み。[公開記録](../quality/beta3-release-2026-10-12.md) |
| 完了 | #434 本文の左余白 | 既定8 DIP、文字サイズ/DPI追従。対象契約と実画面が成立。[検証](../quality/body-padding-2026-10-12.md)。公開beta.3への反映は別 |

## いまの数字と保存形式

- mainと公開beta.3の組み込み配色は125種（#421 / PR422統合済み）。行番号背景を本文と区別。利用者theme v1の任意キー`ui.gutter_background`は省略時に本文背景。構文色は準備済み、解析は未実装。
- Vimの既存oracle 1853件を保持し、折り返しviewportの78件を追加。今回の対象scopeは`--wrapped-view`と`--vim-wrapped`。
- 設定はsettings.v3、セッションはsession.v2。新形式が無いときだけ旧版を読み、旧版を保存時も残す。
- ファイルのドロップは通常/Vim共通。新しい対象scopeは`nib_window_tests --file-drop`の15 checks。実窓通知経路は確認済み、Explorerの物理ジェスチャは操作ツール制約で未測。
- 折り返しは既定オン。Ex/共通Ctrl+P設定の`set wrap` / `set nowrap`で切り替える。
- 設定モーダル・日英表示切り替えは未実装。段階3でコマンド知識なしの設定操作を加える。
- 縦のホイール・ページ移動と、折り返しなしのカーソル追従横スクロールは対応済み。手動の横スクロール・スクロールバー・選択テキストのドラッグ＆ドロップは未実装。
- 性能は[5-eeと全記録](../quality/gate-proofs.md#5-ee--折り返しと通常モードのドラッグissue-410adr-0114)。機械と表示条件を跨ぐ値を同等の高速化率として扱わない。

## 動かないものと残る制限

64 MiB超、split、Markdownは未対応。Vimのgj/gk/g0/g$とVimドラッグも未対応。既存のVim桁差は#411、外部IME overlay差は#365、既存#373は未解決。旧splitの総2ms判定holdをこのベータで変更しない。

## 段階

D44の初回ベータに続き、ファイルドロップと125テーマを含むbeta.3を公開済み。配布物はD:/NeNeNib/releases/v0.1.0-beta.3/、公開/整理結果はD:/NeNeNib/evidence/beta3-release-432/closeout.json。2026-10-12のhideの開始指示により段階0を記録し、段階1Aの保存の土台まで完了。元本文の所有・索引・読込と2GiB試作は次の区切り。構文色の優先対象はPython・JavaScript/TypeScript・JSON・Markdown・C/C++。

#423は紹介用の実機PNG3枚とREADME掲載。[撮影条件/個人情報確認](../quality/showcase-2026-10-12.md)。撮影中の保存待ちは#424へ分離し、この作業で修正しない。
