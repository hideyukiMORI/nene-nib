# ADR 0141 — ファイルの読みは core が予約した封印済みの列で届き、判定と本文へ複写なしで移る

- 状態: 受理・実装済み（`024ad0c`・#581・2026-10-16）。元の行: 提案（設計席 2026-10-16 夜。実装は #570 の後続 Issue で。正式ゲートと独立レビューを経て受理）
- 日付: 2026-10-16
- Issue: #581（後続・#570 の試作 `experiment/open-single-copy`・probe 8.61 → 5.65 ms・`document_opened` 11.3 → 8.3 ms）
- 規則: ARC-003 / ARC-007 / CPP-003 / CPP-005、ADR 0080 / 0121 / 0124 / 0127

## 文脈

16 MiB を開く道で、実機の読み（`Win32FileAdapter` の `resize_and_overwrite` に `ReadFile` が直接書く・ADR 0080 決定 5）は複写を持たないが、`DetectedText::from_bytes(std::string_view)` が外から受けた列を一度だけ防御的に複写する（ADR 0080 決定 1・CPP-003）。この 1 回が 16 MiB で約 3.3 ms（`document_opened` の 11.3 ms のうち）で、D51 の目標 open 60 ms に対して 5% にあたる。ADR 0080 の末尾の独立レビューは「外の `std::string` を move で受ける」案を不採用にした。move の前に `data()` で得た別名から、判定済みの列と本文と索引を書き換えられるからである。#570 の試作はこの案をそのまま実験として置き（製品に入れない）、複写 1 回ぶんの数字だけを取った。

ADR 0127 §1 は「防御コピーは同じ bytes でも別割当てであり、reservation の移管は元を消費して先へ渡し、copy して二重に返却できる値にしない」と定めた。複写をなくす正しい形は、**外から可変な列を受け取らない**ことである。core が列を予約し、adapter は埋める間だけ可変の span を借り、埋め終わった列は封印されて可変の口を持たない。application はその封印済みの列を `DetectedText` へ move するだけで、別名を作れる場所が無い。

## 決定

1. **core に封印済みの列 `core::SealedBytes` を置く。** move だけできる型（複写不可）。作るのは唯一のファクトリ `SealedBytes::filled(std::size_t declared, Fill &&fill)` で、`fill` は `std::span<char>` を受けて書いたバイト数を返す（`resize_and_overwrite` と同じ約束・例外を投げない・`declared` を超えて書かない）。ファクトリが返した後、列を読む口は `std::string_view view() const &` だけで、可変参照・`data()`・rvalue からの借用は出さない。所有する `std::string` を取り出せるのは `DetectedText` だけ（非公開の口・friend の 1 か所）。
2. **`FilePort::read` の成功値は `core::SealedBytes`。** 宣言サイズの全体を取得したときだけ成功する約束（ADR 0124 決定 1〜3）と `maximum_bytes` の上限（ARC-001）はそのまま。`Win32FileAdapter` は `complete_file_read_into(span, operation)` の向け先を `SealedBytes::filled` の span に替えるだけで、反復の正本（ADR 0124 決定 4）と故障注入は変わらない。試験の替え玉 `ScriptedFiles` は自分の内容を `filled` の span へ写す（替え玉の 1 回の複写は製品の経路に無い）。
3. **`DetectedText::from_sealed(SealedBytes &&)` を足す。** 判定（`detect_encoding`）は封印済みの列の view に対して行い、成功した列を move で保持する（複写 0 回）。既存の `from_bytes(std::string_view)` は残し、外の可変データ（試験・ほかの呼び手）には今までどおり防御的に複写する。ADR 0080 決定 1 の「rvalue からの借用・可変文字列・検証を飛ばす生成口は出さない」は保つ（`from_sealed` は検証を飛ばさず、受けるのは可変の口を持たない型）。
4. **application は封印のまま運ぶ。** `EditorController::read_document` は `FilePort::read` の結果を `from_sealed` へ move する。Shift_JIS は `DetectedText` の view を `CodePagePort::to_utf8` に借用で渡す（ADR 0080 決定 3・変換後の列は別の所有）。小さなファイルを読むほかの呼び手（settings / session / history / bookmarks / themes の codec）は `view()` を読むだけに変える（意味は変えない）。
5. **本文への移管は今までどおり 1 本。** `TextBuffer::from_utf8(DetectedText)` → 非公開の実構築へ move（ADR 0080 決定 2）。BOM の除去（決定 3）・改行の索引（ADR 0121）は変えない。
6. **契約で封印を守る。** `tests/unit` の `--file-text` に、(a) `SealedBytes` が copy 構築・copy 代入できず、`view()` 以外に列へ届く公開の口が無いこと（`requires` 式の `static_assert`）、(b) `filled` が `declared` を超えて書かせない・書いた長さだけが `view()` の長さになること、(c) `from_sealed` が `from_bytes` と同じ判定結果（UTF-8 / BOM / Shift_JIS / 不正）を返すこと、(d) 既存の `verify_defensive_boundaries` がそのまま通ることを足す。adapters の試験は `Win32FileAdapter` の読みが `SealedBytes` で届き、短い read の反復（ADR 0124）が同じ結果になることを確かめる。
7. **速さの証明。** `nib_perf_probes` の `controller-open-utf8-16mib` と `measure-speed.py --bench open-large-file-16mib` を main と比べ、`document_opened` が約 3 ms 縮むこと、ほかのベンチに退行が無いことを正式ゲート（5 試行・静かな機械）で示す。

## 却下した選択肢

- 外の `std::string` を move で受ける（`from_bytes(std::string &&)`）: ADR 0080 のレビューが指摘した別名の穴が残る。#570 の試作はこの形で、数字を取るためだけに置いた。
- ADR 0080 を改め「`FilePort` の結果は信頼の境界の内側」と宣言する: 型でなく約束に頼る。呼び手が増えると守れない（ARC-012）。
- 複写を受け入れる（今のまま）: 16 MiB で 3 ms。D51 の目標は満たすが、ADR 0127 の「二重に返却できる値にしない」に沿う形が安く作れるので採る。
- `shared_ptr<const std::string>` を port から返す: 共有の所有は ADR 0127 の勘定（1 つの割当てを 1 回数える）を曖昧にする。

## 限界

- ADR 0127 §1 の予算の台帳（予約の勘定）はこの ADR では実装しない。`SealedBytes` は「可変の口を持たない所有の列」の形だけを定め、容量の勘定は台帳が入るときに `filled` の前で行う。
- 64 MiB の上限・snapshot でないこと・取消・2 GiB は ADR 0124 / 0127 のまま。
- `ScriptedFiles` の複写は替え玉の都合で、probe の数字に 1 回ぶん（約 3.3 ms）が残る。実機の読みには無い。
