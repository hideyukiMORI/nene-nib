# Microsoft Store のスクリーンショットの撮り方 — NeNe Nib 1.0.0

> Issue #1582・施主決定 D88。**撮影はまだしていない。** 撮影は窓を最前面に出して鍵を送るので、
> 設計席が hide の了承を得てから回す（CLAUDE.md §2「窓を最前面に出す確認と計測は、施主に確かめてから回す」）。
> 掲載文は [listing.md](listing.md)、申請の順番は [submission.md](submission.md)。

## Store の要件（Microsoft の文書・2026-11-13 に読んだ）

出典: [App screenshots, images, and trailers for MSIX app](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/screenshots-and-images)。

| 項目 | 要件 |
| --- | --- |
| 必須の枚数 | 1 枚以上（デバイス ファミリのどれか） |
| 推奨の枚数 | 4 枚以上（FAQ では 5〜8 枚） |
| 上限 | Desktop は 10 枚 |
| 大きさ | Desktop は **1366 × 768 以上**。4K（3840 × 2160）まで受ける |
| 形式 | PNG・横でも縦でもよい・1 枚 50 MB 未満 |
| 説明 | 1 枚ごとに 200 文字以内 |
| 言語 | 言語ごとの掲載のページで別々に上げる（同じ画像でも言語ごとに上げ直す） |
| 見せ方 | 大事な絵と文字は上の 3 分の 2 に置く（下の 3 分の 1 に文字が重なることがある）。ロゴや宣伝の文句を足さない |

Store のロゴ（1:1 の 300 × 300）は画像の欄とは別。アイコン一式（D87）の Issue で作る。

## 撮る大きさ（案）

- **窓の client を 1920 × 1080、表示の倍率 125%（120 DPI）で撮る。** 1366 × 768 以上を満たし、Store の大きな表示でも縮みが少ない。
  Nib は枠なしの窓でタブの帯がタイトルバーなので、client がそのまま窓の全体になる。
- 2026-10-12 の紹介の画面（[showcase-2026-10-12.md](../quality/showcase-2026-10-12.md)）は 1600 × 1000・125% だった。
  それでも要件は満たすので、モニタの都合で 1920 × 1080 が取れなければ 1600 × 1000 に下げてよい（5 枚は同じ大きさに揃える）。
- 本文は製品の既定（Cascadia Code・13.5 pt・折り返しオン・操作の案内オン）。
- 画像はリサイズ・合成・描き足しをしない（showcase と同じ）。

## 撮る環境（個人の情報を写さない）

showcase-2026-10-12 と同じ隔離の形にする。

- 起動する子プロセスにだけ `LOCALAPPDATA=D:\NeNeNib\evidence\store-1.0.0\profile` を渡し、設定・一覧・履歴をそこへ閉じる。
  実の設定をコピーしない。
- 文書は [samples/](samples/) を `D:\NeNeNib\evidence\store-1.0.0\documents\` に写して、そこから開く。
  Ctrl+P とエクスプローラーに出るパスは、この撮影用のフォルダだけになる。
- 撮る前に、本文・タブ・Ctrl+P の全行・エクスプローラーの全行・ステータスバーに、個人のパス・実の履歴とブックマーク・
  ほかの案件の名前が無いことを原寸で目で確かめる。
- 道具は既存の `eng/window_driver.py`（起動・鍵・`capture_png`）と `eng/verify-window.py --open <file> --capture <dir>`。
  新しい道具が要るなら 1 回限りのスクリプトを `D:\NeNeNib\scripts\` に置く。
- 表示言語（#1019）が入った後は、掲載の言語ごとに表示を合わせて撮る（5 枚 × 3 言語 = 15 枚・Folio と同じ）。
  間に合わなければ、3 言語とも同じ 5 枚を上げ、説明だけを言語ごとに変える。

## 5 枚の構図

1 枚目がメイン（D88: 既定のダーク = Ubuntu 風の茄子色 `ubuntu-aubergine`）。

| # | 見せるもの | 開く文書 | テーマ | モード | 操作と状態 |
| --- | --- | --- | --- | --- | --- |
| 1 | 本文と Vim のモード（**メイン**） | `TextBuffer.cpp`（アクティブ）・`README.md`・`旅のメモ.md` の 3 タブ | `ubuntu-aubergine`（既定のダーク） | Vim NORMAL | 構文の色が見える位置。ステータスバーに「Vim」と NORMAL。キャレットは関数の中 |
| 2 | Markdown プレビュー | `README.md` | `ubuntu-aubergine` | 通常 | Ctrl+Shift+M で隣の欄に表示。見出し・箇条・チェックボックス・表・コードが上の 3 分の 2 に入る位置 |
| 3 | Ctrl+P の面 | 1 と同じタブ。ブックマーク 2 つ・閉じた履歴 2 つを先に作っておく | `ubuntu-aubergine` | 通常 | Ctrl+P を開いて何も打たない（タブ・ブックマーク・履歴・同じフォルダの 4 種が並ぶ） |
| 4 | エクスプローラーとタブ | 起点を撮影用の `documents\` にして Ctrl+Shift+E。タブ 3〜4 つ | `ubuntu-aubergine` | 通常 | 木を 1 段開いた状態。1 クリックのプレビューのタブ（斜体）は写さない |
| 5 | 別のテーマ | `旅のメモ.md`（日本語の文章・長い段落が折り返す） | 下の候補から 1 つ | 通常 | 折り返しと日本語の表示が見える。ステータスバーに文字コードと改行 |

### 5 枚目のテーマの候補（README の「Find your theme」の 8 つから）

メインがダークなので、5 枚目はライトにして明暗の両方を見せるのを推す。

| 候補 | `:colorscheme` の名前 | 理由 |
| --- | --- | --- |
| **GitHub Light（推し）** | `github-light` | 白地で、使う人が見慣れた配色。メインの茄子色との差がいちばん大きい |
| Rosé Pine Dawn | `rose-pine-dawn` | 温かい明るい地で、日本語の文章が柔らかく見える |
| Code Rain | `code-rain` | ダークのままで、緑の配色で違いが一目で分かる。ただし明暗の両方は見せられない |

## 説明（各 200 文字以内）

| # | 日本語 | English | 简体中文（機械の訳・レビュー歓迎） |
| --- | --- | --- | --- |
| 1 | `普通の操作と Vim の操作をキー 1 つで切り替え。構文の色分けとタブ。` | `Switch between ordinary editing and Vim editing with one key. Syntax colors and tabs.` | `一个按键即可在普通编辑和 Vim 编辑之间切换。支持语法高亮和标签页。` |
| 2 | `編集中の Markdown を隣の欄でプレビュー。` | `Preview the Markdown you are editing in the next pane.` | `在旁边的窗格中预览正在编辑的 Markdown。` |
| 3 | `Ctrl+P でタブ・ブックマーク・最近のファイル・同じフォルダのファイルをまとめて探す。` | `Ctrl+P finds tabs, bookmarks, recent files and files in the current folder in one list.` | `按 Ctrl+P 在一个列表中查找标签页、书签、最近的文件和当前文件夹中的文件。` |
| 4 | `左のファイルエクスプローラーとタイトルバーのタブ。` | `A file explorer on the left and tabs in the title bar.` | `左侧的文件资源管理器和标题栏中的标签页。` |
| 5 | `125 種類のテーマ。日本語の文章も読みやすく折り返す。` | `125 themes. Japanese text wraps cleanly.` | `125 种主题。日文文本也能整齐地自动换行。` |

## 撮影に使う見本の文書（`docs/store/samples/`）

個人の情報を含まない、この Issue のために書いた文書。撮影のときは `D:\NeNeNib\evidence\store-1.0.0\documents\` に写して使う。

| ファイル | 中身 | 使う画像 |
| --- | --- | --- |
| [README.md](samples/README.md) | 見出し・箇条・チェックボックス・表・コードのある Markdown | 1・2・3・4 |
| [TextBuffer.cpp](samples/TextBuffer.cpp) | 短い C++ のクラス（行の索引を持つ文字列） | 1・3・4 |
| [旅のメモ.md](samples/旅のメモ.md) | 日本語の文章（長い段落が折り返す） | 1・4・5 |

Ctrl+P の 3 枚目の「同じフォルダ」には、この 3 つが並ぶ。ブックマークと履歴は撮影の profile の中で、
この 3 つを開いて付け外し・閉じて作る（実の履歴を写さない）。
