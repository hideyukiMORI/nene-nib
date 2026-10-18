# ADR 0138 — 1 行が 16 MiB の文書のベンチ

- 状態: 受理（設計席 2026-10-16。実装は #513。基準値は 2026-10-18 に 1 回測って「はい」の 2 本を採用・#644・[記録](../quality/speed-adoption-2026-10-18.md)。2 本は `OPTIONAL_BENCHES` のままで判定の 8 本に移さない）
- 日付: 2026-10-16
- Issue: #513（施主決定 D52・外部講評 P2）
- 規則: QLT-014、ADR 0011 / 0013 / 0044 / 0075（決定 7）/ 0114

## 文脈

速さの見張りは 8 本（起動 2・打鍵 3・16 MiB 2・面 1・長い行 1）で、長い行は 6 KB の日本語行（ADR 0075 決定 7）。1 行が巨大なファイル（minified JSON・1 行の log）は本文の文字組みが行の全体を 1 つの DirectWrite layout に出す正典（ADR 0114）の弱点で、外部講評 P2 が指摘した。施主は D52 で「ベンチを足して測り、基準は 1 回測ってから、値がひどければ Issue にして直す順を決める（ADR 0114 の再検討を含む）」と決めた。

## 決定

1. **文書は道具が作る `giant-line.txt`。** ASCII だけ・改行なし・16,800,000 bytes ちょうど。形は minified JSON 風（`{"k000000":"nenenib giant line sample value","k000001":…}` を繰り返し、最後の鍵で長さを合わせて `}` で閉じる）。`long_line_document` と同じく SHA-256 を道具に固定し、違えば測らない（#298）。
2. **ベンチは 2 本で、1 回の試行から取る。** `open-giant-line-16mib`（プロセス生成から節目 `body_presented`＝本文を描いた最初の frame が present された時まで・起動の内訳つき。開いた直後の最初の `frame_presented` は表示形式の問い（ADR 0140 の決定 2）を待つ本文の無い frame なので値にしない。道具は問いに答え、窓のスレッドが静かになってから閉じる。`body_presented` は起動の節目の列（ADR 0013）の末尾で、問いの無い文書では最初の `frame_presented` と同じ frame・#546。`open-large-file-16mib` と `startup-first-frame` の値は今までどおり最初の `frame_presented`）と `key-to-frame-single-giant-line-16mib`（文書を起動引数で開き、暖機の 1 文字の後の 1 つの WM_CHAR の `input_received` から次の `frame_presented` まで・`key-to-frame-single-long-line` と同じ経路）。キャレットは開いた直後の位置（行頭）で、打鍵は行の全体の再整形を起こす最悪の位置。
3. **巨大な行の打鍵の試行は 200 打鍵を送らない。** 1 打鍵が秒の桁になり得るので、200 打鍵の往復は試行の時間を何十倍にもする。`keys_trial` に「200 打鍵を送らない」選択を足し、この試行だけ暖機と 1 打鍵で終える。
4. **最初は `--bench` で名指ししたときだけ測る（`OPTIONAL_BENCHES`）。** 既定の 8 本（`BENCHES`）には入れない。製品が 60 秒で frame を出せない場合は欠測として記録し（`LARGE_SECONDS`）、ほかのベンチを止めない。基準値（`--adopt --bench`）を 1 回測って書く commit で、`BENCHES` に移すかどうかを決める（基準値の無いベンチは `compare` が飛ばすので、移しても落ちない）。
5. **記録の形は既存と同じ。** `out/speed/*.json` の `values` と `breakdown`、`document` の欄（bytes・lines=1）。既存 8 本の `values` の鍵は変えない。起動の内訳（`breakdown`）には `body_presented` の区間が加わる（問いの無い文書では 0）。

## 却下した選択肢

- 既定の 8 本に最初から入れる: 1 試行が 60 秒の欠測になり得る段階で、毎回のゲートの時間を 10 分延ばす。
- 1 行 1 MiB で済ませる: 外部講評と D52 の対象は 16 MiB。弱点の見える化が目的なので、値を小さく見せる文書にしない。
- 既存の 16 MiB の文書（200,000 行）を使い回す: 行索引と文字組みの両方が違う弱点で、別の文書でないと切り分けられない。

## 検証

- 実機の Release（main の製品 `build/release-89c582d`）で `--bench open-giant-line-16mib` と `--bench key-to-frame-single-giant-line-16mib` を各 1 回（5 試行）。欠測なら欠測の記録（`missing` の数）を残す。
- 既存 8 本の `values` の鍵の差分 0（`--bench key-to-frame-single` の JSON を前後で比べる）。

## 限界

- 2026-10-16 の値は施主の実機の 1 回で、基準値ではなかった。「ひどい」値（open 6 秒・1 打鍵 5 秒）は D53 の格子と窓で直し、2026-10-18 に静かな機械で測り直した「はい」の open 129.6 ms・1 打鍵 1.23 ms を基準値にした（#644）。開くの 5 試行は 2 つの山（約 35 ms 差）に分かれるので、判定に載せるなら試行を増やすか山の原因を先に見る。
