# イージーモーションの計画（案）

2026-10-28 / hide の指示（外から・「イージーモーションも導入できる？2 バイト文字対応」）/ 設計席（Nibリナ）。
vim-easymotion（`<Leader><Leader>w` で見えている範囲の跳び先に印を出し、印の鍵を打つとそこへ跳ぶ）と同じ振る舞いを Nib の Vim に入れる。plugin の機能なので **本物の Vim の oracle では測れない**（fixture は作れない・契約と性質の試験で守る）。SPECIFICATION に無い機能なので FR と D 番号が要る（§4）。

## 0. 要点

| 問い | 設計席の決め |
|---|---|
| 鍵 | **`\\`（バックスラッシュ 2 回）の後に動作の鍵**。Vim の既定の `mapleader` は `\` で、vim-easymotion の既定は `<Leader><Leader>w`。Nib の Vim の表で `\` は空いている。NORMAL と VISUAL（選択を伸ばす） |
| 動作 | `\\w` `\\b`（語の頭・前後）・`\\e`（語の尾）・`\\j` `\\k`（下・上の行の最初の非空白）・`\\f{文字}` `\\F{文字}`（前・後の文字）・`\\s{文字}`（前後両方）・`\\l`（見えている全部の語の頭） |
| 2 バイト文字 | 跳び先は Vim の語の分類（`w` と同じ表・漢字とかなの切れ目）で日本語の中にも出る。印は跳び先の字の升に重ねる（全角は 2 升で 2 文字の印がそのまま入る・半角に 2 文字の印は次の升に 1 文字はみ出す＝vim-easymotion と同じ）。`\\f` の文字は IME で打てる 1 文字の入力行（`/` の行の IME と同じ構え・D57） |
| 印の文字 | `asdghklqwertyuiopzxcvbnmfj;`（vim-easymotion の既定）。跳び先が 27 以下なら 1 文字・超えたら 2 文字の組（1 文字目で組を絞る）。印は `accent` の地に `on_accent` の字 |
| 範囲 | 見えている行の中（アクティブな枠）。折り返した行の画面の外の部分は初版では対象（限界に書く） |
| 取消 | Esc・印に無い鍵（ビープ・何もしない）。跳びは `''` を置く（jump）。回数は受けない。`.` には影響しない |
| オペレータ | `d\\w` などの operator-pending は初版の外（後続） |
| 工程 | 4 段・ADR 1 本・3.5〜4 席日（推測） |

## 1. 今ある土台

| 項目 | 現物 |
|---|---|
| 鍵の表と接頭辞 | `VimKeyTable.cpp`（NORMAL / VISUAL の表）・`VimPrefix`（`g` `r` `q` `@` `"` `z` `Ctrl-w` `m` `'` `` ` ``）・`VimState.input_wait`（文字検索・接頭辞・テキストオブジェクトの待ち）。`\` は表に無い |
| 語の分類 | `VimWordMotion.cpp` の表（Vim の `utf_class_tab` の写し・#984 で全角の数字を直した・#985 で残り） |
| 見えている範囲 | `VimEditorView`（本文・選択・表示領域・画面行の問い `row_request`・ADR 0129 / 0144） |
| 強調の面 | `LineView.matches` / `current_match`（ADR 0037）・incsearch の preview（engine の外・ADR 0041） |
| IME の入力行 | `/` `:` の行で IME が打てる（D57・ADR 0061 の `ImeStance::as_left`） |
| 印の描画 | キャレットの枠（`accent`）・置き換え文字の tint・IME の下線。字形の run を切る描画（ADR 0156） |

## 2. 形（ADR で確定する）

- **core**: `EasyMotion`（純粋な値・状態機械）。`easymotion_targets(view, kind, 文字)` が見えている範囲の跳び先（offset の列・本文の順）を返し、`easymotion_labels(count)` が印の列（1 文字 / 2 文字・重複なし・接頭辞が他の印と重ならない）を返す。`easymotion_step(state, key)` が印の鍵を 1 つ受けて「絞る / 跳ぶ（offset）/ 取り消す」を返す。`VimState` に `std::optional<EasyMotionState{kind, targets, labels, typed}>` を 1 欄（命令の途中の状態・`vim_resting_from` で消える側）。鍵の表に `\` を接頭辞 `VimPrefix::leader`（1 回目）→ `leader_leader`（2 回目）で足し、2 回目の後の 1 鍵が動作（表に無い鍵はビープ）。`f` `F` `s` は文字の待ち（`VimInputWait` の新しい値・IME の入力行を開く印を `VimStep` に添える）。
- **application**: 跳び先と印は `LineView` に `std::vector<core::MotionLabel{Column column; std::string label; std::uint8_t cells;}>` として見えている行に写す（`colors` と同じ道）。`\\f` の文字の入力行は `CommandInput` の 3 つ目の種類（1 文字で確定・IME の確定文字列は最初の 1 文字・Esc で取消）。跳びは `VimMoveTo` の効果（既存）に `''` の印を添える（jump）。VISUAL では選択を伸ばす（既存の移動と同じ）。
- **ui**: renderer は見えている行の `MotionLabel` を字形の後に描く（`accent` の地の矩形 = 印の文字数 × 升の幅・`on_accent` の字・升は `column_advance` の表）。印が出ている間はキャレットは描く。文字の入力行の IME はステータスバーの行（`/` と同じ位置）に出す。
- **費用**: 跳び先の探索は見えている行だけ（60 行 × 100 文字で数十 µs の見込み）。印が無いときの打鍵の道に分岐を足さない（`\` の 1 回目で表を引くだけ）。

## 3. 施主に確かめる点

| # | 問い | 設計席の推し |
|---|---|---|
| E-1 | 初版の範囲に入れるか（FR-023・D67） | 入れる |
| E-2 | 鍵 | `\\` の後に動作（上の表）。別案: `s`（vim-sneak 風・2 文字の検索）は #768 の `s`（1 文字の置換）と衝突するので採らない。別案: `<Space><Space>` は Nib の Space が `l`（ADR 0049）なので採らない |
| E-3 | 動作の集合 | `w b e j k f F s l` の 9 つ。`t T` `n N` `L` は後続 |
| E-4 | 印の文字 | `asdghklqwertyuiopzxcvbnmfj;`・27 超は 2 文字の組 |
| E-5 | `\\f` の文字の入力 | IME の 1 文字の入力行（日本語の文字も跳び先にできる） |
| E-6 | オペレータ（`d\\w`） | 後続 |
| E-7 | 順 | エクスプローラー（E2〜E4）の後。巨大 段 2 と LSP L0 とは触るファイルが違う（engine と renderer）ので並行してよい |

## 4. 工程

| 段 | 中身 | 席日（推測） | ADR | 前提 |
|---|---|---|---|---|
| M1 | ADR 0163（§2 を決定に・限界に oracle 不能） | 設計席 0.5 | **1** | E-1〜E-6 |
| M2 | core: `EasyMotion` の状態機械・跳び先・印の割り当て・鍵の表の `\`・契約 `--vim-easymotion` と性質（印は重複なく接頭辞が他の印と重ならない・跳び先は本文の順・日本語の語の切れ目） | 1.5 | M1 | 分割（#992・`VimStep` の分割は後でもよい・`VimStepMotions` を触る） |
| M3 | application: `LineView.labels`・文字の入力行（IME）・`VimMoveTo` と `''`・VISUAL・`verify_quiet_paths` | 1 | M1 | M2 |
| M4 | ui: 印の描画・`verify-window.py --easymotion`（印の PNG）・試用 | 1 | M1 | M3 |

計 3.5〜4 席日。受理は D41 の形（独立レビュー・8 本の速さに退行なし・CI・試用）。

## 5. 残る不確かさ

- 折り返した行の画面の外の部分（論理行は見えているが画面行が下にはみ出す）を跳び先に含めるか。初版は含める（限界）。格子表示（巨大な行）では印を出さない。
- 印が 2 文字で半角の字に重なると次の字が隠れる（vim-easymotion と同じ）。見た目の確認は試用で。
- `\` を接頭辞にすると、将来 `\` を別の鍵（Vim の `\` は既定で何もしない）に使えない。問題なし。
