# 依頼書の型 — 実装席

> 正本は [ADR 0038](../adr/0038-model-per-seat-and-scripted-preparation.md)（席ごとのモデル）と [ADR 0039](../adr/0039-implementation-seat-per-step-and-small-tool-output.md)（工程ごとの席・道具出力）。
> この型を `D:\NeNeNib\briefs\` に写して埋め（リポジトリの外の作業場所は D ドライブ・施主指示 2026-09-30。C ドライブの scratchpad と Temp は使わない）、Agent（`subagent_type: general-purpose`・`model: opus`）にパスを渡す。モデルは別名で書き、版の番号は書かない（`model` を省くと設計席のモデルを継ぐ・ADR 0038 の 2026-11-15 の追補）。小さい chore は依頼書を別に書かず、Issue 本文と ADR を指して席とモデルと工程を 1 行で伝える。

```markdown
# 依頼書 #<issue> — <題>（<工程>）

席・モデル・工程: 実装席 / opus / <probe | 実装 | 差し戻し対応 N 回目>
Issue: https://github.com/hideyukiMORI/nene-nib/issues/<issue>（`gh issue view <issue>`）
ADR: docs/adr/<NNNN>-....md（決定 1〜N が設計。変えない）
前の工程の報告: <`out/reports/done-<issue>-<工程>.md` のパス。無ければ「無し」>
ブランチ: <type>/<issue>-<summary>（main `<sha>` から）
範囲: <触ってよい dir と、触らない dir>
やらないこと: <S 級に切った残りの命題。別の席で行う>

## 席の約束（ADR 0039）

- この席は上の 1 工程だけを担う。終わったら報告して止まる。次の工程は新しい席が担う。
- テストは対象だけ実行する（`ctest -R <名前>` / `build/tests/unit/nib_tests.exe <対象>`）。出力は `out/<issue>-<工程>.log` に落とし、失敗行だけ `tail` / `Select-String` で読む。
- ビルド・ゲート（`eng/check.ps1`・`eng/conformance.py`・`eng/symbols.py`）の出力も `out/` のファイルへ落とし、`grep` で読む。端末に全文を流さない。
- 大きいファイル（fixture・生成物・ログ・数千行の翻訳単位）は必要な範囲だけ読む（`Read` の offset / limit・`Grep`）。触る関数とその呼び出し元は、判断に要るだけ読む。
- 作業を捨てる git の命令（`checkout --`・`reset --hard`・`clean`・`push --force`）を使わない。誤って書いた所は Edit で直して報告する（無人の席は権限の確認に答えられない）。
- 設計の穴・依頼書と現物の食い違い・権限で止められた操作に当たったら、迂回せず止まって報告する。
- 最終報告は 30 行以内。詳細は `out/reports/done-<issue>-<工程>.md` に書き、親にはそのパスと数字（checks 数・fixture 件数・SHA・所要時間）だけ返す。
- リポジトリの外に何かを置くときは `D:\NeNeNib\` の下（C ドライブの Temp と scratchpad に書かない）。道具の出力と報告はリポジトリの `out/`。
- commit / push / PR は依頼書に書かれた範囲だけ。Ready・CI・merge は設計席。PR の本文は GIT-004 の必須欄（`Closes #N` / `目的:` / `使った正典経路:` / `規則 ID:` / `振る舞い・スキーマの変更:` / `検証（実行したコマンドと結果）:` / `確認する退行:` / `対象・依存:` / `検証結果:` / `再利用:` / `Waivers:` / `残るリスク:`）をすべて持ち、作る前にファイルに書いて `python eng/git-conventions.py <ファイル> --pr-body` で検証する（#191 で 2 回落ちた）。

## 先に読むもの

<CLAUDE.md §2・関係する ADR・触るファイルの該当範囲（行の範囲まで書く）>

## 進め方

1. <命題 1>
2. <命題 2>
（S 級: 命題 3 つまで。超えるなら別の席）

## 設計判断で迷いそうな所

<設計席の答えを先に書く。答えが無いものは「止まって報告」>

## 検証

- やる: <対象だけ。`ctest -R` / unit の対象指定 / `--regenerate` × 2 / conformance>
- やらない: <全件・Release・速さ（設計席が回す）>

## 報告の形

CLAUDE.md §5 を `done-<issue>-<工程>.md` に書く。親への最終報告は 30 行以内で、Issue / 規則 ID・変更ファイル・検証の対象と結果の数字・報告ファイルのパス・止まった理由（あれば）。
```

## 使い分け

| 工程 | 席 | 渡すもの | 受け取るもの |
| --- | --- | --- | --- |
| probe（現物調査） | 新しい席（`sonnet`。読むだけ・ADR 0038） | Issue・調べる問い（15 件以内） | `probe-<issue>.md`（300 行以内） |
| 実装 | 新しい席（`opus`） | 依頼書・probe の報告のパス | draft PR・報告ファイル・30 行の報告 |
| 差し戻し対応 | 新しい席（`opus`）・変更 1 命題 | 依頼書（狭く）・前の報告のパス・差し戻しの 1 命題 | 同上 |

同じ席を `SendMessage` で次の工程に使わない。同じ工程の続き（429 で止まった・設計の穴で止まって報告した）は、同じ席に答えを送って再開する。main が動いた後のやり直しは新しい席。`fork` は設計席のモデルと全文脈を継ぐので背景席に使わない。
