# Privacy Policy — NeNe Nib

Last updated: 2026-11-14 · Publisher: Hideyuki Mori

**NeNe Nib does not collect or transmit any personal information. The application itself makes no network
connections. Everything it saves stays on your device.**

日本語は[このページの後半](#プライバシーポリシー日本語)にあります。

## What the application does with data

| Data | What happens | Where it goes |
| --- | --- | --- |
| Your documents | The application reads the files you open and writes the files you save. When saving, it first writes a temporary file next to the target (`<file>.nib-tmp-<process id>-<number>`) and then replaces the target. | The files and folders you choose. Nothing is sent anywhere. |
| Unsaved edits | Every 30 seconds, the text of tabs with unsaved changes is copied to a local folder so it can be offered back once if the application ends abnormally. The copy is deleted when the window closes with no unsaved changes left, or when you choose not to save them; otherwise it is kept for the next start. A document larger than 64 MiB is not copied. | `%LOCALAPPDATA%\NeNeNib\autosave\` on your device. |
| Settings | Theme, font, wrapping and other display choices, the explorer root folder, Vim `map` lines and, if you add them, language-server commands. | `%LOCALAPPDATA%\NeNeNib\settings.v4` on your device. |
| Tabs, history and bookmarks | The paths of the tabs open when the window closed (with caret and scroll positions), up to 100 recently closed files, and up to 1024 bookmarks. | `session.v2`, `history.v1` and `bookmarks.v1` in `%LOCALAPPDATA%\NeNeNib\`. |
| Clipboard | The clipboard is read or written only when you copy, cut or paste (including Vim's `"+` and `"*` registers). | Your clipboard, on your device. |
| Windows settings | The application reads the Windows light/dark setting, the Windows display language (to choose the language of its own screen text when the setting says to follow Windows) and the list of installed WSL distributions (to offer them as folders). It does not change them. | Not stored. |
| Explorer registration | Only when you choose "Add to Windows Right-Click Menu" from the operation list (F1), the application adds itself to the File Explorer right-click menu under `HKEY_CURRENT_USER`. "Remove from Windows Right-Click Menu" removes it. Nothing is written at startup. This describes the ZIP version: in the Microsoft Store version, Windows is expected to keep this write in the application's own package storage, so the entry would probably not appear in File Explorer. That has not been checked on a real machine yet. | The Windows registry of your user account. |

The application has no accounts, no telemetry, no analytics, no advertising, no crash reporting, no automatic
update check of its own, and no third-party services.

## The files in `%LOCALAPPDATA%\NeNeNib`

These files contain only what is listed above: settings, file paths with positions, and (for autosave) the text
of your unsaved tabs. They stay on your device until you delete them; deleting the folder resets the application.

If you install the application from the Microsoft Store, Windows may keep this folder inside the application's own
package storage instead, and removes it when you uninstall the application. If the folder already exists from the
portable version, Windows may let the application use it directly.

## Network

The application makes no network connections. It does not use any networking library: the libraries it may link
are fixed by an allow-list in the build (`eng/architecture.json`), and none of them provides networking. It has no
code that opens web pages, and the Markdown preview shows only images it can read as files; `http` and `https`
images are not downloaded and are shown as their alt text (`[Image: alt]`) instead, and images on a
`\\server\share` path are refused.

Two things can still reach a network because you chose them, not because the application does:

- **Files on a network.** If you open or save a file on a network share (for example `\\server\share`) or print to a
  network printer, Windows accesses that location for you. The same applies to Markdown preview images on a drive
  letter mapped to a network share (for example `Z:\`): the preview reads them as it reads files on your device.
- **Language servers (off by default).** If you turn on `lsp` and add a `lsp.<language>.command=` line in the
  settings, the application starts that program on your device and exchanges the text of your named documents with
  it through standard input and output. Untitled documents are not sent. What that program does is up to that
  program; the application does not start any server you have not configured.

If you install the application from the Microsoft Store, the Store itself — not this application — handles download,
installation and updates. That is covered by the
[Microsoft Privacy Statement](https://privacy.microsoft.com/privacystatement).

## Changes and contact

Changes to this policy are made in this file, and its history is public in the repository.
Questions: open an issue at <https://github.com/hideyukiMORI/nene-nib/issues>.

---

# プライバシーポリシー（日本語）

最終更新: 2026-11-14 · 発行者: Hideyuki Mori

**NeNe Nib は個人情報を収集・送信しません。アプリ自身はネットワークに接続しません。保存するものはすべてお使いの端末の中に
残ります。**

## アプリが扱うデータ

| データ | 何をするか | どこへ行くか |
| --- | --- | --- |
| 文書 | 開いたファイルを読み、保存したファイルを書きます。保存するときは、先に保存先の隣へ一時ファイル（`<ファイル>.nib-tmp-<プロセスの番号>-<番号>`）を書いてから置き換えます。 | 選んだファイルとフォルダ。どこにも送りません。 |
| 未保存の編集 | 30 秒ごとに、未保存の変更があるタブの本文を端末の中のフォルダへ写します。アプリが異常に終わったとき、次の起動で 1 回だけ戻すかを聞くためです。未保存の変更を残さずに窓を閉じたとき、または「保存しない」を選んだときに消します。それ以外の終わり方では次の起動のために残します。64 MiB を超える文書は写しません。 | 端末の `%LOCALAPPDATA%\NeNeNib\autosave\`。 |
| 設定 | テーマ・フォント・折り返しなどの表示の選択、エクスプローラーの起点のフォルダ、Vim の `map` の行、書いた場合は言語サーバーの命令。 | 端末の `%LOCALAPPDATA%\NeNeNib\settings.v4`。 |
| タブ・履歴・ブックマーク | 窓を閉じたときに開いていたタブのパス（キャレットとスクロールの位置つき）、閉じたファイル最大 100 件、ブックマーク最大 1024 件。 | `%LOCALAPPDATA%\NeNeNib\` の `session.v2`・`history.v1`・`bookmarks.v1`。 |
| クリップボード | コピー・切り取り・貼り付け（Vim の `"+` `"*` レジスタを含む）の操作をしたときだけ読み書きします。 | お使いの端末のクリップボード。 |
| Windows の設定 | Windows のライト／ダークの設定と、Windows の表示言語（設定が「Windows に合わせる」のとき画面の文の言語を決めるため）と、入っている WSL の一覧（フォルダとして選べるようにするため）を読みます。変更はしません。 | 保存しません。 |
| エクスプローラーへの登録 | 操作の一覧（F1）から「Windows の右クリックに登録する」を選んだときだけ、`HKEY_CURRENT_USER` の下にエクスプローラーの右クリックの項目を書きます。「登録を外す」で消します。起動では書きません。これは ZIP 版の動きです。Microsoft Store 版では、Windows がこの書き込みをアプリ専用の保存場所に写すため、エクスプローラーに項目が出ない見込みです。まだ実機で確かめていません。 | お使いのユーザーのレジストリ。 |

アカウント、利用状況の送信、解析、広告、クラッシュ報告、アプリ独自の更新確認、第三者のサービスはありません。

## `%LOCALAPPDATA%\NeNeNib` のファイル

入るのは上の表のものだけです（設定、位置つきのファイルのパス、自動退避では未保存のタブの本文）。削除するまで端末に残ります。
フォルダを消すとアプリは初期の状態に戻ります。

Microsoft Store 版では、Windows がこのフォルダをアプリ専用の保存場所に置くことがあり、その場合はアンインストールすると消えます。
ZIP 版のフォルダがすでにあれば、Windows がそのフォルダをそのまま使わせることがあります。

## ネットワーク

アプリはネットワークに接続しません。通信用のライブラリを使っていません。結んでよいライブラリはビルドの許可表
（`eng/architecture.json`）で決まっていて、通信の機能を持つものはありません。Web のページを開くコードも無く、
Markdown プレビューが表示する画像はファイルとして読めるものだけです。`http`・`https` の画像は取りに行かず、`[画像: alt]` と表示します。
`\\server\share` の形のパスの画像は読みません。

次の 2 つは、使う人が選んだときにネットワークへ届くことがあります。アプリが自分で通信するのではありません。

- **ネットワーク上のファイル。** 共有フォルダ（例: `\\server\share`）のファイルを開く・保存する、ネットワークのプリンタへ印刷する、
  を選ぶと、Windows がその場所へアクセスします。共有フォルダを割り当てたドライブ（例: `Z:\`）の上の画像を Markdown プレビューに
  表示するときも同じで、プレビューは端末の中のファイルと同じように読みます。
- **言語サーバー（既定はオフ）。** 設定で `lsp` をオンにして `lsp.<言語>.command=` の行を書くと、アプリはその命令のプログラムを端末の中で起動し、
  名前のある文書の本文を標準入出力でやり取りします。無題の文書は送りません。そのプログラムが何をするかはそのプログラムしだいです。
  設定に書いていないサーバーをアプリが起動することはありません。

Microsoft Store から入れた場合、ダウンロード・インストール・更新を行うのは Store であり、このアプリではありません。
その部分は [Microsoft のプライバシーに関する声明](https://privacy.microsoft.com/privacystatement)の対象です。

## 変更と連絡先

このポリシーの変更はこのファイルに対して行い、履歴はリポジトリで公開されます。
質問は <https://github.com/hideyukiMORI/nene-nib/issues> へ Issue としてお寄せください。
