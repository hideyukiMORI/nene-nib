# ADR 0009 — 編集の縦切り: piece table の本文・UTF-8 の内部表現・undo の単位・編集中の状態の見た目

- 状態: 受理
- 日付: 2026-09-15
- Issue: #7
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / ARC-007 / ARC-010 / ARC-011 / CPP-001 / CPP-005 / CPP-007 / CPP-012 / CPP-014 / CPP-016 / CPP-017 / QLT-009 / QLT-013 / QLT-014

## 文脈

Issue #5 までの窓は 1 行の固定文字列を描く。エディタの正本＝本文がまだ無い。SPECIFICATION 第 3 節は piece table とメモリマップを土台に挙げ、
CPP-014 は「内部表現は Phase 3 の piece table の ADR で決める」としていた。編集中の状態の見た目は施主が
[キャンバス](https://claude.ai/code/artifact/f42a77d4-30f3-4867-8303-77686e2cf53c)で選んだ（[採用案](../design/2026-09-15-editing-look.md)）。

## 決定

1. **本文の正本は `core::TextBuffer`（piece table）ただ 1 つ**（ARC-001）。original バッファ（読んだファイル。今は空）と add バッファ（入力）の 2 本と、
   piece の列（どのバッファの、どこから、何バイト）で本文を表す。行索引は piece ごとの改行の位置を持ち、行の取得は行番号 → piece の走査で行う（1 万行で十分。
   1 GB の遅延構築とワーカーはメモリマップの縦切りで ADR 0004 に載せる）
2. **内部表現は UTF-8**（CPP-014 の確定）。位置は `Offset`（バイト）・`LineNumber`（1 始まり）・`Column`（code point・1 始まり）の専用型（CPP-001）で、
   境界でだけ相互に変換する。DirectWrite への UTF-16 変換は描く行だけ、描くときに行う。`WM_CHAR` の UTF-16（サロゲートペアを含む）は窓が UTF-8 へ変換して意図にする
3. **undo の単位は「連続した文字入力」。** 移動・削除・貼り付け・改行で区切る。undo の履歴は編集の逆操作（挿入 ↔ 削除）の列で、`EditorState` が所有する（ARC-004）。
   Vim の undo の区切り（挿入モードの出入り）は Vim エンジンの縦切りで同じ履歴に載せる
4. **選択は anchor と caret の 2 つの位置**で、Shift+移動が anchor を固定する。選択中の入力は削除 → 挿入を 1 つの undo 単位にする
5. **クリップボードはポート**（`ClipboardPort`。application が宣言し、adapters/win32 が Unicode テキストで実装。Loupe と同じ）。中核はクリップボードを知らない
6. **描画は見えている行だけ。** 縦スクロールの量は application が所有し（`ScrollState`。上限は core の純関数）、UI はホイール 3 行とキャレット追従の意図を出す。
   折り返しと横スクロールは持たない（長い行は切る）。1 行ごとに DirectWrite の layout を作って描き、キャッシュは QLT-014 の計測で必要が出てから
7. **編集中の状態の見た目は採用案のとおり。** キャレットは通常モードとINSERT がバー、NORMAL がブロック。選択・検索の当たり・IME の変換中・`:` の行は
   採用案の表（`search` / `ime` のトークンを `Palette` に足して 16 にする）。`:` の行は**案 A**（ステータスバーの左側が置き換わる）
8. 改行は読んだ形を保つ（ARC-009）。新規の本文は CRLF。文字コードはこの縦切りでは UTF-8 だけ（Shift_JIS の判別はファイルの縦切り・D8）
9. **意図は `std::variant` の閉じた和型**（`EditorIntent`。11 の選択肢。方向や操作の種類は閉じた `enum` を持つ型に畳む）で、`EditorController::apply` は `std::visit`。
   写し先が足りなければコンパイルが落ちる。`std::variant` は意味を持つ閉じた和型であり、CPP-006 が禁じる汎用データバッグではない
10. **Esc は窓を閉じない。** 本文が入るようになったので、Issue #5 までの「Esc で終了」は未保存の内容を失う道になる。通常モードでは選択の解除だけ、Vim では NORMAL への遷移（エンジンの縦切りで）
11. **クリックでキャレットを置く**（`PlaceCaret{TextPosition}`。Shift 併用で anchor を固定）。ドラッグ選択はまだ

## 強制

- QLT-009: piece table・行索引・undo・位置変換・選択は単体テストで境界（空・先頭・末尾・piece の境界・サロゲートペア・CRLF の途中）を厚く測り、分岐 90% を保つ
- QLT-013: キー入力の実機は `eng/verify-window.py` が `WM_CHAR` / `WM_KEYDOWN` を `PostMessageW` で送って本文と表示値を見る。CI は WARP で描くだけ
- QLT-014: **planned のまま**。1 MB の挿入と行索引が単体テストで 100 ms 未満であることだけを上限として確かめる（ベンチではない）
- ARC-011 / CPP-017: 窓はキーを意図に変えるだけで、本文に触らない（レビュー事項）

## 結果

- 得られるもの: エディタの正本、undo、選択、クリップボード、複数行の描画とスクロール。Vim エンジンと IME とファイルが載る土台
- 失うもの: 1 GB のファイルはまだ開けない（行索引を同期で作る）。折り返し無し。横スクロール無し。1 行ごとの layout 生成は速さの計測前
- 正直に記録しておくこと: piece table の行索引は piece 内の改行の配列で、巨大ファイルの遅延構築は設計に入れていない（ADR 0004 のワーカーの縦切りで）。
  `WM_CHAR` は IME を通らない入力だけで、変換中の表示（採用案の IME）は FR-012 の縦切り。語の区切りは空白だけ（記号の扱いは Vim の語の定義と一緒に）。
  1 打鍵ごとに `Present(1, 0)` で vsync を待つので、まとめて post した入力は 1 フレーム 1 打鍵で流れる（人の速度では問題にならない。描画のまとめ方は速さの縦切り・QLT-014 の検討事項）。
  `std::string_view::find` は MSVC STL の `__std_find_trivial_*`（純関数・ベクトル化された検索）を core の外へ出す。許可シンボルに足した（時刻・OS・スレッドに触れない）。
  手元の計測（`/O2`・使い捨て）: 1 MB の挿入 0.62 ms、1 万行 CRLF の読み込み 1.93 ms、全行の走査 0.77 ms、末尾への 1 万打鍵 3.25 ms、1.22 MB の中央への 1000 挿入 47.5 ms（piece 1002 本）。QLT-014 は planned のまま

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| gap buffer / rope | piece table は original を不変に保ち、メモリマップ（1 GB）と undo と相性がよい。施主の土台の指定でもある |
| 内部表現を UTF-16（DirectWrite に合わせる） | ファイルとクリップボードの往復で変換が要り、`std::string_view` の道具が使えない。描く行だけ UTF-16 にする方が変換量が少ない |
| undo を 1 文字ずつ | 使い勝手が悪く、Vim の区切りとも合わない |
| `WM_KEYDOWN` だけで文字を扱う | 配列とシフト状態の変換を自分で書くことになる。`WM_CHAR` が正典 |
| 折り返しをこの縦切りで | 行索引と桁の意味が変わる。描画の縦切りを分ける |
| `:` の行を案 B（ステータスバーの上） | 施主が案 A を選んだ |

## 関連

- [ADR 0004](0004-ui-thread-plus-one-worker.md)・[ADR 0005](0005-own-vim-engine-verified-against-real-vim.md)・[ADR 0008](0008-adopted-look-tabs-titlebar-statusbar-mica.md)
- SPECIFICATION 第 3 節・FR-002 / FR-008 / FR-013
