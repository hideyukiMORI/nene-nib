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

- UI スレッドで本文を読まない・照合しない: **active**（`verify_quiet_paths`・契約・`eng/symbols.py` は core の純粋性）。
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
