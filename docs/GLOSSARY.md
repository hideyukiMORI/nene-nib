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
| 編集モード | 通常 / Vim の 2 つ。トグルで入れ替わるのはキー割り当てとマウス規則だけ | `EditMode`（core・閉じた集合） |
| Vim エンジン | Vim の振る舞い（モード・カーソル・レジスタ・オペレータ・テキストオブジェクト・`.`・マクロ）を純関数で持つ自前実装 | `Vim*`（core） |
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
