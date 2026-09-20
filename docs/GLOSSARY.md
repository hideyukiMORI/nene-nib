# 用語集 — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版
> ここに載っている語は、コード・ドキュメント・Issue で**同じ意味**で使う。同義語を発明しない。

| 語 | 意味 | 型／場所 |
| --- | --- | --- |
| 結果（outcome） | 期待される成功・失敗を表す閉じた型 | `std::expected<T, *Failure>` / `*Outcome` |
| 拒否理由（rejection / failure） | 結果に添える閉じた理由の集合 | `*Rejection` / `*Failure`（`enum class`） |
| ポート（port） | 中核がプラットフォームを使うための型のある窓口 | `*Port`（application） |
| アダプタ（adapter） | ポートの実装。プラットフォームに触れてよい唯一の場所 | `*Adapter`（src/adapters/win32） |
| 合成ルート | 依存を結ぶ唯一の場所。端末と終了コードを持つ | `src/app`（`wWinMain`） |
| 意図（intent） | 利用者の操作・ワーカーの完了を application へ渡す閉じた値。UI はこれを発行するだけ | `*Intent`（application） |
| 表示値（view value） | application が作り、UI が描くだけの不変の値 | `*Frame` / `*View`（application） |
| 反映（render） | 表示値を Direct2D の描画へ写す操作。UI 状態を変えてよい唯一の場所 | `render*`（ui/win32） |
| 隔離区画 | 可変性を許した唯一の場所 | ARC-005 の表 |
| テキスト正本 | 開いているバッファの本文の唯一の所有物。piece table で持ち、通常モードと Vim モードが同じものを編集する | `TextBuffer`（core）/ `EditorState`（application） |
| 本文（piece table） | 開いているバッファの正本。original と add の 2 バッファと piece の列。不変で、編集は次の本文を返す | `TextBuffer`（core・ADR 0009） |
| 位置 | バイトの `Offset`（0 始まり）・行の `LineNumber`（1 始まり）・code point の `Column`（1 始まり） | core |
| 選択 | anchor と caret の 2 つの位置。Shift+移動が anchor を固定する | `Selection`（core） |
| 編集履歴 | undo / redo の列。連続した文字入力は 1 単位 | `EditHistory`（core） |
| 編集モード | 通常 / Vim の 2 つ。トグルで入れ替わるのはキー割り当てとマウス規則だけ | `EditMode`（core・閉じた集合） |
| Vim エンジン | Vim の振る舞い（モード・カーソル・レジスタ・オペレータ・テキストオブジェクト・`.`・マクロ）を純関数で持つ自前実装 | `Vim*`（core） |
| 行内文字検索 | `f/F/t/T`の次の対象文字を待ち、`;`/`,`で対象と種別を再利用する移動。待ちと記憶の寿命は別 | `VimCharacterSearchKind` / `VimCharacterSearch`、所有は `VimState`（core・ADR 0026） |
| Vimの次キー待ち | 文字検索の対象待ち、または接頭キーの続き待ちの一方だけを持つ任意の和型 | `VimInputWait` / `VimPrefix`、所有は `VimState`（core・ADR 0027） |
| 回数付き開行の入力記録 | o/Oで最初に開いた1行へのLF入力と残り回数。Escで反復し、移動・記録を越える削除・外部編集で破棄する | `VimInsertRepeat`、所有は `VimState`（core・ADR 0028） |
| 位置指定のVim挿入 | p/P・o/O・Escの反復が返す挿入位置、LF本文、挿入後caret、履歴境界。同じ改行変換とreplaceを使う | `VimInsertAt`（core）→ `EditorController`（application・ADR 0028） |
| 指定行移動 | `gg` / `G`で先頭・末尾・回数で指定した絶対行の最初の非空白へ移る。operatorでは両端を含む行単位範囲 | `VimMotion`（core・ADR 0027） |
| Vimレジスタの種別 | 未設定・文字単位・行単位の区別。未設定と、成功した空範囲yankの文字単位は別の状態 | `VimRegisterKind` / `VimRegister`（core・ADR 0026）、所有は `EditorState` |
| Vim の編集対象の view | 本文・選択の借用と表示領域の値を 1 回の鍵処理へ渡す入力。状態の所有者ではない | `VimEditorView` / `VimViewport`（core・ADR 0019） |
| 画面の境界と追従 | 本文末尾を画面内に埋めるか最終行まで先頭にできるか、カーソルを最小限で追うか Vim の規則で追うかを、それぞれ明示する方針 | `ScrollExtent` / `ScrollFollow`（core・ADR 0019） |
| 画面移動 | 選択と表示先頭行を一緒に更新する Vim の効果。本文を編集しない | `VimNavigate`（core・ADR 0019） |
| キーマップの表 | キー列 → 動作の `constexpr` の表。巨大な `switch` を書かない | `KeyBinding`（core） |
| oracle | 本物の Vim（headless）。fixture の期待値を生成する開発時の道具。製品にも CI にも要らない | `eng/`（ADR 0005） |
| fixture | 初期テキスト・キー列・oracle が出した期待値（本文・カーソル・レジスタ）の組。単体テストが再生する | `tests/unit/`（ADR 0005） |
| ワーカー | `src/adapters/win32` が所有する固定の 1 本のスレッド。要求を受けて完了を返す | `WorkerPort` / `Win32WorkerAdapter`（ADR 0004） |
| 要求（request）/ 完了（completion） | ワーカーへ渡す不変の値と、UI スレッドへ返る版番号付きの値 | application |
| 版番号 | 状態の世代。古い完了を捨てる唯一の判断材料 | application |
| 速さの基準値 | ベンチ 3 本の参照値。退行でゲートが落ちる。「baseline」とは呼ばない | `eng/perf-reference.json`（ADR 0006） |
| 表示文字列 | 検証済み（空・制御文字・不正 UTF-8・256 バイト超を拒否）の UTF-8 文字列。窓に描く 1 行の正本 | `DisplayText`（core） |
| 外観 | OS のライト／ダーク設定に対応する閉じた集合 | `Appearance`（core） |
| 配色 | 外観ごとの背景と文字の色。純関数 `palette_for` の結果 | `Palette` / `RgbColor`（core） |
| 外観ポート | OS の外観を読む唯一の窓口。実装は adapters/win32（レジストリの `AppsUseLightTheme`） | `AppearancePort` / `Win32AppearanceAdapter` |
| 本文設定 | pt の文字サイズ・検証済みフォント名・任意の組み込みテーマ。テーマ無しは OS 追従 | `EditorSettings`（core）、所有者は `EditorState`（ADR 0020） |
| フォントサイズ | 有限の 8〜40 pt。既定 13.5 pt。DIP と既定比率は名前付き純関数で導く | `FontSize`（core） |
| Ex入力 | 本文・undo・レジスタと独立した1行256 bytesまでのUTF-8入力。Tab補完の元prefixと位置も持つ | `CommandLine`（core）、所有者は `EditorState`（ADR 0022） |
| Ex評価 | 設定用コマンドを検証し、任意の保存候補と結果表示、または閉じた失敗を返す純関数 | `evaluate_ex` / `ExResult` / `ExFailure`（core） |
| コマンド入力session | Ex一行入力またはCtrl+P一覧の一方だけを持つ任意の和型 | `CommandInput`（application）、所有者は `EditorState`（ADR 0023） |
| コマンドpalette | 共通の一行入力、決定的な部分列照合、候補選択。候補はEx補完と同じ一覧から導く | `CommandPalette` / `CommandChoice` / `PaletteLayout`（core）、`CommandPaletteView`（application） |
| 利用者テーマの所有値 | ファイル由来の名前・出典・配色を所有し、lvalueから既存Themeのviewを貸す | `ThemeName` / `ThemeDocument` / `OwnedThemeSource`（core、ADR 0024） |
| テーマファイルの検証 | version/名前/必須色/任意UI色/コントラストを検証。制限付き読込は既存FilePortへ委ねる | `ThemeCodec` / `ThemeContrast`、共通 `KeyValueFields`（adapters/win32） |
| 設定ポート | 設定の読込・保存と型付き失敗。保存形式と競合保護は adapter、bytes の置換は FilePort | `SettingsPort` / `Win32SettingsAdapter`（ADR 0020） |
| エディタの依存 | controller へ注入する外観・clipboard・file・code page・設定ポートの名前付き参照 | `EditorPorts`（application / 合成は app、ADR 0020） |
| エディタ状態 | 表示文字列と外観の唯一の所有者。不変で、次状態を返す | `EditorState`（application） |
| エディタフレーム | 表示文字列と配色の表示値。UI はこれを写すだけ | `EditorFrame`（application） |
| 見た目のトークン | 採用案の寸法と配色。`core::Palette` が持ち、UI が写す | `docs/design/2026-09-15-look.md`（ADR 0008） |
| タイトルバーの配置 | タブ・「＋」・窓の操作の矩形とヒットテストを座標から決める純関数 | `TitleBarLayout`（ui/win32） |
| レンダラ | D3D11 → DXGI（flip・waitable・composition）→ DirectComposition → Direct2D の提示経路。`render` だけが描く | `Direct2DRenderer`（ui/win32） |
| 規約検査（conformance） | NeNe Nib 固有の自作ゲート | `eng/conformance.py`（CNF-NNN） |
| シンボル検査 | 中核の静的ライブラリの未定義シンボルを許可リストと照合する検査 | `eng/symbols.py`（ARC-003 / ARC-007 / CPP-013） |
| waiver | 1 つの規則に対する期限付きの狭い例外 | `docs/waivers/WVR-NNNN-*.md` |
| 機械強制の状態 | active / planned / 不能 / 不採用 | `docs/QUALITY_GATES.md` の強制マトリクス |
| negative proof | ゲートが意図した規則で落ちることの実測 | `docs/quality/gate-proofs.md` |

## 使ってはいけない語

`Manager` / `Helper` / `Util` / `Utils` / `Common` を型名の語尾に使わない（CPP-010）。
役割を語る名前が思いつかないときは、その型が 2 つの責務を持っている可能性が高い。
「baseline」は設定ファイル名にも語にも使わない（CNF-005）。速さの参照値は「基準値」と呼ぶ。
