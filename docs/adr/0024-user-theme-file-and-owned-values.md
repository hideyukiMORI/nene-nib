# ADR 0024 — 利用者テーマの版付き形式と所有する値

- 状態: 受理
- 日付: 2026-09-20
- Issue: #68（C4a）
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-011 / CPP-002 / CPP-004 / CPP-007 / CPP-012 / CPP-013 / QLT-001 / QLT-008 / QLT-012 / QLT-013

## 文脈

組み込みThemeはconstexpr表の文字列を参照する。C4のファイル由来の名前・出典には別の所有が必要（ADR 0017）。まず形式と読込の契約を確定する。C4bでディレクトリのカタログ、選択/保存、Ex/Ctrl+Pへ接続する。

## 決定

1. `ThemeName` は1〜64 bytesのASCII小文字から始まる、小文字/数字/ハイフンの値。入力のunderscoreはハイフンへ正規化し、その他は拒否する。ファイル名は正規名の `<name>.v1.theme`。`system` と組み込み名は利用者ファイルに使えない。ファイル内nameは要求した名前と一致する必要がある。
2. `ThemeDocument` が名前、明暗、UI/本文配色、`OwnedThemeSource` のauthor/license/urlを所有する。既存 `Theme` はそのlvalueを借りる `theme_view` で作る。rvalueからのviewは削除したoverloadで拒否する。copy/move後のviewは新しい所有値から作る。組み込み表とTheme自体は変えない。
3. UTF-8（BOM可）、LF/CRLFの `key=value`、最大16KiB。行端のASCII空白と空行は許可。コメント構文は作らず、`#` は色の値の一部。未知キー、重複、必須の欠落、未知version、不正UTF-8/値を拒否する。既存設定codecと同じfield分割を共用し、設定形式の受理/拒否は変えない。
4. 必須は `version=1`、`name`、`appearance=dark|light`、`author`、`license`、`url`、`body.`に続くSyntaxPaletteの16役割。出典は既存DisplayTextの一行/非空/256 bytesで検証する。URLは表示用出典でありアクセスしない。ローカル作品は `url=local` と書ける。
5. 本文色は厳密な `#RRGGBB`（hexは大小可）。役割名は `foreground background cursor selection current_line line_number comment keyword string number type function constant operator error warning`。全16色を要求し、未知役割を黙って読み飛ばさない。
6. UIは `derive_ui(body.background, body.foreground, body.cursor, appearance)` で導出する。任意の `ui.<Paletteの役割名>` があればそのトークンだけ上書きする。selection/searchは厳密な `#RRGGBBAA`、残りはRGB。元の組み込みの例外色を暗黙には継承しない。
7. 本文のforeground/backgroundと、上書き後のUIのtext/backgroundがともにコントラスト比4.5以上であることをadaptersで検証する。計算は既存テストと同じsRGB相対輝度の式。libmをcoreへ持ち込まない。色を勝手に補正したり別テーマへ置換しない。
8. `load_theme(FilePort, FilePath)` が正規basename `<name>.v1.theme` を検証し、期待する名前をその場で導いて既存の制限付き読込とcodecを結ぶ。拡張子/underscore等で非正規なファイル名は読込前に拒否し、callerがパスと矛盾する期待名を渡す余地を作らない。ファイルは書かず、失敗はapplicationの閉じた `ThemeFailure`。呼出元は対象pathを保持し、C4bで名前付きの診断へ写す。C4aではUIの挙動は変えない。

## 検証

テーマの名前/所有/codec/失敗/ファイル読込、共通field分割の直接呼出元である既存settings-codecを対象にする。組み込みテーマをシリアライズした例と派生UI、override、4.5境界、寿命、UTF-8・hex・版・重複・未知キー・上限を検査。新しい境界はsymbolsとconformanceで確認。UI、性能、Vim、ファイル保存の不変な検査は再実行しない。

## 段階と帰結

C4aはファイルを既存Themeへ変換する基盤であり、利用者がアプリから選択できる段階とは扱わない。C4bでカタログと設定参照を設計する。新規runtime依存とwaiverは無い。部分テーマ/継承/JSON/TOML/ホットリロードは扱わない。
