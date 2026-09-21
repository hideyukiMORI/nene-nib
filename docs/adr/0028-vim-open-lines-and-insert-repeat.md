# ADR 0028 — 行を開く操作と回数付きINSERTは既存の挿入・履歴へ流す

- 状態: 受理
- 日付: 2026-09-20
- Issue: #79
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈と実測

FR-003のNORMAL `o` / `O`を実装する。既存のINSERT、行索引、改行変換、EditHistory、IME確定入力を使う。固定Vimのhelpと実測を根拠にし、Vimソースは読まない。

`out/issue79-oracle/fixture-results.json`の36件と追加4件を一度ずつ測定した。各measureは既存の通常/+Esc比較を含む。`do`はdiff操作となり未設定のdiff環境でVimがexit 1を返したため採用せず、後続`dO`は測っていない。

- `3o` / `3O`も最初に開くのは1行。Escで入力を回数分に展開し、どちらも最後の複製の末尾に置く。
- 反復する入力は開いた改行を先頭にしたLFの文字列。Enterを含む複数行も反復する。
- Backspaceは直前の入力を除く。開いた行を結合すると先頭の改行も消え、`3o<BS>X<Esc>`は`aaXXX`となる。さらに元の本文を消すと反復を取り消し、`3o<BS><BS>X<Esc>`は`aX`となる。先頭で効かなかったBackspaceは反復を変えない。
- Home / Endは移動距離が0でも反復を取り消す。矢印・ページ移動もINSERTの移動という同じ境界にする。
- autoindentは固定oracleの既定で無効。VISUALの`o`と`O`は、今回対応する文字・行選択では共に端点交換。

## 決定

1. NORMALの表に `open_line_below` / `open_line_above` を置く。VISUALでは同じactionを端点交換へ写し、旧 `swap_visual_ends` actionを置き換える。次文字待ちは表より先に処理する既存経路を維持する。
2. `VimPutString`を `VimInsertAt` に改名し、既存 `EditBoundary` を追加する。挿入位置・LFの本文・LF換算のcaret・履歴境界を一度に返す。p/Pはseparate、開く操作とEsc時の反復はabsorb。同じcontrollerの改行変換とreplaceに流し、旧型を残さない。
3. `VimState`に `optional<VimInsertRepeat>` を持つ。型は残り回数とLFの入力記録だけを持ち、本文や履歴は所有しない。o/Oの回数が2以上のときだけ作り、文字・Enter・有効なBackspaceで更新する。入力記録を越える削除、移動、モード切替、Escで破棄する。一般のi/aの回数やdot用記録には広げない。
4. Escでは残りの入力を現在位置へ `VimInsertAt` で一度に挿入し、NORMALへ戻る。モードがINSERTへ入るときは効果の前、INSERTを出るときは効果の後に履歴を閉じる。反復も最初の開行・入力と同じundo単位にする。空の反復入力は追加挿入しない。
5. `EditHistory::absorb`は既に挿入した範囲内の挿入・削除・置換も畳む。Oが挿入した改行の手前に文字を入れるために必要な一般の編集合成であり、専用の履歴を増やさない。通常入力のcoalesceは変えない。INSERTの `VimMoveTo` は距離に関係なく履歴を閉じる。エンジン外のcaret移動・全選択・履歴操作も同じcontrollerの中断処理を使い、非absorb編集は反復記録を解除する。記録していない編集の後に古い入力を展開しない。保存が履歴を閉じる既存契約は維持する。
6. 共通の文字列反復は積を計算する前にstringのmax_sizeとの除算で検査する。表現不能な反復は型付きfailureを返す。p/Pは何も貼らず、o/Oは初回の入力を保ってEscを完了する。実メモリ量の予算は設けない。
7. UIのキー配送・IME・描画・保存形式は変更しない。o/Oも既存INSERT状態を返すので、IME許可・キャレット・スクロール追従は同じ経路に従う。

## 限定検証

新規実Vim fixtureと途中のINSERT状態・CRLF元bytes・undo/redo・移動取消・IME確定を確認する。共有の挿入効果を使う既存p/PとINSERT履歴、変更したEditHistoryのcoalesce/absorbだけを直接の回帰対象にする。固定431件の既存fixtureは生成ソース・metadata・入力を照合して逐語再利用し、測定し直さない。

変更targetのDebug build/tidy、core/applicationのsymbols、conformance、変更C++のformatを確認する。nativeはo/O、回数付き入力、CRLF保存とundo、INSERT表示・追従に限定する。全件検証・無関係なテーマや性能測定は実行しない。

## 範囲外と却下

一般のi/a回数、dot、autoindent/smartindent設定、未対応INSERT制御キー、矩形選択、Issue #77は範囲外。反復のためのキー再配送・再帰的controller呼び出し、効果の汎用列、別の本文やundo管理は追加しない。既存の挿入効果と編集合成で表現できるためである。

## 補足（2026-09-22・Issue #87）

決定 3 の「一般の i/a の回数には広げない」と、範囲外に挙げた「一般の i/a 回数」「dot」は [ADR 0030](0030-vim-dot-repeat-as-key-replay.md) が置換した。`.` の回数付き再生（`2.` が `2ifoo<Esc>` を流す）が成り立つには回数付きの `i a I A` も入力を反復する必要があり、固定 Vim 9.1 でも `3ifoo<Esc>` は `foofoofoo` になる。

そこで `VimInsertRepeat` は o/O だけでなく `i a I A` の入りでも立つ。形も破棄の境界（移動・記録を越える削除・外部編集）も決定 2・3 のままで、`counted-insert-*` の 5 fixture が固定 Vim との一致を守る。o/O 以外の INSERT 制御キー・矩形選択は引き続き範囲外。
