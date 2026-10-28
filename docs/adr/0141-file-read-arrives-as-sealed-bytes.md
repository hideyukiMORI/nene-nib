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

## 追補（2026-10-29・PR 前の L2b の席が見つけた穴・設計席の決定・ADR 0159 の決定 2・3 のための型）

ADR 0159 は「大きなファイルは worker の 64 MiB ごとの塊の仕事で読み（FIFO を塞がない）、写しを増やさない」と決めたが、`SealedBytes::filled` は 1 回の同期の `fill` で封印するので、予約を複数の仕事にまたがって持ち回れない。次の形で埋める。

8. **予約を持ち回る型 `core::BytesReservation`。** move だけできる型。作るのは `BytesReservation::reserve(std::size_t declared) -> BytesReservation`（確保だけ・零で埋めない・`resize_and_overwrite` で長さを宣言）。書く口は `[[nodiscard]] std::span<char> window(std::size_t length) noexcept`（まだ埋めていない先頭から `length` だけの可変の窓・`declared − filled` を超えない）と `void commit(std::size_t written) noexcept`（窓に書いた長さを確定・超えた値は切り詰める）の 1 対で、窓は次の `commit` か `seal` まで有効（呼ぶのは同じスレッドの同じ仕事で、仕事の間は窓を持たない）。読む口は無い（`view()` も `data()` も出さない・封印の前の列は誰にも見えない）。封印は `[[nodiscard]] SealedBytes seal() &&`（`filled` の長さに切り詰めて `SealedBytes` にする・決定 1 の約束はそのまま）。`SealedBytes` の private の constructor は `BytesReservation` も friend で呼ぶ（friend は `DetectedText` と `BytesReservation` の 2 つ）。`filled` はこの型の `reserve → window → commit → seal` の薄い入口に置き換え、同期の道と worker の道で型を 2 本にしない。

9. **塊ごとの UTF-8 の検証は予約が抱える。** `commit` は確定した窓の分を `Utf8Validation::with_bytes`（ADR 0122）に流し、進捗を予約が持つ（検証するのは予約自身が受けたバイトだけなので、ADR 0122 決定 7 の「進捗は本文の証明書ではない」に反しない: 進捗と列は同じ型の中で生まれ、外から添えることはできない）。`seal()` は `finish()` の結果（受理なら code point の数・拒否なら値なし）を `SealedBytes` の非公開の欄に添える。`DetectedText::from_sealed` は、その欄が受理なら `detect_encoding` の UTF-8 の走査を飛ばし（BOM の先頭は今までどおり先に見る・`utf8_bom` か `utf8`）、拒否か欄が無ければ（`filled` 以外の道が無いので欄は常にあるが、拒否のとき）今までどおり判定する（Shift_JIS 等）。全文の検証は 1 回になる。`from_bytes`（外の可変データ）は今までどおり自分で検証する。

10. **塊の仕事。** `Win32DocumentReader` は `reserve` を UI スレッドで 1 回（`bad_alloc` はここで受けて `insufficient_memory`）、塊の仕事ごとに worker で `window(64 MiB) → ReadFile → commit → Progress`、最後の仕事で `seal → from_sealed → from_utf8 → Finished`。cancel は次の塊を積まず予約を捨てる（破棄で解放）。同期の道（64 MiB 以下）は同じ 4 つを 1 つの仕事で呼ぶ。

11. **契約。** `--file-text` に: (e) `BytesReservation` が copy できず読む口が無いこと（`requires` の `static_assert`）、(f) `window` が `declared − filled` を超えないこと・`commit` の超過が切り詰められること・`seal` の長さが `filled` の合計と一致すること、(g) 塊の切り方（1 バイトずつ・3 バイトの文字の途中・64 KiB）を変えても `from_sealed` の判定と code point の数が `from_bytes` と一致すること（UTF-8 / BOM / Shift_JIS / 不正）、(h) `filled` が `reserve → seal` と同じ `SealedBytes` を作ること。

却下: B（1 つの仕事の `fill` の中で塊を読む）は 1 GiB で worker を数秒塞ぎ、ADR 0159 決定 3 と ADR 0062 の「FIFO を塞がない」に反する（L2c で結局 A が要る）。C（adapter の `std::string` に書いて最後に 1 回写す）は山が 2 倍で、ADR 0159 の「写しを増やさない」と D63 の勘定（1.05〜1.3 倍）に反する。
