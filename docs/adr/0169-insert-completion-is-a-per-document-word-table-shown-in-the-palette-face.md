# ADR 0169 — INSERT の補完は文書ごとの語の表から、面は Ctrl+P の部品で、LSP の候補も同じ面に載る

- 状態: 受理（施主決定 D67・FR-024・2026-10-28。実装は 1〜2 席・受理は D41 の形・Vim の Ctrl-n の振る舞いは oracle で合わせる）
- 日付: 2026-10-28
- Issue: #1008・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §1 の 10・LSP L2 の前段（ADR 0161 の限界）
- 規則: FR-024、ARC-001 / ARC-003 / ARC-004 / ARC-011、CPP-002 / CPP-005 / CPP-011、QLT-014、ADR 0012（oracle）/ 0014（IME）/ 0015（INSERT の undo 1 単位）/ 0028（INSERT の反復）/ 0030（`.` の記録）/ 0060 / 0062 / 0150（面の候補・見えている行だけ・行の layout の保持）/ 0156（構文の覚えの更新の形）

## 文脈

Vim の INSERT の Ctrl-n / Ctrl-p（本文の語からの補完・`complete=.,w,b,u,t,i` の既定）はヴァニラの機能で、Nib に無い。LSP L2 の補完（ADR 0160 / 0161 の後）は同じ面に候補を足す形が自然なので、面と語の表を先に作る。D67: 語の表を打鍵ごとに作り直さない・面は見えている行だけ。

## 決定

1. **語の表は文書ごとの値。** `WordTable`（`DocumentState` に 1 つ・語（Vim の `iskeyword` の既定と `w` の語の分類で切る・ASCII と非 ASCII の語）→ 出現の数）。文書を開いたときに 1 回作る（16 MiB 超は作らない・補完は本文の語を使わず LSP だけ・限界）。編集は `replace` の 1 か所で**編集の行だけ**を読み直す（構文の覚え `SyntaxStateCache` と同じ形・ADR 0156・1 編集 O(編集の行の長さ)）。開いている他のタブの語（`w` `b` の既定）は、切り替えたときに表を束に残しているので和をとる（読んでいないタブは含めない）。
2. **候補の決め方（Vim と同じ）。** キャレットの直前の語の頭（`iskeyword`）を接頭辞に、同じ文書のキャレットの後ろから前方一致で集め（折り返して前へ）、次に他のタブ。順は Vim の `complete` の既定の順（現在の文書は位置の近い順・重複は 1 つ）。接頭辞が空（語の途中でない）なら全部の語（Vim と同じ）。Ctrl-n は次・Ctrl-p は前（最初の Ctrl-p は後ろから）。候補は 100 件まで（Vim の `pumheight` 相当は面の行数）。
3. **鍵と状態。** INSERT の Ctrl-n / Ctrl-p で面を開き、以後の Ctrl-n / Ctrl-p で選び、選んだ語を本文に即時に入れる（Vim と同じ・`<C-n>` のたびに本文が変わる）。Ctrl-e で元の接頭辞に戻して閉じる・Ctrl-y で確定して閉じる・Esc は確定して INSERT を抜ける（Vim と同じ）・ほかの文字は確定してその文字を打つ（Vim の既定）・Backspace は候補を絞り直す。面が開いている間も undo は INSERT の 1 単位（ADR 0015）。`.` の記録は打った鍵（Ctrl-n を含む・再生時に同じ語の表で同じ結果になるとは限らない・Vim も同じ・限界に書く）。
4. **面は Ctrl+P の部品。** `CommandPalette` の行の描画と保持（`PaletteRowLayouts`・ADR 0150 の追補）を使い、キャレットの下（下に入らなければ上）に幅 240 DIP・最大 8 行。候補の文字は `text`・選んだ行は `current_line`・出どころの印（本文 / 他のタブ / LSP）は `muted` の 1 文字。色のトークンは足さない。IME の変換中は開かない（ADR 0014・確定の後の Ctrl-n で開く）。
5. **LSP L2 の候補は同じ面に。** `CompletionSource`（閉じた enum: `buffer` `other_buffers` `language_server`）を候補に添え、L2 が `textDocument/completion` の答えを同じ列に足す（順は Vim の `complete` の順で LSP を先頭に・L2 の ADR で決める）。面と鍵と状態はこの ADR のまま。
6. **契約。** oracle の fixture（`insert-complete-` の接頭辞・`:normal!` で `i<C-n>` は Vim で動く・接頭辞・前方一致・近い順・Ctrl-p・Ctrl-e・Ctrl-y・Esc・文字で確定・重複・非 ASCII の語・空の接頭辞）と `--insert-completion`（語の表の増分の更新（性質の試験: ランダムな編集の列で表が本文から作り直した表と一致）・他のタブ・100 件・16 MiB 超は無し・面の位置・IME）。ui: `nib_window_tests` の面の画素。
7. **費用。** 語の表は編集の行だけ更新（1 打鍵 O(行の長さ)・空の文書で数百 ns の見込み・A/B を PR 本文に）。面は開いているときだけ描く。任意のベンチ `insert-completion-single-key`（面を開いた状態の 1 打鍵）。
8. **限界（後続）。** `complete` の設定・`completeopt`・辞書（`k`）・タグ（`t`）・Ctrl-x の副モード（Ctrl-x Ctrl-l の行・Ctrl-x Ctrl-f のファイル名）・スニペット（要らない・D70）。

## 強制

- 語の表が全文を打鍵ごとに走らない: **active**（性質の試験・契約）。
- 8 本の速さと +1% の線: **active**（QLT-014・PR 本文の A/B）。
- fixture: **active**（oracle・面の見た目以外）。

## 結果

得られるもの: Vim の Ctrl-n がそのまま効き、LSP の補完の面の土台ができる。
失うもの・残る穴: 16 MiB 超の文書は本文の語で補完しない。`.` の再生は語の表が違えば違う語を入れる（Vim も同じ）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 打鍵ごとに本文を走って語を集める | 16 MiB で止まる（D67） |
| 補完の面を新しく作る | Ctrl+P の面の行の描画と保持がそのまま使える（ADR 0150） |
| LSP の補完を別の面に | 2 つの面の鍵が衝突する。出どころの印で分ければ 1 つで足りる |
