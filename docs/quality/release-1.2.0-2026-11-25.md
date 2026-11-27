# 1.2.0 の公開（2026-11-25・公開の記録の前半）

公開の記録の前半（席 REL2・#2050・節立ては [release-1.1.0-2026-11-17.md](release-1.1.0-2026-11-17.md) と同じ）。骨組みは席 B1（#2034・PR #2038）が置いた。この PR で埋めたのは「決め」の中身の文・「範囲」と表・「中身の一覧」・「この版が書く設定の版と鍵の集合」・「公開元」の分かる所・「根拠」・「既知の限界」・「残るリスク」で、**「梱包」「GitHub の公開」「Microsoft Store の提出」「公開の結果」は設計席が本番の梱包と公開の後に埋める**。**この記録を置いた時点では、1.2.0 はまだ梱包も公開も申請もしていない**（公開版は 1.1.0・公開元 `4fcf258c`）。
まとめ枝 52 の PR と main の commit・全件・網羅率・速さのゲート・CI の欄は、設計席がまとめ枝 52 の merge の後（2026-11-25）に現物の値で埋めた（道具 `D:/NeNeNib/scripts/20261125/fill-record-2050.py`）。
この文書 PR（#2050）の merge の commit が 1.2.0 の公開元になる（1.0.0 / 1.1.0 と同じ形）。1.2.0 は Microsoft Store の 3 回目の申請（2 回目の「更新」）。手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md) の「1.2.0 の更新の申請」と [msix.md](../store/msix.md)、リポジトリの外の手順と道具は `D:/NeNeNib/evidence/v1.2.0-release/`（手順書 `README.md`・決めの一覧 `open-questions.md`）。

## 決め（2026-11-24・施主 hide）

- **次の版は 1.2.0**（MSIX と Store の版は `1.2.0.0`・SPECIFICATION の D115）。版の入力は `CMakeLists.txt` の `project(... VERSION)` の 1 か所で、PR #2038（#2034・まとめ枝 52 の中の 1 commit）で `1.1.0` → `1.2.0` に上げた。
- **1.2.0 は `settings.v6` を書く最初の公開版で、v6 の鍵の集合はこの版で閉じる**（D91 の続き・D115・下の「この版が書く設定の版と鍵の集合」）。
- **クイックフィックス（#2005・Ctrl+. / `gra`・Ctrl+P の `=`）は 1.2.0 に入れない**（次の版・D116）。PR #2022 は 1.2.0 の公開元を切るまで main に入れない。
- 1.2.0 の中身: main `c8ccdc8b` まで（まとめ枝 48〜51・言語ごとのサーバー #1909・「面」の置き換え #1928・名前の変更 #2004 を含む）＋ まとめ枝 52（Issue #2037）。まとめ枝 52 は、設定の鍵の名前と v6 の見本（#2023・PR #2029）・Vim の INSERT で ↑↓ が補完の候補を選ぶ（#2024・PR #2032・D111）・診断の文が出ていてもモードの札を見せる（#2025・PR #2033・D119）・網羅率の試験（#2013・PR #2030）・版の番号（#2034・PR #2038）・名前の変更の答えを待つ間にタブを閉じたら断る（#2039・PR #2041）と噛み合わせの 3 commit。まとめ枝 52 とこの文書 PR が main に入ったら、公開まで main に機能を入れない。
- hide の 2026-11-24 の決め（`open-questions.md` の H-1〜H-11・どれも「おすすめ」のとおり・仕様の D117（H-1〜H-3）・D118（H-4〜H-6）・D110（H-8）・D120（H-9〜H-11））:

  | # | 決め |
  | --- | --- |
  | H-1 | 名前の変更で開いていなかったファイルは、保存せずに ● のタブで残す（自動で保存しない） |
  | H-2 | 名前の変更が一度に変えるファイルの上限は 32 |
  | H-3 | 名前の変更の鍵は F2 と Vim の `grn`、操作の名前は「名前を変更」（英語 `Rename symbol`・中国語「重命名」） |
  | H-4 | 設定の選ぶ欄の Enter は「一覧を開く」（次の値へ送るのは → のまま） |
  | H-5 | 選ぶ欄の一覧を動かしただけでは配色を本文で見せない（1.2.0 は見せないまま・#1731 で決める） |
  | H-6 | Ctrl+P の一覧と右クリックのメニューの選んだ行の枠は 1.2.0 では変えない |
  | H-7 | 設定の一覧を開いたまま窓から焦点が外れたときは、試用で気になれば閉じる形に直す（気にならなければこのまま） |
  | H-8 | 自動で開いた補完の一覧から確定した語は、打った本文としてマクロに入る（このまま） |
  | H-9 | Store のスクリーンショットは差し替えない（1.1.0 の 5 枚のまま） |
  | H-10 | GitHub の tag と Release は Store の認定の後に出す（1.0.0 / 1.1.0 と同じ順） |
  | H-11 | GitHub の Release の本文は日本語と英語の両方 |

**範囲**: 1.1.0 の公開元 `4fcf258c065216bd2a6106e90593fdbd70fbcd2a` から、公開元 この文書 PR（#2050）の merge の commit（この記録を載せた main の commit そのものなので、SHA はここに書けない。梱包の節と `docs/todo/current.md` に書く） までの main。この PR の起点（まとめ枝 52 の枝の先端 `5ad83876`）で数えると、`git log --oneline 4fcf258c..5ad83876` は 55 本（main `c8ccdc8b` までの 46 本 ＋ まとめ枝 52 の 9 本）、`git diff --shortstat 4fcf258c 5ad83876 -- src` は 356 ファイル（+21,057 / −1,470）。公開元での本数は まとめ枝 52 の後の main `68c5059f` で 55 本（`git log --oneline 4fcf258c..68c5059f`）＋ この文書 PR の 1 本 = 56 本（この PR は文書だけなので `src` の数は変わらない）。製品の変更は次のまとめ枝と単独の PR で入った（日付は JST・まとめ枝は rebase merge で 1 PR = 1 commit・「main」は main に入った commit）。

| まとめ枝 / 単独 | PR（merge） | 製品の変更（Issue / 元の PR / main） |
| --- | --- | --- |
| 単独 | PR #1916（2026-11-22 14:05 squash merge・main `5d76cc71`） | 言語サーバーを言語ごとに持ち、タブの切り替えで止めない（#1909・LSP 段 S2・席の上限 4・ADR 0160 / 0161 の 2026-11-20 の追補） |
| 48（#1948） | PR #1957（2026-11-23 01:32 rebase merge・main `6fb3686e`・9 commit） | 言語の表に 16 言語（色なし・#1917 / PR #1943 / `2c8008bb`）・言語サーバーの root を文書から上へ印で探す（#1940 / PR #1944 / `7fdae329`・足した 16 言語の印 `4fa77149`）・診断の間を跳ぶ鍵と Ctrl+P の `^`（#1939 / PR #1946 / `2dd660aa`）・split の境界線をドラッグと鍵で動かす（#1941 / PR #1949 / `e3e70ff8`）・設定の言語サーバーの節に PC で見つかった候補（#1945 / PR #1951 / `1786e4c9`）・言語サーバーの記録 `:LspLog` と進み具合（#1942 / PR #1950 / `be15c990`）・噛み合わせ 2（`57bc9976` `6fb3686e`） |
| 49（#1966） | PR #1967（2026-11-23 09:33 rebase merge・main `7f266cfb`・5 commit） | `initialize` で `publishDiagnostics` と `synchronization` を名乗る（#1961 / PR #1964 / `a3f0974a`）・キャレットの動きと `settings.v6`（#1933 / PR #1956 / `a1f4a4b9`・ADR 0192・D108）・参照の一覧（#1947 / PR #1954 / `f1949e12`・ADR 0191）・root の印を設定の root まで探す（#1960 / PR #1963 / `a85044f6`）・Vim の INSERT でも trigger の文字で補完の一覧（#1962 / PR #1965 / `7f266cfb`・D109） |
| 50（#1998） | PR #2001（2026-11-23 23:35 rebase merge・main `9e8adbf5`・10 commit。同じ木の PR #2000 は噛み合わせの commit の文に Issue を閉じる行が無く、積み直した枝で出し直した） | 記号の一覧（#1955 / PR #1958 / `5393eae4`）・引数の情報（#1959 / PR #1968 / `a7bf7f84`）・アクセントの上の字の色（#1953 / PR #1980 / `fbeca7ae`）・文字の大きさの 0.5 pt（#1927 / PR #1985 / `7a4aad49`・D121）・横のプレビューから全面へ入って戻っても枠の比を保つ（#1973 / PR #1990 / `930da278`）・打ったときの粒（#1979 / PR #1988 / `cdbb88eb`・D112）・インデントガイド（#1975 / PR #1992 / `5efdf6f7`・ADR 0193・D114）・split の 1 回分の設定（#1972 / PR #1994 / `69891532`・D113）・噛み合わせと試験（`126828e7` `9e8adbf5`） |
| 51（#2010） | PR #2021（2026-11-24 17:07 rebase merge・main `7e2e34e0`・6 commit。PR #2015 は commit の文の形で、PR #2020 は main より 1 commit 遅れているとして merge の道具が断ったので、main `37626142` の上に同じ 6 commit を積んだ枝で出し直した） | 文書と選択範囲の整形（#1993 / PR #1997 / `9a41937e`）・同じ命令の行の言語は 1 本のサーバーを共有（#1952 / PR #2007 / `76333e2b`）・自動で開いた補完の一覧の確定（#1971 / PR #2008 / `5b0534ff`・D110）・焦点の行の 1 DIP の枠（#1929 / PR #2006 / `f333d1f1`）・設定の選ぶ欄を一覧から（#1926 / PR #2009 / `87512ead`・D118）・窓のタイマーの番号の試験（`7e2e34e0`） |
| 単独 | PR #2014（2026-11-24 18:10 squash merge・main `eca2bc1f`） | 日本語の画面の文の「面」を置き換える（#1928・日本語の 8 文の字面だけ・D122） |
| 単独 | PR #2011（2026-11-24 21:08 squash merge・main `a6b20294`） | 言語サーバーの名前の変更（#2004・LSP 段 S7 a・F2 / `grn`・ADR 0194・D117） |
| 52（#2037） | PR #2052（2026-11-25 15:28 rebase merge・main `68c5059f`・9 commit。main の SHA は #2029 `145b4bc1`・#2032 `79d03866`・#2033 `cf5499a4`・#2030 `55b29509`・噛み合わせ `b4a79eda`・#2038 `18ff0eae`・噛み合わせ `af8213d8`・#2041 `2e8ad529`・文書 `68c5059f`）（枝 `chore/2037-integration-20261125b`・起点 main `c8ccdc8b`・先端 `5ad83876`・9 commit。下の SHA は枝の上の物で、rebase merge の後の main の SHA ではない） | `settings.v6` を閉じる前にインデントガイドの鍵の名前を 2 つ変え、1.2.0 の v6 の見本を試験に置く（#2023 / PR #2029 / 枝 `70467fb5`）・Vim の INSERT で補完の一覧が出ている間 ↑↓ と PageUp / PageDown で候補を選ぶ（#2024 / PR #2032 / 枝 `fbccb572`・D111）・診断の文が出ていてもトグルとモードの札を描く（#2025 / PR #2033 / 枝 `50373288`・D119）・分岐の網羅率を契約の試験で 90% 以上に戻す（#2013 / PR #2030（PR #2016 を含む）/ 枝 `84998c84`・試験だけ）・版を 1.2.0 に上げる（#2034 / PR #2038 / 枝 `138304bb`）・名前の変更の答えを待つ間に写しのタブを閉じたら、タブの無い当て先を含む答えを断る（#2039 / PR #2041 / 枝 `52cce546`）・噛み合わせ 3（枝 `5c6db9fd` 試験の名前・`66be2a85` conformance の試験と README・`5ad83876` README と PRIVACY.md の文） |

製品の外: 道具と試験の直し（probe の古い期待値 #1920 / PR #1923 / main `94a0b301`・`eng/coverage.py` #1921 / PR #2012 / main `37626142`）・ADR（0193 `64d612e9`・0194 `84c490ba`）・1.1.0 の公開の記録（`8156a9a9` `1ce46b7a`）・README（`9e8f7a4c` `5a6af249`）・画像（`10449ad3`）・日報と引き継ぎ（`9bdd4220` `8e125c86` `fc4187c4` `c8ccdc8b`）。

**中身の一覧**（使う人から見える物。LSP の機能は言語サーバーがオフ（既定・D60）なら見えない）:

| 群 | 変更 | Issue |
| --- | --- | --- |
| 言語サーバー | 言語ごとにサーバーを持ち、タブを切り替えても止めない（席の上限 4） | #1909 |
| 言語サーバー | 16 言語（Java・Kotlin・Dart・Zig・Terraform・Makefile・Batch・Ruby・Swift・Scala・Haskell・Elixir・R・Perl・Vue・Svelte）を言語の表に足す（構文の色はまだ付かない） | #1917 |
| 言語サーバー | プロジェクトの root を文書のフォルダから上へ印で探す（設定の root より上へ登らない） | #1940 #1960 |
| 言語サーバー | 診断の間を跳ぶ（F8 / Shift+F8・`]d` `[d`）・`gl` と「診断を表示」・Ctrl+P の `^` の診断の一覧 | #1939 |
| 言語サーバー | 設定の「言語サーバー」の節に PC で見つかったサーバーを出し、「使う」で命令の行を書く（D123） | #1945 |
| 言語サーバー | 言語サーバーの記録（`:LspLog`・F1）とステータスバーの進み具合 | #1942 |
| 言語サーバー | `initialize` で `publishDiagnostics` と `synchronization` を名乗る | #1961 |
| 言語サーバー | 参照の一覧（`grr`・Shift+F12・Ctrl+P の `&`） | #1947 |
| 言語サーバー | Vim の INSERT でも trigger の文字で補完の一覧が自動で開く（D109） | #1962 |
| 言語サーバー | 記号の一覧（`gO`・F1・Ctrl+P の `%`） | #1955 |
| 言語サーバー | 引数の情報（trigger の文字・F1 の「引数の情報を表示」） | #1959 |
| 言語サーバー | 整形（F1 の「文書を整形」「選択範囲を整形」・1 回の undo で戻る） | #1993 |
| 言語サーバー | 同じ命令の行の言語（C と C++ など）は 1 本のサーバーを共有する | #1952 |
| 言語サーバー | 自動で開いた補完の一覧の確定（クリック・Tab・選んだときだけ Enter・D110） | #1971 |
| 言語サーバー | Vim の INSERT で補完の一覧が出ている間、↑↓ と PageUp / PageDown が候補を選ぶ（D111・まとめ枝 52） | #2024 |
| 言語サーバー | 名前の変更（F2・`grn`・F1 の「名前を変更」・上限 32 ファイル・開いていなかったファイルは ● のタブ・D117） | #2004 |
| 言語サーバー | 名前の変更の答えを待つ間にタブを閉じたら、タブの無いファイルを含む答えを古い答えとして断る（まとめ枝 52） | #2039 |
| 言語サーバー | 下の行に診断の文が出ていても「通常 \| Vim」のトグルとモードの札を見せ、文は札の後ろに書く（D119・まとめ枝 52） | #2025 |
| split | 境界線をドラッグで動かす・Vim の `Ctrl-w > < + - = _ \|`・`:resize`・F1 の 3 つ（D113） | #1941 |
| split | 「枠を広げる / 狭める」の 1 回分（`pane_resize_step`・既定 5）と Vim の回数なしの扱い（`pane_resize_step_vim`） | #1972 |
| split | 横のプレビューから全面へ入って戻っても枠の比を保つ | #1973 |
| キャレット | キャレットの動き（既定オフ・D108） | #1933 |
| キャレット | 打ったときの粒（既定オフ・D112） | #1979 |
| インデントガイド | 字下げの段ごとの縦線（既定オフ・段 1・D114）。設定の鍵は `indent_guide_color_count` と `indent_guide_highlight_current`（まとめ枝 52 で名前を変えた・`:set indentguidecolorcount=` `indentguidehighlightcurrent`） | #1975 #2023 |
| 設定の画面 | 選ぶ欄を一覧から選ぶ形に（D118） | #1926 |
| 設定の画面 | 焦点の行を `text` の色の 1 DIP の枠で囲む（補完の一覧の選んだ行も） | #1929 |
| 設定の画面 | 文字の大きさの刻みを 0.5 pt に（D121） | #1927 |
| 画面の文 | 日本語の画面の文の「面」を「パネル」「Ctrl+P の一覧」「フォルダの一覧」「エディター全体」「設定」に置き換える（D122） | #1928 |
| ほか | アクセントの色の上の字を、黒と白のうちコントラスト比が大きい方で選ぶ | #1953 |

使う人から見えない物: 網羅率の試験（#2013・まとめ枝 52・製品のコードと `eng/coverage-policy.json` は変えていない・PR #2030 の本文）・`eng/coverage.py` の直し（#1921）・probe の古い期待値（#1920）・積んだ所の表の数え直しと試験。

入らない物: クイックフィックス（#2005・D116）・不可視文字（ADR 0193 の段 2・#1977・`settings.v7`）・括弧の色分けとペアガイド（段 3・#1976）。

**この版が書く設定の版と鍵の集合**（ADR 0170 の 2026-11-16 の追補の 4）: 1.2.0 は `settings.v6` を書く最初の公開版で、v6 の鍵の集合はここで閉じる。鍵は、`encode_settings`（`src/adapters/win32/SettingsCodec.cpp`）が書く順に `version` `colorscheme` `font_family` `font_size` `guide` `wrap` `ui_language` `lsp` `explorer` `root` `features` `ignorecase` `smartcase` `relativenumber` `scrolloff` `explorer_width` `autoindent` `finder_preview`（ここまでが v5 の鍵・1.1.0 の記録の同じ段）と、v6 で足した `caret_motion`（#1933・既定 off）`caret_motion_time`（既定 120）`caret_sparks`（#1979・既定 off）`pane_resize_step`（#1972・既定 5）`pane_resize_step_vim`（既定 off）`indent_guide_style`（#1975・既定 `rainbow`）`indent_guide_color_count`（既定 4）`indent_guide_highlight_current`（既定 on）`indent_guide_width`（既定 4）の 9 つ（ここまで 27 行）。その後ろに `lsp.<language>.command` の行（言語の名前の順）、既定と違うときだけの `mapleader`、`map.<mode>.<lhs>` の行。読む側の鍵の名前の表は同じファイルの `field_names`（`mapleader` を入れて 28 欄）と接頭辞 `lsp.` `map.`。インデントガイドの 2 つの鍵は公開の前に名前を変えた（`indent_guide_colors` → `indent_guide_color_count`・`indent_guide_active` → `indent_guide_highlight_current`・#2023・PR #2029）。古い 2 つの名前を持つ v6 のファイルは、未知の鍵を持つ壊れた新版として読まず、v5 へ戻らず、保存を断る（試験 `verify_old_v6_names_file`。公開した版はこの名前を書いていないので、当たるのは main の途中の版で設定を保存した人だけ）。
1.2.0 が書く形の見本は `released_v6_settings`（`tests/adapters/SettingsReleasedSixTests.hpp`・v6 の 9 つの鍵が全部既定でない値・`features=minimap,indentguides`・表に無い言語の `lsp.later-lang.command` の行・`mapleader`・`map.i.jj` の行）で、試験 `verify_released_v6_codec` が、読むと値が戻る・書き戻すとバイト列が一致する・未知の鍵と古い 2 つの名前は拒む・v5 と v6 を互いに読まない・欠けた行は既定を補う、を縛る。ADR 0170 / 0192 / 0193 / 0148 の「v6 は 1.2.0 で閉じる」の文と ADR 0193 決定 4 の「段 2 の `list_*` は `settings.v7`」は PR #2029 で書いた。1.2.0 の後は `settings.v6` に鍵を足さない（足すなら `settings.v7`・CLAUDE.md §2）。**閉じた後も版を上げずに足せる族**は 3 つ: `features=` の名前（codec は名前の形だけを見て意味を解かず、知らない名前を往復で残す・ADR 0193 決定 3）・`lsp.<name>.command` の行（形で厳密に読み、言語の表に無い名前の行は使わずに持って書き戻す・ADR 0170 の 2026-11-20 の追補）・`map.<mode>.<lhs>` の行。版が要るのは新しい「値の鍵」だけ。
読む順は `Win32SettingsAdapter.cpp` の `reading_order`（v6 → v5 → v4 → v3 → v2 → v1）。`settings.v5`（1.1.0 の書く形）と `settings.v4`（1.0.0）は `settings.v6` が無いときだけ読み、bytes のまま残す。`session.v2` `history.v1` `bookmarks.v1` `autosave.v1` の codec は 1.1.0 から変わっていない（`git diff 4fcf258c 5ad83876 -- src/adapters/win32/{Session,History,Bookmark,Autosave}Codec.*` は空）。公開元での照合（見本と codec が起点から変わっていないこと）は main `68c5059f` は起点 `5ad83876` と同じ木で、この文書 PR は `docs/` だけを変える（`git diff 5ad83876 <公開元> -- src tests` が空であることを梱包の照合で確かめる）。

Vim の fixture は `tests/vim/fixtures.json` が 6,892 件、折り返しの窓の `tests/vim/wrap-fixtures.json` が 402 件で、どちらも 1.1.0 の公開元と同じ数（`5ad83876` で数えた）。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main） | この文書 PR（#2050）の merge の commit（この記録を載せた main の commit そのものなので、SHA はここに書けない。梱包の節と `docs/todo/current.md` に書く）（まとめ枝 52 の上に、この文書 PR（#2050）が入った main の commit。未実施） |
| 前の公開 | `v1.1.0`（`4fcf258c065216bd2a6106e90593fdbd70fbcd2a`・GitHub 2026-11-23 12:43 JST・Store で公開済み・[記録](release-1.1.0-2026-11-17.md)） |
| 版（`project()`） | `1.2.0`（PR #2038・#2034・まとめ枝 52 の枝 `138304bb`） |
| 梱包の作業木 | 未実施 |

## 根拠（再利用する検証）

製品の変更は、まとめ枝と単独の PR ごとの (a)〜(c) と、各 PR の独立レビューで受けた。版の 1 行（`CMakeLists.txt`・PR #2038）はまとめ枝 52 の中に在り、52 の全件・ゲート・CI はその版の木で回る。この PR（#2050）は文書と掲載文だけで、製品と梱包の道具は変えていない。

- (a) 速さのゲート（QLT-014・`eng/measure-speed.py --check --repetitions 5`・どれも測る前にビルドの process が 0 と負荷の読みを確かめた 1 回）。48〜51 と単独の 2 本は **8 benches checked・0 regression・0 unmeasurable**:

  | 木 | 記録 | 起動 / 窓 / 単打（中央値 ms・括弧は 16 MiB を開く） |
  | --- | --- | --- |
  | PR #1916 `8f9255f0` | `D:/NeNeNib/outputs/20261122/gate-1916.log` と `-load.txt`（2026-11-22 13:06〜13:08・負荷 10 / 6 / 10%） | 49.311 / 35.386 / 0.654（55.952） |
  | 48 `5f70fd8f` | `D:/NeNeNib/outputs/20261122/gate-int48.log` と `-load.txt`（2026-11-23 00:54・負荷 2〜4% の読み 3 回の後） | 43.583 / 32.517 / 0.575（50.668） |
  | 49 `07c8cc31` | `D:/NeNeNib/outputs/20261122/gate-int49.log` と `-load.txt`（2026-11-23 08:38・負荷 1〜6%） | 43.097 / 31.766 / 0.653（48.980） |
  | 50 `4a9511a8`（Release の木。merge した `e8495030` との差は試験のファイルだけ・PR #2001 の本文） | `D:/NeNeNib/outputs/20261123/gate-int50.log`（負荷の読みは `20261122/gate-int50-load.txt`）（2026-11-23 22:38〜22:40・負荷 12% 未満の読み 3 回の後） | 45.417 / 32.971 / 0.590（53.430） |
  | 51 `493ba283`（Release の製品のコード。merge した `10178e8f` との差は試験 3 行と main の #2012 の 3 ファイル・PR #2021 の本文） | `D:/NeNeNib/outputs/20261124/gate-int51.log` と `-load.txt`（2026-11-24 16:15・負荷 7〜11%） | 46.677 / 35.320 / 0.519（54.020） |
  | PR #2011 `38c8ec86` | `D:/NeNeNib/outputs/20261124/gate-s7a-valid.log`（2026-11-24 20:19・測定の間 5 秒ごとに数えたビルドの process は 20 回とも 0）。**19:22 の 1 回目は無効**（開始 21 秒後に別のプロダクトの席のビルドが始まった・`gate-s7a-invalid-1922.log`・結果を見ずに無効と決まる回・採らない） | 44.426 / 32.058 / 0.526（57.947） |
  | 52 `5ad83876`（枝の先端・main `68c5059f` と同じ木） | 8 本・悪化 0・測れない 0（2026-11-25 13:52〜13:54・測定の間のビルドの process 0 / 20・D の Disk Time の中央値 0%・記録 `D:/NeNeNib/outputs/20261125/gate-int52-valid.log`。その前の 3 回は測定の間にほかのプロダクトのビルドの process が居たので、結果を見ずに無効にした・`gate-int52-a1.log`〜`a3.log`）（道具 `D:/NeNeNib/scripts/20261125/int52/gate3.sh`・無効の条件は測る前に決めた・走り書き 11-25 11:14） | 42.025 / 30.350 / 0.582（52.375） |

  PR #2014（#1928・日本語の 8 文の字面と probe の期待）は単独ではゲートを測っていない（PR の本文「全件・Release は設計席のまとめ枝で回す」）。その差はまとめ枝 52 の木に含まれるので、52 のゲートで受ける。PR #1923 と #2012 は試験と道具だけで、製品の exe に入らない。
  公開元の木と最後にゲートを測った木の製品の入力の差: ゲートの木 `5ad83876` と main `68c5059f` は同じ木（`git diff 5ad83876 68c5059f` が空）で、この文書 PR は `docs/` だけを変える（`git diff 68c5059f <公開元> -- src tests eng third_party CMakeLists.txt` が空であることを梱包の照合で確かめる）。梱包の exe では測らない（1.0.0 / 1.1.0 と同じ形）。
- (b) 独立レビュー（読むだけの席・所在の文書が正本・判定の数はその文書の頭の行）:

  | PR | レビュー | 判定（その後） |
  | --- | --- | --- |
  | #1916（#1909） | `D:/NeNeNib/outputs/20261120/review-1916.md`・`D:/NeNeNib/outputs/20261121/review-1916-recheck.md` | 差し戻しの直しと再確認 R1' の検査を足してから入れた（PR #1916 の本文） |
  | #1943 #1944 #1946 #1949 #1950 #1951（まとめ枝 48） | `D:/NeNeNib/outputs/20261122/review-1943.md` `-1944.md` `-1946.md` `-1949.md` `-1950.md` `-1951.md` | #1944 必須 0・#1949 #1950 #1951 必須 1 ずつ。どれも必須を直してから積んだ（PR #1957 の本文「独立レビューで必須を直した PR 6 本」） |
  | #1954 #1956 #1963 #1964 #1965（まとめ枝 49） | `…/20261122/review-1954.md` `-1956.md` `-1963.md` `-1965.md` | #1954 #1956 #1963 必須 1 ずつ → 直して受理・#1965 必須 0。#1964（#1961・3 ファイル 26 行・名乗る能力の文字列）は独立レビューに出さず、設計席が差分を読んで受理した（走り書き `D:/NeNeNib/outputs/20261122/notes.md`） |
  | #1958 #1968 #1980 #1985 #1988 #1990 #1992 #1994（まとめ枝 50） | `…/20261122/review-1958.md` `-1958-fixes.md`・`…/20261123/review-1958-fixes2-and-1968.md` `-1968-fixes.md` `-1985-1990.md` `-1988.md` `-1992.md` `-1994.md` | #1958 は直しの確かめで必須 1 → 2 回目で必須 0・#1968 必須 1 → 直しの確かめで必須 0・#1985 #1990 #1988 必須 0・#1992 コードの必須 0（merge の前のゲート 8 本が要る、の 1 つ → まとめ枝 50 のゲートで満たした）。#1980（#1953）は独立レビューの文書が無く、走り書き（`20261122/notes.md` の 11-23 11:00）は「受理」と hide の決め（既定の 2 テーマの橙の上の字は白のまま）を残す。#1994 の判定は `review-1994.md` |
  | #1997 #2006 #2007 #2008 #2009（まとめ枝 51） | `…/20261123/review-1997.md` `-1997-fixes.md`・`…/20261124/review-1997-fixes2.md` `-2006-2009.md` `-2007.md` `-2008.md` | #1997 は 3 回とも必須 0（推奨を直した）・#2007 必須 1 → 直して受理・#2008 と #2006 #2009 は裁定の後に直して受理（走り書き 11-24） |
  | #2011（#2004） | `…/20261124/review-2011.md`・`review-2011-fixes.md` | 独立レビュー 3 回（必須 M1 → 直しの確かめで必須 2（N1・N2）→ 直した・3 回目の直しは設計席が差分を読んで受理・PR #2011 の本文と走り書き 11-24） |
  | #2014（#1928） | 独立レビューなし | 設計席が日本語の 8 文の差分を読んで受理（走り書き 11-24） |
  | #2029 #2032 #2033 #2038（まとめ枝 52） | 独立レビューなし | #2029 は設計席が CLAUDE.md の差分を読んで受理・#2032 は設計席が `src` の差分を読み、hide が試用の exe で確かめた（(d)）・#2033 は hide が試用の exe で確かめた（(d)）・#2038 は設計席が差分を全部読んだ（製品の差は版の 1 行・走り書き 11-25） |
  | #2030（#2013・#2016 を含む） | `…/20261124/review-2016.md`・`review-2030.md`（試験の質） | #2016 は「仕様と違う今の振る舞いを固定した行」の指摘を直して受理（不具合の候補は Issue #2017 ほかに起こした）・#2030 は必須 0（推奨 3 を直した） |
  | #2041（#2039） | `D:/NeNeNib/outputs/20261125/review-2022-fixes.md`（穴を見つけた N2）・`review-2041.md`（直しの確かめ） | 必須 0・推奨 2。残る穴 P1（タブを閉じずに「名前を付けて保存」）は限界として受け、直しは Issue #2042（下の「既知の限界」） |

- (c) CI（merge の道具の run・どれも `verifiedCI` が success）: PR #1916 [44983752146](https://github.com/hideyukiMORI/nene-nib/actions/runs/44983752146)・まとめ枝 48 [45041083066](https://github.com/hideyukiMORI/nene-nib/actions/runs/45041083066)・49 [45084567090](https://github.com/hideyukiMORI/nene-nib/actions/runs/45084567090)・50 [45186612069](https://github.com/hideyukiMORI/nene-nib/actions/runs/45186612069)・PR #2012 [45323857725](https://github.com/hideyukiMORI/nene-nib/actions/runs/45323857725)・まとめ枝 51 [45347183822](https://github.com/hideyukiMORI/nene-nib/actions/runs/45347183822)・PR #2014 [45353538643](https://github.com/hideyukiMORI/nene-nib/actions/runs/45353538643)・PR #2011 [45383406151](https://github.com/hideyukiMORI/nene-nib/actions/runs/45383406151)（merge の道具のログ `D:/NeNeNib/outputs/20261122/merge-1916.log` `merge-1957.log`・`20261123/merge-1967.log` `merge-2001.log`・`20261124/merge-2012.log` `merge-2021.log` `merge-2014.log` `merge-2011.log`）。まとめ枝 52: [45553560292](https://github.com/hideyukiMORI/nene-nib/actions/runs/45553560292)（check と build-and-test が success・merge の道具のログ `D:/NeNeNib/outputs/20261125/merge-2052.log`）。
  手元の全件の検証（`eng/check.ps1 -Full` の段・速さとカバレッジは別）: 48（木 `5f70fd8f`・ctest 26 本のうち 1 回目は `nib_probe_expectations` だけが落ち、衝突を解いたときに置いた checksum 3 行を期待の表から計算し直して成功・probe-smoke 286 / 286・prove-gates 33）・49（木 `07c8cc31`・ctest 26 / 26・probe-smoke 286 / 286・Vim の fixture differing 0）・50（木 `4a9511a8`・ctest 26 / 26・その後の試験 3 ファイルの commit は `nib_adapter_tests` だけ回した）・51（ctest の 1 回目は `nib_window` が 1 件・試験が「窓が掛けていない番号」に 8 を使っていた・試験 3 行を直して成功）は、どれも各まとめ枝の PR の本文が正本。
  まとめ枝 52 の全件: 木 `5ad83876` で 1 回（2026-11-25 10:07〜11:49・記録 `D:/NeNeNib/worktrees/int52/out/verify-int52c/check.log`）: conformance 0 violation・test-conformance 592 件・validate-git・format-check・configure・build・conformance `--build-dir`・symbols・ctest 26 本のうち 25 本成功（下の `nib_autosave` の 1 本）・probe-smoke・Release のビルド・prove-gates 33・diff-check。この記録を書いた時点（2026-11-25 11:16・走り書き）で分かっていること: 1 回目（木 `66be2a85`・#2039 を積む前）は ctest 26 本のうち 3 本が落ち（`nib_autosave` と `nib_adapters_long_paths` は Timeout・`nib_adapters_last_resort` は 1 件）、2 回目（木 `5ad83876`・10:07 開始）は build・conformance・symbols が 0 で ctest は 26 本のうち 25 本が成功、`nib_autosave` だけが Timeout（60.60 秒）。D ドライブ（HDD）の Disk Time が 500〜1600% の間の回で、自動退避の製品のコードと試験は main から変わっていない。切り分けの 1 回（`nib_autosave_tests.exe` 単体・11:13）は 152 checks・0 failures（`D:/NeNeNib/outputs/20261125/autosave-diagnostic-1.txt`）。設計席は機械のディスクの混みによる失敗と判定し、Issue #2049 を起こした（下の「残るリスク」）。残りの段（probe-smoke・Release・prove-gates・diff-check）は成功した（この段の先頭）。
  網羅率（QLT-009・90%）: `eng/coverage.py` は 2026-11-14 から落ちていて判定できていなかった（#1921）。PR #2012 で道具を直すと main は 88.55% で不合格と分かり（まとめ枝 51 の PR の本文）、#2013 の試験（PR #2016 と #2030）で戻した。PR #2030 の枝 `52bce3d7` での席の測りは 90.43%（31,654 / 35,004・core 91.44%・application 87.69%・閾値と `eng/coverage-policy.json` は変えていない・走り書き 11-24）。まとめ枝 52 の木での 1 回: 木 `5ad83876` で 90.43%（31,720 / 35,078・2026-11-25 14:16〜14:24・exit 0・記録 `D:/NeNeNib/worktrees/int52/out/verify-int52c/coverage.log`）。
- (d) 施主の試用（D41）: 2026-11-24 の夜に、hide が試用の exe（リポジトリの外の `D:/NeNeNib/evidence/20261124-trial/`・profile は別の置き場）で見た。
  - 名前の変更の PR の Release（`NeNeNib-s7a-38c8ec86.exe`・速さのゲートを測った exe）: 設定の画面（選ぶ欄の一覧・Alt+↓ の後の Alt・焦点の枠・外のクリック）・整形・clangd が 1 本、は「他は良さそう」。指摘 T-1（Vim の INSERT で補完の一覧が出ている間、↑↓ が候補を選ばずキャレットが動く）→ Issue #2024 → PR #2032。指摘 T-2（名前の変更が 1 か所だけ）は製品でなく確かめの準備が原因（写しの設定の clangd の命令に別のプロジェクトの `--compile-commands-dir` が付いていた）で、外した後のやり直しは hide「うん、1，2，3，4 大丈夫だと思う。」（3 ファイルの名前の変更と Ctrl+Z）。指摘 T-3（診断の文が出ているとモードが変わらないように見える・1.1.0 でも同じ）→ Issue #2025 → hide の決め「A で。」→ PR #2033。
  - `NeNeNib-arrows-73dea47e.exe`（#2024 の直し）: hide「うん、大丈夫！」。`NeNeNib-status-aeb2e572.exe`（#2025 の直し）: hide「うん、全部いいと思うよ」。
  - 実機で見ていない物（各まとめ枝の PR の「残るリスク」のまま）: split の境界線のドラッグ・`:LspLog`・診断の札・キャレットの動きと粒・インデントガイド・引数の札・参照と記号の一覧・#2039 の断り。本番の MSIX での確かめ（手順書 `D:/NeNeNib/evidence/v1.2.0-release/hide-checklist.md`）: 未実施（梱包の後）。hide の本物の `settings.v6` には古い 2 つの名前（`indent_guide_colors` `indent_guide_active`）の行が在るので、公開元の exe を渡すときに書き換えを案内する（走り書き 11-24）。
- (e) 梱包の道具の試験と、この PR の検証: 梱包の道具（`eng/package-release.{ps1,py}` `eng/package-msix.{ps1,py}`）は 1.1.0 の公開元から変わっていない（`git diff 4fcf258c 5ad83876 -- eng` は `coverage.py`（#1921）・`measure-speed.py` `verify-settings.py` `verify-window.py` `window_driver.py`（設定の鍵と `settings.v6` に合わせた測りと確かめの道具）・`eng/release/README.txt.in`（ZIP の `README.txt` の設定のファイルの名前を `settings.v6` に・2 行）の 6 ファイル）。`eng/test-package-release.py` と `tests.conformance.test_package_msix` と `nib_tests --version-info` は、版を上げた木（まとめ枝 52）の全件と CI で回る（上の印）。
  この PR（#2050・文書と掲載文だけ）の検証は PR の本文（`python eng/conformance.py`・`git diff --check`・掲載文の字数）。製品を変えないのでビルドと試験は回していない。

## 既知の限界

1.1.0 の記録の「既知の限界」を引き継ぐ。下は 1.2.0 で外す物と足す物（どれを公開の文に書くかは設計席が決める。公開の文の下書きは `D:/NeNeNib/evidence/v1.2.0-release/release-notes.md` の「既知の制限」で、席 F1 がまとめ枝 52 の `66be2a85` と照らした・`D:/NeNeNib/outputs/20261125/F1-factcheck.md` `F1-applied.md`）。

**1.1.0 の限界から外す物**（1.2.0 で入った・直った）:

- 「言語サーバーは同時に 1 本」（#1909 で言語ごとに・同時に 4 つまで）。
- 「引数の案内・参照の一覧・名前の変更は無い」（#1959 #1947 #2004）。
- 施主が 2026-11-21 の確かめで気になった 4 つ（設定の値の行が前へ戻れない #1926・文字の大きさの刻み #1927・日本語の「面」 #1928・焦点の行が見分けにくい #1929）。

**1.2.0 で足す物**:

- クイックフィックスは無い（次の版・D116）。
- 名前の変更（ADR 0194 の限界・README の FR-021）: 一度に 32 個を超えるファイルにまたがる答え・ファイルの作成 / 名前の変更 / 削除を含む答え・サーバーが開いていないタブに未保存の変更があるとき・サーバーが開いていないファイルが BOM なしの UTF-8 でない（または 4 MiB を超える）ときは、どこも変えずに 1 行で断る。サーバーがプロジェクトのほかのファイルをまだ読み込んでいないと、開いているファイルだけが変わることがある（Nib からは気づけない・hide の試用の T-2 と同じ形）。
- 名前の変更の答えを待つ間にタブを閉じた形（#2039 の後）: 頼んだ時点に在ったタブを答えの前に閉じると、タブの無いファイルを含む答えは、閉じたファイルと関係が無くても古い答えとして断る（頼み直せば通る）。プレビューのタブの置き換え・無題や記録のタブを閉じた形も同じく断る（安全の側）。タブを閉じずに「名前を付けて保存」で別のパスにした形は塞がっていない（答えを待つ数秒の間に保存のダイアログを終える形）。どちらも Issue #2042（編集の写しにパスを持たせる）で直す（クイックフィックスを main に入れる前）。
- 整形・記号の一覧・引数の情報には通常モードの鍵が無い（F1 の一覧から。記号の一覧は Vim の `gO`）。整形は Vim の INSERT の間は断り、入力の途中に届いた答えは当てない。
- 言語サーバーは同時に 4 つまで。5 つ目の言語を開くと、最も前に使ったサーバーを止めて譲る（5 言語以上を行き来すると止めると起こすが繰り返される・PR #1916 の「残るリスク」）。
- 参照と記号の一覧は 1,000 件まで。参照から別のファイルへ跳んだときは、`''` で元のファイルへは戻らない（定義へ移動と同じ）。
- 言語サーバーの補完は、自動の import・スニペットの跳び先を入れない。hover はマウスを載せては出ない（1.1.0 のまま）。
- 設定の「言語サーバー」の節の「探す」は PC の中の PATH と決まった置き場だけで、ネットワークのドライブと `\\` の場所は探さない（命令の行を手で書けば起こせる・ADR 0160 の 2026-11-22 の追補）。
- 自動で開いた補完の一覧で、`map` の左辺を打っている途中の Enter と Tab は確定しない。Vim の INSERT の ↑↓ は、Ctrl-n / Ctrl-p で開いた一覧では選ぶたびに本文へ入れる（本物の Vim は選ぶだけ・ADR 0190 の追補の限界・PR #2032）。
- 足した 16 言語には構文の色がまだ付かない。
- インデントガイド: 格子表示・巨大な行・Markdown のプレビュー・IME の変換中の行には出ず、折り返した行では最初の画面行にだけ引く。不可視文字の印と括弧の色分けは無い。
- キャレットの動きは OS のタイマーで刻む（約 16 ms の刻みのことがある・ADR 0192 の限界）。Windows の「アニメーション効果」がオフのときと IME の変換中は動かない。
- 設定の選ぶ欄の一覧は、決める前に配色を本文で見せない（← → で 1 つずつ試せる・H-5）。窓が焦点を失っても一覧は閉じない（H-7）。
- 2 分割の大きさは窓を閉じるまで（次の起動では等分に戻る）。
- 設定の互換（D91・D115）: 1.2.0 で設定を変えると `settings.v6` ができ、それ以後 1.1.0 で変えた設定（`settings.v5` に入る）は 1.2.0 から見えず、1.2.0 で変えた設定は 1.1.0 から見えない。どちらも壊れない。main の途中の版（公開していない）で保存した `settings.v6` に古い 2 つの名前の行が在ると、1.2.0 は既定の見た目で始めて保存を断る（上の「この版が書く設定の版と鍵の集合」）。
- `.h` は C の言語。c と cpp に同じ命令を置けば席を共有して `.h` も開かれるが、cpp だけの設定では、名前の変更を当てた後も `.h` は席に開かれず、保存するまで次の名前の変更が断られる（PR #2011 の「残るリスク」）。
- 各レビューの「後続」と「軽い指摘」のうち公開の文に書く物は `D:/NeNeNib/evidence/v1.2.0-release/release-notes.md` の「既知の制限」の節のとおり（2026-11-25 に現物と照らして 36 か所を直した・`D:/NeNeNib/outputs/20261125/F1-applied.md`）。名前の変更の残る限界は #2042。

## 残るリスク（公開の前）

- **`nib_autosave` の flush の試験がディスクの混んだ機械で落ちる**（Issue #2049・関連 #1703）: まとめ枝 52 の手元の全件で、ctest の `nib_autosave` が 2 回とも Timeout になった（上の (c)）。製品の `flush` は 3,000 ms で待ちを打ち切る設計（ADR 0137 決定 9）で、試験は build の下（D・HDD）に書いて `flush` が全部済んだと答えることを縛る。自動退避の製品のコードと試験は main から変わっていない。静かな機械（CI）での確かめ: まとめ枝 52 の CI（run 45553560292）の build-and-test は ctest を含めて success（`nib_autosave` も通った）。ゲートは弱めていない（3,000 ms も試験の期待も変えていない）。
- 実機の目視は hide の試用（(d)）。見ていない物は (d) の一覧のとおりで、本番の MSIX での確かめはこれから。
- WACK（Windows App Certification Kit）は未実施（梱包の後・hide の手）。1.1.0 は `PASS`。1.2.0 は通信・購入・広告を足していない（設定の「言語サーバー」の節の「探す」は PC の中だけ）。`runFullTrust` の理由と認定の注意書きの英語の文は 1.1.0 のままで、設定の画面の「使う」で命令の行を書けるようになった点を文に足すかは [submission.md](../store/submission.md) の「1.2.0 の更新の申請」の印。
- 名前の変更の残る穴（「名前を付けて保存」の形）と断りすぎは Issue #2042（上の「既知の限界」）。
- 速さの文（掲載文の「約 40 ms」「1 ms 未満」）は 1.0.0 の基準値の測りの物。まとめ枝 48〜51 のゲートの起動は 43.1〜46.7 ms（単独の PR は 44.4・49.3 ms）で、文をそのまま使うかは設計席がまとめ枝 52 のゲートの値で決める（[listing.md](../store/listing.md) の脚注）。
- Store の認定の結果はまだ（2026-11-25 夜に申請・下の「Microsoft Store の提出」）。

## 梱包

本番の物（公開元 `b8f0a919917a1f645bb29e0e65ce0872e9094354`・作業木 `D:/NeNeNib/worktrees/v1.2.0`・置き場 `D:/NeNeNib/releases/v1.2.0/`・2026-11-25 15:48〜15:49）。ログ `D:/NeNeNib/outputs/20261125/package-zip.log`・`package-msix.log`。値は設計席の照合 `D:/NeNeNib/evidence/v1.2.0-release/final-verification.md` と `package.json`・`msix.json` から写し、2026-11-27 に `stat` と `sha256sum` で読み直して同じ値。

| 物 | 名前 | bytes | SHA-256 |
| --- | --- | --- | --- |
| exe（`build/release-b8f0a91/NeNeNib.exe`） | `NeNeNib.exe` | 5,089,792 | `ca361007e85e58b4f9a8aff3a39e1b25230a9a058e67e7adf5f9815725cbf55e` |
| ZIP | `NeNeNib-1.2.0-windows-x64.zip` | 2,083,573 | `35ffc31ae36a09aab238cd1020d09662cc47edbb5248a79a47fca7144c854431` |
| ZIP の `SHA256SUMS` | `SHA256SUMS`（1 行） | 97 | `23b7920db16e43e97270881bf24166b793476d0fd8193bc8f8233293f62e4f8f` |
| MSIX（Store 用・署名なし） | `NeNeNib-1.2.0.0-windows-x64-store.msix` | 2,220,860 | `4e7731eb5ab437481f32bd4b54d5a902a503cf290278fa7bb350d06347c845d3` |
| MSIX の `SHA256SUMS` | `SHA256SUMS`（1 行） | 106 | `820cef43c245f8cea176cb7f5ab824e39e7a0d40cad7768b9d5a91ad3e1155b3` |

照合:

- [x] `package.json` と `msix.json` の `commit` が公開元（`builtFromWorktree` false）・`package.json` の `verification.path` がこの記録
- [x] `version` `1.2.0`・`fileVersion` `1.2.0.0`・`packageVersion` `1.2.0.0`・`store` true・`AppxManifest.xml` の `Identity` が `HideyukiMori.NeNeNib`・`Version="1.2.0.0"`・x64
- [x] ZIP の中と MSIX の中の `NeNeNib.exe` の SHA-256 がどちらも上の exe の値（`extractedBytesMatch` true・`zip.executableMatches` true）
- [x] manifest の `Resources` が `en-US` `ja-JP` `zh-Hans` の順
- [x] `msix.json` の `entries` が 32 個（1.1.0 と同じ数・`unpackedMatches` true）。ZIP は `NeNeNib.exe` `README.txt` `LICENSE` `THIRD_PARTY_NOTICES.txt` の 4 個。Release のビルドの warning 0（`zip/build.log`）・import は 15 個
- [x] 展開した exe の短い起動（`release-smoke.py`・隔離 profile・hide の了承の後に 1 回）: 2026-11-27 13:45 JST・`"result": "pass"`・exit 0・`documentUnchanged true`・題 `release-launch.txt - NeNe Nib`・DPI 120・結果 `D:/NeNeNib/evidence/v1.2.0-smoke-b8f0a919/result.json`
- [x] 製品の入力は速さのゲートを測った木と同じ（`git diff 5ad83876 b8f0a919 -- src tests eng third_party CMakeLists.txt` が空）

## GitHub の公開

tag `v1.2.0` → 公開元 `b8f0a919917a1f645bb29e0e65ce0872e9094354`・題 `NeNe Nib 1.2.0`（draft でも pre-release でもない）・2026-11-27 13:46:54 JST・https://github.com/hideyukiMORI/nene-nib/releases/tag/v1.2.0 ・添付は `NeNeNib-1.2.0-windows-x64.zip`（SHA-256 `35ffc31ae36a09aab238cd1020d09662cc47edbb5248a79a47fca7144c854431`）と `SHA256SUMS`。本文は日英（D120）。再取得の照合 `verify-publication.py` は `"result": "pass"`（`downloadedBytesMatch` true・`zipContentsMatch` true・release id 460177908）。公開は Store の公開を hide が知らせた後に設計席が `publish.ps1` で行った。

## Microsoft Store の提出

提出した物は MSIX `NeNeNib-1.2.0.0-windows-x64-store.msix`（2,220,860 bytes・SHA-256 `4e7731eb5ab437481f32bd4b54d5a902a503cf290278fa7bb350d06347c845d3`・`D:/NeNeNib/releases/v1.2.0/msix/SHA256SUMS` と同じ・2026-11-25 22:0x に統合リナが読み直した）。

| 順 | 誰 | 状態 | 日時・記録 |
| --- | --- | --- | --- |
| 手順書（`hide-checklist.md`） | hide | 申請まで進んだ | 紙（手順 10・11 の欄）は受け取っていない（申請の時刻だけ hide の答えで #2065） |
| WACK（手順 8） | hide | `OVERALL_RESULT="PASS"` | 報告の生成 2026-11-25 21:39:01・`APP_NAME="HideyukiMori.NeNeNib"`・`APP_VERSION="1.2.0.0"`・`APP_TYPE="Centennial"`・WACK `10.0.26100.7705`・OS `10.0.26300.0`・報告 `D:/NeNeNib/releases/v1.2.0/wack/report.xml`（ID `c46211a8de0acdf87414aa23ebd5bb93`） |
| 更新の申請（Submission 3） | hide | 提出 | 2026-11-25 22:00 ごろ（JST・hide「22:00くらいだと思うよ」）・Partner Center |
| 認定・Store で公開 | Microsoft | 認定・公開済み（hide の確認） | 2026-11-27 に hide が Store のページで 1.2.0 を確かめた。この PC の Store 版は `1.2.0.0`（`SignatureKind Store`）で、`NeNeNib.exe` の SHA-256 が ZIP の exe と同じ `ca361007e85e58b4f9a8aff3a39e1b25230a9a058e67e7adf5f9815725cbf55e`（2026-11-27 に設計席が照合） |

GitHub の tag `v1.2.0` と Release は Store の認定の後に出す（H-10・D120）。

## 公開の結果

公開版は 1.2.0（Microsoft Store と GitHub）。公開元は main `b8f0a919`。GitHub の再取得の照合は pass で、Store 版と ZIP の exe は同じ。`settings.v6` の鍵の集合はこの版で閉じた（足すなら `settings.v7`）。main の凍結（D116）はここで解き、次の版の PR はまとめ枝 53 から入れる。
