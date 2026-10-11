# いまのタスク — NeNe Nib

2026-10-12。Issueが正本。経緯は[日報](../reports/2026-10-12.md)、根拠は[gate-proofs](../quality/gate-proofs.md)、現在の区切りは[引き継ぎ](../handoffs/2026-10-12.md)を参照する。

## 運用

- サナ単体で進める。変更した機能と直接の境界を検証し、関連入力が不変の成功結果を再利用する（QLT-001/012）。
- 実機画面・性能はhideの他案件と時間帯を調整する。追加worktree/出力はD:/NeNeNib配下、完了後に恒久収載・監査整理。branch/commitを保持する。

## いまのIssueと次の順

| 順 | 対象 | 状態 |
|---|---|---|
| 1 | #410 / PR412 折り返し・通常ドラッグ | PR412/main9afca7fへ統合済み。対象契約/実IME/画面と、修正後のnowrap/wrap性能が成立 |
| 2 | #413 ベータ配布 | PR414/main6e1043f、[v0.1.0-beta.1](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.1)を公開。ZIP/起動/再取得照合成功。収載/整理は恒久先closeout.jsonを参照 |
| 完了 | #419 配色95種と行番号欄 | 対象検証/実画面/確認用ZIPが成立。[設計と一覧](../design/2026-10-12-colorschemes.md)、[検証](../quality/colorschemes-2026-10-12.md)。統合/整理の実状態は恒久先closeout.json |
| 次 | #408 正式リリースのロードマップ | 次の開始指示待ち。範囲整理→大容量成立性→大容量→日常編集→split→Markdown→併用確認/公開 |
| 完了 | #416 / PR417 ファイルのドロップとbeta.2 | main8da81e3へ受理、[beta.2](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.2)を公開/再取得照合済み。[公開記録](../quality/beta2-release-2026-10-12.md) |

## いまの数字と保存形式

- 組み込み配色95種。行番号背景を本文と区別。利用者theme v1へ任意キー`ui.gutter_background`を追加し、省略時は本文背景。Python/TypeScriptの構文色は準備済み、解析は未実装。
- Vimの既存oracle 1853件を保持し、折り返しviewportの78件を追加。今回の対象scopeは`--wrapped-view`と`--vim-wrapped`。
- 設定はsettings.v3、セッションはsession.v2。新形式が無いときだけ旧版を読み、旧版を保存時も残す。
- ファイルのドロップは通常/Vim共通。新しい対象scopeは`nib_window_tests --file-drop`の15 checks。実窓通知経路は確認済み、Explorerの物理ジェスチャは操作ツール制約で未測。
- 折り返しは既定オン。Ex/共通Ctrl+P設定の`set wrap` / `set nowrap`で切り替える。
- 性能は[5-eeと全記録](../quality/gate-proofs.md#5-ee--折り返しと通常モードのドラッグissue-410adr-0114)。機械と表示条件を跨ぐ値を同等の高速化率として扱わない。

## 動かないものと残る制限

64 MiB超、split、Markdownは未対応。Vimのgj/gk/g0/g$とVimドラッグも未対応。既存のVim桁差は#411、外部IME overlay差は#365、既存#373は未解決。旧splitの総2ms判定holdをこのベータで変更しない。

## 段階

D44の初回ベータに続き、ファイルドロップ対応beta.2を公開済み。配色拡充#419はローカル確認用ZIPまで。D:/NeNeNib/evidence/colorschemes-20261012/closeout.jsonが今回の統合/整理結果の所在。正式版の次の機能作業は自動で開始しない。
