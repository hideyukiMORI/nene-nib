# ADR 0159 — 文書を開く道は 1 本の読みの port を通り、大きなファイルは worker で読んで揃ってから採用する

- 状態: 受理（施主 2026-10-28「全部推しでいい」・巨大ファイル 段 2・3。施主決定 D63 と計画 §2 の L-3 / L-4 は計画の推しのまま・hide の確認の後に着手。ADR 0158 の分割の後）
- 日付: 2026-10-28
- Issue: #993（段 2・3）・親 #791・計画 `docs/plans/2026-10-21-large-file.md` §3.2 / 3.3
- 規則: FR-013、ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / ARC-011、CPP-002 / CPP-005 / CPP-013 / CPP-016、QLT-014、ADR 0004 / 0062（worker 1 本と型のある port）/ 0010 決定 12（同期の読み・本 ADR が改める）/ 0121（改行の索引）/ 0122（流れる UTF-8 の検証）/ 0127（資源・券・一回確定）/ 0131（worker の最初の仕事は device）/ 0135（一時ファイル）/ 0137（自動退避）

## 文脈

今の開く道は `FilePort::read(path, 64 MiB)` を UI スレッドで同期に呼び、全文を 1 回の `ReadFile` で読んで `TextBuffer::from_utf8` が検証と改行の索引を作る（16 MiB で 9.9 ms・大きさに比例）。D63 の目標は 1 GB。線形に伸ばすと暖かい SSD で約 0.6 s・冷たい HDD で 7 s 以上 UI が止まる。段 1（#792）で改行の索引は本文の 5% 以下になり、上限の正本は `core::maximum_document_bytes` の 1 か所になった。

計画の決め（D63）: 最初の製品段は piece table のまま、開く道を worker に移して上限を上げる。メモリマップは採らない。索引は全部作り終えてから文書を採用する（部分の本文を公開しない）。

## 決定

1. **開く道は 1 本のまま、読みは型のある port `DocumentReadPort` を通る。** `FolderPort`（ADR 0062）と同じ形: `request(DocumentReadRequest{path, ticket, maximum_bytes})` は頼んですぐ戻り、`collect()` が揃った結果と進み具合を全部返す（UI スレッドだけが呼ぶ）。`FilePort::read` は残すが、開く道（`open_document` の 1 本・呼び手は `accept(OpenDocument)` `open_listed` `reach_tab` 起動の引数・前回のタブの復元・D&D）は呼ばない。`FilePort::read` の残りの呼び手（履歴・ブックマーク・設定・退避の読み戻し・小さなファイル）は今のまま。

2. **結果は adapter が揃えた文書で、application は採用するだけ。** `DocumentReadOutcome` は閉じた和型 `std::variant<DocumentReadProgress, DocumentReadFinished>`。`DocumentReadProgress{ticket, read_bytes, total_bytes}` は 64 MiB の塊ごとに 1 つ。`DocumentReadFinished{ticket, std::expected<OpenedDocument, FileFailure>}` の `OpenedDocument{core::TextBuffer text; core::TextEncoding encoding;}` は、adapter が worker の上で **読み → 符号化の判定（`DetectedText`）→ 流れる UTF-8 の検証（`Utf8Validation`・塊ごと）→ `TextBuffer::from_utf8`（改行の索引）** を全部終えたもの。core の純関数を adapter が worker のスレッドで呼ぶ（ARC-003 は core が OS に触れないこと・adapter が core を呼ぶのは今の `AutosaveCodec` と同じ）。Shift_JIS / UTF-16 の復号は adapter が `CodePagePort` を同じ worker で呼ぶ（UI スレッドの `read_document` の復号は open の道から消える）。application の採用は `TextBuffer` の move で O(1)。

3. **小さなファイルは同期に揃い、大きなファイルだけ worker へ。** `request` は `std::optional<DocumentReadOutcome>` を返し、大きさが `synchronous_read_bytes`（64 MiB・今の上限）以下なら呼んだスレッドで読み切って `Finished` を返す。超えるものは worker に塊の仕事（64 MiB ごとに 1 つ・FIFO を塞がない・ADR 0062 の形）を積み、`std::nullopt` を返す。どちらも application の採用は同じ関数 `adopt_read(outcome)` を通る（道は 1 本）。これで起動と 16 MiB のベンチ（`open-large-file-16mib`）は今の同期の道と同じ費用で、worker の最初の仕事（device の生成・ADR 0131）の後ろに並ばない。

4. **揃うまでタブを足さない。読み込み中はステータスバーに 1 行。** 待っている間はタブも文書も作らず、`EditorState` に「読み込み中の要求」（券・パス・題名・進み具合）を 1 つ持つ。ステータスバーの知らせの欄に「読み込み中: <名前> 37%」（進み具合は `Progress` の `read_bytes / total_bytes`・色は `muted`・新しい色のトークンは足さない）。揃ったら今の `open_document` の続き（同じファイルのタブへ切り替え・無題に開く・新しいタブ）をそのまま走らせる。見た目は 1 行の文字だけなので /design は要らない（変えるなら D 番号）。

5. **券で古い完了を捨てる。** 券はプロセスで 1 つの通し番号。新しい `OpenDocument`・`EndSession`・読み込み中の要求を持つ窓を閉じるときは、券を進めて adapter に `cancel(ticket)` を頼む（worker は塊の間で止める合図を見る・ADR 0004）。`collect` で届いた券が今の要求と違えば捨てる（ADR 0127 §3 の一回確定）。同じパスを読み込み中にもう一度開く意図は、今の要求をそのまま続ける（2 本目を頼まない）。Esc での取消は 段 3 では置かない（限界）。

6. **失敗は値。** `FileFailure` に `insufficient_memory` を足す。adapter は `SealedBytes::filled` と `from_utf8` の確保の `std::bad_alloc` を捕まえて値に変える（ARC-010・core は投げない形のまま・adapter の境界で 1 か所）。`too_large`（上限超え）・`undecodable`・`unreadable`・`not_found` は今のまま。取消は結果を出さない（捨てる）。失敗の出し方（ダイアログか 1 行の知らせか）は呼び手ごとの今の規則のまま。

7. **上限は 2 つの定数、正本は core の 1 か所。** `core::maximum_document_bytes` を **1 GiB**（D63）に上げ、`core::maximum_decoded_document_bytes`（64 MiB）を足す。UTF-8 / UTF-8 BOM は 1 GiB まで、Shift_JIS / UTF-16 は 64 MiB まで（超えたら `too_large`・L-3）。自動退避の読み戻し（`AutosaveCodec`）と退避の対象は `maximum_decoded_document_bytes` を読む（64 MiB を超える本文は退避しない・1 回知らせる・L-4・ADR 0137 の追補は段 4 で）。窓の文言「64 MiB を超えるファイルはまだ開けません」は「1 GB を超えるファイルは開けません」に直し、Shift_JIS / UTF-16 の 64 MiB 超は「64 MiB を超える Shift_JIS / UTF-16 は開けません」。

8. **BOM は消さずにずらす。** `TextBuffer::from_utf8(DetectedText)` の `erase(0, 3)` は、最初の piece の開始を 3 にずらす形に替える（1 GiB の memmove をしない・ADR 0044 の piece の形のまま・改行の索引は original の bit のまま）。

9. **合図と規則。** 完了と進み具合の知らせは今の中身の無い窓メッセージ（`work_message` の 1 つ）→ 意図 `WorkCompleted` → `collect`（ADR 0062 の決定 7）。新しい意図は足さない。`WorkCompleted` が歩き・知らせ・変換・Vim の待ち・`.`・undo を動かさない規則はそのまま。

10. **契約。** 新しい scope `--open-async`（`tests/unit/OpenAsyncTests.cpp`・替え玉 `ScriptedDocumentReads`）: 同期に揃う小さなファイル・大きなファイルの要求 → 進み具合 → 揃う → タブが 1 つ足される・古い券の結果を捨てる・取消・失敗の値（`insufficient_memory` を含む）・読み込み中は帯が変わらず知らせの文が出る・`reach_tab` と復元と `open_listed` が同じ道・`verify_quiet_paths` に「打鍵・切り替え・保存で `DocumentReadPort` を呼ばない」。adapters の試験は 65 MiB の見本（固定の行を書いた一時ファイル）で worker の道を通し、セマフォで待つ（時計を読まない）。速さは 8 本に退行なし（同期の道は不変）、`open-large-file-512mib` と `open-large-file-1gib` は段 6 の任意のベンチ。

11. **限界（本 ADR の外）。** 保存と自動退避は同期のまま（1 GB の保存は数秒 UI が止まる・段 5 の ADR）。全文を走る検索（`/` `*`）は同期（段 5）。undo の履歴は本文と同じ桁のメモリを持ちうる。Esc での取消は無い。読み込み中にその窓を閉じると取消で、ファイルは開かれない。メモリマップは採らない（計画 §0）。2 GiB の宣言は段 6 の実測で決める（D63）。

## 1 打鍵の道の費用

打鍵の道は変えない。`collect` は `WorkCompleted` の道だけで走る。起動と 16 MiB の open は同期の道（決定 3）なので費用は今と同じ。採用は `TextBuffer` の move。

## 強制

- 開く道の呼び手が `DocumentReadPort` だけを通ること・打鍵と切り替えと保存で port を呼ばないこと: **planned**（契約 `--open-async` と `verify_quiet_paths` の欄・段 2 の PR で active）。
- core が OS・スレッド・時刻に触れないこと: **active**（`eng/symbols.py`）。
- 失敗が値であること: **active**（CPP-005・`std::expected`・閉じた enum）。
- 8 本の速さに退行なし: **active**（QLT-014・段 2 の PR で `--check`）。

## 結果

得られるもの: 1 GiB の UTF-8 を開ける（D63）。読んでいる間も別のタブで打てる（worker・塊ごとの仕事）。読み込み中の見え方が 1 行で分かる。
失うもの・残る穴: 1 GiB の本文と索引（約 5%）で private bytes は入力の 1.05〜1.3 倍（512 MiB の条件は必須にしない・D63）。保存・退避・全文検索の停止は段 5 まで残る。Shift_JIS / UTF-16 の大きなファイルは開けない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 全部を worker へ（小さなファイルも） | 起動の最初の仕事が device の生成（160 ms）で、16 MiB の open がその後ろに並ぶ。基準値 49.8 ms を守れない |
| 2 本目のスレッド（読み専用） | ADR 0004 / 0062 の「worker は 1 本」を変える。塊の仕事で FIFO を塞がなければ 1 本で足りる |
| 部分の本文を先に見せる（遅延の行索引） | 行数・末尾・符号化が未確定の状態を application が持つ。計画 §0 で「最初は採らない」 |
| application が worker の上で `from_utf8` を呼ぶ | application はスレッドを持てない（ARC-007・CPP-013）。純関数を呼ぶ側は adapter |
| メモリマップ | 参照時の `EXCEPTION_IN_PAGE_ERROR` を core に持ち込めない（計画 §0・設計案 2026-10-12 で却下済み） |
