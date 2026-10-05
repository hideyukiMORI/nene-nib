# ADR 0074: 表示幅のBMP索引は正本の範囲表から作る

- Status: accepted
- Date: 2026-10-05
- Issue: #291

## 決定

DisplayWidthRange.hppのVim実測値は唯一の意味の表のまま維持する。display_width内部に
コンパイル時生成のBMP索引（65536 byte）を閉じ、BMP外は従来の二分探索を使う。
全呼び出しはdisplay_widthのままで、DisplayLine・Vim仮想桁の意味も変えない。
実行時の表構築、環境参照、可変共有状態、外部のコード生成はない。

## 検証

変更対象は幅の検索方法と直接利用する描画用文字列・仮想桁。全1114112 code pointを正本の
順序走査と照合し、既存のdisplay-lineとvim-virtual-column scopeを実行する。
Releaseで長い行の単入力・連続入力を測定する。未測定時に性能受理とはしない。

## 2026-10-05の検証結果

対象の表示/IME/契約/通常Release比較を完了した。実装の高速化は確認できたが、長行の前後半安定条件で性能の総合受理とmain統合は保留。[gate-proofs 5-cm](../quality/gate-proofs.md#5-cm--字体選択可視字形表示幅とsplit再評価issue-291adr-007100730074)に成功・失敗・再利用・未確認範囲を記録した。
