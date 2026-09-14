# アーキテクチャ決定記録（ADR）

トレードオフのある判断を、**理由ごと**残す場所。正本ドキュメントが「いま何が規範か」を書くのに対し、
ADR は「なぜそう決めたか」「何を却下したか」を書く。

## 書き方

- 連番 4 桁 ＋ kebab の題名: `NNNN-short-kebab-title.md`
- `0000-template.md` を複製して書く
- 状態は `提案` / `受理` / `却下` / `置換（→ NNNN）`
- **却下した選択肢を必ず書く。** 再提案は、その理由への反論から始めること
- 受理された ADR を後から書き換えない。変えるときは新しい ADR で置き換える
- 文脈は可能な限り実データ（コード・履歴・計測・コンパイラの出力）で裏を取る

## 一覧

| ADR | 題名 | 状態 |
| --- | --- | --- |
| [0001](0001-strictness-is-mechanically-enforced.md) | 厳格さは機械で強制する | 受理 |
| [0002](0002-plain-win32-with-direct2d.md) | UI ライブラリを使わず、素の Win32 ＋ Direct2D / DirectWrite で描く | 受理 |
| [0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md) | C++23 / clang-cl の検査基盤と実測できた限界を固定する | 受理 |
| [0004](0004-ui-thread-plus-one-worker.md) | 並行性は「UI スレッド＋固定ワーカー 1 系統」で、やり取りはメッセージだけ | 受理 |
| [0005](0005-own-vim-engine-verified-against-real-vim.md) | Vim は自前実装で、本物の Vim との差分テストで再現度を担保する | 受理 |
| [0006](0006-speed-gate-simd-and-table-driven-dispatch.md) | 速さはゲートで見張り、SIMD は区画に閉じ、Vim の分岐は表で書く | 受理 |
