# ADR 0167 — プロジェクトの全文検索は worker が塊ごとに読んで当たりを Ctrl+P の面に流し込む

- 状態: 受理（施主決定 D67・FR-026・2026-10-28。実装は 2〜3 席・前提はエクスプローラー（ADR 0162・ルート）と巨大 段 2（ADR 0159・読みの port の形）・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #1003・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §1 の 5
- 規則: FR-026、ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-011、CPP-002 / CPP-005 / CPP-011 / CPP-013、QLT-014、ADR 0032（`VimPattern`）/ 0060 / 0062（面の候補は 1 列・見えている行だけ・`FolderPort` と worker 1 本・券）/ 0150（面の行の保持）/ 0159（読みの port）/ 0162（ルート）

## 文脈

telescope の live_grep・fzf の `:Rg` は「プロジェクトの中の文字列を探して開く」で、Neovim の日常の中心。Nib の Ctrl+P は候補の列が 1 つで出どころを行頭の記号で絞り（ADR 0060）、同じフォルダの一覧は worker が裏で読む（ADR 0062）。D67: 全文を UI スレッドで走らない・取消できる・面は見えている行だけ。

## 決定

1. **入口は 2 つ、面は 1 つ。** Ctrl+Shift+F（通常と Vim・操作 `search_project`・F1 の一覧）と Ex `:grep {pattern}`（`:grep` の後の文字列を pattern に・区切り無し）。Ctrl+P の面に新しい出どころ `project_hits`（記号 `!`・`palette_marks` に 1 行・`PaletteScope` と `PaletteOrigin` に 1 値）を足し、入力行は「探す文字列」（面の絞り込みの文字列ではなく、サーバーへ送る検索語の役・入力が変わるたびに新しい券で探し直す・300 ms の間を置かず**打つたびに**要求し、前の券を捨てる）。
2. **範囲。** エクスプローラーのルート（`root`・ADR 0162）。無ければアクティブな文書のフォルダ（無題なら失敗の 1 行）。隠し属性のフォルダ・`.git` `node_modules` `build` `out`（固定の表・設定にしない・限界）は飛ばす。ファイルは 16 MiB 超と NUL を含む（最初の 8 KiB で判定）ものを飛ばす。
3. **探し方は core の純関数。** 検索語は literal（既定・大小を区別しない）か `VimPattern`（先頭が `/` なら `magic` の部分集合・ADR 0032）。1 ファイルの塊（1 MiB・行の境で切る）に対し `project_hits_in(chunk, pattern) -> std::vector<ProjectHit{line, column, line_text(256 バイトで切る)}>`。塊をまたぐ行は次の塊の先頭に繰り越す（最大 1 行・1 行が 1 MiB を超えればその行は飛ばす）。
4. **worker で塊ごとに。** `ProjectSearchPort{request(ProjectSearchRequest{root, pattern, ticket}), cancel(ticket), collect() -> std::vector<ProjectSearchBatch{ticket, hits, progress}>}`（`FolderPort` と同じ形・adapter が `FindFirstFileExW` で木を歩き、ファイルを 1 MiB ずつ読んで core の `project_hits_in` を worker の上で呼ぶ・1 塊 1 仕事で FIFO を塞がない・券が変われば残りの仕事は何もせず戻る）。当たりは 1,000 件で打ち切り（`progress = truncated`）。完了・途中の知らせは `WorkCompleted` → `collect`（新しい合図は足さない）。
5. **面の中。** 候補の 1 行 = `<相対パス>:<行>:<列>  <行の本文（当たりを `search` の面で）>`。絞り込み（面の 2 段目の文字）は無し（入力行は検索語）。Enter で `open_listed` の 1 本で開き、その行と列へキャレット（`PlaceCaret`・`''` を置く）。面の結果は `CommandPalette` の `Result` と同じく 1 回作って見えている行だけ（ADR 0062）。進み具合「探しています… 128 / 2,310 ファイル」を面の先頭の行に（`muted`）。
6. **費用。** UI スレッドは要求と `collect` と面の行の描画だけ。読みと照合は worker（1 MiB の塊で約 1〜3 ms の見込み・FIFO に自動退避や device の仕事が並んでも塊の間で譲る）。面を閉じると `cancel`。任意のベンチ `grep-root-1000-files`（1,000 ファイル × 10 KiB で最初の当たりまで・全部終わるまで・UI の 1 打鍵の増分を同時に測る）。
7. **契約。** core: `--project-search`（literal と pattern・大小・塊の境の行・1 MiB 超の行・NUL・256 バイトの切り詰め・性質の試験: ランダムな本文を塊に切って探した当たりが全文を 1 回で探した当たりと一致）。application: 替え玉 `ScriptedProjectSearch`（券・途中の batch・取消・1,000 件・`open_listed` と位置）。adapters: 一時フォルダに 100 ファイルの木を作って本物の OS で（セマフォ・`TIMEOUT`）。`verify_quiet_paths`: 打鍵・切り替え・保存で port 0 回。
8. **限界（後続）。** 置換（`:grep` の結果への `:s`）・除外の設定（`.gitignore` の読み）・結果の面の絞り込み・複数の根・ファイル名での絞り込み（Ctrl+P の `/` と `#` で足りる）・インデックス（毎回読む・1 GB の木では遅い）。

## 強制

- UI スレッドで本文を読まない・照合しない: **planned**（port と controller が入る段 G2 の `verify_quiet_paths` と契約で active にする。今は `eng/symbols.py` が core の照合の純関数の純粋性だけを守る）。
- 面の候補は 1 列・見えている行だけ: **active**（ADR 0062 の契約の形）。
- 8 本の速さ: **active**（QLT-014）。

## 結果

得られるもの: ルートの下の文字列を打ちながら探し、当たりを選んで開ける。
失うもの・残る穴: インデックスが無いので大きな木では毎回読む（1 GB の木で数秒〜）。除外は固定の表。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 外部の ripgrep を子プロセスで | 配布に exe を抱える・設定に書いたパスだけを起こす規則（ADR 0160）と相性が悪い。自前の読みで足りる |
| インデックスを持つ | 外で変わるファイルの追従と置き場の設計が要る。後続 |
| 300 ms の間を置いて要求 | 時計は adapters だけ（タイマーの意図が要る）。打つたびに券を捨てれば worker の無駄は塊 1 つ分 |

## 追補（2026-11-16・#1758 / PR #1760・段 G1 の core）

独立レビュー V-1760 の結論と設計席の決めを受けて、段 G1 で形を決めた所を書き足す。

1. **literal も `VimPattern` の 1 本で照合する（ARC-012）。** 検索語の literal は通常モードの Ctrl+F と同じ `literal_pattern` で逃がし、`VimPatternCase::ignore`（畳み方は `vim_folded`）の `VimPattern` にする。当たりは `:set ignorecase` のときの Ctrl+F と同じ（全角の英字・Kelvin の `K`・`ſ` も畳む・結合文字の付いた字の途中に当たらない）。`ignorecase` の設定は読まない。先頭が `/` の pattern は今のまま（区別する・`\c` で畳む）。塊を照合する前の絞り込みのバイト列は `VimPattern::required_bytes`（`VimPatternNeedle{bytes, ascii_folded}`）が持つ: 量の付かない literal の原子の最も長い連続で、大小を畳む原子は畳みの仲間を ASCII だけを畳む比較で見分けられないとき（`vim_folded_ascii_comparable`・表は生成物 `CaseMappingTable.hpp` の `case_fold_shared`・`k` と `s` と ASCII の外の大小のある字は列を切る）だけ入れる。絞り込みが当たりを落とさないことは契約の性質の試験が守る。この口は全文検索だけが呼び、`/`・Ctrl+F・`hlsearch` の照合の道からはまだ呼ばない（その速さは #1619 / #1567 の側・8 本の速さのゲートに触れるため）。
2. **当たりの形。** `ProjectHit{line, column, line_text, text_begin, match_begin, match_end}`。`line` は 1 始まり（`\n` で区切る）、`column` は code point の 1 始まり（行の全体で数える）で、`PlaceCaret` の `TextPosition` へそのまま渡す。本文は行末の CR とファイル先頭の UTF-8 の BOM を除いた形。`line_text` は最大 256 バイト・正しい UTF-8・code point を割らない窓で、当たりの終わりが行頭から 256 バイトを越えるときだけ、当たりの前に 64 バイトを残して当たりの側へ寄せる（始まりは code point の境へ戻す）。`text_begin` は窓が行頭から何バイト目か（正なら application が省いた印を足す）、`match_begin` / `match_end` は窓の中の当たり（バイト・半開・窓の終わりで切る）。1 行に当たりは 1 つ（最も左・Vim の `:vimgrep` の g の無い形）。塊は `project_hits_in(scan, chunk, query, room)` と終わりの `project_hits_finished(scan, query, room)` の純関数で、状態 `ProjectScan` を値で受け渡す。
3. **1 MiB の塊の時間（決定 6 の見込みの書き換え）。** Release /O2・ソースらしい CRLF の行・room 1,000・31 回の中央値（ほかの席が動く機械・1 回限りの測り）: 絞り込みのバイト列の在る検索語は 0.1〜1.2 ms（`needle` 1.2・`NEEDLE` 1.1・`/needle` 0.14・`/ne\+dle` 0.20・`/\cneedle` 1.1・当たり無しの `zzzz` 0.22）。絞り込みのバイト列が短い・よく現れる・無い検索語は遅い（`desk` は `de` で 5.1、`kiss` は `i` で 11.1、`sks` は絞り込み無しで 12.0、`/^\s*return` は 15.1）。見込みの「1〜3 ms」は絞り込みの効く検索語だけの値で、効かない検索語は 1 塊が 5〜15 ms になる（worker の上なので UI は止まらない・FIFO の譲りは塊の単位のまま）。
4. **限界に足す。** 正しい UTF-8 でない行（Shift_JIS などのファイル）は照合せず当たらない（文書を開く道の文字コードの判定は使わない・後続）。
