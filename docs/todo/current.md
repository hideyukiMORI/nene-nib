# いまのタスク — NeNe Nib

2026-10-13。Issueが正本。経緯は[日報](../reports/2026-10-13.md)、根拠は[gate-proofs](../quality/gate-proofs.md)、現在の区切りは[引き継ぎ](../handoffs/2026-10-13.md)を参照する。

## 運用

- 既定はサナ単体。10月13日夜のhide指定により、今回のNeNe Nib作業はAstraの設計サナが判断し、SOLの実装サナへ実装・調査・検証を分担する。他案件へ広げない。対象と直接の境界を検証し、関連入力が不変の成功結果を再利用する（QLT-001/012）。
- CIはマージ直前だけ。README・文書のみならマージ時もCI不要で、差分と必要な目視で確認する（#430・ADR 0118）。
- 10月13日夜に作業を再開。実機画面・性能の時間帯調整は未完了だが、独立した設計・小さい契約試験を止めない。追加worktree/出力はD:/NeNeNib配下、完了後に恒久収載・監査整理。branch/commitを保持する。

## いまのIssueと次の順

| 順 | 対象 | 状態 |
|---|---|---|
| 受理 | #446 / PR447 改行索引 | [29,162 checksと実機4条件](../quality/newline-index-2026-10-12.md)。入力/長行/openは基準内。連打は変更前も基準超過のため#460へ分離。索引payloadは2GiB時に計算上288MiB、実容量試験は後続 |
| 測定待ち | #452 / PR453 分割UTF-8検証 | draft `7d8cf09`。拒否時点の18契約を追加しUTF-8 13,939 checks成功、file-text 307は再利用。3案目のRelease probeと固定planを準備済み。性能/GUI openは未実行、main未統合 |
| 技術受理 | #463 宣言サイズまでの読込 | [ADR0124](../adr/0124-file-read-completes-the-declared-size.md)。既存readの短い成功を続行し、不足時は失敗にする。[追加40を含む139契約と境界検査](../quality/complete-file-read-2026-10-13.md)成功。統合/恒久収載/整理は `D:/NeNeNib/evidence/complete-file-read-463/closeout.json` |
| 限定実験 | #465 退避資源の封印と解放 | [ADR0125](../adr/0125-source-resource-lifetime-is-probed-before-product-ports.md)に基づく同期資源の228契約成功。[実結果と限界](../quality/source-resource-2026-10-13.md)。実験コードは専用branchへ保存し、mainにはADR/結果だけ収載。OS取消・製品API・2GiBは後続 |
| 設計 | #450 元本文の所有と読込失敗 | [段階1Aの設計案](../design/2026-10-12-large-file-source-ownership.md)。単位1の読込完了境界#463はPR464/main957d74bへ統合済み。単位2の同期資源寿命を#465で確認。次は個別I/O取消/回収と予算、正典UTF-8/索引の結合、全caller移行の契約。2GiB受理前 |
| 設計 | #454 設定モーダルと日英表示 | [操作設計案](../design/2026-10-12-settings-dialog-and-language.md)を統合済み。6項目、保存/取消/失敗、日英化する表示と実装順を具体化。画面/schema案は#456へ進み、状態/IME/全翻訳の実装設計・採用は後続 |
| 設計 | #456 設定画面と版付き言語設定 | [日英・保存失敗・最小窓の画面案](../design/2026-10-12-settings-dialog-boards.md)と[ADR 0123案](../adr/0123-ui-language-is-a-versioned-setting.md)を具体化。settings.v4は提案のみ。画像は資料の描画で製品試験ではなく、見た目の採用・実装は未了 |
| 次回候補 | 日英の全表示文言 | 操作名・案内・エラーの全対訳を整理する案。22:54の会話で提案した段階で未着手。試験の再開指示ではない |
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

64 MiB超、split、Markdownは未対応。Vimのgj/gk/g0/g$とVimドラッグも未対応。既存のVim桁差は#411、外部IME overlay差は#365、既存#373は未解決。16MiB連打の既存速度基準超過は#460。旧splitの総2ms判定holdをこのベータで変更しない。

## 段階

D44の初回ベータに続き、ファイルドロップと125テーマを含むbeta.3を公開済み。配布物はD:/NeNeNib/releases/v0.1.0-beta.3/、公開/整理結果はD:/NeNeNib/evidence/beta3-release-432/closeout.json。2026-10-12のhideの開始指示により段階0を記録し、段階1Aの保存の土台まで完了。元本文の所有・索引・読込と2GiB試作は次の区切り。構文色の優先対象はPython・JavaScript/TypeScript・JSON・Markdown・C/C++。

#423は紹介用の実機PNG3枚とREADME掲載。[撮影条件/個人情報確認](../quality/showcase-2026-10-12.md)。撮影中の保存待ちは#424へ分離し、この作業で修正しない。
