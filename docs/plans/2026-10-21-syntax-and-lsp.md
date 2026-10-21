# 構文の色分けと言語サーバー（LSP）の導入可否と計画

2026-10-21 / Issue #755 / hide の指示（2026-10-20「コードシンタックスカラー、ラングエッジサーバーが導入可能かの検討」）/ 設計席（Nibリナ）。
調査は `D:\NeNeNib\outputs\20261021\survey-syntax-lsp.md`（調査席・main `d2d5b60` を読むだけ・一次資料は LSP 3.17 / clangd / tree-sitter / Oniguruma の公式）。「推測」と書いた数字は実測していない。

この計画は可否の結論と作業の切り分けで、SPECIFICATION（§6「シンタックスハイライトの範囲」）・ADR 0017 / 0117（テーマの構文色）・[初版の範囲案](2026-10-12-release-scope-and-large-file-probe.md)の構文色の行を置き換えない。LSP は SPECIFICATION に無い新しい機能で、始めるなら FR と D 番号が要る。

## 0. 結論

| 問い | 結論 |
| --- | --- |
| 構文の色分けは可能か | **可能**。SPECIFICATION §6 の方針（見えている範囲だけ・自前の字句解析器・行頭の状態・巨大は自動オフ）のとおりに作れる。土台（`SyntaxPalette` の 10 役割が 125 テーマで埋まっている・`LineView.matches` と同じ区間の形・保持した字形 run を切る描画の先例）は揃っている |
| 字句解析の形 | **手書きの字句解析器**（core の純関数・言語ごと・行の終わりの状態 1 値）。tree-sitter と TextMate は退ける（§1.1） |
| 言語サーバーは可能か | **可能・ただし段階**（0: 前提 → 1: 診断だけ → 2: 補完 → 3: hover と定義）。規則に反する点は無いが、前提が 2 つ（OVERLAPPED のパイプを製品の port にする・core の最小の JSON）。最初のサーバーは **clangd**。**設定で切り替え・既定オフ・オンの速さは別の基準（D60）** |
| 順番の推し | split（試用待ち）→ D48 の分割 → 大容量 段 1 → **構文の色 段 1〜3（C/C++）** → Markdown プレビュー 段 1〜3 → 大容量 段 2〜6 → Markdown 段 4〜6 → 構文の残りの言語 → **LSP 段 0〜1**。LSP の診断は構文の色と同じ区間の形に載るので、構文の後 |
| 工程 | 構文: 4 段・ADR 2〜3 本・10〜12 席日。LSP: 4 段・ADR 4〜5 本・12〜15 席日（推測） |

## 1. 構文の色分け

### 1.1 字句解析の候補

| 候補 | 判定 | 理由 |
| --- | --- | --- |
| (a) 手書き（core・言語ごと・`lex_line(language, state_in, line) -> {spans, state_out}`） | **採る** | core に置ける（シンボルは memcpy 程度）。分岐は表（CPP-012）で `default` 無し（CPP-002）。fixture を Nib の中で作れる。1 行の費用は状態機械で 2〜10 ns/バイト（推測）|
| (b) tree-sitter | 退ける | C++ の文法だけで生成物 `parser.c` が 25.8 MB。既定の allocator は失敗で `fprintf(stderr)` と `abort()`（ARC-010 と相容れない）。Windows では `Interlocked*` と stdio を持ち込むので core に置けず、adapters＋port になる。色だけなら構文木は要らない。外部を入れるなら md4c と同じ 3 点セット（ADR・許可リスト・規約の表）が言語ごとに要る |
| (c) TextMate の文法 | 退ける | Oniguruma が要るが、Oniguruma は 2025-04-24 に開発を終えている。自作すると Nib の `VimPattern` に続く 2 本目の照合器で ARC-001 に反する |

Markdown プレビュー（md4c・描画）と Markdown のソースの色分け（手書きの行の字句器）は役目が違うので 2 経路にならない。ADR に 1 行書く。

### 1.2 区間の載せ方と描画（設計席の決め）

- `LineView` に `std::vector<ColorSpan> colors` を 1 欄足す。`ColorSpan{core::Column begin; core::Column end; SyntaxRole role;}`（本文の桁・`matches` と同じ形・公開 aggregate で比較は非メンバー・1 ファイル 1 型）。`SyntaxRole` は閉じた enum 10 値で、`switch` で `SyntaxPalette` の欄へ写す。格子の画面行は `GridLineRows` が `matches` を移すのと同じ写しを足す。
- 描画は**保持した字形 run を cluster で区切り、区間ごとに brush を替えて `DrawGlyphRun` に 1 回だけ渡す**（`draw_replaced` の `replaced_tints` と同じ道）。字形を保持できない行（collector の棄却・右から左）は `HitTestTextRange` の矩形で切り抜いて `DrawTextLayout` を区間の数だけ通す（遅いが正しい）。見えている x の範囲の区間だけ描く。
- **`SetDrawingEffect` は使わない**。共有 layout（ADR 0114）を書き換え、effect つきの run は ADR 0073 の collector が収集全体を棄却して速い道を失う。色は layout の書式でなく brush。
- 優先: 置き換え（`^M` の muted）> 構文 > `text`。選択の上の字は構文の色のまま。IME の差し込み行は段 1 では色なし。
- 行頭の状態: **見えている最初の行の N 行前（200）から状態だけを走らせる**（Vim の `c_minlines` 型）。「見えている最初の行の状態」を (revision, 行番号) の鍵で frame 間に 1 つ保持し、同じなら遡らない。塊ごとの状態の索引（`NewlineIndex` と同じく版ごとに不変）は要るとわかってから（段 2 の後に判断）。
- 巨大な行（`giant_line_threshold` 超）は色を付けない（窓の先頭の状態を行頭から決められない）。文書の大きさでは切らない（見えている行だけなので本文の大きさに依らない）。
- 言語は**文書の値**（`DisplayForm` と同じ・設定に入れない）。拡張子の表は core の `constexpr` 1 つ（大文字小文字を区別しない）。`:set filetype=cpp` で上書き。ステータスバー右側に言語名（`write_status` の 10 枠に収まるか段 3 で確かめる）。
- 字句 → 役割の対応は core の表 1 つ: comment / keyword（制御語・プリプロセッサ・HTML のタグ・Markdown の見出し）/ string（Markdown のコード）/ number / type（組み込み型・型注釈・HTML の属性）/ function（呼び出し・def の次・Markdown のリンク）/ constant（true false null None・`$変数`）/ operators / error（閉じていない文字列）/ warning（LSP の診断に残す）。

### 1.3 言語の順

hide の指定（範囲案）: Python・JavaScript・TypeScript・JSON・Markdown・C・C++ の 7 つを優先。
順は **C/C++ → JSON → Python → Markdown → JavaScript / TypeScript**。C/C++ はブロックコメントと raw string で「行をまたぐ状態」の設計を最初に試せ、Nib 自身のソースで試用できる。JSON は小さく、Python は 3 連の引用符、Markdown はフェンスの中の別言語の埋め込み、JS/TS はテンプレート文字列の入れ子と正規表現リテラルで一番重い。PowerShell・Bash・YAML・HTML は後続。
手書きの規模（推測）: C/C++ 350〜450 行＋表、Python 250、JSON 100、JS/TS 400、Markdown 250。

### 1.4 費用（推測・段 1 で実測）

| 場面 | 見積もり | 扱い |
| --- | --- | --- |
| 見えている 60 行 × 100 文字 | 12〜60 µs | 単打 0.64 ms の 2〜10% |
| 200 行の遡り | 40〜200 µs | 鍵で保持して毎回走らせない |
| `DrawGlyphRun` が区間の数だけ増える（1 行 5〜20） | 長い行の単打 1.57 ms に効く恐れ | 見えている x の範囲だけ |
| 16 MiB の連打 | 本文の大きさに依らない | 遡りは 200 行で頭打ち |

速さのゲート: 今のベンチの文書は `.txt` 相当で色が付かない。**色の付く文書（Nib の C++ ソース）の単打・長い行・16 MiB 連打を 3 本足す**か、既存の文書の拡張子だけ変えた版にするかは段 1 の ADR で決める。

### 1.5 施主に確かめる点

| # | 問い | 設計席の推し |
| --- | --- | --- |
| S-1 | 構文の色を既定でオンにするか。設定に入れるか | **既定オン**。初版は設定に入れず（`settings.v3` を触らない）、`:syntax off` / `:syntax on` を**文書の値**にする。設定へ入れるのは設定モーダル（v4 への版上げ）と同時 |
| S-2 | 最初の言語の集合と順（§1.3） | 7 つ・C/C++ から |
| S-3 | 自動オフのしきい値 | 行の長さだけ（巨大な行は色なし）。文書の大きさでは切らない |
| S-4 | 色の当て方（字句 → 10 役割の対応・§1.2）と見た目 | 対応表は設計席が ADR に書き、見た目は段 2 の試用で確かめる（テーマ 125 の色はもう決まっているので /design の案は要らない） |

### 1.6 工程

| 段 | 中身 | 席日（推測） | ADR | 前提 |
| --- | --- | --- | --- | --- |
| 1 | `ColorSpan` / `SyntaxRole`・run を区切る描画・格子への写し・plain は空・色つきのベンチの文書 | 2 | **1**（区間の形と描画・状態の決め方・ベンチ） | split の 3 PR の merge（renderer の `draw_line` を触るので衝突を避ける） |
| 2 | C/C++ の字句器と fixture（行ごとの区間と終わりの状態の表）・200 行の遡り・試用 | 2 | 1 に含める | 段 1 |
| 3 | 言語の判定（拡張子の表・`:set filetype=`・`:syntax`・ステータス） | 1 | **1**（判定と文書の値） | 段 2・S-1 |
| 4 | 残りの 6 言語（1 言語 0.5〜1 日）・Markdown のフェンスの埋め込み・塊ごとの状態の索引（要れば） | 5〜7 | 0〜1 | 段 3 |


段 4 の「フェンスの埋め込み」は HTML の `<script>` `<style>` の中身（#838 で区間なしの `raw_string`・`delimiter` は閉じタグの頭）を JavaScript / CSS の字句器へつなぐ所も同じ道で決める（`delimiter` のほかに中の言語を持てるか・#842 のレビュー Q）。

計 4 段・ADR 2〜3 本・10〜12 席日。各段の受理は D41（独立レビュー・速さのゲート・CI・試用）。

## 2. 言語サーバー（LSP）

### 2.1 仕組みの要点（LSP 3.17）

- JSON-RPC 2.0 over stdio。ヘッダ `Content-Length`（必須）→ `\r\n\r\n` → UTF-8 の本文。`initialize` の答えを受けるまで何も送らない → `initialized` → `didOpen` / `didChange`（Incremental は `TextBuffer::replaced(begin, end, inserted)` の 1 編集がそのまま対応）/ `didClose` → `shutdown` → `exit`。
- 位置は既定が **UTF-16 のコード単位**。3.17 の `general.positionEncodings` で `utf-8` を申し出て、サーバーが `capabilities.positionEncoding` で選ぶ。clangd は旧拡張を非推奨にして 3.17 の方式。Nib は `LineUtf16Evaluation` の写しが既にあるので UTF-16 に落ちても扱える。
- 機能: `publishDiagnostics`（range・severity 1〜4）・`completion`・`hover`・`definition`。応答の順は入れ替わるので id で引く。
- 起動: adapters で `CreatePipe`（または OVERLAPPED のための名前つきパイプ）2 本・`CreateProcessW`（`STARTF_USESTDHANDLES`・`CREATE_NO_WINDOW`）・Job Object で Nib が落ちたら子も落とす。すべて adapters（ARC-007）。
- サーバー: clangd（LLVM の単体 exe・Nib の toolchain に同じ LLVM）・rust-analyzer（単体 exe）・pyright と typescript-language-server は **Node.js が要る**。

### 2.2 Nib の規則との整合（設計席の決め）

| 点 | 決め | 理由 |
| --- | --- | --- |
| パイプの読み | **OVERLAPPED で読み、event を UI スレッドの `MsgWaitForMultipleObjectsEx` で待つ**（`Main.cpp` の WARP → 本物の device の待ちが先例・ADR 0126 の隔離実験を製品の port に昇格） | `Win32Worker` は FIFO 1 本で、同期の `ReadFile` を載せると device の生成・列挙・自動退避が止まる。2 本目のスレッドは ADR 0004 / 0062 の「1 本」を変える |
| 書き | OVERLAPPED で書き、完了は同じ待ちで回収（64 KB を超える `didOpen` がパイプで詰まる恐れ・推測） | |
| JSON | **core に手書きの最小の JSON**（値は閉じた variant・読み書き・失敗は `std::expected`） | nlohmann は例外と `std::` を大量に使い、厳格集合を通らない見込み。LSP で要る形は小さい |
| 枠の切り出し | core の純関数（バイト列 → 完全なメッセージの列＋残り） | |
| 時刻（応答なし） | ui のタイマーの意図（`AutosaveTick` と同じ合図の形）で要求 id の古さを数える。`keeps_tab_walk` と `begin_intent` に書く | 時計は adapters だけ |
| 受け取り | `WorkCompleted` と同じく「中身の無い合図 → `collect()`」。新しい合図を足すか共用するかは ADR | |
| 打鍵の道 | UI スレッドは `didChange` の発行だけ（数十 µs・推測）。打鍵ごとに送らず frame の終わりに溜めて 1 通 | 1 打鍵 1 ms の予算 |

### 2.3 表示

| 機能 | 載せ方 |
| --- | --- |
| 診断 | `LineView` に `std::vector<DiagnosticSpan>`（`ColorSpan` と同じ形・役割は error / warning）。renderer は IME の下線の道（`underline_runs`）で波線。件数はステータスバーの枠、キャレットの行の診断文は通知の欄 |
| 補完 | `CommandPalette` と同じく結果を 1 回作って窓だけ frame へ（ADR 0062 / 0150）。面はキャレットの下。INSERT の Ctrl-n / Ctrl-x Ctrl-o |
| hover | 小さな面（`panel` / `panel_border`）。Markdown の答えは段 1 では平文 |
| 定義へ | `Location` の URI → `FilePath` → `open_document`（1 本）→ キャレット。NORMAL の `gd` / `K` |

### 2.4 費用と危険

| 点 | 手当て |
| --- | --- |
| 起動（clangd は `compile_commands.json` を読み、最初の診断まで数秒・推測） | 文書を開いた後の合図で起こす。起動のベンチに入れない。設定で使うときだけ |
| クラッシュ・応答なし | パイプの EOF と `ERROR_BROKEN_PIPE`・プロセスの終了を event で待つ。1 行知らせる。古い要求は `$/cancelRequest`。自動の再起動は回数の上限 |
| 設定 | **D60: `lsp` の on / off を設定に持つ（既定 off）**。サーバーのパスと引数を言語ごとに 1 行。`settings.v3` → **v4 への版上げ**（ARC-009・ADR 0114 と同じ移行の型）。設定モーダルの v4 と合わせる。`:set lsp` / `:set nolsp` と Ctrl+P の `:` から切り替える |
| セキュリティ | **設定に書いたパスだけを起こし、既定は無効**。文書の側（フォルダの `.nib` など）から起動の指定を読まない |
| 文字の位置 | UTF-16 に落ちたときのサロゲートは core の写し 1 本と fixture |
| 巨大な文書 | しきい値を超える文書はサーバーへ送らない（`didOpen` がエスケープで数十 MB になる） |

### 2.5 施主に確かめる点

| # | 問い | 設計席の推し |
| --- | --- | --- |
| L-1 | LSP を初版の範囲に入れるか（SPECIFICATION に FR と D 番号を足す）。入れるならいつ（§0 の順） | 入れる。ただし構文の色と Markdown の後。初版（正式リリース）に入れるかは段 1 の試用で決める |
| L-2 | `gd` を Vim の局所の定義のままにするか、LSP の定義へ跳ぶか | **LSP があれば LSP の定義へ、無ければ Vim の局所**（Neovim の慣習） |
| L-3 | ~~サーバーの起動は設定に書いたパスだけ・既定は無効でよいか~~ → **決定 D60（設定で切り替え・既定オフ）**。残るのは自動再起動の回数 | 再起動は 3 回まで |
| L-4 | 色と LSP を止める文書の大きさ | LSP は 4 MiB 超は送らない（仮・段 1 で測って決める）。色は行の長さだけ（S-3） |
| L-5 | 最初のサーバーは clangd でよいか（pyright は Node.js の導入が要るので 2 本目） | はい |

### 2.6 工程

| 段 | 中身 | 席日（推測） | ADR | 前提 |
| --- | --- | --- | --- | --- |
| L0 | 前提: OVERLAPPED のパイプの port・`pump_messages` の待ちの配列化・子プロセスと Job Object・core の JSON と枠の切り出し | 4〜5 | 2（I/O と子プロセス・JSON） | split・構文 段 1〜3（区間の形）・L-1 |
| L1 | 診断だけ: initialize / didOpen / 増分 didChange / publishDiagnostics → 波線と件数・設定 v4・1 行の知らせ | 3〜4 | 1（client と診断・設定の版上げ） | L0・L-3・L-4 |
| L2 | 補完（面の配置・鍵・INSERT との関係） | 3 | 1 | L1 |
| L3 | hover と定義（面・`open_document`・`gd` `K`） | 2〜3 | 0〜1 | L1・L-2 |

計 4 段・ADR 4〜5 本・12〜15 席日。各段の受理は D41。

### 2.7 速さの評価（D60）

オフのときは今の 8 本のベンチ（`eng/perf-reference.json` の基準値・QLT-014）をそのまま守る。LSP の段 L1 の PR は、オフの 8 本に退行が無いことを今の基準で示す。

オンのときは別のベンチを足し、オフの基準と混ぜない:

| ベンチ（`--bench` で名指し・判定の 8 本に入れない・ADR 0138 と同じ扱い） | 測るもの |
| --- | --- |
| `lsp-single-key` | clangd を起こして `initialize` が返った後の 1 打鍵（入力→提示） |
| `lsp-burst-200` | 同じ状態での 200 打鍵 |
| `lsp-diagnostics-ready` | 打鍵を止めてから診断の下線が frame に載るまで（サーバーの速さを含む・参考値） |
| `lsp-open-16mib`（L-4 のしきい値の外なら送らないので、しきい値の内側の最大で） | 文書を開いてサーバーに `didOpen` を送り終えるまで |

基準値は `eng/perf-reference.json` に `lsp-` の接頭辞で別に持ち、目標は D51 と別に施主が決める（L1 の試用の後）。clangd の版と `compile_commands.json` の有無を記録に書く。測る機械は静かな状態（CLAUDE.md §2）で、clangd の起動を暖機に含めるか捨てるかはベンチの定義に書く。

## 3. 全体の順番（設計席の推し・hide の判断）

| 順 | 仕事 | 理由 |
| --- | --- | --- |
| 1 | split の試用 → #688 → #687 → #692 の merge（rebase 後に本測定を 1 回） | 試用待ち。renderer・frame・other の形がここで確定し、以降の 3 つが乗る |
| 2 | D48 の分割（`VimStep.cpp` は鍵の表の単位・`EditorController.cpp` は意図の種類の単位・ADR で切り方） | 大容量 段 2 と LSP の controller の変更がこのファイルに集中する |
| 3 | 大容量 段 1（索引の細身化・上限の 1 本化）と Markdown 段 1〜2（md4c の取り込み・parse・probe） | どちらも split と独立で、席を並べられる |
| 4 | 構文の色 段 1〜3（C/C++） | ロードマップ段階 3。LSP の診断の区間と同じ形を先に作る |
| 5 | Markdown 段 3〜6 | ロードマップ段階 5。split と md4c の両方が揃う |
| 6 | 大容量 段 2〜6 | ロードマップ段階 2。D48 の後。試用と実測が重い |
| 7 | 構文の残りの言語・LSP L0〜L1 | 新しい範囲。L1 の試用で初版に入れるかを決める。D60 で既定オフなので、オフの速さは今の基準のまま |

ロードマップの順（大容量 → 日常編集 → split → Markdown）と違うのは、split が先に試用まで進んだことと、大容量の段 2 が D48 の分割を待つため。ロードマップの本文は変えず、この表を「2026-10-21 の現在地からの順」として足す。
