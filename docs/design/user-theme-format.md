# 利用者テーマファイル v1

Issue #68 / ADR 0024。C4aで読込と検証の基盤を実装した。アプリのテーマ一覧へ接続するC4bは未実装なので、現時点では配置だけで選べる機能とは扱わない。

予定の配置は `%LOCALAPPDATA%/NeNeNib/themes/<name>.v1.theme`。nameは小文字で始まるASCII小文字・数字・ハイフン、64 bytesまで。underscore入力はハイフンに正規化する。ファイル名と中のnameを一致させ、`system` と組み込みテーマ名は使わない。

UTF-8、LFまたはCRLF、BOM可、最大16KiB。`key=value` を一行ずつ書く。空行と行端のASCII空白は許可する。コメント構文は無い。キーの重複、未知のキー、必須項目の欠落はエラーになる。ファイルを勝手に修復・上書きしない。

本文16色はすべて必要。色は `#RRGGBB`。UIは背景・前景・cursor色と明暗から導出する。`ui.<役割>` を追加するとその色だけ上書きできる。`ui.selection` と `ui.search` だけは `#RRGGBBAA`（末尾が不透明度）で書く。16進数のA〜Fは大小どちらでもよい。

```text
version=1
name=my-theme
appearance=dark
author=hide
license=MIT
url=local

body.foreground=#EEEEEC
body.background=#300A24
body.cursor=#E95420
body.selection=#6B203F
body.current_line=#3E1A32
body.line_number=#7A6675
body.comment=#B8A9B3
body.keyword=#F0A47A
body.string=#8AE234
body.number=#AD7FA8
body.type=#729FCF
body.function=#FCE94F
body.constant=#E9B96E
body.operator=#34E2E2
body.error=#EF2929
body.warning=#FCAF3E

ui.title_bar=#1E0516
ui.selection=#E9542047
```

`appearance` はdarkまたはlight。author/license/urlは非空の一行UTF-8、各256 bytesまで。出典URLは文字列として保持し、アプリがアクセスすることはない。ローカル作品には例のように `url=local` と書ける。licenseには配色の実際の利用条件を書く。

上書きできるUI役割は `background text muted gutter current_line title_bar tab_active status accent selection toggle on_accent panel panel_border search ime`。本文のforeground/backgroundと、上書き後のUIのtext/backgroundのコントラスト比は4.5以上が必要。低い場合は色を自動補正せずエラーとして返す。

読込結果は名前・明暗・全色・出典を所有する `ThemeDocument`。UI・設定との接続は次の段階で、この同じ検証結果を使う。
