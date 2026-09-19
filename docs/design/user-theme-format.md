# 利用者テーマファイル v1

Issue #68 / #70、ADR 0024 / 0025。起動時に読み込み、NORMALの `:colorscheme <name>` またはCtrl+Pのテーマ名検索から選べる。選択はsettings.v1へ保存され、次回起動時にも同じ配色を復元する。

配置は `%LOCALAPPDATA%/NeNeNib/themes/<name>.v1.theme`。nameは小文字で始まるASCII小文字・数字・ハイフン、64 bytesまで。underscore入力はハイフンに正規化する。ファイル名と中のnameを一致させ、`system` と組み込みテーマ名は使わない。

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

フォルダの直下だけを起動時に一度読む。最大128ファイルで、超えた場合は利用者テーマを一件も読み込まず注意を表示する。フォルダ列挙が途中で失敗した場合も、不完全な一覧を採用せず組み込みだけで起動し注意を表示する。ファイルの追加・編集は再起動で反映する。正規のファイル名はハイフン表記のみで、underscoreはコマンド入力で使える別表記。組み込みとsystemは予約名。

正規名の壊れたファイルは一覧に残り、選択時に名前と理由を表示して元の配色を保つ。不正なファイル名やフォルダの読込失敗は起動時の左ステータスに表示する。ファイル引数付き起動でも表示し、次の操作で消える。

保存されたテーマが欠落・破損していたら、名前付きの警告を出して既定配色で起動する。元のsettings.v1を保護し、その起動中は設定を保存しない。テーマを修正して再起動する。`colorscheme system` は正常な設定状態からOS追従へ戻す操作。

読込結果を所有する `ThemeDocument` は不変のカタログと選択参照で共有する。設定・Ex・Ctrl+Pは同じカタログを使い、描画のたびにファイルを読まない。
