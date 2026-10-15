# 5-en — 改行索引の受理と既存連打基準超過（Issue #446・#460）

出典: docs/quality/gate-proofs.md の節 5-en（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-13再開。実装・試験・関連依存/道具とexe SHAの不変を照合し、29,162 checksと境界検査を再利用した。`eng/measure-speed.py --check --bench <対象> --repetitions 5 --executable <変更後exe>`を4対象に各1回。通常入力0.613ms、長行6.341ms、open 213.292msは既存基準内、16MiB連打15.468msは上限9.264msを超過。各5標本・欠測0。初回/timeoutの失敗記録は保持する。
変更前7b117e2を`eng/build-release.ps1 -Ref 7b117e21b88d3c15df1ccea7327674fdd78a590c`で作り、失敗した連打だけ同じ道具・新規profile・5標本で測定すると15.349msで同じ基準超過。既存問題を#460へ分離した。0.119ms差は時間帯の異なる比較で因果を断定せず、全項目合格とは扱わない。[全コマンド・標本・版と限界](../newline-index-2026-10-12.md)。
PR447のhead b5218ba、必須CI38275473024、正規merge e5f8e709eef61d88ebcdbd943d93e837ad0d348fを確認。75ファイル95,162,688 bytesをD:/NeNeNib/evidence/newline-index-446/resume-20261013へ収載・SHA照合後、追加作業木と2出力を監査整理した。closeout-20261013.jsonを参照。ARC/CPP/QLTの詳細はADR0121、QLT-012に従う既存失敗分離。Waivers none。
