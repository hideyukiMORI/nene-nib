# ADR 0134 — クリップボードの書き込みは、開けなければ数回だけ試し、失敗を成功と言わない

- 状態: 提案（設計席 2026-10-15。実装は #496 で）
- 日付: 2026-10-15
- Issue: #496（外部講評 2026-10-15 の P1-2・共通指示 §4-3「クリップボード」）
- 規則: ARC-007 / ARC-010、CPP-005、QLT-013、ADR 0009 / 0051

## 文脈

`src/adapters/win32/Win32ClipboardAdapter.cpp` の `write()` は、UTF-16 への変換・`GlobalAlloc`・複写を `OpenClipboard` の前に済ませている（正典の 1 は満たす）。足りないのは次の 3 つ（事実確認 `D:\NeNeNib\outputs\winapps-review-20261015\fact-check.md` P1-2）。

- `OpenClipboard` が失敗したら（ほかのプロセスが開いている瞬間は普通にある）そのまま `unavailable` を返し、再試行しない。
- `EmptyClipboard` の後に `SetClipboardData` が失敗すると、使う人の前の中身は消えたまま。失敗は呼び手に返るが、使う人には見えない。
- 変換できない本文を空文字列として置き、成功を返す（`:50`）。

同じ穴はシリーズの loupe にもあった（共通指示 §4-2）。正典はコードでなく仕様とテストで共有する。

## 決定

1. **書くデータは `OpenClipboard` の前に全部作る（今日どおり）。変換できなければ書かずに `Failure::unrepresentable`（閉じた値を 1 つ足す）で返す。** 空文字列を置いて成功と言わない。
2. **`OpenClipboard` が失敗したら、短い間隔で数回だけ再試行する。** 回数と間隔は adapters の定数 1 か所（5 回・10 ms）。待つのは adapters の `Sleep`（OS に触れてよいのは adapters だけ・ARC-007）。それでも開けなければ `unavailable`。`read()` も同じ再試行を通す（開く口を 1 本 `opened_clipboard()` にする）。
3. **`EmptyClipboard` の後で失敗したら、黙らない。** adapter は `write_failed` を返し、controller はそれを 1 行の知らせ（ステータスバーの `notice`・既存の経路）で「クリップボードに書けませんでした」と見せる。成功の表示（あれば）を出さない。Vim の `"+` の書き込みも同じ経路（ADR 0051 の `VimStep.clipboard` の後）。
4. **試験は替え玉で注入する。** adapters の試験（`nib_adapter_tests`）に「開けない（`OpenClipboard` が n 回失敗して n+1 回目で開く／ずっと失敗）」「書けない（`SetClipboardData` が失敗）」「変換できない本文」の 3 つを、Win32 の呼び出しを関数ポインタか小さな port で差し替えて注入し、戻り値と「成功の表示が出ない」ことを確かめる。実機のクリップボードは触らない（施主の了承が要る・`clipboard-probe-needs-consent`）。application の契約 `--vim-clipboard` に「書き込みの失敗が知らせになる」を足す。

## 却下した選択肢

- 再試行を無限にする／長く待つ: UI スレッドを止める。5 回・10 ms で十分（ほかのプロセスが開くのは数 ms）。
- `EmptyClipboard` の前に中身を読んで失敗時に戻す: 戻す書き込みも失敗し得る。失敗を見せる方が正直。
- 変換できない本文を置き換え文字で書く: 使う人が貼った内容が変わる。拒否して知らせる。

## 検証

- adapters の注入の試験 3 つ、application の契約（知らせ）、Debug（tidy / ASan / UBSan）、symbols（core / application は不変）、conformance。

## 限界

`OpenClipboard` が長く取られている（別のアプリの不具合）場合は 50 ms 待って諦める。OS のクリップボード履歴（Win+V）への反映は OS の仕事で見ない。
