# 1.1.0 の公開（2026-11-17・下書き）

公開の記録の下書き（席 REL・#1815・節立ては [release-1.0.0-2026-11-14.md](release-1.0.0-2026-11-14.md) と同じ）。この PR で埋めたのは「範囲」「公開元」の分かる所・「根拠」・「既知の限界」の候補だけで、**「梱包」「GitHub の公開」「Microsoft Store の提出」「公開の結果」は設計席が本番の梱包と公開の後に埋める**（`<<…>>` は埋め残しの印）。**この記録を置いた時点では、1.1.0 はまだ梱包も公開も申請もしていない**（公開版は 1.0.0・公開元 `80fd7a53`）。
1.1.0 は Microsoft Store の 2 回目の申請（「更新」）で、施主の決め「46 で区切る」（2026-11-19 夜・まとめ枝 41 を区切りにした 2026-11-17 夜の決めを改めた）のとおり、まとめ枝 46 を区切りにする（区切りを合わせ直したのは席 REL2・#1891）。GitHub は同じ exe の ZIP を tag `v1.1.0` で出し、**題に「Beta」を付けず pre-release の印も付けない**（題 `NeNe Nib 1.1.0`・2026-11-17 夜の施主の決め・1.0.0 は `NeNe Nib 1.0.0 Beta`・pre-release）。手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md) の「1.1.0 の更新の申請」と [msix.md](../store/msix.md)、リポジトリの外の手順と道具は `D:/NeNeNib/evidence/v1.1.0-release/`。

**範囲**: 1.0.0 の公開元 `80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6` から、まとめ枝 46（PR #1889・main `2277217c`）と文書の PR #1890（#1880・main `8d4be38d`・区切りを合わせ直した PR #1891 の起点）までの main。`git log --oneline 80fd7a53..8d4be38d` は 87 本、`git diff --shortstat 80fd7a53 8d4be38d -- src` は 579 ファイル（+38,372 / −1,978）。うちまとめ枝 41 の後（版を上げた `a5a85072` から `8d4be38d`）は 28 本・`src` の 357 ファイル（+23,532 / −1,314）。製品の変更は次のまとめ枝で入った（日付は JST）。

| まとめ枝 | PR（merge） | 製品の変更（Issue / 元の PR） |
| --- | --- | --- |
| 36（#1743） | #1762（2026-11-16 13:07） | 空の無題に開く道も文書を離れる 1 本（#1583 / PR #1662）・入力行の Ctrl-U Ctrl-W と `/ ?` の Tab（#1550 / PR #1669）・`\zs` `\ze`（#1653 / PR #1667）・`:s` の `c` フラグ（#1652 / PR #1666）・Ctrl+P の面の `:set` の値（#1527 / PR #1663）・NORMAL の `s` `S`（#1674 / PR #1676）・`:d` `:y` `:g/pat/d` `:v/pat/d` `:g/pat/s//rep/`（#1654 / PR #1673）・食い違いの直し 3 本（#1743） |
| 37（#1771） | #1778（2026-11-17 10:2x） | Markdown プレビューの大きい段落の固まり（#1485 / PR #1755）・格子の 1 字を繰り返した巨大な行の固まり（#1711 / PR #1757）・改行で終わる本文を Vim のモードで Vim と同じ行の数に（#1741 / PR #1761・D94・ADR 0188） |
| 38（#1776） | #1783（2026-11-17 12:1x） | プロジェクトの全文検索 G1〜G3（#1758 #1763 #1772 / PR #1760 #1764 #1773・FR-026・ADR 0167）: Ctrl+Shift+F・`:grep`・面の出どころ `!` |
| 39（#1788） | #1796（2026-11-17 16:5x） | 自動の字下げと `settings.v5`（#1086 / PR #1668・D91・ADR 0170 の追補）・イージーモーション M2〜M4（#1774 #1779 #1782 / PR #1777 #1781 #1786・FR-023・ADR 0163）と空の行の印の直し（#1788） |
| 40（#1797） | #1804（2026-11-17 20:3x） | 本文の右クリックのメニュー（#1051 / PR #1791・D97）・探して見る面のプレビュー欄 P1 P2（#1785 #1793 / PR #1792 #1794・D96・ADR 0189）・試験の見本の名前（#1797） |
| 41（#1808） | PR #1817・2026-11-18 00:4x rebase merge（main `3bebc927`） | タブの札（#1789 / PR #1802・D98）・イージーモーションの印を左肩の札に（#1800 / PR #1801・D99）・予約名 ＋ 拡張子のファイル名（#1805 / PR #1812）・Store 版で右クリックへの登録の 2 つを F1 の一覧から隠す（#1809 / PR #1814・ADR 0178 の追補）。V-1814 の必須 1（`distribution_of` が分からない答えを packaged に倒す）の直しと `MotionLabel.hpp` の整形の直しを積んだまとめ枝 41 は `27ce4009`（この PR の枝は `git merge` で追いついた） |
| 42（#1849） | PR #1856（2026-11-18 19:37 rebase merge・main `f541d3fb`・7 commit）。同じ commit `e688eb5b` の PR #1855 は CI の `check` が GIT-002 で落ちたので閉じて出し直した（`build-and-test` は成功・走り書き 11-18 18:44） | 言語サーバーの要求の土台（#1825 / PR #1831・ADR 0190）・文書ごとの語の表（#1824 / PR #1832・ADR 0169 の追補）・通常モードの補完の面 Ctrl+Space と triggerCharacters（#1833 / PR #1844）・hover の札 `K` と定義へ `gd` F12（#1834 / PR #1843）・設定の面（右の引き出し・#1836 / PR #1847・土台 #1828 / PR #1835・D102）・Vim の INSERT の Ctrl-n Ctrl-p Ctrl-e Ctrl-y と通常モードの Ctrl+I（#1845 / PR #1850・D104）・Markdown プレビューの打ち消し線・裸の URL・実体参照・front matter・表・GitHub の alert（#1848 / PR #1852・段 M1 #1840 / PR #1846・D103） |
| 43（#1861） | PR #1864（2026-11-18 23:52 rebase merge・main `23b83671`・2 commit） | `nib_tests` は引数が 2 つ以上なら使い方を出して失敗で終わる（#1853 / PR #1859・試験の道具）・Markdown プレビューの囲みのコードを本文と同じ構文の色で描く（#1858 / PR #1860・ADR 0151 の追補） |
| 44（#1875） | PR #1878（2026-11-19 13:10 rebase merge・main `73b3b856`・5 commit） | 設定の面を閉じる 5 つの道の変換の取り消しを数える試験（#1854 / PR #1870・製品の順と回数は同じ）・語の表を最初に作る 1 回の probe（#1868 / PR #1871）・Markdown のリンクの宛先と種類 `link_destination`（#1869 段 L1 / PR #1873・この段では何も開かない）・語の表を作る道の高速化（#1872 / PR #1874）・タブの右クリックのメニュー（#1790 / PR #1813・D98 の (2)・ADR 0078 の追補） |
| 45（#1879） | PR #1882（2026-11-19 15:33 rebase merge・main `f000c42c`・2 commit） | 長い囲みのコードを 128 行ごとの切れに分けて色を保つ（#1863 / PR #1877）・プレビューのリンクを札で確かめてから開く・写す（#1869 段 L2 / PR #1876・D106・`LinkOpenPort`） |
| 46（#1887） | PR #1889（2026-11-19 21:55 rebase merge・main `2277217c`・2 commit）。同じ木の枝 `…20261119e` は commit の本文に `Closes` の行が無く merge の道具が断ったので、本文だけ直した `…20261119f` で出した（`outputs/20261119/merge-1888.log`） | `#見出し` のリンクでその見出しへ跳ぶ（#1869 段 L3 / PR #1881・`third_party/github-slugger` の `regex.js` から生成した表）・枠より広い表の横の送り（#1883 / PR #1884・D107） |

製品の外: 道具の直し（#1719 #1769 #1780 #1784）・CI の上限（#1752）・文書。版を 1.1.0 に上げたのは #1815 の PR #1818（main `a5a85072`・`CMakeLists.txt` の `project()` の 1 行・MSIX の版は `1.1.0.0`）で、まとめ枝 42〜46 はどれもその上で作って測った。

**この版が書く設定の版と鍵の集合**（ADR 0170 の 2026-11-16 の追補の 4）: 1.1.0 は `settings.v5` を書く最初の公開版で、v5 の鍵の集合はここで閉じる。鍵は v4 の全部（`version` `colorscheme` `font_family` `font_size` `guide` `wrap` `ui_language` `lsp` `explorer` `root` `features` `mapleader` `ignorecase` `smartcase` `relativenumber` `scrolloff` `explorer_width`）に `autoindent`（#1086・既定 off）と `finder_preview`（#1793・既定 `auto`・ADR 0189）を足したもの、と `lsp.<language>.command` と `map.<mode>.<lhs>` の行（`src/adapters/win32/SettingsCodec.cpp` の `field_names` と接頭辞・試験 `verify_released_v4_settings` の `released_as_v5`）。ADR 0170 の追補の「足した鍵の記録（v5）」にも 2 つを書いた（#1815 の PR で `finder_preview` を足した）。1.1.0 の後は `settings.v5` に鍵を足さない（足すなら `settings.v6`・CLAUDE.md §2）。`session.v2` `history.v1` `bookmarks.v1` `autosave.v1` の codec は 1.0.0 から変わっていない（`git diff 80fd7a53 8d4be38d -- src/adapters/win32/{Session,History,Bookmark,Autosave}Codec.*` は空）。まとめ枝 42〜46 は設定の鍵を足していない（設定の面は閉じるときに `settings.v5` の今の鍵で 1 回保存する・`git diff a5a85072 8d4be38d -- src/adapters/win32/SettingsCodec.*` は空）。

Vim の fixture は `tests/vim/fixtures.json` が 6,892 件（まとめ枝 41 の `a5a85072` の 6,781 件から 111 件増えた・増えた分は INSERT の補完の `insert-complete-` の 111 件と同じ数）、折り返しの窓の `tests/vim/wrap-fixtures.json` は 402 件のまま（`8d4be38d` で数えた）。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main） | `<<公開元>>`（まとめ枝 46 と文書の PR #1890 と、区切りを合わせ直したこの PR（#1891）が main に入った後の commit。40 桁は `git rev-parse origin/main` から写す） |
| 前の公開 | `v1.0.0`（`80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6`・GitHub 2026-11-17 20:53 JST・Store で公開済み・[記録](release-1.0.0-2026-11-14.md)） |
| 版（`project()`） | `1.1.0`（PR #1818・#1815・main `a5a85072`） |
| 梱包の作業木 | `<<公開元の detached 作業木（HEAD = origin/main = 公開元・未追跡 0）>>` |

## 根拠（再利用する検証）

製品の変更は、まとめ枝ごとの (a)〜(c) と、各 PR の独立レビューで受けた。版の 1 行（`CMakeLists.txt`・PR #1818）はまとめ枝 42 より前に main に入っていて、42〜46 のゲートと CI はその版の木で回った。区切りを合わせ直したこの PR（#1891）は文書と掲載文だけで、製品と梱包の道具は変えていない。

- (a) 速さのゲート（QLT-014・5 runs）。36〜43・45・46 は **8 benches checked・0 regression・0 unmeasurable**。**44 だけは 2 本が上限を越えた**（下の表の 44 の行・同じ時間帯の main との A/B で機械の状態と示して入れた）:

  | まとめ枝の木 | 記録 | 起動 / 窓 / 単打（中央値 ms） |
  | --- | --- | --- |
  | 36 `5594b41e` | `D:/NeNeNib/outputs/20261116/gate-integration36-5594b41e.log` と `-load.txt` | 43.761 / 32.907 / 0.616 |
  | 37 `d18ef324` | `D:/NeNeNib/outputs/20261117/gate-integration37-d18ef324.log` | 43.487 / 32.383 / 0.528 |
  | 38 `8baaec6f` | `…/gate-integration38-8baaec6f.log` | 42.626 / 31.971 / 0.600 |
  | 39 `0afe33c7` | `…/gate-integration39-0afe33c7.log`（この後に M3・M4 の直しを積み直した） | 45.411 / 33.331 / 0.541 |
  | 39 `52b49c63` | `…/gate-integration39c-52b49c63.log`。**`…39b-52b49c63.INVALID.log` は無効**（計測の途中に別の席が `nib_tests` を回した・数字は通っていたが採らない・日報 11-17 の「ゲートの 1 回が無効になった」） | 41.651 / 30.063 / 0.613 |
  | 40 `0afc6a05` | `…/gate-integration40-0afc6a05.log` | 42.493 / 30.136 / 0.625 |
  | 41 | `27ce4009`（V-1814 の直しと整形の直しを積んだ木）・2026-11-17 23:53・8 本 0 regression（起動 44.639・単打 0.593・16 MiB を開く 51.417・面 1.054・長い行 1.130 ms）・`D:/NeNeNib/outputs/20261117/gate-integration41-final.log` | |
  | 42 `e688eb5b` | `D:/NeNeNib/outputs/20261117/gate-int42.log` と `-load.txt`（2026-11-18 17:48〜17:50・負荷 3 / 8 / 4%・施主の了承「ゲートの件、いいよ」の後） | 46.824 / 32.824 / 0.571（16 MiB を開く 53.055） |
  | 43 `ccdec3f6` | `…/20261117/gate-int43.log`（2026-11-18 23:2x・負荷 2 / 5 / 6%・施主の「今静かだから走らせて」の後） | 46.022 / 33.281 / 0.627（53.606） |
  | 44 `c2a0bb4a`（枝 `a` の Release。merge した枝 `b` `aa85409f` との木の差は試験の `OperationChoiceProbe.cpp` だけ） | `D:/NeNeNib/outputs/20261119/gate-int44.log` と `-load.txt`（負荷 7 / 3 / 3%）。**8 benches checked・2 regression(s)**: 起動 53.227 ms > 上限 52.357・16 MiB を開く 63.719 ms > 上限 62.236（標本の幅は起動 43.9〜73.5 ms）。合格まで測り直さず、同じ時間帯に main `8f5b1924` の Release と交互に A B B A × 2 で測った（`D:/NeNeNib/scripts/20261119/ab-startup.sh`・判定なし・`outputs/20261119/ab-startup.log`）: 起動の中央値 main 47.8 / 47.3 / 48.5 / 39.3 ms 対 まとめ枝 53.6 / 46.6 / 48.2 / 46.7 ms、16 MiB を開く main 60.6 / 56.3 / 56.8 / 47.3 ms 対 まとめ枝 58.2 / 56.2 / 55.1 / 55.3 ms。main も同じ時間帯に基準値（起動 41.9・open 49.8 ms）より遅く、差は揺れの中なので、2 本は機械の状態によるもので製品の悪化は無いと判断して入れた（ADR 0075・D41・数字は PR #1878 の本文のまま） | 53.227 / 39.077 / 0.535（63.719） |
  | 45 `43c7bbb8` | `…/20261119/gate-int45.log`（負荷 8 / 6 / 8%） | 50.420 / 36.810 / 0.564（60.042） |
  | 46 `4373ea5b`（木 `e939c19b` は merge した `7f01cf0e` と main `2277217c` と同じ） | `…/20261119/gate-int46.log`（2026-11-19 21:12・負荷 10 / 2 / 1%・施主に確かめてから） | 44.991 / 33.218 / 0.628（51.071） |

  公開元の木と最後にゲートを測った木（まとめ枝 46 `4373ea5b`）の製品の入力の差: `git diff 4373ea5b 8d4be38d -- src third_party tests CMakeLists.txt eng` は空（席 REL2 が確かめた・`8d4be38d` はこの PR の起点）。この PR（#1891）は文書と掲載文だけなので、公開元でも空のはず → `<<設計席が公開元で確かめる>>`。梱包の exe では測らない（1.0.0 と同じ形）。
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
  | #1831 #1832 #1835 #1843 #1844 #1846 #1847 #1850 #1852（まとめ枝 42） | `D:/NeNeNib/outputs/20261118/review-1831.md` `review-1831b.md`・`review-1832.md` `review-1832b.md`・`review-1835.md` `review-1835b.md`・`review-1843.md` `review-1843b.md`・`review-1844.md` `review-1844b.md`・`review-1846.md`・`review-1847.md` `review-1847b.md`・`review-1850.md` `review-1850b.md`・`review-1852.md` `review-1852b.md` | どれも最後の再確認で必須 0（PR #1856 の本文）。#1850 の最後の取り込みの commit（衝突 4 ファイル）はレビューの後に入り、設計席が鍵の振り分けの差分だけ読んで、まとめ枝の全件の検証で受けた |
  | #1860（#1858・まとめ枝 43） | `outputs/20261118/review-1860.md`・`review-1860b.md` | 必須 1（色の区間の数の 2 乗で `SetDrawingEffect` が伸びる）→ 直して再確認で必須 0。#1859（#1853・試験の道具）は設計席が差分を読んだ |
  | #1870 #1873 #1874 #1813（まとめ枝 44） | `outputs/20261119/review-1870.md`・`review-1873.md`・`review-1874.md`・`review-1813.md` | #1870 必須 0・#1873 必須 3 → 直して再確認で必須 1 → 直した・#1874 必須 0（Vim の fixture の全件の再生で differing 0）・#1813 必須 0（推奨 4 を直した）。#1871（probe）は設計席が差分を読んだ |
  | #1876 #1877（まとめ枝 45） | `outputs/20261119/review-1876.md`・`review-1877.md` | どちらも必須 0（推奨 2 と 3 を直した）。段 L1 の安全のレビューは `review-1873.md` |
  | #1881 #1884（まとめ枝 46） | `outputs/20261119/review-1881.md`・`review-1884.md` | #1881 必須 3 → 直して再確認で必須 0・#1884 必須 1 → 直して、再確認の必須 2 も直した（最後の直し `5dd74e75` は設計席が差分を読んだ） |

- (c) CI（merge の道具の run・どれも `check` と `build-and-test` が success）: まとめ枝 36 [43798044246](https://github.com/hideyukiMORI/nene-nib/actions/runs/43798044246)・37 [43990290226](https://github.com/hideyukiMORI/nene-nib/actions/runs/43990290226)・38 [44005218509](https://github.com/hideyukiMORI/nene-nib/actions/runs/44005218509)・39 [44036071609](https://github.com/hideyukiMORI/nene-nib/actions/runs/44036071609)・40 [44069448664](https://github.com/hideyukiMORI/nene-nib/actions/runs/44069448664)（1 回目は `nib_adapters_preview` の 1 件で failure・試験の見本 `nul.bin` が windows-2022 で予約名の装置に当たった → #1797 で直して 2 回目）・41 [44108583088](https://github.com/hideyukiMORI/nene-nib/actions/runs/44108583088)（success・2026-11-18 00:4x）・42 [44278776412](https://github.com/hideyukiMORI/nene-nib/actions/runs/44278776412)（PR #1856。PR #1855 の [44270543208](https://github.com/hideyukiMORI/nene-nib/actions/runs/44270543208) は `check` が GIT-002 で failure・`build-and-test` は success）・43 [44323317610](https://github.com/hideyukiMORI/nene-nib/actions/runs/44323317610)・44 [44451977589](https://github.com/hideyukiMORI/nene-nib/actions/runs/44451977589)・45 [44466594582](https://github.com/hideyukiMORI/nene-nib/actions/runs/44466594582)・46 [44521791145](https://github.com/hideyukiMORI/nene-nib/actions/runs/44521791145)（42〜46 はどれも merge の道具のログ `outputs/2026111{8,9}/merge-*.log` の `verifiedCI` が success）。#1805 の付け直しの道は Windows 10 / Server 2022 でだけ働くので、意味のある証拠は CI（windows-2022）の `nib_adapters_long_paths`（PR #1812 の本文）。
- (d) 施主の試用（D41）: 1.1.0 の機能はまだ施主が試していない（実機の絵は日報 11-17 のとおり席と設計席が撮って送った）。まとめ枝 42〜46 の機能（補完・hover・定義へ・設定の面・表と alert・囲みのコードの色・タブのメニュー・リンクの札・見出しへ跳ぶ・広い表の送り）は、席の試験と独立レビューとまとめ枝の検証の上で入れたもので、実機（本物の言語サーバー・ブラウザ・エクスプローラー・IME・タッチパッド）では見ていない（各まとめ枝の PR の「残るリスク」）。本番の MSIX と ZIP で `D:/NeNeNib/evidence/v1.1.0-release/hide-checklist.md` を通す → `<<結果>>`。packaged の起動・Store 版で F1 の一覧に登録の操作が出ないこと・WACK もそこで見る。
- (e) 梱包の道具の試験と、この PR の検証: 席 REL が版を上げた木で `eng/test-package-release.py` 10 件 OK・`tests.conformance.test_package_msix` 18 件 OK・Debug の全 target 警告 0・`nib_tests` の既定の実行 2,423,869 checks・`--version-info` 19・`--start-screen` 246・`nib_adapter_tests` 1,358・conformance 0（2026-11-17）。main に載せ直した木は設計席が conformance 0 を確かめ、CI は merge のときに回る（`eng/test-package-release.py` と `tests.conformance.test_package_msix`・Debug の全 target・`nib_tests`・adapters の試験・conformance）。
  区切りを合わせ直したこの PR（#1891・文書と掲載文だけ）は、`python eng/conformance.py` 0 violation・`git diff --check` 差なし・掲載文の道具（`D:/NeNeNib/scripts/20261117/1815/extract-store-listing.py`）の字数がどの欄も上限の内（`D:/NeNeNib/evidence/v1.1.0-release/store-listing/counts.md`）。製品を変えないのでビルドと試験は回していない（`eng/format-check.ps1` は C++ のファイルだけを見るので当たらない）。

## 既知の限界

1.0.0 の記録の「既知の限界」を引き継ぐ（1.1.0 で直った物は下で外す）。**1.1.0 で増えた物の候補（どれを公開の文に書くかは設計席が決める）**:

- 1.0.0 の「`:s` の旗 `c` は断る」は外す（#1652 で入った）。`:g` の中の `c` フラグは断る（#1743）。`:g` のほかの命令・`:sort`・入力中のプレビューは無い。
- 1.0.0 の「Markdown プレビューで長い段落を初めて表示する 1 回は、まだ少し待つ」は #1485 で縮んだ（最初の 1 回 366.7 → 168.9 ms・日報 11-16）が、0 にはなっていない（README の FR-007 のまま）。
- 設定の互換（D91・ADR 0170 の追補の 5）: 1.1.0 で設定を変えると `settings.v5` ができ、それ以後 1.0.0 で変えた設定（`settings.v4` に入る）は 1.1.0 から見えず、1.1.0 で変えた設定は 1.0.0 から見えない。どちらも壊れない。
- 全文検索（ADR 0167 の決定 8 と追補）: 当たりは 1,000 件で打ち切り・1 行に 1 つ・正しい UTF-8 でない行（Shift_JIS などのファイル）は当たらない・16 MiB を超えるファイルと最初の 8 KiB に NUL のあるファイルと固定の表のフォルダ（`.git` `node_modules` `build` `out`・隠し属性）は飛ばす・置換と `.gitignore` は無い・毎回読む（索引は無い）・絞り込みの効かない検索語は 1 MiB の塊が 5〜15 ms（UI は止まらない）・中身が端末に無いクラウドのファイルは読まない。
- プレビュー欄（ADR 0189）: 16 MiB を超えるファイルは出さない・構文の色は窓の先頭から始める（複数行のコメントの途中から始まると色が違う）・境のドラッグは無い。待ちの間の演出「解読」（決定 6）は入っていない。
- イージーモーション（ADR 0163 の決定 9）: オペレータの後（`d\\w`）・`t` `T` `n` `N`・2 枠のもう 1 つの枠・回数は無い。V-1801 の軽い指摘（左端の跳び先の札の左の縁が切れる ほか）は後続。
- 本文の右クリックのメニュー: 項目は D97 の 7 つだけ。
- タブの右クリックのメニュー（ADR 0078 の追補 #1790）: 鍵では開かない（Shift+F10 とメニューキーは本文のメニュー）。タブのドラッグでの並べ替えは無い。
- 共用のワーカー 1 本で待つ（ADR 0078 の追補 9・ADR 0151 段 L2 の限界・施主決定 D107 で限界として受けた）: 届かないネットワークの場所（`\\server\share\…`）で「エクスプローラーで場所を開く」を選んだとき、またはリンクを開いてブラウザやメールの起動が遅いとき、それが戻るまで自動退避・ファイルの裏の読み込み・フォルダの一覧・プレビューの読み込みが待つ（画面は止まらない）。
- 言語サーバーの補完・hover・定義へ（ADR 0190・状態は提案のまま）: 自動の import・候補の詳しい読み込み・スニペットの跳び先・引数の案内・参照の一覧・名前の変更は無い。hover はマウスを載せては出ず、通常モードには鍵が無い（F1 の「情報を表示」から・鍵は試用の後に決める）。定義へ移って別のファイルを開いたとき `''` で元のファイルへは戻らない。言語サーバーが動いていない文書の `gd` `K` は今までどおり何もしない（Vim の局所の定義へ跳ぶ `gd`・`keywordprg` の `K` ではない）。
- Vim の INSERT の補完（ADR 0169 の追補・ADR 0190 の決定 5）: 候補が出ていない間の Ctrl+P はファイルの一覧（逆向きの補完は始まらない）。候補が出ている間の Backspace は確定して 1 文字消し、候補を閉じる（端末の Vim は続ける）。Ctrl-x Ctrl-o は無い。ほかのタブの語の順は Vim と違うことがある。補完の語は 1 MiB を超える文書ではキャレットの前後 256 KiB だけ（と、ほかのタブ・言語サーバーの候補）。
- Markdown の表と alert と打ち消し線（ADR 0151 の 2026-11-18 と 2026-11-19 の追補）: 段落の直後（空行なし）の表は表にならない。升の中のコードの `\|` は `\` が残る。広い表は鍵では送れず、選択のドラッグで自動に送らない。溝を押し続けても 1 回だけ送る（#1886）。升の中で折り返さない。エスケープした印（`> \[!NOTE]`）・印の後ろが Tab の行・リストの中の引用は alert にならない。`~` 1 つは打ち消しにしない（`10時~12時` を守る）。`~` の間が画像だけのときは打ち消しのまま。
- 囲みのコードの色（ADR 0151 の #1862・#1863 の追補）: 知らない言語と言語を書いていない囲みは 1 色。`:syntax off` はプレビューの色を消さない（D105）。不正な UTF-8 を含む行は色の位置がずれることがある。1 行に 2,500 を超える色の区切りがある切れ（128 行）は 1 色。1.1.0 の途中（まとめ枝 43）の「300〜500 行より長い囲みは 1 色」は #1863（まとめ枝 45）で直ったので外す。
- リンクと見出しのリンク（ADR 0151 段 L1〜L3 の限界）: リンクの上でカーソルの形は変わらない。国際化ドメイン名のリンクは開かない（punycode の綴りは開く）。`file:` の URL はこの PC の物（host が空か `localhost`）だけ。ほかの文書の見出し（`other.md#sec`）は文書を開くだけで見出しへは跳ばない。英字以外の大文字（ギリシャ・キリル・全角の英字）を含む見出しは、小文字で書いたリンクから跳べないことがある（GitHub の実物とは未照合）。
- 設定の面（ADR 0170 の 2026-11-18 の追補 2）: 切れない機能（括弧の相手の強調など）にはスイッチが無い（「常にオン」の行か、面に出ない）。
- 公開の文に書かない候補（使う人に見えにくい・稀）: 長い囲みの再解析のたびの字句解析・空の行だけの長い囲みの位置の対応のずれ（#1877 のレビュー L1）・見出しの重複の数え O(H²)・升の保持の上限を超える表を毎 frame 作り直す（正しく描くが遅い・巨大な表だけ）・広い表のドラッグの途中で表の数が変わると止まる。最後から 2 つ目は速さに関わるので、書くかは `<<設計席が決める>>`。
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
