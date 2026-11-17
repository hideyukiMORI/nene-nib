# 1.1.0 の公開（2026-11-17・下書き）

公開の記録の下書き（席 REL・#1815・節立ては [release-1.0.0-2026-11-14.md](release-1.0.0-2026-11-14.md) と同じ）。この PR で埋めたのは「範囲」「公開元」の分かる所・「根拠」・「既知の限界」の候補だけで、**「梱包」「GitHub の公開」「Microsoft Store の提出」「公開の結果」は設計席が本番の梱包と公開の後に埋める**（`<<…>>` は埋め残しの印）。**この記録を置いた時点では、1.1.0 はまだ梱包も公開も申請もしていない**（公開版は 1.0.0・公開元 `80fd7a53`）。
1.1.0 は Microsoft Store の 2 回目の申請（「更新」）で、施主の決め「41 で行こう」（2026-11-17 夜）のとおり、まとめ枝 41 を区切りにする。GitHub は同じ exe の ZIP を tag `v1.1.0` で出し、**題に「Beta」を付けず pre-release の印も付けない**（題 `NeNe Nib 1.1.0`・2026-11-17 夜の施主の決め・1.0.0 は `NeNe Nib 1.0.0 Beta`・pre-release）。手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md) の「1.1.0 の更新の申請」と [msix.md](../store/msix.md)、リポジトリの外の手順と道具は `D:/NeNeNib/evidence/v1.1.0-release/`。

**範囲**: 1.0.0 の公開元 `80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6` から、まとめ枝 41（`origin/chore/1808-integration-20261117b`・この PR の起点 `99f02b18`）までの main。`git log --oneline 80fd7a53..99f02b18` は 56 本、`git diff --stat 80fd7a53 99f02b18 -- src` は 305 ファイル（+15,059 / −886）。製品の変更は次のまとめ枝で入った（日付は JST）。**タブの右クリックのメニュー（#1790・PR #1813）は入れない**（施主の決め・まとめ枝 41 が区切り）。

| まとめ枝 | PR（merge） | 製品の変更（Issue / 元の PR） |
| --- | --- | --- |
| 36（#1743） | #1762（2026-11-16 13:07） | 空の無題に開く道も文書を離れる 1 本（#1583 / PR #1662）・入力行の Ctrl-U Ctrl-W と `/ ?` の Tab（#1550 / PR #1669）・`\zs` `\ze`（#1653 / PR #1667）・`:s` の `c` フラグ（#1652 / PR #1666）・Ctrl+P の面の `:set` の値（#1527 / PR #1663）・NORMAL の `s` `S`（#1674 / PR #1676）・`:d` `:y` `:g/pat/d` `:v/pat/d` `:g/pat/s//rep/`（#1654 / PR #1673）・食い違いの直し 3 本（#1743） |
| 37（#1771） | #1778（2026-11-17 10:2x） | Markdown プレビューの大きい段落の固まり（#1485 / PR #1755）・格子の 1 字を繰り返した巨大な行の固まり（#1711 / PR #1757）・改行で終わる本文を Vim のモードで Vim と同じ行の数に（#1741 / PR #1761・D94・ADR 0188） |
| 38（#1776） | #1783（2026-11-17 12:1x） | プロジェクトの全文検索 G1〜G3（#1758 #1763 #1772 / PR #1760 #1764 #1773・FR-026・ADR 0167）: Ctrl+Shift+F・`:grep`・面の出どころ `!` |
| 39（#1788） | #1796（2026-11-17 16:5x） | 自動の字下げと `settings.v5`（#1086 / PR #1668・D91・ADR 0170 の追補）・イージーモーション M2〜M4（#1774 #1779 #1782 / PR #1777 #1781 #1786・FR-023・ADR 0163）と空の行の印の直し（#1788） |
| 40（#1797） | #1804（2026-11-17 20:3x） | 本文の右クリックのメニュー（#1051 / PR #1791・D97）・探して見る面のプレビュー欄 P1 P2（#1785 #1793 / PR #1792 #1794・D96・ADR 0189）・試験の見本の名前（#1797） |
| 41（#1808） | PR #1817・2026-11-18 00:4x rebase merge（main `3bebc927`） | タブの札（#1789 / PR #1802・D98）・イージーモーションの印を左肩の札に（#1800 / PR #1801・D99）・予約名 ＋ 拡張子のファイル名（#1805 / PR #1812）・Store 版で右クリックへの登録の 2 つを F1 の一覧から隠す（#1809 / PR #1814・ADR 0178 の追補）。V-1814 の必須 1（`distribution_of` が分からない答えを packaged に倒す）の直しと `MotionLabel.hpp` の整形の直しを積んだまとめ枝 41 は `27ce4009`（この PR の枝は `git merge` で追いついた） |

製品の外: 道具の直し（#1719 #1769 #1780 #1784）・CI の上限（#1752）・文書。版を 1.1.0 に上げたのはこの PR（`CMakeLists.txt` の `project()` の 1 行・MSIX の版は `1.1.0.0`）。

**この版が書く設定の版と鍵の集合**（ADR 0170 の 2026-11-16 の追補の 4）: 1.1.0 は `settings.v5` を書く最初の公開版で、v5 の鍵の集合はここで閉じる。鍵は v4 の全部（`version` `colorscheme` `font_family` `font_size` `guide` `wrap` `ui_language` `lsp` `explorer` `root` `features` `mapleader` `ignorecase` `smartcase` `relativenumber` `scrolloff` `explorer_width`）に `autoindent`（#1086・既定 off）と `finder_preview`（#1793・既定 `auto`・ADR 0189）を足したもの、と `lsp.<language>.command` と `map.<mode>.<lhs>` の行（`src/adapters/win32/SettingsCodec.cpp` の `field_names` と接頭辞・試験 `verify_released_v4_settings` の `released_as_v5`）。ADR 0170 の追補の「足した鍵の記録（v5）」にも 2 つを書いた（この PR で `finder_preview` を足した）。1.1.0 の後は `settings.v5` に鍵を足さない（足すなら `settings.v6`・CLAUDE.md §2）。`session.v2` `history.v1` `bookmarks.v1` `autosave.v1` の codec は 1.0.0 から変わっていない（`git diff 80fd7a53 99f02b18 -- src/adapters/win32/{Session,History,Bookmark,Autosave}Codec.*` は空）。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main） | `<<公開元>>`（まとめ枝 41 とこの PR が main に入った後の commit。40 桁は `git rev-parse origin/main` から写す） |
| 前の公開 | `v1.0.0`（`80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6`・GitHub 2026-11-17 20:53 JST・Store で公開済み・[記録](release-1.0.0-2026-11-14.md)） |
| 版（`project()`） | `1.1.0`（この PR・#1815） |
| 梱包の作業木 | `<<公開元の detached 作業木（HEAD = origin/main = 公開元・未追跡 0）>>` |

## 根拠（再利用する検証）

製品の変更は、まとめ枝ごとの (a)〜(c) と、各 PR の独立レビューで受けた。この PR の製品の差は版の 1 行だけ（`CMakeLists.txt`）で、梱包の道具は変えていない。

- (a) 速さのゲート（QLT-014・どれも **8 benches checked・0 regression・0 unmeasurable**・5 runs）:

  | まとめ枝の木 | 記録 | 起動 / 窓 / 単打（中央値 ms） |
  | --- | --- | --- |
  | 36 `5594b41e` | `D:/NeNeNib/outputs/20261116/gate-integration36-5594b41e.log` と `-load.txt` | 43.761 / 32.907 / 0.616 |
  | 37 `d18ef324` | `D:/NeNeNib/outputs/20261117/gate-integration37-d18ef324.log` | 43.487 / 32.383 / 0.528 |
  | 38 `8baaec6f` | `…/gate-integration38-8baaec6f.log` | 42.626 / 31.971 / 0.600 |
  | 39 `0afe33c7` | `…/gate-integration39-0afe33c7.log`（この後に M3・M4 の直しを積み直した） | 45.411 / 33.331 / 0.541 |
  | 39 `52b49c63` | `…/gate-integration39c-52b49c63.log`。**`…39b-52b49c63.INVALID.log` は無効**（計測の途中に別の席が `nib_tests` を回した・数字は通っていたが採らない・日報 11-17 の「ゲートの 1 回が無効になった」） | 41.651 / 30.063 / 0.613 |
  | 40 `0afc6a05` | `…/gate-integration40-0afc6a05.log` | 42.493 / 30.136 / 0.625 |
  | 41 | `27ce4009`（V-1814 の直しと整形の直しを積んだ木）・2026-11-17 23:53・8 本 0 regression（起動 44.639・単打 0.593・16 MiB を開く 51.417・面 1.054・長い行 1.130 ms）・`D:/NeNeNib/outputs/20261117/gate-integration41-final.log` | |

  公開元の木とゲートを測った木の製品の入力の差（`git diff <まとめ枝 41 のゲートの木> <<公開元>> -- src third_party tests CMakeLists.txt eng`）は、この PR の版の 1 行だけのはず → `<<設計席が確かめる>>`。版の 1 行は速さに関わらないので、梱包の exe では測らない（1.0.0 と同じ形）。
- (b) 独立レビュー（読むだけの席・所在の `review.md` が正本・無いものは席の返事が正本で走り書きに要約）:

  | PR | レビュー | 判定（その後） |
  | --- | --- | --- |
  | #1662 #1663 #1666 #1667 #1669 #1673 #1676（まとめ枝 36） | `D:/NeNeNib/outputs/20261114/review-1662` `-1663` `-1663b` `-1666` `-1666b` `-1667` `-1669` `-1673` `-1676` | まとめ枝 36 の記録（走り書き `outputs/20261115/` と `20261116/notes.md`）`<<設計席が判定の行を写す>>` |
  | #1755（#1485） | `outputs/20261116/review-1755/review.md` | 直してから（必須は文書と試験の 1 件）→ 直した `73cec7c5` を「積んでよい」（日報 11-16） |
  | #1757（#1711） | `outputs/20261116/review-1757/review.md` | 「積んでよい」`231d5732`（日報 11-16） |
  | #1760（G1） | 席の返事（走り書き `20261116/notes.md` 11:3x） | 直してから（必須 2）→ 再レビューで積んでよい |
  | #1761（#1741） | 1 回目は席の返事（走り書き 12:5x・必須 2）・2 回目 `outputs/20261117/review-1761b/review.md` | 積んでよい（必須 0・Vim と照らした新しい差 0） |
  | #1764（G2） | `outputs/20261117/review-1764/review.md` | 必須 1 → 直して積んだ（日報 11-17） |
  | #1773（G3） | `outputs/20261117/review-1773/review.md` | 積んでよい（必須 0・D-1 クラウドの扱いは設計席が決めた） |
  | #1668（#1086・settings.v5） | `outputs/20261117/review-1668/review.md` | 積んでよい（必須 0） |
  | #1777（M2） | `outputs/20261117/review-1777/review.md` | 積んでよい（必須 0） |
  | #1781 #1786（M3 M4） | `outputs/20261117/review-1781-1786/review.md` | 直してから（必須 1・空の行の跳び先）→ 直して積んだ |
  | #1791（#1051） | `outputs/20261117/review-1791/review.md`・`review-1791b/review.md` | 直してから（必須 3 → 必須 1）→ 直して積んだ |
  | #1792 #1794（P1 P2） | `outputs/20261117/review-1792/review.md`・`review-1794/review.md` | 積んでよい（必須 0）・直してから（必須 2）→ 直して積んだ |
  | #1801（#1800） | `outputs/20261117/review-1801/review.md` | 積んでよい（必須 0） |
  | #1802（#1789） | `outputs/20261117/review-1802/review.md` | 積んでよい（必須 0） |
  | #1812（#1805） | `outputs/20261117/review-1812/review.md` | 積んでよい（必須 0） |
  | #1814（#1809） | `outputs/20261117/review-1814/review.md` | 直してから（必須 1）→ 直しをまとめ枝 41 の `27ce4009` までに積んだ（直しは席の `c301039f` の差分・`nib_adapter_tests` 1,359 checks・まとめ枝 41 の全件の検証と CI [44108583088](https://github.com/hideyukiMORI/nene-nib/actions/runs/44108583088) で確かめた） |

- (c) CI（merge の道具の run・どれも `check` と `build-and-test` が success）: まとめ枝 36 [43798044246](https://github.com/hideyukiMORI/nene-nib/actions/runs/43798044246)・37 [43990290226](https://github.com/hideyukiMORI/nene-nib/actions/runs/43990290226)・38 [44005218509](https://github.com/hideyukiMORI/nene-nib/actions/runs/44005218509)・39 [44036071609](https://github.com/hideyukiMORI/nene-nib/actions/runs/44036071609)・40 [44069448664](https://github.com/hideyukiMORI/nene-nib/actions/runs/44069448664)（1 回目は `nib_adapters_preview` の 1 件で failure・試験の見本 `nul.bin` が windows-2022 で予約名の装置に当たった → #1797 で直して 2 回目）・41 [44108583088](https://github.com/hideyukiMORI/nene-nib/actions/runs/44108583088)（success・2026-11-18 00:4x）。#1805 の付け直しの道は Windows 10 / Server 2022 でだけ働くので、意味のある証拠は CI（windows-2022）の `nib_adapters_long_paths`（PR #1812 の本文）。
- (d) 施主の試用（D41）: 1.1.0 の機能はまだ施主が試していない（実機の絵は日報 11-17 のとおり席と設計席が撮って送った）。本番の MSIX と ZIP で `D:/NeNeNib/evidence/v1.1.0-release/hide-checklist.md` を通す → `<<結果>>`。packaged の起動・Store 版で F1 の一覧に登録の操作が出ないこと・WACK もそこで見る。
- (e) 梱包の道具の試験と、この PR の検証: 席 REL が版を上げた木で `eng/test-package-release.py` 10 件 OK・`tests.conformance.test_package_msix` 18 件 OK・Debug の全 target 警告 0・`nib_tests` の既定の実行 2,423,869 checks・`--version-info` 19・`--start-screen` 246・`nib_adapter_tests` 1,358・conformance 0（2026-11-17）。main に載せ直した木は設計席が conformance 0 を確かめ、CI は merge のときに回る（`eng/test-package-release.py` と `tests.conformance.test_package_msix`・Debug の全 target・`nib_tests`・adapters の試験・conformance）。

## 既知の限界

1.0.0 の記録の「既知の限界」を引き継ぐ（1.1.0 で直った物は下で外す）。**1.1.0 で増えた物の候補（どれを公開の文に書くかは設計席が決める）**:

- 1.0.0 の「`:s` の旗 `c` は断る」は外す（#1652 で入った）。`:g` の中の `c` フラグは断る（#1743）。`:g` のほかの命令・`:sort`・入力中のプレビューは無い。
- 1.0.0 の「Markdown プレビューで長い段落を初めて表示する 1 回は、まだ少し待つ」は #1485 で縮んだ（最初の 1 回 366.7 → 168.9 ms・日報 11-16）が、0 にはなっていない（README の FR-007 のまま）。
- 設定の互換（D91・ADR 0170 の追補の 5）: 1.1.0 で設定を変えると `settings.v5` ができ、それ以後 1.0.0 で変えた設定（`settings.v4` に入る）は 1.1.0 から見えず、1.1.0 で変えた設定は 1.0.0 から見えない。どちらも壊れない。
- 全文検索（ADR 0167 の決定 8 と追補）: 当たりは 1,000 件で打ち切り・1 行に 1 つ・正しい UTF-8 でない行（Shift_JIS などのファイル）は当たらない・16 MiB を超えるファイルと最初の 8 KiB に NUL のあるファイルと固定の表のフォルダ（`.git` `node_modules` `build` `out`・隠し属性）は飛ばす・置換と `.gitignore` は無い・毎回読む（索引は無い）・絞り込みの効かない検索語は 1 MiB の塊が 5〜15 ms（UI は止まらない）・中身が端末に無いクラウドのファイルは読まない。
- プレビュー欄（ADR 0189）: 16 MiB を超えるファイルは出さない・構文の色は窓の先頭から始める（複数行のコメントの途中から始まると色が違う）・境のドラッグは無い。待ちの間の演出「解読」（決定 6）は入っていない。
- イージーモーション（ADR 0163 の決定 9）: オペレータの後（`d\\w`）・`t` `T` `n` `N`・2 枠のもう 1 つの枠・回数は無い。V-1801 の軽い指摘（左端の跳び先の札の左の縁が切れる ほか）は後続。
- 本文の右クリックのメニュー: 項目は D97 の 7 つだけ。タブの右クリックのメニュー（D98 の (2)）は 1.1.0 に無い。
- 表示言語は日本語・英語・中国語（簡体字）のまま。韓国語・スペイン語・ドイツ語は後の版（施主決定 D100・D88 と FR-029 の「1.1.0」を改めた）。
- 予約名の直し（#1805）: 装置の名前そのもの（`NUL` `nul.`）は装置のまま。言語サーバーの `CreateProcessW` の実行ファイルの経路は `native_path` を通らない（PR #1812 の「残るリスク」）。
- 各レビューの「後続」と「軽い指摘」のうち公開の文に書く物は `<<設計席が決める>>`。

## 梱包

`<<本番の物（公開元・作業木・置き場 D:/NeNeNib/releases/v1.1.0/）の表と照合 7 項目。形は 1.0.0 の記録と同じ>>`

| 物 | 名前 | bytes | SHA-256 |
| --- | --- | --- | --- |
| exe | `NeNeNib.exe` | `<<>>` | `<<>>` |
| ZIP | `NeNeNib-1.1.0-windows-x64.zip` | `<<>>` | `<<>>` |
| ZIP の `SHA256SUMS` | `SHA256SUMS` | `<<>>` | `<<>>` |
| MSIX（Store 用・署名なし） | `NeNeNib-1.1.0.0-windows-x64-store.msix` | `<<>>` | `<<>>` |
| MSIX の `SHA256SUMS` | `SHA256SUMS` | `<<>>` | `<<>>` |

照合: `<<package.json と msix.json の commit が公開元・version 1.1.0・packageVersion 1.1.0.0・store true・executableMatches・Resources の順・entries の数・短い起動>>`

## GitHub の公開

`<<tag v1.1.0 → 公開元・題 NeNe Nib 1.1.0（pre-release なし）・URL・添付・再取得の照合（verify-publication.py の result）>>`

## Microsoft Store の提出

`<<提出した MSIX・手順書と WACK の結果（hide）・更新の申請（Submission 2）の日時・認定と公開>>`

## 公開の結果

`<<公開版・公開元・照合の結果・次>>`
