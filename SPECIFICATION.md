# 仕様 — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版（施主 hide の叩き台と決定 D1〜D10 を FR に起こしたもの）
> 「決め打ちしない」項目は明示する。実装して窮屈なら変えてよい。変えたら本書と ADR を更新する。

## 0. 一言

Windows 11 で考えうる限り速い、**単体 exe のテキストエディタ**。通常のエディタ操作と Vim 操作をトグルで切り替える。
見た目はモダンで、メモ帳に無い「ファイルに戻りやすさ」（履歴・ブックマーク・Ctrl+P）を持つ。

## 1. 優先順位（施主決定 2026-09-15）

1. **速さ**と**モダンな見た目**（両立できるならこれを最優先）
2. Vim 操作の再現度（T1 → T2）
3. それ以外

## 2. 施主決定（覆すときは施主へ）

| # | 決定 | 理由・帰結 |
| --- | --- | --- |
| D1 | Windows 11 専用 | 今の Windows 11 で考えられる限りの速さ。Mica・DirectX 12 世代の DXGI を前提にする |
| D2 | Qt / X11 / WinUI 3 / Electron / WebView は使わない | 起動・依存・配布物サイズ。[ADR 0002](docs/adr/0002-plain-win32-with-direct2d.md) |
| D3 | Vim は**自前実装**（Neovim を載せない・Vim のコードは写さない） | 単体 exe。テキストの正本 1 本で、トグルはキー割り当ての入れ替えだけ。MIT のまま行く。[ADR 0005](docs/adr/0005-own-vim-engine-verified-against-real-vim.md) |
| D4 | Vim の再現度は**本物の Vim との差分テスト**で機械的に担保する | 同じ初期テキストとキー列を自前エンジンと headless Vim に流し、本文・カーソル・レジスタを突き合わせる |
| D5 | ファイルアクセスは **Ctrl+P 1 本に統合** | 開いているタブ・ブックマーク・履歴・同じフォルダを 1 つの窓でファジー検索。行頭の記号で絞り込む |
| D6 | タブは**タイトルバーに横並び**（Windows Terminal 型） | 縦幅を節約。数が増えた弱点は Ctrl+P と Ctrl+Tab（最近使った順）で補う |
| D7 | Markdown プレビューを持つ | WebView は使わない（D2）。md パーサ（md4c・MIT・C）＋ Direct2D の自前描画。範囲は CommonMark のサブセット |
| D8 | 文字コードは UTF-8 既定・Shift_JIS を自動判別 | 日本語 Windows の古いファイル対策。改行コードは読んだ形を保つ（推し。未決 6） |
| D9 | タブの復元は**保存済みファイルだけ** | 未保存の新規タブが勝手に溜まるのが Win11 メモ帳の「雑多」の一因 |
| D10 | トグルは 1 キー＋ステータスバーのボタン。鍵は試作で触って決める | — |
| D11 | ダークの配色は Ubuntu をイメージした深い茄子色（施主決定 2026-09-15） | 背景は Ubuntu 端末の #300A24、文字は淡い灰 #EEEEEC。ライトは中立の明るい灰（#F4F5F7 / #1B1F24） |
| D13 | 有名なカラーテーマ（Solarized 系・Monokai・Dracula・One Dark・Night Owl など）を `:colorscheme <name>` で選べるようにする（施主決定 2026-09-15） | 計画は [docs/plans/2026-09-15-colorschemes.md](docs/plans/2026-09-15-colorschemes.md)。既定は OS 追従（茄子色のダーク・中立のライト）。明示的に選んだテーマは OS の切り替えで変わらない |
| D15 | 編集中の状態の見た目（キャレットの形・選択・検索の当たり・IME・`:` の行）は [採用案](docs/design/2026-09-15-editing-look.md)。`:` の行は案 A（ステータスバーの左側が置き換わる）（施主決定 2026-09-15） | ADR 0009 |
| D16 | タイトルバー（タブの帯）の背景は Mica に透かす淡い面ではなく、本文より一段深い不透明の茄子色 #1E0516。アクティブなタブは本文色で本文に繋がる（施主決定 2026-09-17・案 C） | テーマごとの値（`title_bar` / `tab_active`）。ライトは帯 #E1E4E9・タブ #F4F5F7。Mica 自体は起動の面（ADR 0013）と明暗のために残す。案の絵: https://claude.ai/artifact/NsLXAGUtYQnaS788WmFw2G 。実装は Issue #31 |
| D14 | フォントサイズを変更できるようにする（施主決定 2026-09-15） | Ctrl+`+` / Ctrl+`-` / Ctrl+`0`（既定に戻す）と Ctrl+ホイール、Vim では `:set fontsize=<pt>`（`:set guifont` の書式も受ける）。値は設定に保存。計画は docs/plans/2026-09-15-colorschemes.md 第 6 節 |
| D12 | 見た目は `/design` で起こした案（[採用案](docs/design/2026-09-15-look.md)・[ADR 0008](docs/adr/0008-adopted-look-tabs-titlebar-statusbar-mica.md)）を採用。アクセントは Ubuntu 橙 #E95420（施主承認 2026-09-15） | 見た目の判断は実装の前に `/design` で画を作って施主が選ぶ（施主指示 2026-09-15） |

## 3. 技術の土台（ADR で確定したもの）

- 言語と道具: C++23 / clang-cl 19.1.5 / 素の Win32。純粋な core（piece table・Vim エンジン・キーマップ・Ctrl+P の順位付け）は OS 非依存の純関数層（[ADR 0003](docs/adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)）
- 描画: Direct2D ＋ DirectWrite（GPU）・グリフキャッシュ・変わった行だけ再描画（[ADR 0002](docs/adr/0002-plain-win32-with-direct2d.md)）
- 低遅延: キー入力でその場で描く・DXGI flip model ＋ waitable swap chain・DirectComposition で窓へ結ぶ
- 起動: 静的リンク（`/MT`）の exe 1 本・ランタイム依存なし
- 巨大ファイル: メモリマップ ＋ piece table ＋ 行索引の遅延構築（ワーカー・[ADR 0004](docs/adr/0004-ui-thread-plus-one-worker.md)）
- 見た目: Mica（`DWMWA_SYSTEMBACKDROP_TYPE`）・自前タイトルバー（タブ）・Segoe UI Variable・ライト／ダーク／アクセント色は OS 追従・DirectComposition の滑らかスクロール
- 日本語入力: IMM32 / TSF を初版から。**変換中は Vim の鍵を奪わない**
- 速さの見張り: ベンチ 3 本（起動→最初の描画 / キー→画面 / 1 GB を開く）を CI に置き、退行で落とす。目標値は初版を実測してから決める（[ADR 0006](docs/adr/0006-speed-gate-simd-and-table-driven-dispatch.md)・QLT-014）

## 4. 機能要件（初版の輪郭）

| ID | 要件 | 備考 |
| --- | --- | --- |
| FR-001 | 単体 exe で起動し、実行時依存を持たない | OS の DLL 以外を import しない（R1・D1 で実測） |
| FR-002 | 通常モード: メモ帳と同じ操作（Ctrl+C/V/Z、Shift+矢印、マウスはメモ帳と同じ） | |
| FR-003 | Vim モード: T1 / T2 の範囲（第 5 節）。マウスも Vim 流（クリックはノーマルのまま移動・ドラッグでビジュアル・Alt+ドラッグで矩形・ホイールは画面だけ） | |
| FR-004 | トグル: 1 キー＋ステータスバーの「通常 \| Vim」トグル（選択側がアクセントの面）。隣に現在モード。テキスト・undo・カーソルは共通のまま、キー割り当てとマウス規則をセットで入れ替える | D10・D12。鍵は🔶決め打ちしない |
| FR-005 | タブはタイトルバーに横並び（40 DIP の帯・右端に最小化・最大化・閉じる・帯は `title_bar`（不透明・D16））。Ctrl+Tab は最近使った順 | D6・D12・ADR 0008 |
| FR-006 | Ctrl+P: 開いているタブ・ブックマーク・履歴・現在ファイルのフォルダを統合したファジー検索。行頭の記号で絞り込み（例 `★` ブックマーク / `◷` 履歴）。Vim モードの `:e` `:b` `:ls` も同じ一覧 | D5 |
| FR-007 | Markdown プレビュー（CommonMark サブセット・自前描画） | D7。md4c の導入は別 Issue（ADR 0003） |
| FR-008 | 文字コード: UTF-8 既定・Shift_JIS 自動判別。改行コードは読んだ形を保つ | D8 |
| FR-009 | タブの復元は保存済みファイルだけ | D9 |
| FR-010 | ブックマーク: Ctrl+D で付け外し。タスクバーのジャンプリストに履歴とピン留め（`SHAddToRecentDocs` / `ICustomDestinationList`） | |
| FR-011 | 見た目: Mica・自前タイトルバー・Segoe UI Variable・OS のライト／ダーク／アクセント色に追従・滑らかスクロール | ADR 0002 |
| FR-012 | 日本語入力（IMM32 / TSF）。変換中は Vim の鍵を奪わない | |
| FR-013 | 巨大ファイル（1 GB）を開ける。メモリマップと遅延の行索引 | ベンチ 3 本目 |
| FR-014 | 表示は Per-Monitor v2 の DPI で崩れない | マニフェスト |
| FR-015 | 速さの退行は CI が落とす | QLT-014 |
| FR-017 | 本文のフォントサイズを 8〜40 pt の範囲で変更できる。Ctrl+`+` / Ctrl+`-` / Ctrl+`0` と Ctrl+ホイール、`:set fontsize=<pt>`。値は設定に保存し、DPI と掛け合わせて描く。フォント名の変更は同じ設定の縦切りで扱う | D14 |
| FR-016 | `:colorscheme <name>` で組み込みテーマ（ubuntu-aubergine / neutral-light / solarized-dark / solarized-light / monokai / dracula / one-dark / night-owl / night-owl-light）を切り替え、設定に保存する。`:colorscheme system` で OS 追従に戻す。Ctrl+P の `:` 接頭辞からも同じ一覧 | D13。計画は docs/plans/2026-09-15-colorschemes.md。利用者のテーマファイルはC4a/b（#68 / #70・ADR 0024 / 0025）で同じ選択へ接続 |

## 5. Vim の範囲

- **T1（最優先）**: ヴァニラの Vim 既定動作。`.vimrc` の取り込みは初版では不要。「VSCodeVim で困らない水準」を T2 の到達目標の物差しにする（VSCodeVim の対応表を比較材料に使う）。設定ファイル（`map` や `set` 程度）は後から足せる形にだけしておく
- **T2**: 一般的な Vim ユーザーがよく使うところ
  - 移動 `hjkl w b e 0 ^ $ gg G f t ; , % { } H M L Ctrl-d/u`・ジャンプ `Ctrl-o/i`・マーク
  - オペレータ `d c y > < gu gU J r x p P` ＋回数＋移動
  - テキストオブジェクト `iw aw i" a" i( a( i{ it`
  - ビジュアル `v V Ctrl-v`（矩形の挿入・追記含む）
  - 繰り返し `.`・マクロ `q @`・レジスタ `"a "0 "+`
  - 検索・置換 `/ ? n N * #`・`:s`・`:g`
  - `u Ctrl-r`・挿入モードの `Ctrl-w/u/r`・`:w :q :e :b :ls`・簡単な `map`
  - 難所: Vim 正規表現の方言 / `.` の挿入再生 / undo の区切り / 全角・タブ混在の矩形
- **T3（やらない）**: Vim script・プラグイン・折り畳み・`:terminal`・分割ウィンドウ（初版）

## 6. 決め打ちしない項目

| 項目 | 初期値 | 変えてよい条件 |
| --- | --- | --- |
| トグルの鍵（FR-004） | 未定 | 試作で触ってから |
| シンタックスハイライトの範囲 | 見えている範囲だけ・入力経路の外（ワーカー）・行頭状態のキャッシュで差分だけ。自前の字句解析器。巨大ファイルは閾値で自動オフ。初版の言語数を絞る | 実装後の使用感と「キー→画面」の計測 |
| 改行コードの既定 | 読んだ形を保つ（CRLF / LF）。判別は最初の `
` の直前が `
` かどうか、無ければ CRLF（ADR 0010・Issue #11 で確定） | — |
| ベンチの目標値 | 初版の実測後 | 施主の実機で記録（ADR 0006） |
| 利用者のテーマ（配色ファイル） | 組み込み9テーマに加え、起動時に最大128件の `.v1.theme` を読み、共通カタログから選択・保存・復元する。UIはThemeの色だけを使う | C4a/b（#68 / #70・ADR 0024 / 0025）。形式と制限は docs/design/user-theme-format.md |

## 7. 非要件（やらないこと）

- Windows 10 対応（D1）
- UI ライブラリ・WebView（D2）
- Vim script・プラグイン・折り畳み・`:terminal`・分割ウィンドウ（T3）
- 複数のワーカー・並列ハイライト（ADR 0004）
- 署名・インストーラ・自動更新（Phase 4 の別 Issue）

## 8. 設計上の固定点（憲章との対応）

- テキストの正本は 1 本（ARC-001）。通常モードと Vim モードは同じ編集操作の集合を使い、キー割り当てだけが違う
- 描画は表示値を写すだけで、状態遷移を UI に置かない（ARC-011・CPP-017）
- ファイル・時刻・スレッドは `src/adapters/win32` にだけ在る（ARC-003 / ARC-007 / CPP-013）
- Vim の振る舞いの正しさは oracle が決め、fixture が単体テストになる（ADR 0005）
- 速さは実測で書く（QLT-014）。README に「爆速」と書くのは基準値と計測が揃ってから

## 9. 対応環境

Windows 11（22H2 以降・x64）、DWM 有効。Mica は 22H2 以降の機能。GPU が無い環境では WARP で描く（Phase 0 の D1 は WARP で実測）。
