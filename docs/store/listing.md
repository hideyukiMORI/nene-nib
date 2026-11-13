# Microsoft Store の掲載文 — NeNe Nib 1.0.0

> Issue #1582・施主決定 D88。Partner Center への入力は hide が行う。ここは入力する文面の正本である。
> 欄の上限は [Microsoft の文書](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/add-and-edit-store-listing-info)
> （2026-11-13 に読んだ）と、NeNe Loupe の入力で分かったこと（Loupe の `docs/release/store-listing.md`）に合わせた。
> 申請の手順と申告の答えは [submission.md](submission.md)、スクリーンショットは [screenshots.md](screenshots.md)。

製品名 `NeNe Nib`・Store ID `9NCLGDTFQ529`・発行者の表示名 `Hideyuki Mori`・版 1.0.0（GitHub の ZIP も同じ素の版 `1.0.0` で、試験版であることは Release の題の「Beta」と pre-release の印で示す・#1596）。
掲載の言語は日本語・英語・中国語（簡体字）の 3 つ（D88）。

## 書き方の約束

- 各文は README の「What works today」と突き合わせた。**README に無い機能は書かない。** 予定の機能（補完・設定画面・
  プロジェクト全体の検索・エディタの中の右クリックのメニュー）は書かない（`:s` は beta.13 から入ったので、この一覧から外した。掲載文には書いていない）。
- 説明の欄には HTML・コード例・URL を入れない（Microsoft の文書）。サポートとプライバシーの URL は専用の欄に入れる。
- 製品の機能の各項目には箇条書きの記号を付けない（Store が付ける）。
- 最上級（「最速」「fastest」）を使わない。速さは測った値と、測った機械を添えて書く（脚注 [^speed]）。
- **貼る前の確かめ**: 「表示は日本語・英語・中国語（簡体字）」の文は、表示言語（#1019）が main に入り（T1・beta.13 で公開済み）、申請する 1.0.0 の exe で
  3 言語が切り替わることを確かめてから貼る。切り替わらなければ、その文を「表示は日本語と英語」などの事実に直す。
- 「通常」の言い方は施主決定 D89（画面の名前は英語 `Basic`・中国語 `标准`）。中国語の文は画面と同じ `标准编辑` と書き、`普通` を使わない（Vim の「普通模式」と紛れる）。英語の説明の文は画面の名前を引かない所なので ordinary editing のまま。
- 端末の表示からではなく、このファイルをエディタで開いてコピーする（端末からは折り返しの位置に改行が入る）。

## 欄の上限（Microsoft の文書・2026-11-13）

| 欄 | 上限 | この原稿 |
| --- | --- | --- |
| 説明 | 10,000 文字・必須・平文 | 各言語 約 1,000〜2,000 文字 |
| 短い説明 | 1,000 文字（表示は最初の 270 文字。270 未満を推奨） | 各言語 270 文字未満 |
| 製品の機能 | 1 項目 200 文字・20 項目まで | 各言語 14 項目 |
| 検索語 | 7 個・各 40 文字・全体で 21 単語（Loupe の入力画面の表示） | 各言語 7 個 |
| このバージョンの新機能 | 1,500 文字。**初回の申請では空欄にする**（Microsoft の文書） | 空欄。下の文は 1.1.0 以後の書き方の控え |
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
Ctrl+Shift+M で、編集中の Markdown を隣の欄に整えて表示します。もう一度押すと全面の表示、さらに押すと閉じます。端末の中の画像（PNG・JPEG・GIF の最初のコマ・BMP）を本文の流れの中に表示し、タスクリストのチェックボックスはクリックで切り替えられます。

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
・F1 で全部の操作を一覧から探して実行
・言語サーバーの診断（既定はオフ。設定に書いたサーバーを起動します）

■ プライバシー
アカウントは不要です。利用状況を送らず、アプリ自身はネットワークに接続しません。設定と履歴は端末の中に保存します。

■ ご注意
・動作環境: Windows 11（22H2 以降・x64）。Windows 10 には対応していません。
・表示は日本語・英語・中国語（簡体字）です。
・言語サーバーは診断の表示だけで、補完・ホバー・定義へ移動はまだありません。
・Vim の置換コマンド（:s）、プロジェクト全体の検索、設定の画面、エディタの中の右クリックのメニューはまだありません。
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

初回の申請では空欄にする。1.1.0 からは変わった点を 1,500 文字以内で書く（例: `表示言語に韓国語・スペイン語・ドイツ語を加えました。`）。

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
Press Ctrl+Shift+M to see the Markdown you are editing, rendered in the next pane. Press it again for a full view, and once more to close it. Local images (PNG, JPEG, the first frame of a GIF, BMP) appear in the text, and task-list checkboxes can be clicked.

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
• F1 lists every operation, so you can find and run any of them
• Language-server diagnostics (off by default; starts only the server you configure)

Privacy
No account is needed. The app sends no usage data and makes no network connections of its own. Settings and history stay on your device.

Good to know
• Requires Windows 11 (version 22H2 or later, x64). Windows 10 is not supported.
• The interface is available in Japanese, English and Simplified Chinese.
• The language server shows diagnostics only; completion, hover and go-to-definition are not available yet.
• Vim's substitute command (:s), project-wide search, a settings dialog and a right-click menu inside the editor are not available yet.
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

Leave blank for the first submission (Microsoft's guidance). From 1.1.0, describe the changes in up to 1,500
characters (for example `Adds Korean, Spanish and German to the interface languages.`).

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
按 Ctrl+Shift+M，即可在旁边的窗格中查看正在编辑的 Markdown 的排版效果。再按一次切换为全屏显示，再按一次关闭。本地图片（PNG、JPEG、GIF 的第一帧、BMP）显示在正文中，任务列表的复选框可以点击切换。

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
• 按 F1 列出全部操作，可以查找并执行
• 语言服务器的诊断（默认关闭，只启动您在设置中配置的服务器）

隐私
无需账号。不发送使用情况数据，应用本身不连接网络。设置和历史记录保存在您的设备上。

注意事项
• 系统要求：Windows 11（22H2 或更高版本，x64）。不支持 Windows 10。
• 界面支持日语、英语和简体中文。
• 语言服务器目前只显示诊断，尚不支持补全、悬停提示和转到定义。
• 尚不支持 Vim 的替换命令（:s）、整个项目范围的搜索、设置对话框和编辑器内的右键菜单。
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

首次提交时留空（Microsoft 的说明）。从 1.1.0 起，用 1,500 字以内说明变更。

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
    ほかの機械では違う値になる。1.0.0 の exe で値が大きく変わったら、この文を測り直した値に直す。
