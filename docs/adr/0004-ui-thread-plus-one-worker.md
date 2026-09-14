# ADR 0004 — 並行性は「UI スレッド＋固定ワーカー 1 系統」で、やり取りはメッセージだけ

- 状態: 受理
- 日付: 2026-09-15
- Issue: #1
- 影響する規則: ARC-004 / ARC-005 / ARC-007 / CPP-013 / CNF-009

## 文脈

テンプレの既定（Clock SWG-001 / Folio C-013）は「UI スレッドのみ・背景スレッドを作らない」である。Nib はシンタックスハイライト・
Ctrl+P のファイル走査・巨大ファイルの行索引・Markdown 変換を持ち、これらを UI スレッドで同期に行うと「キー→画面」の遅延が要件
（SPECIFICATION 第 1 節）を破る。施主は「UI スレッド＋固定のワーカー 1 系統（メッセージ渡しのみ・共有可変状態なし）」の方向を
了承している（SPEC-DRAFT 3.1）。

Phase 0 で何を機械で強制できるかを実測した（[phase0-results.json](../quality/phase0-results.json)）:

- TH1: `std::thread` / `std::jthread` は `_beginthreadex`、`std::mutex` は `_Mtx_lock` / `_Mtx_unlock`、`std::condition_variable` は
  `_Cnd_*`、`CreateThread` は `__imp_CreateThread`、`std::async` は `Concurrency` 名前空間の ConcRT 群と `_Thrd_*` として
  `llvm-nm --undefined-only` に現れる。**スレッドの生成と同期原始はリンカ段で捕まえられる**
- TH2: `std::atomic<int>::fetch_add` はインライン化され、未定義シンボルは security cookie の 2 つだけ。**共有可変状態そのものは
  リンカでは見えない**

## 決定

**スレッドは UI スレッドと、`src/adapters/win32` が所有する固定のワーカー 1 本だけ。application はワーカーへ「要求」の値を渡し、
ワーカーは「完了」を UI スレッドのメッセージとして返す。両者は可変状態を共有しない。**

- 要求は不変の値型（対象・版番号・範囲）。ワーカーは自分が受け取った値だけを読み、結果を新しい値として作る
- 完了は UI スレッドの窓メッセージ（`PostMessage` に所有権を載せた heap の値）として届き、application が意図として受ける。
  古い版番号の完了は捨てる（状態の所有者は application ただ 1 つ・ARC-004）
- ワーカーはポートの実装であり（`WorkerPort` を application が宣言し、`src/adapters/win32` が実装する）、application と core は
  スレッド・mutex・atomic・条件変数・`std::async`・スレッドプールを一切知らない
- ワーカーの実体は 1 本のスレッドと 1 本のキュー。2 本目が要る計測が出たら新しい ADR
- 中断は版番号で行う（要求ごとに版を持ち、ワーカーは処理の区切りで最新版と比べて古ければ捨てる）。`TerminateThread` は使わない

## 強制

- **planned** → `eng/symbols.py` の `concurrency` 分類（CPP-013）: `_beginthread(ex)?` / `__imp_CreateThread` / `_Thrd_*` / `_Mtx_*` /
  `_Cnd_*` / `__std_atomic_*` / `Concurrency` 名前空間 / `__imp_CreateEvent*` / `__imp_WaitForSingleObject*` /
  `__imp_(Post|Send)Message*` 等が core / application の静的ライブラリの未定義シンボルに現れたら落ちる（TH1）。
  中核のライブラリが生まれて `--require core application` を結線したときに active
- **planned** → CNF-009 の字句検査: `<thread>` `<mutex>` `<atomic>` `<future>` `<condition_variable>` `<semaphore>` `<latch>` `<barrier>`
  `<stop_token>` `process.h` `synchapi.h` を `src/adapters/win32` 以外の `src/` で include したら CPP-013（TH2 の穴を補う。
  検査器の正例・反例テストがゲートで回った時点で CNF-009 は active）
- **不能**: ワーカーが受け取った値を本当に読むだけであること（意味の検査）。レビュー事項

## 結果

得られるもの:

- 「キー→画面」の経路にロックが無い。UI スレッドは常に描ける
- 競合の形が 1 つ（版番号の比較）に固定され、テストで再現できる
- 中核は OS の並行性を知らず、単体テストは単一スレッドで決定的

失うもの:

- 並列度は 1。複数コアでの並列ハイライトは初版の範囲外（要るなら ADR）
- 完了の受け渡しで値を複製する。1 GB の索引は「索引だけ」を渡し、テキストは複製しない設計が要る（Phase 3 の ADR）

正直に記録しておくこと:

- `std::atomic` はリンカに見えない（TH2）。字句検査はヘッダ名しか見ないので、`volatile` や Interlocked 系の関数呼び出しは
  シンボル検査（`__imp_Interlocked*` は無い＝組み込み）でも字句でも見えない。CNF-009 に名前を足すことはできるが、完全ではない
- ui_win32 と app もスレッドを作れない（字句検査が `src/adapters/win32` 以外の `src/` を拒否する）。シンボル検査は core / application だけ

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| UI スレッドのみ（テンプレの既定・Loupe / Folio） | 1 GB の行索引と全文走査を UI スレッドで同期に行うと、要件の「キー→画面」を破る |
| `std::async` / スレッドプール | TH1 で ConcRT（`Concurrency` 名前空間）と `_Thrd_*` を引き込む。並列度が環境で変わり、ベンチが再現しない。「1 系統」に反する |
| ロックで守る共有状態（mutex ＋ 共有バッファ） | 状態の所有者が 2 つになる（ARC-004）。競合の形が無数になり、テストで再現できない |
| C++20 コルーチン | 実行器を自分で書くことになり、ワーカー 1 本より複雑。実測していない |
| Win32 のスレッドプール（`SubmitThreadpoolWork`） | 並列度が OS 任せ。1 系統の版番号中断が書きにくい |

## 関連

- [ADR 0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md)（シンボル検査の 3 分類）
- SPECIFICATION.md 第 3 節
