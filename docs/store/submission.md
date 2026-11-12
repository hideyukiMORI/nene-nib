# Microsoft Store の申請の手順と申告の答え — NeNe Nib 1.0.0

> Issue #1582・施主決定 D88。Partner Center への入力・自己署名での確かめ・WACK・申請は hide の手（管理者権限が要る）。
> ここは欄ごとの答えと順番の正本。掲載文は [listing.md](listing.md)、画像は [screenshots.md](screenshots.md)、
> プライバシーポリシーは [PRIVACY.md](../../PRIVACY.md)。
> 形は NeNe Loupe（`docs/release/store-listing.md`・2026-10-03 申請・10-04 公開）と NeNe Folio（`docs/release/store-listing/`）に合わせた。

## 身元（Partner Center で予約済み・2026-11-12 夜・hide の手）

| 項目 | 値 |
| --- | --- |
| 製品名 | `NeNe Nib` |
| Store ID | `9NCLGDTFQ529` |
| Package/Identity/Name | `HideyukiMori.NeNeNib` |
| Publisher | `CN=C37230AA-B52D-403B-9BFD-E7980F088422`（Loupe・Folio と同じ） |
| PublisherDisplayName | `Hideyuki Mori` |
| Package Family Name | `HideyukiMori.NeNeNib_3sft4ch5kzywy` |
| 版 | `1.0.0.0`（D88。GitHub の配布は `0.1.0-beta.N` の札のまま） |

予約は申請しないまま 3 か月で外れる（SPECIFICATION の D88）。

## 欄ごとの答え

### 価格と提供

| 欄 | 答え | 根拠 |
| --- | --- | --- |
| 価格 | 無料 | Loupe・Folio と同じ。アプリ内の購入・広告は無い |
| 市場 | すべての市場（既定のまま） | 掲載文は 3 言語。ほかの市場では英語の掲載が出る（Store の既定の振る舞い・未確認） |
| 公開の時期 | 「認定されたらすぐに公開する」 | Loupe と同じ |
| 見つけられ方 | 「Store で見つけられるようにする」 | |

### プロパティ

| 欄 | 答え | 根拠 |
| --- | --- | --- |
| カテゴリ | **開発者ツール（Developer tools）を推す**。次点は仕事効率化（Productivity） | 下の「カテゴリの推し」 |
| サブカテゴリ | 画面に出れば「テキストエディタ」に近いもの。無ければ空欄 | 画面の選択肢は未確認 |
| プライバシーポリシーの URL | `https://github.com/hideyukiMORI/nene-nib/blob/main/PRIVACY.md` | このリポジトリの直下 |
| Web サイト | `https://github.com/hideyukiMORI/nene-nib` | |
| サポートの連絡先 | `https://github.com/hideyukiMORI/nene-nib/issues` | |
| システム要件 | 最小: キーボード・マウス。DirectX の欄は既定のまま | GPU が無くても WARP で描く（SPECIFICATION の動作環境） |
| デバイス ファミリ | 「Windows 10/11 Desktop」だけ | 包んだ Win32 のアプリ |
| 製品の申告 | 「アクセシビリティ」は付けない（検証していない）。「ペンとインク」も付けない | 確かめていない性質を申告しない |

**カテゴリの推し**: 開発者ツール。

- 良い点: 紹介の柱（Vim の操作・構文の色分け・言語サーバー・Markdown）を求める人が探す棚で、Store の主なテキストエディタ
  （コード向けのエディタ）もこの棚に並ぶ。Vim を知っている人に届きやすい。
- 悪い点: 「メモ帳の代わり」を探す人には見つかりにくい。そこは検索語（メモ帳・notepad・记事本）で補う。
- 仕事効率化にすると、Markdown のメモを書く人には近いが、Vim と構文の色分けの価値が伝わりにくい。
- 決めるのは hide。

### 年齢区分（IARC のアンケート）

アプリの種類は「その他のすべてのアプリの種類」（Loupe と同じ）。設問は**すべて「いいえ」**で、期待する結果は 3+（D88）。
答えの根拠になる事実:

| 問われそうなこと | 答え | 事実 | 根拠 |
| --- | --- | --- | --- |
| 暴力・性的な内容・恐怖・賭博・薬物・粗野な言葉 | いいえ | アプリ自身は内容を持たない。使う人が開いた文書を表示するだけ | 実装 |
| 利用者同士のやり取り（チャット・投稿・共有） | いいえ | 無い。アプリは通信しない | PRIVACY.md・`out/1582/privacy-evidence.md` |
| 位置情報の共有 | いいえ | 位置の API を使わない | 実装に該当の呼び出しが無い |
| 個人情報を第三者と共有 | いいえ | 集めない・送らない | PRIVACY.md |
| アプリ内の購入・デジタル商品・広告 | いいえ | 無い | 実装 |
| Web の内容を制限なく表示する（ブラウザ） | いいえ | Web のページを開くコードが無い。Markdown プレビューは `http` / `https` の画像を取りに行かない | `src/core/ImageReference.cpp` |

**答え直す条件**: 更新で通信・利用者同士のやり取り・購入や広告・Web の内容の表示のどれかを足すときは、アンケートを答え直す
（Loupe の記録の IARC の通知の条項）。

### プライバシーの設問

設問「この製品は個人情報へのアクセス、収集、または送信を行いますか」:

- **推しは「はい」と答えて PRIVACY.md の URL を入れる。** Nib は使う人の文書（個人情報を含み得る）を読み書きし、
  ファイルのパスを端末に保存し、設定したときだけ文書の本文を手元の言語サーバーのプロセスへ渡す。「アクセス」に当たると読める。
  Loupe は画面の画素を読むことと規約 10.5.1 を理由に「はい」と答えて認定された。
- 「はい」と答えても、ポリシーの中身は D88 の「集めない・送らない」のまま（PRIVACY.md の冒頭の太字）。
- 「いいえ」と答える案もあるが、その場合も Win32 のアプリにはポリシーの URL を求められる見込み（規約 10.5.1・読み）。決めるのは hide。

### 申請オプション

#### `runFullTrust` が必要な理由（上限 500 文字・この文は 481 文字）

1 行で、途中に改行を入れない。

```text
NeNe Nib is an existing Win32 desktop text editor (C++, no UWP components) packaged with MSIX. runFullTrust is required because the executable is a classic Win32 process (EntryPoint Windows.FullTrustApplication). It opens and saves text files in any folder the user chooses and, only if the user enables it and enters a command in the settings, starts that language server as a child process over standard input/output. No elevation, no network connections, no services or drivers.
```

#### 認定の注意書き（Notes for certification）

入力欄は申請オプションのページの「認定の注意書き」の説明にある「追加のテスト情報」のリンクの先（Loupe の記録）。

```text
No account or sign-in is needed. To test: launch the app, type in the untitled document, press Ctrl+S to save it, and press Ctrl+O to open a file. Ctrl+Shift+Space (or a click on "通常 | Vim" in the status bar) switches between ordinary editing and Vim editing. Open a .md file and press Ctrl+Shift+M to show the Markdown preview. Ctrl+P lists open tabs, recent files and files in the same folder; F1 lists every operation. The language-server feature is off by default and starts nothing unless a command is entered in the settings file. The app makes no network connections.
```

WACK の任意の試験で FAIL や WARNING が出たら、その試験の名前と理由をこの欄の末尾に 1 段落で足す（Folio は「Blocked executables」の FAIL をここで説明して認定された）。

### このバージョンの新機能

初回の申請では空欄（Microsoft の文書）。

## 申請までの順番

設計席・席がすること（リポジトリ）と、hide の手（管理者権限・Partner Center）を分ける。

| 順 | 誰 | すること | 出力・確かめ |
| --- | --- | --- | --- |
| 1 | 席 | 表示言語（#1019）を main に入れる | 3 言語が切り替わる。入らなければ listing.md の「表示は…」の文を事実に直す |
| 2 | 席 | アプリアイコン一式（D87・`#461136`）: `.ico` と MSIX の PNG と Store の 300×300 | 別の Issue |
| 3 | 席 | MSIX を作る道具（#1581）: identity・版 `1.0.0.0`・runFullTrust・言語 ja / en / zh-Hans・makepri・makeappx（署名なし）・unpack して exe が zip と同じバイト列かの照合 | `.msix`・中身の一覧・exe の SHA-256 |
| 4 | hide | 自己署名の証明書で MSIX に署名して入れる（証明書を `LocalMachine\TrustedPeople` に入れるのに管理者権限が要る。`.pfx` はリポジトリに置かない） | 起動する |
| 5 | hide | packaged の実機の確かめ（下の表） | 結果を設計席へ。Store 版の登録の扱いは別の ADR |
| 6 | 設計席 | スクリーンショット 5 枚 × 言語（[screenshots.md](screenshots.md)・窓を前に出すので hide の了承の後） | PNG |
| 7 | hide | WACK（Windows App Certification Kit）を管理者で 1 回 | FAIL 0。WARNING と任意の試験の FAIL は認定の注意書きで説明 |
| 8 | hide | Partner Center に署名なしの `.msix` を上げる（Store が署名する）。この文書と listing.md の欄を埋めて申請 | 「Validated」・runFullTrust の承認の警告は想定どおり |
| 9 | 設計席 | 認定の後に結果を `docs/quality/` に記録し、この文書の「確かめていないこと」を更新 | |

### packaged の実機の確かめ（順 5）

| 確かめること | 期待 | なぜ |
| --- | --- | --- |
| 設定・session・history・bookmarks・自動退避の置き場 | ZIP 版の `%LOCALAPPDATA%\NeNeNib` が既にあればそこ、無ければ `Packages\HideyukiMori.NeNeNib_3sft4ch5kzywy\LocalCache\Local\NeNeNib\` | Loupe の実測と同じになるはず（読み）。PRIVACY.md の Store 版の段落の根拠 |
| F1「Windows の右クリックに登録する」 | packaged では HKCU の書き込みがパッケージ専用の場所に写され、エクスプローラーから見えない見込み（読み） | 見えなければ Store 版では操作を隠すか無効にする（別の ADR・D73 の段 2） |
| 言語サーバー（`lsp=on` と clangd の命令） | 起動して診断が出る | full trust の子プロセス（読み・未実測） |
| `\\wsl.localhost\<distro>` を起点にする | 開ける | full trust はファイルの API を普通に使える（読み） |
| 保存（任意のフォルダ・一時ファイルの置き換え） | 保存できる | ADR 0135 |
| 印刷（F1） | 印刷の窓が出る | XPS の印刷 |

## MSIX の道具（#1581）の出力の置き場（案）

- リポジトリの外の `D:\NeNeNib\releases\1.0.0\` に置く（Folio の `D:\NeNeFolio\releases\1.0.0\` と同じ形）。
  - `NeNeNib-v1.0.0-windows-x64-store.msix`・中身の一覧・exe の SHA-256・道具の記録
  - `wack\`（WACK の報告）
  - `store-listing\{ja-JP,en-US,zh-CN}\`（listing.md から作る貼り付け用のテキスト。作るなら道具で生成し、手で直さない）
  - `screenshots\{ja-JP,en-US,zh-CN}\`
- `.msix` と `.pfx` はリポジトリに入れない。リポジトリには記録（`docs/quality/` の日付つきの文書）だけを置く。
- 置き場は #1581 の席が決めた形があればそちらに従い、この節を直す。

## 確かめていないこと

- Partner Center の「カテゴリ」と「サブカテゴリ」の今の選択肢（Microsoft の文書に一覧が無い。画面で確かめる）。
- 市場を「すべて」にしたとき、掲載の無い言語の市場で英語の掲載が出るか。
- 日本語・中国語の検索語で「全体で 21 単語まで」がどう数えられるか（Loupe でも未確認）。
- MSIX の manifest の `MinVersion`。動作環境は Windows 11 22H2 以降（SPECIFICATION の動作環境）なので `10.0.22621.0` が合うが、
  Loupe は `10.0.19041.0` だった。#1581 で決める。
- packaged の実機の表（順 5）はすべて未実測。
