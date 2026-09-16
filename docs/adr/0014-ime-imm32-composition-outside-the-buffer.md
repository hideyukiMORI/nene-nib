# ADR 0014 — 日本語入力は IMM32 を ui/win32 が直接受け、変換中の文字列は本文の外に持って描き、Vim の NORMAL では IME を切る

- 状態: 受理
- 日付: 2026-09-17
- Issue: #28
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / ARC-010 / ARC-011 / CPP-002 / CPP-006 / CPP-011 / CPP-017 / QLT-009 / QLT-013

## 文脈

FR-012 は「日本語入力（IMM32 / TSF）。変換中は Vim の鍵を奪わない」。いまは `WM_CHAR` に来た確定文字だけが本文に入り、IME の既定の変換窓が本文と無関係な位置に出る。
編集の縦切り（ADR 0009）で本文の正本は `TextBuffer` ただ 1 つ、undo は `EditHistory`、意図は閉じた和型。Vim の縦切り（ADR 0012）で NORMAL / INSERT が入り、窓は Vim モードのとき鍵を `VimKeyPress` に写す。
`eng/architecture.json` の ui_win32 の platform library には `imm32` が Phase 1 から載っている（憲章は IME を ui/win32 に置く前提で書かれている）。
見た目は採用案（D15）で決まっている: 注目文節は `accent` の 2 DIP の下線と淡い面、他の文節は `ime` の下線と字色。

## 決定

**IMM32 の `WM_IME_*` を ui/win32 が直接受け、変換中の文字列は `EditorState` が `std::optional<Composition>` として本文の外に持ち、renderer がキャレット位置へ差し込んで描く。確定は `CommitText` の 1 意図で本文に入る。Vim の NORMAL では IME を切り、変換中の鍵は IME に任せる。**

1. **IMM32 を ui/win32 が使う。ポートは作らない**（ARC-007 の例外区画は時刻・乱数・ロケール・環境変数・ファイル・スレッド。IME の変換文字列は `WM_CHAR` と同じ「窓への入力」）。TSF は使わない: IMM32 は TSF の上で動き、Windows 11 の日本語 IME でも `WM_IME_COMPOSITION` は届く。TSF 直接は COM の sink（`ITfContextOwnerCompositionSink` 等）が要り、初版の価値に合わない
2. **変換中の文字列は本文ではない。** `core::Composition`（UTF-8 の文字列・文節の列 `CompositionClause{OffsetRange; ClauseEmphasis}`・変換中のキャレットの `Offset`）を `EditorState` が `std::optional` で持つ。`TextBuffer` と `EditHistory` は確定まで変わらない（ARC-004）。`ClauseEmphasis { target, other }` は閉じた enum
3. **意図は 3 つ**（`EditorIntent` に足す）: `ComposeText{Composition}`・`CommitText{std::string}`・`CancelComposition`。窓は `WM_IME_STARTCOMPOSITION` と `WM_IME_COMPOSITION` を `DefWindowProcW` に渡さず（IME の既定の変換窓を出さない・確定文字を `WM_CHAR` に流さない）、`ImmGetCompositionStringW` の `GCS_COMPSTR` / `GCS_COMPATTR` / `GCS_COMPCLAUSE` / `GCS_CURSORPOS` を `Composition` に、`GCS_RESULTSTR` を `CommitText` に写す。`WM_IME_ENDCOMPOSITION` は変換が残っていれば捨てて `CancelComposition`
4. **確定文字の入り方**: 通常モードは `CommitText` → `InsertText` と同じ `replace`（1 つの undo 単位・`coalesce` は直前の打鍵と繋がる。ADR 0009 の決定 3）。Vim の INSERT では確定した文字列を code point ごとに `vim_step` に `VimCharacter` で流す（Vim も IME の結果を打鍵として見る。`.` の再生・マクロが後で自然に載る）。Vim の NORMAL では `ComposeText` / `CommitText` を捨てる（IME は切ってあるので通常は来ない）
5. **Vim の NORMAL では IME を切る**: INSERT → NORMAL の遷移で `ImmGetContext` → `ImmGetOpenStatus` を控えて `ImmSetOpenStatus(FALSE)`、NORMAL → INSERT で控えた値を戻す（gVim の `iminsert=2` の体験）。通常モードでは IME の開閉に触らない。**変換中の鍵は IME が食う**: `WM_KEYDOWN` は `VK_PROCESSKEY` で来るので `KeyVimSpecial` の表に無く Vim の鍵にならない。Esc は変換の取り消しで、NORMAL への遷移ではない
6. **候補窓の位置**: renderer が最後に描いたキャレットの物理画素の矩形を `caret_rectangle()` で返し、窓が `ImmSetCandidateWindow(CFS_CANDIDATEPOS)` で候補窓をキャレットの直下に置く。`WM_IME_STARTCOMPOSITION` と変換のたびに更新。`ImmSetCompositionWindow` は使わない（変換文字列は自前で描く）
7. **見た目は採用案**: 注目文節（`ATTR_TARGET_CONVERTED` / `ATTR_TARGET_NOTCONVERTED`）は `accent` の 2 DIP の下線と `selection` と同じ 28% の面、他の文節は `ime` の 1 DIP の下線と `ime` の字色。変換中のキャレットは `GCS_CURSORPOS` の位置にバー。文節 → 下線の矩形の列は core の純関数（テストできる）
8. **速さ**: `WM_CHAR` の経路は変えないので 1 打鍵の遅延は変わらない。変換中の描画は `InvalidateRect` → `WM_PAINT` の同じ 1 本（ADR 0011 の決定 6）

## 強制

- CPP-002 / コンパイル: `EditorIntent` の写し先・`ClauseEmphasis` の `switch` — **active**（既存）
- ARC-004: `ComposeText` の間 `TextBuffer` と `EditHistory` が変わらないことを単体テストで — **active**（この Issue で）
- QLT-013: 実機の IME（変換・確定・候補窓の位置・NORMAL で切れる・変換中の Esc）は `eng/verify-window.py` の節（`SendInput` で IME を on にして打つ）か、取れなければ gate-proofs 5-h の手動確認 — **active**（記録の場所として）
- **不能**: IME ごとの `GCS_COMPATTR` の癖（Google 日本語入力・ATOK）。Microsoft IME で実測し、他は利用者の報告で

## 結果

得られるもの: 日本語が打てる。変換中の文字列が本文の位置に採用案の見た目で出て、候補窓がキャレットの下に来る。Vim の NORMAL で IME が邪魔をしない。確定 1 回が undo 1 単位。
失うもの: TSF 直接でできる再変換（`IMR_RECONVERTSTRING`）・確定前の文字列の取得はやらない。IME の on/off の表示（ステータスバー）はまだ。
正直に: `WM_IME_COMPOSITION` を `DefWindowProcW` に渡さないので、IME の既定の変換窓に頼る IME（古い IME）では変換文字列が見えなくなる可能性がある。Microsoft IME（Windows 11）で実測する。Vim の INSERT で確定文字列を code point ごとに流すので、長い確定は意図が文字数ぶん走る（`WM_PAINT` で 1 フレームに畳まれる。速さは実測で見る）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| TSF（`ITfThreadMgr`）を直接使う | COM の sink と文書ロックの実装が要り、IMM32 で同じ結果が得られる。再変換が要るときに置き換える |
| 変換中の文字列を `TextBuffer` に仮に挿入して確定で置き換える | 本文の正本に確定前の文字が入り、undo・保存の印・ファイルの保存が汚れる（ARC-004 / ARC-009） |
| `WM_IME_COMPOSITION` を `DefWindowProcW` に渡して確定文字を `WM_CHAR` で受ける | IME の既定の変換窓が出て採用案と食い違う。確定文字が 1 文字ずつ `WM_CHAR` で来て通常モードの undo 単位が打鍵と混ざる |
| IME の入力をポート（adapters）にする | IME は窓のメッセージそのもので、窓を持たない adapters に置くと第 2 の窓の経路が要る（ARC-001）。`imm32` は憲章で ui_win32 の library |
| Vim の NORMAL でも IME を切らない | NORMAL で日本語を打つと Vim の鍵として捨てられて意味が無い。gVim と同じく切る |

## 関連

ADR 0009（本文の正本・undo）・ADR 0012（Vim の鍵の写し・NORMAL / INSERT）・採用案 `docs/design/2026-09-15-editing-look.md`（`ime` トークン・D15）。
