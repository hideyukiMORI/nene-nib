# Microsoft Store の掲載文 — NeNe Nib 1.1.0

> Issue #1582・施主決定 D88。Partner Center への入力は hide が行う。ここは入力する文面の正本である。
> 欄の上限は [Microsoft の文書](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/add-and-edit-store-listing-info)
> （2026-11-13 に読んだ）と、NeNe Loupe の入力で分かったこと（Loupe の `docs/release/store-listing.md`）に合わせた。
> 申請の手順と申告の答えは [submission.md](submission.md)、スクリーンショットは [screenshots.md](screenshots.md)。

製品名 `NeNe Nib`・Store ID `9NCLGDTFQ529`・発行者の表示名 `Hideyuki Mori`・版 1.1.0（Store の版は `1.1.0.0`。1.0.0 は 2026-11-15 に初回の申請をして公開済みで、1.1.0 は「更新」の申請・#1815。GitHub の ZIP も同じ素の版 `1.1.0`）。
掲載の言語は日本語・英語・中国語（簡体字）の 3 つ（D88）。

## 書き方の約束

- 各文は README の「What works today」と突き合わせた。**README に無い機能は書かない。** 予定の機能は書かない。
  1.1.0 で入ったプロジェクトの全文検索・プレビュー欄・本文の右クリックのメニュー・
  イージーモーション・タブの札・自動の字下げは説明と製品の機能に足し、「ご注意」の `:s` の族は 1.1.0 の形（`c` フラグ・
  `:d` `:y`・`:g` `:v` の `d` と `s` は有り、ほかの `:g` と `:sort` は無し）に直した（#1815）。
  区切りをまとめ枝 46 に移したとき（#1891）、設定の面・補完・言語サーバーの情報と定義へ・Markdown の表と alert と
  囲みのコードの色とリンクの札・タブの右クリックのメニューを説明と製品の機能に足し、「まだ無い」と書いていた
  「ご注意」の 2 行（言語サーバーは診断だけ・設定の画面とタブのメニュー）を消した。
- F1「Windows の右クリックに登録する」は掲載文に書かない。Store 版（MSIX）ではエクスプローラーに出ないことを 1.0.0 で
  実機で確かめ（submission.md の「packaged の実機の確かめ」）、1.1.0 から Store 版の F1 の一覧に出さない（#1809）。
- 説明の欄には HTML・コード例・URL を入れない（Microsoft の文書）。サポートとプライバシーの URL は専用の欄に入れる。
- 製品の機能の各項目には箇条書きの記号を付けない（Store が付ける）。
- 最上級（「最速」「fastest」）を使わない。速さは測った値と、測った機械を添えて書く（脚注 [^speed]）。
- **貼る前の確かめ**: 「表示は日本語・英語・中国語（簡体字）」の文は 1.0.0 で確かめた。1.1.0 でも表示言語は同じ 3 つ
  （韓国語・スペイン語・ドイツ語は後の版・施主決定 D100）。
  新しく足した文（全文検索・プレビュー欄・右クリックのメニュー・イージーモーション・設定の面・補完・言語サーバー・
  Markdown の表とリンク）は、申請する 1.1.0 の exe で動くことを
  施主の試用（`D:/NeNeNib/evidence/v1.1.0-release/hide-checklist.md`）で確かめてから貼る。
- 「通常」の言い方は施主決定 D89（画面の名前は英語 `Basic`・中国語 `标准`）。中国語の文は画面と同じ `标准编辑` と書き、`普通` を使わない（Vim の「普通模式」と紛れる）。英語の説明の文は画面の名前を引かない所なので ordinary editing のまま。
- 端末の表示からではなく、このファイルをエディタで開いてコピーする（端末からは折り返しの位置に改行が入る）。

## 欄の上限（Microsoft の文書・2026-11-13）

| 欄 | 上限 | この原稿 |
| --- | --- | --- |
| 説明 | 10,000 文字・必須・平文 | 1.1.0 の文で数え直した値は `D:/NeNeNib/evidence/v1.1.0-release/store-listing/counts.md`（#1815） |
| 短い説明 | 1,000 文字（表示は最初の 270 文字。270 未満を推奨） | 各言語 270 文字未満 |
| 製品の機能 | 1 項目 200 文字・20 項目まで | 各言語 20 項目（1.1.0 で 6 項目足した・上限に届いた） |
| 検索語 | 7 個・各 40 文字・全体で 21 単語（Loupe の入力画面の表示） | 各言語 7 個 |
| このバージョンの新機能 | 1,500 文字。初回の申請（1.0.0）では空欄にした（Microsoft の文書） | 1.1.0 の文（各言語 1,500 文字未満・数は counts.md） |
| スクリーンショットの説明 | 200 文字 | screenshots.md |

---

## 日本語（ja-JP）

### 製品名

```text
NeNe Nib
```

### 短い説明

```text
すぐ起動して軽く打てる、Windows のテキストエディタ。メモ帳のような普通の操作と Vim の操作をキー 1 つで切り替えられ、Markdown のプレビューも横に並べて見られます。
```

### 説明

```text
NeNe Nib は、Windows 11 のための軽いテキストエディタです。1 つの実行ファイルだけで動き、追加のランタイムは要りません。

■ 軽さ
開発者の PC での実測で、起動から最初の画面まで約 40 ms、1 回の打鍵が画面に出るまで 1 ms 未満です。16 MiB のファイルも約 50 ms で開きます。最大 1 GiB の UTF-8 のファイルは裏で読み込むので、読み込みの間もほかのタブを使えます。

■ 普通の操作と Vim の操作を切り替え
メモ帳と同じ感覚の普通の操作（検索・置換・行へ移動・印刷・日時の挿入）と、Vim の操作を Ctrl+Shift+Space かステータスバーのクリックで切り替えます。Vim の操作は本物の Vim 9.1 の動きと照らし合わせて作っています。移動・オペレータ・テキストオブジェクト・検索・マーク・ジャンプの列・ドットの繰り返し・マクロ・レジスタ・矩形の選択・map に対応します。

■ Markdown プレビュー
Ctrl+Shift+M で、編集中の Markdown を隣の欄に整えて表示します。もう一度押すと全面の表示、さらに押すと閉じます。端末の中の画像（PNG・JPEG・GIF の最初のコマ・BMP）を本文の流れの中に表示し、タスクリストのチェックボックスはクリックで切り替えられます。表と GitHub の alert（NOTE・TIP など）・打ち消し線も表示し、枠より広い表は横に送れます。囲みのコードには本文と同じ構文の色が付きます。リンクをクリックすると本当の宛先を小さな札で見せ、確かめてから既定のブラウザやメールで開きます。#見出し のリンクはその見出しへ跳びます。

■ プロジェクトの全文検索とプレビュー
Ctrl+Shift+F で、エクスプローラーの起点のフォルダ（無ければ開いている文書のフォルダ）の下のファイルを全文検索します。打つたびに探し直し、見つかった行が届いた順に並びます。Enter でそのファイルのその行を開きます。Ctrl+P の一覧では、選んだ候補の中身を右か下のプレビュー欄に表示します（Ctrl+Shift+P で右・下・なしを切り替え）。

■ そのほかの機能
・タイトルバーのタブ。次の起動で前回のタブが戻ります
・Ctrl+P で、開いているタブ・ブックマーク・最近閉じたファイル・同じフォルダのファイルをまとめて探せます
・左側のファイルエクスプローラー（Ctrl+Shift+E）
・左右・上下の 2 分割
・125 種類の組み込みのテーマ。Windows のライト／ダークに従います
・25 言語の構文の色分け
・UTF-8・Shift_JIS・UTF-16 と、CRLF・LF を読んだ形のまま保存
・日本語入力（IME）に対応
・30 秒ごとの自動退避。異常終了の後に 1 回だけ戻すかを聞きます
・F1 の一覧から、ファイル・タブ・表示・文字コード・検索・印刷などの操作を探して実行（Vim の鍵は一覧の外）
・本文の右クリックのメニュー（切り取り・コピー・貼り付け・削除・すべて選択・検索・置換）。Vim のモードでも使えます
・タブの右クリックのメニュー（エクスプローラーで場所を開く・パスをコピー・ほかのタブを閉じる・右のタブを閉じるなど）
・設定の面（Ctrl+,）。窓の右に開き、配色・字体・文字の大きさなどを探して切り替えると、その場で本文に効きます
・補完（通常の操作は Ctrl+Space・Vim の INSERT は Ctrl-n）。開いている文書の語から候補を出します
・タブにマウスを載せると、そのファイルのフルパスを小さな札で表示
・Vim のイージーモーション: バックスラッシュを 2 回打ってから w・b・e・j・k・l や f・F・s と文字を打つと、見えている跳び先に小さな札が出て、札の文字を打つとそこへ跳びます
・Vim の自動の字下げ（:set autoindent）
・言語サーバーの診断・補完・情報の表示・定義へ移動（既定はオフ。設定に書いたサーバーを起動します）

■ プライバシー
アカウントは不要です。利用状況を送らず、アプリ自身はネットワークに接続しません。Markdown プレビューのリンクは、札で確かめた後に既定のブラウザかメールに渡すだけです。設定と履歴は端末の中に保存します。

■ ご注意
・動作環境: Windows 11（22H2 以降・x64）。Windows 10 には対応していません。
・表示は日本語・英語・中国語（簡体字）です。
・Vim の置換（:s・:&・&・g&）と、1 件ずつ確かめながら置換する c フラグ、:d・:y、:g と :v（d と s）に対応しています。:g のほかの命令と :sort はまだありません。
・全文検索は UTF-8 のファイルだけを探します（Shift_JIS などのファイルの中は探しません）。
・MIT License のオープンソースです。
```

### 製品の機能

```text
起動から最初の画面まで約 40 ms、1 回の打鍵の表示は 1 ms 未満（開発者の PC での実測）
普通の操作と Vim の操作を Ctrl+Shift+Space で切り替え
本物の Vim 9.1 と照らし合わせて作った Vim の操作（移動・オペレータ・テキストオブジェクト・マクロ・レジスタ・矩形の選択）
Markdown プレビュー（隣の欄・全面）。端末の中の画像とチェックボックスに対応
タイトルバーのタブと前回のタブの復元
Ctrl+P でタブ・ブックマーク・最近のファイル・同じフォルダのファイルを探す
ファイルエクスプローラーと 2 分割の表示
125 種類のテーマ。Windows のライト／ダークに従う
25 言語の構文の色分け
UTF-8・Shift_JIS・UTF-16 と改行の形を読んだまま保存。最大 1 GiB の UTF-8 を裏で読み込み
検索・置換・行へ移動・日時の挿入はメモ帳と同じ鍵。印刷は F1 の一覧から
日本語入力（IME）に対応
30 秒ごとの自動退避
アカウント不要・利用状況の送信なし・アプリ自身は通信しない
Ctrl+Shift+F でフォルダの下のファイルを全文検索。選んだ候補の中身をプレビュー欄に表示
本文の右クリックのメニュー（切り取り・コピー・貼り付けなど）。Vim のモードでも使える
Vim のイージーモーション。見えている跳び先に小さな札を出して、札の文字で跳ぶ
設定の面（Ctrl+,）。切り替えはその場で本文に効き、閉じると保存
補完（Ctrl+Space・Vim の INSERT の Ctrl-n）と、言語サーバーの情報の表示・定義へ移動
Markdown の表・alert・囲みのコードの色。リンクは宛先を札で確かめてから開く
```

### 検索語

```text
テキストエディタ
Vim
Markdown
メモ帳
エディタ
プログラミング
軽量
```

### このバージョンの新機能

1.0.0 の初回の申請では空欄にした。1.1.0 の文（1,500 文字以内）:

```text
・プロジェクトの全文検索: Ctrl+Shift+F（Vim では :grep も）で、エクスプローラーの起点のフォルダ（無ければ開いている文書のフォルダ）の下のファイルを探します。打つたびに探し直し、Enter で見つかった行を開きます。
・Ctrl+P の一覧にプレビュー欄: 選んだ候補の中身を右か下に表示します。Ctrl+Shift+P で右・下・なしを切り替えます。
・設定の面: Ctrl+, で窓の右に開き、配色・字体・文字の大きさなどを探して切り替えると、その場で本文に効きます。閉じると保存します。
・補完: 通常モードでは Ctrl+Space（Ctrl+I でも）、Vim の INSERT では Ctrl-n で、打っている語の候補を出します。
・言語サーバー（既定はオフ）: 補完の候補に加えて、Vim の K で情報を表示し、gd か F12 で定義へ移動します。
・Markdown プレビュー: 表と GitHub の alert（NOTE・TIP など）、打ち消し線を表示します。広い表は横に送れます。囲みのコードに本文と同じ構文の色が付きます。リンクはクリックすると宛先を札で見せ、確かめてから開きます。#見出し のリンクはその見出しへ跳びます。
・タブの右クリックのメニュー: エクスプローラーで場所を開く・パスをコピー・ファイル名をコピー・保存・ほかのタブを閉じる・右のタブを閉じるなど。
・本文の右クリックのメニュー: 切り取り・コピー・貼り付け・削除・すべて選択・検索・置換。Shift+F10 とメニューキーでも開き、Vim のモードでも使えます。
・Vim のイージーモーション: バックスラッシュを 2 回打ってから w・b・e・j・k・l や f・F・s と文字を打つと、見えている跳び先に小さな札が出て、札の文字でそこへ跳びます。
・タブにマウスを載せると、フルパスを小さな札で表示します。
・Vim: 自動の字下げ（:set autoindent）、:s の c フラグ、:d・:y・:g・:v、検索の \zs と \ze、NORMAL の s と S、: と / の行の Ctrl+U と Ctrl+W。改行で終わるファイルの行の数を Vim と同じにしました。
・直し: Markdown プレビューの長い段落で止まることがあったのを直しました。1 つの字を長く繰り返した巨大な行を格子表示にしたとき止まることがあったのを直しました。Windows 11 より前の Windows で nul.txt のような名前のファイルの中身が失われることがあったのを直しました。
・Store 版では、効かない「Windows の右クリックに登録する」と「登録を外す」を F1 の一覧に出さないようにしました。
```

---

## 英語（en-US）

### Product name

```text
NeNe Nib
```

### Short description

```text
A lightweight text editor for Windows that opens instantly and keeps up with your typing. Switch between ordinary Notepad-style editing and Vim editing with one key, and preview Markdown side by side.
```

### Description

```text
NeNe Nib is a lightweight text editor for Windows 11. It is a single executable with no extra runtime to install.

Light and quick
Measured on the developer's PC, the first screen appears about 40 ms after launch, and a keystroke reaches the screen in under 1 ms. A 16 MiB file opens in about 50 ms. UTF-8 files of up to 1 GiB are read in the background, so your other tabs stay usable while they load.

Ordinary editing or Vim editing
Switch between ordinary editing, which works like Notepad (find, replace, go to line, print, insert date and time), and Vim editing with Ctrl+Shift+Space or a click on the status bar. The Vim editing is built from scratch and checked against the real Vim 9.1. It covers motions, operators, text objects, search, marks, the jump list, dot repeat, macros, registers, blockwise selection and map.

Markdown preview
Press Ctrl+Shift+M to see the Markdown you are editing, rendered in the next pane. Press it again for a full view, and once more to close it. Local images (PNG, JPEG, the first frame of a GIF, BMP) appear in the text, and task-list checkboxes can be clicked. Tables, GitHub alerts (NOTE, TIP and more) and strikethrough are drawn too, and a table wider than the pane scrolls sideways. Fenced code gets the editor's syntax colors. A click on a link shows its real destination in a small card, and it opens in the default browser or mail app only after you confirm. A #heading link jumps to that heading.

Project-wide search with a preview
Press Ctrl+Shift+F to search the files under the explorer's root folder, or under the folder of the document you are editing. Every change to the search text starts a new search, the matching lines arrive as they are found, and Enter opens the file at that line. In the Ctrl+P list, a preview pane shows the chosen entry's text to the right or below (Ctrl+Shift+P switches between right, below and off).

Also included
• Tabs in the title bar, restored the next time you start
• Ctrl+P finds open tabs, bookmarks, recently closed files and files in the current folder in one list
• A file explorer on the left (Ctrl+Shift+E)
• Two split panes, side by side or one above the other
• 125 built-in themes that follow the Windows light or dark setting
• Syntax colors for 25 languages
• UTF-8, Shift_JIS and UTF-16, with CRLF or LF kept as read
• Japanese input (IME)
• Autosave every 30 seconds, offered back once after an abnormal exit
• F1 lists the commands for files, tabs, view, encodings, find, print and more, so you can find and run them (Vim keys are not in the list)
• A right-click menu in the text (cut, copy, paste, delete, select all, find, replace), also in Vim editing
• A right-click menu on tabs (Reveal in File Explorer, Copy Path, Close Other Tabs, Close Tabs to the Right and more)
• A settings drawer (Ctrl+,) at the right: find and switch themes, fonts, text size and more, applied at once
• Completion (Ctrl+Space in ordinary editing, Ctrl-n in Vim INSERT) from the words of the open documents
• Rest the pointer on a tab to see the file's full path in a small tip
• Vim easymotion: type backslash twice and then w, b, e, j, k or l, or f, F or s and a character, and every visible target gets a small label; type the label to jump there
• Vim autoindent (:set autoindent)
• Language-server diagnostics, completion, information and go-to-definition (off by default; starts only the server you configure)

Privacy
No account is needed. The app sends no usage data and makes no network connections of its own. A link in the Markdown preview is only handed to the default browser or mail app after you confirm it in a small card. Settings and history stay on your device.

Good to know
• Requires Windows 11 (version 22H2 or later, x64). Windows 10 is not supported.
• The interface is available in Japanese, English and Simplified Chinese.
• Vim's substitute (:s, :&, & and g&) is available with its c (confirm) flag, as are :d, :y, and :g and :v with d and s; the other :g commands and :sort are not yet.
• Project-wide search reads UTF-8 files only (it does not search inside Shift_JIS and other files).
• Open source under the MIT License.
```

### Product features

```text
About 40 ms from launch to the first screen, under 1 ms per keystroke (measured on the developer's PC)
Switch between ordinary editing and Vim editing with Ctrl+Shift+Space
Vim editing checked against the real Vim 9.1: motions, operators, text objects, macros, registers, blockwise selection
Markdown preview in the next pane or full view, with local images and clickable checkboxes
Tabs in the title bar, restored on the next start
Ctrl+P finds tabs, bookmarks, recent files and files in the current folder
File explorer and two split panes
125 themes that follow the Windows light or dark setting
Syntax colors for 25 languages
UTF-8, Shift_JIS and UTF-16 with line endings kept as read; UTF-8 files up to 1 GiB load in the background
Find, replace, go to line and insert date and time with Notepad's keys; print from the F1 list
Japanese input (IME)
Autosave every 30 seconds
No account, no usage data, no network connections of its own
Project-wide search with Ctrl+Shift+F, and a preview pane for the chosen entry
A right-click menu in the text (cut, copy, paste and more), also in Vim editing
Vim easymotion: small labels on the visible targets, type one to jump there
A settings drawer (Ctrl+,): changes apply at once and are saved on closing
Completion (Ctrl+Space, Vim INSERT Ctrl-n), and language-server information and go-to-definition
Markdown tables, alerts and colored code blocks; a link shows its real destination before it opens
```

### Search terms

```text
text editor
Vim
Markdown
notepad
code editor
lightweight
Markdown preview
```

### What's new in this version

Left blank for the first submission (1.0.0, Microsoft's guidance). The text for 1.1.0 (up to 1,500 characters):

```text
• Project-wide search: Ctrl+Shift+F (or :grep in Vim) searches the files under the explorer's root or the document's folder.
• A preview pane in the Ctrl+P list (Ctrl+Shift+P: right, below or off).
• Settings: Ctrl+, opens a drawer at the right. Changes apply at once and are saved on closing.
• Completion: Ctrl+Space (or Ctrl+I), or Ctrl-n in Vim INSERT.
• Language server (off by default): completion, K for information, gd or F12 to go to a definition.
• Markdown preview: tables, GitHub alerts, strikethrough, sideways scrolling for wide tables, syntax colors in fenced code; a link shows its real destination in a small card before it opens; #heading links jump.
• Tabs: a right-click menu (Reveal in File Explorer, Copy Path, Close Other Tabs and more) and the full path on hover.
• A right-click menu in the text (also Shift+F10 and the menu key, in Vim too): cut, copy, paste, delete, select all, find, replace.
• Vim easymotion: backslash twice, then w, b, e, j, k, l, or f, F, s and a character; typing a label jumps there.
• Vim: :set autoindent, :s with the c flag, :d, :y, :g, :v, \zs and \ze, NORMAL s and S, Ctrl+U and Ctrl+W on the : and / lines; line counts match Vim for files that end with a line break.
• Fixes: stalls on a long Markdown paragraph or a giant line in the grid display; before Windows 11, a file named like nul.txt could lose its text.
• In the Store version, F1 no longer lists the two Windows right-click menu operations (they cannot work there).
```

---

## 中国語（簡体字・manifest の札は zh-Hans）— 機械の訳・レビュー歓迎

> この節は設計席が日本語と英語から訳した文で、中国語を母語とする人のレビューを受けていない。
> 誤りや不自然な所は GitHub の Issue で知らせてほしい（Machine translation; reviews are welcome via GitHub issues）。

### 产品名称

```text
NeNe Nib
```

### 简短说明

```text
启动迅速、输入流畅的 Windows 文本编辑器。只需一个按键，即可在类似记事本的标准编辑和 Vim 编辑之间切换，还能并排预览 Markdown。
```

### 说明

```text
NeNe Nib 是一款适用于 Windows 11 的轻量文本编辑器。只有一个可执行文件，无需安装额外的运行库。

轻巧迅速
在开发者的电脑上实测，从启动到显示第一个画面约 40 毫秒，一次按键显示到屏幕上不到 1 毫秒。打开 16 MiB 的文件约需 50 毫秒。最大 1 GiB 的 UTF-8 文件在后台读取，读取期间其他标签页仍可正常使用。

标准编辑与 Vim 编辑
按 Ctrl+Shift+Space 或点击状态栏，即可在标准编辑（与记事本相同的操作：查找、替换、转到行、打印、插入日期和时间）和 Vim 编辑之间切换。Vim 编辑从零开始实现，并与真正的 Vim 9.1 进行对照验证。支持移动、操作符、文本对象、搜索、标记、跳转列表、点号重复、宏、寄存器、块选择和 map。

Markdown 预览
按 Ctrl+Shift+M，即可在旁边的窗格中查看正在编辑的 Markdown 的排版效果。再按一次切换为全屏显示，再按一次关闭。本地图片（PNG、JPEG、GIF 的第一帧、BMP）显示在正文中，任务列表的复选框可以点击切换。还会显示表格、GitHub 的 alert（NOTE、TIP 等）和删除线，较宽的表格可以横向滚动。围栏代码块使用与正文相同的语法着色。点击链接时，先在小卡片中显示真正的目标，确认后才用默认的浏览器或邮件应用打开。#标题 链接会跳转到该标题。

项目全文搜索与预览
按 Ctrl+Shift+F，即可搜索资源管理器的根文件夹（没有时为正在编辑的文档所在的文件夹）下的文件。每次修改搜索文字都会重新搜索，找到的行按到达的顺序列出，按 Enter 即可打开该文件的该行。在 Ctrl+P 的列表中，预览窗格会在右侧或下方显示所选项的内容（按 Ctrl+Shift+P 在右侧、下方和关闭之间切换）。

其他功能
• 标题栏中的标签页，下次启动时自动恢复
• 按 Ctrl+P 在一个列表中查找打开的标签页、书签、最近关闭的文件和当前文件夹中的文件
• 左侧的文件资源管理器（Ctrl+Shift+E）
• 左右或上下两个分割窗格
• 125 种内置主题，跟随 Windows 的浅色或深色设置
• 25 种语言的语法高亮
• 支持 UTF-8、Shift_JIS 和 UTF-16，按读取时的形式保留 CRLF 或 LF
• 支持日语输入法（IME）
• 每 30 秒自动备份，异常退出后下次启动时询问一次是否恢复
• 按 F1 列出文件、标签页、显示、编码、查找、打印等操作，可以查找并执行（Vim 按键不在列表中）
• 正文的右键菜单（剪切、复制、粘贴、删除、全选、查找、替换），Vim 编辑中也可使用
• 标签页的右键菜单（在文件资源管理器中显示、复制路径、关闭其他标签页、关闭右侧的标签页等）
• 设置面板（Ctrl+,）在窗口右侧打开，搜索并切换配色、字体、字号等，正文会立即生效
• 补全（标准编辑中按 Ctrl+Space，Vim 的 INSERT 中按 Ctrl-n），从打开的文档的单词中显示候选
• 鼠标停在标签页上时，以小提示显示文件的完整路径
• Vim 的 easymotion：连按两次反斜杠后输入 w、b、e、j、k、l，或输入 f、F、s 加一个字符，可见的跳转目标上会出现小标签，输入标签即可跳到该处
• Vim 的自动缩进（:set autoindent）
• 语言服务器的诊断、补全、信息显示和转到定义（默认关闭，只启动您在设置中配置的服务器）

隐私
无需账号。不发送使用情况数据，应用本身不连接网络。Markdown 预览中的链接，只在您于小卡片中确认后交给默认的浏览器或邮件应用。设置和历史记录保存在您的设备上。

注意事项
• 系统要求：Windows 11（22H2 或更高版本，x64）。不支持 Windows 10。
• 界面支持日语、英语和简体中文。
• 支持 Vim 的替换命令（:s、:&、&、g&）及逐个确认的 c 标志，以及 :d、:y 和带 d、s 的 :g、:v；尚不支持其他 :g 命令和 :sort。
• 项目全文搜索只搜索 UTF-8 文件（不搜索 Shift_JIS 等文件的内容）。
• 基于 MIT 许可证的开源软件。
```

### 产品功能

```text
从启动到第一个画面约 40 毫秒，每次按键的显示不到 1 毫秒（在开发者的电脑上实测）
按 Ctrl+Shift+Space 在标准编辑和 Vim 编辑之间切换
与真正的 Vim 9.1 对照验证的 Vim 编辑：移动、操作符、文本对象、宏、寄存器、块选择
Markdown 预览（旁边的窗格或全屏），支持本地图片和可点击的复选框
标题栏中的标签页，下次启动时自动恢复
按 Ctrl+P 查找标签页、书签、最近的文件和当前文件夹中的文件
文件资源管理器和两个分割窗格
125 种主题，跟随 Windows 的浅色或深色设置
25 种语言的语法高亮
支持 UTF-8、Shift_JIS 和 UTF-16，保留原有换行符；最大 1 GiB 的 UTF-8 文件在后台读取
使用与记事本相同的按键查找、替换、转到行和插入日期时间；从 F1 列表打印
支持日语输入法（IME）
每 30 秒自动备份
无需账号，不发送使用情况数据，应用本身不连接网络
按 Ctrl+Shift+F 搜索文件夹下的文件，并在预览窗格中显示所选项的内容
正文的右键菜单（剪切、复制、粘贴等），Vim 编辑中也可使用
Vim 的 easymotion：在可见的跳转目标上显示小标签，输入标签即可跳转
设置面板（Ctrl+,），切换立即生效，关闭时保存
补全（Ctrl+Space、Vim 的 INSERT 中的 Ctrl-n），以及语言服务器的信息显示和转到定义
Markdown 的表格、alert 和代码块着色；链接先在小卡片中确认目标再打开
```

### 搜索词

```text
文本编辑器
Vim
Markdown
记事本
代码编辑器
轻量
Markdown 预览
```

### 此版本中的新增功能

首次提交（1.0.0）时留空（Microsoft 的说明）。1.1.0 的文字（1,500 字以内）：

```text
• 项目全文搜索：按 Ctrl+Shift+F（Vim 中也可用 :grep），搜索资源管理器的根文件夹（没有时为当前文档所在的文件夹）下的文件。每次修改文字都会重新搜索，按 Enter 打开找到的行。
• Ctrl+P 的列表新增预览窗格，在右侧或下方显示所选项的内容。按 Ctrl+Shift+P 在右侧、下方和关闭之间切换。
• 设置：按 Ctrl+, 在窗口右侧打开设置面板。搜索并切换配色、字体、字号等，正文会立即生效。关闭时保存。
• 补全：标准模式下按 Ctrl+Space（也可用 Ctrl+I），Vim 的 INSERT 中按 Ctrl-n，显示正在输入的单词的候选。
• 语言服务器（默认关闭）：除补全候选外，可在 Vim 中按 K 显示信息，按 gd 或 F12 转到定义。
• Markdown 预览：显示表格、GitHub 的 alert（NOTE、TIP 等）和删除线。较宽的表格可以横向滚动。围栏代码块使用与正文相同的语法着色。点击链接时先在小卡片中显示目标，确认后再打开。#标题 链接会跳转到该标题。
• 标签页的右键菜单：在文件资源管理器中显示、复制路径、复制文件名、保存、关闭其他标签页、关闭右侧的标签页等。
• 正文的右键菜单：剪切、复制、粘贴、删除、全选、查找和替换。也可用 Shift+F10 和菜单键打开，Vim 编辑中也可使用。
• Vim 的 easymotion：连按两次反斜杠后输入 w、b、e、j、k、l，或输入 f、F、s 加一个字符，可见的跳转目标上会出现小标签，输入标签即可跳到该处。
• 鼠标停在标签页上时，以小提示显示完整路径。
• Vim：自动缩进（:set autoindent）、:s 的 c 标志、:d、:y、:g、:v、搜索模式中的 \zs 和 \ze、NORMAL 的 s 和 S，以及 : 和 / 行中的 Ctrl+U 和 Ctrl+W。以换行结尾的文件的行数现在与 Vim 相同。
• 修复：Markdown 预览中的长段落可能使编辑器停顿；在网格显示中，重复同一个字符的超长行可能使编辑器停顿；在 Windows 11 之前的 Windows 上，名称类似 nul.txt 的文件的内容可能丢失。
• 在 Store 版中，F1 列表不再显示无法使用的「添加到 Windows 右键菜单」和「从 Windows 右键菜单中移除」。
```

---

## 3 言語に共通の欄

| 欄 | 入れる内容 | 根拠 |
| --- | --- | --- |
| 著作権と商標の情報 | `© 2026 Hideyuki Mori` | `LICENSE`（MIT） |
| サポートの連絡先 | `https://github.com/hideyukiMORI/nene-nib/issues` | PRIVACY.md の連絡先と同じ |
| Web サイト | `https://github.com/hideyukiMORI/nene-nib` | |
| プライバシーポリシーの URL | `https://github.com/hideyukiMORI/nene-nib/blob/main/PRIVACY.md` | `PRIVACY.md`（このリポジトリの直下） |
| 追加のライセンス条項 | 空欄（MIT は説明の末尾で触れる。md4c の表記は exe の配布物の `THIRD_PARTY_NOTICES.txt`） | ADR 0151 決定 11 |

## 数字の出典

[^speed]: 起動 約 40 ms・1 打鍵 1 ms 未満・16 MiB を開く 約 50 ms は、
    [docs/quality/speed-adoption-2026-10-18.md](../quality/speed-adoption-2026-10-18.md) の 5 試行の中央値
    （`startup-first-frame` 41.886 ms・`key-to-frame-single` 0.641 ms・`open-large-file-16mib` 49.789 ms）。
    測った機械は施主の PC（Intel Core i9-10850K・NVIDIA GeForce RTX 3090・表示の倍率 125%）、
    Release の exe（main `b3e6f01`）、道具は `eng/measure-speed.py`。速さのゲートはこの値を基準に退行を落とす（QLT-014）。
    ほかの機械では違う値になる。申請する版の exe で値が大きく変わったら、この文を測り直した値に直す（1.1.0 のまとめ枝のゲートは、44 の起動と 16 MiB を開くの 2 本が上限を越えて同じ時間帯の main との A/B で機械の状態と示したほかは 8 本とも 0 regression・46 は起動 44.991 ms・16 MiB を開く 51.071 ms・公開の記録 `release-1.1.0-2026-11-17.md`）。
