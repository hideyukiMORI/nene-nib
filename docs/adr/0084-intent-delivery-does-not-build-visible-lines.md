# ADR 0084 — 意図の反映値は行を作らず、描画と当たり判定でだけ frame を作る

- 状態: 受理（実装と性能の受理は別）
- 日付: 2026-10-09
- Issue: #322
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / ARC-011 / CPP-002 / QLT-001 / QLT-010 / QLT-012 / QLT-014 / ADR 0009 / ADR 0011 / ADR 0075

## 文脈

`EditorController::apply` は意図を適用するたびに `frame()` を返す。窓の `deliver` は文書名・失敗・設定・IME・閉じる要求などを読むだけで、本文の行は読まない。それでも全可視行の本文・表示文字列・桁の表・検索一致を作り、後の `WM_PAINT` で同じ行をもう一度作る。さらに変換中かだけを調べる打鍵経路も `frame()` を呼ぶ。

ADR 0011 の意図は、窓にまとめて届いた打鍵を `WM_PAINT` の一枚に畳むことである。行を作る仕事も同じ時点まで遅らせられる。SPECIFICATION D1〜D43 の操作・表示・IME・保存の意味は変えず、状態遷移とポート呼び出しを省略・延期しない。

## 決定

1. **製品の意図入口 `apply` は一つのまま、戻り値を `EditorDelivery` にする。** 現在の `settle_tab_walk_before` → `begin_intent` → `std::visit(accept)` は同じ順で即時に実行する。最後に `delivery()` を返す。キャッシュや dirty 印、行を作るかを選ぶ boolean は加えない。
2. **`EditorDelivery` は窓への反映値で、本文の行を含まない。** `appearance`、`mode`、`vim_mode`、`ime`、`composition`、`command_composition`、`document`、`settings`、`settings_failure`、`command_message`、`closing`、`close_request`、`operation_request` を持つ。これは毎回現在状態から作る所有されたスナップショットであり、文書・設定の新しい所有者でも、未完成の `EditorFrame` でもない。行・キャレットの桁・検索・タブの一覧・面の候補・status の文字列は作らない。
3. **共通する反映値の組み立ては `delivery()` の一か所。** `frame()` はそこから共通欄を取り、描画固有の行・キャレット・status・候補・タブ等を加えて既存の完全な `EditorFrame` を作る。`EditorFrame` の既存の欄とその意味は保つ。共通欄を state からもう一度独立に計算する実装は置かない。
4. **試験の「意図を流して完全な表示値を読む」は `apply_frame` という薄い組み合わせにする。** 実装は `apply(intent)` と `frame()` の呼び出しだけ。既存の単体・契約・fixture の `.apply` を機械的に `.apply_frame` へ移し、期待値と検査を変えない。製品の窓・起動時のファイル読込は `apply` を使う。Vim harness の `press_vim_key(s)` は従来どおりで、独立した編集アルゴリズムを増やさない。計測 probe は製品の `apply` を呼ぶので、前後で同じ意図列を比較できる。
5. **窓の `deliver` は軽い結果を反映する。** IME 取消、要求の退避、字体変更時の可視行数の更新、IME の構え、mode の記録、無効化、題名・告知、操作・終了の順を保つ。`close_request` / `operation_request` / `closing` は後続の `VisibleLines` や再入の前に退避する。IME 取消で再入しても現在の一段で止まる契約を変えない。字体変更時の前の pt、`body_lines`、保存・題名・外観など本文を読まない口も `delivery()` を読む。
6. **軽い問い合わせは既存の正典計算を共用する。** `is_composing()` は現在の composition の有無だけを返し、UI の打鍵・IME の判定を移す。値は `composing(frame())` と同じであることを試験する。タブを閉じる判断には `documents()`（既存 `tab_views` を呼ぶ）、面のクリックには既存 `command_palette_view()` を公開して用いる。これらを `delivery` に載せて毎打鍵コピーしない。`frame()` を必要とする製品の場所は描画と本文の当たり判定だけとし、後者では `LineView` を renderer の正典 hit test へ渡す。
7. **描く契機と画は変えない。** 最初の描画は寸法の意図を流してから `frame()`、通常は既存 `WM_PAINT` → `frame()` → renderer のまま。`invalidate` のまとめ方、全クライアント描画、Present、時計の位置は変えない。

## 検証と性能比較

- 対象の Debug build と clang-tidy、conformance / symbols / 差分整形を実行する。application の入口とその UI caller の変更なので、application（Vim fixture を含む）、tabs、operations、background-work、IME と incsearch の既存 scope を選ぶ。単に試験の呼び名を変更した scope を理由なく全件再実行しない。
- 新しい契約は、同じ意図列で `apply` の反映値と後続 `frame` の共通欄が一致すること、失敗・閉じる要求・操作要求の一意図寿命、本文と面の変換、設定変更、MRU / worker 完了を確認する。試験 wrapper が正典 `apply` を通ることも差分レビューする。
- #329 の同じ harness を変更前後に載せ、入力準備・最終全文確認を区間外に置き、200回の `apply(InsertText)` を固定 ABBA 順で比較する。長い行・16 MiB の可視行についても同じ目的の workload を先に固定してから測る。採用する数だけ選ばず全値を保存する。
- UI の前後比較は既存の22場面を一度だけ取り、打鍵・IME・検索・一覧・設定・タブの描画と要求の反映が変わらないことを確認する。変更が複数の表示値入口を横断するため、この範囲が直接の影響範囲である。以後は同じ実装の成功証拠を再利用する。
- main 統合には D41 / ADR 0075 の独立レビュー・関係する正式速度ゲート・CI・施主の試用が必要。閉じた比較は正式ゲートの置き換えではなく、既存の基準値と許容は変更しない。

## 却下

- `EditorFrame` の lines だけ空にして返す: 完全な描画値と未完成値が同じ型になる。
- `apply` の別実装や `build_lines` の切替引数: 状態遷移と観測の組み合わせを分ければ足り、製品の正典遷移は一つでよい。
- 前の frame を保持して無効化する: 行や検索の寿命に新しい状態を増やす。不要な生成をしないだけでよい。
- 状態遷移そのものを WM_PAINT まで遅らせる: 保存・IME・操作要求・終了の即時性を変える。

## 限界

反映値の小さなコピーと、実際に描く時の一回の行生成は残る。試験専用の `apply_frame` は反映値も完全な frame も作るため、製品の打鍵時間の測定には使わない。
