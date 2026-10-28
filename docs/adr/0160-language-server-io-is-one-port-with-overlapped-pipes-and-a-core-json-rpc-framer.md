# ADR 0160 — 言語サーバーの入出力は 1 本の port（OVERLAPPED のパイプと子プロセス）で、JSON-RPC の枠と JSON は core の純関数

- 状態: 受理（施主 2026-10-28「全部推しでいい」・LSP の段 L0・計画 `docs/plans/2026-10-21-syntax-and-lsp.md` §2。施主決定 D60（設定で切り替え・既定オフ・オンの速さは別の基準）。L-1（初版の範囲に入れるか・FR と D 番号）・L-3 の残り（自動の再起動の回数）・L-4（送らない文書の大きさ）・L-5（最初は clangd）は hide の確認の後に着手）
- 日付: 2026-10-28
- Issue: #994（L0）・親は計画の Issue #755・構文の色の親 #814
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-010 / ARC-011、CPP-002 / CPP-005 / CPP-011 / CPP-013 / CPP-016 / CPP-017、QLT-014、ADR 0004 / 0062（worker 1 本・型のある port・中身の無い合図）/ 0114（設定の版）/ 0126（OVERLAPPED の取消と完了の分離）/ 0137（タイマーの意図は使う人の操作ではない）/ 0156（構文の色の区間の形）

## 文脈

LSP 3.17 は子プロセスと stdio で JSON-RPC 2.0 を話す（`Content-Length` の見出し → `\r\n\r\n` → UTF-8 の本文）。Nib の規則では、時刻・プロセス・パイプ・スレッドは `src/adapters/win32` にしか置けず（ARC-007）、core は OS に触れず（ARC-003）、application は型のある port と意図だけを見る（ARC-004 / 0011）。既存の裏の仕事は `Win32Worker` 1 本の FIFO で、同期の `ReadFile` を載せると device の生成・列挙・退避が止まる（計画 §2.2）。本 ADR は L0（前提: 入出力・子プロセス・枠と JSON）だけを決め、診断（L1）・補完（L2）・hover と定義（L3）は別の ADR で、この port の上に載せる。

## 決定

1. **port は 1 本 `LanguageServerPort`（application）。** `start(ServerLaunch{FilePath executable; std::vector<std::string> arguments; FilePath root;})`・`send(std::string message)`（JSON-RPC の本文 1 通・見出しは adapter が付ける）・`collect() -> std::vector<ServerEvent>`・`stop()`（`shutdown` の後の `exit` と終端の回収は adapter）。`ServerEvent` は閉じた和型 `std::variant<ServerStarted, ServerMessage{std::string body}, ServerExited{std::int32_t code}, ServerFailed{ServerFailure}>`。UI スレッドだけが呼ぶ。`FolderPort` と同じ「頼んですぐ戻る・合図で `collect`」の形で、**新しい合図は足さず** `work_message` → `WorkCompleted` → `collect` を通る（ADR 0062 の決定 7）。

2. **adapter `Win32LanguageServer` は子プロセスと OVERLAPPED の名前つきパイプ 2 本で、待ちは UI スレッドの pump の配列に載せる。** `CreateProcessW`（`STARTF_USESTDHANDLES`・`CREATE_NO_WINDOW`・標準入出力は `FILE_FLAG_OVERLAPPED` の名前つきパイプ・標準エラーは捨てる）・Job Object（`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`・Nib が落ちたら子も落ちる）・読みは常に 1 本の `ReadFile` を発行して event を立て、書きは待ち行列から 1 本ずつ `WriteFile`（64 KB を超える `didOpen` が詰まらない）。event と子プロセスの handle は adapter が `waitable_handles()` で窓の pump に貸し、pump は `MsgWaitForMultipleObjectsEx` の配列に足す（WARP → 本物の device の待ちが先例）。立ったら adapter の `completed(handle)` が I/O を回収して event の列に積み、`work_message` を 1 回送る。取消と完了は ADR 0126 の契約（取消の成功だけで資源を解放しない・最終完了を待つ）。`Win32Worker` は使わない（同期の読みを FIFO に載せない・2 本目のスレッドも作らない）。

3. **枠の切り出しは core の純関数。** `lsp_frames(std::string_view bytes) -> std::expected<LspFrames{std::vector<std::string_view> bodies; std::size_t consumed;}, LspFrameFailure>`。`Content-Length` だけを読み（`Content-Type` は無視）、見出しの誤り・本文の長さの上限（16 MiB・L-4 の文書の大きさと同じ桁）超え・見出しの長さの上限（8 KiB・`header_too_long`）超えは閉じた失敗。adapter は受け取ったバイト列を繋いでこの関数に渡し、`consumed` だけ捨てる。

4. **JSON は core に手書きの最小の 1 本。** `JsonValue` は閉じた variant（null・bool・number（`double`・整数は `std::int64_t` を別に持つ）・string・array・object（並びを保つ `std::vector<std::pair<std::string, JsonValue>>`））。`parse_json(std::string_view) -> std::expected<JsonValue, JsonFailure>`・`write_json(const JsonValue &) -> std::string`。受けるのは RFC 8259（`\uXXXX` と代理対・数は `double` の範囲・入れ子の深さ 128 まで・重複する鍵は後勝ち）。外部の JSON ライブラリは入れない（例外と `std::` の大量の使用で厳格集合を通らない見込み・md4c の 3 点セットを増やさない）。性質の試験（固定 seed のランダムな値の往復・不正な入力の閉じた失敗・深さの上限）を既定実行に載せる。

5. **JSON-RPC の対応は core の純関数の層。** 要求 id は application が通し番号で付け、`LspRequest{id, method, params}` → 本文、本文 → `LspIncoming`（`Response{id, result | error}` / `Notification{method, params}` / `Request{id, method, params}`・サーバーからの要求（`window/workDoneProgress/create` など）は `MethodNotFound` で返す）。位置は `general.positionEncodings: ["utf-8", "utf-16"]` を申し出て、サーバーの `positionEncoding` が `utf-16` ならコードポイント → UTF-16 の桁の写しを core の `Utf16` の 1 本で行う。

6. **寿命と失敗。** `initialize` の答えまで何も送らない → `initialized` → 文書の通知（L1）→ `shutdown` → `exit`。パイプの EOF・`ERROR_BROKEN_PIPE`・子プロセスの終了は `ServerExited` / `ServerFailed`（値・例外にしない）。自動の再起動は **3 回まで**（L-3 の残り・hide の確認）で、超えたら 1 行知らせて止まる（`:set lsp` で手で再開）。応答の古さはタイマーの意図 `LanguageServerTick`（`AutosaveTick` と同じ形・`keeps_tab_walk` と `begin_intent` に「使う人の操作ではない」と書く・時計は adapters だけ）で数え、30 秒を超えた要求は `$/cancelRequest` を送って捨てる。

7. **設定（D60）。** `settings.v3` → **v4**（ADR 0114 と同じ移行の型・v4 不在時は v3 を読む・保存は v4 だけ）に `lsp`（`on` / `off`・既定 `off`）と言語ごとの `lsp.<language>.command`（実行ファイルのパスと引数・1 行）を足す。`:set lsp` / `:set nolsp` / `:set lsp?`（ADR 0051 の追補の `clipboard` と同じ request の形）と Ctrl+P の `:` の候補。**設定に書いたパスだけを起こす。** 文書の側（フォルダの設定ファイル・`.nib` など）から起動の指定を読まない。既定の表には clangd の行だけ（コメントアウト・L-5）。

8. **送らない文書。** 4 MiB を超える文書・`plain` の文書・言語に command が無い文書は `didOpen` を送らない（L-4・段 L1 で測って決める）。

9. **契約。** core: `--json`（parse / write の例と性質・RFC の例）・`--lsp-frames`（見出しの切り出し・途中・2 通・上限）・`--lsp-messages`（要求と答えの対応・位置の写し）。application: `--language-server`（替え玉 `ScriptedLanguageServer`: 起動 → `initialize` → 答え → `initialized`・失敗の値・再起動の上限・tick の古さ・設定の on / off で start / stop）。adapters: `tests/adapters/` に**替え玉のサーバー exe**（`nib_fake_language_server`・標準入力の枠を読んで決まった答えを返す数十行の C++・CTest に登録・時計を読まず終了は親の `exit`）で、パイプ・Job Object・EOF・取消を本物の OS で通す（セマフォで待つ・`TIMEOUT` で落ちる）。clangd や Node.js は CI に要らない。`verify_quiet_paths` に「打鍵・切り替え・保存で `LanguageServerPort` を呼ばない」（オフのとき）。速さ: オフは 8 本に退行なし（QLT-014・今の基準）。オンの `lsp-` のベンチは段 L1（D60・別の基準値）。

10. **限界（本 ADR の外）。** 診断の表示・補完・hover・定義（L1〜L3）。複数のサーバーの同時起動（言語ごとに 1 本・同時は 1 本まで・L1 で決める）。`window/showMessage` の表示（L1 で 1 行の知らせに）。TCP / socket の transport。設定モーダル（v4 の欄はモーダルの計画 #454 と合わせる）。

## 1 打鍵の道の費用

オフのときは打鍵の道に 1 つの分岐も足さない（`lsp` が `off` なら port を呼ぶ所に入らない）。オンのときの `didChange`（L1）は frame の終わりに 1 通に溜める（この ADR では決めない）。

## 強制

- core が OS に触れないこと（JSON・枠・対応は純関数）: **active**（`eng/symbols.py`）。
- スレッドとプロセスとパイプは adapters だけ: **active**（`eng/symbols.py` の `_beginthreadex` / `CreateProcessW` の検査・CPP-013）。
- port の呼び手がオフのとき 0 回: **planned**（`verify_quiet_paths` の欄・L0 の PR で active）。
- オフの 8 本に退行なし: **active**（QLT-014）。

## 結果

得られるもの: 診断・補完・定義を載せる土台。設定で切れて、既定の振る舞いと速さは変わらない。CI は替え玉のサーバーで本物の OS の道を通せる。
失うもの・残る穴: pump の待ちの配列が増える（窓の `pump_messages` の形を変える・1 か所）。サーバーの出す巨大な JSON（16 MiB 超）は切る。サーバーの起動の数秒は設計の外（L1 で合図の後に起こす）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| `Win32Worker` に同期の `ReadFile` を載せる | FIFO 1 本が止まる（device・列挙・退避）。ADR 0062 の形に反する |
| 読み専用の 2 本目のスレッド | ADR 0004 / 0062 の「1 本」を変える。OVERLAPPED と pump の配列で足りる |
| nlohmann / simdjson などの外部の JSON | 例外と `std::` の大量の使用で厳格集合を通らない見込み。LSP で要る形は小さく、手書き 500 行で足りる（推測） |
| TCP の transport | clangd / pyright は stdio が既定。Windows の firewall の問いが出る |
| 文書の側の設定（`.nib` / `.vscode`）からサーバーを起こす | 開いただけで任意の exe が走る。設定に書いたパスだけ |

## 追補（2026-10-28・PR #1021 の独立レビュー）: `stop` は待たず、`shutdown` / `exit` は controller が送る

- **決定 1 の `stop`**: adapter の `stop()` は `shutdown` も `exit` も送らない（それは controller の役・L1 で `shutdown` の答えを待って `exit` を送る）。`stop()` は書きの待ち行列を drain し、入力のパイプを閉じ、読みを drain し、出力のパイプを閉じ、子の process と job を「退く子」の枠に移して**即座に戻る**（UI スレッドで待たない）。退く子の process の handle は `waitable_handles()` に含め、pump が立ちを見たら `completed(handle)` で片付ける。2 秒の期限は `LanguageServerTick`（決定 6）で controller が `expire_retired()` を呼び、残っていれば `TerminateJobObject`。窓を閉じるときは job を閉じるだけ（`KILL_ON_JOB_CLOSE` が子を殺す・待たない）。
- **絶対パスだけ**: `ServerLaunch.executable` が絶対パスでなければ `ServerFailure::not_found`（決定 7 の「設定に書いたパスだけを起こす」を作業フォルダの相対の解決で崩さない）。
- **孫の継承**: 子の process が終わったら `TerminateJobObject` で子孫を片付け、出力の EOF を待たずに `ServerExited` へ進める。
- **枠の切り出し**: #1009 の `lsp_frames` が main に入るまで adapter の `ServerFramer` が自前で切る（2 本目の経路・#1022 で置き換える）。
- **`stop` で event の列を捨てる**（古い子の `ServerMessage` が新しい `ServerStarted` の前に混ざらない）。
- **後続**: パイプの DACL を自分だけに（今は名前の推測で先に繋がれても `pipe_failed` で安全に落ちる）。
